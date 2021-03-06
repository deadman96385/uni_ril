/**
 * ril_sim.c --- SIM-related requests process functions implementation
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#define LOG_TAG "RIL"

#include "sprd_ril.h"
#include "ril_sim.h"
#include "ril_stk.h"
#include "ril_utils.h"
#include "ril_network.h"
#include "custom/ril_custom.h"
#include "ril_async_cmd_handler.h"

/* Property to save pin for modem assert */
#define SIM_PIN_PROP                            "vendor.ril.sim.pin"
#define MODEM_ASSERT_PROP                       "vendor.ril.modem.assert"
#define AUTO_SAVE_PIN                           "persist.vendor.radio.mdrec.simpin.cache"
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
#define READ_BINERY                             0xb0
#define DF_ADF                                  "3F007FFF"
#define DF_GSM                                  "3F007F20"
#define EFID_SST                                0x6f38

#define SIM_DROP                                1
#define SIM_REMOVE                              2

#define AUTH_CONTEXT_EAP_SIM                    128
#define AUTH_CONTEXT_EAP_AKA                    129
#define SIM_AUTH_RESPONSE_SUCCESS               0
#define SIM_AUTH_RESPONSE_SYNC_FAILURE          3

#define REQUEST_SIMLOCK_WHITE_LIST_PS           1
#define REQUEST_SIMLOCK_WHITE_LIST_PN           2
#define REQUEST_SIMLOCK_WHITE_LIST_PU           3
#define REQUEST_SIMLOCK_WHITE_LIST_PP           4
#define REQUEST_SIMLOCK_WHITE_LIST_PC           5
#define WHITE_LIST_HEAD_LENGTH                  5
#define WHITE_LIST_PS_PART_LENGTH               (19 + 1)
#define WHITE_LIST_COLUMN                       17
#define IMSI_VAL_NUM                            8
#define IMSI_TOTAL_LEN                          (16 + 1)
#define SMALL_IMSI_LEN                          (2 + 1)
#define READ_BINERY                             176
#define SIMLOCK_ATTEMPT_TIMES_PROP              "vendor.sim.attempttimes.%s"

static pthread_mutex_t s_remainTimesMutex = PTHREAD_MUTEX_INITIALIZER;
RIL_AppType s_appType[SIM_COUNT];
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
static bool s_needQueryPinPuk2Times[SIM_COUNT] = {
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
int s_imsInitISIM[SIM_COUNT] = {
        -1
#if (SIM_COUNT >= 2)
       ,-1
#if (SIM_COUNT >= 3)
       ,-1
#if (SIM_COUNT >= 4)
       ,-1
#endif
#endif
#endif
        };

const char *base64char =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int s_simSessionId[SIM_COUNT] = {
        -1
#if (SIM_COUNT >= 2)
       ,-1
#if (SIM_COUNT >= 3)
       ,-1
#if (SIM_COUNT >= 4)
       ,-1
#endif
#endif
#endif
};

static pthread_mutex_t s_simStatusMutex[SIM_COUNT] = {
        PTHREAD_MUTEX_INITIALIZER
#if (SIM_COUNT >= 2)
       ,PTHREAD_MUTEX_INITIALIZER
#if (SIM_COUNT >= 3)
       ,PTHREAD_MUTEX_INITIALIZER
#if (SIM_COUNT >= 4)
       ,PTHREAD_MUTEX_INITIALIZER
#endif
#endif
#endif
};

static void setSIMPowerOff(void *param);
static int queryFDNServiceAvailable(int channelID);
int initISIM(int channelID);

static bool needCacheSimPin() {
    bool ret = false;
    char prop[PROPERTY_VALUE_MAX] = { 0 };
    property_get(AUTO_SAVE_PIN, prop, "0");
    if (0 == strcmp(prop, "1")) {
        ret = true;
    }
    return ret;
}

void onModemReset_Sim() {
    RIL_SOCKET_ID socket_id  = 0;
    s_presentSIMCount = 0;

    for (socket_id = RIL_SOCKET_1; socket_id < RIL_SOCKET_NUM; socket_id++) {
        s_isSimPresent[socket_id] = 0;
        s_imsInitISIM[socket_id] = -1;
        s_simSessionId[socket_id] = -1;
        s_appType[socket_id] = 0;

        if (s_simBusy[socket_id].s_sim_busy) {
            pthread_mutex_lock(&s_simBusy[socket_id].s_sim_busy_mutex);
            s_simBusy[socket_id].s_sim_busy = false;
            pthread_cond_signal(&s_simBusy[socket_id].s_sim_busy_cond);
            pthread_mutex_unlock(&s_simBusy[socket_id].s_sim_busy_mutex);
        }
    }
}

static int getSimlockRemainTimes(int channelID, SimUnlockType type) {
    int err, result;
    int remaintime = 3;
    char cmd[AT_COMMAND_LEN] = {0};
    char *line;
    ATResponse *p_response = NULL;

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
            }
        }
    }
    at_response_free(p_response);

    /* Bug 523208 set pin/puk remain times to prop. @{ */
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
    pthread_mutex_lock(&s_remainTimesMutex);
    setPinPukRemainTimes(type, remaintime, socket_id);
    pthread_mutex_unlock(&s_remainTimesMutex);
    /* }@ */
    return remaintime;
}

static void getSIMStatusAgainForSimBusy(void *param) {
    ATResponse *p_response = NULL;
    int err;
    int channelID;

    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    if ((int)socket_id < 0 || (int)socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        return;
    }

    channelID = getChannel(socket_id);

    if (s_radioState[socket_id] == RADIO_STATE_UNAVAILABLE) {
        goto done;
    }
    err = at_send_command_singleline(s_ATChannels[channelID],
            "AT+CPIN?", "+CPIN:", &p_response);

    if (err != 0) {
        goto done;
    }
    switch (at_get_cme_error(p_response)) {
        case CME_SIM_BUSY:
            RIL_requestTimedCallback(getSIMStatusAgainForSimBusy,
                    (void *)&s_socketId[socket_id], &TIMEVAL_SIMPOLL);
            goto done;
        default:
            if (s_simBusy[socket_id].s_sim_busy) {
                pthread_mutex_lock(&s_simBusy[socket_id].s_sim_busy_mutex);
                s_simBusy[socket_id].s_sim_busy = false;
                pthread_cond_signal(&s_simBusy[socket_id].s_sim_busy_cond);
                pthread_mutex_unlock(&s_simBusy[socket_id].s_sim_busy_mutex);
            }
            RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED,
                    NULL, 0, socket_id);
            RIL_onUnsolicitedResponse(RIL_EXT_UNSOL_SIMMGR_SIM_STATUS_CHANGED,
                    NULL, 0, socket_id);
            goto done;
    }
done:
    putChannel(channelID);
    at_response_free(p_response);
    return;
}

void encryptPin(int len, char *pin, unsigned char encryptPin[17]) {
    int encryptArray[10][10] = {
            {'r', 's', 'e', 'f', 'a', 'q', 't', 'd', 'w', 'c'},
            {'1', '6', '2', '3', '0', '5', '4', '8', '9', '7'},
            {'z', 's', 'e', 't', 'g', 'j', 'k', 'n', 'p', 'c'},
            {'a', 'u', 'e', 'n', 'z', 'k', 'd', 'm', 'r', 'c'},
            {'u', 't', 'e', 's', 'd', 'g', 'k', 'b', 'c', 'z'},
            {'e', 'r', 'i', 'f', 'd', 'j', 'l', 'm', 'c', 'x'},
            {'5', '2', '7', '8', '4', '1', '3', '6', '0', '9'},
            {'z', 's', 'e', 't', 'g', 'j', 'k', 'n', 'p', 'c'},
            {'1', '6', '2', '3', '0', '5', '4', '8', '9', '7'},
            {'s', 'd', 'f', 'z', 'w', 'e', 't', 'j', 'l', 'c'}};

    int randEncryptArray[10] =
            {'p', 'a', 'r', 'k', 'y', 'o', 'u', 'n', 'g', 'j'};

    int lenData = len;
    int i = 0;
    int offset = 0;

    encryptPin[offset] = randEncryptArray[lenData];
    offset++;

    srand((unsigned int)time(0));

    for (i = 0; i < 8; i++) {
        int code = 0;
        int randVal = rand() % 10;
        encryptPin[offset] = randEncryptArray[randVal];
        offset++;

        if (i < lenData) {
            code = pin[i] - 0x30;
        } else {
            code = rand() % 10;
        }
        encryptPin[offset] = encryptArray[randVal][code];
        offset++;
    }
    return;
}

