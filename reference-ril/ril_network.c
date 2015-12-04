/**
 * ril_network.c --- Network-related requests process functions implementation
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */
#define LOG_TAG "RIL"

#include "reference-ril.h"
#include "ril_network.h"
#include "ril_sim.h"
#include "ril_data.h"

#define SIM_PRESENT_PROP        "ril.sim.present"
#define MODEM_WORKMODE_PROP     "persist.radio.modem.workmode"

LTE_PS_REG_STATE s_LTERegState[SIM_COUNT] = {
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

static SimBusy s_simBusy[SIM_COUNT] = {
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

static pthread_mutex_t s_simPresentMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_oneSimOnlyMutex = PTHREAD_MUTEX_INITIALIZER;
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

bool s_oneSimOnly = false;
int s_in4G[SIM_COUNT];
int s_workMode[SIM_COUNT] = {0};
int s_desiredRadioState[SIM_COUNT] = {0};

void setSimPresent(RIL_SOCKET_ID socket_id, bool hasSim) {
    RLOGD("setSimPresent hasSim = %d", hasSim);
    pthread_mutex_lock(&s_simPresentMutex);
    if (hasSim) {
        setProperty(socket_id, SIM_PRESENT_PROP, "1");
    } else {
        setProperty(socket_id, SIM_PRESENT_PROP, "0");
    }
    pthread_mutex_unlock(&s_simPresentMutex);
}

int isSimPresent(RIL_SOCKET_ID socket_id) {
    int hasSim = 0;
    char prop[PROPERTY_VALUE_MAX];
    pthread_mutex_lock(&s_simPresentMutex);
    getProperty(socket_id, SIM_PRESENT_PROP, prop, "0");
    pthread_mutex_unlock(&s_simPresentMutex);

    hasSim = atoi(prop);
    return hasSim;
}

int getWorkMode(RIL_SOCKET_ID socket_id) {
    int workMode = 0;
    char prop[PROPERTY_VALUE_MAX] = {0};

    getProperty(socket_id, MODEM_WORKMODE_PROP, prop, "254");

    workMode = atoi(prop);
    if (!strcmp(s_modem, "tl") || !strcmp(s_modem, "lf")) {
        if ((workMode == 13) || (workMode == 14)) {
              workMode = 255;
        }
    }
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
                strcpy(strFormatter, cmd);
                strcat(strFormatter, ",%d");
                snprintf(cmd, size, strFormatter, 254);
                RLOGD("buildWorkModeCmd cmd(SingleSim): %s", cmd);
            }
        } else {
            strcpy(strFormatter, cmd);
            strcat(strFormatter, ",%d");
            snprintf(cmd, size, strFormatter, getWorkMode(simId));
            RLOGD("buildWorkModeCmd cmd: %s", cmd);
        }
    }
}

void initSIMPresentState() {
    int simId = 0;
    int presentSIMCount = 0;

    for (simId = 0; simId < SIM_COUNT; simId++) {
        if (isSimPresent(simId) == 1) {
            ++presentSIMCount;
        }
    }
    pthread_mutex_lock(&s_oneSimOnlyMutex);
    s_oneSimOnly = presentSIMCount == 1 ? true : false;
    pthread_mutex_unlock(&s_oneSimOnlyMutex);
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
        default:
            out_response = 0;    /* UNKNOWN */
            break;
    }
    return out_response;
}

static int getCeMode(RIL_SOCKET_ID socket_id) {
    int cemode = 0;
    switch (getWorkMode(socket_id)) {
        case 0:
        case 1:
        case 2:
        case 3:
            cemode = 0;
            break;
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
            cemode = 1;
            break;
        default:
            cemode = 0;
            break;
    }
    return cemode;
}

static void setCeMode(int channelID) {
    char cmd[AT_COMMAND_LEN] = {0};
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
    sprintf(cmd, "AT+CEMODE=%d", getCeMode(socket_id));
    RLOGD("setCeMode: %s", cmd);
    at_send_command(s_ATChannels[channelID], cmd, NULL);
}

