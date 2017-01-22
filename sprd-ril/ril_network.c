/**
 * ril_network.c --- Network-related requests process functions implementation
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */
#define LOG_TAG "RIL"

#include "sprd_ril.h"
#include "ril_network.h"
#include "ril_sim.h"
#include "ril_data.h"
#include "ril_misc.h"
#include "TelephonyEx.h"
#include "ril_async_cmd_handler.h"
#include "channel_controller.h"

/* Save physical cellID for AGPS */
#define PHYSICAL_CELLID_PROP    "gsm.cell.physical_cellid"
/* Save NITZ operator name string for UI to display right PLMN name */
#define NITZ_OPERATOR_PROP      "persist.radio.nitz.operator"
#define FIXED_SLOT_PROP         "ro.radio.fixed_slot"
#define PHONE_EXTENSION_PROP    "ril.sim.phone_ex.start"
#define COPS_MODE_PROP          "persist.sys.cops.mode"
/* set network type for engineer mode */
#define ENGTEST_ENABLE_PROP     "persist.radio.engtest.enable"

RIL_RegState s_CSRegStateDetail[SIM_COUNT] = {
        RIL_REG_STATE_UNKNOWN
#if (SIM_COUNT >= 2)
        ,RIL_REG_STATE_UNKNOWN
#if (SIM_COUNT >= 3)
        ,RIL_REG_STATE_UNKNOWN
#if (SIM_COUNT >= 4)
        ,RIL_REG_STATE_UNKNOWN
#endif
#endif
#endif
        };
RIL_RegState s_PSRegStateDetail[SIM_COUNT] = {
        RIL_REG_STATE_UNKNOWN
#if (SIM_COUNT >= 2)
        ,RIL_REG_STATE_UNKNOWN
#if (SIM_COUNT >= 3)
        ,RIL_REG_STATE_UNKNOWN
#if (SIM_COUNT >= 4)
        ,RIL_REG_STATE_UNKNOWN
#endif
#endif
#endif
        };
LTE_PS_REG_STATE s_PSRegState[SIM_COUNT] = {
        STATE_OUT_OF_SERVICE
#if (SIM_COUNT >= 2)
        ,STATE_OUT_OF_SERVICE
#if (SIM_COUNT >= 3)
        ,STATE_OUT_OF_SERVICE
#if (SIM_COUNT >= 4)
        ,STATE_OUT_OF_SERVICE
#endif
#endif
#endif
        };
SimBusy s_simBusy[SIM_COUNT] = {
        {false, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER}
#if (SIM_COUNT >= 2)
       ,{false, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER}
#endif
#if (SIM_COUNT >= 3)
       ,{false, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER}
#endif
#if (SIM_COUNT >= 4)
       ,{false, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER}
#endif
        };
static pthread_mutex_t s_workModeMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_simPresentMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_presentSIMCountMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_physicalCellidMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t s_LTEAttachMutex[SIM_COUNT] = {
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
pthread_mutex_t s_radioPowerMutex[SIM_COUNT] = {
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
pthread_mutex_t s_operatorInfoListMutex = PTHREAD_MUTEX_INITIALIZER;

int s_imsRegistered[SIM_COUNT];  // 0 == unregistered
int s_imsBearerEstablished[SIM_COUNT];
int s_in4G[SIM_COUNT];
int s_workMode[SIM_COUNT] = {0};
int s_desiredRadioState[SIM_COUNT] = {0};
int s_requestSetRC[SIM_COUNT] = {0};
int s_sessionId[SIM_COUNT] = {0};
int s_presentSIMCount = 0;
bool s_isSimPresent[SIM_COUNT];
static bool s_radioOnError[SIM_COUNT];  // 0 -- false, 1 -- true
OperatorInfoList s_operatorInfoList;

int s_psOpened[SIM_COUNT] = {0};
int rxlev[SIM_COUNT], ber[SIM_COUNT], rscp[SIM_COUNT];
int ecno[SIM_COUNT], rsrq[SIM_COUNT], rsrp[SIM_COUNT];
int rssi[SIM_COUNT], berr[SIM_COUNT];

void setSimPresent(RIL_SOCKET_ID socket_id, bool hasSim) {
    RLOGD("setSimPresent hasSim = %d", hasSim);
    pthread_mutex_lock(&s_simPresentMutex);
    s_isSimPresent[socket_id] = hasSim;
    pthread_mutex_unlock(&s_simPresentMutex);
}

int isSimPresent(RIL_SOCKET_ID socket_id) {
    int hasSim = 0;
    char prop[PROPERTY_VALUE_MAX];
    pthread_mutex_lock(&s_simPresentMutex);
    hasSim = s_isSimPresent[socket_id];
    pthread_mutex_unlock(&s_simPresentMutex);

    return hasSim;
}

void initSIMPresentState() {
    int simId = 0;
    pthread_mutex_lock(&s_presentSIMCountMutex);
    s_presentSIMCount = 0;
    for (simId = 0; simId < SIM_COUNT; simId++) {
        if (isSimPresent(simId) == 1) {
            ++s_presentSIMCount;
        }
    }
    pthread_mutex_unlock(&s_presentSIMCountMutex);
}

int getMultiMode() {
    char prop[PROPERTY_VALUE_MAX] = {0};
    int workMode = 0;
    property_get(MODEM_CONFIG_PROP, prop, "");
    if (strcmp(prop, "") == 0) {
        // WCDMA
        workMode = WCDMA_AND_GSM;
    } else if (strstr(prop, "TL_LF_TD_W_G")) {
        workMode = TD_LTE_AND_LTE_FDD_AND_W_AND_TD_AND_GSM_CSFB;
    } else if (strstr(prop, "TL_LF_W_G")) {
        workMode = TD_LTE_AND_LTE_FDD_AND_W_AND_GSM_CSFB;
    } else if (strstr(prop, "TL_TD_G")) {
        workMode = TD_LTE_AND_TD_AND_GSM_CSFB;
    } else if (strcmp(prop, "W_G,G") == 0) {
        workMode = WCDMA_AND_GSM;
    }
    return workMode;
}

int getWorkMode(RIL_SOCKET_ID socket_id) {
    int workMode = 0;
    char prop[PROPERTY_VALUE_MAX] = {0};
    char numToStr[ARRAY_SIZE];

    pthread_mutex_lock(&s_workModeMutex);
    getProperty(socket_id, MODEM_WORKMODE_PROP, prop, "10");

    workMode = atoi(prop);
    if (!strcmp(s_modem, "tl") || !strcmp(s_modem, "lf")) {
        if ((workMode == TD_AND_GSM) || (workMode == WCDMA_AND_GSM)) {
              workMode = TD_AND_WCDMA;
        }
    }

    initSIMPresentState();
    RLOGD("getWorkmode: s_presentSIMCount = %d", s_presentSIMCount);
#if (SIM_COUNT == 2)
    if (s_presentSIMCount == 1) {  // only one SIM card present
        if (!isSimPresent(socket_id)) {
            if (s_modemConfig == LWG_G || s_modemConfig == W_G) {
                workMode = GSM_ONLY;
            }
        } else {
            if (s_modemConfig == LWG_G || s_modemConfig == W_G) {
                if (workMode == GSM_ONLY || workMode == NONE) {
                    workMode = getMultiMode();
                }
            }
            if (s_multiModeSim != socket_id) {
                s_multiModeSim = socket_id;
                snprintf(numToStr, sizeof(numToStr), "%d", s_multiModeSim);
                property_set(PRIMARY_SIM_PROP, numToStr);
            }
        }
    }
    memset(numToStr, 0, sizeof(numToStr));
    snprintf(numToStr, sizeof(numToStr), "%d", workMode);
    setProperty(socket_id, MODEM_WORKMODE_PROP, numToStr);
#endif
    s_workMode[socket_id] = workMode;
    pthread_mutex_unlock(&s_workModeMutex);
    RLOGD("getWorkMode socket_id = %d, workMode = %d", socket_id, workMode);
    return workMode;
}

void buildWorkModeCmd(char *cmd, size_t size) {
    int simId;
    char strFormatter[AT_COMMAND_LEN] = {0};
    memset(strFormatter, 0, sizeof(strFormatter));

    for (simId = 0; simId < SIM_COUNT; simId++) {
        if (simId == 0) {
            snprintf(cmd, size, "AT+SPTESTMODEM=%d", getWorkMode(simId));
            if (SIM_COUNT == 1) {
                strncpy(strFormatter, cmd, size);
                strncat(strFormatter, ",%d", strlen(",%d"));
                snprintf(cmd, size, strFormatter, GSM_ONLY);
            }
        } else {
            strncpy(strFormatter, cmd, size);
            strncat(strFormatter, ",%d", strlen(",%d"));
            snprintf(cmd, size, strFormatter, getWorkMode(simId));
        }
    }
}

void *setRadioOnWhileSimBusy(void *param) {
    int channelID;
    int err;
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);

    RLOGD("SIM is busy now, please wait!");
    pthread_mutex_lock(&s_simBusy[socket_id].s_sim_busy_mutex);
    pthread_cond_wait(&s_simBusy[socket_id].s_sim_busy_cond,
                        &s_simBusy[socket_id].s_sim_busy_mutex);
    pthread_mutex_unlock(&s_simBusy[socket_id].s_sim_busy_mutex);

    RLOGD("CPIN is READY now, set radio power on again!");
    channelID = getChannel(socket_id);
    err = at_send_command(s_ATChannels[channelID], "AT+SFUN=4", &p_response);
    if (err < 0|| p_response->success == 0) {
        if (isRadioOn(channelID) != 1) {
            goto error;
        }
    }
    setRadioState(channelID, RADIO_STATE_SIM_NOT_READY);

error:
    putChannel(channelID);
    at_response_free(p_response);
    return NULL;
}

static int mapCgregResponse(int in_response) {
    int out_response = 0;

    switch (in_response) {
        case 0:
            out_response = 1;    /* GPRS */
            break;
        case 3:
            out_response = 2;    /* EDGE */
            break;
        case 2:
            out_response = 3;    /* TD */
            break;
        case 4:
            out_response = 9;    /* HSDPA */
            break;
        case 5:
            out_response = 10;   /* HSUPA */
            break;
        case 6:
            out_response = 11;   /* HSPA */
            break;
        case 15:
            out_response = 15;   /* HSPA+ */
            break;
        case 7:
            out_response = 14;   /* LTE */
            break;
        case 16:
            out_response = 19;   /* LTE_CA */
            break;
        default:
            out_response = 0;    /* UNKNOWN */
            break;
    }
    return out_response;
}

static int mapRegState(int inResponse) {
    int outResponse = RIL_REG_STATE_UNKNOWN;

    switch (inResponse) {
        case 0:
            outResponse = RIL_REG_STATE_NOT_REG;
            break;
        case 1:
            outResponse = RIL_REG_STATE_HOME;
            break;
        case 2:
            outResponse = RIL_REG_STATE_SEARCHING;
            break;
        case 3:
            outResponse = RIL_REG_STATE_DENIED;
            break;
        case 4:
            outResponse = RIL_REG_STATE_UNKNOWN;
            break;
        case 5:
            outResponse = RIL_REG_STATE_ROAMING;
            break;
        case 8:
        case 10:
            outResponse = RIL_REG_STATE_NOT_REG_EMERGENCY_CALL_ENABLED;
            break;
        case 12:
            outResponse = RIL_REG_STATE_SEARCHING_EMERGENCY_CALL_ENABLED;
            break;
        case 13:
            outResponse = RIL_REG_STATE_DENIED_EMERGENCY_CALL_ENABLED;
            break;
        case 14:
            outResponse = RIL_REG_STATE_UNKNOWN_EMERGENCY_CALL_ENABLED;
            break;
        default:
            outResponse = RIL_REG_STATE_UNKNOWN;
            break;
    }
    return outResponse;
}

/* 1 explain that CS/PS domain is 4G */
int is4G(int urcNetType, int mapNetType, RIL_SOCKET_ID socketId) {
    if (urcNetType == 7 || urcNetType == 16) {
        s_in4G[socketId] = 1;
    } else if (mapNetType == 14 || mapNetType == 19) {
        s_in4G[socketId] = 1;
    } else {
        s_in4G[socketId] = 0;
    }
    return s_in4G[socketId];
}

static void requestSignalStrength(int channelID, void *data,
                                      size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int err;
    char *line;
    ATResponse *p_response = NULL;
    ATResponse *p_newResponse = NULL;
    RIL_SignalStrength_v6 response_v6;

    RIL_SIGNALSTRENGTH_INIT(response_v6);

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

    err = at_tok_nextint(&line,
            &(response_v6.GW_SignalStrength.signalStrength));
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &(response_v6.GW_SignalStrength.bitErrorRate));
    if (err < 0) goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response_v6,
                          sizeof(RIL_SignalStrength_v6));
    at_response_free(p_response);
    at_response_free(p_newResponse);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
    at_response_free(p_newResponse);
}

static void requestSignalStrengthLTE(int channelID, void *data,
                                          size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int err;
    int skip;
    char *line;
    int response[6] = {-1, -1, -1, -1, -1, -1};
    ATResponse *p_response = NULL;
    ATResponse *p_newResponse = NULL;
    RIL_SignalStrength_v6 responseV6;

    RIL_SIGNALSTRENGTH_INIT(responseV6);

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+CESQ",
                                     "+CESQ:", &p_response);
    if (err < 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
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
        responseV6.GW_SignalStrength.signalStrength = response[0];
    }
    if (response[2] != -1 && response[2] != 255) {
        responseV6.GW_SignalStrength.signalStrength = response[2];
    }
    if (response[5] != -1 && response[5] != 255 && response[5] != -255) {
        responseV6.LTE_SignalStrength.rsrp = response[5];
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &responseV6,
                          sizeof(RIL_SignalStrength_v6));
    at_response_free(p_response);
    at_response_free(p_newResponse);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
    at_response_free(p_newResponse);
}

