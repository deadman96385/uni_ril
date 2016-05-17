/**
 * ril_sim.c --- SIM-related requests process functions implementation
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#define LOG_TAG "RIL"

#include "sprd-ril.h"
#include "ril_sim.h"
#include "ril_utils.h"
#include "ril_network.h"
#include "custom/ril_custom.h"

#define RIL_SIM_PIN_PROPERTY                    "ril.sim.pin"
#define FACILITY_LOCK_REQUEST                   "2"

#define TYPE_FCP                                0x62
#define COMMAND_GET_RESPONSE                    0xc0
#define TYPE_EF                                 4
#define RESPONSE_EF_SIZE                        15
#define TYPE_FILE_DES_LEN                       5
#define RESPONSE_DATA_FCP_FLAG                  0
#define RESPONSE_DATA_FILE_DES_FLAG             2
#define RESPONSE_DATA_FILE_DES_LEN_FLAG         3
#define RESPONSE_DATA_FILE_TYPE                 6
#define RESPONSE_DATA_FILE_SIZE_1               2
#define RESPONSE_DATA_FILE_SIZE_2               3
#define RESPONSE_DATA_STRUCTURE                 13
#define RESPONSE_DATA_RECORD_LENGTH             14
#define RESPONSE_DATA_FILE_RECORD_COUNT_FLAG    8
#define RESPONSE_DATA_FILE_RECORD_LEN_1         6
#define RESPONSE_DATA_FILE_RECORD_LEN_2         7
#define EF_TYPE_TRANSPARENT                     0x01
#define EF_TYPE_LINEAR_FIXED                    0x02
#define EF_TYPE_CYCLIC                          0x06
#define USIM_DATA_OFFSET_2                      2
#define USIM_DATA_OFFSET_3                      3
#define USIM_FILE_DES_TAG                       0x82
#define USIM_FILE_SIZE_TAG                      0x80
#define TYPE_CHAR_SIZE                          sizeof(char)

#define SIM_DROP                                1
#define SIM_REMOVE                              2

#define AUTH_CONTEXT_EAP_SIM                    128
#define AUTH_CONTEXT_EAP_AKA                    129
#define SIM_AUTH_RESPONSE_SUCCESS               0
#define SIM_AUTH_RESPONSE_SYNC_FAILURE          3

static int s_simState[SIM_COUNT];
static pthread_mutex_t s_remainTimesMutex = PTHREAD_MUTEX_INITIALIZER;
static bool s_needQueryPinTimes[SIM_COUNT] = {
        true
#if (SIM_COUNT >= 2)
        ,true
#if (SIM_COUNT >= 3)
        ,true
#if (SIM_COUNT >= 4)
        ,true
#endif
#endif
#endif
        };
static bool s_needQueryPukTimes[SIM_COUNT] = {
        true
#if (SIM_COUNT >= 2)
        ,true
#if (SIM_COUNT >= 3)
        ,true
#if (SIM_COUNT >= 4)
        ,true
#endif
#endif
#endif
        };
const char *base64char =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int getSimlockRemainTimes(int channelID, SimUnlockType type) {
    int err, result;
    int remaintime = 3;
    char cmd[AT_COMMAND_LEN] = {0};
    char *line;
    ATResponse *p_response = NULL;

    RLOGD("getSimlockRemainTimes: type = %d", type);

    if (UNLOCK_PUK == type || UNLOCK_PUK2 == type) {
        remaintime = 10;
    }

    snprintf(cmd, sizeof(cmd), "AT+XX=%d", type);
    err = at_send_command_singleline(s_ATChannels[channelID], cmd, "+XX:",
                                     &p_response);
    if (err < 0 || p_response->success == 0) {
        RLOGD("getSimlockRemainTimes: +XX response error !");
    } else {
        line = p_response->p_intermediates->line;
        err = at_tok_start(&line);
        if (err == 0) {
            err = at_tok_nextint(&line, &result);
            if (err == 0) {
                remaintime = result;
                RLOGD("getSimlockRemainTimes:remaintime=%d", remaintime);
            }
        }
    }
    at_response_free(p_response);

    /* Bug 523208 set pin/puk remain times to prop. @{ */
    if (UNLOCK_PUK == type || UNLOCK_PIN == type) {
        RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
        pthread_mutex_lock(&s_remainTimesMutex);
        setPinPukRemainTimes(type, remaintime, socket_id);
        pthread_mutex_unlock(&s_remainTimesMutex);
    }
    /* }@ */
    return remaintime;
}

/* Returns SIM_NOT_READY on error */
SimStatus getSIMStatus(int channelID) {
    ATResponse *p_response = NULL;
    int err;
    int ret = SIM_NOT_READY;
    char *cpinLine;
    char *cpinResult;

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
    RLOGD("getSIMStatus(). s_radioState: %d", s_radioState[socket_id]);
    if (s_radioState[socket_id] == RADIO_STATE_UNAVAILABLE) {
        ret = SIM_NOT_READY;
        goto done;
    }

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+CPIN?",
                                     "+CPIN:", &p_response);
    if (err != 0) {
        ret = SIM_NOT_READY;
        goto done;
    }

    switch (at_get_cme_error(p_response)) {
        case CME_SUCCESS:
            break;
        case CME_SIM_NOT_INSERTED:
            ret = SIM_ABSENT;
            goto done;
        default:
            ret = SIM_NOT_READY;
            goto done;
    }

    /* CPIN? has succeeded, now look at the result */
    cpinLine = p_response->p_intermediates->line;
    err = at_tok_start(&cpinLine);

    if (err < 0) {
        ret = SIM_NOT_READY;
        goto done;
    }

    err = at_tok_nextstr(&cpinLine, &cpinResult);

    if (err < 0) {
        ret = SIM_NOT_READY;
        goto done;
    }
    RLOGD("cpin result = %s", cpinResult);
    if (0 == strcmp(cpinResult, "SIM PIN")) {
        ret = SIM_PIN;
        goto done;
    } else if (0 == strcmp(cpinResult, "SIM PUK")) {
        ret = SIM_PUK;
        goto done;
    } else if (0 == strcmp(cpinResult, "PH-NET PIN")) {
        return SIM_NETWORK_PERSONALIZATION;
    } else if (0 != strcmp(cpinResult, "READY")) {
        /* we're treating unsupported lock types as "sim absent" */
        ret = SIM_ABSENT;
        goto done;
    }

    ret = SIM_READY;

done:
    at_response_free(p_response);
    if (ret == SIM_ABSENT) {
        setSimPresent(socket_id, false);
    } else {
        setSimPresent(socket_id, true);
    }

    /** SPRD: Bug 523208 set pin/puk remain times to prop. @{*/
    if ((s_needQueryPinTimes[socket_id] && ret == SIM_PIN)
            || ( s_needQueryPukTimes[socket_id] && ret == SIM_PUK)) {
        if (ret == SIM_PIN) {
            s_needQueryPinTimes[socket_id] = false;
        } else {
            s_needQueryPukTimes[socket_id] = false;
        }
        int remaintime = getSimlockRemainTimes(channelID,
                ret == SIM_PIN ? UNLOCK_PIN : UNLOCK_PUK);
    } else if (ret == SIM_ABSENT) {
        s_needQueryPinTimes[socket_id] = true;
        s_needQueryPukTimes[socket_id] = true;
    }
    /** }@ */

    return ret;
}

RIL_AppType getSimType(int channelID) {
    int err, skip;
    int cardType;
    char *line;
    ATResponse *p_response = NULL;
    RIL_AppType ret = RIL_APPTYPE_UNKNOWN;

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+EUICC?",
                                     "+EUICC:", &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &skip);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &skip);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &cardType);
    if (err < 0) goto error;

    if (cardType == 1) {
        ret = RIL_APPTYPE_USIM;
    } else if (cardType == 0) {
        ret = RIL_APPTYPE_SIM;
    } else {
        ret = RIL_APPTYPE_UNKNOWN;
    }

    at_response_free(p_response);
    return ret;

error:
    at_response_free(p_response);
    return RIL_APPTYPE_UNKNOWN;
}

