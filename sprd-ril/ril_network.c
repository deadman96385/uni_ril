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
#include "ril_async_cmd_handler.h"
#include "channel_controller.h"
#include "ril_call.h"
#include "ril_utils.h"
#include "time.h"

/* Save physical cellID for AGPS */
//#define PHYSICAL_CELLID_PROP    "gsm.cell.physical_cellid"
/* Save NITZ operator name string for UI to display right PLMN name */
#define NITZ_OPERATOR_PROP      "persist.vendor.radio.nitz"
#define FIXED_SLOT_PROP         "ro.vendor.radio.fixed_slot"
#define COPS_MODE_PROP          "persist.vendor.radio.copsmode"
/* set network type for engineer mode */
#define ENGTEST_ENABLE_PROP     "persist.vendor.radio.engtest.enable"
/* set the comb-register flag */
#define CEMODE_PROP             "persist.vendor.radio.cemode"

RIL_RegState s_PSRegStateDetail[SIM_COUNT] = {
        RIL_UNKNOWN
#if (SIM_COUNT >= 2)
        ,RIL_UNKNOWN
#if (SIM_COUNT >= 3)
        ,RIL_UNKNOWN
#if (SIM_COUNT >= 4)
        ,RIL_UNKNOWN
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
pthread_mutex_t s_operatorInfoListMutex[SIM_COUNT] = {
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
pthread_mutex_t s_operatorXmlInfoListMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t s_signalProcessMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t s_signalProcessCond = PTHREAD_COND_INITIALIZER;

int s_multiModeSim = 0;
int s_imsRegistered[SIM_COUNT];  // 0 == unregistered
int s_imsBearerEstablished[SIM_COUNT];
int s_in4G[SIM_COUNT];
int s_in2G[SIM_COUNT] = {0};
int s_workMode[SIM_COUNT] = {0};
int s_desiredRadioState[SIM_COUNT] = {0};
int s_requestSetRC[SIM_COUNT] = {0};
int s_sessionId[SIM_COUNT] = {0};
int s_radioAccessFamily[SIM_COUNT] = {0};
int s_presentSIMCount = 0;
static bool s_radioOnError[SIM_COUNT];  // 0 -- false, 1 -- true
static char s_nitzOperatorInfo[SIM_COUNT][ARRAY_SIZE] = {{0}, {0}};
OperatorInfoList s_operatorInfoList[SIM_COUNT];
OperatorInfoList s_operatorXmlInfoList;

int s_psOpened[SIM_COUNT] = {0};
int rxlev[SIM_COUNT], ber[SIM_COUNT], rscp[SIM_COUNT];
int ecno[SIM_COUNT], rsrq[SIM_COUNT], rsrp[SIM_COUNT];
int rssi[SIM_COUNT], berr[SIM_COUNT];

void setWorkMode();
void initWorkMode();

void onModemReset_Network() {
    RIL_SOCKET_ID socket_id  = 0;

    for (socket_id = RIL_SOCKET_1; socket_id < RIL_SOCKET_NUM; socket_id++) {
        s_PSRegStateDetail[socket_id] = RIL_UNKNOWN;
        s_PSRegState[socket_id] = STATE_OUT_OF_SERVICE;
        s_in4G[socket_id] = 0;
        s_in2G[socket_id] = 0;
        s_imsRegistered[socket_id] = 0;

        // signal process related
        s_psOpened[socket_id] = 0;
        rxlev[socket_id] = 0;
        ber[socket_id] = 0;
        rscp[socket_id] = 0;
        ecno[socket_id] = 0;
        rsrq[socket_id] = 0;
        rsrp[socket_id] = 0;
        rssi[socket_id] = 0;
        berr[socket_id] = 0;
    }
}

// for L+W product
bool isPrimaryCardWorkMode(int workMode) {
    if (workMode == GSM_ONLY || workMode == WCDMA_ONLY ||
        workMode == WCDMA_AND_GSM || workMode == TD_AND_WCDMA ||
        workMode == NONE) {
        return false;
    }
    return true;
}

void initPrimarySim() {
    char prop[PROPERTY_VALUE_MAX] = {0};
    char numToStr[ARRAY_SIZE] = {0};
    RIL_SOCKET_ID simId = RIL_SOCKET_1;

    property_get(PRIMARY_SIM_PROP, prop, "0");
    s_multiModeSim = atoi(prop);
    RLOGD("before initPrimarySim: s_multiModeSim = %d", s_multiModeSim);

    initWorkMode();

#if (SIM_COUNT == 2)
    property_get(MODEM_CONFIG_PROP, prop, "");
    if (s_modemConfig == LWG_G || s_modemConfig == W_G ||
        s_modemConfig == LG_G) {
        for (simId = RIL_SOCKET_1; simId < RIL_SOCKET_NUM; simId++) {
            if (s_workMode[simId] == 10 && s_multiModeSim == simId) {
                s_multiModeSim = 1 - simId;
                snprintf(numToStr, sizeof(numToStr), "%d", s_multiModeSim);
                property_set(PRIMARY_SIM_PROP, numToStr);
            }
        }
    } else if (s_modemConfig == LWG_WG) {
        for (simId = RIL_SOCKET_1; simId < RIL_SOCKET_NUM; simId++) {
            if (!isPrimaryCardWorkMode(s_workMode[simId]) && s_multiModeSim == simId) {
                s_multiModeSim = 1 - simId;
                snprintf(numToStr, sizeof(numToStr), "%d", s_multiModeSim);
                property_set(PRIMARY_SIM_PROP, numToStr);
            }
        }
    }
#endif
    RLOGD("after initPrimarySim : s_multiModeSim = %d", s_multiModeSim);
}

void setSimPresent(RIL_SOCKET_ID socket_id, int hasSim) {
    RLOGD("setSimPresent hasSim = %d", hasSim);
    pthread_mutex_lock(&s_simPresentMutex);
    s_isSimPresent[socket_id] = hasSim;
    pthread_mutex_unlock(&s_simPresentMutex);
}

int isSimPresent(RIL_SOCKET_ID socket_id) {
    int hasSim = 0;
    pthread_mutex_lock(&s_simPresentMutex);
    hasSim = s_isSimPresent[socket_id];
    pthread_mutex_unlock(&s_simPresentMutex);

    return hasSim;
}

void initSIMPresentState() {
    int simId = 0;
    for (simId = 0; simId < SIM_COUNT; simId++) {
        if (s_isSimPresent[simId] == SIM_UNKNOWN) {
            RLOGD("s_isSimPresent unknown  %d", simId);
            int channelID = getChannel(simId);
            getSIMStatus(false, channelID);
            putChannel(channelID);
        }
    }
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
        workMode = TD_LTE_AND_LTE_FDD_AND_W_AND_TD_AND_GSM;
    } else if (strstr(prop, "TL_LF_W_G")) {
        workMode = TD_LTE_AND_LTE_FDD_AND_W_AND_GSM;
    } else if (strstr(prop, "TL_TD_G")) {
        workMode = TD_LTE_AND_TD_AND_GSM;
    } else if (strcmp(prop, "W_G,G") == 0) {
        workMode = WCDMA_AND_GSM;
    } else if (strcmp(prop, "W_G,W_G") == 0) {
        workMode = PRIMARY_WCDMA_AND_GSM;
    } else if (strcmp(prop, "TL_LF_G,G") == 0) {
        workMode = TD_LTE_AND_LTE_FDD_AND_GSM;
    }
    return workMode;
}

void initWorkMode() {
    int workMode[SIM_COUNT];
    char prop[PROPERTY_VALUE_MAX] = {0};
    char numToStr[ARRAY_SIZE] = {0};
    RIL_SOCKET_ID simId = RIL_SOCKET_1;

    pthread_mutex_lock(&s_workModeMutex);

    for (simId = RIL_SOCKET_1; simId < SIM_COUNT; simId++) {
        memset(prop, 0, sizeof(prop));
        getProperty(simId, MODEM_WORKMODE_PROP, prop, "10");
        s_workMode[simId] = atoi(prop);
        workMode[simId] = s_workMode[simId];
        if (!strcmp(s_modem, "tl") || !strcmp(s_modem, "lf")) {
            if ((workMode[simId] == TD_AND_GSM) ||
                    (workMode[simId] == WCDMA_AND_GSM)) {
                workMode[simId] = TD_AND_WCDMA;
            }
        }
    }

#if (SIM_COUNT >= 2)
    if (s_modemConfig == LWG_G || s_modemConfig == W_G ||
        s_modemConfig == LG_G) {
        if (workMode[RIL_SOCKET_1] != GSM_ONLY &&
            workMode[RIL_SOCKET_2] != GSM_ONLY) {
            RLOGD("initWorkMode: change the work mode to 10");
            if (RIL_SOCKET_1 == s_multiModeSim) {
                workMode[RIL_SOCKET_2] = GSM_ONLY;
            } else {
                workMode[RIL_SOCKET_1] = GSM_ONLY;
            }
        }
    } else if (s_modemConfig == LWG_WG) {
        if (isPrimaryCardWorkMode(workMode[RIL_SOCKET_1]) &&
            isPrimaryCardWorkMode(workMode[RIL_SOCKET_2])) {
           RLOGD("initWorkMode: change the work mode to 255");
           if (RIL_SOCKET_1 == s_multiModeSim) {
               workMode[RIL_SOCKET_2] = TD_AND_WCDMA;
           } else {
               workMode[RIL_SOCKET_1] = TD_AND_WCDMA;
           }

       }
    }
    if (workMode[RIL_SOCKET_1] != s_workMode[RIL_SOCKET_1] ||
        workMode[RIL_SOCKET_2] != s_workMode[RIL_SOCKET_2]) {
        snprintf(numToStr, sizeof(numToStr), "%d,%d", workMode[RIL_SOCKET_1],
                 workMode[RIL_SOCKET_2]);
        RLOGD("initWorkMode: %s", numToStr);
        s_workMode[RIL_SOCKET_1] = workMode[RIL_SOCKET_1];
        s_workMode[RIL_SOCKET_2] = workMode[RIL_SOCKET_2];
        property_set(MODEM_WORKMODE_PROP, numToStr);
    }
#else
    if (workMode[RIL_SOCKET_1] != s_workMode[RIL_SOCKET_1]) {
        snprintf(numToStr, sizeof(numToStr), "%d,10", workMode[RIL_SOCKET_1]);
        RLOGD("initWorkMode: %s", numToStr);
        s_workMode[RIL_SOCKET_1] = workMode[RIL_SOCKET_1];
        property_set(MODEM_WORKMODE_PROP, numToStr);
    }
#endif

    pthread_mutex_unlock(&s_workModeMutex);
}

void buildWorkModeCmd(char *cmd, size_t size) {
    int simId;
    char strFormatter[AT_COMMAND_LEN] = {0};
    memset(strFormatter, 0, sizeof(strFormatter));

    for (simId = 0; simId < SIM_COUNT; simId++) {
        if (simId == 0) {
            snprintf(cmd, size, "AT+SPTESTMODEM=%d", s_workMode[simId]);
            if (SIM_COUNT == 1) {
                strncpy(strFormatter, cmd, size);
                strncat(strFormatter, ",%d", strlen(",%d"));
                snprintf(cmd, size, strFormatter, GSM_ONLY);
            }
        } else {
            strncpy(strFormatter, cmd, size);
            strncat(strFormatter, ",%d", strlen(",%d"));
            snprintf(cmd, size, strFormatter, s_workMode[simId]);
        }
    }
}

void *setRadioOnWhileSimBusy(void *param) {
    int channelID;
    int err;
    ATResponse *p_response = NULL;

    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    if ((int)socket_id < 0 || (int)socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        return NULL;
    }

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
            out_response = RADIO_TECH_GPRS;    /* GPRS */
            break;
        case 3:
            out_response = RADIO_TECH_EDGE;    /* EDGE */
            break;
        case 2:
            out_response = RADIO_TECH_UMTS;    /* TD */
            break;
        case 4:
            out_response = RADIO_TECH_HSDPA;    /* HSDPA */
            break;
        case 5:
            out_response = RADIO_TECH_HSUPA;   /* HSUPA */
            break;
        case 6:
            out_response = RADIO_TECH_HSPA;   /* HSPA */
            break;
        case 15:
            out_response = RADIO_TECH_HSPAP;   /* HSPA+ */
            break;
        case 7:
            out_response = RADIO_TECH_LTE;   /* LTE */
            break;
        case 16:
            out_response = RADIO_TECH_LTE_CA;   /* LTE_CA */
            break;
        default:
            out_response = RADIO_TECH_UNKNOWN;    /* UNKNOWN */
            break;
    }
    return out_response;
}

