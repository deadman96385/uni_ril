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
#include "ril_sim.h"
#include "channel_controller.h"

#include "ca_rate.h"

/* Fast Dormancy disable property */
#define RADIO_FD_DISABLE_PROP "persist.vendor.radio.fd.disable"
/* PROP_FAST_DORMANCY value is "a,b". a is screen_off value, b is on value */
#define PROP_FAST_DORMANCY    "persist.vendor.radio.fastdormancy"
#define SOCKET_NAME_VSIM "vsim_socket"
/* for sleep log */
#define BUFFER_SIZE     (12 * 1024 * 4)
#define CONSTANT_DIVIDE 32768.0

/* single channel call, no need to distinguish sim1 and sim2*/
int s_maybeAddCall = 0;
int s_screenState = 1;
int s_vsimClientFd = -1;
int s_vsimServerFd = -1;
bool s_vsimListenLoop = false;
bool s_vsimInitFlag[SIM_COUNT] = {false, false};
pthread_mutex_t s_vsimSocketMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t s_vsimSocketCond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t s_screenMutex = PTHREAD_MUTEX_INITIALIZER;
struct timeval s_timevalCloseVsim = {60, 0};

typedef struct {
    int socket_id1;
    int socket_id2;
} VirtualCardPara;

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

static void requestDeviceIdentify(int channelID, void *data, size_t datalen,
                                  RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int err = 0;
    int sv;
    char *line;
    char *response[4] = {NULL, NULL, NULL, NULL};
    char buf[4][ARRAY_SIZE];
    ATResponse *p_response = NULL;

    memset(buf, 0, 4 * ARRAY_SIZE);

    // get IMEI
    err = at_send_command_numeric(s_ATChannels[channelID], "AT+CGSN",
                                  &p_response);

    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    snprintf(buf[0], ARRAY_SIZE, "%s", p_response->p_intermediates->line);
    response[0] = buf[0];

    AT_RESPONSE_FREE(p_response);

    // get IMEISV
    err = at_send_command_singleline(s_ATChannels[channelID], "AT+SGMR=0,0,2",
                                     "+SGMR:", &p_response);
    if (err < 0 || p_response->success == 0) {
        goto done;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto done;

    err = at_tok_nextint(&line, &sv);
    if (err < 0) goto done;

    if (sv >= 0 && sv < 10) {
        snprintf(buf[1], ARRAY_SIZE, "0%d", sv);
    } else {
        snprintf(buf[1], ARRAY_SIZE, "%d", sv);
    }
    response[1] = buf[1];
done:
    RIL_onRequestComplete(t, RIL_E_SUCCESS, response, 4 * sizeof(char *));
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
    ATResponse *p_newResponse = NULL;
    RIL_SignalStrength_v10 response_v10;

    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    if ((int)socket_id < 0 || (int)socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        return;
    }

    int channelID = getChannel(socket_id);

    RLOGE("query signal strength LTE when screen on");
    RIL_SIGNALSTRENGTH_INIT(response_v10);

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+CESQ",
                                     "+CESQ:", &p_response);

    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    cesq_execute_cmd_rsp(p_response, &p_newResponse);
    if (p_newResponse == NULL) goto error;

    line = p_newResponse->p_intermediates->line;

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
        response_v10.GW_SignalStrength.signalStrength = response[0];
        rxlev[socket_id] = response[0];
    }
    if (response[2] != -1 && response[2] != 255) {
        response_v10.GW_SignalStrength.signalStrength = response[2];
        rscp[socket_id] = response[2];
    }
    if (response[5] != -1 && response[5] != 255 && response[5] != -255) {
        response_v10.LTE_SignalStrength.rsrp = response[5];
        rsrp[socket_id] = response[5];
    }
    pthread_mutex_lock(&s_signalProcessMutex);
    pthread_cond_signal(&s_signalProcessCond);
    pthread_mutex_unlock(&s_signalProcessMutex);

    RIL_onUnsolicitedResponse(RIL_UNSOL_SIGNAL_STRENGTH, &response_v10,
                              sizeof(RIL_SignalStrength_v10), socket_id);
    putChannel(channelID);
    at_response_free(p_response);
    at_response_free(p_newResponse);
    return;

