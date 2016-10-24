/**
 * ril_misc.c --- Any other requests besides data/sim/call...
 *                process functions implementation
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */
#define LOG_TAG "RIL"

#include "sprd_ril.h"
#include "ril_misc.h"
#include "ril_data.h"
#include "ril_network.h"

/* Fast Dormancy disable property */
#define RADIO_FD_DISABLE_PROP "persist.radio.fd.disable"

/* for sleep log */
#define BUFFER_SIZE     (12 * 1024 * 4)
#define CONSTANT_DIVIDE 32768.0

/* single channel call, no need to distinguish sim1 and sim2*/
int s_maybeAddCall = 0;
int s_screenState = 1;
static pthread_mutex_t s_screenMutex = PTHREAD_MUTEX_INITIALIZER;

static void requestBasebandVersion(int channelID, void *data, size_t datalen,
                                   RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int i, err;
    char response[ARRAY_SIZE];
    char *line = NULL;
    ATLine *p_cur = NULL;
    ATResponse *p_response = NULL;

    err = at_send_command_multiline(s_ATChannels[channelID], "AT+CGMR", "",
                                    &p_response);
    memset(response, 0, sizeof(response));
    if (err != 0 || p_response->success == 0) {
        RLOGE("requestBasebandVersion:Send command error!");
        goto error;
    }
    for (i = 0, p_cur = p_response->p_intermediates; p_cur != NULL;
         p_cur = p_cur->p_next, i++) {
        line = p_cur->line;
        if (i < 2) continue;
        if (i < 4) {
            if (at_tok_start(&line) == 0) {
                skipWhiteSpace(&line);
                strncat(response, line, strlen(line));
                strncat(response, "|", strlen("|"));
            } else {
                continue;
            }
        } else {
            skipWhiteSpace(&line);
            strncat(response, line, strlen(line));
        }
    }
    if (strlen(response) == 0) {
        RLOGE("requestBasebandVersion: Parameter parse error!");
        goto error;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, response, strlen(response) + 1);
    at_response_free(p_response);
    return;

error:
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

static void requestGetIMEISV(int channelID, void *data, size_t datalen,
                             RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int err = 0;
    int sv;
    char *line;
    char response[ARRAY_SIZE];  // 10 --> ARRAY_SIZE  debug
    ATResponse *p_response = NULL;

    memset(response, 0, sizeof(response));

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+SGMR=0,0,2",
                                     "+SGMR:", &p_response);

    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &sv);
    if (err < 0) goto error;

    if (sv >= 0 && sv < 10) {
        snprintf(response, sizeof(response), "0%d", sv);
    } else {
        snprintf(response, sizeof(response), "%d", sv);
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, response, strlen(response) + 1);
    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static void onQuerySignalStrengthLTE(void *param) {
    int err;
    int skip;
    int response[6] = {-1, -1, -1, -1, -1, -1};
    char *line;
    ATResponse *p_response = NULL;
    RIL_SignalStrength_v6 response_v6;

    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    int channelID = getChannel(socket_id);

    RLOGE("query signal strength LTE when screen on");
    RIL_SIGNALSTRENGTH_INIT(response_v6);

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+CESQ",
                                     "+CESQ:", &p_response);

    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &response[0]);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &response[1]);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &response[2]);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &skip);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &skip);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &response[5]);
    if (err < 0) goto error;

    if (response[0] != -1 && response[0] != 99) {
        response_v6.GW_SignalStrength.signalStrength = response[0];
    }
    if (response[2] != -1 && response[2] != 255) {
        response_v6.GW_SignalStrength.signalStrength = response[2];
    }
    if (response[5] != -1 && response[5] != 255 && response[5] != -255) {
        response_v6.LTE_SignalStrength.rsrp = response[5];
    }

    RIL_onUnsolicitedResponse(RIL_UNSOL_SIGNAL_STRENGTH, &response_v6,
                              sizeof(RIL_SignalStrength_v6), socket_id);
    putChannel(channelID);
    at_response_free(p_response);
    return;

error:
    RLOGE("onQuerySignalStrengthLTE fail");
    putChannel(channelID);
    at_response_free(p_response);
}