void decryptPin(char *pin, unsigned char encryptedPin[17]) {
    int encryptArray[10][10] = {
            {'r', 's', 'e', 'f', 'a', 'q', 't', 'd', 'w', 'c'},
            {'1', '6', '2', '3', '0', '5', '4', '8', '9', '7'},
            {'z', 's', 'e', 't', 'g', 'j', 'k', 'n', 'p', 'c'},
            {'a', 'u', 'e', 'n', 'z', 'k', 'd', 'm', 'r', 'c'},
            {'u', 't', 'e', 's', 'd', 'g', 'k', 'b', 'c', 'z'},
            {'e', 'r', 'i', 'f', 'd', 'j', 'l', 'm', 'c', 'x'},
            {'5', '2', '7', '8', '4', '1', '3', '6', '0', '9'},
            {'z', 's', 'e', 't', 'g', 'j', 'k', 'n', 'p', 'c'},
            {'1', '6', '2', '3', '0', '5', '4', '8', '9', '7'},
            {'s', 'd', 'f', 'z', 'w', 'e', 't', 'j', 'l', 'c'}};

    int randEncryptArray[10] =
            {'p', 'a', 'r', 'k', 'y', 'o', 'u', 'n', 'g', 'j'};

    int i = 0;
    int j = 0;
    int offset = 0;
    int pinLen = -1;
    for (i = 0; i < 10; i++) {
        if (randEncryptArray[i] == encryptedPin[offset]) {
            pinLen = i;
        }
    }
    if (pinLen == -1) {
        RLOGD("Cant find SR Len");
        return;
    }
    offset++;
    for (i = 0; i < pinLen; i++) {
        int randVal = -1;
        for (j = 0; j < 10; j++) {
            if (randEncryptArray[j] == encryptedPin[offset]) {
                randVal = j;
            }
        }
        if (randVal == -1) {
            RLOGD("Cant find Val");
            return;
        }
        offset++;
        for (j = 0; j < 10; j++) {
            if (encryptArray[randVal][j] == encryptedPin[offset]) {
                pin[i] = j + 0x30;
                offset++;
                break;
            }
        }
        if (j == 10) {
            RLOGD("Cant find the Code");
            return;
        }
    }

    return;
}