/**
 * Get the current card status.
 *
 * This must be freed using freeCardStatus.
 * @return: On success returns RIL_E_SUCCESS
 */
static int getCardStatus(int channelID, RIL_CardStatus_v6 **pp_card_status) {
    static RIL_AppStatus app_status_array[] = {
        // SIM_ABSENT = 0
        {RIL_APPTYPE_UNKNOWN, RIL_APPSTATE_UNKNOWN, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN},
        // SIM_NOT_READY = 1
        {RIL_APPTYPE_SIM, RIL_APPSTATE_DETECTED, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN},
        // SIM_READY = 2
        {RIL_APPTYPE_SIM, RIL_APPSTATE_READY, RIL_PERSOSUBSTATE_READY,
          NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN},
        // SIM_PIN = 3
        {RIL_APPTYPE_SIM, RIL_APPSTATE_PIN, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN},
        // SIM_PUK = 4
        {RIL_APPTYPE_SIM, RIL_APPSTATE_PUK, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_ENABLED_BLOCKED, RIL_PINSTATE_UNKNOWN},
        // SIM_NETWORK_PERSONALIZATION = 5
        {RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_NETWORK,
          NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN},
        // RUIM_ABSENT = 6
        {RIL_APPTYPE_UNKNOWN, RIL_APPSTATE_UNKNOWN, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN},
        // RUIM_NOT_READY = 7
        {RIL_APPTYPE_RUIM, RIL_APPSTATE_DETECTED, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN},
        // RUIM_READY = 8
        {RIL_APPTYPE_RUIM, RIL_APPSTATE_READY, RIL_PERSOSUBSTATE_READY,
          NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN},
        // RUIM_PIN = 9
        {RIL_APPTYPE_RUIM, RIL_APPSTATE_PIN, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN},
        // RUIM_PUK = 10
        {RIL_APPTYPE_RUIM, RIL_APPSTATE_PUK, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_ENABLED_BLOCKED, RIL_PINSTATE_UNKNOWN},
        // RUIM_NETWORK_PERSONALIZATION = 11
        {RIL_APPTYPE_RUIM, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_NETWORK,
          NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN}
    };
    RIL_CardState card_state;
    int num_apps;

    int sim_status = getSIMStatus(channelID);
    if (sim_status == SIM_ABSENT) {
        card_state = RIL_CARDSTATE_ABSENT;
        num_apps = 0;
    } else {
        card_state = RIL_CARDSTATE_PRESENT;
        num_apps = 1;
    }

    /* Allocate and initialize base card status. */
    RIL_CardStatus_v6 *p_card_status = malloc(sizeof(RIL_CardStatus_v6));
    p_card_status->card_state = card_state;
    p_card_status->universal_pin_state = RIL_PINSTATE_UNKNOWN;
    p_card_status->gsm_umts_subscription_app_index = RIL_CARD_MAX_APPS;
    p_card_status->cdma_subscription_app_index = RIL_CARD_MAX_APPS;
    p_card_status->ims_subscription_app_index = RIL_CARD_MAX_APPS;
    p_card_status->num_applications = num_apps;

    RIL_AppType app_type = getSimType(channelID);

    /* Initialize application status */
    unsigned int i;
    for (i = 0; i < RIL_CARD_MAX_APPS; i++) {
        p_card_status->applications[i] = app_status_array[SIM_ABSENT];
    }

    for (i = 0; i < sizeof(app_status_array) / sizeof(RIL_AppStatus); i++) {
        app_status_array[i].app_type = app_type;
    }
    /* Pickup the appropriate application status
     * that reflects sim_status for gsm.
     */
    if (num_apps != 0) {
        /* Only support one app, gsm */
        p_card_status->num_applications = 1;
        p_card_status->gsm_umts_subscription_app_index = 0;

        /* Get the correct app status */
        p_card_status->applications[0] = app_status_array[sim_status];
    }

    *pp_card_status = p_card_status;
    return RIL_E_SUCCESS;
}

/**
 * Free the card status returned by getCardStatus
 */
static void freeCardStatus(RIL_CardStatus_v6 *p_card_status) {
    free(p_card_status);
}

int getNetLockRemainTimes(int channelID, int type) {
    int err;
    int ret = -1;
    int fac = type;
    int ck_type = 1;
    int result[2] = {0, 0};
    char *line;
    char cmd[AT_COMMAND_LEN] = {0};
    ATResponse *p_response = NULL;

    RLOGD("[MBBMS] fac:%d, ck_type:%d", fac, ck_type);
    snprintf(cmd, sizeof(cmd), "AT+SPSMPN=%d,%d", fac, ck_type);
    err = at_send_command_singleline(s_ATChannels[channelID], cmd, "+SPSMPN:",
                                     &p_response);
    if (err < 0 || p_response->success == 0) {
        ret = 10;
    } else {
        line = p_response->p_intermediates->line;

        err = at_tok_start(&line);

        if (err == 0) {
            err = at_tok_nextint(&line, &result[0]);
            if (err == 0) {
                at_tok_nextint(&line, &result[1]);
                err = at_tok_nextint(&line, &result[1]);
            }
        }

        if (err == 0) {
            ret = result[0] - result[1];
        } else {
            ret = 10;
        }
    }
    at_response_free(p_response);
    return ret;
}

int getRemainTimes(int channelID, char *type) {
  if (0 == strcmp(type, "PS")) {
     return getNetLockRemainTimes(channelID, 1);
  } else if (0 == strcmp(type, "PN")) {
      return getNetLockRemainTimes(channelID, 2);
  } else if (0 == strcmp(type, "PU")) {
      return getNetLockRemainTimes(channelID, 3);
  } else if (0 == strcmp(type, "PP")) {
      return getNetLockRemainTimes(channelID, 4);
  } else if (0 == strcmp(type, "PC")) {
      return getNetLockRemainTimes(channelID, 5);
  } else if (0 == strcmp(type, "SC")) {
      return getSimlockRemainTimes(channelID, 0);
  } else if (0 == strcmp(type, "FD")) {
      return getSimlockRemainTimes(channelID, 1);
  } else {
      RLOGD("wrong type %s , return -1", type);
      return -1;
  }
}

unsigned char *convertUsimToSim(unsigned char const *byteUSIM, int len,
                                    unsigned char *hexUSIM) {
    int desIndex = 0;
    int sizeIndex = 0;
    int i = 0;
    unsigned char byteSIM[RESPONSE_EF_SIZE] = {0};
    for (i = 0; i < len; i++) {
        if (byteUSIM[i] == USIM_FILE_DES_TAG) {
            desIndex = i;
            break;
        }
    }
    RLOGE("TYPE_FCP_DES index = %d", desIndex);
    for (i = desIndex; i < len;) {
        if (byteUSIM[i] == USIM_FILE_SIZE_TAG) {
            sizeIndex = i;
            break;
        } else {
            i += (byteUSIM[i + 1] + 2);
        }
    }
    RLOGE("TYPE_FCP_SIZE index = %d ", sizeIndex);
    byteSIM[RESPONSE_DATA_FILE_SIZE_1] =
            byteUSIM[sizeIndex + USIM_DATA_OFFSET_2];
    byteSIM[RESPONSE_DATA_FILE_SIZE_2] =
            byteUSIM[sizeIndex + USIM_DATA_OFFSET_3];
    byteSIM[RESPONSE_DATA_FILE_TYPE] = TYPE_EF;
    if ((byteUSIM[desIndex + RESPONSE_DATA_FILE_DES_FLAG] & 0x07) ==
        EF_TYPE_TRANSPARENT) {
        RLOGE("EF_TYPE_TRANSPARENT");
        byteSIM[RESPONSE_DATA_STRUCTURE] = 0;
    } else if ((byteUSIM[desIndex + RESPONSE_DATA_FILE_DES_FLAG] & 0x07) ==
                EF_TYPE_LINEAR_FIXED) {
        RLOGE("EF_TYPE_LINEAR_FIXED");
        if (USIM_FILE_DES_TAG != byteUSIM[RESPONSE_DATA_FILE_DES_FLAG]) {
            RLOGE("USIM_FILE_DES_TAG != ...");
            goto error;
        }
        if (TYPE_FILE_DES_LEN != byteUSIM[RESPONSE_DATA_FILE_DES_LEN_FLAG]) {
            RLOGE("TYPE_FILE_DES_LEN != ...");
            goto error;
        }
        byteSIM[RESPONSE_DATA_STRUCTURE] = 1;
        byteSIM[RESPONSE_DATA_RECORD_LENGTH] =
                ((byteUSIM[RESPONSE_DATA_FILE_RECORD_LEN_1] & 0xff) << 8) +
                (byteUSIM[RESPONSE_DATA_FILE_RECORD_LEN_2] & 0xff);
    } else if ((byteUSIM[desIndex + RESPONSE_DATA_FILE_DES_FLAG] & 0x07) ==
                EF_TYPE_CYCLIC) {
        RLOGE("EF_TYPE_CYCLIC");
        byteSIM[RESPONSE_DATA_STRUCTURE] = 3;
        byteSIM[RESPONSE_DATA_RECORD_LENGTH] =
                ((byteUSIM[RESPONSE_DATA_FILE_RECORD_LEN_1] & 0xff) << 8) +
                (byteUSIM[RESPONSE_DATA_FILE_RECORD_LEN_2] & 0xff);
    }
    convertBinToHex((char *)byteSIM, RESPONSE_EF_SIZE, hexUSIM);
    RLOGD("convert to sim done, return: %s", hexUSIM);
    return hexUSIM;
error:
    RLOGD("convert to sim error, return NULL");
    return NULL;
}