static void queryCeregTac(int channelID, int *tac) {
    int err;
    int commas, skip;
    int response[2] = {-1, -1};
    char *p = NULL;
    char *line = NULL;
    ATResponse *p_response = NULL;

    err = at_send_command_singleline(s_ATChannels[channelID],
            "AT+CEREG?", "+CEREG:", &p_response);
    if (err != 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    commas = 0;
    for (p = line; *p != '\0'; p++) {
        if (*p == ',') commas++;
    }
    // +CEREG: <n>,<stat>[,[<tac>,[<ci>],[<AcT>]]{,<cause_type>,<reject_cause>]}
    if (commas >= 2) {
        err = at_tok_nextint(&line, &skip);
        if (err < 0) goto error;
        err = at_tok_nextint(&line, &response[0]);
        if (err < 0) goto error;
        err = at_tok_nexthexint(&line, &response[1]);
        if (err < 0) goto error;
    }

    if (response[1] != -1) {
        RLOGD("CEREG tac = %x", response[1]);
        *tac = response[1];
    }

error:
    at_response_free(p_response);
}
static void requestRegistrationState(int channelID, int request,
                                          void *data, size_t datalen,
                                          RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int i, err;
    int commas, skip;
    int response[4] = {-1, -1, -1, -1};
    char *responseStr[15] = {NULL};
    char res[5][20];
    char *line, *p;
    const char *cmd;
    const char *prefix;
    ATResponse *p_response = NULL;

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
    bool islte = s_isLTE;

    if (request == RIL_REQUEST_VOICE_REGISTRATION_STATE) {
        cmd = "AT+CREG?";
        prefix = "+CREG:";
    } else if (request == RIL_REQUEST_DATA_REGISTRATION_STATE) {
        if (islte) {
            cmd = "AT+CEREG?";
            prefix = "+CEREG:";
        } else {
            cmd = "AT+CGREG?";
            prefix = "+CGREG:";
        }
    } else if (request == RIL_REQUEST_IMS_REGISTRATION_STATE) {
        cmd = "AT+CIREG?";
        prefix = "+CIREG:";
    }  else {
        assert(0);
        goto error;
    }

    err = at_send_command_singleline(s_ATChannels[channelID], cmd, prefix,
                                     &p_response);
    if (err != 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    /* Ok you have to be careful here
     * The solicited version of the CREG response is
     * +CREG: n, stat, [lac, cid]
     * and the unsolicited version is
     * +CREG: stat, [lac, cid]
     * The <n> parameter is basically "is unsolicited creg on?"
     * which it should always be
     *
     * Now we should normally get the solicited version here,
     * but the unsolicited version could have snuck in
     * so we have to handle both
     *
     * Also since the LAC and CID are only reported when registered,
     * we can have 1, 2, 3, or 4 arguments here
     *
     * finally, a +CGREG: answer may have a fifth value that corresponds
     * to the network type, as in;
     *
     *   +CGREG: n, stat [,lac, cid [,networkType]]
     */

    /* count number of commas */
    commas = 0;
    for (p = line; *p != '\0'; p++) {
        if (*p == ',') commas++;
    }

    switch (commas) {
        case 0: {  /* +CREG: <stat> */
            err = at_tok_nextint(&line, &response[0]);
            if (err < 0) goto error;
            break;
        }
        case 1: {  /* +CREG: <n>, <stat> */
            err = at_tok_nextint(&line, &skip);
            if (err < 0) goto error;
            err = at_tok_nextint(&line, &response[0]);
            if (err < 0) goto error;
            break;
        }
        case 2: {
            // +CREG: <stat>, <lac>, <cid>
            // or+CIREG: <n>, <reg_info>, [<ext_info>]
            err = at_tok_nextint(&line, &response[0]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[1]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[2]);
            if (err < 0) goto error;
            break;
        }
        case 3: {  /* +CREG: <n>, <stat>, <lac>, <cid> */
            err = at_tok_nextint(&line, &skip);
            if (err < 0) goto error;
            err = at_tok_nextint(&line, &response[0]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[1]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[2]);
            if (err < 0) goto error;
            break;
        }
        /* special case for CGREG, there is a fourth parameter
         * that is the network type (unknown/gprs/edge/umts)
         */
        case 4: {  /* +CGREG: <n>, <stat>, <lac>, <cid>, <networkType> */
            err = at_tok_nextint(&line, &skip);
            if (err < 0) goto error;
            err = at_tok_nextint(&line, &response[0]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[1]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[2]);
            if (err < 0) goto error;
            err = at_tok_nextint(&line, &response[3]);
            if (err < 0) goto error;
            break;
        }
        case 5: {  /* +CEREG: <n>, <stat>, <lac>, <rac>, <cid>, <networkType> */
            err = at_tok_nextint(&line, &skip);
            if (err < 0) goto error;
            err = at_tok_nextint(&line, &response[0]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[1]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[2]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &skip);
            if (err < 0) goto error;
            err = at_tok_nextint(&line, &response[3]);
            if (err < 0) goto error;
            break;
        }
        default:
            goto error;
    }

    int regState = mapRegState(response[0]);
    if (request == RIL_REQUEST_VOICE_REGISTRATION_STATE) {
        s_CSRegStateDetail[socket_id] = regState;
    } else if (request == RIL_REQUEST_DATA_REGISTRATION_STATE) {
        s_PSRegStateDetail[socket_id] = regState;
    }

    if (8 == response[0]) {
        response[0] = RIL_REG_STATE_NOT_REG_EMERGENCY_CALL_ENABLED;
    }
    snprintf(res[0], sizeof(res[0]), "%d", response[0]);
    responseStr[0] = res[0];

    if (response[1] != -1) {
        snprintf(res[1], sizeof(res[1]), "%x", response[1]);
        responseStr[1] = res[1];
    }

    if (response[2] != -1) {
        snprintf(res[2], sizeof(res[2]), "%x", response[2]);
        responseStr[2] = res[2];
    }

    if (response[3] != -1) {
        response[3] = mapCgregResponse(response[3]);
        snprintf(res[3], sizeof(res[3]), "%d", response[3]);
        responseStr[3] = res[3];
    }

    if (islte && request == RIL_REQUEST_DATA_REGISTRATION_STATE) {
        if (response[0] == 1 || response[0] == 5) {
            pthread_mutex_lock(&s_LTEAttachMutex[socket_id]);
            if (s_PSRegState[socket_id] == STATE_OUT_OF_SERVICE) {
                s_PSRegState[socket_id] = STATE_IN_SERVICE;
            }
            pthread_mutex_unlock(&s_LTEAttachMutex[socket_id]);
            is4G(-1, response[3], socket_id);
        } else {
            pthread_mutex_lock(&s_LTEAttachMutex[socket_id]);
            if (s_PSRegState[socket_id] == STATE_IN_SERVICE) {
                s_PSRegState[socket_id] = STATE_OUT_OF_SERVICE;
            }
            pthread_mutex_unlock(&s_LTEAttachMutex[socket_id]);
            s_in4G[socket_id] = 0;
        }
    }

    if (request == RIL_REQUEST_VOICE_REGISTRATION_STATE) {
        snprintf(res[4], sizeof(res[4]), "0");
        responseStr[7] = res[4];
        if (islte && s_in4G[socket_id]) {
            queryCeregTac(channelID, &(response[1]));
            snprintf(res[1], sizeof(res[1]), "%x", response[1]);
            responseStr[1] = res[1];
        }
        RIL_onRequestComplete(t, RIL_E_SUCCESS, responseStr,
                              15 * sizeof(char *));
    } else if (request == RIL_REQUEST_DATA_REGISTRATION_STATE) {
        snprintf(res[4], sizeof(res[4]), "3");
        responseStr[5] = res[4];
        RIL_onRequestComplete(t, RIL_E_SUCCESS, responseStr,
                              6 * sizeof(char *));
    } else if (request == RIL_REQUEST_IMS_REGISTRATION_STATE) {
        s_imsRegistered[socket_id] = response[1];
        RLOGD("imsRegistered[%d] = %d", socket_id, s_imsRegistered[socket_id]);
        RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));
    }
    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static int getOperatorName(char *plmn, char *operatorName) {
    if (plmn == NULL || operatorName == NULL) {
        return -1;
    }

    int ret = -1;
    MUTEX_ACQUIRE(s_operatorInfoListMutex);
    OperatorInfoList *pList = s_operatorInfoList.next;
    OperatorInfoList *next;
    while (pList != &s_operatorInfoList) {
        next = pList->next;
        if (strcmp(plmn, pList->plmn) == 0) {
            memcpy(operatorName, pList->operatorName,
                   strlen(pList->operatorName) + 1);
            ret = 0;
            break;
        }
        pList = next;
    }
    MUTEX_RELEASE(s_operatorInfoListMutex);
    return ret;
}

static void addToOperatorInfoList(char *plmn, char *operatorName) {
    if (plmn == NULL || operatorName == NULL) {
        return;
    }

    MUTEX_ACQUIRE(s_operatorInfoListMutex);

    OperatorInfoList *pList = s_operatorInfoList.next;
    OperatorInfoList *next;
    while (pList != &s_operatorInfoList) {
        next = pList->next;
        if (strcmp(plmn, pList->plmn) == 0) {
            RLOGD("addToOperatorInfoList: had add this operator before");
            goto exit;
        }
        pList = next;
    }

    int plmnLen = strlen(plmn) + 1;
    int nameLen = strlen(operatorName) + 1;

    OperatorInfoList *pNode =
            (OperatorInfoList *)calloc(1, sizeof(OperatorInfoList));
    pNode->plmn = (char *)calloc(plmnLen, sizeof(char));
    pNode->operatorName = (char *)calloc(nameLen, sizeof(char));

    memcpy(pNode->plmn, plmn, plmnLen);
    memcpy(pNode->operatorName, operatorName, nameLen);

    OperatorInfoList *pHead = &s_operatorInfoList;
    pNode->next = pHead;
    pNode->prev = pHead->prev;
    pHead->prev->next = pNode;
    pHead->prev = pNode;

exit:
    MUTEX_RELEASE(s_operatorInfoListMutex);
}

static void requestOperator(int channelID, void *data, size_t datalen,
                                RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int err;
    int i;
    int skip;
    char prop[PROPERTY_VALUE_MAX];
    char *response[3];
    ATLine *p_cur;
    ATResponse *p_response = NULL;

    memset(response, 0, sizeof(response));

    property_get(PHONE_EXTENSION_PROP, prop, "false");

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    err = at_send_command_multiline(s_ATChannels[channelID],
            "AT+COPS=3,0;+COPS?;+COPS=3,1;+COPS?;+COPS=3,2;+COPS?", "+COPS:",
            &p_response);

    /* we expect 3 lines here:
     * +COPS: 0,0,"T - Mobile"
     * +COPS: 0,1,"TMO"
     * +COPS: 0,2,"310170"
     */
    if (err != 0) goto error;

    for (i = 0, p_cur = p_response->p_intermediates; p_cur != NULL;
         p_cur = p_cur->p_next, i++) {
        char *line = p_cur->line;

        err = at_tok_start(&line);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &skip);
        if (err < 0) goto error;

        /* If we're unregistered, we may just get
         * a "+COPS: 0" response
         */
        if (!at_tok_hasmore(&line)) {
            response[i] = NULL;
            continue;
        }

        err = at_tok_nextint(&line, &skip);
        if (err < 0) goto error;

        /* a "+COPS: 0, n" response is also possible */
        if (!at_tok_hasmore(&line)) {
            response[i] = NULL;
            continue;
        }

        err = at_tok_nextstr(&line, &(response[i]));
        if (err < 0) goto error;
    }

    if (i != 3) {
        /* expect 3 lines exactly */
        goto error;
    }

#if defined (RIL_EXTENSION)
    if (strcmp(prop, "true") == 0 && response[2] != NULL) {
        int ret = -1;
        char updatedPlmn[64] = {0};

        memset(updatedPlmn, 0, sizeof(updatedPlmn));
        ret = getOperatorName(response[2], updatedPlmn);
        if (ret != 0) {
            err = updatePlmn(socket_id, (const char *)(response[2]), updatedPlmn);
            if (err == 0 && strcmp(updatedPlmn, response[2])) {
                RLOGD("updated plmn = %s", updatedPlmn);
                response[0] = updatedPlmn;
                response[1] = updatedPlmn;
                addToOperatorInfoList(response[2], updatedPlmn);
            }
        } else {
            RLOGD("get Operator Name = %s", updatedPlmn);
            response[0] = updatedPlmn;
            response[1] = updatedPlmn;
        }
    }
#endif

    RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));
    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static int getRadioFeatures(int socket_id, int isUnsolHandling) {
    int rat = 0;
    const int WCDMA = RAF_HSDPA | RAF_HSUPA | RAF_HSPA | RAF_HSPAP | RAF_UMTS;
    const int GSM = RAF_GSM | RAF_GPRS | RAF_EDGE;
    int workMode;

    if (s_modemConfig == LWG_WG) {
        if (socket_id == s_multiModeSim) {
            rat = RAF_LTE | WCDMA | GSM;
        } else {
            rat = WCDMA | GSM;
        }
    } else if (s_modemConfig == LWG_LWG) {
        rat = RAF_LTE | WCDMA | GSM;
    } else {
        if (isUnsolHandling == 1) {  // UNSOL_RADIO_CAPABILITY
            char prop[PROPERTY_VALUE_MAX] = {0};
            getProperty(socket_id, MODEM_WORKMODE_PROP, prop, "10");
            workMode = atoi(prop);
        } else {  // GET_RADIO_CAPABILITY
            workMode = getWorkMode(socket_id);
        }
        if (workMode == GSM_ONLY) {
            rat = GSM;
        } else if (workMode == NONE) {
            rat = RAF_UNKNOWN;
        } else if (s_isLTE) {
            rat = RAF_LTE | WCDMA | GSM;
        } else {
            rat = WCDMA | GSM;
        }
        if (workMode == NONE && isSimPresent(socket_id)) {
           rat = GSM;
        }
    }
    RLOGD("getRadioFeatures rat %d", rat);
    return rat;
}