error:
    RLOGE("onQuerySignalStrengthLTE fail");
    putChannel(channelID);
    at_response_free(p_response);
    at_response_free(p_newResponse);
}

static void onQuerySignalStrength(void *param) {
    int err;
    char *line;
    RIL_SignalStrength_v10 response_v10;
    ATResponse *p_response = NULL;
    ATResponse *p_newResponse = NULL;

    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    if ((int)socket_id < 0 || (int)socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        return;
    }

    int channelID = getChannel(socket_id);

    RLOGE("query signal strength when screen on");
    RIL_SIGNALSTRENGTH_INIT(response_v10);

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+CSQ", "+CSQ:",
                                     &p_response);

    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    csq_execute_cmd_rsp(p_response, &p_newResponse);
    if (p_newResponse == NULL) goto error;

    line = p_newResponse->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err =
        at_tok_nextint(&line, &(response_v10.GW_SignalStrength.signalStrength));
    if (err < 0) goto error;
    rssi[socket_id] = response_v10.GW_SignalStrength.signalStrength;
    pthread_mutex_lock(&s_signalProcessMutex);
    pthread_cond_signal(&s_signalProcessCond);
    pthread_mutex_unlock(&s_signalProcessMutex);

    err = at_tok_nextint(&line, &(response_v10.GW_SignalStrength.bitErrorRate));
    if (err < 0) goto error;

    RIL_onUnsolicitedResponse(RIL_UNSOL_SIGNAL_STRENGTH, &response_v10,
                              sizeof(RIL_SignalStrength_v10), socket_id);
    putChannel(channelID);
    at_response_free(p_response);
    at_response_free(p_newResponse);
    return;

error:
    RLOGE("onQuerySignalStrength fail");
    putChannel(channelID);
    at_response_free(p_response);
    at_response_free(p_newResponse);
}

int getFastDormancyTime(int screeState) {
    int screenOffValue = 2, screenOnValue = 5, fastDormancyTime = 0, err;
    char fastDormancyPropValue[PROPERTY_VALUE_MAX] = {0};
    char overseaProp[PROPERTY_VALUE_MAX] = {0};
    char *p_fastDormancy = NULL;

    property_get(PROP_FAST_DORMANCY, fastDormancyPropValue, "");
    property_get(OVERSEA_VERSION, overseaProp, "unknown");
    if (!strcmp(overseaProp, "orange")) {
        screenOnValue = 8;
        screenOffValue = 5;
        goto done;
    }
    if (strcmp(fastDormancyPropValue, "")) {
        p_fastDormancy = fastDormancyPropValue;
        err = at_tok_nextint(&p_fastDormancy, &screenOffValue);
        if (err < 0) {
            goto done;
        }
        err = at_tok_nextint(&p_fastDormancy, &screenOnValue);
        if (err < 0) {
            goto done;
        }
    }
done:
    screeState ? (fastDormancyTime = screenOnValue) :
                 (fastDormancyTime = screenOffValue);
    return fastDormancyTime;
}