static void requestEnterSimPin(int channelID, void *data, size_t datalen,
                                   RIL_Token t) {
    int err, ret;
    int remaintime = 3;
    char *cmd = NULL;
    char *cpinLine;
    char *cpinResult;
    ATResponse *p_response = NULL;
    SimUnlockType rsqtype = UNLOCK_PIN;
    SimStatus simstatus = SIM_ABSENT;

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    const char **strings = (const char **)data;

    if (datalen == 2 * sizeof(char *)) {
        ret = asprintf(&cmd, "AT+CPIN=%s", strings[0]);
        rsqtype = UNLOCK_PIN;
    } else if (datalen == 3 * sizeof(char *)) {
        err = at_send_command_singleline(s_ATChannels[channelID], "AT+CPIN?",
                                         "+CPIN:", &p_response);
        if (err < 0 || p_response->success == 0) {
            goto error;
        }

        cpinLine = p_response->p_intermediates->line;
        err = at_tok_start(&cpinLine);
        if (err < 0) goto error;
        err = at_tok_nextstr(&cpinLine, &cpinResult);
        if (err < 0) goto error;

        if ((0 == strcmp(cpinResult, "READY")) ||
            (0 == strcmp(cpinResult, "SIM PIN"))) {
            ret = asprintf(&cmd, "ATD**05*%s*%s*%s#", strings[0], strings[1],
                            strings[1]);
        } else {
            ret = asprintf(&cmd, "AT+CPIN=%s,%s", strings[0], strings[1]);
        }
        rsqtype = UNLOCK_PUK;
        at_response_free(p_response);
    } else {
        goto error;
    }

    if (ret < 0) {
        RLOGE("Failed to allocate memory");
        cmd = NULL;
        goto error;
    }

    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    free(cmd);

    if (err < 0 || p_response->success == 0) {
        goto error;
    } else {
        /* add for modem reboot */
        const char *pin = NULL;

        if (datalen == sizeof(char *)) {
            pin = strings[0];
        } else if (datalen == 2 * sizeof(char *)) {
            pin = strings[1];
        } else {
            goto out;
        }

        if (pin != NULL) {
            setProperty(socket_id, RIL_SIM_PIN_PROPERTY, pin);
        }

out:
        remaintime = getSimlockRemainTimes(channelID, rsqtype);
        RIL_onRequestComplete(t, RIL_E_SUCCESS, &remaintime,
                sizeof(remaintime));
        simstatus = getSIMStatus(channelID);
        RLOGD("simstatus = %d, radioStatus = %d", simstatus,
              s_radioState[socket_id]);

        if (simstatus == SIM_READY &&
            s_radioState[socket_id] == RADIO_STATE_ON) {
            setRadioState(channelID, RADIO_STATE_SIM_READY);
        } else if (SIM_NETWORK_PERSONALIZATION == simstatus) {
            RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED,
                                      NULL, 0, socket_id);
        }
        at_response_free(p_response);
        return;
    }

error:
    remaintime = getSimlockRemainTimes(channelID, rsqtype);
    RIL_onRequestComplete(t, RIL_E_PASSWORD_INCORRECT, &remaintime,
                          sizeof(remaintime));
    at_response_free(p_response);
}

static void requestEnterSimPuk2(int channelID, void *data, size_t datalen,
                                    RIL_Token t) {
    int err, ret;
    char *cmd = NULL;
    const char **strings = (const char **)data;
    ATResponse *p_response = NULL;
    SimStatus simstatus = SIM_ABSENT;

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    if (datalen == 3 * sizeof(char *)) {
        ret = asprintf(&cmd, "ATD**052*%s*%s*%s#", strings[0], strings[1],
                        strings[1]);
    } else {
        goto error;
    }

    if (ret < 0) {
        RLOGE("Failed to allocate memory");
        cmd = NULL;
        goto error;
    }

    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    free(cmd);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);

    simstatus = getSIMStatus(channelID);
    RLOGD("simstatus = %d, radioStatus = %d", simstatus,
          s_radioState[socket_id]);
    if (simstatus == SIM_READY  && s_radioState[socket_id] == RADIO_STATE_ON) {
        setRadioState(channelID, RADIO_STATE_SIM_READY);
    } else if (SIM_NETWORK_PERSONALIZATION == simstatus) {
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED, NULL,
                                  0, socket_id);
    }
    return;

error:
    RIL_onRequestComplete(t, RIL_E_PASSWORD_INCORRECT, NULL, 0);
    at_response_free(p_response);
}

static void requestChangeSimPin(int channelID, void *data, size_t datalen,
                                    RIL_Token t) {
    int err, ret;
    int remaintime = 3;
    char *cmd = NULL;
    const char **strings = (const char **)data;
    ATResponse *p_response = NULL;

    if (datalen == 3 * sizeof(char *)) {
        ret = asprintf(&cmd, "AT+CPWD=\"SC\",\"%s\",\"%s\"", strings[0],
                        strings[1]);
    } else {
        goto error;
    }

    if (ret < 0) {
        RLOGE("Failed to allocate memory");
        cmd = NULL;
        goto error;
    }

    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    free(cmd);
    if (err < 0 || p_response->success == 0) {
        goto error;
    } else {
        /* add for modem reboot */
        const char *pin = NULL;
        pin = strings[1];

        RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

        setProperty(socket_id, RIL_SIM_PIN_PROPERTY, pin);
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);
    return;

error:
    remaintime = getSimlockRemainTimes(channelID, UNLOCK_PIN);
    RIL_onRequestComplete(t, RIL_E_PASSWORD_INCORRECT, &remaintime,
                          sizeof(remaintime));
    at_response_free(p_response);
}

static void requestChangeSimPin2(int channelID, void *data, size_t datalen,
                                     RIL_Token t) {
    int err, ret;
    int remaintime = 3;
    char *cmd = NULL;
    const char **strings = (const char **)data;
    ATResponse *p_response = NULL;

    if (datalen == 3 * sizeof(char *)) {
        ret = asprintf(&cmd, "AT+CPWD=\"P2\",\"%s\",\"%s\"", strings[0],
                        strings[1]);
    } else {
        goto error;
    }

    if (ret < 0) {
        RLOGE("Failed to allocate memory");
        cmd = NULL;
        goto error;
    }

    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    free(cmd);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);
    return;

error:
    remaintime = getSimlockRemainTimes(channelID, UNLOCK_PIN2);
    RIL_onRequestComplete(t, RIL_E_PASSWORD_INCORRECT, &remaintime,
                          sizeof(remaintime));
    at_response_free(p_response);
}