static void sendUnsolRadioCapability() {
    RIL_RadioCapability *responseRc = (RIL_RadioCapability *)malloc(
             sizeof(RIL_RadioCapability));
    memset(responseRc, 0, sizeof(RIL_RadioCapability));
    responseRc->version = RIL_RADIO_CAPABILITY_VERSION;
    responseRc->session = s_sessionId[s_multiModeSim];
    responseRc->phase = RC_PHASE_UNSOL_RSP;
    responseRc->rat = getRadioFeatures(s_multiModeSim, 1);
    responseRc->status = RC_STATUS_SUCCESS;
    strncpy(responseRc->logicalModemUuid, "com.sprd.modem_multiMode",
            sizeof("com.sprd.modem_multiMode"));
    RIL_onUnsolicitedResponse(RIL_UNSOL_RADIO_CAPABILITY, responseRc,
            sizeof(RIL_RadioCapability), s_multiModeSim);
    s_sessionId[s_multiModeSim] = 0;
#if (SIM_COUNT == 2)
    RIL_SOCKET_ID singleModeSim = RIL_SOCKET_1;
    if (s_multiModeSim == RIL_SOCKET_1) {
        singleModeSim = RIL_SOCKET_2;
    } else if (s_multiModeSim == RIL_SOCKET_2) {
        singleModeSim = RIL_SOCKET_1;
    }
    responseRc->session = s_sessionId[singleModeSim];
    responseRc->rat = getRadioFeatures(singleModeSim, 1);
    strncpy(responseRc->logicalModemUuid, "com.sprd.modem_singleMode",
            sizeof("com.sprd.modem_singleMode"));
    RIL_onUnsolicitedResponse(RIL_UNSOL_RADIO_CAPABILITY, responseRc,
            sizeof(RIL_RadioCapability), singleModeSim);
    s_sessionId[singleModeSim] = 0;
#endif
    free(responseRc);
}

static void requestRadioPower(int channelID, void *data, size_t datalen,
                                  RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err, i;
    char sim_prop[PROPERTY_VALUE_MAX];
    char data_prop[PROPERTY_VALUE_MAX];
    char cmd[AT_COMMAND_LEN] = {0};
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    assert(datalen >= sizeof(int *));
    s_desiredRadioState[socket_id] = ((int *)data)[0];
    if (s_desiredRadioState[socket_id] == 0) {
        int sim_status = getSIMStatus(channelID);

        /* The system ask to shutdown the radio */
        err = at_send_command(s_ATChannels[channelID],
                "AT+SFUN=5", &p_response);
        if (err < 0 || p_response->success == 0) {
            goto error;
        }

        for (i = 0; i < MAX_PDP; i++) {
            if (s_dataAllowed[socket_id] && s_PDP[i].state == PDP_BUSY) {
                RLOGD("s_PDP[%d].state = %d", i, s_PDP[i].state);
                putPDP(i);
            }
        }
        setRadioState(channelID, RADIO_STATE_OFF);
    } else if (s_desiredRadioState[socket_id] > 0 &&
                s_radioState[socket_id] == RADIO_STATE_OFF) {
        initSIMPresentState();
        buildWorkModeCmd(cmd, sizeof(cmd));

#if (SIM_COUNT == 2)
        if (s_presentSIMCount == 0) {
            if (socket_id != s_multiModeSim) {
                RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED,
                                          NULL, 0, socket_id);
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
                return;
            }
        } else if (s_presentSIMCount == 1) {
            if (isSimPresent(socket_id) == 0) {
                RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED,
                                          NULL, 0, socket_id);
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
                return;
            }
        }
#endif

        at_send_command(s_ATChannels[channelID], cmd, NULL);

        if (s_isLTE) {
            at_send_command(s_ATChannels[channelID], "AT+CEMODE=1", NULL);
            p_response = NULL;
            if (s_presentSIMCount == 1) {
                err = at_send_command(s_ATChannels[channelID], "AT+SAUTOATT=1",
                                      &p_response);
                if (err < 0 || p_response->success == 0) {
                    RLOGE("GPRS auto attach failed!");
                }
                AT_RESPONSE_FREE(p_response);
            }
#if defined (ANDROID_MULTI_SIM)
            else {
                if (socket_id != s_multiModeSim) {
                    RLOGD("socket_id = %d, s_dataAllowed = %d", socket_id,
                          s_dataAllowed[socket_id]);
                    snprintf(cmd, sizeof(cmd), "AT+SPSWITCHDATACARD=%d,0", socket_id);
                    at_send_command(s_ATChannels[channelID], cmd, NULL);
                }
            }
#endif
        } else {
#if defined (ANDROID_MULTI_SIM)
            if (s_presentSIMCount == 2) {
                if (s_workMode[socket_id] != GSM_ONLY) {
                    at_send_command(s_ATChannels[channelID],
                            "AT+SAUTOATT=1", NULL);
                } else {
                    at_send_command(s_ATChannels[channelID],
                            "AT+SAUTOATT=0", NULL);
                }
            } else {
                at_send_command(s_ATChannels[channelID], "AT+SAUTOATT=1", NULL);
            }
#else
            at_send_command(s_ATChannels[channelID], "AT+SAUTOATT=1", NULL);
#endif
        }

        err = at_send_command(s_ATChannels[channelID], "AT+SFUN=4",
                              &p_response);
        if (err < 0|| p_response->success == 0) {
            /* Some stacks return an error when there is no SIM,
             * but they really turn the RF portion on
             * So, if we get an error, let's check to see if it
             * turned on anyway
             */
            if (s_simBusy[socket_id].s_sim_busy) {
                RLOGD("Now SIM is busy, wait for CPIN READY and then"
                      "set radio on again.");
                pthread_t tid;
                pthread_attr_t attr;
                pthread_attr_init(&attr);
                pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
                pthread_create(&tid, &attr, (void *)setRadioOnWhileSimBusy,
                                 (void *)&s_socketId[socket_id]);
            }

            if (isRadioOn(channelID) != 1) {
                goto error;
            }

            if (err == AT_ERROR_TIMEOUT) {
                s_radioOnError[socket_id] = true;
                RLOGD("requestRadioPower: radio on ERROR");
                goto error;
            }
        }
        setRadioState(channelID, RADIO_STATE_SIM_NOT_READY);
    }

    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    return;

error:
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

static void requestQueryNetworkSelectionMode(int channelID, void *data,
                                                  size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int err;
    int response = 0;
    char *line;
    ATResponse *p_response = NULL;

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+COPS?",
                                     "+COPS:", &p_response);

    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &response);
    if (err < 0) goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(int));
    at_response_free(p_response);
    return;

error:
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