/* Returns SIM_NOT_READY on error */
SimStatus getSIMStatus(int request, int channelID) {
    ATResponse *p_response = NULL;
    int err;
    int ret = SIM_NOT_READY;
    char *cpinLine;
    char *cpinResult;

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
    pthread_mutex_lock(&s_simStatusMutex[socket_id]);

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
        case CME_SIM_BUSY:
            ret = SIM_NOT_READY;
            if (!s_simBusy[socket_id].s_sim_busy) {
                s_simBusy[socket_id].s_sim_busy = true;
                RIL_requestTimedCallback(getSIMStatusAgainForSimBusy,
                    (void *)&s_socketId[socket_id], &TIMEVAL_SIMPOLL);
            }
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

    if (0 == strcmp(cpinResult, "SIM PIN")) {
        if (needCacheSimPin()) {
            char modemAssertProp[PROPERTY_VALUE_MAX];

            getProperty(socket_id, MODEM_ASSERT_PROP, modemAssertProp, "0");
            if (strcmp(modemAssertProp, "1") == 0) {
                setProperty(socket_id, MODEM_ASSERT_PROP, "0");

                char cmd[AT_COMMAND_LEN];
                char pin[PROPERTY_VALUE_MAX];
                char encryptedPin[PROPERTY_VALUE_MAX];
                ATResponse *p_resp = NULL;

                memset(pin, 0, sizeof(pin));
                getProperty(socket_id, SIM_PIN_PROP, encryptedPin, "");
                decryptPin(pin, (unsigned char *) encryptedPin);

                if (strlen(pin) != 4) {
                    goto out;
                } else {
                    snprintf(cmd, sizeof(cmd), "AT+CPIN=%s", pin);
                    err = at_send_command(s_ATChannels[channelID], cmd, &p_resp);
                    if (err < 0 || p_resp->success == 0) {
                        at_response_free(p_resp);
                        goto out;
                    }
                    at_response_free(p_resp);
                    ret = SIM_NOT_READY;
                    goto done;
                }
            }
        }
out:
        ret = SIM_PIN;
        goto done;
    } else if (0 == strcmp(cpinResult, "SIM PUK")) {
        ret = SIM_PUK;
        goto done;
    } else if (0 == strcmp(cpinResult, "PH-NET PIN")) {
        ret = SIM_NETWORK_PERSONALIZATION;
        goto done;
    } else if (0 == strcmp(cpinResult, "PH-NETSUB PIN")) {
        ret = SIM_NETWORK_SUBSET_PERSONALIZATION;
        goto done;
    } else if (0 == strcmp(cpinResult, "PH-SP PIN")) {
        ret = SIM_SERVICE_PROVIDER_PERSONALIZATION;
        goto done;
    } else if (0 == strcmp(cpinResult, "PH-CORP PIN")) {
        ret = SIM_CORPORATE_PERSONALIZATION;
        goto done;
    } else if (0 == strcmp(cpinResult, "PH-SIM PIN")) {
        ret = SIM_SIM_PERSONALIZATION;
        goto done;
    } else if (0 == strcmp(cpinResult, "PH-NET PUK")) {
        ret = SIM_NETWORK_PUK;
        goto done;
    } else if (0 == strcmp(cpinResult, "PH-NETSUB PUK")) {
        ret = SIM_NETWORK_SUBSET_PUK;
        goto done;
    } else if (0 == strcmp(cpinResult, "PH-SP PUK")) {
        ret = SIM_SERVICE_PROVIDER_PUK;
        goto done;
    } else if (0 == strcmp(cpinResult, "PH-CORP PUK")) {
        ret = SIM_CORPORATE_PUK;
        goto done;
    } else if (0 == strcmp(cpinResult, "PH-SIM PUK")) {
        ret = SIM_SIM_PUK;
        goto done;
    } else if (0 == strcmp(cpinResult, "PH-INTEGRITY FAIL")) {
        ret = SIM_SIMLOCK_FOREVER;
        goto done;
    } else if (0 == strcmp(cpinResult, "PIN1_BLOCK_PUK1_BLOCK")) {
        ret = SIM_PERM_BLOCK;
        goto done;
    } else if (0 != strcmp(cpinResult, "READY")) {
        /* we're treating unsupported lock types as "sim absent" */
        ret = SIM_ABSENT;
        goto done;
    }

    ret = SIM_READY;

done:
    at_response_free(p_response);
    if (ret != SIM_ABSENT) {
        char simEnabledProp[PROPERTY_VALUE_MAX] = {0};
        getProperty(socket_id, SIM_ENABLED_PROP, simEnabledProp, "1");
        if (request != RIL_EXT_REQUEST_SIMMGR_GET_SIM_STATUS &&
                strcmp(simEnabledProp, "0") == 0) {
            ret = SIM_ABSENT;
        }
    }

    if (ret == SIM_ABSENT) {
        setSimPresent(socket_id, ABSENT);
    } else if (ret == SIM_NOT_READY &&
           s_simBusy[socket_id].s_sim_busy == true) {
        setSimPresent(socket_id, SIM_UNKNOWN);
    } else {
        setSimPresent(socket_id, PRESENT);
    }

    /** SPRD: Bug 523208 set pin/puk remain times to prop. @{*/
    if ((s_needQueryPinTimes[socket_id] && (ret == SIM_PIN || ret == SIM_READY))
            || (s_needQueryPukTimes[socket_id] && ret == SIM_PUK)) {
        if (ret == SIM_PIN || ret == SIM_READY) {
            s_needQueryPinTimes[socket_id] = false;
        } else {
            s_needQueryPukTimes[socket_id] = false;
        }
        int remaintime = getSimlockRemainTimes(channelID,
                ret == SIM_PUK ? UNLOCK_PUK : UNLOCK_PIN);
    } else if (s_needQueryPinPuk2Times[socket_id] && ret == SIM_READY) {
        s_needQueryPinPuk2Times[socket_id] = false;
        getSimlockRemainTimes(channelID, UNLOCK_PIN2);
        getSimlockRemainTimes(channelID, UNLOCK_PUK2);
    } else if (ret == SIM_ABSENT) {
        s_needQueryPinTimes[socket_id] = true;
        s_needQueryPukTimes[socket_id] = true;
        s_needQueryPinPuk2Times[socket_id] = true;
        s_imsInitISIM[socket_id] = -1;
        s_simSessionId[socket_id] = -1;
    }
    /** }@ */
    pthread_mutex_unlock(&s_simStatusMutex[socket_id]);

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
static int getCardStatus(int request, int channelID,
                         RIL_CardStatus_v6 **pp_card_status) {
    static RIL_AppStatus app_status_array[] = {
        // SIM_ABSENT = 0
        {RIL_APPTYPE_UNKNOWN, RIL_APPSTATE_UNKNOWN, RIL_PERSOSUBSTATE_UNKNOWN,
         NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN},
        // SIM_NOT_READY = 1
        {RIL_APPTYPE_USIM, RIL_APPSTATE_DETECTED, RIL_PERSOSUBSTATE_UNKNOWN,
         NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN},
        // SIM_READY = 2
        {RIL_APPTYPE_USIM, RIL_APPSTATE_READY, RIL_PERSOSUBSTATE_READY,
         NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN},
        // SIM_PIN = 3
        {RIL_APPTYPE_USIM, RIL_APPSTATE_PIN, RIL_PERSOSUBSTATE_UNKNOWN,
         NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN},
        // SIM_PUK = 4
        {RIL_APPTYPE_USIM, RIL_APPSTATE_PUK, RIL_PERSOSUBSTATE_UNKNOWN,
         NULL, NULL, 0, RIL_PINSTATE_ENABLED_BLOCKED, RIL_PINSTATE_UNKNOWN},
        // SIM_NETWORK_PERSONALIZATION = 5
        {RIL_APPTYPE_USIM, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_NETWORK,
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
         NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN},
        // SIM_NETWORK_SUBSET_PERSONALIZATION = EXT_SIM_STATUS_BASE + 1
        {RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO,
         RIL_PERSOSUBSTATE_SIM_NETWORK_SUBSET, NULL, NULL, 0,
         RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN},
        // SIM_SERVICE_PROVIDER_PERSONALIZATION = EXT_SIM_STATUS_BASE + 2
        {RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO,
         RIL_PERSOSUBSTATE_SIM_SERVICE_PROVIDER, NULL, NULL, 0,
         RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN},
        // SIM_CORPORATE_PERSONALIZATION = EXT_SIM_STATUS_BASE + 3
        {RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO,
         RIL_PERSOSUBSTATE_SIM_CORPORATE, NULL, NULL, 0,
         RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN},
        // SIM_SIM_PERSONALIZATION = EXT_SIM_STATUS_BASE + 4
        {RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO,
         RIL_PERSOSUBSTATE_SIM_SIM, NULL, NULL, 0,
         RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN},
        // PERSOSUBSTATE_SIM_NETWORK_PUK = EXT_SIM_STATUS_BASE + 5
        {RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO,
         RIL_PERSOSUBSTATE_SIM_NETWORK_PUK, NULL, NULL, 0,
         RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN},
        // PERSOSUBSTATE_SIM_NETWORK_SUBSET_PUK = EXT_SIM_STATUS_BASE + 6
        {RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO,
         RIL_PERSOSUBSTATE_SIM_NETWORK_SUBSET_PUK, NULL, NULL, 0,
         RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN},
        // PERSOSUBSTATE_SIM_SERVICE_PROVIDER_PUK = EXT_SIM_STATUS_BASE + 7
        {RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO,
         RIL_PERSOSUBSTATE_SIM_SERVICE_PROVIDER_PUK, NULL, NULL, 0,
         RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN},
        // PERSOSUBSTATE_SIM_CORPORATE_PUK = EXT_SIM_STATUS_BASE + 8
        {RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO,
         RIL_PERSOSUBSTATE_SIM_CORPORATE_PUK, NULL, NULL, 0,
         RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN},
        // PERSOSUBSTATE_SIM_SIM_PUK = EXT_SIM_STATUS_BASE + 9
        {RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO,
         RIL_PERSOSUBSTATE_SIM_SIM_PUK, NULL, NULL, 0,
         RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN},
        // PERSOSUBSTATE_SIMLOCK_FOREVER = EXT_SIM_STATUS_BASE + 10
        {RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO,
         RIL_PERSOSUBSTATE_SIMLOCK_FOREVER, NULL, NULL, 0,
         RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN},
         // SIM_PERM_BLOCK = EXT_SIM_STATUS_BASE + 11
         { RIL_APPTYPE_SIM, RIL_APPSTATE_PUK,
         RIL_PERSOSUBSTATE_UNKNOWN, NULL, NULL, 0,
         RIL_PINSTATE_ENABLED_PERM_BLOCKED, RIL_PINSTATE_UNKNOWN }
    };
    static RIL_AppStatus ims_app_status_array[] = {
        {RIL_APPTYPE_ISIM, RIL_APPSTATE_READY, RIL_PERSOSUBSTATE_READY,
         NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN}
    };

    RIL_CardState card_state;
    int num_apps;
    int sim_status;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
    char simEnabledProp[PROPERTY_VALUE_MAX] = {0};

    getProperty(socket_id, SIM_ENABLED_PROP, simEnabledProp, "1");
    if (request != RIL_EXT_REQUEST_SIMMGR_GET_SIM_STATUS &&
            strcmp(simEnabledProp, "0") == 0) {
        sim_status = SIM_ABSENT;
    } else {
        sim_status = getSIMStatus(request, channelID);
    }

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
    p_card_status->gsm_umts_subscription_app_index = -1;
    p_card_status->cdma_subscription_app_index = -1;
    p_card_status->ims_subscription_app_index = -1;
    p_card_status->num_applications = num_apps;

    s_appType[socket_id] = getSimType(channelID);

    int isimResp = 0;
    if (sim_status == SIM_READY && s_appType[socket_id] == RIL_APPTYPE_USIM) {
        isimResp = initISIM(channelID);
        RLOGD("app type %d", isimResp);
    }

    /* Initialize application status */
    unsigned int i;
    for (i = 0; i < RIL_CARD_MAX_APPS; i++) {
        p_card_status->applications[i] = app_status_array[SIM_ABSENT];
    }

    for (i = 0; i < sizeof(app_status_array) / sizeof(RIL_AppStatus); i++) {
        app_status_array[i].app_type = s_appType[socket_id];
    }
    /* Pickup the appropriate application status
     * that reflects sim_status for gsm.
     */
    if (num_apps != 0) {
        if (isimResp != 1)  {
            /* Only support one app, gsm */
            p_card_status->num_applications = 1;
            p_card_status->gsm_umts_subscription_app_index = 0;

            /* Get the correct app status */
            p_card_status->applications[0] = app_status_array[sim_status];
        } else {
            p_card_status->num_applications = 2;
            p_card_status->gsm_umts_subscription_app_index = 0;
            p_card_status->ims_subscription_app_index = 1;
            p_card_status->applications[0] = app_status_array[sim_status];
            p_card_status->applications[1] = ims_app_status_array[0];
        }
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


void setSimLockAttemptTimes(int type, int attemptTimes,
                            RIL_SOCKET_ID socketId) {
    char num[ARRAY_SIZE] = {0};
    char prop[PROPERTY_VALUE_MAX] = {0};
    static char s_simlockType [5][4] = {"PS", "PN", "PU", "PP", "PC"};
    if (type > 0 && type <= 5) {
        snprintf(prop, sizeof(prop), SIMLOCK_ATTEMPT_TIMES_PROP,
                 s_simlockType[type - 1]);
        RLOGD("set %s, attemptTimes = %d for SIM%d", prop, attemptTimes, socketId);
        snprintf(num, sizeof(num), "%d", attemptTimes);
        setProperty(socketId, prop, num);
    } else {
        RLOGE("invalid type: %d", type);
    }
}


int getNetLockRemainTimes(int channelID, int type) {
    int err;
    int ret = -1;
    int fac = type;
    int ck_type = 1;
    int result[2] = {0, 0};
    char *line;
    char prop[PROPERTY_VALUE_MAX] = {0};
    char cmd[AT_COMMAND_LEN] = {0};
    ATResponse *p_response = NULL;

    RLOGD("[MBBMS] fac:%d, ck_type:%d", fac, ck_type);
    snprintf(cmd, sizeof(cmd), "AT+SPSMPN=%d,%d", fac, ck_type);
    err = at_send_command_singleline(s_ATChannels[channelID], cmd, "+SPSMPN:",
                                     &p_response);
    if (err < 0 || p_response->success == 0) {
        ret = -1;
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
            RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
            pthread_mutex_lock(&s_remainTimesMutex);
            setSimLockAttemptTimes(type, result[1], socket_id);
            pthread_mutex_unlock(&s_remainTimesMutex);
        } else {
            ret = -1;
        }
    }
    at_response_free(p_response);
    return ret;
}

int getRemainTimes(int channelID, char *type) {
    if (type == NULL) {
        RLOGE("type is null, return -1");
        return -1;
    } else if (0 == strcmp(type, "PS")) {
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
        RLOGE("wrong type %s, return -1", type);
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

        if (datalen == 2 * sizeof(char *)) {
            pin = strings[0];
        } else if (datalen == 3 * sizeof(char *)) {
            pin = strings[1];
        } else {
            goto out;
        }

        if ((pin != NULL) && needCacheSimPin()) {
            unsigned char encryptedPin[ARRAY_SIZE];
            memset(encryptedPin, 0, sizeof(encryptedPin));
            encryptPin(strlen(pin), (char *)pin, encryptedPin);
            setProperty(socket_id, SIM_PIN_PROP, (const char *)encryptedPin);
        }

out:
        remaintime = getSimlockRemainTimes(channelID, rsqtype);
        if (UNLOCK_PUK == rsqtype) {
            getSimlockRemainTimes(channelID, UNLOCK_PIN);
        }
        RIL_onRequestComplete(t, RIL_E_SUCCESS, &remaintime,
                sizeof(remaintime));
        simstatus = getSIMStatus(-1, channelID);
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

static void requestEnterSimPin2(int channelID, void *data, size_t datalen,
                                RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err = -1;
    int remaintimes = 3;
    char cmd[AT_COMMAND_LEN];
    ATResponse *p_response = NULL;
    SimUnlockType rsqtype = UNLOCK_PIN2;
    RIL_Errno errnoType = RIL_E_PASSWORD_INCORRECT;

    const char **pin2 = (const char **)data;
    snprintf(cmd, sizeof(cmd), "AT+ECPIN2=\"%s\"", pin2[0]);
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (err < 0 || p_response->success == 0) {
        errnoType = RIL_E_PASSWORD_INCORRECT;
        goto out;
    }

    errnoType = RIL_E_SUCCESS;

out:
    remaintimes = getSimlockRemainTimes(channelID, rsqtype);
    RIL_onRequestComplete(t, errnoType, &remaintimes,
                          sizeof(remaintimes));
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
    getSimlockRemainTimes(channelID, UNLOCK_PUK2);
    getSimlockRemainTimes(channelID, UNLOCK_PIN2);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);

    simstatus = getSIMStatus(-1, channelID);
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
    char *cpinLine = NULL, *cpinResult = NULL;
    const char **strings = (const char **)data;
    ATResponse *p_response = NULL;

    if (datalen == 3 * sizeof(char *)) {
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

        if (0 == strcmp(cpinResult, "SIM PIN")) {
            ret = asprintf(&cmd, "ATD**04*%s*%s*%s#", strings[0], strings[1],
                            strings[1]);
        } else {
            ret = asprintf(&cmd, "AT+CPWD=\"SC\",\"%s\",\"%s\"", strings[0],
                            strings[1]);
        }
        AT_RESPONSE_FREE(p_response);
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
    remaintime = getSimlockRemainTimes(channelID, UNLOCK_PIN);
    if (err < 0 || p_response->success == 0) {
        goto error;
    } else {
        /* add for modem reboot */
        if (needCacheSimPin()) {
            const char *pin = NULL;
            pin = strings[1];
            unsigned char encryptedPin[ARRAY_SIZE];

            RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
            memset(encryptedPin, 0, sizeof(encryptedPin));
            encryptPin(strlen(pin), (char *) pin, encryptedPin);
            setProperty(socket_id, SIM_PIN_PROP, (const char *) encryptedPin);
        }
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_PASSWORD_INCORRECT, &remaintime,
                          sizeof(remaintime));
    at_response_free(p_response);
}

static void requestChangeSimPin2(int channelID, void *data, size_t datalen,
                                     RIL_Token t) {
    int err, ret;
    int remaintime = 3;
    char *cmd = NULL;
    char *cpinLine = NULL, *cpinResult = NULL;
    const char **strings = (const char **)data;
    ATResponse *p_response = NULL;

    if (datalen == 3 * sizeof(char *)) {
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

        if (0 == strcmp(cpinResult, "SIM PIN")) {
            ret = asprintf(&cmd, "ATD**042*%s*%s*%s#", strings[0], strings[1],
                            strings[1]);
        } else {
            ret = asprintf(&cmd, "AT+CPWD=\"P2\",\"%s\",\"%s\"", strings[0],
                            strings[1]);
        }
        AT_RESPONSE_FREE(p_response);
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
    remaintime = getSimlockRemainTimes(channelID, UNLOCK_PIN2);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_PASSWORD_INCORRECT, &remaintime,
                          sizeof(remaintime));
    at_response_free(p_response);
}

static void requestFacilityLock(int request, int channelID, char **data,
                                size_t datalen, RIL_Token t) {
    int err, result, status;
    int serviceClass = 0;
    int ret = -1;
    int errNum = -1;
    int remainTimes = 10;
    int response[2] = {0};
    char *cmd, *line;
    ATLine *p_cur;
    ATResponse *p_response = NULL;
    RIL_Errno errnoType = RIL_E_GENERIC_FAILURE;

    char *type = data[0];
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    if (datalen != 5 * sizeof(char *)) {
        goto error1;
    }
    if (data[0] == NULL || data[1] == NULL ||
       (data[2] == NULL && request == RIL_REQUEST_SET_FACILITY_LOCK) ||
        strlen(data[0]) == 0 || strlen(data[1]) == 0 ||
       (request == RIL_REQUEST_SET_FACILITY_LOCK && strlen(data[2]) == 0 )) {
        errnoType = RIL_E_INVALID_ARGUMENTS;
        RLOGE("FacilityLock invalid arguments");
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

    if (*data[1] == '2') {  // query status
        err = at_send_command_multiline(s_ATChannels[channelID], cmd, "+CLCK: ",
                                        &p_response);
        free(cmd);

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
        if (0 == strcmp(data[0], "FD")) {
            if (queryFDNServiceAvailable(channelID) == 2) {
                response[0] = 2;
            }
        }

        if (request == RIL_REQUEST_EXT_QUERY_FACILITY_LOCK) {
            RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));
        } else {
            RIL_onRequestComplete(t, RIL_E_SUCCESS, &response[0], sizeof(int));
        }

    } else {  // unlock/lock this facility
        const char *str = "+CLCK:";
        if (!strcmp(data[0], "FD")) {
            int *mode = (int *) malloc(sizeof(int));
            *mode = atoi(data[1]);
            // timeout is in seconds
            RLOGD("addAsyncCmdList");
            addAsyncCmdList(socket_id, t, str, (void *) mode, 10);
        }
        err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
        free(cmd);
        if (err < 0 || p_response->success == 0) {
            if (!strcmp(data[0], "FD")) {
                RLOGD("removeAsyncCmdList");
                removeAsyncCmdList(t, str);
            }
            goto error;
        } else if (!strcmp(data[0], "SC")) {
            /* add for modem reboot */
            if (needCacheSimPin()) {
                char *pin = NULL;
                pin = data[2];
                unsigned char encryptedPin[ARRAY_SIZE];
                memset(encryptedPin, 0, sizeof(encryptedPin));
                encryptPin(strlen(pin), pin, encryptedPin);

                setProperty(socket_id, SIM_PIN_PROP, (const char *)encryptedPin);
            }
            getSimlockRemainTimes(channelID, UNLOCK_PIN);
        } else if (!strcmp(data[0], "FD")) {
            getSimlockRemainTimes(channelID, UNLOCK_PIN2);
            goto done;
        }
        result = 1;
        RIL_onRequestComplete(t, RIL_E_SUCCESS, &result, sizeof(result));
    }
done:
    at_response_free(p_response);
    return;

error:
    if (p_response != NULL &&
            strStartsWith(p_response->finalResponse, "+CME ERROR:")) {
        line = p_response->finalResponse;
        err = at_tok_start(&line);
        if (err >= 0) {
            err = at_tok_nextint(&line, &errNum);
            if (err >= 0) {
                if (errNum == 11 || errNum == 12) {
                    setRadioState(channelID, RADIO_STATE_SIM_LOCKED_OR_ABSENT);
                } else if (errNum == 70 || errNum == 128 || errNum == 254) {
                    errnoType = RIL_E_FDN_CHECK_FAILURE;
                } else if (errNum == 16) {
                    errnoType = RIL_E_PASSWORD_INCORRECT;
                } else if (errNum == 3 && !strcmp(data[0], "SC") &&
                        *data[1] == '1') {
                    errnoType = RIL_E_SUCCESS;
                }
            }
        }
    }

error1:
    remainTimes = getRemainTimes(channelID, type);
    RIL_onRequestComplete(t, errnoType, &remainTimes, sizeof(remainTimes));
    at_response_free(p_response);
}

static int queryFDNServiceAvailable(int channelID) {
    int status = -1;
    int err;
    char *cmd = NULL;
    char *line = NULL;
    char pad_data = '0';
    ATResponse *p_response = NULL;
    RIL_SIM_IO_Response sr;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    memset(&sr, 0, sizeof(sr));

    if (s_appType[socket_id] == RIL_APPTYPE_USIM) {
        asprintf(&cmd, "AT+CRSM=%d,%d,%d,%d,%d,%c,\"%s\"",
                 READ_BINERY, EFID_SST, 0, 0, 1, pad_data, DF_ADF);
    } else if (s_appType[socket_id] == RIL_APPTYPE_SIM) {
        asprintf(&cmd, "AT+CRSM=%d,%d,%d,%d,%d,%c,\"%s\"",
                 READ_BINERY, EFID_SST, 0, 0, 1, pad_data, DF_GSM);
    } else {
        goto out;
    }
    err = at_send_command_singleline(s_ATChannels[channelID], cmd, "+CRSM:",
                                     &p_response);
    free(cmd);
    if (err < 0 || p_response->success == 0) {
        goto out;
    }
    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto out;

    err = at_tok_nextint(&line, &(sr.sw1));
    if (err < 0) goto out;

    err = at_tok_nextint(&line, &(sr.sw2));
    if (err < 0) goto out;

    if (at_tok_hasmore(&line)) {
        err = at_tok_nextstr(&line, &(sr.simResponse));
        if (err < 0) goto out;
    }

    if (sr.simResponse == NULL || strlen(sr.simResponse) < 2) {
        goto out;
    }

    unsigned char byteFdn[2];
    memset(byteFdn, 0, sizeof(byteFdn));
    convertHexToBin(sr.simResponse, strlen(sr.simResponse),
                    (char *)byteFdn);
    RLOGD("queryFDNServiceAvailable: byteFdn[0] = %d", byteFdn[0]);
    if (s_appType[socket_id] == RIL_APPTYPE_USIM) {
        if ((byteFdn[0] & 0x02) != 0x02) status = 2;
    } else if (s_appType[socket_id] == RIL_APPTYPE_SIM) {
        if (((byteFdn[0] >> 4) & 0x01) != 0x01) status = 2;
    }
out:
    at_response_free(p_response);
    return status;
}

static bool isISIMFileId(int fileId) {
    return (fileId == 0x6f04 || fileId == 0x6f02 || fileId == 0x6f03 ||
            fileId == 0x6f07  || fileId == 0x6f09 || fileId == 0x6fe5);
}

int readSimRecord(int channelID, RIL_SIM_IO_v6 *data, RIL_SIM_IO_Response *sr) {
    int err;
    char *cmd = NULL;
    char *line = NULL;
    char pad_data = '0';
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    RIL_SIM_IO_v6 *p_args = data;
    bool isISIMfile = isISIMFileId(p_args->fileid);

    /* FIXME handle pin2 */
    if (p_args->pin2 != NULL) {
        RLOGI("Reference-ril. requestSIM_IO pin2");
    }
    if (p_args->data == NULL) {
        if (isISIMfile) {
            if (s_simSessionId[socket_id] == -1) {
                RLOGE("s_simSessionId is -1, SIM_IO return ERROR");
                goto error;
            }
            err = asprintf(&cmd, "AT+CRLA=%d,%d,%d,%d,%d,%d,%c,\"%s\"",
                    s_simSessionId[socket_id],
                    p_args->command, p_args->fileid,
                    p_args->p1, p_args->p2, p_args->p3,pad_data,p_args->path);
        }  else {
            err = asprintf(&cmd, "AT+CRSM=%d,%d,%d,%d,%d,%c,\"%s\"",
                    p_args->command, p_args->fileid, p_args->p1, p_args->p2,
                    p_args->p3, pad_data, p_args->path);
        }
    } else {
        if (isISIMfile) {
            if (s_simSessionId[socket_id] == -1) {
                RLOGE("s_simSessionId is -1, SIM_IO return ERROR");
                goto error;
            }
            err = asprintf(&cmd, "AT+CRLA=%d,%d,%d,%d,%d,%d,\"%s\",\"%s\"",
                    s_simSessionId[socket_id],
                    p_args->command, p_args->fileid,
                    p_args->p1, p_args->p2, p_args->p3, p_args->data,p_args->path);
        } else {
            err = asprintf(&cmd, "AT+CRSM=%d,%d,%d,%d,%d,\"%s\",\"%s\"",
                    p_args->command, p_args->fileid, p_args->p1, p_args->p2,
                    p_args->p3, p_args->data, p_args->path);
        }
    }
    if (err < 0) {
        RLOGE("Failed to allocate memory");
        cmd = NULL;
        goto error;
    }


    err = at_send_command_singleline(s_ATChannels[channelID], cmd,
            isISIMfile ? "+CRLA:" : "+CRSM:",  &p_response);
    free(cmd);

    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &(sr->sw1));
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &(sr->sw2));
    if (err < 0) goto error;


    if (at_tok_hasmore(&line)) {
        char *simResponse = NULL;
        err = at_tok_nextstr(&line, &simResponse);
        if (err < 0 || simResponse == NULL) goto error;

        sr->simResponse = (char *)calloc(strlen(simResponse) + 1, sizeof(char));
        snprintf(sr->simResponse, strlen(simResponse) + 1, "%s", simResponse);
    } 
    if(p_args->fileid == 12258) {
        RLOGD("p_args->fileid = %d,p_args->command =%d,sr->sw1 = %d",
            p_args->fileid,p_args->command,sr->sw1);

    }
    if (p_args->fileid == 12258 && (p_args->command == READ_BINERY)
        && (sr->sw1 == 105) && (sr->sw2 == 134)) {
                at_response_free(p_response);
                err = at_send_command_singleline(s_ATChannels[channelID], "AT+CCID",
                       "+CCID:",  &p_response);
                if (err < 0 || p_response->success == 0) {
                    goto error;
                }
                line = p_response->p_intermediates->line;
                err = at_tok_start(&line);
                if (err < 0) goto error;
                char *simResponse12 = NULL;
                err = at_tok_nextstr(&line, &simResponse12);
                if (err < 0 || simResponse12 == NULL) goto error;
                //IccUtils.hexStringToBytes(hexString)
                sr->simResponse = (char *)calloc(strlen(simResponse12) + 1, sizeof(char));
                snprintf(sr->simResponse, strlen(simResponse12) + 1, "%s", simResponse12);
                char* simhexstr = sr->simResponse;
                unsigned char *byteUSIM = (char *)calloc(strlen(simResponse12) + 1, sizeof(char));
                int i;
                for (i=0;i<strlen(simhexstr);i+=2) {
                    char ch1 = simhexstr[i];
                    char ch2 = simhexstr[i+1];
                    byteUSIM[i] = ch2;
                    byteUSIM[i+1] = ch1;

                }
                snprintf(byteUSIM, strlen(byteUSIM) + 1, "%s", byteUSIM);
                RLOGD("--byteUSIM= %s",byteUSIM);
                snprintf(sr->simResponse, strlen(byteUSIM) + 1, "%s", byteUSIM);
                RLOGD("change the sw value");
                sr->sw1 = 144;
                sr->sw2 = 0;
                RLOGD("sr->simResponse =  %s",sr->simResponse);
                if (byteUSIM != NULL) {
                free(byteUSIM);
                byteUSIM = NULL;
            }
                
    }


    if (s_appType[socket_id] == RIL_APPTYPE_USIM &&
        (p_args->command == COMMAND_GET_RESPONSE)) {
        RLOGD("usim card, change to sim format");
        if (sr->simResponse != NULL) {
            RLOGD("sr.simResponse NOT NULL, convert to sim");
            unsigned char *byteUSIM = NULL;
            // simResponse could not be odd, ex "EF3EF0"
            int usimLen = strlen(sr->simResponse) / 2;
            byteUSIM = (unsigned char *)malloc(usimLen + sizeof(char));
            memset(byteUSIM, 0, usimLen + sizeof(char));
            convertHexToBin(sr->simResponse, strlen(sr->simResponse),
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
                memset(sr->simResponse, 0, usimLen * 2);
                strncpy(sr->simResponse, (char *)hexUSIM,
                        RESPONSE_EF_SIZE * 2);
            }
            if (byteUSIM != NULL) {
                free(byteUSIM);
                byteUSIM = NULL;
            }
            if (sr->simResponse == NULL) {
                 RLOGE("unable convert to sim, return error");
                 goto error;
             }
        }
    }
    at_response_free(p_response);
    return 0;

error:
    at_response_free(p_response);
    return -1;
}
static void requestSIM_IO(int channelID, void *data, size_t datalen,
                             RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err;
    RIL_SIM_IO_v6 *p_args;
    RIL_SIM_IO_Response *sr = NULL;
    p_args = (RIL_SIM_IO_v6 *)data;
    sr = (RIL_SIM_IO_Response *)calloc(1, sizeof(RIL_SIM_IO_Response));

    if (p_args->command == READ_BINERY && p_args->p3 > 255 ) {
        //if p3 = 780,need send 4 times CRSM
        //1:P1 = 0, P2 = 0, P3 = 255
        //2:P1 = 0, P2 = 255, P3 = 255
        //3:P1 = 1, P2 = 254, P3 = 255
        //4:P1 = 2, P2 = 253, P3 = 15 (780-765)
        int p3 = p_args->p3;
        int times, i, total = 0;
        char *simResponse = (char *)calloc(p3 * 2 + 1, sizeof(char));
        if (p3 % 255 == 0) {
            times = p3 / 255;
        } else {
            times = p3 / 255 + 1;
        }
        for (i = 0; i < times; i++) {
            memset(sr, 0, sizeof(RIL_SIM_IO_Response));

            p_args->p1 = total >> 8;
            p_args->p2 = total & 0xff;
            p_args->p3 = (p3-total) >= 255 ? 255 : p3 % 255;
            total += p_args->p3;
            RLOGD("p1 = %d, p2 = %d, p3 = %d", p_args->p1, p_args->p2, p_args->p3);
            err = readSimRecord(channelID, p_args, sr);
            if (err < 0) {
                FREEMEMORY(sr->simResponse);
                sr->simResponse = simResponse;
                goto error;
            }
            if (sr->sw1 != 0x90 && sr->sw1 != 0x91 && sr->sw1 != 0x9e
                    && sr->sw1 != 0x9f) {
                sr->simResponse = simResponse;
                goto done;
            }
            strncat(simResponse, sr->simResponse, p_args->p3 * 2);
            FREEMEMORY(sr->simResponse);
        }
        sr->simResponse = simResponse;
        RLOGD("simResponse = %s", sr->simResponse);
    } else {
        err = readSimRecord(channelID, p_args, sr);
        if (err < 0) goto error;
    }
done:
    RIL_onRequestComplete(t, RIL_E_SUCCESS, sr, sizeof(RIL_SIM_IO_Response));
    free(sr->simResponse);
    free(sr);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    free(sr->simResponse);
    free(sr);
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
                10 + (int)strlen(p_args->data), p_args->cla,
                p_args->instruction, p_args->p1, p_args->p2, p_args->p3,
                p_args->data);
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

    RIL_OpenChannelParams *params = (RIL_OpenChannelParams *)data;

    asprintf(&cmd, "AT+SPCCHO=\"%s\"", params->aidPtr);
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

static void  requestSIMOpenChannelWithP2(int channelID, void *data,
                                         size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err;
    int rspLen = 1;
    int response[260];

    char *cmd = NULL;
    char *line = NULL;
    char *statusWord = NULL;
    char *responseData = NULL;
    RIL_OpenChannelParams *params = (RIL_OpenChannelParams *)data;

    ATResponse *p_response = NULL;
    RIL_Errno errType = RIL_E_GENERIC_FAILURE;

    asprintf(&cmd, "AT+SPCCHO=\"%s\",%d", params->aidPtr, params->p2);
    err = at_send_command_singleline(s_ATChannels[channelID], cmd, "+SPCCHO:",
                                     &p_response);
    free(cmd);
    if (err < 0) goto error;
    if (p_response != NULL && p_response->success == 0) {
        if (!strcmp(p_response->finalResponse, "+CME ERROR: 20")) {
            errType = RIL_E_MISSING_RESOURCE;
        } else if (!strcmp(p_response->finalResponse, "+CME ERROR: 22")) {
            errType = RIL_E_NO_SUCH_ELEMENT;
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
            while (rspLen <= length) {
                sscanf(responseData, "%02x", &(response[rspLen]));
                rspLen++;
                responseData += 2;
            }
            sscanf(statusWord, "%02x%02x", &(response[rspLen]),
                   &(response[rspLen + 1]));
            rspLen = rspLen + 2;
        } else {
            goto error;
        }
    } else {
        // no select response, set status word
        response[rspLen] = 0x90;
        response[rspLen + 1] = 0x00;
        rspLen = rspLen + 2;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, response, rspLen * sizeof(int));
    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, errType, NULL, 0);
    at_response_free(p_response);
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

    if (session_id == 0) {
        RIL_onRequestComplete(t, RIL_E_INVALID_ARGUMENTS, NULL, 0);
    } else {
        snprintf(cmd, sizeof(cmd), "AT+CCHC=%d", session_id);
        err = at_send_command(s_ATChannels[channelID], cmd, &p_response);

        if (err < 0 || p_response->success == 0) {
            RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        } else {
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        }
        at_response_free(p_response);
    }
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

retry:
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
                p_args->sessionid, 10 + (int)strlen(p_args->data), p_args->cla,
                p_args->instruction, p_args->p1, p_args->p2, p_args->p3,
                p_args->data);
    }

    err = at_send_command_singleline(s_ATChannels[channelID], cmd, "+CGLA:",
                                     &p_response);
    free(cmd);
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

    // Handle status word 6c and 61, REF(ISO/IEC 7816-4):
    // 6cxx means xx data available for GET RESPONSE;
    // 61xx means expected data length (p3 or le) should be xx;
    // Add for OMAPI, if there is data follow with 61xx, report to Apk without retry;
    if (sr.sw1 == 0x6c) {
        p_args->p3 = sr.sw2;
        at_response_free(p_response);
        RLOGD("Received APDU sw1 6c. Retry with GET RESPONSE.");
        goto retry;
    } else if (sr.sw1 == 0x61
            && (strlen(sr.simResponse) == 0)) {
        p_args->p1 = 0x00;
        p_args->p2 = 0x00;
        p_args->p3 = sr.sw2;
        p_args->instruction = 0xc0;
        p_args->data = NULL;
        at_response_free(p_response);
        RLOGD("Received APDU sw1 61 without data. Retry with correct Le.");
        goto retry;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &sr, sizeof(sr));
    at_response_free(p_response);

    // end sim toolkit session if 90 00 on TERMINAL RESPONSE
    if ((p_args->instruction == 20) && (sr.sw1 == 0x90)) {
        RIL_onUnsolicitedResponse(RIL_UNSOL_STK_SESSION_END, NULL, 0,
                                  socket_id);
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
    free(cmd);
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

int initISIM(int channelID) {
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
    if (s_imsInitISIM[socket_id] != -1) {
        return s_imsInitISIM[socket_id];
    }
    ATResponse *p_response = NULL;
    char *line;
    int err;
    err = at_send_command_singleline(s_ATChannels[channelID], "AT+ISIM=1",
                                     "+ISIM:", &p_response);
    if (err >= 0 && p_response->success) {
        line = p_response->p_intermediates->line;
        err = at_tok_start(&line);
        if (err >= 0) {
            err = at_tok_nextint(&line, &s_imsInitISIM[socket_id]);
            RLOGD("Response of ISIM is %d", s_imsInitISIM[socket_id]);
            if (s_imsInitISIM[socket_id] == 1) {
                err = at_tok_nextint(&line, &s_simSessionId[socket_id]);
                RLOGE("SessionId of ISIM is %d", s_simSessionId[socket_id]);
            }
        }
    }
    at_response_free(p_response);
    return s_imsInitISIM[socket_id];
}

static void requestInitISIM(int channelID, void *data, size_t datalen,
                            RIL_Token t) {
    int err;
    int response = initISIM(channelID);
    char cmd[AT_COMMAND_LEN] = {0};
    const char **strings = (const char **)data;
    ATResponse *p_response = NULL;

    if (response == 1) {
        RLOGD("ISIM card, need send AT+IMSCOUNTCFG=1 to CP");
        err = at_send_command(s_ATChannels[channelID], "AT+IMSCOUNTCFG=1", NULL);
    }

    /* 7 means the number of data from fwk, represents 7 AT commands value */
    if (datalen == 7 * sizeof(char *) && strings[0] != NULL &&
        strlen(strings[0]) > 0) {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(int));
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
}

void notifySIMStatus(int channelID, void *data, RIL_Token t) {
    int onOff = ((int *)data)[0];
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);

    RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED, NULL, 0,
                              socket_id);
}