static void requestFacilityLock(int channelID, char **data, size_t datalen,
                                    RIL_Token t) {
    int err, result, status;
    int serviceClass = 0;
    int ret = -1;
    int errNum = -1;
    int remainTimes = 10;
    int response[2] = {0};
    char *cmd, *line;
    ATLine *p_cur;
    ATResponse *p_response = NULL;

    char *type = data[0];
    RLOGD("requestFacilityLock, type = %s ", type);

    if (datalen != 5 * sizeof(char *)) {
        goto error1;
    }

    serviceClass = atoi(data[3]);
    if (serviceClass == 0) {
        ret = asprintf(&cmd, "AT+CLCK=\"%s\",%c,\"%s\"", data[0], *data[1],
                        data[2]);
    } else {
        ret = asprintf(&cmd, "AT+CLCK=\"%s\",%c,\"%s\",%s", data[0], *data[1],
                        data[2], data[3]);
    }

    if (ret < 0) {
        RLOGE("Failed to allocate memory");
        cmd = NULL;
        goto error1;
    }

    if (*data[1] == '2') {
        err = at_send_command_multiline(s_ATChannels[channelID], cmd, "+CLCK: ",
                                        &p_response);
        free(cmd);
    } else {
        RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

        err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
        free(cmd);
        if (err < 0 || p_response->success == 0) {
            goto error;
        } else if (!strcmp(data[0], "SC")) {
            /* add for modem reboot */
            char *pin = NULL;
            pin = data[2];

            setProperty(socket_id, RIL_SIM_PIN_PROPERTY, pin);
        }

        result = 1;
        RIL_onRequestComplete(t, RIL_E_SUCCESS, &result, sizeof(result));
        at_response_free(p_response);
        return;
    }

    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    for (p_cur = p_response->p_intermediates; p_cur != NULL;
         p_cur = p_cur->p_next) {
        line = p_cur->line;

        err = at_tok_start(&line);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &status);
        if (err < 0) goto error;
        if (at_tok_hasmore(&line)) {
            err = at_tok_nextint(&line, &serviceClass);
            if (err < 0) goto error;
        }
        response[0] = status;
        response[1] |= serviceClass;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(response));
    at_response_free(p_response);
    return;

error:
    if (strStartsWith(p_response->finalResponse, "+CME ERROR:")) {
        line = p_response->finalResponse;
        err = at_tok_start(&line);
        if (err >= 0) {
            err = at_tok_nextint(&line, &errNum);
            if (err >= 0) {
                if (errNum == 11 || errNum == 12) {
                    setRadioState(channelID, RADIO_STATE_SIM_LOCKED_OR_ABSENT);
                } else if (errNum == 70 || errNum == 3 || errNum == 128 ||
                            errNum == 254) {
                    remainTimes = getRemainTimes(channelID, type);
                    if (errNum == 3 && !strcmp(data[0], "SC") &&
                            *data[1] == '1') {
                        RIL_onRequestComplete(t, RIL_E_SUCCESS, &remainTimes,
                                              sizeof(remainTimes));
                    } else {
                        RIL_onRequestComplete(t, RIL_E_FDN_CHECK_FAILURE,
                                              &remainTimes,
                                              sizeof(remainTimes));
                    }
                    at_response_free(p_response);
                    return;
                } else if (errNum == 16) {
                    remainTimes = getRemainTimes(channelID, type);
                    RIL_onRequestComplete(t, RIL_E_PASSWORD_INCORRECT,
                                          &remainTimes, sizeof(remainTimes));
                    at_response_free(p_response);
                    return;
                }
            }
        }
    }

error1:
    remainTimes = getRemainTimes(channelID, type);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, &remainTimes,
                          sizeof(remainTimes));
    at_response_free(p_response);
}

static void requestSIM_IO(int channelID, void *data, size_t datalen,
                             RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err;
    char *cmd = NULL;
    char *line = NULL;
    char pad_data = '0';
    RIL_SIM_IO_v6 *p_args;
    ATResponse *p_response = NULL;
    RIL_SIM_IO_Response sr;

    memset(&sr, 0, sizeof(sr));

    p_args = (RIL_SIM_IO_v6 *)data;

    /* FIXME handle pin2 */
    if (p_args->pin2 != NULL) {
        RLOGI("Reference-ril. requestSIM_IO pin2");
    }
    if (p_args->data == NULL) {
        err = asprintf(&cmd, "AT+CRSM=%d,%d,%d,%d,%d,%c,\"%s\"",
                        p_args->command, p_args->fileid, p_args->p1, p_args->p2,
                        p_args->p3, pad_data, p_args->path);
    } else {
        err = asprintf(&cmd, "AT+CRSM=%d,%d,%d,%d,%d,\"%s\",\"%s\"",
                        p_args->command, p_args->fileid, p_args->p1, p_args->p2,
                        p_args->p3, p_args->data, p_args->path);
    }
    if (err < 0) {
        RLOGE("Failed to allocate memory");
        cmd = NULL;
        goto error;
    }

    err = at_send_command_singleline(s_ATChannels[channelID], cmd, "+CRSM:",
                                     &p_response);
    free(cmd);

    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &(sr.sw1));
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &(sr.sw2));
    if (err < 0) goto error;

    if (at_tok_hasmore(&line)) {
        err = at_tok_nextstr(&line, &(sr.simResponse));
        if (err < 0) goto error;
    }

    if (getSimType(channelID) == RIL_APPTYPE_USIM &&
        (p_args->command == COMMAND_GET_RESPONSE)) {
        RLOGD("usim card, change to sim format");
        if (sr.simResponse != NULL) {
            RLOGD("sr.simResponse NOT NULL, convert to sim");
            unsigned char *byteUSIM = NULL;
            // simResponse could not be odd, ex "EF3EF0"
            int usimLen = strlen(sr.simResponse) / 2;
            byteUSIM = (unsigned char *)malloc(usimLen + sizeof(char));
            memset(byteUSIM, 0, usimLen + sizeof(char));
            convertHexToBin(sr.simResponse, strlen(sr.simResponse),
                    (char *)byteUSIM);
            if (byteUSIM[RESPONSE_DATA_FCP_FLAG] != TYPE_FCP) {
                RLOGE("wrong fcp flag, unable to convert to sim ");
                if (byteUSIM != NULL) {
                    free(byteUSIM);
                    byteUSIM = NULL;
                }
                goto error;
            }

            unsigned char hexUSIM[RESPONSE_EF_SIZE * 2 + TYPE_CHAR_SIZE] = {0};
            memset(hexUSIM, 0, RESPONSE_EF_SIZE * 2 + TYPE_CHAR_SIZE);
            if (NULL != convertUsimToSim(byteUSIM, usimLen, hexUSIM)) {
                memset(sr.simResponse, 0, usimLen * 2);
                strncpy(sr.simResponse, (char *)hexUSIM,
                        RESPONSE_EF_SIZE * 2);
            }
            if (byteUSIM != NULL) {
                free(byteUSIM);
                byteUSIM = NULL;
            }
            if (sr.simResponse == NULL) {
                 RLOGE("unable convert to sim, return error");
                 goto error;
             }
        }
    }
    RIL_onRequestComplete(t, RIL_E_SUCCESS, &sr, sizeof(sr));
    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static void requestTransmitApduBasic(int channelID, void *data,
                                          size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err, len;
    char *cmd = NULL;
    char *line = NULL;
    RIL_SIM_APDU *p_args = NULL;
    ATResponse *p_response = NULL;
    RIL_SIM_IO_Response sr;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    memset(&sr, 0, sizeof(sr));

    p_args = (RIL_SIM_APDU *)data;

    if ((p_args->data == NULL) || (strlen(p_args->data) == 0)) {
        if (p_args->p3 < 0) {
            asprintf(&cmd, "AT+CSIM=%d,\"%02x%02x%02x%02x\"", 8, p_args->cla,
                    p_args->instruction, p_args->p1, p_args->p2);
        } else {
            asprintf(&cmd, "AT+CSIM=%d,\"%02x%02x%02x%02x%02x\"", 10,
                    p_args->cla, p_args->instruction, p_args->p1, p_args->p2,
                    p_args->p3);
        }
    } else {
        asprintf(&cmd, "AT+CSIM=%d,\"%02x%02x%02x%02x%02x%s\"",
                10 + strlen(p_args->data), p_args->cla, p_args->instruction,
                p_args->p1, p_args->p2, p_args->p3, p_args->data);
    }
    err = at_send_command_singleline(s_ATChannels[channelID], cmd, "+CSIM:",
                                     &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &len);
    if (err < 0) goto error;

    err = at_tok_nextstr(&line, &(sr.simResponse));
    if (err < 0) goto error;

    sscanf(&(sr.simResponse[len - 4]), "%02x%02x", &(sr.sw1), &(sr.sw2));
    sr.simResponse[len - 4] = '\0';

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &sr, sizeof(sr));
    at_response_free(p_response);
    free(cmd);

    // end sim toolkit session if 90 00 on TERMINAL RESPONSE
    if ((p_args->instruction == 20) && (sr.sw1 == 0x90)) {
        RIL_onUnsolicitedResponse(RIL_UNSOL_STK_SESSION_END,
                NULL, 0, socket_id);
    }

    // return if no sim toolkit proactive command is ready
    if (sr.sw1 != 0x91) {
        return;
    }