static void requestNetworkRegistration(int channelID, void *data,
                                            size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err;
    char cmd[AT_COMMAND_LEN] = {0};
    ATResponse *p_response = NULL;
    RIL_NetworkList *network = (RIL_NetworkList *)data;

    char prop[PROPERTY_VALUE_MAX];
    int copsMode = 1;
    property_get(COPS_MODE_PROP, prop, "manual");
    if (!strcmp(prop, "automatic")) {
        copsMode = 4;
    }
    RLOGD("cops mode = %d", copsMode);

    if (network) {
        char *p = strstr(network->operatorNumeric, " ");
        if (p != NULL) {
            network->act = atoi(p + 1);
            *p = 0;
        }
        if (network->act >= 0) {
            snprintf(cmd, sizeof(cmd), "AT+COPS=%d,2,\"%s\",%d", copsMode,
                     network->operatorNumeric, network->act);
        } else {
            snprintf(cmd, sizeof(cmd), "AT+COPS=%d,2,\"%s\"", copsMode,
                     network->operatorNumeric);
        }
        err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
        if (err != 0 || p_response->success == 0) {
            goto error;
        }
    } else {
        goto error;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static bool plmnFiltration(char *plmn) {
    int i;
    // Array of unwanted plmns; "46003","46005","46011","45502" are CTCC
    char *unwantedPlmns[] = {"46003", "46005", "46011", "45502"};
    int length = sizeof(unwantedPlmns) / sizeof(char *);
    for (i = 0; i < length; i++) {
        if (strcmp(unwantedPlmns[i], plmn) == 0) {
            return true;
        }
    }
    return false;
}

static void requestNetworkList(int channelID, void *data, size_t datalen,
                                   RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    static char *statStr[] = {
        "unknown",
        "available",
        "current",
        "forbidden"
    };
    static char *actStr[] = {
        "GSM",
        "GSMCompact",
        "UTRAN",
        "E-UTRAN"
    };
    int err, stat, act;
    int tok = 0, count = 0, i = 0;
    char *line;
    char **responses, **cur;
    char *tmp, *startTmp = NULL;
    char prop[PROPERTY_VALUE_MAX];
    ATResponse *p_response = NULL;

    property_get(PHONE_EXTENSION_PROP, prop, "false");

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+COPS=?",
                                     "+COPS:", &p_response);
    if (err != 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;

    while (*line) {
        if (*line == '(')
            tok++;
        if (*line  == ')') {
            if (tok == 1) {
                count++;
                tok--;
            }
        }
        if (*line == '"') {
            do {
                line++;
                if (*line == 0)
                    break;
                else if (*line == '"')
                    break;
            } while (1);
        }
        if (*line != 0)
            line++;
    }
    RLOGD("Searched available network list numbers = %d", count - 2);
    if (count <= 2) {
        goto error;
    }
    count -= 2;

    line = p_response->p_intermediates->line;
    // (,,,,),,(0-4),(0-2)
    if (strstr(line, ",,,,")) {
        RLOGD("No network");
        goto error;
    }

    // (1,"CHINA MOBILE","CMCC","46000",0), (2,"CHINA MOBILE","CMCC","46000",2),
    // (3,"CHN-UNICOM","CUCC","46001",0),,(0-4),(0-2)
    responses = alloca(count * 4 * sizeof(char *));
    cur = responses;
    tmp = (char *)malloc(count * sizeof(char) * 30);
    startTmp = tmp;

    char *updatedNetList = (char *)alloca(count * sizeof(char) * 64);
    int unwantedPlmnCount = 0;
    while ((line = strchr(line, '(')) && (i++ < count)) {
        line++;
        err = at_tok_nextint(&line, &stat);
        if (err < 0) continue;

        cur[3] = statStr[stat];

        err = at_tok_nextstr(&line, &(cur[0]));
        if (err < 0) continue;

        err = at_tok_nextstr(&line, &(cur[1]));
        if (err < 0) continue;

        err = at_tok_nextstr(&line, &(cur[2]));
        if (err < 0) continue;
        if (plmnFiltration(cur[2])) {
            unwantedPlmnCount++;
            continue;
        }

        err = at_tok_nextint(&line, &act);
        if (err < 0) continue;
        snprintf(tmp, count * sizeof(char) * 30, "%s%s%d", cur[2], " ", act);
        RLOGD("requestNetworkList cur[2] act = %s", tmp);
        cur[2] = tmp;

#if defined (RIL_EXTENSION)
        if (strcmp(prop, "true") == 0) {
            err = updateNetworkList(socket_id, cur, 4 * sizeof(char *),
                                    updatedNetList);
            if (err == 0) {
                RLOGD("updatedNetworkList: %s", updatedNetList);
                cur[0] = updatedNetList;
            }
            updatedNetList += 64;
        }
#endif
        cur += 4;
        tmp += 30;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, responses,
            (count - unwantedPlmnCount) * 4 * sizeof(char *));
    at_response_free(p_response);
    free(startTmp);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
    free(startTmp);
}

static void requestResetRadio(int channelID, void *data, size_t datalen,
                                  RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int onOff, err;
    RIL_RadioState currentState;
    ATResponse *p_response = NULL;

    err = at_send_command(s_ATChannels[channelID], "AT+CFUN=0", &p_response);
    if (err < 0 || p_response->success == 0) goto error;
    AT_RESPONSE_FREE(p_response);

    setRadioState(channelID, RADIO_STATE_OFF);

    err = at_send_command(s_ATChannels[channelID], "AT+CFUN=1", &p_response);
    if (err < 0 || p_response->success == 0) {
        /* Some stacks return an error when there is no SIM,
         * but they really turn the RF portion on
         * So, if we get an error, let's check to see if it turned on anyway
         */
        if (isRadioOn(channelID) != 1) goto error;
    }

    setRadioState(channelID, RADIO_STATE_SIM_NOT_READY);

    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    return;

error:
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    return;
}

static int requestSetLTEPreferredNetType(int channelID, void *data,
                                            size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err, type = 0;
    char cmd[AT_COMMAND_LEN] = {0};
    char prop[PROPERTY_VALUE_MAX];
    ATResponse *p_response = NULL;
    RIL_Errno errType = RIL_E_GENERIC_FAILURE;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    property_get(ENGTEST_ENABLE_PROP, prop, "false");

    pthread_mutex_lock(&s_workModeMutex);
    if (0 == strcmp(prop, "true")) {  // request by engineer mode
        switch (((int *)data)[0]) {
            case NT_TD_LTE:
                type = TD_LTE;
                break;
            case NT_LTE_FDD:
                type = LTE_FDD;
                break;
            case NT_LTE_FDD_TD_LTE:
                type = TD_LTE_AND_LTE_FDD;
                break;
            case NT_LTE_FDD_WCDMA_GSM:
                type = LTE_FDD_AND_W_AND_GSM_CSFB;
                break;
            case NT_TD_LTE_WCDMA_GSM:
                type = TD_LTE_AND_W_AND_GSM_CSFB;
                break;
            case NT_LTE_FDD_TD_LTE_WCDMA_GSM:
                type = TD_LTE_AND_LTE_FDD_AND_W_AND_GSM_CSFB;
                break;
            case NT_TD_LTE_TDSCDMA_GSM:
                type = TD_LTE_AND_TD_AND_GSM_CSFB;
                break;
            case NT_LTE_FDD_TD_LTE_TDSCDMA_GSM:
                type = TD_LTE_AND_LTE_FDD_AND_TD_AND_GSM_CSFB;
                break;
            case NT_LTE_FDD_TD_LTE_WCDMA_TDSCDMA_GSM:
                type = TD_LTE_AND_LTE_FDD_AND_W_AND_TD_AND_GSM_CSFB;
                break;
            case NT_GSM: {
                if (s_modemConfig != LWG_WG) {
                    type = PRIMARY_GSM_ONLY;
                } else {
                    if (socket_id == s_multiModeSim) {
                        type = PRIMARY_GSM_ONLY;
                    } else {
                        type = GSM_ONLY;
                    }
                }
                break;
            }
            case NT_WCDMA: {
                if (s_modemConfig == LWG_WG) {
                    if (socket_id == s_multiModeSim) {
                        type = PRIMARY_WCDMA_ONLY;
                    } else {
                        type = WCDMA_ONLY;
                    }
                } else {
                    type = WCDMA_ONLY;
                }
                break;
            }
            case NT_TDSCDMA: {
                if (s_modemConfig == LWG_WG) {
                    type = 0;
                } else {
                    type = TD_ONLY;
                }
                break;
            }
            case NT_TDSCDMA_GSM: {
                if (s_modemConfig == LWG_WG) {
                    type = 0;
                } else {
                    type = TD_AND_GSM;
                }
                break;
            }
            case NT_WCDMA_GSM: {
                if (s_modemConfig == LWG_WG) {
                    if (socket_id == s_multiModeSim) {
                        type = PRIMARY_WCDMA_AND_GSM;
                    } else {
                        type = TD_AND_WCDMA;
                    }
                } else {
                    type = WCDMA_AND_GSM;
                }
                break;
            }
            case NT_WCDMA_TDSCDMA_GSM: {
                if (s_modemConfig == LWG_WG) {
                    if (socket_id == s_multiModeSim) {
                        type = PRIMARY_TD_AND_WCDMA;
                    } else {
                        type = TD_AND_WCDMA;
                    }
                } else {
                    type = TD_AND_WCDMA;
                }
                break;
            }
            default:
                break;
        }
    } else {  // request by FWK
        switch (((int *)data)[0]) {
            case NETWORK_MODE_LTE_GSM_WCDMA:
                type = getMultiMode();
                break;
            case NETWORK_MODE_WCDMA_PREF: {
                if (s_modemConfig == LWG_WG) {
                    if (socket_id == s_multiModeSim) {
                        type = PRIMARY_TD_AND_WCDMA;
                    } else {
                        type = TD_AND_WCDMA;
                    }
                } else {
                    int mode = getMultiMode();
                    if (mode == TD_LTE_AND_LTE_FDD_AND_W_AND_TD_AND_GSM_CSFB) {
                        type = TD_AND_WCDMA;
                    } else if (mode == TD_LTE_AND_LTE_FDD_AND_W_AND_GSM_CSFB) {
                        type = WCDMA_AND_GSM;
                    } else if (mode == TD_LTE_AND_TD_AND_GSM_CSFB) {
                        type = TD_AND_GSM;
                    }
                }
                break;
            }
            case NETWORK_MODE_GSM_ONLY:
                if (s_modemConfig != LWG_WG) {
                    type = PRIMARY_GSM_ONLY;
                } else {
                    if (socket_id == s_multiModeSim) {
                        type = PRIMARY_GSM_ONLY;
                    } else {
                        type = GSM_ONLY;
                    }
                }
                break;
            case NETWORK_MODE_LTE_ONLY: {
                int mode = getMultiMode();
                if (mode == TD_LTE_AND_LTE_FDD_AND_W_AND_TD_AND_GSM_CSFB ||
                    mode == TD_LTE_AND_LTE_FDD_AND_W_AND_GSM_CSFB) {
                    type = TD_LTE_AND_LTE_FDD;
                } else if (mode == TD_LTE_AND_TD_AND_GSM_CSFB) {
                    type = TD_LTE;
                }
                break;
            }
            case NETWORK_MODE_WCDMA_ONLY:
                if (s_modemConfig == LWG_WG) {
                    if (socket_id == s_multiModeSim) {
                        type = PRIMARY_WCDMA_ONLY;
                    } else {
                        type = WCDMA_ONLY;
                    }
                } else {
                    type = WCDMA_ONLY;
                }
                break;
            default:
                break;
        }
    }

    if (0 == type) {
        RLOGE("set preferred network failed, type incorrect: %d",
              ((int *)data)[0]);
        errType = RIL_E_GENERIC_FAILURE;
        goto done;
    }

    int workMode;
    char numToStr[ARRAY_SIZE];

    workMode = s_workMode[socket_id];
    if (s_modemConfig == LWG_G || s_modemConfig == W_G) {
        if (workMode == NONE || workMode == GSM_ONLY) {
            RLOGD("SetLTEPreferredNetType: not data card");
            errType = RIL_E_SUCCESS;
            goto done;
        }
    }
    if (type == workMode) {
        RLOGD("SetLTEPreferredNetType: has send the request before");
        errType = RIL_E_SUCCESS;
        goto done;
    }

#if defined (ANDROID_MULTI_SIM)
    if (socket_id == RIL_SOCKET_1) {
        snprintf(cmd, sizeof(cmd), "AT+SPTESTMODE=%d,%d", type,
                 s_workMode[RIL_SOCKET_2]);
    } else if (socket_id == RIL_SOCKET_2) {
        snprintf(cmd, sizeof(cmd), "AT+SPTESTMODE=%d,%d",
                 s_workMode[RIL_SOCKET_1], type);
    }
#else
    snprintf(cmd, sizeof(cmd), "AT+SPTESTMODE=%d,10", type);
#endif

    snprintf(numToStr, sizeof(numToStr), "%d", type);
    setProperty(socket_id, MODEM_WORKMODE_PROP, numToStr);
    s_workMode[socket_id] = type;

    const char *respCmd = "+SPTESTMODE:";
    /* timeout is in seconds
     * Due to AT+SPTESTMODE's response maybe be later than URC response,
     * so addAsyncCmdList before at_send_command
     */
    addAsyncCmdList(socket_id, t, respCmd, NULL, 120);
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (err < 0 || p_response->success == 0) {
        errType = RIL_E_GENERIC_FAILURE;
        removeAsyncCmdList(t, respCmd);
        goto done;
    }

    at_response_free(p_response);
    pthread_mutex_unlock(&s_workModeMutex);
    return 0;

done:
    pthread_mutex_unlock(&s_workModeMutex);
    at_response_free(p_response);
    RIL_onRequestComplete(t, errType, NULL, 0);
    return -1;
}

static void requestSetPreferredNetType(int channelID, void *data,
                                            size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err, type = 0;
    int workmode = 0;
    char cmd[AT_COMMAND_LEN] = {0};
    ATResponse *p_response = NULL;

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    /* AT^SYSCONFIG=<mode>,<acqorder>,<roam>,<srvdomain>
     * mode: 2:Auto 13:GSM ONLY 14:WCDMA_ONLY 15:TDSCDMA ONLY
     * acqorder: 3 -- no change
     * roam: 2 -- no change
     * srvdomain: 4 -- no change
     */
    RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED,
                              NULL, 0, socket_id);
    // transfer rilconstant to at

    pthread_mutex_lock(&s_workModeMutex);
    switch (((int *)data)[0]) {
        case NETWORK_MODE_WCDMA_PREF:
            type = 2;
            workmode = WCDMA_AND_GSM;
            break;
        case NETWORK_MODE_GSM_ONLY:
            type = 13;
            workmode = PRIMARY_GSM_ONLY;
            break;
        case NETWORK_MODE_WCDMA_ONLY:  // or TDSCDMA ONLY
            if (!strcmp(s_modem, "t")) {
                type = 15;
                workmode = TD_ONLY;
            } else if (!strcmp(s_modem, "w")) {
                type = 14;
                workmode = WCDMA_ONLY;
            }
            break;
        default:
            break;
    }
    if (0 == type) {
        RLOGE("set preferred network failed, type incorrect: %d",
              ((int *)data)[0]);
        goto error;
    }

    if (s_workMode[socket_id] == NONE || s_workMode[socket_id] == GSM_ONLY) {
        RLOGD("SetLTEPreferredNetType: not data card");
        goto done;
    }
    if (s_workMode[socket_id] == workmode) {
        RLOGD("SetPreferredNetType: has send the request before");
        goto done;
    }
#if defined (ANDROID_MULTI_SIM)
    if (socket_id == RIL_SOCKET_1) {
        if (!s_isLTE) {
            if (s_dataAllowed[socket_id] == 1) {
                at_send_command(s_ATChannels[channelID], "AT+SAUTOATT=1", NULL);
            } else {
                at_send_command(s_ATChannels[channelID], "AT+SAUTOATT=0", NULL);
            }
        }
        snprintf(cmd, sizeof(cmd), "AT^SYSCONFIG=%d,3,2,4", type);
        err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
        if (err < 0 || p_response->success == 0) {
            goto error;
        }
    } else if (socket_id == RIL_SOCKET_2) {
        if (!s_isLTE) {
            if (s_dataAllowed[socket_id] == 1) {
                at_send_command(s_ATChannels[channelID], "AT+SAUTOATT=1", NULL);
            } else {
                if (isSimPresent(0) == 0) {
                    at_send_command(s_ATChannels[channelID],
                            "AT+SAUTOATT=1", NULL);
                } else {
                    at_send_command(s_ATChannels[channelID],
                            "AT+SAUTOATT=0", NULL);
                }
            }
        }
        snprintf(cmd, sizeof(cmd), "AT^SYSCONFIG=%d,3,2,4", type);
        err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
        if (err < 0 || p_response->success == 0) {
            goto error;
        }
    }
#else
    snprintf(cmd, sizeof(cmd), "AT^SYSCONFIG=%d,3,2,4", type);
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }
#endif

    char numToStr[ARRAY_SIZE];
    snprintf(numToStr, sizeof(numToStr), "%d", workmode);
    setProperty(socket_id, MODEM_WORKMODE_PROP, numToStr);
    s_workMode[socket_id] = workmode;

done:
    pthread_mutex_unlock(&s_workModeMutex);
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    return;

error:
    pthread_mutex_unlock(&s_workModeMutex);
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

static void requestGetLTEPreferredNetType(int channelID,
        void *data, size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int type = -1;
    char prop[PROPERTY_VALUE_MAX];
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    property_get(ENGTEST_ENABLE_PROP, prop, "false");

    if (0 == strcmp(prop, "true")) {  // request by engineer mode
        switch (s_workMode[socket_id]) {
            case TD_LTE:
                type = NT_TD_LTE;
                break;
            case LTE_FDD:
                type = NT_LTE_FDD;
                break;
            case TD_LTE_AND_LTE_FDD:
                type = NT_LTE_FDD_TD_LTE;
                break;
            case LTE_FDD_AND_W_AND_GSM_CSFB:
                type = NT_LTE_FDD_WCDMA_GSM;
                break;
            case TD_LTE_AND_W_AND_GSM_CSFB:
                type = NT_TD_LTE_WCDMA_GSM;
                break;
            case TD_LTE_AND_LTE_FDD_AND_W_AND_GSM_CSFB:
                type = NT_LTE_FDD_TD_LTE_WCDMA_GSM;
                break;
            case TD_LTE_AND_TD_AND_GSM_CSFB:
                type = NT_TD_LTE_TDSCDMA_GSM;
                break;
            case TD_LTE_AND_LTE_FDD_AND_TD_AND_GSM_CSFB:
                type = NT_LTE_FDD_TD_LTE_TDSCDMA_GSM;
                break;
            case TD_LTE_AND_LTE_FDD_AND_W_AND_TD_AND_GSM_CSFB:
                type = NT_LTE_FDD_TD_LTE_WCDMA_TDSCDMA_GSM;
                break;
            case PRIMARY_GSM_ONLY:
            case GSM_ONLY:
                type = NT_GSM;
                break;
            case WCDMA_ONLY:
            case PRIMARY_WCDMA_ONLY:
                type = NT_WCDMA;
                break;
            case TD_ONLY:
                type = NT_TDSCDMA;
                break;
            case TD_AND_GSM:
                type = NT_TDSCDMA_GSM;
                break;
            case WCDMA_AND_GSM:
            case PRIMARY_WCDMA_AND_GSM:
                type = NT_WCDMA_GSM;
                break;
            case TD_AND_WCDMA:
            case PRIMARY_TD_AND_WCDMA:
                type = NT_WCDMA_TDSCDMA_GSM;
                break;
            default:
                break;
        }
    } else {
        switch (s_workMode[socket_id]) {
            case TD_LTE_AND_LTE_FDD_AND_W_AND_TD_AND_GSM_CSFB:
            case TD_LTE_AND_LTE_FDD_AND_W_AND_GSM_CSFB:
            case TD_LTE_AND_TD_AND_GSM_CSFB:
                type = NETWORK_MODE_LTE_GSM_WCDMA;
                break;
            case TD_AND_WCDMA:
            case PRIMARY_TD_AND_WCDMA:
            case WCDMA_AND_GSM:
            case PRIMARY_WCDMA_AND_GSM:
            case TD_AND_GSM:
                type = NETWORK_MODE_WCDMA_PREF;
                break;
            case PRIMARY_GSM_ONLY:
            case GSM_ONLY:
                type = NETWORK_MODE_GSM_ONLY;
                break;
            case TD_LTE:
            case LTE_FDD:
            case TD_LTE_AND_LTE_FDD:
                type = NETWORK_MODE_LTE_ONLY;
                break;
            case WCDMA_ONLY:
            case PRIMARY_WCDMA_ONLY:
                type = NETWORK_MODE_WCDMA_ONLY;
                break;
            default:
                break;
        }
    }
    if (type < 0) {
        RLOGD("GetLTEPreferredNetType: incorrect workmode %d",
              s_workMode[socket_id]);
        goto error;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &type, sizeof(type));
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

static void requestGetPreferredNetType(int channelID,
        void *data, size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int err, type;
    int response = 0;
    ATResponse *p_response = NULL;

    err = at_send_command_singleline(s_ATChannels[channelID], "AT^SYSCONFIG?",
                                     "^SYSCONFIG:", &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    char *line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err >= 0) {
        err = at_tok_nextint(&line, &response);
        // transfer at to rilconstant
        switch (response) {
            case 2:
                type = 0;  // NETWORK_MODE_WCDMA_PREF
                break;
            case 13:
                type = 1;  // NETWORK_MODE_GSM_ONLY
                break;
            case 14:
            case 15:
                type = 2;  // NETWORK_MODE_WCDMA_ONLY or TDSCDMA ONLY
                break;
            default:
                break;
        }
    }
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, &type, sizeof(type));
    return;

error:
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

static void requestNeighboaringCellIds(int channelID,
        void *data, size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int err = 0;
    int cellIdNumber;
    int current = 0;
    char *line;
    ATResponse *p_response = NULL;
    RIL_NeighboringCell *NeighboringCell;
    RIL_NeighboringCell **NeighboringCellList;

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+Q2GNCELL",
                                     "+Q2GNCELL:", &p_response);
    if (err != 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    if (at_tok_hasmore(&line)) {  // only in 2G
        char *sskip = NULL;
        int skip;

        err = at_tok_nextstr(&line, &sskip);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &skip);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &cellIdNumber);
        if (err < 0 || cellIdNumber == 0) {
            goto error;
        }
        NeighboringCellList = (RIL_NeighboringCell **)
                alloca(cellIdNumber * sizeof(RIL_NeighboringCell *));

        NeighboringCell = (RIL_NeighboringCell *)
                alloca(cellIdNumber * sizeof(RIL_NeighboringCell));

        for (current = 0; at_tok_hasmore(&line), current < cellIdNumber;
                current++) {
            err = at_tok_nextstr(&line, &(NeighboringCell[current].cid));
            if (err < 0) goto error;

            err = at_tok_nextint(&line, &(NeighboringCell[current].rssi));
            if (err < 0) goto error;

            RLOGD("Neighbor cell_id %s = %d",
                  NeighboringCell[current].cid, NeighboringCell[current].rssi);

            NeighboringCellList[current] = &NeighboringCell[current];
        }
    } else {
        AT_RESPONSE_FREE(p_response);
        err = at_send_command_singleline(s_ATChannels[channelID], "AT+Q3GNCELL",
                                         "+Q3GNCELL:", &p_response);
        if (err != 0 || p_response->success == 0) {
            goto error;
        }

        line = p_response->p_intermediates->line;

        err = at_tok_start(&line);
        if (err < 0) goto error;

        if (at_tok_hasmore(&line)) {  // only in 3G
            char *sskip = NULL;
            int skip;

            err = at_tok_nextstr(&line, &sskip);
            if (err < 0) goto error;

            err = at_tok_nextint(&line, &skip);
            if (err < 0) goto error;

            err = at_tok_nextint(&line, &cellIdNumber);
            if (err < 0 || cellIdNumber == 0) {
                goto error;
            }
            NeighboringCellList = (RIL_NeighboringCell **)
                    alloca(cellIdNumber * sizeof(RIL_NeighboringCell *));

            NeighboringCell = (RIL_NeighboringCell *)
                    alloca(cellIdNumber * sizeof(RIL_NeighboringCell));

            for (current = 0; at_tok_hasmore(&line), current < cellIdNumber;
                    current++) {
                err = at_tok_nextstr(&line, &(NeighboringCell[current].cid));
                if (err < 0) goto error;

                err = at_tok_nextint(&line, &(NeighboringCell[current].rssi));
                if (err < 0) goto error;

                RLOGD("Neighbor cell_id %s = %d", NeighboringCell[current].cid,
                      NeighboringCell[current].rssi);

                NeighboringCellList[current] = &NeighboringCell[current];
            }
        } else {
            AT_RESPONSE_FREE(p_response);
            err = at_send_command_singleline(s_ATChannels[channelID],
                    "AT+SPQ4GNCELL", "+SPQ4GNCELL:", &p_response);
            if (err != 0 || p_response->success == 0)
                goto error;

            line = p_response->p_intermediates->line;

            err = at_tok_start(&line);
            if (err < 0) goto error;
            if (at_tok_hasmore(&line)) {  // only in 4G
                char *sskip = NULL;
                int skip;

                err = at_tok_nextstr(&line, &sskip);
                if (err < 0) goto error;

                err = at_tok_nextint(&line, &skip);
                if (err < 0) goto error;

                err = at_tok_nextint(&line, &cellIdNumber);
                if (err < 0 || cellIdNumber == 0) {
                    goto error;
                }
                NeighboringCellList = (RIL_NeighboringCell **)
                        alloca(cellIdNumber * sizeof(RIL_NeighboringCell *));

                NeighboringCell = (RIL_NeighboringCell *)
                        alloca(cellIdNumber * sizeof(RIL_NeighboringCell));

                for (current = 0; at_tok_hasmore(&line),
                     current < cellIdNumber; current++) {
                    err = at_tok_nextstr(&line, &sskip);
                    if (err < 0) goto error;

                    err = at_tok_nextstr(&line,
                            &(NeighboringCell[current].cid));
                    if (err < 0) goto error;

                    err = at_tok_nextint(&line,
                            &(NeighboringCell[current].rssi));
                    if (err < 0) goto error;

                    RLOGD("Neighbor cell_id %s = %d",
                          NeighboringCell[current].cid,
                          NeighboringCell[current].rssi);

                    NeighboringCellList[current] = &NeighboringCell[current];
                }
            } else {
                goto error;
            }
        }
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NeighboringCellList,
                          cellIdNumber * sizeof(RIL_NeighboringCell *));
    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static void requestGetCellInfoList(int channelID, void *data,
                                       size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    const int count = 1;
    int err, i;
    int mcc, mnc, lac = 0;
    int sig2G = 0, sig3G = 0;
    int rsrp = 0, rsrq = 0, act = 0, cid = 0, pscPci = 0;
    int commas = 0, cellNum = 0, sskip = 0, registered = 0;
    int netType = 0, cellType = 0, biterr2G = 0, biterr3G = 0;
    char *line =  NULL, *p = NULL, *skip = NULL, *plmn = NULL;

    ATResponse *p_response = NULL;
    ATResponse *p_newResponse = NULL;
    RIL_CellInfo **response = NULL;

    if (!s_screenState) {
        RLOGD("GetCellInfo ScreenState %d", s_screenState);
        goto error;
    }
    response = (RIL_CellInfo **)malloc(count * sizeof(RIL_CellInfo *));
    if (response == NULL) {
        goto error;
    }
    memset(response, 0, count * sizeof(RIL_CellInfo *));

    for (i = 0; i < count; i++) {
        response[i] = malloc(sizeof(RIL_CellInfo));
        memset(response[i], 0, sizeof(RIL_CellInfo));
    }
    // for mcc & mnc
    err = at_send_command_singleline(s_ATChannels[channelID], "AT+COPS?",
                                     "+COPS:", &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }
    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &sskip);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &sskip);
    if (err < 0) goto error;

    err = at_tok_nextstr(&line, &plmn);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &netType);
    if (err < 0) goto error;

    mcc = atoi(plmn) / 100;
    mnc = atoi(plmn) - mcc * 100;

    if (netType == 7) {
        cellType = RIL_CELL_INFO_TYPE_LTE;
    } else if (netType == 1 || netType == 0) {
        cellType = RIL_CELL_INFO_TYPE_GSM;
    } else {
        cellType = RIL_CELL_INFO_TYPE_WCDMA;
    }

    AT_RESPONSE_FREE(p_response);

    // For net type, tac
    if (cellType == RIL_CELL_INFO_TYPE_LTE) {
        err = at_send_command_singleline(s_ATChannels[channelID], "AT+CEREG?",
                                         "+CEREG:", &p_response);
    } else {
        err = at_send_command_singleline(s_ATChannels[channelID], "AT+CREG?",
                                         "+CREG:", &p_response);
    }
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0) goto error;

    for (p = line; *p != '\0'; p++) {
        if (*p == ',') commas++;
    }
    if (commas > 3) {
        char *endptr;
        err = at_tok_nextint(&line, &sskip);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &registered);
        if (err < 0) goto error;

        err = at_tok_nextstr(&line, &skip);  // 2/3G:s_lac  4G:tac
        if (err < 0) goto error;

        lac = strtol(skip, &endptr, 16);
        err = at_tok_nextstr(&line, &skip);  // 2/3G:s_lac  4G:tac
        if (err < 0) goto error;

        cid = strtol(skip, &endptr, 16);
    }

    AT_RESPONSE_FREE(p_response);

    // For cellinfo
    if (cellType == RIL_CELL_INFO_TYPE_LTE) {
        err = at_send_command_singleline(s_ATChannels[channelID],
                "AT+SPQ4GNCELL", "+SPQ4GNCELL", &p_response);
    } else if (cellType == RIL_CELL_INFO_TYPE_GSM) {
        err = at_send_command_singleline(s_ATChannels[channelID],
                "AT+SPQ2GNCELL", "+SPQ2GNCELL", &p_response);
    } else {
        err = at_send_command_singleline(s_ATChannels[channelID],
                "AT+SPQ3GNCELL", "+SPQ3GNCELL", &p_response);
    }

    if (err < 0 || p_response->success == 0) {
        goto error;
    }
    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextstr(&line, &skip);
    if (err < 0) goto error;

    char *endptr;
    pscPci = strtol(skip, &endptr, 16);
    err = at_tok_nextint(&line, &sskip);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &cellNum);
    if (err < 0) goto error;

    AT_RESPONSE_FREE(p_response);

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

    err = at_tok_nextint(&line, &sig2G);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &biterr2G);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &sig3G);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &biterr3G);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &rsrq);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &rsrp);
    if (err < 0) goto error;

    uint64_t curTime = ril_nano_time();
    response[0]->registered = registered;
    response[0]->cellInfoType = cellType;
    response[0]->timeStampType = RIL_TIMESTAMP_TYPE_OEM_RIL;
    response[0]->timeStamp = curTime - 1000;
    if (cellType == RIL_CELL_INFO_TYPE_LTE) {  // 4G
        response[0]->CellInfo.lte.cellIdentityLte.mnc = mnc;
        response[0]->CellInfo.lte.cellIdentityLte.mcc = mcc;
        response[0]->CellInfo.lte.cellIdentityLte.ci  = cid;
        response[0]->CellInfo.lte.cellIdentityLte.pci = pscPci;
        response[0]->CellInfo.lte.cellIdentityLte.tac = lac;
        response[0]->CellInfo.lte.signalStrengthLte.cqi = 100;
        response[0]->CellInfo.lte.signalStrengthLte.rsrp = rsrp;
        response[0]->CellInfo.lte.signalStrengthLte.rsrq = rsrq;
        response[0]->CellInfo.lte.signalStrengthLte.rssnr = 100;
        response[0]->CellInfo.lte.signalStrengthLte.signalStrength = rsrp + 140;
        response[0]->CellInfo.lte.signalStrengthLte.timingAdvance  = 100;
    } else if (cellType == RIL_CELL_INFO_TYPE_GSM) {  // 2G
        response[0]->CellInfo.gsm.cellIdentityGsm.mcc = mcc;
        response[0]->CellInfo.gsm.cellIdentityGsm.mnc = mnc;
        response[0]->CellInfo.gsm.cellIdentityGsm.lac = lac;
        response[0]->CellInfo.gsm.cellIdentityGsm.cid = cid;
        response[0]->CellInfo.gsm.signalStrengthGsm.bitErrorRate = biterr2G;
        response[0]->CellInfo.gsm.signalStrengthGsm.signalStrength = sig2G;
    } else {  // 3G, Don't support CDMA
        response[0]->CellInfo.wcdma.cellIdentityWcdma.mcc = mcc;
        response[0]->CellInfo.wcdma.cellIdentityWcdma.mnc = mnc;
        response[0]->CellInfo.wcdma.cellIdentityWcdma.lac = lac;
        response[0]->CellInfo.wcdma.cellIdentityWcdma.cid = cid;
        response[0]->CellInfo.wcdma.cellIdentityWcdma.psc = pscPci;
        response[0]->CellInfo.wcdma.signalStrengthWcdma.bitErrorRate = biterr3G;
        response[0]->CellInfo.wcdma.signalStrengthWcdma.signalStrength = sig3G;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, (*response),
                          count * sizeof(RIL_CellInfo));
    at_response_free(p_response);
    at_response_free(p_newResponse);
    if (response != NULL) {
        for (i = 0; i < count; i++) {
            free(response[i]);
        }
        free(response);
    }
    return;