static void requestSignalStrength(int channelID, void *data,
                                      size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int err;
    char *line;
    ATResponse *p_response = NULL;
    RIL_SignalStrength_v6 response_v6;

    RIL_SIGNALSTRENGTH_INIT(response_v6);

    err = at_send_command_singleline(s_ATChannels[channelID],
            "AT+CSQ", "+CSQ:", &p_response);
    if (err < 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &(response_v6.GW_SignalStrength.signalStrength));
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &(response_v6.GW_SignalStrength.bitErrorRate));
    if (err < 0) goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response_v6,
                          sizeof(RIL_SignalStrength_v6));
    at_response_free(p_response);
    return;

error:
    RLOGE("requestSignalStrength must never return an error when radio is on");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
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
    RIL_SignalStrength_v6 responseV6;

    RIL_SIGNALSTRENGTH_INIT(responseV6);

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+CESQ",
                                     "+CESQ:", &p_response);
    if (err < 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
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
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
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

    bool islte = isLte();

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
    for (p = line ; *p != '\0'; p++) {
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
        case 2: {  /* +CREG: <stat>, <lac>, <cid> */
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

    sprintf(res[0], "%d", response[0]);
    responseStr[0] = res[0];

    if (response[1] != -1) {
        sprintf(res[1], "%x", response[1]);
        responseStr[1] = res[1];
    }

    if (response[2] != -1) {
        sprintf(res[2], "%x", response[2]);
        responseStr[2] = res[2];
    }

    if (response[3] != -1) {
        response[3] = mapCgregResponse(response[3]);
        sprintf(res[3], "%d", response[3]);
        responseStr[3] = res[3];
    }

    if (islte && request == RIL_REQUEST_DATA_REGISTRATION_STATE) {
        RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
        if (response[0] == 1 || response[0] == 5) {
            pthread_mutex_lock(&s_LTEAttachMutex[socket_id]);
            if (s_LTERegState[socket_id] == STATE_OUT_OF_SERVICE) {
                s_LTERegState[socket_id] = STATE_IN_SERVICE;
            }
            pthread_mutex_unlock(&s_LTEAttachMutex[socket_id]);
            if (response[3] == 14) {
                s_in4G[socket_id] = 1;
            } else {
                s_in4G[socket_id] = 0;
            }
        } else {
            pthread_mutex_lock(&s_LTEAttachMutex[socket_id]);
            if (s_LTERegState[socket_id] == STATE_IN_SERVICE) {
                s_LTERegState[socket_id] = STATE_OUT_OF_SERVICE;
            }
            pthread_mutex_unlock(&s_LTEAttachMutex[socket_id]);
            s_in4G[socket_id] = 0;
        }
    }

    if (request == RIL_REQUEST_VOICE_REGISTRATION_STATE) {
        sprintf(res[4], "0");
        responseStr[7] = res[4];
        RIL_onRequestComplete(t, RIL_E_SUCCESS, responseStr,
                              15 * sizeof(char *));
    } else if (request == RIL_REQUEST_DATA_REGISTRATION_STATE) {
        sprintf(res[4], "3");
        responseStr[5] = res[4];
        RIL_onRequestComplete(t, RIL_E_SUCCESS, responseStr,
                              6 * sizeof(char *));
    } else if (request == RIL_REQUEST_IMS_REGISTRATION_STATE) {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));
    }
    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static void requestOperator(int channelID, void *data, size_t datalen,
                                RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int err;
    int i;
    int skip;
    char *response[3];
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

    RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));
    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
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
    RLOGD("enter radio power on: %d, ", s_desiredRadioState[socket_id]);
    if (s_desiredRadioState[socket_id] == 0) {
#if defined (ANDROID_MULTI_SIM)
#if (SIM_COUNT == 2)
        if (!s_oneSimOnly && s_workMode[socket_id] == 10) {
            snprintf(cmd, sizeof(cmd), "AT+SPSWITCHDATACARD=%d,0", socket_id);
            err = at_send_command(s_ATChannels[channelID], cmd, NULL);
        }
#endif
#endif
        int sim_status = getSIMStatus(channelID);
        setSimPresent(socket_id, sim_status == SIM_ABSENT ? false: true);

        /* The system ask to shutdown the radio */
        err = at_send_command(s_ATChannels[channelID], "AT+SFUN=5", &p_response);
        if (err < 0 || p_response->success == 0) {
            goto error;
        }

        for (i = 0; i < MAX_PDP; i++) {
            if (s_PDP[i].cid > 0) {
                RLOGD("s_PDP[%d].state = %d", i, s_PDP[i].state);
                putPDP(i);
            }
        }
        setRadioState(channelID, RADIO_STATE_OFF);
    } else if (s_desiredRadioState[socket_id] > 0 &&
                s_radioState[socket_id] == RADIO_STATE_OFF) {
#if defined (ANDROID_MULTI_SIM)
#if (SIM_COUNT == 2)
        if (socket_id == RIL_SOCKET_1) {
            if (isSimPresent(socket_id) == 0 &&
                isSimPresent(RIL_SOCKET_2) == 1) {
                RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED,
                                          NULL, 0, socket_id);
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
                return;
           }
       } else if (socket_id == RIL_SOCKET_2) {
           if (isSimPresent(socket_id) == 0) {
               RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED,
                                         NULL, 0, socket_id);
               RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
               return;
           }
        }