fetch:
    p_response = NULL;
    asprintf(&cmd, "AT+CSIM=10,\"a0120000%02x\"", sr.sw2);
    err = at_send_command_singleline(s_ATChannels[channelID], cmd, "+CSIM:",
                                     &p_response);
    if (err < 0 || p_response->success == 0) {
        goto fetch_error;
    }

    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0) goto fetch_error;

    err = at_tok_nextint(&line, &len);
    if (err < 0) goto fetch_error;

    err = at_tok_nextstr(&line, &(sr.simResponse));
    if (err < 0) goto fetch_error;

    sscanf(&(sr.simResponse[len - 4]), "%02x%02x", &(sr.sw1), &(sr.sw2));
    sr.simResponse[len - 4] = '\0';

    RIL_onUnsolicitedResponse(RIL_UNSOL_STK_PROACTIVE_COMMAND, sr.simResponse,
                              strlen(sr.simResponse), socket_id);
    goto fetch_error;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);

fetch_error:
    at_response_free(p_response);
    free(cmd);
}

static void requestOpenLogicalChannel(int channelID, void *data,
                                           size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err;
    int err_no = RIL_E_GENERIC_FAILURE;
    int responseLen = 1;
    int response[260];
    char *cmd = NULL;
    char *line = NULL;
    char *statusWord = NULL;
    char *responseData = NULL;
    ATResponse *p_response = NULL;

    asprintf(&cmd, "AT+SPCCHO=\"%s\"", (char *)data);
    err = at_send_command_singleline(s_ATChannels[channelID], cmd, "+SPCCHO:",
                                     &p_response);
    if (err < 0) goto error;
    if (p_response != NULL && p_response->success == 0) {
        if (!strcmp(p_response->finalResponse, "+CME ERROR: 20")) {
            err_no = RIL_E_MISSING_RESOURCE;
        } else if (!strcmp(p_response->finalResponse, "+CME ERROR: 22")) {
            err_no = RIL_E_NO_SUCH_ELEMENT;
        }
        goto error;
    }

    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0) goto error;

    // Read channel number
    err = at_tok_nextint(&line, &response[0]);
    if (err < 0) goto error;

    // Read select response (if available)
    if (at_tok_hasmore(&line)) {
        err = at_tok_nextstr(&line, &statusWord);
        if (err < 0) goto error;

        if (at_tok_hasmore(&line)) {
            err = at_tok_nextstr(&line, &responseData);
            if (err < 0) goto error;
            int length = strlen(responseData) / 2;
            while (responseLen <= length) {
                sscanf(responseData, "%02x", &(response[responseLen]));
                responseLen++;
                responseData += 2;
            }
            sscanf(statusWord, "%02x%02x", &(response[responseLen]),
                    &(response[responseLen + 1]));
            responseLen = responseLen + 2;
        } else {
            sscanf(statusWord, "%02x%02x", &(response[responseLen]),
                    &(response[responseLen + 1]));
            responseLen = responseLen + 2;
        }
    } else {
        // no select response, set status word
        response[responseLen] = 0x90;
        response[responseLen + 1] = 0x00;
        responseLen = responseLen + 2;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS,
            response, responseLen * sizeof(int));
    at_response_free(p_response);
    free(cmd);
    return;

error:
    RIL_onRequestComplete(t, err_no, NULL, 0);
    at_response_free(p_response);
    free(cmd);
}

static void requestCloseLogicalChannel(int channelID, void *data,
                                            size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err;
    int session_id = -1;
    char cmd[AT_COMMAND_LEN] = {0};
    char *line;
    ATResponse *p_response = NULL;

    session_id = ((int *)data)[0];

    snprintf(cmd, sizeof(cmd), "AT+CCHC=%d", session_id);
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);

    if (err < 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    }
    at_response_free(p_response);
}

static void requestTransmitApdu(int channelID, void *data, size_t datalen,
                                    RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err;
    int err_no = RIL_E_GENERIC_FAILURE;
    int len;
    char *cmd = NULL;
    char *line = NULL;
    ATResponse *p_response = NULL;
    RIL_SIM_IO_Response sr;
    RIL_SIM_APDU *p_args = NULL;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    memset(&sr, 0, sizeof(sr));

    p_args = (RIL_SIM_APDU *)data;
    if ((p_args->data == NULL) || (strlen(p_args->data) == 0)) {
        if (p_args->p3 < 0) {
            asprintf(&cmd, "AT+CGLA=%d,%d,\"%02x%02x%02x%02x\"",
                    p_args->sessionid, 8, p_args->cla, p_args->instruction,
                    p_args->p1, p_args->p2);
        } else {
            asprintf(&cmd, "AT+CGLA=%d,%d,\"%02x%02x%02x%02x%02x\"",
                    p_args->sessionid, 10, p_args->cla, p_args->instruction,
                    p_args->p1, p_args->p2, p_args->p3);
        }
    } else {
        asprintf(&cmd, "AT+CGLA=%d,%d,\"%02x%02x%02x%02x%02x%s\"",
                p_args->sessionid, 10 + strlen(p_args->data), p_args->cla,
                p_args->instruction, p_args->p1, p_args->p2, p_args->p3,
                p_args->data);
    }

    err = at_send_command_singleline(s_ATChannels[channelID], cmd, "+CGLA:",
                                     &p_response);
    if (err < 0) goto error;
    if (p_response != NULL && p_response->success == 0) {
        if (!strcmp(p_response->finalResponse, "+CME ERROR: 21") ||
            !strcmp(p_response->finalResponse, "+CME ERROR: 50")) {
            err_no = RIL_E_INVALID_PARAMETER;
        }
        goto error;
    }

    line = p_response->p_intermediates->line;

    if (at_tok_start(&line) < 0 || at_tok_nextint(&line, &len) < 0
            || at_tok_nextstr(&line, &(sr.simResponse)) < 0) {
        err = RIL_E_GENERIC_FAILURE;
        goto error;
    }

    sscanf(&(sr.simResponse[len - 4]), "%02x%02x", &(sr.sw1), &(sr.sw2));
    sr.simResponse[len - 4] = '\0';

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &sr, sizeof(sr));
    at_response_free(p_response);
    free(cmd);
    // end sim toolkit session if 90 00 on TERMINAL RESPONSE
    if ((p_args->instruction == 20) && (sr.sw1 == 0x90)) {
        RIL_onUnsolicitedResponse(RIL_UNSOL_STK_SESSION_END, NULL, 0, socket_id);
    }

    // return if no sim toolkit proactive command is ready
    if (sr.sw1 != 0x91) {
        return;
    }