error:
    at_response_free(p_response);
    at_response_free(p_newResponse);
    if (response != NULL) {
        for (i = 0; i < count; i++) {
            free(response[i]);
        }
        free(response);
    }
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

static void requestShutdown(int channelID,
        void *data, size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int onOff;
    int err;

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    if (s_radioState[socket_id] != RADIO_STATE_OFF) {
        err = at_send_command(s_ATChannels[channelID], "AT+SFUN=5", NULL);
    }

    err = at_send_command(s_ATChannels[channelID], "AT+SFUN=3", NULL);
    setRadioState(channelID, RADIO_STATE_UNAVAILABLE);

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    return;
}

static void requestGetRadioCapability(int channelID, void *data,
                                           size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    RIL_RadioCapability *rc = (RIL_RadioCapability *)calloc(1,
            sizeof(RIL_RadioCapability));
    rc->version = RIL_RADIO_CAPABILITY_VERSION;
    rc->session = 0;
    rc->phase = RC_PHASE_CONFIGURED;
    rc->rat = getRadioFeatures(socket_id, 0);
    rc->status = RC_STATUS_NONE;
    if (socket_id == s_multiModeSim) {
        strncpy(rc->logicalModemUuid, "com.sprd.modem_multiMode",
                sizeof("com.sprd.modem_multiMode"));
    } else {
        strncpy(rc->logicalModemUuid, "com.sprd.modem_singleMode",
                sizeof("com.sprd.modem_singleMode"));
    }
    RLOGD("getRadioCapability rat = %d, logicalModemUuid = %s", rc->rat,
            rc->logicalModemUuid);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, rc, sizeof(RIL_RadioCapability));
    free(rc);
}