static int mapRegState(int inResponse) {
    int outResponse = RIL_UNKNOWN;

    switch (inResponse) {
        case 0:
            outResponse = RIL_NOT_REG_AND_NOT_SEARCHING;
            break;
        case 1:
            outResponse = RIL_REG_HOME;
            break;
        case 2:
            outResponse = RIL_NOT_REG_AND_SEARCHING;
            break;
        case 3:
            outResponse = RIL_REG_DENIED;
            break;
        case 4:
            outResponse = RIL_UNKNOWN;
            break;
        case 5:
            outResponse = RIL_REG_ROAMING;
            break;
        case 8:
        case 10:
            outResponse = RIL_NOT_REG_AND_EMERGENCY_AVAILABLE_AND_NOT_SEARCHING;
            break;
        case 12:
            outResponse = RIL_NOT_REG_AND_EMERGENCY_AVAILABLE_AND_SEARCHING;
            break;
        case 13:
            outResponse = RIL_REG_DENIED_AND_EMERGENCY_AVAILABLE;
            break;
        case 14:
            outResponse = RIL_UNKNOWN_AND_EMERGENCY_AVAILABLE;
            break;
        default:
            outResponse = RIL_UNKNOWN;
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

/* Calculation the number of one-bits in input parameter */
int bitCount(int rat) {
    int num = 0;
    while (rat) {
        if (rat % 2 == 1) {
            num++;
        }
        rat = rat / 2;
    }
    return num;
}

int isMaxRat(int rat) {
    int simId = 0, bitNum = 0, maxBitValue = -1;
    for (simId = 0; simId < SIM_COUNT; simId++) {
        bitNum = bitCount(s_radioAccessFamily[simId]);
        if (bitNum > maxBitValue) {
            maxBitValue = bitNum;
        }
    }
    return (bitCount(rat) == maxBitValue ? 1 : 0);
}

static void requestSignalStrength(int channelID, void *data,
                                      size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int err;
    char *line;
    ATResponse *p_response = NULL;
    ATResponse *p_newResponse = NULL;
    RIL_SignalStrength_v10 response_v10;

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

    err = at_tok_nextint(&line,
            &(response_v10.GW_SignalStrength.signalStrength));
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &(response_v10.GW_SignalStrength.bitErrorRate));
    if (err < 0) goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response_v10,
                          sizeof(RIL_SignalStrength_v10));
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
    RIL_SignalStrength_v10 responseV10;

    RIL_SIGNALSTRENGTH_INIT(responseV10);

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
        responseV10.GW_SignalStrength.signalStrength = response[0];
    }
    if (response[2] != -1 && response[2] != 255) {
        responseV10.GW_SignalStrength.signalStrength = response[2];
    }
    if (response[5] != -1 && response[5] != 255 && response[5] != -255) {
        responseV10.LTE_SignalStrength.rsrp = response[5];
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &responseV10,
                          sizeof(RIL_SignalStrength_v10));
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

    int err;
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

    if (request == RIL_REQUEST_VOICE_REGISTRATION_STATE ||
        request == RIL_REQUEST_VOICE_RADIO_TECH) {
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

    if (request == RIL_REQUEST_DATA_REGISTRATION_STATE) {
        s_PSRegStateDetail[socket_id] = regState;
    }

    if (8 == response[0]) {
        response[0] = RIL_NOT_REG_AND_EMERGENCY_AVAILABLE_AND_NOT_SEARCHING;
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
        /* STK case27.22.4.27.2.8 :if the command is rejected because
         * the class B terminal(Register State is 2g) is busy on a call
         */
        if (response[3] == RADIO_TECH_GPRS
                || response[3] == RADIO_TECH_EDGE) {
            s_in2G[socket_id] = 1;
        }
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
        int imsResp[2] = {0, 0};
        imsResp[0] = response[1];
        imsResp[1] = response[2];
        RIL_onRequestComplete(t, RIL_E_SUCCESS, imsResp, sizeof(imsResp));
    } else if (request == RIL_REQUEST_VOICE_RADIO_TECH) {
        RIL_RadioAccessFamily rAFamliy = RAF_UNKNOWN;
        if (response[3] != -1) {
            rAFamliy = 1 << response[3];
        }
        RIL_onRequestComplete(t, RIL_E_SUCCESS, &rAFamliy, sizeof(int));
    }
    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static int getOperatorName(char *plmn, char *operatorName,
                                OperatorInfoList *optList,
                                pthread_mutex_t *optListMutex) {
    if (plmn == NULL || operatorName == NULL) {
        return -1;
    }

    int ret = -1;
    MUTEX_ACQUIRE(*optListMutex);
    OperatorInfoList *pList = optList->next;
    OperatorInfoList *next;
    while (pList != optList) {
        next = pList->next;
        if (strcmp(plmn, pList->plmn) == 0) {
            memcpy(operatorName, pList->operatorName,
                   strlen(pList->operatorName) + 1);
            ret = 0;
            break;
        }
        pList = next;
    }
    MUTEX_RELEASE(*optListMutex);
    return ret;
}

static void addToOperatorInfoList(char *plmn, char *operatorName,
                                        OperatorInfoList *optList,
                                        pthread_mutex_t *optListMutex) {
    if (plmn == NULL || operatorName == NULL) {
        return;
    }

    MUTEX_ACQUIRE(*optListMutex);

    OperatorInfoList *pList = optList->next;
    OperatorInfoList *next;
    while (pList != optList) {
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

    OperatorInfoList *pHead = optList;
    pNode->next = pHead;
    pNode->prev = pHead->prev;
    pHead->prev->next = pNode;
    pHead->prev = pNode;
exit:
    MUTEX_RELEASE(*optListMutex);
}

static int matchNITZOperatorInfo(char *updatePlmn, const char *optInfo, char* plmn)
{
    if (plmn == NULL || updatePlmn == NULL) {
        return -1;
    }
    char operatorInfo[ARRAY_SIZE] = {0};
    char *ptr[3] = { NULL, NULL, NULL };
    char *outer_ptr = NULL;
    int i = 1;
    int ret = -1;
    snprintf(operatorInfo, sizeof(operatorInfo), "%s", optInfo);
    ptr[0] = strtok_r(operatorInfo, ",", &outer_ptr);
    if (ptr[0] != NULL) {
        if (!strcmp(ptr[0], plmn)) {
            while((ptr[i] = strtok_r(NULL, ",", &outer_ptr)) != NULL) {
                i++;
            }
            if (ptr[1] != NULL) {
                if (strcmp(ptr[1], "")) {
                    strncpy(updatePlmn, ptr[1], strlen(ptr[1]) + 1);
                    ret = 0;
                }
            } else if (ptr[2] != NULL) {
                if (strcmp(ptr[2], "")) {
                    strncpy(updatePlmn, ptr[2], strlen(ptr[2]) + 1);
                    ret = 0;
                }
            }
            RLOGD("match NITZ plmn: %s, Name: %s", plmn, updatePlmn);
        }
    }
    return ret;
}

static void requestOperator(int channelID, void *data, size_t datalen,
                                RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int err;
    int i;
    int skip;
    int ret = -1;
    char *response[3];
    char updatedPlmn[64] = {0};
    char plmnName[64] = {0};
    ATLine *p_cur;
    ATResponse *p_response = NULL;

    memset(response, 0, sizeof(response));

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
    if (response[2] != NULL) {
        if (strcmp(s_nitzOperatorInfo[socket_id], "")) {
            ret = matchNITZOperatorInfo(plmnName, s_nitzOperatorInfo[socket_id],
                                            response[2]);
        }
        if (ret < 0) {
            ret = getOperatorName(response[2], plmnName, &s_operatorXmlInfoList,
                                      &s_operatorXmlInfoListMutex);
            if (ret != 0) {
                ret = RIL_getONS(plmnName, response[2]);
                if (0 == ret) {
                    addToOperatorInfoList(response[2], plmnName,
                                              &s_operatorXmlInfoList,
                                              &s_operatorXmlInfoListMutex);
                }
            }
        }
        if (0 == ret) {
            response[0] = plmnName;
            response[1] = plmnName;
            RLOGD("get Operator Name: %s", response[0]);
        }

        memset(updatedPlmn, 0, sizeof(updatedPlmn));
        ret = getOperatorName(response[2], updatedPlmn,
                                  &s_operatorInfoList[socket_id],
                                  &s_operatorInfoListMutex[socket_id]);
        if (ret != 0) {
            err = updatePlmn(socket_id, (const char *)(response[2]),
                    updatedPlmn, sizeof(updatedPlmn));
            if (err == 0 && strcmp(updatedPlmn, "")) {
                RLOGD("updated plmn = %s", updatedPlmn);
                response[0] = updatedPlmn;
                response[1] = updatedPlmn;
                addToOperatorInfoList(response[2], updatedPlmn,
                                          &s_operatorInfoList[socket_id],
                                          &s_operatorInfoListMutex[socket_id]);
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

static int getRadioFeatures(int socket_id) {
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
    } else if (s_modemConfig == WG_WG) {
        rat = WCDMA | GSM;
    } else {
        workMode = s_workMode[socket_id];
        if (workMode == GSM_ONLY) {
            rat = GSM;
        } else if (workMode == NONE) {
            rat = RAF_UNKNOWN;
        } else if (s_modemConfig == LG_G) {
                rat = RAF_LTE | GSM;
        } else if (s_modemConfig == W_G ) {
            rat = WCDMA | GSM;
        } else {
            rat = RAF_LTE | WCDMA | GSM;
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
    responseRc->rat = getRadioFeatures(s_multiModeSim);
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
    responseRc->rat = getRadioFeatures(singleModeSim);
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

    int err, i, rc;
    char cmd[AT_COMMAND_LEN] = {0};
    char simEnabledProp[PROPERTY_VALUE_MAX] = {0};
    char radioResetProp[PROPERTY_VALUE_MAX] = {0};
    char manualAttachProp[PROPERTY_VALUE_MAX] = {0};
    struct timespec timeout;
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    assert(datalen >= sizeof(int *));
    s_desiredRadioState[socket_id] = ((int *)data)[0];
    if (s_desiredRadioState[socket_id] == 0) {
        getSIMStatus(false, channelID);

        /* The system ask to shutdown the radio */
        err = at_send_command(s_ATChannels[channelID],
                "AT+SFUN=5", &p_response);
        if (err < 0 || p_response->success == 0) {
            goto error;
        }

        for (i = 0; i < MAX_PDP; i++) {
            if (s_dataAllowed[socket_id] && s_PDP[socket_id][i].state == PDP_BUSY) {
                RLOGD("s_PDP[%d].state = %d", i, s_PDP[socket_id][i].state);
                putPDP(socket_id, i);
            }
        }
        setRadioState(channelID, RADIO_STATE_OFF);
    } else if (s_desiredRadioState[socket_id] > 0 &&
                s_radioState[socket_id] == RADIO_STATE_OFF) {
        getSIMStatus(false, channelID);
        if (s_simBusy[socket_id].s_sim_busy) {
            RLOGD("SIM is busy now, wait for CPIN status!");
            clock_gettime(CLOCK_MONOTONIC, &timeout);
            timeout.tv_sec += 8;
            pthread_mutex_lock(&s_simBusy[socket_id].s_sim_busy_mutex);
            rc = pthread_cond_timedwait(&s_simBusy[socket_id].s_sim_busy_cond,
                                &s_simBusy[socket_id].s_sim_busy_mutex,
                                &timeout);
            if (rc == ETIMEDOUT) {
                RLOGD("stop waiting when time is out!");
            } else {
                RLOGD("CPIN is OK now, do it!");
            }
            pthread_mutex_unlock(&s_simBusy[socket_id].s_sim_busy_mutex);
        }
        initSIMPresentState();
        getProperty(socket_id, SIM_ENABLED_PROP, simEnabledProp, "1");
        if (strcmp(simEnabledProp, "0") == 0) {
            RLOGE("sim enable false,radio power on failed");
            goto error;
        }
        buildWorkModeCmd(cmd, sizeof(cmd));

#if (SIM_COUNT == 2)
        if (s_presentSIMCount == 0) {
            if (s_isSimPresent[1 - socket_id] == SIM_UNKNOWN) {
                RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED,
                                          NULL, 0, socket_id);
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
                return;
            } else {
                getProperty(1 - socket_id, SIM_ENABLED_PROP, simEnabledProp, "1");
                if (socket_id != s_multiModeSim && !strcmp(simEnabledProp, "1")) {
                    if (s_radioState[1 - socket_id] == RADIO_STATE_OFF ||
                            s_radioState[1 - socket_id] == RADIO_STATE_UNAVAILABLE) {
                        int *data = (int *)calloc(1, sizeof(int));
                        data[0] = 1;
                        onRequest(RIL_REQUEST_RADIO_POWER, data, sizeof(int), NULL, 1 - socket_id);
                    }
                    RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED,
                                              NULL, 0, socket_id);
                    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
                    return;
                }
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

        err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
        if (err < 0 || p_response->success == 0) {
            if (p_response != NULL &&
                    strcmp(p_response->finalResponse, "+CME ERROR: 15") == 0) {
                RLOGE("set wrong workmode in cmcc version");
            #if (SIM_COUNT == 2)
                if (s_multiModeSim == RIL_SOCKET_1) {
                    s_multiModeSim = RIL_SOCKET_2;
                } else if (s_multiModeSim == RIL_SOCKET_2) {
                    s_multiModeSim = RIL_SOCKET_1;
                }
            #endif
                setWorkMode();
                buildWorkModeCmd(cmd, sizeof(cmd));
                at_send_command(s_ATChannels[channelID], cmd, NULL);
                RIL_onUnsolicitedResponse(
                        RIL_EXT_UNSOL_RADIO_CAPABILITY_CHANGED,
                        NULL, 0, socket_id);
            }
        }
        AT_RESPONSE_FREE(p_response);

        if (s_isLTE) {
            int cemode = 0;
            char cemodeProp[PROPERTY_VALUE_MAX] = {0};
            property_get(CEMODE_PROP, cemodeProp, "1");
            cemode = atoi(cemodeProp);
            memset(cmd, 0, sizeof(cmd));
            snprintf(cmd, sizeof(cmd), "AT+CEMODE=%d", cemode);
            at_send_command(s_ATChannels[channelID], cmd, NULL);
            p_response = NULL;

            if (s_roModemConfig == LWG_LWG) {
                RLOGD("socket_id = %d, s_multiModeSim = %d", socket_id,
                        s_multiModeSim);
                if (socket_id == s_multiModeSim) {
                    snprintf(cmd, sizeof(cmd), "AT+SPSWDATA");
                    at_send_command(s_ATChannels[channelID], cmd, NULL);
                }
            } else {
                if (s_presentSIMCount == 1 && socket_id == s_multiModeSim) {
                    err = at_send_command(s_ATChannels[channelID],
                            "AT+SAUTOATT=1", &p_response);
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
                        if (s_modemConfig == WG_WG) {
                            RLOGD("switch data card according to allow data");
                            snprintf(cmd, sizeof(cmd), "AT+SPSWITCHDATACARD=%d,%d", socket_id,s_dataAllowed[socket_id]);
                            at_send_command(s_ATChannels[channelID], cmd, NULL);
                            if (s_dataAllowed[socket_id] == 1) {
                                at_send_command(s_ATChannels[channelID], "AT+SAUTOATT=1",NULL);
                            }
                        } else {
                            snprintf(cmd, sizeof(cmd), "AT+SPSWITCHDATACARD=%d,0", socket_id);
                            at_send_command(s_ATChannels[channelID], cmd, NULL);
                        }
                    }
                }
#endif
            }
            property_get(LTE_MANUAL_ATTACH_PROP, manualAttachProp, "0");
            RLOGD("persist.vendor.radio.manualattach: %s", manualAttachProp);
            snprintf(cmd, sizeof(cmd), "AT+SPLTEMANUATT=%s", manualAttachProp);
            at_send_command(s_ATChannels[channelID], cmd, NULL);

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
//            if (s_simBusy[socket_id].s_sim_busy) {
//                RLOGD("Now SIM is busy, wait for CPIN READY and then"
//                      "set radio on again.");
//                pthread_t tid;
//                pthread_attr_t attr;
//                pthread_attr_init(&attr);
//                pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
//                pthread_create(&tid, &attr, (void *)setRadioOnWhileSimBusy,
//                                 (void *)&s_socketId[socket_id]);
//            }

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

    RIL_NetworkList *network =
            (RIL_NetworkList *)calloc(1, sizeof(RIL_NetworkList));
    network->operatorNumeric = (char *)data;

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
    free(network);
    return;

error:
    if (p_response != NULL &&
            !strcmp(p_response->finalResponse, "+CME ERROR: 30")) {
        RIL_onRequestComplete(t, RIL_E_ILLEGAL_SIM_OR_ME, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
    at_response_free(p_response);
    free(network);
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
    int err, stat, act;
    int tok = 0, count = 0, i = 0;
    char *line;
    char **responses, **cur;
    char *tmp, *startTmp = NULL;
    ATResponse *p_response = NULL;

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
    tmp = (char *)malloc(count * sizeof(char) * 32);
    startTmp = tmp;

    char *updatedNetList = (char *)alloca(count * sizeof(char) * 64);
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

        err = at_tok_nextint(&line, &act);
        if (err < 0) continue;

        snprintf(tmp, count * sizeof(char) * 32, "%s%s%d", cur[2], " ", act);
        RLOGD("requestNetworkList cur[2] act = %s", tmp);
        cur[2] = tmp;

#if defined (RIL_EXTENSION)
        err = updateNetworkList(socket_id, cur, 4 * sizeof(char *),
                                updatedNetList, count * sizeof(char) * 64);
        if (err == 0) {
            RLOGD("updatedNetworkList: %s", updatedNetList);
            cur[0] = updatedNetList;
        }
        updatedNetList += 64;
#endif
        cur += 4;
        tmp += 32;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, responses,
            count * 4 * sizeof(char *));
    at_response_free(p_response);
    free(startTmp);
    return;

error:
    if (p_response != NULL &&
            !strcmp(p_response->finalResponse, "+CME ERROR: 3")) {
        RIL_onRequestComplete(t, RIL_E_OPERATION_NOT_ALLOWED, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
    at_response_free(p_response);
    free(startTmp);
}

static void requestResetRadio(int channelID, void *data, size_t datalen,
                                  RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int err;
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

static void requestSetBandMode(int channelID, void *data, size_t datalen,
                                   RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    switch (((int *)data)[0]) {
        case BAND_MODE_UNSPECIFIED:
            at_send_command(s_ATChannels[channelID], "AT+SBAND=1,13,14", NULL);
            at_send_command(s_ATChannels[channelID], "AT+SPFDDBAND=1,1,1", NULL);
            at_send_command(s_ATChannels[channelID], "AT+SPFDDBAND=1,2,1", NULL);
            at_send_command(s_ATChannels[channelID], "AT+SPFDDBAND=1,5,1", NULL);
            break;
        case BAND_MODE_EURO:
            at_send_command(s_ATChannels[channelID], "AT+SBAND=1,13,4", NULL);
            at_send_command(s_ATChannels[channelID], "AT+SPFDDBAND=1,1,1", NULL);
            break;
        case BAND_MODE_USA:
            at_send_command(s_ATChannels[channelID], "AT+SBAND=1,13,7", NULL);
            at_send_command(s_ATChannels[channelID], "AT+SPFDDBAND=1,2,1", NULL);
            at_send_command(s_ATChannels[channelID], "AT+SPFDDBAND=1,5,1", NULL);
            break;
        case BAND_MODE_JPN:
            at_send_command(s_ATChannels[channelID], "AT+SPFDDBAND=1,2,1", NULL);
            break;
        case BAND_MODE_AUS:
            at_send_command(s_ATChannels[channelID], "AT+SBAND=1,13,4", NULL);
            at_send_command(s_ATChannels[channelID], "AT+SPFDDBAND=1,2,1", NULL);
            at_send_command(s_ATChannels[channelID], "AT+SPFDDBAND=1,5,1", NULL);
            break;
        case BAND_MODE_AUS_2:
            at_send_command(s_ATChannels[channelID], "AT+SBAND=1,13,4", NULL);
            at_send_command(s_ATChannels[channelID], "AT+SPFDDBAND=1,5,1", NULL);
            break;
        default:
            break;
    }
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
}

static void requestGetBandMode(int channelID, void *data, size_t datalen,
                               RIL_Token t) {
    RIL_UNUSED_PARM(channelID);
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int i, size = 5;
    int response[20] = {0};

    response[0] = size;
    for (i = 1; i <= size; i++) {
        response[i] = i;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS,
            response, (size + 1) * sizeof(int));
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
    RLOGD("ENGTEST_ENABLE_PROP is %s", prop);

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
                type = LTE_FDD_AND_W_AND_GSM;
                break;
            case NT_TD_LTE_WCDMA_GSM:
                type = TD_LTE_AND_W_AND_GSM;
                break;
            case NT_LTE_FDD_TD_LTE_WCDMA_GSM:
                type = TD_LTE_AND_LTE_FDD_AND_W_AND_GSM;
                break;
            case NT_TD_LTE_TDSCDMA_GSM:
                type = TD_LTE_AND_TD_AND_GSM;
                break;
            case NT_LTE_FDD_TD_LTE_TDSCDMA_GSM:
                type = TD_LTE_AND_LTE_FDD_AND_TD_AND_GSM;
                break;
            case NT_LTE_FDD_TD_LTE_WCDMA_TDSCDMA_GSM:
                type = TD_LTE_AND_LTE_FDD_AND_W_AND_TD_AND_GSM;
                break;
            case NT_GSM: {
                type = GSM_ONLY;
                if (s_modemConfig != LWG_LWG) {
                    if (socket_id == s_multiModeSim) {
                        type = PRIMARY_GSM_ONLY;
                    }
                }
                break;
            }
            case NT_WCDMA: {
                type = WCDMA_ONLY;
                if (s_modemConfig == LWG_WG || s_modemConfig == WG_WG) {
                    if (socket_id == s_multiModeSim) {
                        type = PRIMARY_WCDMA_ONLY;
                    }
                }
                break;
            }
            case NT_TDSCDMA: {
                type = TD_ONLY;
                break;
            }
            case NT_TDSCDMA_GSM: {
                type = TD_AND_GSM;
                break;
            }
            case NT_WCDMA_GSM: {
                type = WCDMA_AND_GSM;
                if (s_modemConfig == LWG_WG) {
                    if (socket_id == s_multiModeSim) {
                        type = PRIMARY_WCDMA_AND_GSM;
                    } else {
                        type = TD_AND_WCDMA;
                    }
                } else if (s_modemConfig == WG_WG) {
                    if (socket_id == s_multiModeSim) {
                        type = PRIMARY_WCDMA_AND_GSM;
                    }
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
            case NT_LTE_FDD_TD_LTE_GSM:
                type = TD_LTE_AND_LTE_FDD_AND_GSM;
                break;
            default:
                break;
        }
    } else {  // request by FWK
        switch (((int *)data)[0]) {
            case NETWORK_MODE_LTE_GSM_WCDMA:
            case NETWORK_MODE_LTE_GSM:
                type = getMultiMode();
                break;
            case NETWORK_MODE_WCDMA_PREF: {
                if (s_modemConfig == LWG_WG) {
                    if (socket_id == s_multiModeSim) {
                        type = PRIMARY_TD_AND_WCDMA;
                    } else {
                        type = TD_AND_WCDMA;
                    }
                } else if (s_modemConfig == WG_WG) {
                    if (socket_id == s_multiModeSim) {
                        type = PRIMARY_WCDMA_AND_GSM;
                    } else {
                        type = WCDMA_AND_GSM;
                    }
                } else {
                    int mode = getMultiMode();
                    type = mode;
                    if (mode == TD_LTE_AND_LTE_FDD_AND_W_AND_TD_AND_GSM) {
                        type = TD_AND_WCDMA;
                    } else if (mode == TD_LTE_AND_LTE_FDD_AND_W_AND_GSM) {
                        type = WCDMA_AND_GSM;
                    } else if (mode == TD_LTE_AND_TD_AND_GSM) {
                        type = TD_AND_GSM;
                    }
                }
                break;
            }
            case NETWORK_MODE_GSM_ONLY:
                type = GSM_ONLY;
                if (s_modemConfig != LWG_LWG) {
                    if (socket_id == s_multiModeSim) {
                        type = PRIMARY_GSM_ONLY;
                    }
                }
                break;
            case NETWORK_MODE_LTE_ONLY: {
                int mode = getMultiMode();
                type = mode;
                if (mode == TD_LTE_AND_LTE_FDD_AND_W_AND_TD_AND_GSM ||
                    mode == TD_LTE_AND_LTE_FDD_AND_W_AND_GSM ||
                    mode == TD_LTE_AND_LTE_FDD_AND_GSM) {
                    type = TD_LTE_AND_LTE_FDD;
                } else if (mode == TD_LTE_AND_TD_AND_GSM) {
                    type = TD_LTE;
                }
                break;
            }
            case NETWORK_MODE_WCDMA_ONLY:
                type = WCDMA_ONLY;
                if (s_modemConfig == LWG_WG || s_modemConfig == WG_WG) {
                    if (socket_id == s_multiModeSim) {
                        type = PRIMARY_WCDMA_ONLY;
                    }
                }
                break;
            case NETWORK_MODE_LTE_WCDMA:
                type = TD_LTE_AND_LTE_FDD_WCDMA;
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
    if (s_modemConfig == LWG_G || s_modemConfig == W_G ||
        s_modemConfig == LG_G) {
        if (workMode == NONE || workMode == GSM_ONLY) {
            RLOGD("SetLTEPreferredNetType: not data card");
            errType = RIL_E_SUCCESS;
            goto done;
        }
    } else if (s_modemConfig == LWG_WG) {
        if (s_multiModeSim != socket_id && isPrimaryCardWorkMode(type)) {
            RLOGE("SetLTEPreferredNetType: not data card");
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
#if (SIM_COUNT == 2)
    if (socket_id == RIL_SOCKET_1) {
        snprintf(numToStr, sizeof(numToStr), "%d,%d", type,
                s_workMode[RIL_SOCKET_2]);
    } else if (socket_id == RIL_SOCKET_2){
        snprintf(numToStr, sizeof(numToStr), "%d,%d", s_workMode[RIL_SOCKET_1],
                type);
    }
#endif
#else
     snprintf(numToStr, sizeof(numToStr), "%d,10", type);
#endif

    RLOGD("set network type workmode:%s", numToStr);
    snprintf(cmd, sizeof(cmd), "AT+SPTESTMODE=%s", numToStr);

    const char *respCmd = "+SPTESTMODE:";
    int retryTimes = 0;

again:
    /* timeout is in seconds
     * Due to AT+SPTESTMODE's response maybe be later than URC response,
     * so addAsyncCmdList before at_send_command
     */
    addAsyncCmdList(socket_id, t, respCmd, NULL, 120);
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (err < 0 || p_response->success == 0) {
        if (retryTimes < 3 && p_response != NULL &&
                strcmp(p_response->finalResponse, "+CME ERROR: 3") == 0) {
            RLOGE("AT+SPTESTMODE return +CME ERROR: 3, try again");
            retryTimes++;
            AT_RESPONSE_FREE(p_response);
            removeAsyncCmdList(t, respCmd);
            sleep(3);
            goto again;
        } else {
            errType = RIL_E_GENERIC_FAILURE;
            removeAsyncCmdList(t, respCmd);
            goto done;
        }
    }

    s_workMode[socket_id] = type;
    property_set(MODEM_WORKMODE_PROP, numToStr);

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

    s_workMode[socket_id] = workmode;
#if defined (ANDROID_MULTI_SIM)
    #if (SIM_COUNT == 2)
        snprintf(numToStr, sizeof(numToStr), "%d,%d", s_workMode[RIL_SOCKET_1],
                s_workMode[RIL_SOCKET_2]);
    #endif
#else
    snprintf(numToStr, sizeof(numToStr), "%d,10", s_workMode[RIL_SOCKET_1]);
#endif

    RLOGD("requestSetPreferredNetType set network type workmode:%s", numToStr);
    property_set(MODEM_WORKMODE_PROP, numToStr);

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
    RLOGD("ENGTEST_ENABLE_PROP is %s", prop);

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
            case LTE_FDD_AND_W_AND_GSM:
                type = NT_LTE_FDD_WCDMA_GSM;
                break;
            case TD_LTE_AND_W_AND_GSM:
                type = NT_TD_LTE_WCDMA_GSM;
                break;
            case TD_LTE_AND_LTE_FDD_AND_W_AND_GSM:
                type = NT_LTE_FDD_TD_LTE_WCDMA_GSM;
                break;
            case TD_LTE_AND_TD_AND_GSM:
                type = NT_TD_LTE_TDSCDMA_GSM;
                break;
            case TD_LTE_AND_LTE_FDD_AND_TD_AND_GSM:
                type = NT_LTE_FDD_TD_LTE_TDSCDMA_GSM;
                break;
            case TD_LTE_AND_LTE_FDD_AND_W_AND_TD_AND_GSM:
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
            case TD_LTE_AND_LTE_FDD_AND_GSM:
                type = NT_LTE_FDD_TD_LTE_GSM;
                break;
            case TD_LTE_AND_LTE_FDD_WCDMA:
                type = NT_LTE_FDD_TD_LTE_WCDMA;
                break;
            default:
                break;
        }
    } else {
        switch (s_workMode[socket_id]) {
            case TD_LTE_AND_LTE_FDD_AND_W_AND_TD_AND_GSM:
            case TD_LTE_AND_LTE_FDD_AND_W_AND_GSM:
            case TD_LTE_AND_TD_AND_GSM:
                type = NETWORK_MODE_LTE_GSM_WCDMA;
                break;
            case TD_LTE_AND_LTE_FDD_AND_GSM:
                type = NETWORK_MODE_LTE_GSM;
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
            case TD_LTE_AND_LTE_FDD_WCDMA:
                type = NETWORK_MODE_LTE_WCDMA;
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
    //for vts cases
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
    if (s_isSimPresent[socket_id] != PRESENT) {
        RLOGE("requestNeighboaringCellIds: card is absent");
        RIL_onRequestComplete(t, RIL_E_SIM_ABSENT, NULL, 0);
        return;
    }

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

    int err, i;
    int mcc, mnc, mnc_digit, lac = 0;
    int sig2G = 0, sig3G = 0;
    int pci = 0, cid = 0;
    int arfcn = INT_MAX, bsic = INT_MAX, psc =INT_MAX;
    int rsrp = 0, rsrq = 0, act = 0;
    int commas = 0, sskip = 0, registered = 0;
    int netType = 0, cellType = 0, biterr2G = 0, biterr3G = 0;
    int cellIdNumber = 0, current = 0, signalStrength = 0;
    char *line =  NULL, *p = NULL, *skip = NULL, *plmn = NULL;

    ATResponse *p_response = NULL;
    ATResponse *p_newResponse = NULL;
    RIL_CellInfo_v12 *response = NULL;

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

    if (plmn != NULL) {
        mnc_digit = strlen(plmn) - 3;
        if (strlen(plmn) == 5) {
            mcc = atoi(plmn) / 100;
            mnc = atoi(plmn) - mcc * 100;
        } else if (strlen(plmn) == 6) {
            mcc = atoi(plmn) / 1000;
            mnc = atoi(plmn) - mcc * 1000;
        } else {
            RLOGE("Invalid plmn");
        }
    }


    if (netType == 7 || netType == 16) {
        cellType = RIL_CELL_INFO_TYPE_LTE;
    } else if (netType == 0 || netType == 1 || netType == 3) {
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

    if (s_isLTE) {
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
    } else {
        err = at_send_command_singleline(s_ATChannels[channelID], "AT+CSQ",
                                         "+CSQ:", &p_response);
        if (err < 0 || p_response->success == 0) {
            goto error;
        }

        csq_execute_cmd_rsp(p_response, &p_newResponse);
        if (p_newResponse == NULL) goto error;

        line = p_newResponse->p_intermediates->line;

        err = at_tok_start(&line);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &sig3G);
        if (err < 0) goto error;

        sig2G = sig3G;

        err = at_tok_nextint(&line, &biterr3G);
        if (err < 0) goto error;

        biterr2G = biterr3G;
    }

    AT_RESPONSE_FREE(p_response);

    // For cellinfo
    if (cellType == RIL_CELL_INFO_TYPE_LTE) {
        err = at_send_command_singleline(s_ATChannels[channelID],
                "AT+SPQ4GNCELLEX=4,4", "+SPQ4GNCELL", &p_response);
        if (err < 0 || p_response->success == 0) {
            goto error;
        }

        line = p_response->p_intermediates->line;
        err = at_tok_start(&line);
        if (err < 0) goto error;

        // AT> +SPQ4GNCELLEX: 4,4
        // AT< +SPQ4GNCELL: Serv_Cell_Earfcn, Serv_Cell_Pci, Serv_Cell_Rsrp, Serv_Cell_Rsrq, Ncell_num,
        // Neighbor_Cell_Earfcn, Neighbor_Cell_Pci, Neighbor_Cell_Rsrp, Neighbor_Cell_Rsrq,
        // ...

        err = at_tok_nextint(&line, &arfcn);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &pci);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &rsrp);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &rsrq);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &cellIdNumber);
        if (err < 0) goto error;

        response = (RIL_CellInfo_v12 *)
                calloc((cellIdNumber + 1), sizeof(RIL_CellInfo_v12));
        if (response == NULL) {
            RLOGE("Failed to calloc memory for response");
            goto error;
        }

        response[0].CellInfo.lte.cellIdentityLte.mnc = mnc;
        response[0].CellInfo.lte.cellIdentityLte.mnc_digit = mnc_digit;
        response[0].CellInfo.lte.cellIdentityLte.mcc = mcc;
        response[0].CellInfo.lte.cellIdentityLte.ci  = cid;
        response[0].CellInfo.lte.cellIdentityLte.pci = pci;
        response[0].CellInfo.lte.cellIdentityLte.tac = lac;
        response[0].CellInfo.lte.cellIdentityLte.earfcn = arfcn;

        response[0].CellInfo.lte.signalStrengthLte.cqi = INT_MAX;
        response[0].CellInfo.lte.signalStrengthLte.rsrp = rsrp / (-100);
        response[0].CellInfo.lte.signalStrengthLte.rsrq = rsrq / (-100);
        response[0].CellInfo.lte.signalStrengthLte.rssnr = INT_MAX;
        response[0].CellInfo.lte.signalStrengthLte.signalStrength = rsrp / 100 + 140;
        response[0].CellInfo.lte.signalStrengthLte.timingAdvance  = INT_MAX;

        for (current = 0; at_tok_hasmore(&line), current < cellIdNumber;
             current++) {
            err = at_tok_nextint(&line, &arfcn);
            if (err < 0) goto error;

            err = at_tok_nextint(&line, &pci);
            if (err < 0) goto error;

            err = at_tok_nextint(&line, &rsrp);
            if (err < 0) goto error;


            err = at_tok_nextint(&line, &rsrq);
            if (err < 0) goto error;

            response[current + 1].CellInfo.lte.cellIdentityLte.mcc = INT_MAX;
            response[current + 1].CellInfo.lte.cellIdentityLte.mnc = INT_MAX;
            response[current + 1].CellInfo.lte.cellIdentityLte.ci = INT_MAX;
            response[current + 1].CellInfo.lte.cellIdentityLte.tac = INT_MAX;
            response[current + 1].CellInfo.lte.cellIdentityLte.pci = pci;
            response[current + 1].CellInfo.lte.cellIdentityLte.earfcn = arfcn;

            response[current + 1].CellInfo.lte.signalStrengthLte.cqi = INT_MAX;
            response[current + 1].CellInfo.lte.signalStrengthLte.rsrp = rsrp / (-100);
            response[current + 1].CellInfo.lte.signalStrengthLte.rsrq = rsrq / (-100);
            response[current + 1].CellInfo.lte.signalStrengthLte.rssnr = INT_MAX;
            response[current + 1].CellInfo.lte.signalStrengthLte.signalStrength = rsrp / 100 + 140;
            response[current + 1].CellInfo.lte.signalStrengthLte.timingAdvance  = INT_MAX;

            response[current + 1].registered = 0;
            response[current + 1].cellInfoType = cellType;
            response[current + 1].timeStampType = RIL_TIMESTAMP_TYPE_OEM_RIL;
            response[current + 1].timeStamp = INT_MAX;
        }
    } else if (cellType == RIL_CELL_INFO_TYPE_GSM) {
        err = at_send_command_singleline(s_ATChannels[channelID],
                "AT+SPQ2GNCELL", "+SPQ2GNCELL", &p_response);
        if (err < 0 || p_response->success == 0) {
            goto error;
        }

        // AT+SPQ2GNCELL:
        // Serv_Cell_Lac+Cid, Serv_Cell_SignalStrength, Serv_Cell_Arfcn, Serv_Cell_Bsic, Ncell_num,
        // Neighbor_Cell_Lac+Cid, Neighbor_Cell_SignalStrength, Neighbor_Cell_Arfcn, Neighbor_Cell_Bsic,
        // ...

        line = p_response->p_intermediates->line;
        err = at_tok_start(&line);
        if (err < 0) goto error;

        err = at_tok_nextstr(&line, &skip);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &sskip);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &arfcn);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &bsic);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &cellIdNumber);
        if (err < 0) goto error;

        response = (RIL_CellInfo_v12 *)
                calloc((cellIdNumber + 1), sizeof(RIL_CellInfo_v12));
        if (response == NULL) {
            RLOGE("Failed to calloc memory for response");
            goto error;
        }

        response[0].CellInfo.gsm.cellIdentityGsm.mcc = mcc;
        response[0].CellInfo.gsm.cellIdentityGsm.mnc = mnc;
        response[0].CellInfo.gsm.cellIdentityGsm.mnc_digit = mnc_digit;
        response[0].CellInfo.gsm.cellIdentityGsm.lac = lac;
        response[0].CellInfo.gsm.cellIdentityGsm.cid = cid;
        response[0].CellInfo.gsm.cellIdentityGsm.arfcn = arfcn;
        response[0].CellInfo.gsm.cellIdentityGsm.bsic = bsic;

        response[0].CellInfo.gsm.signalStrengthGsm.bitErrorRate = biterr2G;
        response[0].CellInfo.gsm.signalStrengthGsm.signalStrength = sig2G;
        response[0].CellInfo.gsm.signalStrengthGsm.timingAdvance = INT_MAX;

        for (current = 0; at_tok_hasmore(&line), current < cellIdNumber;
             current++) {
            err = at_tok_nextstr(&line, &skip);
            if (err < 0) goto error;

            sscanf(skip, "%04x%04x", &lac, &cid);

            err = at_tok_nextint(&line, &sig2G);
            if (err < 0) goto error;

            err = at_tok_nextint(&line, &arfcn);
            if (err < 0) goto error;

            err = at_tok_nextint(&line, &bsic);
            if (err < 0) goto error;

            signalStrength = (sig2G + 3) / 2;

            response[current + 1].CellInfo.gsm.cellIdentityGsm.mcc = INT_MAX;
            response[current + 1].CellInfo.gsm.cellIdentityGsm.mnc = INT_MAX;
            response[current + 1].CellInfo.gsm.cellIdentityGsm.lac = lac;
            response[current + 1].CellInfo.gsm.cellIdentityGsm.cid = cid;
            response[current + 1].CellInfo.gsm.cellIdentityGsm.arfcn = arfcn;
            response[current + 1].CellInfo.gsm.cellIdentityGsm.bsic = bsic;

            response[current + 1].CellInfo.gsm.signalStrengthGsm.bitErrorRate = INT_MAX;
            response[current + 1].CellInfo.gsm.signalStrengthGsm.signalStrength =
                    signalStrength > 31 ? 31 : (signalStrength < 0 ? 0 : signalStrength);
            response[current + 1].CellInfo.gsm.signalStrengthGsm.timingAdvance = INT_MAX;

            response[current + 1].registered = 0;
            response[current + 1].cellInfoType = cellType;
            response[current + 1].timeStampType = RIL_TIMESTAMP_TYPE_OEM_RIL;
            response[current + 1].timeStamp = INT_MAX;
        }
    } else {
        err = at_send_command_singleline(s_ATChannels[channelID],
                "AT+SPQ3GNCELLEX=4,3", "+SPQ3GNCELL", &p_response);
        if (err < 0 || p_response->success == 0) {
            goto error;
        }

        // AT> +SPQ3GNCELLEX: 4,3
        // AT< +SPQ3GNCELLEX:
        // "00000000", Serv_Cell_SignalStrength, Serv_Cell_Uarfcn, Serv_Cell_Psc, Ncell_num,
        // Neighbor_Cell_Uarfcn, Neighbor_Cell_Psc, Neighbor_Cell_SignalStrength,
        // ...

        line = p_response->p_intermediates->line;
        err = at_tok_start(&line);
        if (err < 0) goto error;

        err = at_tok_nextstr(&line, &skip);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &sskip);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &arfcn);
        if (err < 0) goto error;

        if (at_tok_hasmore(&line)) {
            err = at_tok_nextint(&line, &psc);
            if (err < 0) goto error;

            err = at_tok_nextint(&line, &cellIdNumber);
            if (err < 0) goto error;
        }

        response = (RIL_CellInfo_v12 *)
                calloc((cellIdNumber + 1), sizeof(RIL_CellInfo_v12));
        if (response == NULL) {
            RLOGE("Failed to calloc memory for response");
            goto error;
        }

        response[0].CellInfo.wcdma.cellIdentityWcdma.mcc = mcc;
        response[0].CellInfo.wcdma.cellIdentityWcdma.mnc = mnc;
        response[0].CellInfo.wcdma.cellIdentityWcdma.mnc_digit = mnc_digit;
        response[0].CellInfo.wcdma.cellIdentityWcdma.lac = lac;
        response[0].CellInfo.wcdma.cellIdentityWcdma.cid = cid;
        response[0].CellInfo.wcdma.cellIdentityWcdma.psc = psc;
        response[0].CellInfo.wcdma.cellIdentityWcdma.uarfcn = arfcn;

        response[0].CellInfo.wcdma.signalStrengthWcdma.bitErrorRate = biterr3G;
        response[0].CellInfo.wcdma.signalStrengthWcdma.signalStrength = sig3G;

        if (at_tok_hasmore(&line)) {
            for (current = 0; at_tok_hasmore(&line), current < cellIdNumber;
                    current++) {
                err = at_tok_nextint(&line, &arfcn);
                if (err < 0) goto error;

                err = at_tok_nextint(&line, &psc);
                if (err < 0) goto error;

                err = at_tok_nextint(&line, &sig3G);
                if (err < 0) goto error;

                signalStrength = (sig3G - 3) / 2;

                response[current + 1].CellInfo.wcdma.cellIdentityWcdma.mcc = INT_MAX;
                response[current + 1].CellInfo.wcdma.cellIdentityWcdma.mnc = INT_MAX;
                response[current + 1].CellInfo.wcdma.cellIdentityWcdma.lac = INT_MAX;
                response[current + 1].CellInfo.wcdma.cellIdentityWcdma.cid = INT_MAX;
                response[current + 1].CellInfo.wcdma.cellIdentityWcdma.psc = psc;
                response[current + 1].CellInfo.wcdma.cellIdentityWcdma.uarfcn = arfcn;

                response[current + 1].CellInfo.wcdma.signalStrengthWcdma.bitErrorRate = INT_MAX;
                response[current + 1].CellInfo.wcdma.signalStrengthWcdma.signalStrength =
                        signalStrength > 31 ? 31 : (signalStrength < 0 ? 0 : signalStrength);

                response[current + 1].registered = 0;
                response[current + 1].cellInfoType = cellType;
                response[current + 1].timeStampType = RIL_TIMESTAMP_TYPE_OEM_RIL;
                response[current + 1].timeStamp = INT_MAX;
            }
        }
    }

    uint64_t curTime = ril_nano_time();
    if (registered == 1 || registered == 5) {
        registered = 1;
    }
    response[0].registered = registered;
    response[0].cellInfoType = cellType;
    response[0].timeStampType = RIL_TIMESTAMP_TYPE_OEM_RIL;
    response[0].timeStamp = curTime - 1000;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, response,
            (cellIdNumber + 1) * sizeof(RIL_CellInfo_v12));

    at_response_free(p_response);
    at_response_free(p_newResponse);
    free(response);
    return;

error:
    at_response_free(p_response);
    at_response_free(p_newResponse);
    free(response);

    RIL_onRequestComplete(t, RIL_E_NO_NETWORK_FOUND, NULL, 0);
}

static void requestShutdown(int channelID,
        void *data, size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int err;

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    if (all_calls(channelID, 1)) {
        at_send_command(s_ATChannels[channelID], "ATH", NULL);
    }

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
    rc->rat = getRadioFeatures(socket_id);
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
    char numToStr[ARRAY_SIZE] = {0};
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

    workMode = s_workMode[singleModeSim];
    s_workMode[singleModeSim] = s_workMode[s_multiModeSim];
    s_workMode[s_multiModeSim] = workMode;

#if defined (ANDROID_MULTI_SIM)
#if (SIM_COUNT == 2)
    snprintf(numToStr, sizeof(numToStr), "%d,%d", s_workMode[RIL_SOCKET_1],
            s_workMode[RIL_SOCKET_2]);
#endif
#else
    snprintf(numToStr, sizeof(numToStr), "%d,10", s_workMode[RIL_SOCKET_1]);
#endif

    RLOGD("setWorkMode: %s", numToStr);
    property_set(MODEM_WORKMODE_PROP, numToStr);
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
    int retryTimes = 0;
    char cmd[AT_COMMAND_LEN] = {0};
    char numToStr[ARRAY_SIZE];
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    if (isMaxRat(rc->rat)) {
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
    if (SIM_COUNT <= 2) {
        snprintf(cmd, sizeof(cmd), "AT+SPSWITCHDATACARD=%d,1", s_multiModeSim);
        at_send_command(s_ATChannels[channelID], cmd, NULL);
    }
    snprintf(numToStr, sizeof(numToStr), "%d", s_multiModeSim);
    property_set(PRIMARY_SIM_PROP, numToStr);
    RLOGD("applySetLTERadioCapability: multiModeSim %d", s_multiModeSim);

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

again:
    addAsyncCmdList(socket_id, t, respCmd, responseRc, 120);
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (err < 0 || p_response->success == 0) {
        if (retryTimes < 3 && p_response != NULL &&
                strcmp(p_response->finalResponse, "+CME ERROR: 3") == 0) {
            RLOGE("AT+SPTESTMODE return +CME ERROR: 3, try again");
            retryTimes++;
            AT_RESPONSE_FREE(p_response);
            removeAsyncCmdList(t, respCmd);
            sleep(3);
            goto again;
        } else {
            removeAsyncCmdList(t, respCmd);
            goto error;
        }
    }
    AT_RESPONSE_FREE(p_response);
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
            s_radioAccessFamily[socket_id] = rc.rat;
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
            RIL_onRequestComplete(t, RIL_E_INVALID_STATE, NULL, 0);
            break;
    }

exit:
    free(responseRc);
}

void requestUpdateOperatorName(int channelID, void *data, size_t datalen,
                               RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err = -1;
    char *plmn = (char *)data;
    char operatorName[ARRAY_SIZE] = {0};
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    memset(operatorName, 0, sizeof(operatorName));
    err = updatePlmn(socket_id, (const char *)plmn, operatorName,
                     sizeof(operatorName));
    RLOGD("updated plmn = %s, opeatorName = %s", plmn, operatorName);
    if ((err == 0) && strcmp(operatorName, "")) {
        MUTEX_ACQUIRE(s_operatorInfoListMutex[socket_id]);
        OperatorInfoList *pList = s_operatorInfoList[socket_id].next;
        OperatorInfoList *next;
        while (pList != &s_operatorInfoList[socket_id]) {
            next = pList->next;
            if (strcmp(plmn, pList->plmn) == 0) {
                RLOGD("find the plmn, remove it from s_operatorInfoList[%d]!",
                        socket_id);
                pList->next->prev = pList->prev;
                pList->prev->next = pList->next;
                pList->next = NULL;
                pList->prev = NULL;

                free(pList->plmn);
                free(pList->operatorName);
                free(pList);
                break;
            }
            pList = next;
        }
        MUTEX_RELEASE(s_operatorInfoListMutex[socket_id]);
        addToOperatorInfoList(plmn, operatorName,
                                  &s_operatorInfoList[socket_id],
                                  &s_operatorInfoListMutex[socket_id]);
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);

        RIL_onUnsolicitedResponse(
                RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED,
                NULL, 0, socket_id);
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
}

void requestStartNetworkScan(int channelID, void *data, size_t datalen,
                             RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    uint32_t i, j, k;
    int type, interval, access, err;
    char bands[AT_COMMAND_LEN] = {0};
    char channels[AT_COMMAND_LEN] = {0};
    char cmd[AT_COMMAND_LEN] = {0};
    ATResponse *p_response = NULL;
    memset(cmd, 0, sizeof(cmd));

    RIL_NetworkScanRequest *p_scanRequest = (RIL_NetworkScanRequest *)data;
    RIL_RadioAccessSpecifier *p_accessSpecifier = NULL;

    type = p_scanRequest->type;
    interval = p_scanRequest->interval;

    for (i = 0; i < p_scanRequest->specifiers_length; i++) {
        memset(bands, 0, sizeof(bands));
        memset(channels, 0, sizeof(channels));
        access = p_scanRequest->specifiers[i].radio_access_network;
        p_accessSpecifier = &(p_scanRequest->specifiers[i]);
        if (p_accessSpecifier->bands_length <= 0) {
            snprintf(bands, sizeof(bands), "");
        } else {
            switch (access) {
            case GERAN:
                snprintf(bands, sizeof(bands), "%d", p_accessSpecifier->bands.geran_bands[0]);
                for (j = 1; j < p_accessSpecifier->bands_length; j++) {
                    snprintf(bands, sizeof(bands), "%s,%d", bands, p_accessSpecifier->bands.geran_bands[j]);
                }
                break;
            case UTRAN:
                snprintf(bands, sizeof(bands), "%d", p_accessSpecifier->bands.utran_bands[0]);
                for (j = 1; j < p_accessSpecifier->bands_length; j++) {
                    snprintf(bands, sizeof(bands), "%s,%d", bands, p_accessSpecifier->bands.utran_bands[j]);
                }
                break;
            case EUTRAN:
                snprintf(bands, sizeof(bands), "%d", p_accessSpecifier->bands.eutran_bands[0]);
                for (j = 1; j < p_accessSpecifier->bands_length; j++) {
                    snprintf(bands, sizeof(bands), "%s,%d", bands, p_accessSpecifier->bands.eutran_bands[j]);
                }
                break;
            default:
                break;
            }
        }
        if (p_accessSpecifier->channels_length <= 0) {
            snprintf(channels, sizeof(channels), "");
        } else {
            snprintf(channels, sizeof(channels), "%d", p_accessSpecifier->channels[0]);
            for (k = 1; k < p_accessSpecifier->channels_length; k++) {
                snprintf(channels, sizeof(channels), "%s,%d", channels, p_accessSpecifier->channels[k]);
            }
        }
        RLOGD("network scan: bands = %s, channels = %s  i = %d", bands, channels, i);
        if (i > 0) {
            snprintf(cmd, sizeof(cmd), "%s,%d,\"%s\",\"%s\"", cmd, access, bands, channels);
        } else {
            snprintf(cmd, sizeof(cmd), "AT+SPFREQSCAN=%d,\"%s\",\"%s\"", access, bands, channels);
        }
    }

    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (err < 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    }
    AT_RESPONSE_FREE(p_response);
}

void requestStopNetworkScan(int channelID, void *data, size_t datalen,
                            RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    at_send_command(s_ATChannels[channelID], "AT+SAC", NULL);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
}

void requestSetLocationUpdates(int channelID, void *data, size_t datalen,
        RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int enable = ((int *)data)[0];
    if (enable == 0) {
        at_send_command(s_ATChannels[channelID], "AT+CREG=1", NULL);
    } else {
        at_send_command(s_ATChannels[channelID], "AT+CREG=2", NULL);
    }
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
}

void requestSetEmergencyOnly(int channelID, void *data, size_t datalen,
                             RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err = -1;
    int value = -1;
    char cmd[AT_COMMAND_LEN] = {0};
    bool isRadioStateOn = false;
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    value = ((int *)data)[0];
    snprintf(cmd, sizeof(cmd), "AT+SPECCMOD=%d", value);

    if (value == 1 || value == 0) {
        err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    } else {
        RLOGE("Invalid param value: %d", value);
        goto error;
    }

    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    if (isRadioOn(channelID) == 1) {
        isRadioStateOn = true;
    }

    if (value == 0 && isRadioStateOn) {
        RLOGD("restart protocol stack");
        at_send_command(s_ATChannels[channelID], "AT+SFUN=5", NULL);
        at_send_command(s_ATChannels[channelID], "AT+SFUN=4", NULL);
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
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
        case RIL_REQUEST_VOICE_RADIO_TECH:
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
            s_setNetworkId = getSocketIdByChannelID (channelID);
            p_response = NULL;
            err = at_send_command(s_ATChannels[channelID], "AT+COPS=0",
                                  &p_response);
            if (err >= 0 && p_response->success) {
                AT_RESPONSE_FREE(p_response);
                err = at_send_command(s_ATChannels[channelID], "AT+CGAUTO=1",
                                      &p_response);
            }
            if (err < 0 || p_response->success == 0) {
                goto error;
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
                at_response_free(p_response);
                s_setNetworkId = -1;
                break;
            }
        error:
            if (p_response != NULL &&
                    !strcmp(p_response->finalResponse, "+CME ERROR: 30")) {
                RIL_onRequestComplete(t, RIL_E_ILLEGAL_SIM_OR_ME, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            }
            at_response_free(p_response);
            s_setNetworkId = -1;
            break;
        }
        case RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL:
            s_setNetworkId = getSocketIdByChannelID (channelID);
            requestNetworkRegistration(channelID, data, datalen, t);
            s_setNetworkId = -1;
            break;
        case RIL_REQUEST_QUERY_AVAILABLE_NETWORKS: {
            s_manualSearchNetworkId = getSocketIdByChannelID (channelID);
            requestNetworkList(channelID, data, datalen, t);
            s_manualSearchNetworkId = -1;
            break;
        }
        case RIL_REQUEST_RESET_RADIO:
            requestResetRadio(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_SET_BAND_MODE:
            requestSetBandMode(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE:
            requestGetBandMode(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE:
        case RIL_EXT_REQUEST_SET_PREFERRED_NETWORK_TYPE: {
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
        case RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE:
        case RIL_EXT_REQUEST_GET_PREFERRED_NETWORK_TYPE: {
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
        case RIL_REQUEST_SET_LOCATION_UPDATES:
            requestSetLocationUpdates(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_GET_CELL_INFO_LIST:
            requestGetCellInfoList(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_SET_UNSOL_CELL_INFO_LIST_RATE:
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;
        case RIL_REQUEST_SHUTDOWN:
        case RIL_EXT_REQUEST_SHUTDOWN:
            requestShutdown(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_GET_RADIO_CAPABILITY:
            requestGetRadioCapability(channelID, data, datalen, t);
            break;
#if defined (ANDROID_MULTI_SIM)
        case RIL_REQUEST_SET_RADIO_CAPABILITY: {
            char prop[PROPERTY_VALUE_MAX];

            property_get(FIXED_SLOT_PROP, prop, "false");
            if (strcmp(prop, "true") == 0) {
                RIL_onRequestComplete(t, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
            } else {
                requestSetRadioCapability(channelID, data, datalen, t);
            }
            break;
        }
#endif
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
        case RIL_EXT_REQUEST_UPDATE_OPERATOR_NAME: {
            requestUpdateOperatorName(channelID, data, datalen, t);
            break;
        }
        case RIL_REQUEST_START_NETWORK_SCAN: {
            requestStartNetworkScan(channelID, data, datalen, t);
            break;
        }
        case RIL_REQUEST_STOP_NETWORK_SCAN: {
            requestStopNetworkScan(channelID, data, datalen, t);
            break;
        }
        case RIL_EXT_REQUEST_SET_EMERGENCY_ONLY: {
            pthread_mutex_lock(&s_radioPowerMutex[socket_id]);
            requestSetEmergencyOnly(channelID, data, datalen, t);
            pthread_mutex_unlock(&s_radioPowerMutex[socket_id]);
            break;
        }
        default:
            return 0;
    }

    return 1;
}

static void radioPowerOnTimeout(void *param) {
    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    if ((int)socket_id < 0 || (int)socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        return;
    }
    int channelID = getChannel(socket_id);
    setRadioState(channelID, RADIO_STATE_SIM_NOT_READY);
    putChannel(channelID);
}

static void setPropForAsync(void *param) {
    SetPropPara *propPara = (SetPropPara *)param;
    if (propPara == NULL) {
        RLOGE("Invalid param");
        return;
    }
    RLOGD("setprop socketId:%d,name:%s value:%s mutex:%p",
            propPara->socketId, propPara->propName, propPara->propValue, propPara->mutex);
    pthread_mutex_lock(propPara->mutex);
    setProperty(propPara->socketId, propPara->propName, propPara->propValue);
    pthread_mutex_unlock(propPara->mutex);
    RLOGD("setprop complete");
    free(propPara->propName);
    free(propPara->propValue);
    free(propPara);
}

static void processNetworkScanResults(char *line, RIL_SOCKET_ID socket_id) {
    int err = -1;
    int rat = -1, cell_num = 0, bsic = 0;
    int current = 0, skip;
    char *tmp = NULL;
    int rsrp = 0, rsrq = 0;
    RIL_NetworkScanResult *scanResult = NULL;

    tmp = line;
    if (tmp == NULL) {
        RLOGE("Invalid param, return");
        return;
    }

    // +SPFREQSCAN: 255 /+SPFREQSCAN: 254 --complete
    // +SPFREQSCAN: 0-2-1,20,9,11,460,44,2,1-5,512,9,11,460,44,2,1
    // +SPFREQSCAN: 1-1-2,10700,200,9,-27,460,44,2,1
    // +SPFREQSCAN: 2-1-1,9596,0,62,-96,460,44,2,1
    // +SPFREQSCAN: 3-3-0,0,38000,-8600,-1500,460,44,2,1-2,2,38200,-12100,-1500,460,44,2,2-6,6,1250,-12100,-1500,460,44,2,2
    // GSM: 0-cell_num-cid,arfcn,basic,rssi,mcc,mnc,mnc_digit,lac-cid,???.
    // WCDMA???1-cell_num-cid,uarfcn,psc,rssi,ecio,mcc,mnc,mnc_digital,lac-cid???
    // TD???2-cell_num-cid,uarfcn,psc,rssi,rscp,mcc,mnc,mnc_digit,lac-cid???
    // LTE???3-cell_num-cid,pcid,arfcn, rsrp,rsrq, mcc,mnc,mnc_digit,tac-cid???
    rat = atoi(tmp);
    RLOGD("network scan rat = %d", rat);
    scanResult = (RIL_NetworkScanResult *)calloc(1, sizeof(RIL_NetworkScanResult));
    if (rat < 0 || rat == 255 || rat == 254) {
        scanResult->status = COMPLETE;
        RIL_onUnsolicitedResponse(RIL_UNSOL_NETWORK_SCAN_RESULT, scanResult,
                                  sizeof(RIL_NetworkScanResult), socket_id);
        goto out;
    }
    strsep(&tmp, "-");
    if (tmp == NULL) {
        RLOGE("network scan param error");
        scanResult->status = COMPLETE;
        RIL_onUnsolicitedResponse(RIL_UNSOL_NETWORK_SCAN_RESULT, scanResult,
                                  sizeof(RIL_NetworkScanResult), socket_id);
        goto out;
    } else {
        cell_num = atoi(tmp);
        RLOGD("network scan cell_num = %d", cell_num);
    }
    scanResult->status = PARTIAL;
    scanResult->network_infos = (RIL_CellInfo_v12 *)calloc(cell_num, sizeof(RIL_CellInfo_v12));
    scanResult->network_infos_length = cell_num;

    switch (rat) {
    case 0:  // GSM
        for (current = 0; current < cell_num; current++) {
            tmp = strchr(tmp, '-');
            tmp++;
            scanResult->network_infos[current].cellInfoType = RIL_CELL_INFO_TYPE_GSM;

            err = at_tok_nextint(&tmp, &scanResult->network_infos[current].CellInfo.gsm.cellIdentityGsm.cid);
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &scanResult->network_infos[current].CellInfo.gsm.cellIdentityGsm.arfcn);
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &bsic);
            if (err < 0) goto out;
            scanResult->network_infos[current].CellInfo.gsm.cellIdentityGsm.bsic = (uint8_t)bsic;

            err = at_tok_nextint(&tmp, &scanResult->network_infos[current].CellInfo.gsm.signalStrengthGsm.signalStrength);
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &scanResult->network_infos[current].CellInfo.gsm.cellIdentityGsm.mcc);
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &scanResult->network_infos[current].CellInfo.gsm.cellIdentityGsm.mnc);
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &scanResult->network_infos[current].CellInfo.gsm.cellIdentityGsm.mnc_digit);
            if (err < 0) goto out;

            scanResult->network_infos[current].CellInfo.gsm.cellIdentityGsm.lac = atoi(tmp);

        }
        break;
    case 1:  // WCDMA
        for (current = 0; current < cell_num; current++) {
            scanResult->network_infos[current].cellInfoType = RIL_CELL_INFO_TYPE_WCDMA;
            tmp = strchr(tmp, '-');
            tmp++;
            err = at_tok_nextint(&tmp, &scanResult->network_infos[current].CellInfo.wcdma.cellIdentityWcdma.cid);
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &scanResult->network_infos[current].CellInfo.wcdma.cellIdentityWcdma.uarfcn);
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &scanResult->network_infos[current].CellInfo.wcdma.cellIdentityWcdma.psc);
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &scanResult->network_infos[current].CellInfo.wcdma.signalStrengthWcdma.signalStrength);
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp,&skip);  // ecio
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &scanResult->network_infos[current].CellInfo.wcdma.cellIdentityWcdma.mcc);
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &scanResult->network_infos[current].CellInfo.wcdma.cellIdentityWcdma.mnc);
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &scanResult->network_infos[current].CellInfo.wcdma.cellIdentityWcdma.mnc_digit);
            if (err < 0) goto out;

            scanResult->network_infos[current].CellInfo.wcdma.cellIdentityWcdma.lac = atoi(tmp);
        }
        break;
    case 2:  // TD
        for (current = 0; current < cell_num; current++) {
            scanResult->network_infos[current].cellInfoType = RIL_CELL_INFO_TYPE_CDMA;
            tmp = strchr(tmp, '-');
            tmp++;
            err = at_tok_nextint(&tmp, &scanResult->network_infos[current].CellInfo.tdscdma.cellIdentityTdscdma.cid);
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &skip);  // uarfcn
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &skip);  // psc
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &skip);  // rssi
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &scanResult->network_infos[current].CellInfo.tdscdma.signalStrengthTdscdma.rscp);
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &scanResult->network_infos[current].CellInfo.tdscdma.cellIdentityTdscdma.mcc);
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &scanResult->network_infos[current].CellInfo.tdscdma.cellIdentityTdscdma.mnc);
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &scanResult->network_infos[current].CellInfo.tdscdma.cellIdentityTdscdma.mnc_digit);
            if (err < 0) goto out;

            scanResult->network_infos[current].CellInfo.tdscdma.cellIdentityTdscdma.lac = atoi(tmp);
        }
        break;
    case 3: //LTE
        for (current = 0; current < cell_num; current++) {
            scanResult->network_infos[current].cellInfoType = RIL_CELL_INFO_TYPE_LTE;
            tmp = strchr(tmp, '-');
            tmp++;

            err = at_tok_nextint(&tmp, &scanResult->network_infos[current].CellInfo.lte.cellIdentityLte.ci);
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &scanResult->network_infos[current].CellInfo.lte.cellIdentityLte.pci);
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &scanResult->network_infos[current].CellInfo.lte.cellIdentityLte.earfcn);
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &rsrp);
            if (err < 0) goto out;
            scanResult->network_infos[current].CellInfo.lte.signalStrengthLte.rsrp = rsrp / (-100);

            err = at_tok_nextint(&tmp, &rsrq);
            if (err < 0) goto out;
            scanResult->network_infos[current].CellInfo.lte.signalStrengthLte.rsrq = rsrq / (-100);

            err = at_tok_nextint(&tmp, &scanResult->network_infos[current].CellInfo.lte.cellIdentityLte.mcc);
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &scanResult->network_infos[current].CellInfo.lte.cellIdentityLte.mnc);
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp, &scanResult->network_infos[current].CellInfo.lte.cellIdentityLte.mnc_digit);
            if (err < 0) goto out;

            scanResult->network_infos[current].CellInfo.lte.cellIdentityLte.tac = atoi(tmp);
        }
        break;
    default:
        break;
    }

    RIL_onUnsolicitedResponse(RIL_UNSOL_NETWORK_SCAN_RESULT, scanResult,
                              sizeof(RIL_NetworkScanResult), socket_id);