int setupVPIfNeeded() {
    char bt_pan_active[PROPERTY_VALUE_MAX] = {0};
    char agps_active[PROPERTY_VALUE_MAX] = {0};
    char internet_tethering[PROPERTY_VALUE_MAX] = {0};
    char wifi_active[PROPERTY_VALUE_MAX] = {0};
    char mms_active[PROPERTY_VALUE_MAX] = {0};
    int setupVPIfNeeded = 0;

    property_get("vendor.bluetooth.ril.bt-pan.active", bt_pan_active, "1");
    property_get("vendor.sys.ril.agps.active", agps_active, "1");
    property_get("vendor.sys.ril.internet_tethering",
                  internet_tethering, "1");
    property_get("vendor.sys.ril.wifi.active", wifi_active, "1");
    property_get("vendor.ril.mms.active", mms_active, "1");

    if (strcmp(bt_pan_active, "1") && strcmp(agps_active, "1") &&
        strcmp(internet_tethering, "1") && strcmp(wifi_active, "1") &&
        strcmp(mms_active, "1") ) {
        setupVPIfNeeded = 1;
    }
    RLOGD("%s,%s,%s,%s,%s, setupVPIfNeeded=%d", bt_pan_active, agps_active,
          internet_tethering, wifi_active, mms_active, setupVPIfNeeded);

    return setupVPIfNeeded;
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
            at_send_command(s_ATChannels[channelID], "AT+SPEDDAENABLE=1", NULL);
        }
        at_send_command(s_ATChannels[channelID], "AT+CREG=1", NULL);
        at_send_command(s_ATChannels[channelID], "AT+CGREG=1", NULL);

        if (isVoLteEnable()) {
            at_send_command(s_ATChannels[channelID], "AT+CIREG=1", NULL);
        }
        if (isExistActivePdp(socket_id) && !strcmp(prop, "0")) {
            char cmd[ARRAY_SIZE] = {0};
            snprintf(cmd, sizeof(cmd), "AT*FDY=1,%d",
                     getFastDormancyTime(status));
            at_send_command(s_ATChannels[channelID], cmd, NULL);
        }
        if (setupVPIfNeeded()) {
            at_send_command(s_ATChannels[channelID], "AT+SPVOICEPREFER=1", NULL);
        }
    } else {
        /* Resume */
        at_send_command(s_ATChannels[channelID], "AT+CCED=1,8", NULL);
        if (s_isLTE) {
            at_send_command(s_ATChannels[channelID], "AT+CEREG=2", NULL);
            at_send_command(s_ATChannels[channelID], "AT+SPEDDAENABLE=0", NULL);
        }
        at_send_command(s_ATChannels[channelID], "AT+CREG=2", NULL);
        at_send_command(s_ATChannels[channelID], "AT+CGREG=2", NULL);

        if (isVoLteEnable()) {
            at_send_command(s_ATChannels[channelID], "AT+CIREG=2", NULL);
        }
        if (isExistActivePdp(socket_id) && !strcmp(prop, "0")) {
            char cmd[ARRAY_SIZE] = {0};
            snprintf(cmd, sizeof(cmd), "AT*FDY=1,%d",
                     getFastDormancyTime(status));
            at_send_command(s_ATChannels[channelID], cmd, NULL);
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
        at_send_command(s_ATChannels[channelID], "AT+SPVOICEPREFER=0", NULL);
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

int vsimQueryVirtual(int socket_id){
    RLOGD("vsimQueryVirtual, phoneId: %d", socket_id);
    int err = -1;
    int channelID = -1;
    int vsimMode = -1;
    char *line = NULL;
    ATResponse *p_response = NULL;
    channelID = getChannel(socket_id);
    err = at_send_command_singleline(s_ATChannels[channelID],
            "AT+VIRTUALSIMINIT?", "+VIRTUALSIMINIT:", &p_response);
    if (err < 0 || p_response->success == 0) {
        RLOGD("vsim query virtual card error");
    } else {
        line = p_response->p_intermediates->line;
        RLOGD("vsim query virtual card resp:%s",line);
        err = at_tok_start(&line);
        err = at_tok_nextint(&line, &vsimMode);
    }
    at_response_free(p_response);
    if (vsimMode > 0) {
        at_send_command(s_ATChannels[channelID],"AT+RSIMRSP=\"ERRO\",1,", NULL);
    }
    putChannel(channelID);

    return vsimMode;
}

void onSimDisabled(int channelID) {
    int sim_status = getSIMStatus(false, channelID);
    at_send_command(s_ATChannels[channelID],
                    "AT+SFUN=5", NULL);
    setRadioState(channelID, RADIO_STATE_OFF);
}

int closeVirtual(int socket_id){
    RLOGD("closeVirtual, phoneId: %d", socket_id);
    int err = -1;
    int channelID = -1;
    channelID = getChannel(socket_id);

    err = at_send_command(s_ATChannels[channelID],"AT+RSIMRSP=\"VSIM\",0", NULL);
    onSimDisabled(channelID);

    putChannel(channelID);
    return err;
}

static void closeVirtualThread(void *param) {
    RLOGD("closeVsimCard");
    VirtualCardPara *virtualCardPara = (VirtualCardPara *)param;
    if (s_vsimClientFd < 0 ) {
        if (virtualCardPara->socket_id1 == 1) {
            closeVirtual(RIL_SOCKET_1);
        }
#if (SIM_COUNT >= 2)
        if (virtualCardPara->socket_id2 == 1) {
            closeVirtual(RIL_SOCKET_2);
        }
#endif
    }
    free(virtualCardPara);
}

void *listenVsimSocketThread() {
    int ret = -1;
    RLOGD("listenVsimSocketThread start");
    if (s_vsimServerFd < 0) {
        s_vsimServerFd = socket_local_server(SOCKET_NAME_VSIM,
               ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
        if (s_vsimServerFd < 0) {
            RLOGE("Failed to get socket %s", SOCKET_NAME_VSIM);
        }

        ret = listen(s_vsimServerFd, 1);
        if (ret < 0) {
            RLOGE("Failed to listen on control socket '%d': %s",
                    s_vsimServerFd, strerror(errno));
        }
    }
    s_vsimClientFd = accept(s_vsimServerFd, NULL, NULL);
    pthread_mutex_lock(&s_vsimSocketMutex);
    pthread_cond_signal(&s_vsimSocketCond);
    s_vsimListenLoop = true;
    pthread_mutex_unlock(&s_vsimSocketMutex);
    RLOGD("vsim connected %d",s_vsimClientFd);
    do {
        char error[ARRAY_SIZE] = {0};
        RLOGD("vsim read begin");
        if (TEMP_FAILURE_RETRY(read(s_vsimClientFd, &error, sizeof(error)))
                <= 0) {
            RLOGE("read error from vsim! err = %s",strerror(errno));
            close(s_vsimClientFd);
            s_vsimListenLoop = false;
            s_vsimClientFd = -1;

            int vsimMode1 = -1;
            int vsimMode2 = -1;
            VirtualCardPara *virtualCardPara = NULL;
            vsimMode1 = vsimQueryVirtual(RIL_SOCKET_1);
#if (SIM_COUNT >= 2)
    vsimMode2 = vsimQueryVirtual(RIL_SOCKET_2);
#endif

            virtualCardPara = (VirtualCardPara *)calloc(1, sizeof(VirtualCardPara));
            virtualCardPara->socket_id1 = vsimMode1;
            virtualCardPara->socket_id2 = vsimMode2;

            RIL_requestTimedCallback(closeVirtualThread,
                        (void *)virtualCardPara, &s_timevalCloseVsim);
        }
        RLOGD("vsim read %s",error);
    } while (s_vsimClientFd > 0);
    return NULL;
}

void vsimInit() {
    int ret;
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&tid, &attr, (void *)listenVsimSocketThread, NULL);
    if (ret < 0) {
        RLOGE("Failed to create listen_vsim_socket_thread errno: %d", errno);
    }
}

void* sendVsimReqThread(void *cmd) {
    RLOGD("vsim write cmd = %s", cmd);
    if (s_vsimClientFd >= 0) {
        int len = strlen((char *)cmd);
        RLOGD("vsim write cmd len= %d", len);
        if (TEMP_FAILURE_RETRY(write(s_vsimClientFd, cmd, len)) !=
                                      len) {
            RLOGE("Failed to write cmd to vsim!error = %s", strerror(errno));
            close(s_vsimClientFd);
            s_vsimClientFd = -1;

            vsimQueryVirtual(RIL_SOCKET_1);
#if (SIM_COUNT >= 2)
    vsimQueryVirtual(RIL_SOCKET_2);
#endif
        }
        RLOGD("vsim write OK");
    } else {
        RLOGE("vsim socket disconnected");
    }
    free(cmd);
    return NULL;
}

void sendVsimReq(char *cmd) {
    int ret;
    pthread_t tid;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&tid, &attr, (void *)sendVsimReqThread, (void *)cmd);
    if (ret < 0) {
        RLOGE("Failed to create sendVsimReqThread errno: %d", errno);
    }
}

void requestSendAT(int channelID, const char *data, size_t datalen,
                   RIL_Token t, char *atResp, int responseLen) {
    RIL_UNUSED_PARM(datalen);

    int i, err;
    char *ATcmd = (char *)data;
    char buf[MAX_AT_RESPONSE] = {0};
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
            if (t != NULL) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, response,
                                      sizeof(char *));
            } else if (atResp != NULL) {
                snprintf(atResp, responseLen, "ERROR");
            }
            return;
        }

        *pdu = '\0';
        pdu++;
        RLOGD("SNVM: cmd %s, pdu %s", cmd, pdu);
        err = at_send_command_snvm(s_ATChannels[channelID], cmd, pdu, "",
                                   &p_response);
    }  else if (strStartsWith(ATcmd, "VSIM_CREATE")) {
        int socket_id = getSocketIdByChannelID(channelID);
        s_vsimInitFlag[socket_id] = true;
        //create socket
        if (!s_vsimListenLoop) {
            vsimInit();
            response[0] = "OK";
        } else {
            response[0] = "ERROR";
        }
        snprintf(atResp, responseLen, "%s", response[0]);
        return;
    } else if (strStartsWith(ATcmd, "VSIM_INIT")) {
        char *cmd = NULL;
        RLOGD("wait for vsim socket connect");
        pthread_mutex_lock(&s_vsimSocketMutex);
        while (s_vsimClientFd < 0) {
            pthread_cond_wait(&s_vsimSocketCond, &s_vsimSocketMutex);
        }
        pthread_mutex_unlock(&s_vsimSocketMutex);
        RLOGD("vsim socket connected");
        //send AT
        cmd = ATcmd;
        at_tok_start(&cmd);
        err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    } else if (strStartsWith(ATcmd, "VSIM_EXIT")) {
        char *cmd = NULL;
        int socket_id = getSocketIdByChannelID(channelID);
        s_vsimInitFlag[socket_id] = false;

        //send AT
        cmd = ATcmd;
        at_tok_start(&cmd);
        err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
        if (err < 0 || p_response->success == 0) {
            if (p_response != NULL) {
                strlcat(buf, p_response->finalResponse, sizeof(buf));
                strlcat(buf, "\r\n", sizeof(buf));
                response[0] = buf;
                snprintf(atResp, responseLen, "%s", response[0]);
            } else {
                goto error;
            }
        } else {
#if (SIM_COUNT >= 2)
        if ((!s_vsimInitFlag[RIL_SOCKET_1]) && (!s_vsimInitFlag[RIL_SOCKET_2]))
#else
        if (!s_vsimInitFlag[RIL_SOCKET_1])
#endif
            {
                if (s_vsimClientFd != -1) {
                    close(s_vsimClientFd);
                    s_vsimClientFd = -1;
                }
                s_vsimListenLoop = false;
            }
            onSimDisabled(channelID);
            strlcat(buf, p_response->finalResponse, sizeof(buf));
            strlcat(buf, "\r\n", sizeof(buf));
            response[0] = buf;
            snprintf(atResp, responseLen, "%s", response[0]);
        }
        at_response_free(p_response);
        return;
    }  else if (strStartsWith(ATcmd, "VSIM_TIMEOUT")) {
        int time = -1;
        char *cmd = NULL;
        cmd = ATcmd;
        at_tok_start(&cmd);
        err = at_tok_nextint(&cmd, &time);
        RLOGD("VSIM_TIMEOUT:%d",time);
        if (time > 0) {
            s_timevalCloseVsim.tv_sec = time;
            response[0] = "OK";
        } else {
            response[0] = "ERROR";
        }
        snprintf(atResp, responseLen, "%s", response[0]);
        return;
    } else {
        err = at_send_command_multiline(s_ATChannels[channelID], ATcmd, "",
                                        &p_response);
    }

    if (err < 0 || p_response->success == 0) {
        if (err == AT_ERROR_CHANNEL_CLOSED) {
            goto error;
        }
        if (p_response != NULL) {
            strlcat(buf, p_response->finalResponse, sizeof(buf));
            strlcat(buf, "\r\n", sizeof(buf));
            response[0] = buf;
            if (t != NULL) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, response,
                                      sizeof(char *));
            } else if (atResp != NULL) {
                snprintf(atResp, responseLen, "%s", p_response->finalResponse);
            }
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
        if (t != NULL) {
            RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(char *));
        } else if (atResp != NULL) {
            snprintf(atResp, responseLen, "%s", buf);
        }
        if (!strncasecmp(ATcmd, "AT+SFUN=5", strlen("AT+SFUN=5"))) {
            setRadioState(channelID, RADIO_STATE_OFF);
        }
    }
    at_response_free(p_response);
    return;