static void onQuerySignalStrength(void *param) {
    int err;
    char *line;
    RIL_SignalStrength_v6 response_v6;
    ATResponse *p_response = NULL;

    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    int channelID = getChannel(socket_id);

    RLOGE("query signal strength when screen on");
    RIL_SIGNALSTRENGTH_INIT(response_v6);

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+CSQ", "+CSQ:",
                                     &p_response);

    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err =
        at_tok_nextint(&line, &(response_v6.GW_SignalStrength.signalStrength));
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &(response_v6.GW_SignalStrength.bitErrorRate));
    if (err < 0) goto error;

    RIL_onUnsolicitedResponse(RIL_UNSOL_SIGNAL_STRENGTH, &response_v6,
                              sizeof(RIL_SignalStrength_v6), socket_id);
    putChannel(channelID);
    at_response_free(p_response);
    return;

error:
    RLOGE("onQuerySignalStrength fail");
    putChannel(channelID);
    at_response_free(p_response);
}

static void requestScreeState(int channelID, int status, RIL_Token t) {
    int err;
    int stat;
    char prop[PROPERTY_VALUE_MAX] = {0};

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    pthread_mutex_lock(&s_screenMutex);
    property_get(RADIO_FD_DISABLE_PROP, prop, "0");
    RLOGD("RADIO_FD_DISABLE_PROP = %s", prop);
    s_screenState = status;

    if (!status) {
        /* Suspend */
        at_send_command(s_ATChannels[channelID], "AT+CCED=2,8", NULL);
        if (s_isLTE) {
            at_send_command(s_ATChannels[channelID], "AT+CEREG=1", NULL);
        }
        at_send_command(s_ATChannels[channelID], "AT+CREG=1", NULL);
        at_send_command(s_ATChannels[channelID], "AT+CGREG=1", NULL);

        if (isVoLteEnable()) {
            at_send_command(s_ATChannels[channelID], "AT+CIREG=0", NULL);
        }
        if (isExistActivePdp() && !strcmp(prop, "0")) {
            at_send_command(s_ATChannels[channelID], "AT*FDY=1,2", NULL);
        }
    } else {
        /* Resume */
        at_send_command(s_ATChannels[channelID], "AT+CCED=1,8", NULL);
        if (s_isLTE) {
            at_send_command(s_ATChannels[channelID], "AT+CEREG=2", NULL);
        }
        at_send_command(s_ATChannels[channelID], "AT+CREG=2", NULL);
        at_send_command(s_ATChannels[channelID], "AT+CGREG=2", NULL);

        if (isVoLteEnable()) {
            at_send_command(s_ATChannels[channelID], "AT+CIREG=2", NULL);
            /**
             * add for bug 534775
             * due to the unsol response is reported
             * with a int value which means the IMS registration state,
             * and the FWK does not use the response,
             * so report the 0 response
             */
            int response = 0;
            RIL_onUnsolicitedResponse(
                RIL_UNSOL_RESPONSE_IMS_NETWORK_STATE_CHANGED, &response,
                sizeof(response), socket_id);
        }
        if (isExistActivePdp() && !strcmp(prop, "0")) {
            at_send_command(s_ATChannels[channelID], "AT*FDY=1,5", NULL);
        }

        if (s_radioState[socket_id] == RADIO_STATE_SIM_READY) {
            if (s_isLTE) {
                RIL_requestTimedCallback(onQuerySignalStrengthLTE,
                                         (void *)&s_socketId[socket_id], NULL);
            } else {
                RIL_requestTimedCallback(onQuerySignalStrength,
                                         (void *)&s_socketId[socket_id], NULL);
            }
        }
        RIL_onUnsolicitedResponse(
            RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED, NULL, 0, socket_id);
    }
    pthread_mutex_unlock(&s_screenMutex);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
}

static void requestGetHardwareConfig(void *data, size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    RIL_HardwareConfig hwCfg;

    hwCfg.type = -1;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &hwCfg, sizeof(hwCfg));
}