fetch:
    p_response = NULL;
    asprintf(&cmd, "AT+CGLA= %d, 10,\"a0120000%02x\"",
            p_args->sessionid, sr.sw2);
    err = at_send_command_singleline(s_ATChannels[channelID], cmd, "+CSIM:",
            &p_response);
    if (err < 0 || p_response->success == 0) {
        goto fetch_error;
    }
    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto fetch_error;

    err = at_tok_nextint(&line, &len);
    if (err < 0) goto fetch_error;

    err = at_tok_nextstr(&line, &(sr.simResponse));
    if (err < 0) goto fetch_error;

    sscanf(&(sr.simResponse[len - 4]), "%02x%02x", &(sr.sw1), &(sr.sw2));
    sr.simResponse[len - 4] = '\0';

    RIL_onUnsolicitedResponse(RIL_UNSOL_STK_PROACTIVE_COMMAND, sr.simResponse,
                              strlen(sr.simResponse), socket_id);
    goto fetch_error;

error:
    RIL_onRequestComplete(t, err_no, NULL, 0);

fetch_error:
    at_response_free(p_response);
    free(cmd);
}

int base64_decode(const char *base64, unsigned char *bindata) {
    int i, j;
    unsigned char k;
    unsigned char temp[4];

    for (i = 0, j = 0; base64[i] != '\0'; i += 4) {
        memset(temp, 0xFF, sizeof(temp));
        for (k = 0; k < 64; k++) {
            if (base64char[k] == base64[i]) {
                temp[0] = k;
            }
        }
        for (k = 0; k < 64; k++) {
            if (base64char[k] == base64[i + 1]) {
                temp[1] = k;
            }
        }
        for (k = 0; k < 64; k++) {
            if (base64char[k] == base64[i + 2]) {
                temp[2] = k;
            }
        }
        for (k = 0; k < 64; k++) {
            if (base64char[k] == base64[i + 3]) {
                temp[3] = k;
            }
        }

        bindata[j++] =
                ((unsigned char)(((unsigned char)(temp[0] << 2)) & 0xFC))
                | ((unsigned char)((unsigned char)(temp[1] >> 4) & 0x03));
        if (base64[i + 2] == '=') {
            break;
        }
        bindata[j++] =
                ((unsigned char)(((unsigned char)(temp[1] << 4)) & 0xF0))
                | ((unsigned char)((unsigned char)(temp[2] >> 2) & 0x0F));
        if (base64[i + 3] == '=') {
            break;
        }
        bindata[j++] =
                ((unsigned char)(((unsigned char)(temp[2] << 6)) & 0xF0))
                | ((unsigned char) (temp[3] & 0x3F));
    }
    return j;
}

char *base64_encode(const unsigned char *bindata, char *base64,
                      int binlength) {
    int i, j;
    unsigned char current;

    for (i = 0, j = 0; i < binlength; i += 3) {
        current = (bindata[i] >> 2);
        current &= (unsigned char)0x3F;
        base64[j++] = base64char[(int)current];

        current = ((unsigned char)(bindata[i] << 4)) & ((unsigned char)0x30);
        if (i + 1 >= binlength) {
            base64[j++] = base64char[(int)current];
            base64[j++] = '=';
            base64[j++] = '=';
            break;
        }
        current |= ((unsigned char)(bindata[i + 1] >> 4))
                & ((unsigned char) 0x0F);
        base64[j++] = base64char[(int)current];

        current = ((unsigned char)(bindata[i + 1] << 2))
                & ((unsigned char)0x3C);
        if (i + 2 >= binlength) {
            base64[j++] = base64char[(int)current];
            base64[j++] = '=';
            break;
        }
        current |= ((unsigned char)(bindata[i + 2] >> 6))
                & ((unsigned char)0x03);
        base64[j++] = base64char[(int)current];

        current = ((unsigned char)bindata[i + 2]) & ((unsigned char)0x3F);
        base64[j++] = base64char[(int)current];
    }
    base64[j] = '\0';
    return base64;
}

static void requestUSimAuthentication(int channelID, char *authData,
        RIL_Token t) {
    int err, ret;
    int status;
    int binSimResponseLen;
    int randLen, autnLen, resLen, ckLen, ikLen, autsLen;
    char *cmd = NULL;
    char *line = NULL;
    char *rand = NULL;
    char *autn = NULL;
    char *res, *ck, *ik, *auts;
    unsigned char *binSimResponse = NULL;
    unsigned char *binAuthData = NULL;
    unsigned char *hexAuthData = NULL;
    ATResponse *p_response = NULL;
    RIL_SIM_IO_Response response;

    memset(&response, 0, sizeof(response));
    response.sw1 = 0x90;
    response.sw2 = 0;

    binAuthData  = (unsigned char *)malloc(sizeof(char) * strlen(authData));
    if (binAuthData == NULL) {
        goto error;
    }
    base64_decode(authData, binAuthData);
    hexAuthData = (unsigned char *)malloc(strlen(authData) * 2 + sizeof(char));
    if (hexAuthData == NULL) {
        goto error;
    }
    memset(hexAuthData, 0, strlen(authData) * 2 + sizeof(char));
    convertBinToHex((char *)binAuthData, strlen(authData), hexAuthData);

    randLen = binAuthData[0];
    autnLen = binAuthData[randLen + 1];
    rand = (char *)malloc(sizeof(char) * (randLen * 2 + sizeof(char)));
    if (rand == NULL) {
        goto error;
    }
    autn = (char *)malloc(sizeof(char) * (autnLen * 2 + sizeof(char)));
    if (autn == NULL) {
        goto error;
    }
    memcpy(rand, hexAuthData + 2, randLen * 2);
    memcpy(rand + randLen * 2, "\0", 1);
    memcpy(autn, hexAuthData + randLen * 2 +4, autnLen * 2);
    memcpy(autn + autnLen * 2, "\0", 1);

    RLOGD("requestUSimAuthentication rand = %s, autn = %s", rand, autn);

    ret = asprintf(&cmd, "AT^MBAU=\"%s\",\"%s\"", rand, autn);
    if (ret < 0) {
        RLOGE("Failed to allocate memory");
        cmd = NULL;
        goto error;
    }
    err = at_send_command_singleline(s_ATChannels[channelID], cmd, "^MBAU:",
            &p_response);
    free(cmd);

    if (err < 0 || p_response->success == 0) {
        goto error;
    } else {
        line = p_response->p_intermediates->line;

        err = at_tok_start(&line);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &status);
        if (err < 0) goto error;

        if (status == SIM_AUTH_RESPONSE_SUCCESS) {
            err = at_tok_nextstr(&line, &res);
            if (err < 0) goto error;
            resLen = strlen(res);

            err = at_tok_nextstr(&line, &ck);
            if (err < 0) goto error;
            ckLen = strlen(ck);

            err = at_tok_nextstr(&line, &ik);
            if (err < 0) goto error;
            ikLen = strlen(ik);

            // 0xdb + resLen + res + ckLen + ck  + ikLen + ik + '\0'
            binSimResponseLen =
                    (resLen + ckLen + ikLen) / 2 + 4 * sizeof(char);
            binSimResponse =
                (unsigned char *)malloc(binSimResponseLen + sizeof(char));
            if (binSimResponse == NULL) {
                goto error;
            }
            memset(binSimResponse, 0, binSimResponseLen + sizeof(char));
            // set flag to first byte
            binSimResponse[0] = 0xDB;
            // set resLen and res
            binSimResponse[1] = (resLen / 2) & 0xFF;
            convertHexToBin(res, resLen, (char *)(binSimResponse + 2));
            // set ckLen and ck
            binSimResponse[2 + resLen / 2] = (ckLen / 2) & 0xFF;
            convertHexToBin(ck, ckLen,
                (char *)(binSimResponse + 2 + resLen / 2 + 1));
            // set ikLen and ik
            binSimResponse[2 + resLen / 2 + 1 + ckLen / 2] = (ikLen / 2) & 0xFF;
            convertHexToBin(ik, ikLen,
                (char *)(binSimResponse + 2 + resLen/2 + 1 + ckLen / 2 + 1));

            response.simResponse =
                    (char *)malloc(2 * binSimResponseLen + sizeof(char));
            if (response.simResponse  == NULL) {
                goto error;
            }
            base64_encode(binSimResponse, response.simResponse,
                    binSimResponse[1] + binSimResponse[2 + resLen / 2] +
                    binSimResponse[2 + resLen / 2 + 1 + ckLen / 2] + 4);
            RIL_onRequestComplete(t, RIL_E_SUCCESS, &response,
                    sizeof(response));
        } else if (status == SIM_AUTH_RESPONSE_SYNC_FAILURE) {
            err = at_tok_nextstr(&line, &auts);
            if (err < 0) goto error;
            autsLen = strlen(auts);
            RLOGD("requestUSimAuthentication auts = %s, autsLen = %d",
                    auts, autsLen);

            binSimResponseLen = autsLen / 2 + 2 * sizeof(char);
            binSimResponse =
                (unsigned char *)malloc(binSimResponseLen + sizeof(char));
            if (binSimResponse  == NULL) {
                goto error;
            }
            memset(binSimResponse, 0, binSimResponseLen + sizeof(char));
            // set flag to first byte
            binSimResponse[0] = 0xDC;
            // set autsLen and auts
            binSimResponse[1] = (autsLen / 2) & 0xFF;
            convertHexToBin(auts, autsLen, (char *)(binSimResponse + 2));

            response.simResponse =
                (char *)malloc(2 * binSimResponseLen + sizeof(char));
            if (response.simResponse  == NULL) {
                goto error;
            }
            base64_encode(binSimResponse, response.simResponse,
                    binSimResponse[1] + 2);
            RIL_onRequestComplete(t, RIL_E_SUCCESS, &response,
                    sizeof(response));
        } else {
            goto error;
        }
    }
    at_response_free(p_response);

    FREEMEMORY(binAuthData);
    FREEMEMORY(hexAuthData);
    FREEMEMORY(rand);
    FREEMEMORY(autn);
    FREEMEMORY(response.simResponse);
    FREEMEMORY(binSimResponse);
    return;