error:
    if (t != NULL) {
        memset(buf, 0 ,sizeof(buf));
        strlcat(buf, "ERROR", sizeof(buf));
        strlcat(buf, "\r\n", sizeof(buf));
        response[0] = buf;
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, response,
                              sizeof(char *));
    } else if (atResp != NULL) {
        snprintf(atResp, responseLen, "ERROR");
    }
    at_response_free(p_response);

}

void sendCmdSync(int phoneId, char *cmd, char *response, int responseLen) {
    RLOGD("sendCmdSync: simId = %d, cmd = %s", phoneId, cmd);

    int channelID = getChannel((RIL_SOCKET_ID)phoneId);
    requestSendAT(channelID, (const char *)cmd, 0, NULL, response, responseLen);
    putChannel(channelID);
}

static void requestVsimCmd(int channelID, void *data, size_t datalen,
                             RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int i, err;
    char *ATcmd = (char *)data;
    char buf[MAX_AT_RESPONSE] = {0};
    char *cmd;
    char *pdu;
    char *response[1] = {NULL};
    ATLine *p_cur = NULL;
    ATResponse *p_response = NULL;

    if (ATcmd == NULL) {
        RLOGE("Invalid AT command");
        goto error;
    }

    if (strStartsWith(ATcmd, "VSIM_INIT")) {
        char *cmd = NULL;
        RLOGD("vsim init");
        //send AT
        cmd = ATcmd;
        at_tok_start(&cmd);
        err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    } else if (strStartsWith(ATcmd, "VSIM_EXIT")) {
        char *cmd = NULL;
        int socket_id = getSocketIdByChannelID(channelID);

        //send AT
        cmd = ATcmd;
        at_tok_start(&cmd);
        err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
        if (err >= 0 && p_response->success != 0) {
            onSimDisabled(channelID);
        }
    }  else if (strStartsWith(ATcmd, "VSIM_TIMEOUT")) {
        int time = -1;
        char *cmd = NULL;
        cmd = ATcmd;
        at_tok_start(&cmd);
        err = at_tok_nextint(&cmd, &time);
        RLOGD("VSIM_TIMEOUT:%d",time);
        if (time > 0) {
            s_timevalCloseVsim.tv_sec = time;
        }
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
    if (t != NULL) {
        memset(buf, 0 ,sizeof(buf));
        strlcat(buf, "ERROR", sizeof(buf));
        strlcat(buf, "\r\n", sizeof(buf));
        response[0] = buf;
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, response,
                              sizeof(char *));
    }
    at_response_free(p_response);
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
        case RIL_REQUEST_DEVICE_IDENTITY:
            requestDeviceIdentify(channelID, data, datalen, t);
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
            const char **cur = (const char **)data;

            requestSendAT(channelID, *cur, datalen, t, NULL, 0);
            break;
        }
        case RIL_REQUEST_NV_RESET_CONFIG: {
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;
        }
        case RIL_EXT_REQUEST_GET_BAND_INFO: {
            p_response = NULL;
            char *line = NULL;
            RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
            RLOGD("GET_BAND_INFO s_in4G[%d]=%d", socket_id, s_in4G[socket_id]);
            if (s_in4G[socket_id]) {
                err = at_send_command_singleline(s_ATChannels[channelID], "AT+SPCLB?",
                                                 "+SPCLB:", &p_response);
                if (err < 0 || p_response->success == 0) {
                    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
                } else {
                    line = p_response->p_intermediates->line;
                    at_tok_start(&line);
                    skipWhiteSpace(&line);
                    RIL_onRequestComplete(t, RIL_E_SUCCESS, line, strlen(line) + 1);
                }
                at_response_free(p_response);
            } else {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            }
            break;
        }
        case RIL_EXT_REQUEST_SET_BAND_INFO_MODE: {
            p_response = NULL;
            int mode = ((int *)data)[0];
            char cmd[AT_COMMAND_LEN] = {0};
            snprintf(cmd, sizeof(cmd), "AT+SPCLB=%d", mode);
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_EXT_REQUEST_SET_SPECIAL_RATCAP:{
            p_response = NULL;
            int value = ((int*)data)[0];
            char cmd[32] = {0};
            snprintf(cmd, sizeof(cmd), "AT+SPOPRCAP=1,1,%d", value);
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }

            at_response_free(p_response);
            break;
        }
        case RIL_EXT_REQUEST_SEND_CMD: {
            requestSendAT(channelID, (const char *)data, datalen, t, NULL, 0);
            break;
        }
        case RIL_ATC_REQUEST_VSIM_SEND_CMD: {
            requestVsimCmd(channelID, (const char *)data, datalen, t);
            break;
        }
        case RIL_EXT_REQUEST_RADIO_POWER_FALLBACK: {
            p_response = NULL;
            int mode = ((int *)data)[0];
            char cmd[AT_COMMAND_LEN] = {0};
            snprintf(cmd, sizeof(cmd), "AT+SPPOWERFB=%d", mode);
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        default:
            return 0;
    }

    return 1;
}