void setWorkMode() {
    int workMode = 0;
    int singleModeSim = 0;
    char numToStr[ARRAY_SIZE];
    char prop[PROPERTY_VALUE_MAX] = {0};
    int simId = 0;

#if (SIM_COUNT == 2)
    if (s_multiModeSim == RIL_SOCKET_1) {
        singleModeSim = RIL_SOCKET_2;
    } else if (s_multiModeSim == RIL_SOCKET_2) {
        singleModeSim = RIL_SOCKET_1;
    }
#endif
    pthread_mutex_lock(&s_workModeMutex);
    getProperty(s_multiModeSim, MODEM_WORKMODE_PROP, prop, "10");
    workMode = atoi(prop);
    s_workMode[singleModeSim] = workMode;

    getProperty(singleModeSim, MODEM_WORKMODE_PROP, prop, "10");
    workMode = atoi(prop);
    s_workMode[s_multiModeSim] = workMode;
    RLOGD("setRadioCapability multiMode is %d", s_workMode[s_multiModeSim]);

    for (simId = 0; simId < SIM_COUNT; simId++) {
        memset(numToStr, 0, sizeof(numToStr));
        snprintf(numToStr, sizeof(numToStr), "%d", s_workMode[simId]);
        setProperty(simId, MODEM_WORKMODE_PROP, numToStr);
    }
    pthread_mutex_unlock(&s_workModeMutex);
}

/*
 * return :  0: set radio capability failed;
 *           1: send AT SPTESTMODE,wait for async unsol response;
 *           2: no change,return success
 */
static int applySetLTERadioCapability(RIL_RadioCapability *rc, int channelID,
                                   RIL_Token t) {
    int err = -1, channel[SIM_COUNT];
    int simId = 0;
    char cmd[AT_COMMAND_LEN] = {0};
    char numToStr[ARRAY_SIZE];
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    if ((rc->rat & RAF_LTE) == RAF_LTE) {
        if (s_multiModeSim != socket_id) {
            s_multiModeSim = socket_id;
        } else {
            return 2;
        }
    } else {
#if (SIM_COUNT == 2)
        if (s_multiModeSim == socket_id) {
            s_multiModeSim = 1 - socket_id;
        } else {
            return 2;
        }
#endif
    }
    snprintf(numToStr, sizeof(numToStr), "%d", s_multiModeSim);
    property_set(PRIMARY_SIM_PROP, numToStr);
    RLOGD("applySetLTERadioCapability: multiModeSim %d", s_multiModeSim);
    if (SIM_COUNT <= 2) {
        snprintf(cmd, sizeof(cmd), "AT+SPSWITCHDATACARD=%d,1", s_multiModeSim);
        at_send_command(s_ATChannels[channelID], cmd, NULL);
    }

    setWorkMode();
#if (SIM_COUNT == 2)
    snprintf(cmd, sizeof(cmd), "AT+SPTESTMODE=%d,%d", s_workMode[RIL_SOCKET_1],
            s_workMode[RIL_SOCKET_2]);
#elif (SIM_COUNT == 1)
    snprintf(cmd, sizeof(cmd), "AT+SPTESTMODE=%d,10", s_workMode[RIL_SOCKET_1]);
#endif

    char *respCmd = "+SPTESTMODE:";
    RIL_RadioCapability *responseRc = (RIL_RadioCapability *)malloc(
            sizeof(RIL_RadioCapability));
    memcpy(responseRc, rc, sizeof(RIL_RadioCapability));
    addAsyncCmdList(socket_id, t, respCmd, responseRc, 120);
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (err < 0 || p_response->success == 0) {
        removeAsyncCmdList(t, respCmd);
        goto error;
    }
    return 1;

error:
    AT_RESPONSE_FREE(p_response);
    return 0;
}

static int applySetRadioCapability(RIL_RadioCapability *rc, int channelID) {
    int err, channel[SIM_COUNT];
    int simId = 0;
    char cmd[AT_COMMAND_LEN] = {0};
    char numToStr[ARRAY_SIZE];
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    if ((rc->rat & RAF_UMTS) == RAF_UMTS
            || (rc->rat & RAF_TD_SCDMA) == RAF_TD_SCDMA
            || (rc->rat & RAF_LTE) == RAF_LTE) {
        if (s_multiModeSim != socket_id) {
            s_multiModeSim = socket_id;
        } else {
            return 1;
        }
    } else {
#if (SIM_COUNT == 2)
        if (s_multiModeSim == socket_id) {
            s_multiModeSim = 1 - socket_id;
        } else {
            return 1;
        }
#endif
    }
    snprintf(numToStr, sizeof(numToStr), "%d", s_multiModeSim);
    property_set(PRIMARY_SIM_PROP, numToStr);
    RLOGD("applySetRadioCapability: multiModeSim %d", s_multiModeSim);

    memset(channel, -1, sizeof(channel));
    // shut down all sim cards radio
    for (simId = 0; simId < SIM_COUNT; simId++) {
        if (s_radioState[simId] == RADIO_STATE_OFF) {
            continue;
        }
        if (simId != (int)socket_id) {
            channel[simId] = getChannel(simId);
        } else {
            channel[simId] = channelID;
        }

        err = at_send_command(s_ATChannels[channel[simId]], "AT+SFUN=5",
                              &p_response);
        if (err < 0 || p_response->success == 0) {
            RLOGE("shut down radio failed, sim%d", simId);
            int i;
            for (i = 0; i < simId; i++) {
                if (i != (int)socket_id) {
                    putChannel(channel[i]);
                }
            }
            goto error;
        }
        AT_RESPONSE_FREE(p_response);
    }

    setWorkMode();
    buildWorkModeCmd(cmd, sizeof(cmd));

    for (simId = 0; simId < SIM_COUNT; simId++) {
        if (s_radioState[simId] == RADIO_STATE_OFF) {
            if (simId != (int)socket_id) {
                putChannel(channel[simId]);
            }
            continue;
        }

        at_send_command(s_ATChannels[channel[simId]], cmd, NULL);

#if defined (ANDROID_MULTI_SIM)
        if (s_presentSIMCount == 2) {
            if (simId == s_multiModeSim) {
                at_send_command(s_ATChannels[channel[simId]],
                        "AT+SAUTOATT=1", NULL);
            } else {
                at_send_command(s_ATChannels[channel[simId]],
                        "AT+SAUTOATT=0", NULL);
            }
        } else {
            at_send_command(s_ATChannels[channel[simId]], "AT+SAUTOATT=1",
                            NULL);
        }
#else
        at_send_command(s_ATChannels[channel[simId]], "AT+SAUTOATT=1", NULL);
#endif

        err = at_send_command(s_ATChannels[channel[simId]], "AT+SFUN=4",
                              &p_response);
        if (simId != (int)socket_id) {
            putChannel(channel[simId]);
        }
        if (err < 0 || p_response->success == 0) {
            goto error;
        }
        AT_RESPONSE_FREE(p_response);
    }

    return 1;

error:
    AT_RESPONSE_FREE(p_response);
    return 0;
}

static void requestSetRadioCapability(int channelID, void *data,
                                           size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    RIL_RadioCapability rc;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    memcpy(&rc, data, sizeof(RIL_RadioCapability));
    RLOGD("requestSetRadioCapability : %d, %d, %d, %d, %s, %d, rild:%d",
            rc.version, rc.session, rc.phase, rc.rat, rc.logicalModemUuid,
            rc.status, socket_id);

    RIL_RadioCapability *responseRc = (RIL_RadioCapability *)malloc(
            sizeof(RIL_RadioCapability));
    memcpy(responseRc, &rc, sizeof(RIL_RadioCapability));

    switch (rc.phase) {
        case RC_PHASE_START:
            s_sessionId[socket_id] = rc.session;
            RLOGD("requestSetRadioCapability RC_PHASE_START");
            responseRc->status = RC_STATUS_SUCCESS;
            RIL_onRequestComplete(t, RIL_E_SUCCESS, responseRc,
                    sizeof(RIL_RadioCapability));
            break;
        case RC_PHASE_FINISH:
            RLOGD("requestSetRadioCapability RC_PHASE_FINISH");
            s_sessionId[socket_id] = 0;
            responseRc->phase = RC_PHASE_CONFIGURED;
            responseRc->status = RC_STATUS_SUCCESS;
            RIL_onRequestComplete(t, RIL_E_SUCCESS, responseRc,
                    sizeof(RIL_RadioCapability));
            break;
        case RC_PHASE_APPLY: {
            int simId = 0;
            int ret = -1;
            s_sessionId[socket_id] = rc.session;
            responseRc->status = RC_STATUS_SUCCESS;
            s_requestSetRC[socket_id] = 1;
            for (simId = 0; simId < SIM_COUNT; simId++) {
                if (s_requestSetRC[simId] != 1) {
                    RIL_onRequestComplete(t, RIL_E_SUCCESS, responseRc,
                            sizeof(RIL_RadioCapability));
                    goto exit;
                }
            }
            for (simId = 0; simId < SIM_COUNT; simId++) {
                s_requestSetRC[simId] = 0;
            }
            if (s_isLTE) {
#if (SIM_COUNT == 2)
                pthread_mutex_lock(&s_radioPowerMutex[RIL_SOCKET_1]);
                pthread_mutex_lock(&s_radioPowerMutex[RIL_SOCKET_2]);
#endif
                ret = applySetLTERadioCapability(responseRc, channelID, t);
                if (ret <= 0) {
#if (SIM_COUNT == 2)
                    pthread_mutex_unlock(&s_radioPowerMutex[RIL_SOCKET_1]);
                    pthread_mutex_unlock(&s_radioPowerMutex[RIL_SOCKET_2]);
#endif
                    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, responseRc,
                            sizeof(RIL_RadioCapability));
                } else if (ret == 2) {
#if (SIM_COUNT == 2)
                    pthread_mutex_unlock(&s_radioPowerMutex[RIL_SOCKET_1]);
                    pthread_mutex_unlock(&s_radioPowerMutex[RIL_SOCKET_2]);
#endif
                    RIL_onRequestComplete(t, RIL_E_SUCCESS, responseRc,
                                          sizeof(RIL_RadioCapability));
                    sendUnsolRadioCapability();
                }
            } else {
#if (SIM_COUNT == 2)
                pthread_mutex_lock(&s_radioPowerMutex[RIL_SOCKET_1]);
                pthread_mutex_lock(&s_radioPowerMutex[RIL_SOCKET_2]);
#endif
                ret = applySetRadioCapability(responseRc, channelID);
#if (SIM_COUNT == 2)
                pthread_mutex_unlock(&s_radioPowerMutex[RIL_SOCKET_1]);
                pthread_mutex_unlock(&s_radioPowerMutex[RIL_SOCKET_2]);
#endif
                if (ret > 0) {
                    RIL_onRequestComplete(t, RIL_E_SUCCESS, responseRc,
                                          sizeof(RIL_RadioCapability));
                    sendUnsolRadioCapability();
                } else {
                    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, responseRc,
                                    sizeof(RIL_RadioCapability));
                }
            }
            break;
        }
        default:
            s_sessionId[socket_id] = rc.session;
            responseRc->status = RC_STATUS_FAIL;
            RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            break;
    }

exit:
    free(responseRc);
}