error:
    FREEMEMORY(binAuthData);
    FREEMEMORY(hexAuthData);
    FREEMEMORY(rand);
    FREEMEMORY(autn);
    FREEMEMORY(response.simResponse);
    FREEMEMORY(binSimResponse);

    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static void requestSimAuthentication(int channelID, char *authData,
        RIL_Token t) {
    int err, ret;
    int status;
    int binSimResponseLen;
    int randLen, kcLen, sresLen;
    char *cmd;
    char *line;
    char *rand = NULL;
    char *kc, *sres;
    unsigned char *binSimResponse = NULL;
    unsigned char *binAuthData = NULL;
    unsigned char *hexAuthData = NULL;
    ATResponse *p_response = NULL;
    RIL_SIM_IO_Response response;

    memset(&response, 0, sizeof(response));
    response.sw1 = 0x90;
    response.sw2 = 0;

    binAuthData  =
            (unsigned char *)malloc(sizeof(char) * strlen(authData));
    if (binAuthData == NULL) {
        goto error;
    }
    base64_decode(authData, binAuthData);
    hexAuthData =
            (unsigned char *)malloc(strlen(authData) * 2 + sizeof(char));
    if (hexAuthData == NULL) {
        goto error;
    }
    memset(hexAuthData, 0, strlen(authData) * 2 + sizeof(char));
    convertBinToHex((char *)binAuthData, strlen(authData), hexAuthData);

    randLen = binAuthData[0];
    rand = (char *)malloc(sizeof(char) * (randLen * 2 + sizeof(char)));
    if (rand == NULL) {
        goto error;
    }
    memcpy(rand, hexAuthData + 2, randLen * 2);
    memcpy(rand + randLen * 2, "\0", 1);

    RLOGD("requestSimAuthentication rand = %s", rand);
    ret = asprintf(&cmd, "AT^MBAU=\"%s\"", rand);
    if (ret < 0) {
        RLOGE("Failed to allocate memory");
        cmd = NULL;
        goto error;
    }
    err = at_send_command_singleline(s_ATChannels[channelID], cmd, "^MBAU:",
            &p_response);
    free(cmd);

    if (err < 0 || p_response->success == 0) {
        goto error;
    } else {
        line = p_response->p_intermediates->line;
        RLOGD("requestSimAuthentication: err= %d line= %s", err, line);
        err = at_tok_start(&line);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &status);
        if (err < 0) goto error;

        if (status == SIM_AUTH_RESPONSE_SUCCESS) {
            err = at_tok_nextstr(&line, &kc);
            if (err < 0) goto error;
            kcLen = strlen(kc);

            err = at_tok_nextstr(&line, &sres);
            if (err < 0) goto error;
            sresLen = strlen(sres);

            // sresLen + sres + kcLen + kc + '\0'
            binSimResponseLen = (kcLen + sresLen) / 2 + 3 * sizeof(char);
            binSimResponse =
                (unsigned char *)malloc(binSimResponseLen + sizeof(char));
            if (binSimResponse == NULL) {
                goto error;
            }
            memset(binSimResponse, 0, binSimResponseLen + sizeof(char));
            // set sresLen and sres
            binSimResponse[0] = (sresLen / 2) & 0xFF;
            convertHexToBin(sres, sresLen, (char *)(binSimResponse + 1));
            // set kcLen and kc
            binSimResponse[1 + sresLen / 2] = (kcLen / 2) & 0xFF;
            convertHexToBin(kc, kcLen,
                    (char *)(binSimResponse + 1 + sresLen / 2 + 1));

            response.simResponse =
                    (char *)malloc(2 * binSimResponseLen + sizeof(char));
            if (response.simResponse == NULL) {
                goto error;
            }
            base64_encode(binSimResponse, response.simResponse,
                    binSimResponse[0] + binSimResponse[1 + sresLen / 2] + 2);
            RIL_onRequestComplete(t, RIL_E_SUCCESS, &response,
                    sizeof(response));
        } else {
            goto error;
        }
    }
    at_response_free(p_response);

    FREEMEMORY(binAuthData);
    FREEMEMORY(hexAuthData);
    FREEMEMORY(rand);
    FREEMEMORY(response.simResponse);
    FREEMEMORY(binSimResponse);
    return;

error:
    FREEMEMORY(binAuthData);
    FREEMEMORY(hexAuthData);
    FREEMEMORY(rand);
    FREEMEMORY(response.simResponse);
    FREEMEMORY(binSimResponse);

    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static void requestInitISIM(int channelID, void *data, size_t datalen,
                               RIL_Token t) {
    int err;
    int response = 0;
    char cmd[AT_COMMAND_LEN] = {0};
    char *line;
    const char **strings = (const char **)data;
    ATResponse *p_response = NULL;

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+ISIM=1",
                                     "+ISIM:", &p_response);
    if (err >= 0 && p_response->success) {
        line = p_response->p_intermediates->line;
        err = at_tok_start(&line);
        if (err >= 0) {
            err = at_tok_nextint(&line, &response);
            if (err >= 0) {
                RLOGD("Response of ISIM is %d", response);
            }
        }
    }
    AT_RESPONSE_FREE(p_response);

    /* 7 means the number of data from fwk, represents 7 AT commands value */
    if (datalen == 7 * sizeof(char *) && strings[0] != NULL &&
        strlen(strings[0]) > 0) {
        if (response == 0) {
            memset(cmd, 0, sizeof(cmd));
            RLOGE("requestInitISIM impu = \"%s\"", strings[2]);
            snprintf(cmd, sizeof(cmd), "AT+IMPU=\"%s\"", strings[2]);
            err = at_send_command(s_ATChannels[channelID], cmd , NULL);

            memset(cmd, 0, sizeof(cmd));
            RLOGD("requestInitISIM impi = \"%s\"", strings[3]);
            snprintf(cmd, sizeof(cmd), "AT+IMPI=\"%s\"", strings[3]);
            err = at_send_command(s_ATChannels[channelID], cmd , NULL);

            memset(cmd, 0, sizeof(cmd));
            RLOGD("requestInitISIM domain = \"%s\"", strings[4]);
            snprintf(cmd, sizeof(cmd), "AT+DOMAIN=\"%s\"", strings[4]);
            err = at_send_command(s_ATChannels[channelID], cmd , NULL);

            memset(cmd, 0, sizeof(cmd));
            RLOGD("requestInitISIM xcap = \"%s\"", strings[5]);
            snprintf(cmd, sizeof(cmd), "AT+XCAPRTURI=\"%s\"", strings[5]);
            err = at_send_command(s_ATChannels[channelID], cmd , NULL);

            memset(cmd, 0, sizeof(cmd));
            RLOGD("requestInitISIM bsf = \"%s\"", strings[6]);
            snprintf(cmd, sizeof(cmd), "AT+BSF=\"%s\"", strings[6]);
            err = at_send_command(s_ATChannels[channelID], cmd , NULL);
        }
        memset(cmd, 0, sizeof(cmd));
        RLOGD("requestInitISIM instanceId = \"%s\"", strings[1]);
        snprintf(cmd, sizeof(cmd), "AT+INSTANCEID=\"%s\"", strings[1]);
        err = at_send_command(s_ATChannels[channelID], cmd , NULL);

        memset(cmd, 0, sizeof(cmd));
        RLOGD("requestInitISIM confuri = \"%s\"", strings[0]);
        snprintf(cmd, sizeof(cmd), "AT+CONFURI=0,\"%s\"", strings[0]);
        err = at_send_command(s_ATChannels[channelID], cmd , NULL);

        RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
        RIL_onRequestComplete(t, RIL_E_SUCCESS, &s_imsConn[socket_id],
                sizeof(int));
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
}