int isVendorRadioProp(char *key) {
    int i;
    const char *prop_buffer[] = {
            "persist.vendor.radio.",
            "ro.vendor.modem.",
            "ro.vendor.radio.",
            "vendor.radio.",
            "vendor.ril.",
            "vendor.sim.",
            "vendor.data.",
            "vendor.net.",
            "persist.vendor.sys.",
            "vendor.sys.",
            NULL
    };
    for (i = 0; prop_buffer[i]; i++) {
        if (strncmp(prop_buffer[i], key, strlen(prop_buffer[i])) == 0) {
            return 1;
        }
    }
    return 0;
}

static void requestGetRadioPreference(int request, void *data,
        size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    char prop[PROPERTY_VALUE_MAX] = {0};
    char *key = NULL;

    key = ((char *)data);
    if (!isVendorRadioProp(key)) {
        RLOGE("get %s is not vendor radio prop", key);
        RIL_onRequestComplete(t, RIL_E_INVALID_ARGUMENTS, NULL, 0);
    } else {
        property_get(key, prop, "");
        RLOGD("get prop key = %s, value = %s", key, prop);
        RIL_onRequestComplete(t, RIL_E_SUCCESS, &prop, sizeof(prop));
    }
}

static void requestSetRadioPreference(int request, void *data,
        size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(datalen);
    int ret = 0;
    char key[PROPERTY_VALUE_MAX] = {0};
    char value[PROPERTY_VALUE_MAX] = {0};

    strncpy(key, ((char **)data)[0], strlen(((char **)data)[0]) + 1);
    strncpy(value, ((char **)data)[1], strlen(((char **)data)[1]) + 1);

    if (!isVendorRadioProp(key)) {
        RLOGE("set %s is not vendor radio prop", key);
        RIL_onRequestComplete(t, RIL_E_INVALID_ARGUMENTS, NULL, 0);
    } else {
        RLOGD("set prop key = %s, value = %s", key, value);
        ret = property_set(key, value);
        if (ret < 0) {
            RLOGE("Error while set prop!");
            RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        } else {
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        }
    }
}