int processNetworkRequests(int request, void *data, size_t datalen,
                              RIL_Token t, int channelID) {
    int err;
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    switch (request) {
        case RIL_REQUEST_SIGNAL_STRENGTH: {
            if (s_isLTE) {
                requestSignalStrengthLTE(channelID, data, datalen, t);
            } else {
                requestSignalStrength(channelID, data, datalen, t);
            }
            break;
        }
        case RIL_REQUEST_VOICE_REGISTRATION_STATE:
        case RIL_REQUEST_DATA_REGISTRATION_STATE:
        case RIL_REQUEST_IMS_REGISTRATION_STATE:
            requestRegistrationState(channelID, request, data, datalen, t);
            break;
        case RIL_REQUEST_OPERATOR:
            requestOperator(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_RADIO_POWER: {
            pthread_mutex_lock(&s_radioPowerMutex[socket_id]);
            requestRadioPower(channelID, data, datalen, t);
            pthread_mutex_unlock(&s_radioPowerMutex[socket_id]);
            break;
        }
        case RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE:
            requestQueryNetworkSelectionMode(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC: {
            p_response = NULL;
            err = at_send_command(s_ATChannels[channelID], "AT+COPS=0",
                                  &p_response);
            if (err >= 0 && p_response->success) {
                AT_RESPONSE_FREE(p_response);
                err = at_send_command(s_ATChannels[channelID], "AT+CGAUTO=1",
                                      &p_response);
            }
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL:
            requestNetworkRegistration(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_QUERY_AVAILABLE_NETWORKS: {
            cleanUpAllConnections();
            requestNetworkList(channelID, data, datalen, t);
            activeAllConnections();
            break;
        }
        case RIL_REQUEST_RESET_RADIO:
            requestResetRadio(channelID, data, datalen, t);
            break;
        // case RIL_REQUEST_SET_BAND_MODE:
        //    break;
        // case RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE:
        //    break;
        case RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE: {
            if (s_isLTE) {
#if (SIM_COUNT == 2)
                pthread_mutex_lock(&s_radioPowerMutex[RIL_SOCKET_1]);
                pthread_mutex_lock(&s_radioPowerMutex[RIL_SOCKET_2]);
#endif
                err = requestSetLTEPreferredNetType(channelID, data, datalen, t);
                if (err < 0) {
#if (SIM_COUNT == 2)
                pthread_mutex_unlock(&s_radioPowerMutex[RIL_SOCKET_1]);
                pthread_mutex_unlock(&s_radioPowerMutex[RIL_SOCKET_2]);
#endif
                }
            } else {
                requestSetPreferredNetType(channelID, data, datalen, t);
            }
            break;
        }
        case RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE: {
            if (s_isLTE) {
                requestGetLTEPreferredNetType(channelID, data, datalen, t);
            } else {
                requestGetPreferredNetType(channelID, data, datalen, t);
            }
            break;
        }
        case RIL_REQUEST_GET_NEIGHBORING_CELL_IDS:
            requestNeighboaringCellIds(channelID, data, datalen, t);
            break;
        // case RIL_REQUEST_SET_LOCATION_UPDATES:
        //    break;
        // case RIL_REQUEST_VOICE_RADIO_TECH:
        //    break;
        case RIL_REQUEST_GET_CELL_INFO_LIST:
            requestGetCellInfoList(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_SET_UNSOL_CELL_INFO_LIST_RATE:
            RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            break;
        case RIL_REQUEST_SHUTDOWN:
            requestShutdown(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_GET_RADIO_CAPABILITY:
            requestGetRadioCapability(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_SET_RADIO_CAPABILITY: {
            char prop[PROPERTY_VALUE_MAX];
            property_get(FIXED_SLOT_PROP, prop, "false");
            if (strcmp(prop, "true") == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                requestSetRadioCapability(channelID, data, datalen, t);
            }
            break;
        }
        case RIL_REQUEST_GET_IMS_BEARER_STATE:
            RIL_onRequestComplete(t, RIL_E_SUCCESS,
                    (void *)&s_imsBearerEstablished[socket_id], sizeof(int));
            break;
        case RIL_EXT_REQUEST_STOP_QUERY_NETWORK: {
            int err;
            p_response = NULL;
            err = at_send_command(s_ATChannels[channelID], "AT+SAC",
                                  &p_response);
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

static void radioPowerOnTimeout(void *param) {
    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    int channelID = getChannel(socket_id);
    setRadioState(channelID, RADIO_STATE_SIM_NOT_READY);
    putChannel(channelID);
}

int processNetworkUnsolicited(RIL_SOCKET_ID socket_id, const char *s) {
    char *line = NULL;
    int err;

    if (strStartsWith(s, "+CSQ:")) {
        if (s_isLTE) {
            RLOGD("for +CSQ, current is lte ril,do nothing");
            goto out;
        }

        RIL_SignalStrength_v6 responseV6;
        char *tmp;
        char newLine[AT_COMMAND_LEN];

        line = strdup(s);
        tmp = line;

        err = csq_unsol_rsp(tmp, socket_id, newLine);
        if (err == 0) {
            RIL_SIGNALSTRENGTH_INIT(responseV6);

            tmp = newLine;
            at_tok_start(&tmp);

            err = at_tok_nextint(&tmp,
                    &(responseV6.GW_SignalStrength.signalStrength));
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp,
                    &(responseV6.GW_SignalStrength.bitErrorRate));
            if (err < 0) goto out;

            RIL_onUnsolicitedResponse(RIL_UNSOL_SIGNAL_STRENGTH, &responseV6,
                                      sizeof(RIL_SignalStrength_v6), socket_id);
        }
    } else if (strStartsWith(s, "+CESQ:")) {
        if (!strcmp(s_modem, "t")) {
            RLOGD("for +CESQ, current is td ril, do nothing");
            goto out;
        }

        RIL_SignalStrength_v6 response_v6;
        char *tmp;
        int skip;
        int response[6] = {-1, -1, -1, -1, -1, -1};
        char newLine[AT_COMMAND_LEN];

        line = strdup(s);
        tmp = line;

        err = cesq_unsol_rsp(tmp, socket_id, newLine);
        if (err == 0) {
            RIL_SIGNALSTRENGTH_INIT(response_v6);

            tmp = newLine;
            at_tok_start(&tmp);

            err = at_tok_nextint(&tmp, &response[0]);
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &response[1]);
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &response[2]);
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &skip);
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &skip);
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &response[5]);
            if (err < 0) goto out;

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
        }
    } else if (strStartsWith(s, "+CREG:") ||
                strStartsWith(s, "+CGREG:")) {
        RIL_onUnsolicitedResponse(
                RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED,
                NULL, 0, socket_id);
        if (s_radioOnError[socket_id] && strStartsWith(s, "+CREG:") &&
            s_radioState[socket_id] == RADIO_STATE_OFF) {
            RLOGD("Radio is on, setRadioState now.");
            s_radioOnError[socket_id] = false;
            RIL_requestTimedCallback(radioPowerOnTimeout,
                                     (void *)&s_socketId[socket_id], NULL);
        }
    } else if (strStartsWith(s, "+CEREG:")) {
        char *p, *tmp;
        int lteState;
        int commas = 0;
        int netType = -1;
        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        for (p = tmp; *p != '\0'; p++) {
            if (*p == ',') commas++;
        }
        err = at_tok_nextint(&tmp, &lteState);
        if (err < 0) goto out;

        if (s_isLTE) {
            if (commas == 0 && lteState == 0) {
                s_in4G[socket_id] = 0;
                if (s_PSRegState[socket_id] == STATE_IN_SERVICE) {
                    s_LTEDetached[socket_id] = true;
                }
            }
        }

        if (lteState == 1 || lteState == 5) {
            if (commas >= 3) {
                skipNextComma(&tmp);
                skipNextComma(&tmp);
                err = at_tok_nextint(&tmp, &netType);
                if (err < 0) goto out;
            }
            is4G(netType, -1, socket_id);
            RLOGD("netType is %d", netType);
            pthread_mutex_lock(&s_LTEAttachMutex[socket_id]);
            if (s_PSRegState[socket_id] == STATE_OUT_OF_SERVICE) {
                s_PSRegState[socket_id] = STATE_IN_SERVICE;
            }
            pthread_mutex_unlock(&s_LTEAttachMutex[socket_id]);
            RLOGD("s_PSRegState is IN SERVICE");
        } else {
            pthread_mutex_lock(&s_LTEAttachMutex[socket_id]);
            if (s_PSRegState[socket_id] == STATE_IN_SERVICE) {
                s_PSRegState[socket_id] = STATE_OUT_OF_SERVICE;
            }
            pthread_mutex_unlock(&s_LTEAttachMutex[socket_id]);
            RLOGD("s_PSRegState is OUT OF SERVICE.");
        }
        RIL_onUnsolicitedResponse(
                RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED,
                NULL, 0, socket_id);
    } else if (strStartsWith(s, "+CIREGU:")) {
        int response;
        int index = 0;
        char *tmp;
        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);
        err = at_tok_nextint(&tmp, &response);
        if (err < 0) {
            RLOGD("%s fail", s);
            goto out;
        }
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_IMS_NETWORK_STATE_CHANGED,
                                  &response, sizeof(response), socket_id);
        RIL_onUnsolicitedResponse(RIL_UNSOL_IMS_NETWORK_STATE_CHANGED,
                                  &response, sizeof(response), socket_id);
    } else if (strStartsWith(s, "^CONN:")) {
        int cid;
        int type;
        int active;
        int index = 0;
        char *tmp;
        line = strdup(s);
        tmp = line;

        at_tok_start(&tmp);
        err = at_tok_nextint(&tmp, &cid);
        if (err < 0) {
            RLOGD("get cid fail");
            goto out;
        }
        err = at_tok_nextint(&tmp, &type);
        if (err < 0) {
            RLOGD("get type fail");
            goto out;
        }
        err = at_tok_nextint(&tmp, &active);
        if (err < 0) {
            RLOGD("get active fail");
            goto out;
        }

        if (cid == 11) {
            s_imsBearerEstablished[socket_id] = active;
            RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_IMS_BEARER_ESTABLISTED,
                    (void *)&s_imsBearerEstablished[socket_id], sizeof(int),
                    socket_id);
        }
    } else if (strStartsWith(s, "+SPPCI:")) {
        char *tmp;
        int cid;
        char phy_cellid[ARRAY_SIZE];

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);
        err = at_tok_nexthexint(&tmp, &cid);
        if (err < 0) {
            RLOGD("get physicel cell id fail");
            goto out;
        }
        snprintf(phy_cellid, sizeof(phy_cellid), "%d", cid);
        pthread_mutex_lock(&s_physicalCellidMutex);
        setProperty(socket_id, PHYSICAL_CELLID_PROP, phy_cellid);
        pthread_mutex_unlock(&s_physicalCellidMutex);
    } else if (strStartsWith(s, "+SPNWNAME:")) {
        /* NITZ operator name */
        char *tmp = NULL;
        char *mcc = NULL;
        char *mnc = NULL;
        char *fullName = NULL;
        char *shortName = NULL;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextstr(&tmp, &mcc);
        if (err < 0) goto out;

        err = at_tok_nextstr(&tmp, &mnc);
        if (err < 0) goto out;

        err = at_tok_nextstr(&tmp, &fullName);
        if (err < 0) goto out;

        err = at_tok_nextstr(&tmp, &shortName);
        if (err < 0) goto out;

        char nitzOperatorInfo[PROPERTY_VALUE_MAX] = {0};
        char propName[ARRAY_SIZE] = {0};

        if (socket_id == RIL_SOCKET_1) {
            snprintf(propName, sizeof(propName), "%s", NITZ_OPERATOR_PROP);
        } else if (socket_id > RIL_SOCKET_1) {
            snprintf(propName, sizeof(propName), "%s%d", NITZ_OPERATOR_PROP,
                      socket_id);
        }

        snprintf(nitzOperatorInfo, sizeof(nitzOperatorInfo), "%s,%s,%s%s",
                  fullName, shortName, mcc, mnc);

        property_set(propName, nitzOperatorInfo);

        if (s_radioState[socket_id] == RADIO_STATE_SIM_READY) {
            RIL_onUnsolicitedResponse(
                    RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED,
                    NULL, 0, socket_id);
        }
    } else if (strStartsWith(s, "+SPTESTMODE:")) {
        int response;
        char *tmp = NULL;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextint(&tmp, &response);
        if (err < 0) goto out;

        const char *cmd = "+SPTESTMODE:";
        checkAndCompleteRequest(socket_id, cmd, (void *)(&response));
    } else {
        return 0;
    }

out:
    free(line);
    return 1;
}

void dispatchSPTESTMODE(RIL_Token t, void *data, void *resp) {
    int status = ((int *)resp)[0];

#if (SIM_COUNT == 2)
    pthread_mutex_unlock(&s_radioPowerMutex[RIL_SOCKET_1]);
    pthread_mutex_unlock(&s_radioPowerMutex[RIL_SOCKET_2]);
#endif
    if (data != NULL) {
        RIL_RadioCapability *rc = (RIL_RadioCapability *)data;
        if (status == 1) {
            RIL_onRequestComplete(t, RIL_E_SUCCESS, rc,
                    sizeof(RIL_RadioCapability));
            /* after applying radio capability success
             * send unsol radio capability.
             */
            sendUnsolRadioCapability();
        } else {
            RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, rc,
                    sizeof(RIL_RadioCapability));
        }
    } else {
        if (status == 1) {
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        } else {
            RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        }
    }
}


/*
 * phoneserver used to process these AT Commands or its response
 *    # AT+CSQ execute command response process
 *    # AT+CESQ execute command response process
 *    # +CSQ: unsol response process
 *    # +CESQ: unsol response process
 *
 * since phoneserver has been removed, its function should be realized in RIL,
 * so when used AT+CSQ, AT+CESQ, AT+CGDATA=, +CSQ: and +CESQ:,
 * please make sure to call the corresponding process functions.
 */

static int least_squares(int y[]) {
    int i = 0;
    int x[SIG_POOL_SIZE] = {0};
    int sum_x = 0, sum_y = 0, sum_xy = 0, square = 0;
    float a = 0.0, b = 0.0, value = 0.0;

    for (i = 0; i < SIG_POOL_SIZE; ++i) {
        x[i] = i;
        sum_x += x[i];
        sum_y += y[i];
        sum_xy += x[i] * y[i];
        square += x[i] * x[i];
    }
    a = ((float)(sum_xy * SIG_POOL_SIZE - sum_x * sum_y))
            / (square * SIG_POOL_SIZE - sum_x * sum_x);
    b = ((float)sum_y) / SIG_POOL_SIZE - a * sum_x / SIG_POOL_SIZE;
    value = a * x[SIG_POOL_SIZE - 1] + b;
    return (int)(value) + ((int)(10 * value) % 10 < 5 ? 0 : 1);
}