void requestSendAT(int channelID, const char *data, size_t datalen,
                   RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int i, err;
    char *ATcmd = (char *)data;
    char buf[ARRAY_SIZE * 8] = {0};
    char *cmd;
    char *pdu;
    char *response[1] = {NULL};
    ATLine *p_cur = NULL;
    ATResponse *p_response = NULL;

    if (ATcmd == NULL) {
        RLOGE("Invalid AT command");
        goto error;
    }

    // AT+SNVM=1,2118,01
    if (!strncasecmp(ATcmd, "AT+SNVM=1", strlen("AT+SNVM=1"))) {
        cmd = ATcmd;
        skipNextComma(&ATcmd);
        pdu = strchr(ATcmd, ',');
        if (pdu == NULL) {
            RLOGE("SNVM: cmd is %s pdu is NULL !", cmd);
            strlcat(buf, "\r\n", sizeof(buf));
            response[0] = buf;
            RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, response,
                                  sizeof(char *));
            return;
        }

        *pdu = '\0';
        pdu++;
        RLOGD("SNVM: cmd %s, pdu %s", cmd, pdu);
        err = at_send_command_snvm(s_ATChannels[channelID], cmd, pdu, "",
                                   &p_response);
    } else {
        err = at_send_command_multiline(s_ATChannels[channelID], ATcmd, "",
                                        &p_response);
    }

    if (err < 0 || p_response->success == 0) {
        if (p_response != NULL) {
            strlcat(buf, p_response->finalResponse, sizeof(buf));
            strlcat(buf, "\r\n", sizeof(buf));
            response[0] = buf;
            RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, response,
                                  sizeof(char *));
        } else {
            goto error;
        }
    } else {
        p_cur = p_response->p_intermediates;
        for (i = 0; p_cur != NULL; p_cur = p_cur->p_next, i++) {
            strlcat(buf, p_cur->line, sizeof(buf));
            strlcat(buf, "\r\n", sizeof(buf));
        }
        strlcat(buf, p_response->finalResponse, sizeof(buf));
        strlcat(buf, "\r\n", sizeof(buf));
        response[0] = buf;
        RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(char *));
        if (!strncasecmp(ATcmd, "AT+SFUN=5", strlen("AT+SFUN=5"))) {
            setRadioState(channelID, RADIO_STATE_OFF);
        }
    }
    at_response_free(p_response);
    return;

error:
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

int processMiscRequests(int request, void *data, size_t datalen, RIL_Token t,
                        int channelID) {
    int err;
    ATResponse *p_response = NULL;

    switch (request) {
        case RIL_REQUEST_BASEBAND_VERSION:
            requestBasebandVersion(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_GET_IMEI: {
            err = at_send_command_numeric(s_ATChannels[channelID], "AT+CGSN",
                                          &p_response);

            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(
                    t, RIL_E_SUCCESS, p_response->p_intermediates->line,
                    strlen(p_response->p_intermediates->line) + 1);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_REQUEST_GET_IMEISV:
            requestGetIMEISV(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_SET_MUTE: {
            char cmd[AT_COMMAND_LEN] = {0};
            snprintf(cmd, sizeof(cmd), "AT+CMUT=%d", ((int *)data)[0]);
            if (s_maybeAddCall == 1 && ((int *)data)[0] == 0) {
                RLOGD("Don't cancel mute when dialing the second call");
                s_maybeAddCall = 0;
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
                break;
            }
            at_send_command(s_ATChannels[channelID], cmd, NULL);
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;
        }
        case RIL_REQUEST_GET_MUTE: {
            p_response = NULL;
            int response = 0;
            err = at_send_command_singleline(
                s_ATChannels[channelID], "AT+CMUT?", "+CMUT: ", &p_response);
            if (err >= 0 && p_response->success) {
                char *line = p_response->p_intermediates->line;
                err = at_tok_start(&line);
                if (err >= 0) {
                    err = at_tok_nextint(&line, &response);
                    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response,
                                          sizeof(response));
                }
            } else {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            }

            at_response_free(p_response);
            break;
        }
        case RIL_REQUEST_SCREEN_STATE:
            requestScreeState(channelID, ((int *)data)[0], t);
            break;
        case RIL_REQUEST_GET_HARDWARE_CONFIG:
            requestGetHardwareConfig(data, datalen, t);
            break;
        case RIL_REQUEST_OEM_HOOK_STRINGS: {
            int i;
            const char **cur;

            for (i = (datalen / sizeof(char *)), cur = (const char **)data;
                 i > 0; cur++, i--) {
                RLOGD("> '%s'", *cur);
                break;
            }
            RLOGD(">>> '%s'", *cur);
            requestSendAT(channelID, *cur, datalen, t);
            break;
        }
        default:
            return 0;
    }

    return 1;
}

int processMiscUnsolicited(RIL_SOCKET_ID socket_id, const char *s) {
    int err;
    char *line = NULL;

    if (strStartsWith(s, "+CTZV:")) {
        /* NITZ time */
        char *response;
        char *tmp;
        char *raw_str;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextstr(&tmp, &response);
        if (err != 0) {
            RLOGE("invalid NITZ line %s\n", s);
        } else {
            RIL_onUnsolicitedResponse(RIL_UNSOL_NITZ_TIME_RECEIVED, response,
                                      strlen(response) + 1, socket_id);
        }
    } else {
        return 0;
    }

out:
    free(line);
    return 1;
}