#endif
#endif

        s_workMode[socket_id] = getWorkMode(socket_id);
        initSIMPresentState();
        RLOGD("s_oneSimOnly = %d", s_oneSimOnly);
        if (isLte() || !strcmp(s_modem, "w")) {
            buildWorkModeCmd(cmd, sizeof(cmd));
            at_send_command(s_ATChannels[channelID], cmd, NULL);
        }

        if (isLte()) {
            setCeMode(channelID);

            p_response = NULL;
            if (s_oneSimOnly) {
                err = at_send_command(s_ATChannels[channelID], "AT+SAUTOATT=1",
                                      &p_response);
                if (err < 0 || p_response->success == 0) {
                    RLOGE("GPRS auto attach failed!");
                }
                AT_RESPONSE_FREE(p_response);
            }
#if defined (ANDROID_MULTI_SIM)
            else {
                if (s_workMode[socket_id] == 10) {
                    RLOGD("socket_id = %d, s_dataAllowed = %d", socket_id,
                          s_dataAllowed[socket_id]);
                    snprintf(cmd, sizeof(cmd), "AT+SPSWITCHDATACARD=%d,%d",
                             socket_id, s_dataAllowed[socket_id]);
                    at_send_command(s_ATChannels[channelID], cmd, NULL);
                }
            }
#endif
        }

        err = at_send_command(s_ATChannels[channelID], "AT+SFUN=4", &p_response);

        if (err < 0|| p_response->success == 0) {
            /* Some stacks return an error when there is no SIM,
             * but they really turn the RF portion on
             * So, if we get an error, let's check to see if it
             * turned on anyway
             */
            if (s_simBusy[socket_id].s_sim_busy) {
                RLOGD("Now SIM busy. We create a thread to wait for CPIN READY, \
                      and then set radio on again.");
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
    char *network = (char *)data;

    ATResponse *p_response = NULL;

    if (network) {
        snprintf(cmd, sizeof(cmd), "AT+COPS=1,2,\"%s\"", network);
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
    tmp = (char *)malloc(count * sizeof(char) * 30);
    startTmp = tmp;

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
        if (cur[1] == NULL) {
            if (cur[0] != NULL) {
                cur[1] = cur[0];
            } else {
                cur[1] = tmp;
            }
        }
        strcpy(tmp, cur[1]);
        strcat(tmp, " ");
        RLOGD("requestNetworkList  tmp cur[1] = %s", tmp);
        // set act in cur[1], cur[1] = CMCC UTRAN
        strcat(tmp, actStr[act == 7 ? 3 : act]);
        RLOGD("requestNetworkList cur[1] act = %s", tmp);
        cur[1] = tmp;

        cur += 4;
        tmp += 30;
    }
    RIL_onRequestComplete(t, RIL_E_SUCCESS, responses,
                          count * 4 * sizeof(char *));
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

static void requestSetPreferredNetType(int channelID, void *data,
                                            size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err, type;
    char cmd[AT_COMMAND_LEN] = {0};
    ATResponse *p_response = NULL;

    if (isLte()) goto error;

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

    switch (((int *)data)[0]) {
        case 0:  // NETWORK_MODE_WCDMA_PREF
            type = 2;
            break;
        case 1:  // NETWORK_MODE_GSM_ONLY
            type = 13;
            break;
        case 2:  // NETWORK_MODE_WCDMA_ONLY or TDSCDMA ONLY
            if (!strcmp(s_modem, "t")) {
                type = 15;
            } else if (!strcmp(s_modem, "w")) {
                type = 14;
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

#if defined (ANDROID_MULTI_SIM)
    if (socket_id == RIL_SOCKET_1) {
        if (!isLte()) {
            at_send_command(s_ATChannels[channelID], "AT+SAUTOATT=1", NULL);
        }
        snprintf(cmd, sizeof(cmd), "AT^SYSCONFIG=%d,3,2,4", type);
        err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
        if (err < 0 || p_response->success == 0) {
            goto error;
        }
    }
#if (SIM_COUNT >= 2)
    else if (socket_id == RIL_SOCKET_2) {
        if (!isLte()) {
            if (isSimPresent(0) == 0) {
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
    }
#endif
#else
    snprintf(cmd, sizeof(cmd), "AT^SYSCONFIG=%d,3,2,4", type);
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }
#endif

    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);

error:
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

static void requestGetPreferredNetType(int channelID,
        void *data, size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int err, type;
    int response = 0;
    ATResponse *p_response = NULL;

    if (isLte()) goto error;

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

                    err = at_tok_nextstr(&line, &(NeighboringCell[current].cid));
                    if (err < 0) goto error;

                    err = at_tok_nextint(&line, &(NeighboringCell[current].rssi));
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
    RIL_CellInfo **response = NULL;

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
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        goto error;
    }

    line = p_response->p_intermediates->line;
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
    if (response != NULL) {
        for (i = 0; i < count; i++) {
            free(response[i]);
        }
        free(response);
    }
    return;

error:
    at_response_free(p_response);
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
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    if (s_radioState[socket_id] != RADIO_STATE_OFF) {
        err = at_send_command(s_ATChannels[channelID], "AT+SFUN=5", &p_response);
        AT_RESPONSE_FREE(p_response);
        err = at_send_command(s_ATChannels[channelID], "AT+SFUN=3", &p_response);
        setRadioState(channelID, RADIO_STATE_UNAVAILABLE);
    }

    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    return;
}

int processNetworkRequests(int request, void *data, size_t datalen,
                              RIL_Token t, int channelID) {
    int err;
    ATResponse *p_response = NULL;

    switch (request) {
        case RIL_REQUEST_SIGNAL_STRENGTH: {
            if (isLte()) {
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
        case RIL_REQUEST_RADIO_POWER:
            requestRadioPower(channelID, data, datalen, t);
            break;
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
        case RIL_REQUEST_QUERY_AVAILABLE_NETWORKS:
            requestNetworkList(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_RESET_RADIO:
            requestResetRadio(channelID, data, datalen, t);
            break;
        // TODO: for now, not realized
        // case RIL_REQUEST_SET_BAND_MODE:
        //    break;
        // case RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE:
        //    break;
        case RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE:
            requestSetPreferredNetType(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE:
            requestGetPreferredNetType(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_GET_NEIGHBORING_CELL_IDS:
            requestNeighboaringCellIds(channelID, data, datalen, t);
            break;
        // TODO: for now, not realized
        // case RIL_REQUEST_SET_LOCATION_UPDATES:
        //    break;
        // case RIL_REQUEST_VOICE_RADIO_TECH:
        //    break;
        case RIL_REQUEST_GET_CELL_INFO_LIST:
            requestGetCellInfoList(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_SET_UNSOL_CELL_INFO_LIST_RATE:
            // requestSetCellInfoListRate(data, datalen, t);
            // TODO: For now we'll save the rate
            // but no RIL_UNSOL_CELL_INFO_LIST messages will be sent.
            RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            break;
        case RIL_REQUEST_SHUTDOWN:
            requestShutdown(channelID, data, datalen, t);
            break;
        // TODO: for now, not realized
        // case RIL_REQUEST_GET_RADIO_CAPABILITY:
        //    break;
        // case RIL_REQUEST_SET_RADIO_CAPABILITY:
        //    break;
        default:
            return 0;
    }

    return 1;
}

int processNetworkUnsolicited(RIL_SOCKET_ID socket_id, const char *s) {
    char *line = NULL;
    int err;

    if (strStartsWith(s, "+CSQ:")) {
        RIL_SignalStrength_v6 responseV6;
        char *tmp;

        RLOGD("[unsl] +CSQ enter");
        if (isLte()) {
            RLOGD("for +CSQ, current is lte ril,do nothing");
            goto out;
        }

        RIL_SIGNALSTRENGTH_INIT(responseV6);
        line = strdup(s);
        tmp = line;

        at_tok_start(&tmp);

        err = at_tok_nextint(&tmp,
                             &(responseV6.GW_SignalStrength.signalStrength));
        if (err < 0) goto out;

        err = at_tok_nextint(&tmp,
                             &(responseV6.GW_SignalStrength.bitErrorRate));
        if (err < 0) goto out;

        RIL_onUnsolicitedResponse(RIL_UNSOL_SIGNAL_STRENGTH, &responseV6,
                                  sizeof(RIL_SignalStrength_v6), socket_id);
    } else if (strStartsWith(s, "+CESQ:")) {
        RIL_SignalStrength_v6 response_v6;
        char *tmp;
        int skip;
        int response[6] = {-1, -1, -1, -1, -1, -1};

        RLOGD("[unsl] +CESQ enter");
        if (!strcmp(s_modem, "t")) {
            RLOGD("for +CESQ, current is td ril, do nothing");
            goto out;
        }

        RIL_SIGNALSTRENGTH_INIT(response_v6);
        line = strdup(s);
        tmp = line;

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
    } else if (strStartsWith(s, "+CREG:") ||
                strStartsWith(s, "+CGREG:")) {
        RIL_onUnsolicitedResponse(
                RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED,
                NULL, 0, socket_id);
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

        if (isLte()) {
            if (commas == 0 && lteState == 0) {
                s_in4G[socket_id] = 0;
                s_LTEDetached[socket_id] = true;
            }
        }

        if (lteState == 1 || lteState == 5) {
            if (commas >= 3) {
                skipNextComma(&tmp);
                skipNextComma(&tmp);
                err = at_tok_nextint(&tmp, &netType);
                if (err < 0) goto out;
            }
            if (netType == 7) {
                s_in4G[socket_id] = 1;
            }
            RLOGD("netType is %d", netType);
            pthread_mutex_lock(&s_LTEAttachMutex[socket_id]);
            if (s_LTERegState[socket_id] == STATE_OUT_OF_SERVICE) {
                s_LTERegState[socket_id] = STATE_IN_SERVICE;
            }
            pthread_mutex_unlock(&s_LTEAttachMutex[socket_id]);
            RLOGD("s_LTERegState is IN SERVICE");
        } else {
            pthread_mutex_lock(&s_LTEAttachMutex[socket_id]);
            if (s_LTERegState[socket_id] == STATE_IN_SERVICE) {
                s_LTERegState[socket_id] = STATE_OUT_OF_SERVICE;
            }
            pthread_mutex_unlock(&s_LTEAttachMutex[socket_id]);
            RLOGD("s_LTERegState is OUT OF SERVICE.");
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
    } else {
        return 0;
    }

out:
    free(line);
    return 1;
}