void *signal_process() {
    int sim_index = 0;
    int i = 0;
    int sample_rsrp_sim[SIM_COUNT][SIG_POOL_SIZE];
    int sample_rscp_sim[SIM_COUNT][SIG_POOL_SIZE];
    int sample_rxlev_sim[SIM_COUNT][SIG_POOL_SIZE];
    int sample_rssi_sim[SIM_COUNT][SIG_POOL_SIZE];
    int *rsrp_array = NULL, *rscp_array = NULL, *rxlev_array, newSig;
    int upValue = -1, lowValue = -1;
    int rsrp_value, rscp_value, rxlev_value;
    // 3 means count 2G/3G/4G
    int nosigUpdate[SIM_COUNT], MAXSigCount = 3 * (SIG_POOL_SIZE - 1);

    memset(sample_rsrp_sim, 0, SIM_COUNT);
    memset(sample_rscp_sim, 0, SIM_COUNT);
    memset(sample_rxlev_sim, 0, SIM_COUNT);
    memset(sample_rssi_sim, 0, SIM_COUNT);

    if (!s_isLTE) {
        MAXSigCount = SIG_POOL_SIZE - 1;
    }

    while (1) {
        for (sim_index = 0; sim_index < SIM_COUNT; sim_index++) {
            // compute the rsrp(4G) rscp(3G) rxlev(2G) or rssi(CSQ)
            if (!s_isLTE) {
                rsrp_array = NULL;
                rscp_array = sample_rssi_sim[sim_index];
                rxlev_array = NULL;
                newSig = rssi[sim_index];
                upValue = 31;
                lowValue = 0;
            } else {
                rsrp_array = sample_rsrp_sim[sim_index];
                rscp_array = sample_rscp_sim[sim_index];
                rxlev_array = sample_rxlev_sim[sim_index];
                newSig = rscp[sim_index];
                upValue = 140;
                lowValue = 44;
            }
            nosigUpdate[sim_index] = 0;

            for (i = 0; i < SIG_POOL_SIZE - 1; ++i) {
                if (rsrp_array != NULL) {  // w/td mode no rsrp
                    if (rsrp_array[i] == rsrp_array[i + 1]) {
                        if (rsrp_array[i] == rsrp[sim_index]) {
                            nosigUpdate[sim_index]++;
                        } else if (rsrp_array[i] == 0 ||
                                   rsrp_array[i] < lowValue ||
                                   rsrp_array[i] > upValue) {
                            rsrp_array[i] = rsrp[sim_index];
                        }
                    } else {
                        rsrp_array[i] = rsrp_array[i + 1];
                    }
                }

                if (rscp_array != NULL) {
                    if (rscp_array[i] == rscp_array[i + 1]) {
                        if (rscp_array[i] == newSig) {
                            nosigUpdate[sim_index]++;
                        } else if (rscp_array[i] <= 0 || rscp_array[i] > 31) {
                            // the first unsolicitied
                            rscp_array[i] = newSig;
                        }
                    } else {
                        rscp_array[i] = rscp_array[i + 1];
                    }
                }

                if (rxlev_array != NULL) {  // w/td mode no rxlev
                    if (rxlev_array[i] == rxlev_array[i + 1]) {
                        if (rxlev_array[i] == rxlev[sim_index]) {
                            nosigUpdate[sim_index]++;
                        } else if (rxlev_array[i] <= 0 ||
                                    rxlev_array[i] > 31) {
                            rxlev_array[i] = rxlev[sim_index];
                        }
                    } else {
                        rxlev_array[i] = rxlev_array[i + 1];
                    }
                }
            }

            if (nosigUpdate[sim_index] == MAXSigCount) {
                continue;
            }

            if (rsrp_array != NULL) {  // w/td mode no rsrp
                rsrp_array[SIG_POOL_SIZE - 1] = rsrp[sim_index];
                if (rsrp_array[SIG_POOL_SIZE - 1] <=
                    rsrp_array[SIG_POOL_SIZE - 2]) {  // signal go up
                    rsrp_value = rsrp[sim_index];
                } else {  // signal come down
                    if (rsrp_array[SIG_POOL_SIZE - 1] ==
                        rsrp_array[SIG_POOL_SIZE - 2] + 1) {
                        rsrp_value = rsrp[sim_index];
                    } else {
                        rsrp_value = least_squares(rsrp_array);
                        // if invalid, use current value
                        if (rsrp_value < lowValue || rsrp_value > upValue  ||
                            rsrp_value > rsrp[sim_index]) {
                            rsrp_value = rsrp[sim_index];
                        }
                        rsrp_array[SIG_POOL_SIZE - 1] = rsrp_value;
                    }
                }
            }

            if (rscp_array != NULL) {
                rscp_array[SIG_POOL_SIZE - 1] = newSig;
                if (rscp_array[SIG_POOL_SIZE - 1]
                        >= rscp_array[SIG_POOL_SIZE - 2]) {  // signal go up
                    rscp_value = newSig;
                } else {  // signal come down
                    if (rscp_array[SIG_POOL_SIZE - 1]
                            == rscp_array[SIG_POOL_SIZE - 2] - 1) {
                        rscp_value = newSig;
                    } else {
                        rscp_value = least_squares(rscp_array);
                        // if invalid, use current value
                        if (rscp_value < 0 || rscp_value > 31
                                || rscp_value < newSig) {
                            rscp_value = newSig;
                        }
                        rscp_array[SIG_POOL_SIZE - 1] = rscp_value;
                    }
                }
            }
            if (rxlev_array != NULL) {  // w/td mode no rxlev
                rxlev_array[SIG_POOL_SIZE - 1] = rxlev[sim_index];
                if (rxlev_array[SIG_POOL_SIZE - 1]
                        >= rxlev_array[SIG_POOL_SIZE - 2]) {  // signal go up
                    rxlev_value = rxlev[sim_index];
                } else {  // signal come down
                    if (rxlev_array[SIG_POOL_SIZE - 1]
                            == rxlev_array[SIG_POOL_SIZE - 2] - 1) {
                        rxlev_value = rxlev[sim_index];
                    } else {
                        rxlev_value = least_squares(rxlev_array);
                        // if invalid, use current value
                        if (rxlev_value < 0 || rxlev_value > 31
                                || rxlev_value < rxlev[sim_index]) {
                            rxlev_value = rxlev[sim_index];
                        }
                        rxlev_array[SIG_POOL_SIZE - 1] = rxlev_value;
                    }
                }
            }

            if (s_isLTE) {  // l/tl/lf
                RIL_SignalStrength_v6 response_v6;
                int response[6] = {-1, -1, -1, -1, -1, -1};
                RIL_SIGNALSTRENGTH_INIT(response_v6);

                response[0] = rxlev_value;
                response[1] = ber[sim_index];
                response[2] = rscp_value;
                response[5] = rsrp_value;

                if (response[0] != -1 && response[0] != 99) {
                    response_v6.GW_SignalStrength.signalStrength = response[0];
                }
                if (response[2] != -1 && response[2] != 255) {
                    response_v6.GW_SignalStrength.signalStrength = response[2];
                }
                if (response[5] != -1 && response[5] != 255 && response[5] != -255) {
                    response_v6.LTE_SignalStrength.rsrp = response[5];
                }
                RIL_onUnsolicitedResponse(RIL_UNSOL_SIGNAL_STRENGTH,
                        &response_v6, sizeof(RIL_SignalStrength_v6), sim_index);
            } else {  // w/t
                RIL_SignalStrength_v6 responseV6;
                responseV6.GW_SignalStrength.signalStrength = rscp_value;
                responseV6.GW_SignalStrength.bitErrorRate = ber[sim_index];
                RIL_onUnsolicitedResponse(RIL_UNSOL_SIGNAL_STRENGTH,
                        &responseV6, sizeof(RIL_SignalStrength_v6), sim_index);
            }
        }
        sleep(1);
    }
    return NULL;
}

/* for +CSQ: unsol response process */
int csq_unsol_rsp(char *line, RIL_SOCKET_ID socket_id, char *newLine) {
    int err;
    char *atInStr;

    atInStr = line;
    err = at_tok_flag_start(&atInStr, ':');
    if (err < 0) goto error;

    /*skip cause value */
    err = at_tok_nextint(&atInStr, &rssi[socket_id]);
    if (err < 0) goto error;

    err = at_tok_nextint(&atInStr, &berr[socket_id]);
    if (err < 0) goto error;

    if (rssi[socket_id] >= 100 && rssi[socket_id] < 103) {
        rssi[socket_id] = 0;
    } else if (rssi[socket_id] >= 103 && rssi[socket_id] < 165) {
        // add 1 for compensation
        rssi[socket_id] = ((rssi[socket_id] - 103) + 1) / 2;
    } else if (rssi[socket_id] >= 165 && rssi[socket_id] <= 191) {
        rssi[socket_id] = 31;
    } else if ((rssi[socket_id] > 31 && rssi[socket_id] < 100 ) ||
                rssi[socket_id] > 191) {
        rssi[socket_id] = 99;
    }
    if (berr[socket_id] > 99) {
        berr[socket_id] = 99;
    }

    if (s_psOpened[socket_id] == 1) {
        if (!s_isLTE) {
            s_psOpened[socket_id] = 0;
            if (newLine != NULL) {
                snprintf(newLine, AT_COMMAND_LEN, "+CSQ: %d,%d",
                         rssi[socket_id], berr[socket_id]);
                return AT_RESULT_OK;
            }
        }
    }

error:
    return AT_RESULT_NG;
}

/* for +CESQ: unsol response process */
int cesq_unsol_rsp(char *line, RIL_SOCKET_ID socket_id, char *newLine) {
    int err;
    char *atInStr;

    atInStr = line;
    err = at_tok_flag_start(&atInStr, ':');
    if (err < 0) goto error;

    err = at_tok_nextint(&atInStr, &rxlev[socket_id]);
    if (err < 0) goto error;

    if (rxlev[socket_id] <= 61) {
        rxlev[socket_id] = (rxlev[socket_id] + 2) / 2;
    } else if (rxlev[socket_id] > 61 && rxlev[socket_id] <= 63) {
        rxlev[socket_id] = 31;
    } else if (rxlev[socket_id] >= 100 && rxlev[socket_id] < 103) {
        rxlev[socket_id] = 0;
    } else if (rxlev[socket_id] >= 103 && rxlev[socket_id] < 165) {
        rxlev[socket_id] = ((rxlev[socket_id] - 103) + 1) / 2;
    } else if (rxlev[socket_id] >= 165 && rxlev[socket_id] <= 191) {
        rxlev[socket_id] = 31;
    }

    err = at_tok_nextint(&atInStr, &ber[socket_id]);
    if (err < 0) goto error;

    err = at_tok_nextint(&atInStr, &rscp[socket_id]);
    if (err < 0) goto error;

    if (rscp[socket_id] >= 100 && rscp[socket_id] < 103) {
        rscp[socket_id] = 0;
    } else if (rscp[socket_id] >= 103 && rscp[socket_id] < 165) {
        rscp[socket_id] = ((rscp[socket_id] - 103) + 1) / 2;
    } else if (rscp[socket_id] >= 165 && rscp[socket_id] <= 191) {
        rscp[socket_id] = 31;
    }

    err = at_tok_nextint(&atInStr, &ecno[socket_id]);
    if (err < 0) goto error;

    err = at_tok_nextint(&atInStr, &rsrq[socket_id]);
    if (err < 0) goto error;

    err = at_tok_nextint(&atInStr, &rsrp[socket_id]);
    if (err < 0) goto error;

    if (rsrp[socket_id] == 255) {
        rsrp[socket_id] = -255;
    } else {
        rsrp[socket_id] = 141 - rsrp[socket_id];
    }

    if (s_psOpened[socket_id] == 1) {
        if (s_isLTE) {
            s_psOpened[socket_id] = 0;
            snprintf(newLine, AT_COMMAND_LEN, "+CESQ: %d,%d,%d,%d,%d,%d",
                     rxlev[socket_id], ber[socket_id], rscp[socket_id],
                     ecno[socket_id], rsrq[socket_id], rsrp[socket_id]);
            return AT_RESULT_OK;
        }
    }

error:
    return AT_RESULT_NG;
}

/* for AT+CSQ execute command response process */
void csq_execute_cmd_rsp(ATResponse *p_response, ATResponse **p_newResponse) {
    int ret;
    char *input;
    int len;
    int rssi_3g = 0, rssi_2g = 0, ber;
    char rspStr[MAX_AT_RESPONSE];

    ATResponse *sp_response = at_response_new();

    input = p_response->p_intermediates->line;
    len = strlen(input);
    if (findInBuf(input, len, "+CSQ")) {
        /* get the 3g rssi value and then convert into 2g rssi */
        if (0 == at_tok_flag_start(&input, ':')) {
            if (0 == at_tok_nextint(&input, &rssi_3g)) {
                if (rssi_3g <= 31) {
                    rssi_2g = rssi_3g;
                } else if (rssi_3g >= 100 && rssi_3g < 103) {
                    rssi_2g = 0;
                } else if (rssi_3g >= 103 && rssi_3g < 165) {
                    rssi_2g = ((rssi_3g - 103) + 1) / 2;
                } else if (rssi_3g >= 165 && rssi_3g <= 191) {
                    rssi_2g = 31;
                } else {
                    rssi_2g = 99;
                }

                if (0 == at_tok_nextint(&input, &ber)) {
                    if (ber > 99) {
                        ber = 99;
                    }
                }
                snprintf(rspStr, sizeof(rspStr), "+CSQ:%d,%d", rssi_2g, ber);
                reWriteIntermediate(sp_response, rspStr);
                if (p_newResponse == NULL) {
                    at_response_free(sp_response);
                } else {
                    *p_newResponse = sp_response;
                }
            }
        }
    }
}

/* for AT+CESQ execute command response process */
void cesq_execute_cmd_rsp(ATResponse *p_response, ATResponse **p_newResponse) {
    int ret;
    char *line;
    int err, len;
    int rxlev = 0, ber = 0, rscp = 0, ecno = 0, rsrq = 0, rsrp = 0;
    char respStr[MAX_AT_RESPONSE];
    ATResponse *sp_response = at_response_new();

    line = p_response->p_intermediates->line;
    len = strlen(line);
    if (findInBuf(line, len, "+CESQ")) {
        err = at_tok_flag_start(&line, ':');
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &rxlev);
        if (err < 0) goto error;

        if (rxlev <= 61) {
            rxlev = (rxlev + 2) / 2;
        } else if (rxlev > 61 && rxlev <= 63) {
            rxlev = 31;
        } else if (rxlev >= 100 && rxlev < 103) {
            rxlev = 0;
        } else if (rxlev >= 103 && rxlev < 165) {
            rxlev = ((rxlev - 103) + 1) / 2;  // add 1 for compensation
        } else if (rxlev >= 165 && rxlev <= 191) {
            rxlev = 31;
        }

        err = at_tok_nextint(&line, &ber);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &rscp);
        if (err < 0) goto error;

        if (rscp >= 100 && rscp < 103) {
            rscp = 0;
        } else if (rscp >= 103 && rscp < 165) {
            rscp = ((rscp - 103) + 1) / 2;  // add 1 for compensation
        } else if (rscp >= 165 && rscp <= 191) {
            rscp = 31;
        }

        err = at_tok_nextint(&line, &ecno);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &rsrq);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &rsrp);
        if (err < 0) goto error;

        if (rsrp == 255) {
            rsrp = -255;
        } else {
            rsrp = 141 - rsrp;  // modified by bug#486220
        }
        snprintf(respStr, sizeof(respStr), "+CESQ: %d,%d,%d,%d,%d,%d",
                 rxlev, ber, rscp, ecno, rsrq, rsrp);
        reWriteIntermediate(sp_response, respStr);
        if (p_newResponse == NULL) {
            at_response_free(sp_response);
        } else {
            *p_newResponse = sp_response;
        }
    }

error:
    return;
}
