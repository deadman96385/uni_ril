/**
 * ril_stk.c --- STK-related requests process functions implementation
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */
#define LOG_TAG "RIL"

#include "sprd_ril.h"
#include "ril_stk.h"
#include "ril_network.h"
#include "ril_sim.h"
#include "channel_controller.h"
#include "ril_stk_parser.h"
#include "ril_stk_bip.h"
#include "ril_utils.h"

#define STK_BIP_MODE_PROP        "persist.vendor.radio.bipmode"

bool s_stkServiceRunning[SIM_COUNT];
static bool s_lunchOpenChannelDialog[SIM_COUNT];
static char *s_stkUnsolResponse[SIM_COUNT];

StkContextList *s_stkContextList[SIM_COUNT];
static int s_curBipChannelID[SIM_COUNT];
enum BipChannelState s_bipState[SIM_COUNT][MAX_BIP_CHANNELS];
pthread_mutex_t s_bipChannelMutex[SIM_COUNT] = {
            PTHREAD_MUTEX_INITIALIZER,
        #if (SIM_COUNT >= 2)
            PTHREAD_MUTEX_INITIALIZER,
        #if (SIM_COUNT >= 3)
            PTHREAD_MUTEX_INITIALIZER,
        #endif
        #if (SIM_COUNT >= 4)
            PTHREAD_MUTEX_INITIALIZER,
        #endif
        #endif
};

void freeStkContextList(StkContextList *stkContextList);

void onModemReset_Stk() {
    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;

    for (socket_id = RIL_SOCKET_1; socket_id < RIL_SOCKET_NUM; socket_id++) {
        s_stkServiceRunning[socket_id] = false;
        s_lunchOpenChannelDialog[socket_id] = false;

        free(s_stkUnsolResponse[socket_id]);
        s_stkUnsolResponse[socket_id] = NULL;

        StkContextList *pList = s_stkContextList[socket_id];
        StkContextList *next = NULL;
        while (pList != NULL) {
            next = pList->next;

            pList->next->prev = pList->prev;
            pList->prev->next = pList->next;
            pList->next = NULL;
            pList->prev = NULL;

            freeStkContextList(pList);

            pList = next;
        }
        s_stkContextList[socket_id] = NULL;

        s_curBipChannelID[socket_id] = 0;
        for (int i = 0; i < MAX_BIP_CHANNELS; i++) {
            s_bipState[socket_id][i] = BIP_CHANNEL_IDLE;
        }
    }
}

void freeStkContextList(StkContextList *stkContextList) {
    if (stkContextList == NULL) return;

    StkContext *pstkcontext = NULL;
    pstkcontext = stkContextList->pstkcontext;

    if (pstkcontext != NULL) {
        if (pstkcontext->recevieData != NULL) {
            free(pstkcontext->recevieData);
            pstkcontext->recevieData = NULL;
        }
        if (pstkcontext->pOCData != NULL) {
            if (pstkcontext->pOCData->channelData.text != NULL) {
                free(pstkcontext->pOCData->channelData.text);
                pstkcontext->pOCData->channelData.text = NULL;
            }
            free(pstkcontext->pOCData);
            pstkcontext->pOCData = NULL;
        }
        if (pstkcontext->pCStatus != NULL) {
            free(pstkcontext->pCStatus);
            pstkcontext->pCStatus = NULL;
        }
        if (pstkcontext->pSCData != NULL) {
            if (pstkcontext->pSCData->channelData.text != NULL) {
                free(pstkcontext->pSCData->channelData.text);
                pstkcontext->pSCData->channelData.text = NULL;
            }
            free(pstkcontext->pSCData);
            pstkcontext->pSCData = NULL;
        }
        if (pstkcontext->pRCData != NULL) {
            if (pstkcontext->pRCData->channelData.text != NULL) {
                free(pstkcontext->pRCData->channelData.text);
                pstkcontext->pRCData->channelData.text = NULL;
            }
            free(pstkcontext->pRCData);
            pstkcontext->pRCData = NULL;
        }
        if (pstkcontext->pCCData != NULL) {
            if (pstkcontext->pCCData->channelData.text != NULL) {
                free(pstkcontext->pCCData->channelData.text);
                pstkcontext->pCCData->channelData.text = NULL;
            }
            free(pstkcontext->pCCData);
            pstkcontext->pCCData = NULL;
        }

        if (pstkcontext->pCmdDet != NULL) {
            free(pstkcontext->pCmdDet);
            pstkcontext->pCmdDet = NULL;
        }

        free(pstkcontext);
        pstkcontext = NULL;
    }

    free(stkContextList);
    stkContextList = NULL;
}

void putBipChannel(int socket_id, int channelID) {
    if (channelID < 1 || channelID > MAX_BIP_CHANNELS) {
        return;
    }

    pthread_mutex_lock(&s_bipChannelMutex[socket_id]);
    if (s_bipState[socket_id][channelID-1] != BIP_CHANNEL_BUSY) {
        goto done;
    }
    s_bipState[socket_id][channelID-1] = BIP_CHANNEL_IDLE;

done:
    RLOGD("put BipChannel%d", channelID);
    pthread_mutex_unlock(&s_bipChannelMutex[socket_id]);
}