void requestSIMPower(int channelID, void *data, RIL_Token t) {
    int err = 0;
    int onOff = ((int *)data)[0];
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    if (onOff == 0) {
        err = at_send_command(s_ATChannels[channelID], "AT+SPDISABLESIM=1",
                              NULL);
        err = at_send_command(s_ATChannels[channelID], "AT+SFUN=3",
                              &p_response);
    } else if (onOff > 0) {
        err = at_send_command(s_ATChannels[channelID], "AT+SFUN=2",
                              &p_response);
    }
    if (err < 0 || p_response->success == 0) {
        goto error;
    }
    at_response_free(p_response);

    notifySIMStatus(channelID, data, t);
    return;

error:
    at_response_free(p_response);
    if (t != NULL) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
}

void requestGetICCID(int channelID, RIL_Token t) {
    int err;
    char *line = NULL;
    char *response = NULL;
    ATResponse *p_response = NULL;

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+CCID?",
                                     "+CCID: ", &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }
    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextstr(&line, &response);
    if (err < 0) goto error;

    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, response, strlen(response) + 1);
    return;

error:
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}


static void setSIMPowerOff(void *param) {
    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    if ((int)socket_id < 0 || (int)socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        return;
    }

    int channelID = getChannel(socket_id);
    pthread_mutex_lock(&s_radioPowerMutex[socket_id]);
    requestSIMPower(channelID, 0, NULL);
    pthread_mutex_unlock(&s_radioPowerMutex[socket_id]);
    putChannel(channelID);
}