out:
    free(scanResult->network_infos);
    free(scanResult);
}
int processNetworkUnsolicited(RIL_SOCKET_ID socket_id, const char *s) {
    char *line = NULL;
    int err;

    if (strStartsWith(s, "+CSQ:")) {
        if (s_isLTE) {
            goto out;
        }

        RIL_SignalStrength_v10 response_v10;
        char *tmp;
        char newLine[AT_COMMAND_LEN];

        line = strdup(s);
        tmp = line;

        pthread_mutex_lock(&s_signalProcessMutex);
        pthread_cond_signal(&s_signalProcessCond);
        pthread_mutex_unlock(&s_signalProcessMutex);

        err = csq_unsol_rsp(tmp, socket_id, newLine);
        if (err == 0) {
            RIL_SIGNALSTRENGTH_INIT(response_v10);

            tmp = newLine;
            at_tok_start(&tmp);

            err = at_tok_nextint(&tmp,
                    &(response_v10.GW_SignalStrength.signalStrength));
            if (err < 0) goto out;

            err = at_tok_nextint(&tmp,
                    &(response_v10.GW_SignalStrength.bitErrorRate));
            if (err < 0) goto out;

            RIL_onUnsolicitedResponse(RIL_UNSOL_SIGNAL_STRENGTH, &response_v10,
                                      sizeof(RIL_SignalStrength_v10), socket_id);
        }
    } else if (strStartsWith(s, "+CESQ:")) {
        if (!strcmp(s_modem, "t") || !strcmp(s_modem, "w")) {
            goto out;
        }

        RIL_SignalStrength_v10 response_v10;
        char *tmp;
        int skip;
        int response[6] = {-1, -1, -1, -1, -1, -1};
        char newLine[AT_COMMAND_LEN];

        line = strdup(s);
        tmp = line;

        pthread_mutex_lock(&s_signalProcessMutex);
        pthread_cond_signal(&s_signalProcessCond);
        pthread_mutex_unlock(&s_signalProcessMutex);

        err = cesq_unsol_rsp(tmp, socket_id, newLine);
        if (err == 0) {
            RIL_SIGNALSTRENGTH_INIT(response_v10);

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
                response_v10.GW_SignalStrength.signalStrength = response[0];
            }
            if (response[2] != -1 && response[2] != 255) {
                response_v10.GW_SignalStrength.signalStrength = response[2];
            }
            if (response[5] != -1 && response[5] != 255 && response[5] != -255) {
                response_v10.LTE_SignalStrength.rsrp = response[5];
            }
            RIL_onUnsolicitedResponse(RIL_UNSOL_SIGNAL_STRENGTH, &response_v10,
                                      sizeof(RIL_SignalStrength_v10), socket_id);
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
//        char *tmp;
//        int cid, propNameLen, propValueLen;
//        char phy_cellid[ARRAY_SIZE];
//        line = strdup(s);
//        tmp = line;
//        at_tok_start(&tmp);
//        err = at_tok_nexthexint(&tmp, &cid);
//        if (err < 0) {
//            RLOGD("get physicel cell id fail");
//            goto out;
//        }
//        snprintf(phy_cellid, sizeof(phy_cellid), "%d", cid);
//        SetPropPara *cellIdPara = (SetPropPara *)calloc(1, sizeof(SetPropPara));
//        propNameLen = strlen(PHYSICAL_CELLID_PROP) + 1;
//        propValueLen = strlen(phy_cellid) + 1;
//        cellIdPara->socketId = socket_id;
//        cellIdPara->propName =
//                (char *)calloc(propNameLen, sizeof(char));
//        cellIdPara->propValue =
//                (char *)calloc(propValueLen, sizeof(char));
//        memcpy(cellIdPara->propName, PHYSICAL_CELLID_PROP, propNameLen);
//        memcpy(cellIdPara->propValue, phy_cellid, propValueLen);
//        cellIdPara->mutex = &s_physicalCellidMutex;
//        pthread_t tid;
//        pthread_attr_t attr;
//        pthread_attr_init(&attr);
//        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
//        pthread_create(&tid, &attr, (void *)setPropForAsync, (void *)cellIdPara);
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

        snprintf(s_nitzOperatorInfo[socket_id], sizeof(s_nitzOperatorInfo[socket_id]),
                "%s%s,%s,%s", mcc, mnc, fullName, shortName);
        RLOGD("s_nitzOperatorInfo[%d] = %s", socket_id, s_nitzOperatorInfo[socket_id]);

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
    } else if (strStartsWith(s, "+SPDSDASTATUS:")) {
        int response = 0;
        char *tmp = NULL;
        char status[AT_COMMAND_LEN] = {0};
        int propNameLen, propValueLen;
        line = strdup(s);
        tmp = line;

        err = at_tok_start(&tmp);
        if (err < 0) goto out;

        err = at_tok_nextint(&tmp, &response);
        if (err < 0) goto out;
        //TO DO :add Unsol
    } else if (strStartsWith(s, "+SPFREQSCAN:")) {
        char *tmp = NULL;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);
        skipWhiteSpace(&tmp);
        if (tmp == NULL) {
            RLOGE("network scan param is NULL");
            goto out;
        }
        processNetworkScanResults(tmp, socket_id);
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
    int noSigChange = 0;  // numbers of SIM cards with constant signal value
    int noSimCard = 0;  // numbers of SIM slot without card

    memset(sample_rsrp_sim, 0, sizeof(int) * SIM_COUNT * SIG_POOL_SIZE);
    memset(sample_rscp_sim, 0, sizeof(int) * SIM_COUNT * SIG_POOL_SIZE);
    memset(sample_rxlev_sim, 0, sizeof(int) * SIM_COUNT * SIG_POOL_SIZE);
    memset(sample_rssi_sim, 0, sizeof(int) * SIM_COUNT * SIG_POOL_SIZE);

    if (!s_isLTE) {
        MAXSigCount = SIG_POOL_SIZE - 1;
    }

    while (1) {
        noSigChange = 0;
        noSimCard = 0;
        for (sim_index = 0; sim_index < SIM_COUNT; sim_index++) {
            if (s_isSimPresent[sim_index] != PRESENT) {  // no sim card
                noSimCard++;
                continue;
            }
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
                noSigChange++;
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
                RIL_SignalStrength_v10 response_v10;
                int response[6] = {-1, -1, -1, -1, -1, -1};
                RIL_SIGNALSTRENGTH_INIT(response_v10);

                response[0] = rxlev_value;
                response[1] = ber[sim_index];
                response[2] = rscp_value;
                response[5] = rsrp_value;

                if (response[0] != -1 && response[0] != 99) {
                    response_v10.GW_SignalStrength.signalStrength = response[0];
                }
                if (response[2] != -1 && response[2] != 255) {
                    response_v10.GW_SignalStrength.signalStrength = response[2];
                }
                if (response[5] != -1 && response[5] != 255 && response[5] != -255) {
                    response_v10.LTE_SignalStrength.rsrp = response[5];
                }
                RLOGD("rxlev[%d]=%d, rscp[%d]=%d, rsrp[%d]=%d", sim_index,
                     rxlev_value, sim_index, rscp_value, sim_index, rsrp_value);
                RIL_onUnsolicitedResponse(RIL_UNSOL_SIGNAL_STRENGTH,
                        &response_v10, sizeof(RIL_SignalStrength_v10), sim_index);
            } else {  // w/t
                RIL_SignalStrength_v10 responseV10;
                RIL_SIGNALSTRENGTH_INIT(responseV10);
                responseV10.GW_SignalStrength.signalStrength = rscp_value;
                responseV10.GW_SignalStrength.bitErrorRate = ber[sim_index];
                RLOGD("rssi[%d]=%d", sim_index, rscp_value);
                RIL_onUnsolicitedResponse(RIL_UNSOL_SIGNAL_STRENGTH,
                        &responseV10, sizeof(RIL_SignalStrength_v10), sim_index);
            }
        }
        sleep(1);
        if ((noSigChange + noSimCard == SIM_COUNT) || (s_screenState == 0)) {
            pthread_mutex_lock(&s_signalProcessMutex);
            pthread_cond_wait(&s_signalProcessCond, &s_signalProcessMutex);
            pthread_mutex_unlock(&s_signalProcessMutex);
        }
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
                ATResponse *sp_response = at_response_new();
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
        ATResponse *sp_response = at_response_new();
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