int processSimRequests(int request, void *data, size_t datalen, RIL_Token t,
                          int channelID) {
    int err;
    ATResponse *p_response = NULL;

    switch (request) {
        case RIL_REQUEST_GET_SIM_STATUS: {
            RIL_CardStatus_v6 *p_card_status;
            char *p_buffer;
            int buffer_size;

            sem_wait(&s_sem);
            int result = getCardStatus(channelID, &p_card_status);
            sem_post(&s_sem);
            if (result == RIL_E_SUCCESS) {
                p_buffer = (char *)p_card_status;
                buffer_size = sizeof(*p_card_status);
            } else {
                p_buffer = NULL;
                buffer_size = 0;
            }
            RIL_onRequestComplete(t, result, p_buffer, buffer_size);
            freeCardStatus(p_card_status);
            break;
        }
        case RIL_REQUEST_ENTER_SIM_PIN :
        case RIL_REQUEST_ENTER_SIM_PUK :
        case RIL_REQUEST_ENTER_SIM_PIN2 :
            requestEnterSimPin(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_ENTER_SIM_PUK2 :
            requestEnterSimPuk2(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_CHANGE_SIM_PIN :
            requestChangeSimPin(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_CHANGE_SIM_PIN2 :
            requestChangeSimPin2(channelID, data, datalen, t);
            break;
        // case RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION :
        //    break;
        case RIL_REQUEST_GET_IMSI: {
            p_response = NULL;
            err = at_send_command_numeric(s_ATChannels[channelID], "AT+CIMI",
                                          &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS,
                        p_response->p_intermediates->line,
                        strlen(p_response->p_intermediates->line) + 1);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_REQUEST_SIM_IO:
            requestSIM_IO(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_QUERY_FACILITY_LOCK: {
            char *lockData[4];
            lockData[0] = ((char **)data)[0];
            lockData[1] = FACILITY_LOCK_REQUEST;
            lockData[2] = ((char **)data)[1];
            lockData[3] = ((char **)data)[2];
            requestFacilityLock(channelID, lockData, datalen + sizeof(char *),
                                t);
            break;
        }
        case RIL_REQUEST_SET_FACILITY_LOCK:
            requestFacilityLock(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_SIM_TRANSMIT_APDU_BASIC:
            requestTransmitApduBasic(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_SIM_OPEN_CHANNEL:
            requestOpenLogicalChannel(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_SIM_CLOSE_CHANNEL:
            requestCloseLogicalChannel(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_SIM_TRANSMIT_APDU_CHANNEL:
            requestTransmitApdu(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_SIM_AUTHENTICATION: {
            RIL_SimAuthentication *sim_auth = (RIL_SimAuthentication *)data;
            RLOGD("RIL_REQUEST_SIM_AUTHENTICATION authContext = %d,"
                  "rand_autn = %s, aid = %s", sim_auth->authContext,
                  sim_auth->authData, sim_auth->aid);
            if (sim_auth->authContext == AUTH_CONTEXT_EAP_AKA) {
                requestUSimAuthentication(channelID, sim_auth->authData, t);
            } else if (sim_auth->authContext == AUTH_CONTEXT_EAP_SIM) {
                requestSimAuthentication(channelID, sim_auth->authData, t);
            } else {
                RLOGD("invalid authContext");
            }
            break;
        }
        /* IMS request @{ */
        case RIL_REQUEST_INIT_ISIM:
            requestInitISIM(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_ENABLE_IMS: {
            err = at_send_command(s_ATChannels[channelID], "AT+IMSEN=1" , NULL);
            if (err < 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            break;
        }
        case RIL_REQUEST_DISABLE_IMS: {
            err = at_send_command(s_ATChannels[channelID], "AT+IMSEN=0" , NULL);
            if (err < 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            break;
        }
        /* }@ */
        default:
            return 0;
    }

    return 1;
}

static void onSimAbsent(void *param) {
    int channelID;
    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);

    channelID = getChannel(socket_id);
    if (s_radioState[socket_id] == RADIO_STATE_SIM_NOT_READY ||
        s_radioState[socket_id] == RADIO_STATE_SIM_READY) {
        setRadioState(channelID, RADIO_STATE_SIM_LOCKED_OR_ABSENT);
    }
    putChannel(channelID);

    RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED, NULL, 0,
                              socket_id);
}

static void onSimPresent(void *param) {
    int channelID;
    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);

    channelID = getChannel(socket_id);
    if (isRadioOn(channelID) > 0) {
        setRadioState(channelID, RADIO_STATE_SIM_NOT_READY);
    }
    putChannel(channelID);

    if (s_radioState[socket_id] == RADIO_STATE_SIM_LOCKED_OR_ABSENT ||
        s_radioState[socket_id] == RADIO_STATE_OFF) {
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED, NULL,
                                  0, socket_id);
    }
}

void onSimStatusChanged(RIL_SOCKET_ID socket_id, const char *s) {
    int err;
    int type;
    int value = 0, cause = -1;
    char *line = NULL;
    char *tmp;

    line = strdup(s);
    tmp = line;
    at_tok_start(&tmp);

    err = at_tok_nextint(&tmp, &type);
    if (err < 0) goto out;

    if (type == 3) {
        if (at_tok_hasmore(&tmp)) {
            err = at_tok_nextint(&tmp, &value);
            if (err < 0) goto out;
            if (value == 1) {
                if (at_tok_hasmore(&tmp)) {
                    err = at_tok_nextint(&tmp, &cause);
                    if (err < 0) goto out;
                    if (cause == 2) {
                        s_simState[socket_id] = SIM_DROP;
                        RIL_requestTimedCallback(onSimAbsent,
                                (void *)&s_socketId[socket_id], NULL);
                    }
                    if (cause == 34) {  // sim removed
                        s_simState[socket_id] = SIM_REMOVE;
                        RIL_requestTimedCallback(onSimAbsent,
                                (void *)&s_socketId[socket_id], NULL);
                    }
                    if (cause == 1) {  // no sim card
                        RIL_onUnsolicitedResponse(
                                RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED, NULL, 0,
                                socket_id);
                    }
                }
            } else if (value == 100 || value == 4) {
                RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED,
                                          NULL, 0, socket_id);
            } else if (value == 0 || value == 2) {
                RIL_requestTimedCallback(onSimPresent,
                                         (void *)&s_socketId[socket_id], NULL);
            }
        }
    }

out:
    free(line);
}

int processSimUnsolicited(RIL_SOCKET_ID socket_id, const char *s) {
    if (strStartsWith(s, "+ECIND:")) {
        onSimStatusChanged(socket_id, s);
    } else {
        return 0;
    }

    return 1;
}