static void requestSIMGetAtr(int channelID, RIL_Token t) {
    int err;
    RIL_Errno errType = RIL_E_GENERIC_FAILURE;
    char *line = NULL;
    char *response = NULL;
    ATResponse *p_response = NULL;

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+SPATR?",
                                     "+SPATR:", &p_response);
    if (err < 0) goto error;
    if (p_response != NULL && p_response->success == 0) {
        if (!strcmp(p_response->finalResponse, "+CME ERROR: 20")) {
            errType = RIL_E_MISSING_RESOURCE;
        } else {
            errType = RIL_E_GENERIC_FAILURE;
        }
        goto error;
    }

    line = p_response->p_intermediates->line;

    if (at_tok_start(&line) < 0) goto error;
    if (at_tok_nextstr(&line, &response) < 0) goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, response, strlen(response));
    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, errType, NULL, 0);
    at_response_free(p_response);
}

static void getIMEIPassword(int channelID, char imeiPwd[]) {
    ATResponse *p_response = NULL;
    char password[15];
    int i = 0;
    int j = 0;
    int err;
    char *line;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
    if (socket_id != RIL_SOCKET_1) return;

    err = at_send_command_numeric(s_ATChannels[channelID], "AT+CGSN",
            &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;

    if (strlen(line) != 15) goto error;
    while (*line != '\0') {
        if (i > 15) break;
        password[i] = *line;
        line++;
        i++;
    }
    for (i = 0, j = 0; i < 14 && j <= 6; i += 2, j++) {
        imeiPwd[j] = (password[i] - 48 + password[i + 1] - 48) % 10 + '0';
    }
    imeiPwd[7] = password[0];
    imeiPwd[8] = '\0';
    at_response_free(p_response);
    return;
error:
    RLOGE(" get IMEI failed or IMEI is not rigth");
    at_response_free(p_response);
    return;
}

static void requestFacilityLockByUser(int channelID, char **data,
                                          size_t datalen, RIL_Token t) {
    ATResponse *p_response = NULL;
    char imeiPwd[9] = {0};
    int err;
    int result;
    int status;
    char *cmd, *line;
    int errNum = -1;
    int ret = -1;

    if (datalen != 2 * sizeof(char *)) {
        goto error1;
    }

    getIMEIPassword(channelID, imeiPwd);

    ret = asprintf(&cmd, "AT+CLCK=\"%s\",%c,\"%s\"",
            data[0], *data[1], imeiPwd);

    if (ret < 0) {
        RLOGE("Failed to allocate memory");
        cmd = NULL;
        goto error1;
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
    if (p_response != NULL && strStartsWith(p_response->finalResponse,
            "+CME ERROR:")) {
        line = p_response->finalResponse;
        err = at_tok_start(&line);
        if (err >= 0) {
            err = at_tok_nextint(&line, &errNum);
            if (err >= 0) {
                if (errNum == 11 || errNum == 12) {
                    setRadioState(channelID, RADIO_STATE_SIM_LOCKED_OR_ABSENT);
                } else if (errNum == 70 || errNum == 128) {
                    RIL_onRequestComplete(t, RIL_E_FDN_CHECK_FAILURE, NULL, 0);
                    at_response_free(p_response);
                    return;
                } else if (errNum == 16) {
                    RIL_onRequestComplete(t, RIL_E_PASSWORD_INCORRECT, NULL, 0);
                    at_response_free(p_response);
                    return;
                }
            }
        }
    }
error1:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static void requestGetSimLockStatus(int channelID, void *data, size_t datalen,
                                        RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    ATResponse *p_response = NULL;
    char *cmd, *line;
    int err, skip, status;
    int ret = -1;

    int fac = ((int *)data)[0];

    ret = asprintf(&cmd, "AT+SPSMPN=%d,1", fac);
    if (ret < 0) {
        RLOGE("Failed to allocate memory");
        cmd = NULL;
        goto error;
    }

    err = at_send_command_singleline(s_ATChannels[channelID], cmd, "+SPSMPN:",
                                         &p_response);
    free(cmd);

    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &skip);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &status);
    if (err < 0) goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &status, sizeof(status));
    at_response_free(p_response);
    return;
error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
    return;
}