int processPropRequests(int request, void *data, size_t datalen, RIL_Token t) {
    switch (request) {
        case RIL_EXT_REQUEST_GET_RADIO_PREFERENCE: {
            requestGetRadioPreference(request, data, datalen, t);
            break;
        }
        case RIL_EXT_REQUEST_SET_RADIO_PREFERENCE: {
            requestSetRadioPreference(request, data, datalen, t);
            break;
        }
        default:
            return 0;
    }
    return 1;
}

//when high rate mode, open DVFS/Thermal/IPA/lock CUP Freq/RPS
static void handleHighRateMode() {
    RLOGD("handleHighRateMode START!");
    //open RPS
    property_set("ctl.start","vendor.rps_on");

    //CPU Frequency
    setCPUFrequency(true);

    setThermal(true);
    RLOGD("handleHighRateMode DONE!");
}

static void handleNormalRateMode() {
    RLOGD("handleNormalRateMode START!");
    //open RPS
    property_set("ctl.start","vendor.rps_off");

    //need auto CPU Frequency
    setCPUFrequency(false);

    //Thermal
    setThermal(false);
    RLOGD("handleNormalRateMode DONE!");
}

int processMiscUnsolicited(RIL_SOCKET_ID socket_id, const char *s) {
    int err;
    char *line = NULL;

    if (strStartsWith(s, "+CTZV:")) {
        /* NITZ time */
        char *response = NULL;
        char *tmp = NULL;
        char *tmp_response = NULL;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextstr(&tmp, &tmp_response);
        if (err != 0) {
            RLOGE("invalid NITZ line %s\n", s);
        } else {
            if (strstr(tmp_response, "//,::")) {
                char strTm[ARRAY_SIZE/2] = {0}, tmpRsp[ARRAY_SIZE] = {0};
                time_t now = time(NULL);
                struct tm *curtime = gmtime(&now);

                strftime(strTm, sizeof(strTm), "%y/%m/%d,%H:%M:%S", curtime);
                snprintf(tmpRsp, sizeof(tmpRsp), "%s%s", strTm, tmp_response + strlen("//,::"));
                response = tmpRsp;
            } else {
                response = tmp_response;
            }
            RIL_onUnsolicitedResponse(RIL_UNSOL_NITZ_TIME_RECEIVED, response,
                                      strlen(response) + 1, socket_id);
        }
    } else if (strStartsWith(s, "+SPCLB:")) {
        char *tmp;
        char *response = NULL;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);
        skipWhiteSpace(&tmp);
        response = tmp;
        RIL_onUnsolicitedResponse(RIL_EXT_UNSOL_BAND_INFO, response,
                                  strlen(response) + 1, socket_id);
    } else if (strStartsWith(s, "%RSIMREQ:")) {
        char *tmp;
        char *response = NULL;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);
        skipWhiteSpace(&tmp);
        response = (char *)calloc((strlen(tmp) + 5), sizeof(char));
        snprintf(response, strlen(tmp) + 4, "%d,%s\r\n", socket_id, tmp);
        RIL_onUnsolicitedResponse(RIL_ATC_UNSOL_VSIM_RSIM_REQ, response,
                                  strlen(response) + 1, socket_id);
    } else if (strStartsWith(s, "+SPLTERATEMODE:")) {
       /*
        * +SPLTERATEMODE:<mode>,[max],[rate]
        *
        * <mode>    description
        * 0              Low/Normal rate mode
        * 1              High rate mode
        * <max>      Current band max rate
        * <rate>      Latest detected rate
        */
        int mode, rate, max_rate;
        int err;
        char* tmp;

        RLOGD("CA NVIOT rate URC: %s", s);
        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextint(&tmp, &mode);
        if (err < 0) {
            RLOGD("CA NVIOT rate -- get mode error");
            goto out;
        }

        if (at_tok_hasmore(&tmp)) {
            err = at_tok_nextint(&tmp, &max_rate);
            if (err < 0) {
                RLOGD("CA NVIOT rate -- get max error");
                goto out;
            }
        }

        if (at_tok_hasmore(&tmp)) {
            err = at_tok_nextint(&tmp, &rate);
            if (err < 0) {
                RLOGD("CA NVIOT rate -- get rate error");
                goto out;
            }
        }

        if (mode) {
            handleHighRateMode();
        } else {
            handleNormalRateMode();
        }
    } else {
        return 0;
    }

out:
    free(line);
    return 1;
}