int getBipChannel(int socket_id) {
    int channelID = 0;
    int firstChannel = 0;
    int lastChannel = MAX_BIP_CHANNELS;

    for (;;) {
        for (channelID = firstChannel; channelID < lastChannel; channelID++) {
            pthread_mutex_lock(&s_bipChannelMutex[socket_id]);
            if (s_bipState[socket_id][channelID] == BIP_CHANNEL_IDLE) {
                s_bipState[socket_id][channelID] = BIP_CHANNEL_BUSY;
                RLOGD("get BipChannel%d", channelID+1);
                pthread_mutex_unlock(&s_bipChannelMutex[socket_id]);
                return channelID+1;
            }
            pthread_mutex_unlock(&s_bipChannelMutex[socket_id]);
        }
        usleep(5000);
    }

}

int initStkContext(StkContext *pstkcontext, int socket_id, OpenChannelData *openChannelData, CommandDetails *cmdDet) {
    RLOGD("initStkContext");
    if (pstkcontext == NULL) {
        RLOGE("pstkcontext is NULL");
        return 0;
    }

    static pthread_mutex_t tempInitMutex = PTHREAD_MUTEX_INITIALIZER;
    pstkcontext->openchannelCid = -1;
    pstkcontext->tcpSocket = -1;
    pstkcontext->udpSocket = -1;
    pstkcontext->threadInLoop = false;
    pstkcontext->needRuning = false;
    pstkcontext->recevieData = NULL;

    memcpy(&pstkcontext->writeStkMutex, &tempInitMutex, sizeof(pthread_mutex_t));
    memcpy(&pstkcontext->receiveDataMutex, &tempInitMutex, sizeof(pthread_mutex_t));
    memcpy(&pstkcontext->closeChannelMutex, &tempInitMutex, sizeof(pthread_mutex_t));
    memcpy(&pstkcontext->bipTcpUdpMutex, &tempInitMutex, sizeof(pthread_mutex_t));

    memset(pstkcontext->sendDataStr, 0 , sizeof(pstkcontext->sendDataStr));
    memset(pstkcontext->sendDataStorer, 0 , sizeof(pstkcontext->sendDataStorer));

    pstkcontext->pOCData = NULL;
    pstkcontext->pCStatus = NULL;
    pstkcontext->pSCData = NULL;
    pstkcontext->pRCData = NULL;
    pstkcontext->pCCData = NULL;

    pstkcontext->pCmdDet = NULL;

    pstkcontext->phone_id = socket_id;
    pstkcontext->channel_id = getBipChannel(socket_id);

    s_curBipChannelID[socket_id] = pstkcontext->channel_id;

    pstkcontext->pOCData = openChannelData;
    pstkcontext->pCmdDet = cmdDet;

    RLOGD("initStkContext pstkcontext->channel_id = %d", pstkcontext->channel_id);
    RLOGD("initStkContext pCmdDet->commandNumber = %d", pstkcontext->pCmdDet->commandNumber);
    RLOGD("initStkContext pCmdDet->typeOfCommand = %d", pstkcontext->pCmdDet->typeOfCommand);
    RLOGD("initStkContext pCmdDet->commandQualifier = %d", pstkcontext->pCmdDet->commandQualifier);

    return 1;
}

int addToStkContextList(StkContext *pstkcontext, int socket_id) {
    if (pstkcontext != NULL) {
        if (s_stkContextList[socket_id] == NULL) {
            RLOGD("init s_stkContextList");
            s_stkContextList[socket_id] = (StkContextList *)calloc(1, sizeof(StkContextList));
            if (s_stkContextList[socket_id] == NULL) {
                RLOGE("s_stkContextList malloc error");
                return 0;
            }
            s_stkContextList[socket_id]->pstkcontext = pstkcontext;
            s_stkContextList[socket_id]->next = s_stkContextList[socket_id];
            s_stkContextList[socket_id]->prev = s_stkContextList[socket_id];
        } else {
            RLOGD("add s_stkContextList");
            StkContextList *tmpList = NULL;
            tmpList = (StkContextList *)calloc(1, sizeof(StkContextList));
            if (tmpList == NULL) {
                RLOGE("tmpList malloc error");
                return 0;
            }
            tmpList->pstkcontext = pstkcontext;
            tmpList->next = s_stkContextList[socket_id];
            tmpList->prev = s_stkContextList[socket_id]->prev;
            tmpList->prev->next = tmpList;
            s_stkContextList[socket_id]->prev = tmpList;
        }
    } else {
        RLOGE("pstkcontext is NULL");
        return 0;
    }

    return 1;
}