static void requestGetSimLockDummys(int channelID, RIL_Token t) {
    ATResponse *p_response = NULL;
    int err, i;
    char *line;
    int dummy[8];
    err = at_send_command_singleline(s_ATChannels[channelID], "AT+SPSLDUM?",
                                         "+SPSLDUM:", &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0) goto error;

    for (i = 0; i < 8; i++) {
        err = at_tok_nextint(&line, &dummy[i]);
        if (err < 0) goto error;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &dummy, 8 * sizeof(int));
    at_response_free(p_response);
    return;
error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
    return;
}

static void fillAndCatPlmn(char *plmn, char *mcc, char *mnc, int mncLen) {
    // add for test sim card:mcc=001
    int mccLen = strlen(mcc);
    if (mccLen == 1) {
        strncpy(plmn, "00", strlen("00") + 1);
    } else if (mccLen == 2) {
        strncpy(plmn, "0", strlen("0") + 1);
    }
    strncat(plmn, mcc, mccLen);

    int toFillMncLen = mncLen - strlen(mnc);
    if (toFillMncLen == 1) {
        strncat(plmn, "0", strlen("0"));
    } else if (toFillMncLen == 2) {
        strncat(plmn, "00", strlen("00"));
    }
    strncat(plmn, mnc, strlen(mnc));
}