int removeFromStkContextList(StkContext *pstkcontext, int socket_id) {
    if (s_stkContextList[socket_id] != NULL && pstkcontext != NULL) {
        StkContextList *tmpList = s_stkContextList[socket_id];
        if (tmpList == tmpList->next) {
            if (pstkcontext != tmpList->pstkcontext) {
                return 0;
            } else {
                freeStkContextList(tmpList);
                s_stkContextList[socket_id] = NULL;
            }
        } else {
            if (pstkcontext != tmpList->pstkcontext) {
                while ((tmpList != s_stkContextList[socket_id]) && (pstkcontext != tmpList->pstkcontext)) {
                    tmpList = tmpList->next;
                }
                if (tmpList == s_stkContextList[socket_id]) {
                    return 0;
                } else {
                    tmpList->next->prev = tmpList->prev;
                    tmpList->prev->next = tmpList->next;
                    freeStkContextList(tmpList);
                }
            } else {
                tmpList->next->prev = tmpList->prev;
                tmpList->prev->next = tmpList->next;
                s_stkContextList[socket_id] = tmpList->next;
                freeStkContextList(tmpList);
            }
        }
    } else {
        RLOGE("s_stkContextList or pstkcontext is NULL");
        return 0;
    }

    return 1;
}

StkContext *getStkContext(int socket_id, int channel_id) {
    RLOGD("getStkContext");
    StkContextList *tmp = NULL;
    tmp = s_stkContextList[socket_id];
    if (channel_id < 0) {
        return NULL;
    }
    if (tmp == NULL) {
        RLOGD("s_stkContextList[socket_id] is NULL");
        return NULL;
    }

    do {
        if (channel_id == tmp->pstkcontext->channel_id) {
            return tmp->pstkcontext;
        } else {
            tmp = tmp->next;
        }
    } while (tmp != s_stkContextList[socket_id]);

    return NULL;
 }

StkContext *getStkContextUseCid(int socket_id, int openchannelCid) {
    StkContextList *tmp = NULL;
    tmp = s_stkContextList[socket_id];
    if (openchannelCid < 0) {
        return NULL;
    }
    if (tmp == NULL) {
        RLOGD("s_stkContextList[socket_id] is NULL");
        return NULL;
    }

    if ((openchannelCid != tmp->pstkcontext->openchannelCid) && tmp != tmp->next) {
        tmp = tmp->next;
    } else {
        return tmp->pstkcontext;
    }

    while ((tmp != s_stkContextList[socket_id]) && (openchannelCid != tmp->pstkcontext->openchannelCid)) {
        tmp = tmp->next;
    }
    if (tmp == s_stkContextList[socket_id]) {
        return NULL;
    } else {
        return tmp->pstkcontext;
    }
 }

void sendEvenLoopThread(void *param) {
    RLOGD("sendEvenLoopThread");
    int socket_id = -1;
    int openchannelCid = -1;
    StkContext *pstkContext = NULL;

    if (param && ((CallbackPara *)param)->para) {
        socket_id = (int)(((CallbackPara *)param)->socket_id);
        openchannelCid = *((int *)(((CallbackPara *)param)->para));
        RLOGD("sendEvenLoopThread socket_id:%d openchannelCid:%d", socket_id, openchannelCid);
    }
    if (socket_id < 0 || socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        return;
    }
    if (openchannelCid < 0) {
        RLOGE("sendEvenLoopThread openchannelCid less than 0");
        return;
    }

    pstkContext = getStkContextUseCid(socket_id, openchannelCid);
    if (pstkContext == NULL) {
        RLOGE("getStkContextUseCid pstkContext is NULL");
        return;
    }

    pstkContext->channelEstablished = false;
    sendEventChannelStatus(pstkContext, socket_id);

    pstkContext->openchannelCid = -1;

    return;
}

static void requestDefaultNetworkName(int channelID, RIL_Token t) {
    ATResponse *p_response = NULL;
    ATResponse *p_newResponse = NULL;
    ATLine *p_cur;
    int err;
    char *apn = NULL;

    err = at_send_command_multiline(s_ATChannels[channelID],
                "AT+SPIPCONTEXT?", "+SPIPCONTEXT:", &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    cgdcont_read_cmd_rsp(p_response, &p_newResponse);
    for (p_cur = p_newResponse->p_intermediates; p_cur != NULL;
            p_cur = p_cur->p_next) {
        char *line = p_cur->line;
        int ncid;
        int active;

        err = at_tok_start(&line);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &ncid);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &active);
        if (err < 0) goto error;
        if (ncid == 1) {
            err = at_tok_nextstr(&line, &apn);
            if (err < 0 || strlen(apn) == 0) goto error;
            break;
        }
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, apn, strlen(apn) + 1);
    AT_RESPONSE_FREE(p_response);
    AT_RESPONSE_FREE(p_newResponse);
    return;
error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    AT_RESPONSE_FREE(p_response);
    AT_RESPONSE_FREE(p_newResponse);
    return;
}