static void catSimLockWhiteListString(char *whitelist, int whitelistLen,
                                          char **parts, int partRow) {
    int totalLen = 0;
    int i;

    for (i = 0; i < partRow; i++) {
        if (parts[i] == NULL) {
            RLOGE("catSimLockWhiteListString: parts[%d] is NULL!", i);
            break;
        }
        if (strlen(parts[i]) == 0) {
            break;
        }
        totalLen += strlen(parts[i]);
        if (whitelistLen < totalLen) {
            RLOGE("catSimLockWhiteListString overlay!");
            return;
        }
        if (i > 0) {
            strncat(whitelist, ",", strlen(","));
            totalLen++;
        }
        strncat(whitelist, parts[i], strlen(parts[i]));
    }
    RLOGD("catSimLockWhiteListString whitelist=[%s]", whitelist);
}

static void requestGetSimLockWhiteList(int channelID, void *data,
                                           size_t datalen, RIL_Token t) {
    ATResponse *p_response = NULL;
    int err, i;
    int result;
    int status;
    char *cmd, *line, *mcc, *mnc, *whiteList, *type_ret, *numlocks_ret;
    int errNum = -1;
    int ret = -1;
    int row = 0;
    int type, type_back, numlocks, mnc_digit;

    if (datalen != 1 * sizeof(int)) {
        goto error;
    }

    type = ((int *)data)[0];

    ret = asprintf(&cmd, "AT+SPSMNW=%d,\"%s\",%d", type, "12345678", 1);
    if (ret < 0) {
        RLOGE("Failed to allocate memory");
        cmd = NULL;
        goto error;
    }
    err = at_send_command_singleline(s_ATChannels[channelID], cmd, "+SPSMNW:",
                                         &p_response);
    free(cmd);

    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextstr(&line, &type_ret);
    if (err < 0) goto error;
    type_back = atoi(type_ret);

    err = at_tok_nextstr(&line, &numlocks_ret);
    if (err < 0 ) goto error;

    numlocks = atoi(numlocks_ret);
    if (numlocks < 0) goto error;

    int whiteListLen = sizeof(char) * (numlocks * WHITE_LIST_PS_PART_LENGTH
                    + WHITE_LIST_HEAD_LENGTH);
    // 3 is according to PC, 2 is head for fixing type_ret & numlocks_ret
    int partsRow = 3 * numlocks + 2;

    char **parts = NULL;
    parts = (char **)alloca(sizeof(char *) * partsRow);
    for (i = 0; i < partsRow; i++) {
        parts[i] = (char *)alloca(sizeof(char) * WHITE_LIST_COLUMN);
        memset(parts[i], 0, sizeof(char) * WHITE_LIST_COLUMN);
    }

    whiteList = (char *)alloca(whiteListLen);
    memset(whiteList, 0, whiteListLen);

    memcpy(parts[row], type_ret, strlen(type_ret));
    memcpy(parts[++row], numlocks_ret, strlen(numlocks_ret));

    switch (type_back) {
        case REQUEST_SIMLOCK_WHITE_LIST_PS: {
            char *imsi_len, *tmpImsi, *fixedImsi;
            int imsi_index;

            fixedImsi = (char *)alloca(sizeof(char) * SMALL_IMSI_LEN);

            for (i = 0; i < numlocks; i++) {
                err = at_tok_nextstr(&line, &imsi_len);
                if (err < 0) goto error;
                strncat(parts[++row], imsi_len, strlen(imsi_len));
                row++;

                for (imsi_index = 0; imsi_index < IMSI_VAL_NUM; imsi_index++) {
                    err = at_tok_nextstr(&line, &tmpImsi);
                    if (err < 0) goto error;

                    memset(fixedImsi, 0, sizeof(char) * SMALL_IMSI_LEN);

                    int len = strlen(tmpImsi);
                    if (len == 0) {
                        strncpy(fixedImsi, "00", strlen("00") + 1);
                    } else if (len == 1) {
                        strncpy(fixedImsi, "0", strlen("0") + 1);
                    }
                    strncat(fixedImsi, tmpImsi, len);
                    strncat(parts[row], fixedImsi, strlen(fixedImsi));
                }
            }
            break;
        }

        case REQUEST_SIMLOCK_WHITE_LIST_PN: {
            for (i = 0; i < numlocks; i++) {
                err = at_tok_nextstr(&line, &mcc);
                if (err < 0) goto error;
                err = at_tok_nextstr(&line, &mnc);
                if (err < 0) goto error;
                err = at_tok_nextint(&line, &mnc_digit);
                if (err < 0) goto error;
                fillAndCatPlmn(parts[++row], mcc, mnc, mnc_digit);
            }
            break;
        }

        case REQUEST_SIMLOCK_WHITE_LIST_PU: {
            char *network_subset1, *network_subset2;

            for (i = 0; i < numlocks; i++) {
                err = at_tok_nextstr(&line, &mcc);
                if (err < 0) goto error;
                err = at_tok_nextstr(&line, &mnc);
                if (err < 0) goto error;
                err = at_tok_nextint(&line, &mnc_digit);
                if (err < 0) goto error;
                fillAndCatPlmn(parts[++row], mcc, mnc, mnc_digit);

                err = at_tok_nextstr(&line, &network_subset1);
                if (err < 0) goto error;
                strncat(parts[row], network_subset1, strlen(network_subset1));

                err = at_tok_nextstr(&line, &network_subset2);
                if (err < 0) goto error;
                strncat(parts[row], network_subset2, strlen(network_subset2));
            }
            break;
        }

        case REQUEST_SIMLOCK_WHITE_LIST_PP: {
            char *gid1;

            for (i = 0; i < numlocks; i++) {
                err = at_tok_nextstr(&line, &mcc);
                if (err < 0) goto error;
                err = at_tok_nextstr(&line, &mnc);
                if (err < 0) goto error;
                err = at_tok_nextint(&line, &mnc_digit);
                if (err < 0) goto error;
                fillAndCatPlmn(parts[++row], mcc, mnc, mnc_digit);

                err = at_tok_nextstr(&line, &gid1);
                if (err < 0) goto error;
                strncat(parts[++row], gid1, strlen(gid1));
            }
            break;
        }

        case REQUEST_SIMLOCK_WHITE_LIST_PC: {
            char *gid1, *gid2;

            for (i = 0; i < numlocks; i++) {
                err = at_tok_nextstr(&line, &mcc);
                if (err < 0) goto error;
                err = at_tok_nextstr(&line, &mnc);
                if (err < 0) goto error;
                err = at_tok_nextint(&line, &mnc_digit);
                if (err < 0) goto error;
                fillAndCatPlmn(parts[++row], mcc, mnc, mnc_digit);

                err = at_tok_nextstr(&line, &gid1);
                if (err < 0) goto error;
                strncat(parts[++row], gid1, strlen(gid1));

                err = at_tok_nextstr(&line, &gid2);
                if (err < 0) goto error;
                strncat(parts[++row], gid2, strlen(gid2));
            }
            break;
        }

        default:
            goto error;
            break;
    }
    catSimLockWhiteListString(whiteList, whiteListLen, parts, partsRow);

    RIL_onRequestComplete(t, RIL_E_SUCCESS, whiteList, strlen(whiteList) + 1);
    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
    return;
}