int processStkRequests(int request, void *data, size_t datalen, RIL_Token t,
                          int channelID) {
    RIL_UNUSED_PARM(datalen);

    int err;
    ATResponse *p_response = NULL;

    switch (request) {
        case RIL_REQUEST_STK_GET_PROFILE: {
            char *line;
            p_response = NULL;
            err = at_send_command_singleline(s_ATChannels[channelID],
                        "AT+SPUSATPROFILE?", "+SPUSATPROFILE:", &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                line = p_response->p_intermediates->line;
                RIL_onRequestComplete(t, RIL_E_SUCCESS, line,
                        strlen(line) + 1);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND: {
            char *cmd;
            int ret;
            if ((char *)(data) == NULL || strlen((char *)data) == 0) {
                RLOGE("data is invalid");
                RIL_onRequestComplete(t, RIL_E_INVALID_ARGUMENTS, NULL, 0);
                break;
            }
            ret = asprintf(&cmd, "AT+SPUSATENVECMD=\"%s\"", (char *)(data));
            if (ret < 0) {
                RLOGE("Failed to allocate memory");
                cmd = NULL;
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
                break;
            }
            err = at_send_command_singleline(s_ATChannels[channelID], cmd,
                                             "+SPUSATENVECMD:", &p_response);
            free(cmd);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE: {
            char *cmd;
            int ret;
            if ((char *)(data) == NULL || strlen((char *)data) == 0) {
                RLOGE("data is invalid");
                RIL_onRequestComplete(t, RIL_E_INVALID_ARGUMENTS, NULL, 0);
                break;
            }
            RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
            RLOGD("STK_SEND_TERMINAL_RESPONSE s_lunchOpenChannelDialog:%d", (int)s_lunchOpenChannelDialog);
            if (s_lunchOpenChannelDialog[socket_id]) {
                int dataLen = strlen(data);
                int channelId = s_curBipChannelID[socket_id];
                RLOGD("dataLen:%d",dataLen);
                if (((char *)data)[dataLen - 2] == '0') {
                    RLOGD("accept lunchOpenChannel");
                    lunchOpenChannel(socket_id, channelId);
                } else if (((char *)data)[dataLen - 2] == '1') {
                    RLOGD("cancel send TR");
                    StkContext *pstkContext = NULL;
                    pstkContext = getStkContext(socket_id, channelId);
                    sendChannelResponse(pstkContext, USER_NOT_ACCEPT, socket_id);

                    removeFromStkContextList(pstkContext, socket_id);
                    putBipChannel(socket_id, channelId);
                    s_curBipChannelID[socket_id] = 0;
                } else {
                    RLOGD("timeout send TR");
                    StkContext *pstkContext = NULL;
                    pstkContext = getStkContext(socket_id, channelId);
                    sendChannelResponse(pstkContext, NO_RESPONSE_FROM_USER, socket_id);

                    removeFromStkContextList(pstkContext, socket_id);
                    putBipChannel(socket_id, channelId);
                    s_curBipChannelID[socket_id] = 0;
                }
                s_lunchOpenChannelDialog[socket_id] = false;
                break;
            }
            ret = asprintf(&cmd, "AT+SPUSATTERMINAL=\"%s\"", (char *)(data));
            if (ret < 0) {
                RLOGE("Failed to allocate memory");
                cmd = NULL;
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
                break;
            }
            err = at_send_command_singleline(s_ATChannels[channelID], cmd,
                                             "+SPUSATTERMINAL:", &p_response);
            free(cmd);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM: {
            int value = ((int *)data)[0];
            if (value == 0) {
                err = at_send_command(s_ATChannels[channelID],
                                      "AT+SPUSATCALLSETUP=0", &p_response);
            } else {
                err = at_send_command(s_ATChannels[channelID],
                                      "AT+SPUSATCALLSETUP=1", &p_response);
            }
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING: {
            RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
            s_stkServiceRunning[socket_id] = true;
            if (NULL != s_stkUnsolResponse[socket_id]) {
               int respLen = strlen(s_stkUnsolResponse[socket_id]) + 1;
               RIL_onUnsolicitedResponse(RIL_UNSOL_STK_PROACTIVE_COMMAND,
                             s_stkUnsolResponse[socket_id], respLen, socket_id);
               free(s_stkUnsolResponse[socket_id]);
               s_stkUnsolResponse[socket_id] = NULL;
               RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
               break;
            }
            int response = 0;
            err = at_send_command_singleline(s_ATChannels[channelID],
                    "AT+SPUSATPROFILE?", "+SPUSATPROFILE:", &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_EXT_REQUEST_GET_DEFAULT_NAN:
            requestDefaultNetworkName(channelID, t);
            break;
        default:
            return 0;
    }

    return 1;
}

static int checkStkProCmdType(char *cmdStr) {
    char tempStr[3] = {0};
    char *end = NULL;
    int cmdType = 0;

    memcpy(tempStr, cmdStr, 2);

    cmdType = strtoul(tempStr, &end, 16);
    cmdType = 0xFF & cmdType;
    RLOGD("cmdType: %d",cmdType);

    return cmdType;
}

static void *processBipClient(void *param) {
    int length = -1;
    int ret = RESULT_EXCEPTION;
    int cmdType = 0;
    int cmdDetParseDone = -1;
    char *rawData = NULL;
    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    BipClient *pBipClient = NULL;
    BerTlv *berTlv = NULL;
    CommandDetails *cmdDet = NULL;
    ComprehensionTlv *comprehensionTlv = NULL;
    RilMessage *rilMessage = NULL;
    OpenChannelData *openChannelData = NULL;
    SendChannelData *sendData = NULL;
    ReceiveChannelData *receiveData = NULL;
    CloseChannelData *closeChannelData = NULL;
    CatResponseMessageSprd *pResMsg= NULL;

    if (param) {
        pBipClient = (BipClient *)param;
    }
    if (pBipClient == NULL) {
        RLOGE("openChannelThread pstkContext is NULL");
        goto EXIT;
    }

    rawData = pBipClient->response;
    length = strlen(rawData);
    socket_id = pBipClient->socket_id;

    pResMsg = (CatResponseMessageSprd *)calloc(1, sizeof(CatResponseMessageSprd));
    rilMessage = (RilMessage *)calloc(1, sizeof(RilMessage));
    berTlv = (BerTlv *)calloc(1, sizeof(BerTlv));

    switch (parseProCommand(rawData, length, berTlv)) {
        case OK: {
             comprehensionTlv = berTlv->compTlvs;
             cmdDet = (CommandDetails *)calloc(1, sizeof(CommandDetails));
             cmdDetParseDone = processCommandDetails(comprehensionTlv, cmdDet);
             if (cmdDetParseDone == -1) {
                ret = CMD_DATA_NOT_UNDERSTOOD;
                pResMsg->includeAdditionalInfo = false;
                pResMsg->additionalInfo = 0x00;
                pResMsg->resCode = ret;
                sendTerminalResponse(cmdDet, pResMsg, socket_id, 0, NULL);
                goto EXIT;
             }
            break;
        }
        case CMD_DATA_NOT_UNDERSTOOD:
        case REQUIRED_VALUES_MISSING:
        case RESULT_EXCEPTION:
        default:
            comprehensionTlv = berTlv->compTlvs;
            goto EXIT;
    }

     if (!berTlv->lengthValid) {
         RLOGD("berTlv->lengthValid:%d",berTlv->lengthValid);
         ret = CMD_DATA_NOT_UNDERSTOOD;
         pResMsg->includeAdditionalInfo = false;
         pResMsg->additionalInfo = 0x00;
         pResMsg->resCode = ret;

         sendTerminalResponse(cmdDet, pResMsg, socket_id, 0, NULL);
         goto EXIT;
     }

     cmdType = cmdDet->typeOfCommand;

     switch (cmdType) {
        case OPEN_CHANNEL: {
            openChannelData = (OpenChannelData *)calloc(1, sizeof(OpenChannelData));
            processOpenChannel(comprehensionTlv, openChannelData, rilMessage);
            if (rilMessage->resCode == OK) {
                RLOGD("processOpenChannel resCode == OK");
                int err = -1;
                StkContext *pstkcontext = NULL;
                pstkcontext = (StkContext*)calloc(1, sizeof(StkContext));
                memset(pstkcontext, 0 ,sizeof(StkContext));
                err = initStkContext(pstkcontext, socket_id, openChannelData, cmdDet);
                if (err == 0) {
                    free(pstkcontext);
                    goto EXIT;
                }
                err = addToStkContextList(pstkcontext, socket_id);
                if (err == 0) {
                    free(pstkcontext);
                    goto EXIT;
                }

                if (openChannelData->channelData.text == NULL) {
                    int channelId = s_curBipChannelID[socket_id];
                    RLOGD("direct lunchOpenChannel");
                    lunchOpenChannel(socket_id, channelId);
                } else {
                    int bufCount = 0;
                    int totalLength = 0;
                    unsigned char buf[MAX_BUFFER_BYTES / 2] = {0};
                    unsigned char hexString[MAX_BUFFER_BYTES] = {0};
                    char *pStr = NULL;

                    s_lunchOpenChannelDialog[socket_id] = true;

                    int tag = 0xD0;
                    totalLength = 12 + (openChannelData->alphaIdLength);

                    buf[bufCount++] = tag;
                    buf[bufCount++] = totalLength;

                    tag = 0x80 | COMMAND_DETAILS;
                    buf[bufCount++] = tag;
                    buf[bufCount++] = 0x03;
                    buf[bufCount++] = cmdDet->commandNumber;
                    buf[bufCount++] = 0x21;
                    buf[bufCount++] = 0x81;

                    tag = 0x80 | DEVICE_IDENTITIES;
                    buf[bufCount++] = tag;
                    buf[bufCount++] = 0x02;
                    buf[bufCount++] = 0x81;
                    buf[bufCount++] = 0x02;

                    tag = 0x80 | TEXT_STRING;
                    buf[bufCount++] = tag;
                    buf[bufCount++] = openChannelData->alphaIdLength + 1;
                    buf[bufCount++] = 0x04;
                    while (comprehensionTlv != NULL) {
                        if (ALPHA_ID == comprehensionTlv->tTlv.tag) {
                            int length = 0;
                            length = comprehensionTlv->tTlv.length;
                            pStr = comprehensionTlv->tTlv.value;
                            if (pStr != NULL) {
                                while (length > 0) {
                                   buf[bufCount++] = *pStr++;
                                    length--;
                                }
                                break;
                            }
                        }
                        comprehensionTlv = comprehensionTlv->next;
                    }
                    convertBinToHex((char *)buf, strlen((const char *)buf), hexString);
                    RIL_onUnsolicitedResponse(RIL_UNSOL_STK_PROACTIVE_COMMAND, hexString,
                                                  strlen((const char *)hexString) + 1, socket_id);
                }
            } else {
                free(cmdDet);
                free(openChannelData->channelData.text);
                free(openChannelData);
                openChannelData = NULL;
            }
            break;
        }
        case SEND_DATA: {
            sendData = (SendChannelData *)calloc(1, sizeof(SendChannelData));
            processSendData(comprehensionTlv, sendData, rilMessage);
            if (rilMessage->resCode == OK) {
                RLOGD("processSendData resCode == OK");
                int channelId = sendData->channelId;
                StkContext *pstkContext = NULL;
                pstkContext = getStkContext(socket_id, channelId);
                if (pstkContext == NULL) {
                    RLOGD("processSendData getStkContext is NULL");
                    ret = BIP_ERROR;
                    pResMsg->includeAdditionalInfo = true;
                    pResMsg->additionalInfo = CHANNEL_ID_INVALID;
                    pResMsg->resCode = ret;

                    sendTerminalResponse(cmdDet, pResMsg, socket_id, 0, NULL);
                }
                pstkContext->pSCData = sendData;
                free(pstkContext->pCmdDet);
                pstkContext->pCmdDet = cmdDet;

                lunchSendData(socket_id, channelId);
            } else {
                free(cmdDet);
                free(sendData->channelData.text);
                free(sendData->sendDataStr);
                free(sendData);
                sendData = NULL;
            }
            break;
        }
        case CLOSE_CHANNEL: {
            closeChannelData = (CloseChannelData *)calloc(1, sizeof(CloseChannelData));
            RLOGD("CLOSE_CHANNEL processCloseChannel enter");
            processCloseChannel(comprehensionTlv, closeChannelData, rilMessage);
            RLOGD("CLOSE_CHANNEL processCloseChannel EXIT");
            if (rilMessage->resCode == OK) {
                RLOGD("processCloseChannel resCode == OK");
                int channelId = closeChannelData->channelId;
                StkContext *pstkContext = NULL;
                pstkContext = getStkContext(socket_id, channelId);
                if (pstkContext == NULL) {
                    RLOGD("processCloseChannel getStkContext is NULL");
                    if (s_stkContextList[socket_id] == NULL) {
                        RLOGD("processCloseChannel s_stkContextList is NULL");
                        ret = BIP_ERROR;
                        pResMsg->includeAdditionalInfo = true;
                        pResMsg->additionalInfo = CHANNEL_CLOSED;
                        pResMsg->resCode = ret;

                        sendTerminalResponse(cmdDet, pResMsg, socket_id, 0, NULL);
                        break;
                    } else {
                        ret = BIP_ERROR;
                        pResMsg->includeAdditionalInfo = true;
                        pResMsg->additionalInfo = CHANNEL_ID_INVALID;
                        pResMsg->resCode = ret;

                        sendTerminalResponse(cmdDet, pResMsg, socket_id, 0, NULL);
                        break;
                    }
                }
                pstkContext->pCCData = closeChannelData;
                free(pstkContext->pCmdDet);
                pstkContext->pCmdDet = cmdDet;

                lunchCloseChannel(socket_id, channelId);
                removeFromStkContextList(pstkContext, socket_id);
                putBipChannel(socket_id, channelId);
                s_curBipChannelID[socket_id] = 0;
            } else {
                free(cmdDet);
                free(closeChannelData->channelData.text);
                free(closeChannelData);
                closeChannelData = NULL;
            }
            break;
        }
        case RECEIVE_DATA: {
            receiveData = (ReceiveChannelData *)calloc(1, sizeof(ReceiveChannelData));
            processReceiveData(comprehensionTlv, receiveData, rilMessage);
            if (rilMessage->resCode == OK) {
                RLOGD("processReceiveData resCode == OK");
                int channelId = receiveData->channelId;
                StkContext *pstkContext = NULL;
                pstkContext = getStkContext(socket_id, channelId);
                pstkContext->pRCData = receiveData;
                free(pstkContext->pCmdDet);
                pstkContext->pCmdDet = cmdDet;
                lunchReceiveData(socket_id, channelId);
            } else {
                free(cmdDet);
                free(receiveData->channelData.text);
                free(receiveData);
                receiveData = NULL;
            }
            break;
        }
        case GET_CHANNEL_STATUS: {
            processGetChannelStatus(rilMessage);
            if (rilMessage->resCode == OK) {
                int channelId = DEFAULT_CHANNELID;
                StkContext *pstkContext = NULL;
                pstkContext = getStkContext(socket_id, channelId);

                if (pstkContext == NULL) {
                    RLOGD("processGetChannelStatus getStkContext is NULL");
                    ret = 0x00;
                    pResMsg->includeAdditionalInfo = false;
                    pResMsg->resCode = ret;

                    sendTerminalResponse(cmdDet, pResMsg, socket_id, CSRD_SPRD, NULL);
                    break;
                }
                free(pstkContext->pCmdDet);
                pstkContext->pCmdDet = cmdDet;

                lunchGetChannelStatus(socket_id, channelId);
            } else {
                free(cmdDet);
            }
            break;
        }
       default:
            break;
    }

EXIT:
    free(rilMessage->rawData);
    free(rilMessage);
    free(pResMsg->BearerParam);
    free(pResMsg->addedInfo);
    free(pResMsg->channelData);
    free(pResMsg->usersInput);
    free(pResMsg);
    free(berTlv);
    free(pBipClient->response);
    free(pBipClient);

    // cmdDet use pstkcontext->pCmdDet free
    // berTlv->compTlvs use comprehensionTlv free

    ComprehensionTlv *next = NULL;
    while (comprehensionTlv != NULL) {
        next = comprehensionTlv->next;
        free(comprehensionTlv->tTlv.value);
        free(comprehensionTlv);
        comprehensionTlv = next;
    }

    return NULL;
}

static void initIsimCard(void *param){
    int channelID;
    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    if ((int)socket_id < 0 || (int)socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        return;
    }
    channelID = getChannel(socket_id);
    int response = initISIM(channelID);
    putChannel(channelID);
}

int processStkUnsolicited(RIL_SOCKET_ID socket_id, const char *s) {
    int err;
    char *line = NULL;
    char bipMode[NAME_SIZE] = {0};

    if (strStartsWith(s, "+SPUSATENDSESSIONIND")) {
        RIL_onUnsolicitedResponse(RIL_UNSOL_STK_SESSION_END, NULL, 0,
                socket_id);
    } else if (strStartsWith(s, "+SPUSATPROCMDIND:")) {
        int ret = -1;
        int typePos = 0;
        int channelId = -1;
        char *tmp = NULL;;
        char *response = NULL;
        BipClient *pBipClient = NULL;
        pthread_attr_t attr;
        pthread_t bipClientTid;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);
        err = at_tok_nextstr(&tmp, &response);
        if (err < 0) {
            RLOGE("%s fail", s);
            goto out;
        }

        if (response[2] <= '7') {
           typePos = 10;
        } else {
           typePos = 12;
        }

        switch (checkStkProCmdType(&(response[typePos]))) {
           case OPEN_CHANNEL: {
               property_get(STK_BIP_MODE_PROP, bipMode, "single");
               RLOGD("getBipChannel bipMode: %s", bipMode);
               if (strcmp(bipMode,"single") == 0) {
                   if (s_stkContextList[socket_id] != NULL) {
                       RLOGD("clear s_stkContextList");
                       channelId = s_curBipChannelID[socket_id];
                       removeFromStkContextList(s_stkContextList[socket_id]->pstkcontext, socket_id);
                       putBipChannel(socket_id, channelId);
                   }
               }
           }
           case CLOSE_CHANNEL:
           case RECEIVE_DATA:
           case SEND_DATA:
           case GET_CHANNEL_STATUS:
                pBipClient = (BipClient *)calloc(1, sizeof(BipClient));
                pBipClient->response = strdup(response);
                pBipClient->socket_id = socket_id;
                pthread_attr_init(&attr);
                pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
                ret = pthread_create(&bipClientTid, &attr, processBipClient, (void *)pBipClient);
                if (ret < 0) {
                    RLOGE("Failed to create createSocketThread errno: %d", errno);
                    free(pBipClient->response);
                    pBipClient->response = NULL;
                    free(pBipClient);
                    pBipClient = NULL;
                }
                goto out;
           case REFRESH:
                if (strncasecmp(&(response[typePos + 2]), "04", 2) == 0) { //SIM_RESET
                    RLOGD("Type of Refresh is SIM_RESET");
                    s_stkServiceRunning[socket_id] = false;
                    RIL_onUnsolicitedResponse(RIL_UNSOL_STK_PROACTIVE_COMMAND, response,
                                      strlen(response) + 1, socket_id);
                    goto out;
                }
                break;
           default:
                break;
        }

        if (false == s_stkServiceRunning[socket_id]) {
            s_stkUnsolResponse[socket_id] =
                          (char *)calloc((strlen(response) + 1), sizeof(char));
            snprintf(s_stkUnsolResponse[socket_id], strlen(response) + 1,
                     "%s", response);
            RLOGD("STK service is not running [%s]",
                     s_stkUnsolResponse[socket_id]);
        } else {
            RIL_onUnsolicitedResponse(RIL_UNSOL_STK_PROACTIVE_COMMAND, response,
                                      strlen(response) + 1, socket_id);
        }
    } else if (strStartsWith(s, "+SPUSATDISPLAY:")) {
        char *response = NULL;
        char *tmp;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);
        err = at_tok_nextstr(&tmp, &response);
        if (err < 0) {
            RLOGD("%s fail", s);
            goto out;
        }
        RIL_onUnsolicitedResponse(RIL_UNSOL_STK_EVENT_NOTIFY, response,
                                  strlen(response) + 1, socket_id);
    } else if (strStartsWith(s, "+SPUSATSETUPCALL:")) {
        char *response = NULL;
        char *tmp;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);
        err = at_tok_nextstr(&tmp, &response);
        if (err < 0) {
            RLOGD("%s fail", s);
            goto out;
        }

        RIL_onUnsolicitedResponse(RIL_UNSOL_STK_PROACTIVE_COMMAND, response,
                                  strlen(response) + 1, socket_id);
    } else if (strStartsWith(s, "+SPUSATREFRESH:")) {
        char *tmp;
        int result = 0;
        RIL_SimRefreshResponse_v7 *response = NULL;

        response = (RIL_SimRefreshResponse_v7 *)
                   alloca(sizeof(RIL_SimRefreshResponse_v7));
        if (response == NULL) {
            goto out;
        }
        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);
        err = at_tok_nextint(&tmp, &result);
        if (err < 0) {
            RLOGD("%s fail", s);
            goto out;
        }
        err = at_tok_nextint(&tmp, &response->ef_id);
        if (err < 0) {
            RLOGD("%s fail", s);
            goto out;
        }
        skipNextComma(&tmp);
        err = at_tok_nextstr(&tmp, &response->aid);
        if (err < 0) {
            RLOGD("%s fail", s);
            goto out;
        }
        response->result = result;
        if (SIM_RESET == result) {
            s_imsInitISIM[socket_id] = -1;
            s_stkServiceRunning[socket_id] = false;
        }
        if (strcmp(response->aid, "") != 0) {
            if (strncasecmp((response->aid) + 10, "1004", 4) == 0) { //1004 Isim app change
                RLOGD("Isim app change");
                s_imsInitISIM[socket_id] = -1;
                RIL_requestTimedCallback(initIsimCard, (void *)&s_socketId[socket_id], NULL);
            }
        }
        response->aid = "";
        RIL_onUnsolicitedResponse(RIL_UNSOL_SIM_REFRESH, response,
                sizeof(RIL_SimRefreshResponse_v7), socket_id);
    /* SPRD: add for alpha identifier display in stk @{ */
    } else if (strStartsWith(s, "+SPUSATCALLCTRL:")) {
        char *tmp;
        RIL_StkCallControlResult *response = NULL;;

        response = (RIL_StkCallControlResult *)alloca(sizeof(RIL_StkCallControlResult));
        if (response == NULL) goto out;
        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);
        err = at_tok_nextint(&tmp, &response->call_type);
        if (err < 0) {
            RLOGD("%s fail", s);
            goto out;
        }
        err = at_tok_nextint(&tmp, &response->result);
        if (err < 0) {
            RLOGD("%s fail", s);
            goto out;
        }
        err = at_tok_nextint(&tmp, &response->is_alpha);
        if (err < 0) {
            RLOGD("%s fail", s);
            goto out;
        }
        err = at_tok_nextint(&tmp, &response->alpha_len);
        if (err < 0 || response->alpha_len == 0) {
            RLOGD("%s fail", s);
            goto out;
        }
        err = at_tok_nextstr(&tmp, &response->alpha_data);
        if (err < 0 || strlen(response->alpha_data) == 0) {
            RLOGD("%s fail", s);
            goto out;
        }
        err = at_tok_nextint(&tmp, &response->pre_type);
        if (err < 0) {
            RLOGD("%s fail", s);
            goto out;
        }
        err = at_tok_nextint(&tmp, &response->ton);
        if (err < 0) {
            RLOGD("%s fail", s);
            goto out;
        }
        err = at_tok_nextint(&tmp, &response->npi);
        if (err < 0) {
            RLOGD("%s fail", s);
            goto out;
        }
        err = at_tok_nextint(&tmp, &response->num_len);
        if (err < 0) {
            RLOGD("%s fail", s);
            goto out;
        }
        err = at_tok_nextstr(&tmp, &response->number);
        if (err < 0) {
            RLOGD("%s fail", s);
            goto out;
        }
        RIL_onUnsolicitedResponse(RIL_UNSOL_STK_CC_ALPHA_NOTIFY,
                                  response->alpha_data,
                                  strlen(response->alpha_data) + 1,
                                  socket_id);
    /* @} */
    } else {
        return 0;
    }

out:
    free(line);
    return 1;
}