int processSimRequests(int request, void *data, size_t datalen, RIL_Token t,
                          int channelID) {
    int err;
    ATResponse *p_response = NULL;

    switch (request) {
        case RIL_REQUEST_GET_SIM_STATUS:
        case RIL_EXT_REQUEST_GET_SIM_STATUS:
        case RIL_EXT_REQUEST_SIMMGR_GET_SIM_STATUS: {
            RIL_CardStatus_v6 *p_card_status;
            char *p_buffer;
            int buffer_size;
            RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
            sem_wait(&(s_sem[socket_id]));
            int result = getCardStatus(request, channelID, &p_card_status);
            sem_post(&(s_sem[socket_id]));

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
            requestEnterSimPin(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_ENTER_SIM_PIN2 :
            requestEnterSimPin2(channelID, data, datalen, t);
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
        case RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION : {
            RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
            if (s_isSimPresent[socket_id] == SIM_ABSENT) {
                RIL_onRequestComplete(t, RIL_E_INVALID_SIM_STATE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
            }
            break;
        }
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
        case RIL_REQUEST_QUERY_FACILITY_LOCK:
        case RIL_REQUEST_EXT_QUERY_FACILITY_LOCK: {
            char *lockData[4];
            lockData[0] = ((char **)data)[0];
            lockData[1] = FACILITY_LOCK_REQUEST;
            lockData[2] = ((char **)data)[1];
            lockData[3] = ((char **)data)[2];
            requestFacilityLock(request, channelID, lockData, datalen + sizeof(char *),
                                t);
            break;
        }
        case RIL_REQUEST_SET_FACILITY_LOCK:
            requestFacilityLock(request, channelID, data, datalen, t);
            break;
        case RIL_REQUEST_SIM_TRANSMIT_APDU_BASIC:
            requestTransmitApduBasic(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_SIM_OPEN_CHANNEL: {
            RIL_OpenChannelParams *params = (RIL_OpenChannelParams *)data;
            if (params->p2 < 0) {
                requestOpenLogicalChannel(channelID, data, datalen, t);
            } else {
                requestSIMOpenChannelWithP2(channelID, data, datalen, t);
            }
            break;
        }
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
            if (sim_auth->authContext == AUTH_CONTEXT_EAP_AKA &&
                sim_auth->authData != NULL) {
                requestUSimAuthentication(channelID, sim_auth->authData, t);
            } else if (sim_auth->authContext == AUTH_CONTEXT_EAP_SIM &&
                       sim_auth->authData != NULL) {
                requestSimAuthentication(channelID, sim_auth->authData, t);
            } else {
                RLOGE("invalid authContext");
                RIL_onRequestComplete(t, RIL_E_INVALID_ARGUMENTS, NULL, 0);
            }
            break;
        }
        case RIL_EXT_REQUEST_GET_SIMLOCK_REMAIN_TIMES: {
            int fac = ((int *)data)[0];
            int result = getNetLockRemainTimes(channelID, fac);
            RIL_onRequestComplete(t, RIL_E_SUCCESS, &result, sizeof(int));
            break;
        }
        case RIL_EXT_REQUEST_SET_FACILITY_LOCK_FOR_USER:
            requestFacilityLockByUser(channelID, data, datalen, t);
            break;
        case RIL_EXT_REQUEST_GET_SIMLOCK_STATUS:
            requestGetSimLockStatus(channelID, data, datalen, t);
            break;
        case RIL_EXT_REQUEST_GET_SIMLOCK_DUMMYS:
            requestGetSimLockDummys(channelID, t);
            break;
        case RIL_EXT_REQUEST_GET_SIMLOCK_WHITE_LIST:
            requestGetSimLockWhiteList(channelID, data, datalen, t);
            break;
        /* IMS request @{ */
        case RIL_REQUEST_INIT_ISIM:
            requestInitISIM(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_ENABLE_IMS: {
            err = at_send_command(s_ATChannels[channelID], "AT+IMSEN=1", NULL);
            if (err < 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            break;
        }
        case RIL_REQUEST_DISABLE_IMS: {
            err = at_send_command(s_ATChannels[channelID], "AT+IMSEN=0", NULL);
            if (err < 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            break;
        }
        case RIL_EXT_REQUEST_SIMMGR_SIM_POWER:
            notifySIMStatus(channelID, data, t);
            break;
        /* }@ */
        case RIL_EXT_REQUEST_SIM_GET_ATR:
            requestSIMGetAtr(channelID, t);
            break;
        case RIL_EXT_REQUEST_SIM_POWER_REAL:
            requestSIMPower(channelID, data, NULL);
            break;
        case RIL_REQUEST_SET_SIM_CARD_POWER:
            requestSIMPower(channelID, data, t);
            break;
        case RIL_EXT_REQUEST_GET_SUBSIDYLOCK_STATUS: {
            p_response = NULL;
            char *line;
            int result;
            err = at_send_command_singleline(s_ATChannels[channelID],
                    "AT+SPSLENABLED?", "+SPSLENABLED:", &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                line = p_response->p_intermediates->line;
                err = at_tok_start(&line);
                if (err == 0) {
                    err = at_tok_nextint(&line, &result);
                }
                RIL_onRequestComplete(t, RIL_E_SUCCESS, &result, sizeof(result));
            }
            at_response_free(p_response);
            break;
        }
        case RIL_EXT_REQUEST_GET_ICCID:
            requestGetICCID(channelID, t);
            break;
        default:
            return 0;
    }

    return 1;
}

static void onSimAbsent(void *param) {
    int channelID;
    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    if ((int)socket_id < 0 || (int)socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        return;
    }

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
    if ((int)socket_id < 0 || (int)socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        return;
    }

    channelID = getChannel(socket_id);
    if (isRadioOn(channelID) > 0) {
        setRadioState(channelID, RADIO_STATE_SIM_NOT_READY);
    }
    putChannel(channelID);

    RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED, NULL,
                              0, socket_id);
}

static void onSimlockLocked(void *param) {
    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);

    RIL_onUnsolicitedResponse(RIL_EXT_UNSOL_SIMLOCK_STATUS_CHANGED,
                               NULL, 0, socket_id);
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
                        RIL_onUnsolicitedResponse(
                                RIL_EXT_UNSOL_SIMMGR_SIM_STATUS_CHANGED, NULL,
                                0, socket_id);
                        RIL_requestTimedCallback(onSimAbsent,
                                (void *)&s_socketId[socket_id], NULL);
                        // sim hot plug out and set stk to not enable
                        s_stkServiceRunning[socket_id] = false;
                    } else if (cause == 34 || cause == 25) {  // sim removed or turn off
                        RIL_onUnsolicitedResponse(
                                RIL_EXT_UNSOL_SIMMGR_SIM_STATUS_CHANGED, NULL,
                                0, socket_id);
                        if (cause == 34) {
                            RIL_requestTimedCallback(onSimAbsent,
                                    (void *)&s_socketId[socket_id], NULL);
                            // sim hot plug out and set stk to not enable
                            s_stkServiceRunning[socket_id] = false;
                        }else if (cause == 25) {
                            RIL_onUnsolicitedResponse(
                                      RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED,
                                      NULL, 0, socket_id);
                        }
                    } else if (cause == 1 || cause == 7) {  // no sim card
                        RIL_onUnsolicitedResponse(
                                RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED, NULL, 0,
                                socket_id);
                        RIL_onUnsolicitedResponse(
                                RIL_EXT_UNSOL_SIMMGR_SIM_STATUS_CHANGED, NULL,
                                0, socket_id);
                        // sim hot plug out and set stk to not enable
                        s_stkServiceRunning[socket_id] = false;
                    }
                }
            } else if (value == 100 || value == 4) {
                RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED,
                                          NULL, 0, socket_id);
                if (value == 4) {
                    RIL_requestTimedCallback(onSimlockLocked,
                            (void *)&s_socketId[socket_id], &TIMEVAL_CALLSTATEPOLL);
                } else if (value == 100) {
                    RIL_onUnsolicitedResponse(
                            RIL_EXT_UNSOL_SIMMGR_SIM_STATUS_CHANGED, NULL, 0,
                            socket_id);
                }
            } else if (value == 0 || value == 2) {
                if (at_tok_hasmore(&tmp)) {
                    err = at_tok_nextint(&tmp, &cause);
                    if (err < 0) goto out;
                    if (cause == 0) {
                        s_imsInitISIM[socket_id] = -1;
                    }
                }
                RIL_requestTimedCallback(onSimPresent,
                                         (void *)&s_socketId[socket_id], NULL);
            }
        }
    }

out:
    free(line);
}

int processSimUnsolicited(RIL_SOCKET_ID socket_id, const char *s) {
    int err;
    char *line = NULL;

    if (strStartsWith(s, "+ECIND:")) {
        onSimStatusChanged(socket_id, s);
    } else if (strStartsWith(s, "+SPEXPIRESIM:")) {
        int simID;
        char *tmp = NULL;

        line = strdup(s);
        tmp = line;
        err = at_tok_start(&tmp);
        if (err < 0) goto out;

        err = at_tok_nextint(&tmp, &simID);
        if (err < 0) goto out;

        RIL_onUnsolicitedResponse(RIL_EXT_UNSOL_SIMLOCK_SIM_EXPIRED, &simID,
                sizeof(simID), socket_id);
    } else if (strStartsWith(s, "+CLCK:")) {
        int response;
        char *tmp = NULL;
        char *type = NULL;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextstr(&tmp, &type);
        if (err < 0) goto out;

        if (0 == strcmp(type, "FD")) {
            err = at_tok_nextint(&tmp, &response);
            if (err < 0) goto out;

            const char *cmd = "+CLCK:";
            checkAndCompleteRequest(socket_id, cmd, (void *)(&response));
        }
    } else if (strStartsWith(s, "+SPSLENABLED:")) {
        int status = 0;
        char *tmp = NULL;

        line = strdup(s);
        tmp = line;
        err = at_tok_start(&tmp);
        if (err < 0)
            goto out;

        err = at_tok_nextint(&tmp, &status);
        if (err < 0) goto out;

        RIL_onUnsolicitedResponse(RIL_EXT_UNSOL_SUBSIDYLOCK_STATUS_CHANGED,
                &status, sizeof(status), socket_id);
    } else {
        return 0;
    }

out:
    free(line);
    return 1;
}

/* AT Command [AT+CLCK="FD",....] used to enable/disable FDN facility.
 * But the AT response is async
 * the status of "+CLCK:"FD",status" URC is the real result
 * dispatchCLCK according the URC to complete the request
 */
void dispatchCLCK(RIL_Token t, void *data, void *resp) {
    int mode = ((int *)data)[0];
    int status = ((int *)resp)[0];

    if (mode == status) {
        int result = 1;
        RIL_onRequestComplete(t, RIL_E_SUCCESS, &result, sizeof(result));
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
}
