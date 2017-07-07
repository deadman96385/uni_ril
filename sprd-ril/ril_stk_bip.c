/**
 * ril_stk_bip.c --- stk bip session functions implementation
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */
#define LOG_TAG "RIL_STK_BIP"

#include <arpa/inet.h>
#include "ril_utils.h"
#include "sprd_ril.h"
#include "ril_utils.h"
#include "ril_data.h"
#include "ril_stk_bip.h"
#include "ril_stk_parser.h"

extern StkContextList *s_stkContextList;

int sendTRData(int socket_id, char *data) {
    int ret = -1;
    int err = -1;
    int sendTRChannelID = getChannel(socket_id);
    char *cmd = NULL;
    ATResponse *p_response = NULL;

    ret = asprintf(&cmd, "AT+SPUSATTERMINAL=\"%s\"", data);
    if (ret < 0) {
        RLOGE("Failed to allocate memory");
        cmd = NULL;
        goto error;
    }
    err = at_send_command_singleline(s_ATChannels[sendTRChannelID], cmd,
                                     "+SPUSATTERMINAL:", &p_response);
    free(cmd);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }
    putChannel(sendTRChannelID);
    AT_RESPONSE_FREE(p_response);
    return 1;

error:
    putChannel(sendTRChannelID);
    AT_RESPONSE_FREE(p_response);
    return 0;
}

int sendELData(int socket_id, char *data) {
    int ret = -1;
    int err = -1;
    int sendELChannelID = getChannel(socket_id);
    char *cmd = NULL;
    ATResponse *p_response = NULL;

    ret = asprintf(&cmd, "AT+SPUSATENVECMD=\"%s\"", data);
    if (ret < 0) {
        RLOGE("Failed to allocate memory");
        cmd = NULL;
        goto error;
    }
    err = at_send_command_singleline(s_ATChannels[sendELChannelID], cmd,
                                     "+SPUSATENVECMD:", &p_response);
    free(cmd);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }
    putChannel(sendELChannelID);
    AT_RESPONSE_FREE(p_response);
    return 1;

error:
    putChannel(sendELChannelID);
    AT_RESPONSE_FREE(p_response);
    return 0;
}

static char *getDefaultBearerNetAccessName(int channelID) {
    int err = -1;
    char *apn = NULL;
    ATLine *p_cur = NULL;
    ATResponse *p_response = NULL;
    ATResponse *p_newResponse = NULL;

    err = at_send_command_multiline(s_ATChannels[channelID],
                "AT+SPIPCONTEXT?", "+SPIPCONTEXT:", &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    cgdcont_read_cmd_rsp(p_response, &p_newResponse);
    for (p_cur = p_newResponse->p_intermediates; p_cur != NULL;
            p_cur = p_cur->p_next) {
        char *line = p_cur->line;
        int ncid = -1;
        int active = -1;

        err = at_tok_start(&line);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &ncid);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &active);
        if (err < 0) goto error;

        if (ncid == 1) {
            err = at_tok_nextstr(&line, &apn);
            if (err < 0 || emNStrlen(apn) == 0) goto error;
            break;
        }
    }

    AT_RESPONSE_FREE(p_response);
    AT_RESPONSE_FREE(p_newResponse);
    return apn;

error:
    AT_RESPONSE_FREE(p_response);
    AT_RESPONSE_FREE(p_newResponse);
    return NULL;
}

int endConnectivity(StkContext *pstkContext, int socket_id) {
    int countStrings = 1;
    char **pStrings = NULL;
    char cmd[AT_COMMAND_LEN] = {0};

    pStrings = (char **)calloc(countStrings, sizeof(char *));
    if (pStrings == NULL) {
        RLOGE("Memory allocation failed for request");
        return -1;
    }

    snprintf(cmd, sizeof(cmd), "%d", pstkContext->openchannelCid);
    pStrings[0] = cmd;

    RLOGD("endConnectivity pstkContext->openchannelCid: %s",pStrings[0]);

    int sendChannelID = getChannel(socket_id);
    requestDeactiveDataConnection(sendChannelID, pStrings, countStrings);
    putChannel(sendChannelID);

    free(pStrings);
    pstkContext->openchannelCid = -1;

    return 1;
}

int SetupStkConnect(StkContext *pstkContext, int socket_id) {
    RLOGD("[stk] SetupStkConnect");
    int countStrings = 7;
    char **pStrings = NULL;

    pStrings = (char **)calloc(countStrings, sizeof(char *));
    if (pStrings == NULL) {
        RLOGE("Memory allocation failed for request");
        return -1;
    }

    // apn
    if (emNStrlen(pstkContext->pOCData->NetAccessName) == 0) {
        RLOGD("[stk] SetupStkConnect pStrings[2]");
        pStrings[2] = "TestGp.rs";
    } else {
        pStrings[2] = pstkContext->pOCData->NetAccessName;
    }
    // user
    pStrings[3] = (char *)pstkContext->pOCData->LoginStr;
    // password
    pStrings[4] = (char *)pstkContext->pOCData->PwdStr;
    // authType
    if (pstkContext->pOCData->PwdStr[0] != 0 ||
        pstkContext->pOCData->LoginStr[0] != 0) {
        pStrings[5] = "3";      // PAP and CHAP
    } else {
        pStrings[5] = "0";      // None
    }

    // protocol
    char protocol[NAME_SIZE] = {0};
    if (emNStrlen(pstkContext->pOCData->BearerParam) != 0) {
        RLOGD("pstkContext->pOCData->BearerParam length: %d",
                emNStrlen(pstkContext->pOCData->BearerParam));
        if (strStartsWith(pstkContext->pOCData->BearerParam +
                emNStrlen(pstkContext->pOCData->BearerParam) - 2,
                TYPE_PACKAGE_DATA_PROTOCOL_IP)) {
            memcpy(protocol, "IP", 2);
        } else {
            memcpy(protocol, "IPV4V6", 6);
        }
    } else {
        memcpy(protocol, "IPV4V6", 6);
    }
    pStrings[6] = protocol;

    int sendChannelID = getChannel(socket_id);
    pstkContext->openchannelCid =
            requestSetupDataConnection(sendChannelID, pStrings, countStrings);
    RLOGD("SetupStkConnect pstkContext->openchannelCid: %d",
            pstkContext->openchannelCid);
    putChannel(sendChannelID);

    free(pStrings);
    return 1;
}

void sendEventLoop(CatResponseMessageSprd *pResMsg, int event, int socket_id) {
    int bufCount = 0;
    unsigned char buf[MAX_BUFFER_BYTES / 2];
    unsigned char hexString[MAX_BUFFER_BYTES];
    memset(buf, 0, sizeof(buf));
    memset(hexString, 0, sizeof(hexString));

    buf[bufCount++] = BER_EVENT_DOWNLOAD_TAG;

    buf[bufCount++] = 0x00;

    buf[bufCount++] = 0x80 | EVENT_LIST;
    buf[bufCount++] = 0x01;

    switch(event) {
        case CHANNEL_STATUS_EVENT:
            buf[bufCount++] = CHANNEL_STATUS_EVENT;
            break;
        case DATA_AVAILABLE_EVENT:
            buf[bufCount++] = DATA_AVAILABLE_EVENT;
            break;
        default:
            return;
    }

    buf[bufCount++] = 0x80 | DEVICE_IDENTITIES;
    buf[bufCount++] = 0x02;
    buf[bufCount++] = DEV_ID_TERMINAL;
    buf[bufCount++] = DEV_ID_UICC;

    switch(event) {
        case CHANNEL_STATUS_EVENT:
            buf[bufCount++] = 0x80 | CHANNEL_STATUS;
            buf[bufCount++] = 0x02;
            buf[bufCount++] = pResMsg->ChannelId | (pResMsg->LinkStatus ? 0x80 : 0);
            buf[bufCount++] = pResMsg->mode;
            break;
        case DATA_AVAILABLE_EVENT:
            buf[bufCount++] = 0x80 | CHANNEL_STATUS;
            buf[bufCount++] = 0x02;
            buf[bufCount++] = pResMsg->ChannelId | (pResMsg->LinkStatus ? 0x80 : 0);
            buf[bufCount++] = pResMsg->mode;

            buf[bufCount++] = 0x80 | CHANNEL_DATA_LENGTH;
            buf[bufCount++] = 0x01;
            buf[bufCount++] = pResMsg->channelDataLen;
            break;
        default:
            return;
    }

    buf[1] = bufCount - 2;

    convertBinToHex((char *)buf, bufCount, hexString);

    RLOGD("ENVELOPE COMMAND: %s", hexString);

    sendELData(socket_id, (char *)hexString);
}

void sendTerminalResponse(CommandDetails *pCmdDet,
                          CatResponseMessageSprd *pResMsg, int socket_id,
                          int respId, ResponseDataSprd *pResp) {
    RLOGD("sendTerminalResponse");
    int iRDCount = 0;
    int bufCount = 0;
    int tag = COMMAND_DETAILS;
    int sendTRChannelID = -1;
    int additionalInfo = pResMsg->additionalInfo;
    char *pStr = NULL;
    unsigned char hexString[MAX_BUFFER_BYTES];
    unsigned char buf[MAX_BUFFER_BYTES/2];
    ResultCode resultCode = pResMsg->resCode;
    bool includeAdditionalInfo = pResMsg->includeAdditionalInfo;

    memset(buf, 0, sizeof(buf));
    memset(hexString, 0, sizeof(hexString));
    if (pCmdDet == NULL) {
        RLOGE("pCmdDet is NULL");
        return;
    }

    RLOGD("pCmdDet addr = %p", pCmdDet);
    RLOGD("[stk] pCmdDet->compRequired = %d", pCmdDet->compRequired);
    RLOGD("[stk] pCmdDet->commandNumber = %d", pCmdDet->commandNumber);
    RLOGD("[stk] pCmdDet->typeOfCommand = %d", pCmdDet->typeOfCommand);
    RLOGD("[stk] pCmdDet->commandQualifier = %d", pCmdDet->commandQualifier);

    if (pCmdDet->compRequired) {
        tag |= 0x80;
    }
    buf[bufCount++] = tag;
    buf[bufCount++] = 0x03;
    buf[bufCount++] = pCmdDet->commandNumber;
    buf[bufCount++] = pCmdDet->typeOfCommand;
    buf[bufCount++] = pCmdDet->commandQualifier;

    tag = 0x80 | DEVICE_IDENTITIES;
    buf[bufCount++] = tag;
    buf[bufCount++] = 0x02;
    buf[bufCount++] = DEV_ID_TERMINAL;
    buf[bufCount++] = DEV_ID_UICC;

    tag = RESULT;
    if (pCmdDet->compRequired) {
        tag |= 0x80;
    }
    buf[bufCount++] = tag;
    int length = includeAdditionalInfo ? 2 : 1;
    buf[bufCount++] = length;
    buf[bufCount++] = resultCode;

    if (includeAdditionalInfo) {
        buf[bufCount++] = additionalInfo;
    }

    switch (respId){
        case OCRD_SPRD: {
            RLOGD("[stk] OpenChannelResponseData linkStatus = %d", pResp->linkStatus);
            if (pResp->linkStatus) {
                tag = CHANNEL_STATUS;
                buf[bufCount++] = tag;
                buf[bufCount++] = 0x02;
                buf[bufCount++] = (pResp->channelId | 0x80);
                buf[bufCount++] = 0x00;
            }
            tag = BEARER_DESCRIPTION;
            buf[bufCount++] = tag;

            if (emNStrlen(pResp->bearerParam) != 0) {
                RLOGD("pResp->bearerParam length:%d", emNStrlen(pResp->bearerParam));
                if (strStartsWith(pResp->bearerParam, "09")) {
                    char strTmp[ARRAY_SIZE] = {0};
                    snprintf(strTmp ,sizeof(strTmp), "09%s",
                            pResp->bearerParam + emNStrlen(pResp->bearerParam) - 2);
                    pStr = (char *)calloc(emNStrlen(strTmp) / 2, sizeof(char));
                    convertHexToBin(strTmp, emNStrlen(strTmp), pStr);

                    char *pStrTmp = pStr;
                    buf[bufCount++] = 0x03;
                    buf[bufCount++] = pResp->bearerType;
                    buf[bufCount++] = *(pStrTmp++) & 0xff;
                    buf[bufCount++] = *(pStrTmp++) & 0xff;
                } else {
                    pStr = (char *)calloc(emNStrlen(pResp->bearerParam) / 2, sizeof(char));
                    int iw = 1;
                    int lenBP = emNStrlen(pResp->bearerParam);
                    convertHexToBin(pResp->bearerParam, lenBP, pStr);

                    char *pStrTmp = pStr;
                    lenBP = lenBP / 2;
                    buf[bufCount++] = lenBP + 1;
                    buf[bufCount++] = pResp->bearerType;
                    for (iw = 1; iw <= lenBP; iw++) {
                        buf[bufCount++] = *(pStrTmp++) & 0xff;
                    }
                }
            } else {
                buf[bufCount++] = 0x01;
                buf[bufCount++] = pResp->bearerType;
            }

            tag = BUFFER_SIZE;
            buf[bufCount++] = tag;
            buf[bufCount++] = 0x02;
            buf[bufCount++] = (pResp->bufferSize & 0xff00) >> 8;
            buf[bufCount++] = pResp->bufferSize & 0x00ff;
            break;
        }
        case CSRD_SPRD: {
            if (pResp != NULL) {
                RLOGD("[stk] ChannelStatusResponseData linkStatus = %d", pResp->linkStatus);
            } else {
                RLOGD("[stk] ChannelStatusResponseData stkContext is NULL");
            }
            tag = 0x80 | CHANNEL_STATUS;
            buf[bufCount++] = tag;
            buf[bufCount++] = 0x02;

            if (pResp == NULL) {
                buf[bufCount++] = 0x00;
            } else {
                StkContextList *tmpList = s_stkContextList;
                if (tmpList == NULL) {
                    RLOGD("s_stkContextList is NULL");
                    buf[bufCount++] = 0x00;
                } else {
                    buf[bufCount++] =
                            (pResp->linkStatus ? pResp->channelId : 0) |
                            (pResp->linkStatus ? 0x80 : 0);
                    while (tmpList->next != s_stkContextList) {
                        RLOGD("multi bip");
                        tmpList = tmpList->next;
                        buf[bufCount++] = 0x00;
                        buf[bufCount++] = tag;
                        buf[bufCount++] = 0x02;
                        buf[bufCount++] = tmpList->pstkcontext->channel_id |
                            (tmpList->pstkcontext->openchannelCid ? 0x80 : 0);
                    }
                }
            }
            buf[bufCount++] = 0x00;
            break;
        }
        case SDRD_SPRD: {
            RLOGD("[stk] SendDataResponseData channelLen = %d", pResp->channelLen);
            tag = 0x80 | CHANNEL_DATA_LENGTH;
            buf[bufCount++] = tag;
            buf[bufCount++] = 0x01;
            buf[bufCount++] = pResp->channelLen;
            break;
        }
        case RDRD_SPRD: {
            RLOGD("[stk] ReceiveDataResponseData dataLen = %d, dataStr = %s",
                    pResp->dataLen, pResp->dataStr);
            tag = 0x80 | CHANNEL_DATA;
            buf[bufCount++] = tag;

            int nHexStringLen = emNStrlen(pResp->dataStr);
            int nRawDataLen = (nHexStringLen >> 1);
            if (nHexStringLen > 0) {
                pStr = (char *)calloc(nRawDataLen, sizeof(char));
                convertHexToBin(pResp->dataStr, nHexStringLen, pStr);
                if (nRawDataLen < 0x80) {
                    buf[bufCount++] = nRawDataLen;
                } else {
                    buf[bufCount++] = 0x81;
                    buf[bufCount++] = nRawDataLen;
                }
                char *pStrTmp = pStr;
                for (iRDCount = 0; iRDCount < nRawDataLen; iRDCount++) {
                    buf[bufCount++] = *(pStrTmp++) & 0xff;
                }
            } else {
                buf[bufCount++] = 0x00;
            }
            tag = 0x80 | CHANNEL_DATA_LENGTH;
            buf[bufCount++] = tag;
            buf[bufCount++] = 0x01;
            buf[bufCount++] = pResp->dataLen;
            break;
        }
        case CCRD_SPRD:
            break;
        case OTSRD_SPRD:
            break;
        default:
            break;
    }

    convertBinToHex((char *)buf, bufCount, hexString);

    RLOGD("TERMINAL RESPONSE: %s", hexString);

    if (pStr != NULL) {
        free(pStr);
        pStr = NULL;
    }

    sendTRData(socket_id, (char *)hexString);
}

void onCmdResponse(StkContext *pstkContext, CatResponseMessageSprd *pResMsg, int type, int socket_id) {
    int resCode = pResMsg->resCode;
    int respId = OTSRD_SPRD;
    ResponseDataSprd *pResp = NULL;
    pResp = (ResponseDataSprd *)calloc(1, sizeof(ResponseDataSprd));

    switch(resCode) {
        case HELP_INFO_REQUIRED:
        case OK:
        case PRFRMD_WITH_PARTIAL_COMPREHENSION:
        case PRFRMD_WITH_MISSING_INFO:
        case PRFRMD_WITH_ADDITIONAL_EFS_READ:
        case PRFRMD_ICON_NOT_DISPLAYED:
        case PRFRMD_MODIFIED_BY_NAA:
        case PRFRMD_LIMITED_SERVICE:
        case PRFRMD_WITH_MODIFICATION:
        case PRFRMD_NAA_NOT_ACTIVE:
        case PRFRMD_TONE_NOT_PLAYED:
        case LAUNCH_BROWSER_ERROR:
        case TERMINAL_CRNTLY_UNABLE_TO_PROCESS: {
            switch (type) {
                case OPEN_CHANNEL:
                    if (resCode == TERMINAL_CRNTLY_UNABLE_TO_PROCESS) {
                        RLOGD("< %d > OPEN_CHANNEL RES TERMINAL_CRNTLY_UNABLE_TO_PROCESS", socket_id);
                        pResMsg->includeAdditionalInfo = true;
                        pResMsg->additionalInfo = BUSY_ON_CALL;
                    } else {
                        RLOGD("< %d > OPEN_CHANNEL RES OK", socket_id);
                    }
                    respId = OCRD_SPRD;
                    pResp->bearerType = pResMsg->BearerType;
                    pResp->bearerParam = pResMsg->BearerParam;
                    pResp->bufferSize = pResMsg->bufferSize;
                    pResp->channelId = pResMsg->ChannelId;
                    pResp->linkStatus = pResMsg->LinkStatus;
                    break;
                case SEND_DATA:
                    RLOGD("< %d > SEND_DATA RES OK", socket_id);
                    respId = SDRD_SPRD;
                    pResp->channelLen = pResMsg->channelDataLen;
                    break;
                case RECEIVE_DATA:
                    RLOGD("< %d > RECEIVE_DATA RES OK", socket_id);
                    respId = RDRD_SPRD;
                    pResp->dataLen = pResMsg->channelDataLen;
                    pResp->dataStr = pResMsg->channelData;
                    break;
                case GET_CHANNEL_STATUS:
                    RLOGD("< %d > GET_CHANNEL_STATUS RES OK", socket_id);
                    respId = CSRD_SPRD;
                    pResp->channelId = pResMsg->ChannelId;
                    pResp->linkStatus = pResMsg->LinkStatus;
                    break;
                default:
                    break;
            }
            break;
        }
        case BACKWARD_MOVE_BY_USER:
        case USER_NOT_ACCEPT: {
            switch (type) {
                case OPEN_CHANNEL:
                    RLOGD("< %d > OPEN_CHANNEL USER_NOT_ACCEPT", socket_id);
                    respId = OCRD_SPRD;
                    pResp->bearerType = pResMsg->BearerType;
                    pResp->bearerParam = pResMsg->BearerParam;
                    pResp->bufferSize = pResMsg->bufferSize;
                    pResp->channelId = pResMsg->ChannelId;
                    pResp->linkStatus = pResMsg->LinkStatus;
                    break;
                default:
                    break;
            }
            break;
        }
        case NO_RESPONSE_FROM_USER:
        case UICC_SESSION_TERM_BY_USER:
            break;
        case BEYOND_TERMINAL_CAPABILITY: {
            switch (type) {
                case OPEN_CHANNEL:
                    RLOGD("< %d > OPEN_CHANNEL BEYOND_TERMINAL_CAPABILITY", socket_id);
                    respId = OCRD_SPRD;
                    pResp->bearerType = pResMsg->BearerType;
                    pResp->bearerParam = pResMsg->BearerParam;
                    pResp->bufferSize = pResMsg->bufferSize;
                    pResp->channelId = pResMsg->ChannelId;
                    pResp->linkStatus = pResMsg->LinkStatus;
                    break;
                case SEND_DATA:
                    RLOGD("< %d > SEND_DATA BEYOND_TERMINAL_CAPABILITY", socket_id);
                    respId = SDRD_SPRD;
                    pResMsg->additionalInfo = TRANSPORT_LEVEL_NOT_AVAILABLE;
                    break;
                case RECEIVE_DATA:
                    RLOGD("< %d > RECEIVE_DATA BEYOND_TERMINAL_CAPABILITY", socket_id);
                    respId = RDRD_SPRD;
                    pResMsg->additionalInfo = NO_SPECIFIC_CAUSE;
                    break;
            }
            break;
        }
        case BIP_ERROR: {
            switch (type) {
                case SEND_DATA:
                    RLOGD("< %d > SEND_DATA BIP_ERROR", socket_id);
                    respId = SDRD_SPRD;
                    pResMsg->additionalInfo = CHANNEL_ID_INVALID;
                    break;
                case CLOSE_CHANNEL:
                    RLOGD("< %d > CLOSE_CHANNEL BIP_ERROR", socket_id);
                    respId = CCRD_SPRD;
                    pResMsg->additionalInfo = CHANNEL_CLOSED;
                    break;
            }
            break;
        }
        default:
            return;
    }

    sendTerminalResponse(pstkContext->pCmdDet, pResMsg, socket_id, respId, pResp);
    free(pResp);
}

int sendChannelResponse(StkContext *pstkContext, int resCode, int socket_id) {
    RLOGD("sendChannelResponse");
    CatResponseMessageSprd *pResMsg = NULL;
    pResMsg = (CatResponseMessageSprd *)calloc(1, sizeof(CatResponseMessageSprd));
    pResMsg->includeAdditionalInfo = false;
    pResMsg->channelData = NULL;

    pstkContext->cmdInProgress = false;
    if (resCode == BEYOND_TERMINAL_CAPABILITY) {
        endConnectivity(pstkContext, socket_id);
        pstkContext->connectType = -1;
        if (pstkContext->tcpSocket != -1) {
            close(pstkContext->tcpSocket);
            pstkContext->tcpSocket = -1;
        }
    }

    RLOGD("sendChannelResponse LinkStatus = %d" ,pstkContext->channelStatus.linkStatus);
    pResMsg->resCode = resCode;
    pResMsg->BearerType = pstkContext->pOCData->BearerType;
    pResMsg->BearerParam = pstkContext->pOCData->BearerParam;
    pResMsg->bufferSize = pstkContext->pOCData->bufferSize;
    pResMsg->ChannelId = pstkContext->channelStatus.channelId;
    pResMsg->LinkStatus = pstkContext->channelStatus.linkStatus;
    pResMsg->mode = pstkContext->channelStatus.mode_info;

    RLOGD("[stk] pCmdDet->compRequired = %d", pstkContext->pCmdDet->compRequired);
    RLOGD("[stk] pCmdDet->commandNumber = %d", pstkContext->pCmdDet->commandNumber);
    RLOGD("[stk] pCmdDet->typeOfCommand = %d", pstkContext->pCmdDet->typeOfCommand);
    RLOGD("[stk] pCmdDet->commandQualifier = %d", pstkContext->pCmdDet->commandQualifier);

    onCmdResponse(pstkContext, pResMsg, OPEN_CHANNEL, socket_id);
    free(pResMsg);

    return 1;
}

int sendChannelStatusResponse(StkContext *pstkContext, int resCode, int socket_id) {
    RLOGD("sendChannelStatusResponse");
    CatResponseMessageSprd *pResMsg = NULL;
    pResMsg = (CatResponseMessageSprd*)calloc(1, sizeof(CatResponseMessageSprd));
    pResMsg->includeAdditionalInfo = false;
    pResMsg->channelData = NULL;

    pstkContext->cmdInProgress = false;

    RLOGD("SendChannelResponse LinkStatus = %d", pstkContext->channelStatus.linkStatus);
    pResMsg->resCode = resCode;
    pResMsg->ChannelId = pstkContext->channelStatus.channelId;
    pResMsg->LinkStatus = pstkContext->channelStatus.linkStatus;
    pResMsg->mode = pstkContext->channelStatus.mode_info;

    onCmdResponse(pstkContext, pResMsg, GET_CHANNEL_STATUS, socket_id);
    free(pResMsg);

    return 1;
}

int SendDataResponse(StkContext *pstkContext, int len, int resCode, int socket_id) {
    RLOGD("SendDataResponse len = %d", len);
    CatResponseMessageSprd *pResMsg = NULL;
    pResMsg = (CatResponseMessageSprd*)calloc(1, sizeof(CatResponseMessageSprd));
    pResMsg->includeAdditionalInfo = false;
    pResMsg->channelData = NULL;

    pstkContext->cmdInProgress = false;

    pResMsg->resCode = resCode;
    pResMsg->channelDataLen = len;

    onCmdResponse(pstkContext, pResMsg, SEND_DATA, socket_id);
    free(pResMsg);

    return 1;
}

int ReceiveDataResponse(StkContext *pstkContext, char *dataStr, int len, int resCode, int socket_id) {
    RLOGD("ReceiveDataResponse len = %d, data = %s", len, dataStr);
    CatResponseMessageSprd *pResMsg = NULL;
    pResMsg = (CatResponseMessageSprd*)calloc(1, sizeof(CatResponseMessageSprd));
    pResMsg->includeAdditionalInfo = false;
    pResMsg->channelData = NULL;

    pstkContext->cmdInProgress = false;

    pResMsg->resCode = resCode;
    pResMsg->channelDataLen = len;
    pResMsg->channelData = strdup(dataStr);

    onCmdResponse(pstkContext, pResMsg, RECEIVE_DATA, socket_id);
    free(pResMsg->channelData);
    free(pResMsg);

    return 1;
}

int SendCloseChannelResponse(StkContext *pstkContext, int resCode, int socket_id) {
    RLOGD("SendCloseChannelResponse");
    CatResponseMessageSprd *pResMsg = NULL;
    pResMsg = (CatResponseMessageSprd*)calloc(1, sizeof(CatResponseMessageSprd));
    pResMsg->includeAdditionalInfo = false;
    pResMsg->channelData = NULL;

    pstkContext->cmdInProgress = false;

    pResMsg->resCode = resCode;
    onCmdResponse(pstkContext, pResMsg, CLOSE_CHANNEL, socket_id);
    free(pResMsg);

    return 1;
}

int sendSetUpEventResponse(StkContext *pstkContext, int event, unsigned char *addedInfo, int socket_id) {
    RLOGD("sendSetUpEventResponse: event = %d, socket_id = %d", event, socket_id);
    CatResponseMessageSprd *pResMsg = NULL;
    pResMsg = (CatResponseMessageSprd*)calloc(1, sizeof(CatResponseMessageSprd));
    pResMsg->includeAdditionalInfo = false;
    pResMsg->channelData = NULL;

    if (event == CHANNEL_STATUS_EVENT) {
        pResMsg->ChannelId = pstkContext->channelStatus.channelId;
        pResMsg->LinkStatus = pstkContext->channelStatus.linkStatus;
        pResMsg->mode = pstkContext->channelStatus.mode_info;
    } else if (event == DATA_AVAILABLE_EVENT) {
        int length = 0;
        if (pstkContext->receiveDataLen > RECEIVE_DATA_MAX_TR_LEN) {
            length = 0xff;
        } else {
            length = pstkContext->receiveDataLen;
        }
        RLOGD("sendEventDataAvailable, length = %d", length);
        pResMsg->channelDataLen = length;
        pResMsg->ChannelId = pstkContext->channelStatus.channelId;
        pResMsg->LinkStatus = pstkContext->channelStatus.linkStatus;
        pResMsg->mode = pstkContext->channelStatus.mode_info;
    }

    pResMsg->resCode = OK;
    pResMsg->eventValue = event;
    pResMsg->addedInfo = addedInfo;

    sendEventLoop(pResMsg, event, socket_id);
    free(pResMsg);

    return 1;
}

void sendEventDataAvailable(StkContext *pstkContext, int socket_id) {
    sendSetUpEventResponse(pstkContext, DATA_AVAILABLE_EVENT, NULL, socket_id);
}

void sendEventChannelStatus(StkContext *pstkContext, int socket_id) {
    RLOGD("sendEventChannelStatus");
    RLOGD("handleDataState disconnected mChannelEstablished = %d",
            pstkContext->channelEstablished);
    if (!(pstkContext->channelEstablished)) {
        pstkContext->channelStatus.channelId = DEFAULT_CHANNELID;
        pstkContext->channelStatus.linkStatus = false;
        pstkContext->channelStatus.mode_info = CHANNEL_MODE_LINK_DROPPED;
    }

    if ((pstkContext->lastChannelStatus.channelId == pstkContext->channelStatus.channelId)
        && (pstkContext->lastChannelStatus.linkStatus == pstkContext->channelStatus.linkStatus)
        && (pstkContext->lastChannelStatus.mode_info == pstkContext->channelStatus.mode_info)) {
        return;
    }
    pstkContext->lastChannelStatus.channelId = pstkContext->channelStatus.channelId;
    pstkContext->lastChannelStatus.linkStatus = pstkContext->channelStatus.linkStatus;
    pstkContext->lastChannelStatus.mode_info = pstkContext->channelStatus.mode_info;

    sendSetUpEventResponse(pstkContext, CHANNEL_STATUS_EVENT, NULL, socket_id);
}

int createSocket(StkContext *pstkContext) {
    RLOGD("[stk] createSocket");
    int enable = 1;
    int fdSocketId = -1;
    int port = pstkContext->pOCData->portNumber;
    char *address = pstkContext->pOCData->DataDstAddress;

    char ifName[NAME_SIZE] = {0};
    snprintf(ifName, sizeof(ifName), "seth_lte%d", pstkContext->openchannelCid - 1);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    RLOGD("createSocket ifName: %s", ifName);
    RLOGD("createSocket address: %s", address);

    if (pstkContext->pOCData->transportType == TRANSPORT_TYPE_TCP) {
        RLOGD("[stk] create tcp socket");
        if ((pstkContext->tcpSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            RLOGE("socket create error");
            return -1;
        }
        RLOGD("createSocket pstkContext->tcpSocket: %d", pstkContext->tcpSocket);
        if (setsockopt(pstkContext->tcpSocket, SOL_SOCKET, SO_BINDTODEVICE,
                ifName, strlen(ifName)) < 0){
            RLOGE("setsockopt error errno = %s", strerror(errno));
            close(pstkContext->tcpSocket);
            pstkContext->tcpSocket = -1;
        }
        fdSocketId = pstkContext->tcpSocket;
        RLOGD("createSocket fdSocketId: %d", fdSocketId);
    } else if (pstkContext->pOCData->transportType == TRANSPORT_TYPE_UDP) {
        RLOGD("[stk] create udp socket");
        if ((pstkContext->udpSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            RLOGE("socket create error");
            return -1;
        }
        RLOGD("createSocket pstkContext->udpSocket:%d", pstkContext->udpSocket);
        if (setsockopt(pstkContext->udpSocket, SOL_SOCKET, SO_BINDTODEVICE, ifName, strlen(ifName)) < 0){
            RLOGE("setsockopt error  errno = %s", strerror(errno));
            close(pstkContext->udpSocket);
            pstkContext->udpSocket = -1;
        }
        fdSocketId = pstkContext->udpSocket;
        RLOGD("createSocket fdSocketId:%d", fdSocketId);
    } else {
        return -1;
    }

    if (inet_pton(AF_INET, address, &addr.sin_addr) <= 0){
        RLOGE("socket inet_pton error errno = %s", strerror(errno));
        return -1;
    }
    if (connect(fdSocketId, (const struct sockaddr *)&addr,
            sizeof(struct sockaddr_in)) < 0) {
        RLOGE("socket connect error errno = %s", strerror(errno));
        return -1;
    }

    pstkContext->needRuning = true;
    return 1;
}

static void *createSocketThread(void *param) {
    RLOGD("[stk] createSocketThread");
    int socket_id = -1;
    int sendTRChannelID = -1;
    StkContext *pstkContext = NULL;

    if (param) {
        pstkContext = (StkContext *)param;
    }
    if (pstkContext == NULL) {
        RLOGE("createSocketThread pstkContext is NULL");
        return NULL;
    }

    socket_id = pstkContext->phone_id;

    if (socket_id < 0 || socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        return NULL;
    }

    pstkContext->channelStatus.channelId = DEFAULT_CHANNELID;
    pstkContext->channelStatus.linkStatus = true;
    pstkContext->channelStatus.mode_info = CHANNEL_MODE_NO_FURTHER_INFO;
    pstkContext->lastChannelStatus.linkStatus = true;

    if (createSocket(pstkContext) > 0) {
        RLOGD("[stk] createSocketThread success");
        RLOGD("[stk] openchannel success buf size = %d", pstkContext->pOCData->bufferSize);
        if (pstkContext->pOCData->bufferSize > DEFAULT_BUFFER_SIZE) {
            pstkContext->pOCData->bufferSize = DEFAULT_BUFFER_SIZE;
            sendChannelResponse(pstkContext, PRFRMD_WITH_MODIFICATION, socket_id);
        } else {
            RLOGD("[stk] sendChannelResponse OK");
            sendChannelResponse(pstkContext, OK, socket_id);
        }
        pstkContext->channelEstablished = true;
    } else {
        RLOGD("[stk] createSocketThread fail");
        pstkContext->channelStatus.channelId = 0;
        pstkContext->channelStatus.linkStatus = false;
        pstkContext->channelStatus.mode_info = CHANNEL_MODE_NO_FURTHER_INFO;
        sendChannelResponse(pstkContext, BEYOND_TERMINAL_CAPABILITY, socket_id);
        pstkContext->channelEstablished = false;
    }

    return NULL;
}

static void *openChannelThread(void *param) {
    RLOGD("[stk] openChannelThread");
    int ret = -1;
    int socket_id = -1;
    int sendTRChannelID = -1;
    char *data = NULL;
    pthread_attr_t attr;
    StkContext *pstkContext = NULL;

    if (param) {
        pstkContext = (StkContext*)param;
    }
    if (pstkContext == NULL) {
        RLOGE("openChannelThread pstkContext is NULL");
        return NULL;
    }

    socket_id = pstkContext->phone_id;

    if (socket_id < 0 || socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        return NULL;
    }

    if ((pstkContext->pOCData->BearerType != BEARER_TYPE_GPRS)
            && (pstkContext->pOCData->BearerType != BEARER_TYPE_DEFAULT)
            && (pstkContext->pOCData->BearerType != BEARER_TYPE_UTRAN_PACKET_SERVICE)) {
        RLOGD("openchannel BearerType error");
        sendChannelResponse(pstkContext, BEYOND_TERMINAL_CAPABILITY, socket_id);
        return NULL;
    }

    if (pstkContext->pOCData->DataDstAddressType != ADDRESS_TYPE_IPV4) {
        RLOGD("DataDstAddressType not ipv4");
        sendChannelResponse(pstkContext, BEYOND_TERMINAL_CAPABILITY, socket_id);
        return NULL;
    }

    if (emNStrlen(pstkContext->pOCData->NetAccessName) != 0) {
        RLOGD("[stk] NetAccessName = %s", pstkContext->pOCData->NetAccessName);
    }
    SetupStkConnect(pstkContext, socket_id);
    if (pstkContext->openchannelCid != -1) {
        RLOGD("[stk] TelephonyManager.DATA_CONNECTED");
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        RLOGD("[stk] CreateSocket run....");
        ret = pthread_create(&pstkContext->openChannelTid, &attr,
                createSocketThread, (void *)pstkContext);
        if (ret < 0) {
            RLOGE("Failed to create createSocketThread errno: %d", errno);
        }
    }
    return NULL;
}

static void *receiveDataThread(void *param) {
    RLOGD("[stk] receiveDataThread");
    int socket_id = -1;
    int ret, index = 0;
    StkContext *pstkContext = NULL;

    if (param) {
        pstkContext = (StkContext*)param;
    }
    if (pstkContext == NULL) {
        RLOGE("receiveDataThread pstkContext is NULL");
        return NULL;
    }
    pthread_mutex_lock(&pstkContext->bipTcpUdpMutex);
    socket_id = pstkContext->phone_id;

    if (socket_id < 0 || socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        pthread_mutex_unlock(&pstkContext->bipTcpUdpMutex);
        return NULL;
    }

    if (pstkContext->pOCData->transportType == TRANSPORT_TYPE_TCP) {
        while (pstkContext->needRuning && (pstkContext->tcpSocket != -1)) {
            RLOGD("[stk] TCP receiveDataThread try to read data from tcpSocket: %d", pstkContext->tcpSocket);
            int count = -1;
            char *p_read = NULL;
            p_read = (char *)calloc(MAX_BUFFER_BYTES, sizeof(char));

            if (!pstkContext->threadInLoop) {
                pstkContext->threadInLoop = true;
            }
            ret = fcntl(pstkContext->tcpSocket, F_SETFL, 0);

            RLOGD("receiveDataThread read data..");
            do {
                count = read(pstkContext->tcpSocket, p_read, MAX_BUFFER_BYTES);
            } while (count < 0 && errno == EINTR);
            RLOGD("read %d from fd %d, erro %s", count, pstkContext->tcpSocket,
                        strerror(errno));

            if (count > 0) {
                pthread_mutex_lock(&pstkContext->receiveDataMutex);
                unsigned char *checkHexStr = (unsigned char *)
                    calloc(count * 2 + 1, sizeof(unsigned char));
                convertBinToHex(p_read, count, checkHexStr);
                RLOGD("lunchReceiveData checkHexStr: %s", checkHexStr);
                free(checkHexStr);

                if (pstkContext->recevieData != NULL) {
                    free(pstkContext->recevieData);
                    pstkContext->recevieData = NULL;
                }
                pstkContext->recevieData =(char *)calloc(count + 1, sizeof(char));
                memcpy(pstkContext->recevieData, p_read, count);
                pstkContext->receiveDataLen = count;
                pstkContext->receiveDataOffset = 0;
                pthread_mutex_unlock(&pstkContext->receiveDataMutex);
                sendEventDataAvailable(pstkContext, socket_id);

                while (pstkContext->needRuning && pstkContext->receiveDataLen > 0
                        && pstkContext->recevieData != NULL) {
                    RLOGD("[stk] TCP receiveDataThread mRecevieData != null , need wait...");
                    usleep(500 * 1000);
                }
            } else if (count <= 0) {  /* read error encountered or EOF reached */
                RLOGD("[stk] TCP receiveDataThread no data available, need wait...");
                usleep(500 * 1000);
            }
            free(p_read);
        }
    } else if (pstkContext->pOCData->transportType == TRANSPORT_TYPE_UDP) {
        while (pstkContext->needRuning && (pstkContext->udpSocket != -1)) {
            RLOGD("[stk] UDP receiveDataThread try to read data from udpSocket: %d",
                    pstkContext->udpSocket);
            int count = -1;
            char *p_read = NULL;
            p_read = (char *)calloc(MAX_BUFFER_BYTES, sizeof(char));

            if (!pstkContext->threadInLoop) {
                pstkContext->threadInLoop = true;
            }
            ret = fcntl(pstkContext->udpSocket, F_SETFL, 0);

            RLOGD("receiveDataThread read data..");
            do {
                count = read(pstkContext->udpSocket, p_read, MAX_BUFFER_BYTES);
            } while (count < 0 && errno == EINTR);
            RLOGD("read %d from fd %d, erro %s", count, pstkContext->udpSocket,
                        strerror(errno));

            if (count > 0) {
                pthread_mutex_lock(&pstkContext->receiveDataMutex);
                unsigned char *checkHexStr = (unsigned char *)
                    calloc(count * 2 + 1, sizeof(unsigned char));
                convertBinToHex(p_read, count, checkHexStr);
                RLOGD("lunchReceiveData checkHexStr: %s", checkHexStr);
                free(checkHexStr);

                if (pstkContext->recevieData != NULL) {
                    free(pstkContext->recevieData);
                    pstkContext->recevieData = NULL;
                }
                pstkContext->recevieData =(char *)calloc(count+1, sizeof(char));
                memcpy(pstkContext->recevieData, p_read, count);
                pstkContext->receiveDataLen = count;
                pstkContext->receiveDataOffset = 0;
                pthread_mutex_unlock(&pstkContext->receiveDataMutex);
                sendEventDataAvailable(pstkContext, socket_id);

                while (pstkContext->needRuning && pstkContext->receiveDataLen > 0
                        && pstkContext->recevieData != NULL) {
                    RLOGD("[stk] UDP receiveDataThread mRecevieData != null , need wait...");
                    usleep(500 * 1000);
                }
            } else if (count <= 0) {  /* read error encountered or EOF reached */
                RLOGD("[stk] UDP receiveDataThread no data available, need wait...");
                usleep(500 * 1000);
            }
            free(p_read);
        }
    } else {
        RLOGD("[stk] receiveDataThread transportType mismatch");
        pthread_mutex_unlock(&pstkContext->bipTcpUdpMutex);
        return NULL;
    }
    pstkContext->threadInLoop = false;
    RLOGD("[stk] receiveDataThread exit...");
    pthread_mutex_unlock(&pstkContext->bipTcpUdpMutex);

    return NULL;
}

static int blockingWrite(int fd, const void *buffer, size_t len) {
    RLOGD("blockingWrite");
    size_t writeOffset = 0;
    const uint8_t *toWrite = NULL;
    toWrite = (const uint8_t *)buffer;
    while (writeOffset < len) {
        ssize_t written = -1;
        do {
            written = write(fd, toWrite + writeOffset,
                                len - writeOffset);
            RLOGD("send to:%d %d byte :%s", fd, (int)(len - writeOffset),
                    toWrite + writeOffset);
        } while (written < 0 && ((errno == EINTR) || (errno == EAGAIN)));
        if (written >= 0) {
            writeOffset += written;
        } else {   // written < 0
            RLOGE("Response: unexpected error on write errno:%d, %s",
                   errno, strerror(errno));
            close(fd);
            return -1;
        }
    }
    return 0;
}

static int sendStrData(StkContext *pstkContext, int fd, const void *data, size_t dataSize) {
    RLOGD("sendStrData");
    int ret = -1;
    uint32_t header = 0;

    if (fd < 0) {
        return -1;
    }
    if (dataSize > MAX_BUFFER_BYTES) {
        RLOGE("packet larger than %u (%u)",
               MAX_BUFFER_BYTES, (unsigned int)dataSize);
        return -1;
    }
    pthread_mutex_lock(&pstkContext->writeStkMutex);
    ret = blockingWrite(fd, data, dataSize);
    if (ret < 0) {
        RLOGE("blockingWrite error");
        pthread_mutex_unlock(&pstkContext->writeStkMutex);
        return ret;
    }

    pthread_mutex_unlock(&pstkContext->writeStkMutex);
    return 0;
}

int CalcChannelDataLen(int mode, int sendlen, StkContext *pstkContext) {
    int retLen = 0;
    RLOGD("CalcChannelDataLen mode = %d, sendlen = %d", mode, sendlen);
    pstkContext->sendDataLen += sendlen;

    if (mode == SEND_DATA_IMMEDIATELY) {
        pstkContext->sendDataLen = 0;
    }
    if (pstkContext->pOCData->bufferSize - pstkContext->sendDataLen
            > RECEIVE_DATA_MAX_TR_LEN) {
        retLen = 0xff;
    } else {
        retLen = pstkContext->pOCData->bufferSize
                - pstkContext->sendDataLen;
    }
    RLOGD("CalcChannelDataLen retLen = %d", retLen);

    return retLen;
}

int lunchOpenChannel(int channel_id) {
    RLOGD("lunchOpenChannel");
    int ret = -1;
    int socket_id = -1;
    char *netAccessName = NULL;
    StkContext *pstkContext = NULL;
    pthread_attr_t attr;

    RLOGD("lunchOpenChannel channel_id = %d", channel_id);
    pstkContext = getStkContext(channel_id);

    if (pstkContext == NULL) {
        RLOGE("lunchOpenChannel pstkContext is NULL");
        return -1;
    }

    socket_id = pstkContext->phone_id;
    if (socket_id < 0 || socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        return -1;
    }

    RLOGD("[%d]pstkContext->pOCData->BearerType is %d", socket_id, pstkContext->pOCData->BearerType);
    RLOGD("[stk] pCmdDet->compRequired = %d", pstkContext->pCmdDet->compRequired);
    RLOGD("[stk] pCmdDet->commandNumber = %d", pstkContext->pCmdDet->commandNumber);
    RLOGD("[stk] pCmdDet->typeOfCommand = %d", pstkContext->pCmdDet->typeOfCommand);
    RLOGD("[stk] pCmdDet->commandQualifier = %d", pstkContext->pCmdDet->commandQualifier);

    if (pstkContext->pOCData->BearerType == BEARER_TYPE_DEFAULT) {
        RLOGD("pstkContext->pOCData->BearerType is BEARER_TYPE_DEFAULT");
        if (emNStrlen(pstkContext->pOCData->NetAccessName) == 0) {
            RLOGD("pstkContext->pOCData->NetAccessName is empty");
            int sendTRChannelID = getChannel(socket_id);
            netAccessName = getDefaultBearerNetAccessName(sendTRChannelID);
            putChannel(sendTRChannelID);
            if (netAccessName != NULL) {
                RLOGD("netAccessName is %s", netAccessName);
                memcpy(pstkContext->pOCData->NetAccessName, netAccessName,
                        emNStrlen(netAccessName) + 1);
            }
        }
        RLOGD("[%d]stkContext->pOCData->NetAccessName is %s", socket_id,
                pstkContext->pOCData->NetAccessName);
    }

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&pstkContext->openChannelTid, &attr, openChannelThread,
            (void *)pstkContext);
    if (ret < 0) {
        RLOGE("Failed to create openChannelThread errno: %d", errno);
        return -1;
    }

    return 1;
}

int lunchGetChannelStatus(int channel_id) {
    RLOGD("lunchGetChannelStatus");
    int socket_id = -1;
    StkContext *pstkContext = NULL;

    RLOGD("lunchGetChannelStatus channel_id = %d", channel_id);
    pstkContext = getStkContext(channel_id);

    if (pstkContext == NULL) {
        RLOGE("lunchGetChannelStatus pstkContext is NULL");
        return -1;
    }

    socket_id = pstkContext->phone_id;
    if (socket_id < 0 || socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        return -1;
    }

    pstkContext->pCStatus = (ChannelStatus *)calloc(1, sizeof(ChannelStatus));;
    pstkContext->pCStatus->channelId = channel_id;
    if (pstkContext->openchannelCid > 0) {
        pstkContext->pCStatus->linkStatus = true;
    } else {
        pstkContext->pCStatus->linkStatus = false;
    }
    pstkContext->pCStatus->mode_info = CHANNEL_MODE_NO_FURTHER_INFO;
    sendChannelStatusResponse(pstkContext, OK, socket_id);

    return 1;
}

int lunchSendData(int channel_id) {
    RLOGD("lunchSendData");
    int ret = -1;
    int port = 0;
    int sendlen = 0;
    int socket_id = -1;
    int channellen = 0;
    int fdSocketId = -1;
    unsigned char type = '\0';
    char *pStr = NULL;
    char *address = NULL;

    pthread_attr_t attr;
    StkContext *pstkContext = NULL;

    RLOGD("lunchSendData channel_id = %d", channel_id);
    pstkContext = getStkContext(channel_id);

    if (pstkContext == NULL) {
        RLOGE("lunchSendData pstkContext is NULL");
        return -1;
    }

    socket_id = pstkContext->phone_id;
    if (socket_id < 0 || socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        return -1;
    }

    RLOGD("lunchSendData send mode:%d, send data: %s",
            pstkContext->pCmdDet->commandQualifier, pstkContext->pSCData->sendDataStr);
    type = pstkContext->pOCData->transportType;
    port = pstkContext->pOCData->portNumber;
    address = pstkContext->pOCData->DataDstAddress;
    sendlen = emNStrlen((char *)pstkContext->pSCData->sendDataStr) / 2;

    RLOGD("[stk] lunchSendData type = %d", type);
    RLOGD("[stk] lunchSendData port = %d", port);
    RLOGD("[stk] lunchSendData address = %s", address);
    RLOGD("[stk] lunchSendData sendlen = %d", sendlen);

    if (pstkContext->pOCData->transportType == TRANSPORT_TYPE_TCP) {
        RLOGD("[stk] lunchSendData send TCP data");
        fdSocketId = pstkContext->tcpSocket;
    } else if (pstkContext->pOCData->transportType == TRANSPORT_TYPE_UDP) {
        RLOGD("[stk] lunchSendData send UDP data");
        fdSocketId = pstkContext->udpSocket;
    } else {
        RLOGD("[stk] lunchSendData transportType mismatch");
        SendDataResponse(pstkContext, 0, BEYOND_TERMINAL_CAPABILITY, socket_id);
        return -1;
    }
    RLOGD("[stk] lunchSendData fdSocketId:%d", fdSocketId);
    if (fdSocketId == -1) {
        RLOGD("[stk] fdSocketId is null, channel not established yet!");
        SendDataResponse(pstkContext, 0, BEYOND_TERMINAL_CAPABILITY, socket_id);
        return -1;
    }
    if (pstkContext->pCmdDet->commandQualifier == SEND_DATA_STORE) {
        RLOGD("[stk] lunchSendData SEND_DATA_STORE");
        strlcat(pstkContext->sendDataStorer, (char *)pstkContext->pSCData->sendDataStr,
                sizeof(pstkContext->sendDataStorer));
        channellen = CalcChannelDataLen(pstkContext->pCmdDet->commandQualifier, sendlen, pstkContext);
        SendDataResponse(pstkContext, channellen, OK, socket_id);
    } else {
        RLOGD("[stk] lunchSendData SEND_DATA_IMMEDIATELY");
        strlcat(pstkContext->sendDataStorer, (char *)pstkContext->pSCData->sendDataStr,
                sizeof(pstkContext->sendDataStorer));
        RLOGD("[stk] pstkContext->pSCData->sendDataStr = %s", pstkContext->pSCData->sendDataStr);
        RLOGD("[stk] pstkContext->sendDataStorer = %s", pstkContext->sendDataStorer);

        pStr = (char *)calloc(emNStrlen(pstkContext->sendDataStorer) / 2 + 1, sizeof(char));
        convertHexToBin(pstkContext->sendDataStorer, emNStrlen(pstkContext->sendDataStorer), pStr);
        sendStrData(pstkContext, fdSocketId, pStr, emNStrlen(pstkContext->sendDataStorer) / 2);
        channellen = CalcChannelDataLen(pstkContext->pCmdDet->commandQualifier, sendlen, pstkContext);
        SendDataResponse(pstkContext, channellen, OK, socket_id);
        free(pStr);

        if (!pstkContext->threadInLoop) {
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
            RLOGD("[stk] fdSocketId receive thread start");
            ret = pthread_create(&pstkContext->sendDataTid, &attr,
                    receiveDataThread, (void *)pstkContext);
            if (ret < 0) {
                RLOGE("Failed to create openChannelThread errno: %d", errno);
            }
        }
        memset(pstkContext->sendDataStorer, 0 , sizeof(pstkContext->sendDataStorer));
    }

    return 1;
}

int lunchReceiveData(int channel_id) {
    RLOGD("lunchReceiveData");
    int count = 0;
    int remainLen = 0;
    int socket_id = -1;
    int channelDataLength = 0;
    char *from = NULL;
    char *to = NULL;
    char *dataTmp = NULL;
    unsigned char *hexString = NULL;
    StkContext *pstkContext = NULL;

    RLOGD("lunchReceiveData channel_id = %d", channel_id);
    pstkContext = getStkContext(channel_id);

    if (pstkContext == NULL) {
        RLOGE("lunchReceiveData pstkContext is NULL");
        return -1;
    }

    socket_id = pstkContext->phone_id;
    if (socket_id < 0 || socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        return -1;
    }

    RLOGD("lunchReceiveData channel datalen:%d, receiveDataLen = %d, receiveDataOffset = %d"
            , pstkContext->pRCData->channelDataLength
            , pstkContext->receiveDataLen, pstkContext->receiveDataOffset);

    pthread_mutex_lock(&pstkContext->receiveDataMutex);
    if (pstkContext->receiveDataLen > 0 && pstkContext->recevieData != NULL) {
        if (pstkContext->pRCData->channelDataLength >= RECEIVE_DATA_MAX_TR_LEN) {
            if (pstkContext->receiveDataLen - RECEIVE_DATA_MAX_TR_LEN > RECEIVE_DATA_MAX_TR_LEN) {
                remainLen = 0xff;
            } else {
                remainLen = pstkContext->receiveDataLen - RECEIVE_DATA_MAX_TR_LEN;
            }
            dataTmp = (char *)calloc(RECEIVE_DATA_MAX_TR_LEN, sizeof(char));
            from = pstkContext->recevieData;
            to = dataTmp;
            from += pstkContext->receiveDataOffset;
            count = RECEIVE_DATA_MAX_TR_LEN;
            while (count-- > 0) {
                *to++ = *from++;
            }
            hexString = (unsigned char *)
                calloc(RECEIVE_DATA_MAX_TR_LEN * 2 + 1, sizeof(unsigned char));
            convertBinToHex(dataTmp, RECEIVE_DATA_MAX_TR_LEN, hexString);
            free(dataTmp);
            RLOGD("lunchReceiveData hexString:%s", hexString);

            pstkContext->receiveDataLen -= RECEIVE_DATA_MAX_TR_LEN;
            pstkContext->receiveDataOffset += RECEIVE_DATA_MAX_TR_LEN;
        } else {
            channelDataLength = pstkContext->pRCData->channelDataLength;
            if (pstkContext->receiveDataLen > pstkContext->pRCData->channelDataLength) {
                remainLen = pstkContext->receiveDataLen - pstkContext->pRCData->channelDataLength;
                if (remainLen > RECEIVE_DATA_MAX_TR_LEN) {
                    remainLen = 0xFF;
                }
            } else {
                remainLen = 0;
                channelDataLength = pstkContext->receiveDataLen;
            }

            dataTmp = (char *)calloc(RECEIVE_DATA_MAX_TR_LEN + 1, sizeof(char));
            from = pstkContext->recevieData;
            to = dataTmp;
            from += pstkContext->receiveDataOffset;
            count = channelDataLength;
            RLOGD("lunchReceiveData count:%d", count);
            while (count-- > 0) {
                *to++ = *from++;
            }
            hexString = (unsigned char *)
                calloc(channelDataLength * 2 + 1, sizeof(unsigned char));
            convertBinToHex(dataTmp, channelDataLength, hexString);
            free(dataTmp);
            RLOGD("lunchReceiveData hexString:%s", hexString);

            if (remainLen == 0) {
                pstkContext->receiveDataLen = 0;
                pstkContext->receiveDataOffset = 0;
            } else {
                pstkContext->receiveDataLen -= channelDataLength;
                pstkContext->receiveDataOffset += channelDataLength;
            }
        }
        if (pstkContext->receiveDataLen <= 0) {
            free(pstkContext->recevieData);
            pstkContext->recevieData = NULL;
        }
        ReceiveDataResponse(pstkContext, (char *)hexString, remainLen, OK, socket_id);
        free(hexString);
    } else {
        RLOGD("receiveData no data");
        ReceiveDataResponse(pstkContext, NULL, 0, BEYOND_TERMINAL_CAPABILITY, socket_id);
    }
    pthread_mutex_unlock(&pstkContext->receiveDataMutex);

    return 1;

}

int lunchCloseChannel(int channel_id) {
    RLOGD("lunchCloseChannel");
    int socket_id = -1;
    StkContext *pstkContext = NULL;

    RLOGD("lunchReceiveData channel_id = %d", channel_id);
    pstkContext = getStkContext(channel_id);

    if (pstkContext == NULL) {
        RLOGE("lunchCloseChannel pstkContext is NULL");
        return -1;
    }
    pstkContext->needRuning = false;

    socket_id = pstkContext->phone_id;
    if (socket_id < 0 || socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        return -1;
    }

    pthread_mutex_lock(&pstkContext->closeChannelMutex);
    RLOGD("closeSocket");
    if (pstkContext->tcpSocket != -1) {
        RLOGD("close tcpSocket");
        close(pstkContext->tcpSocket);
        pstkContext->tcpSocket = -1;
    } else if (pstkContext->udpSocket != -1) {
        RLOGD("close udpSocket");
        close(pstkContext->udpSocket);
        pstkContext->udpSocket = -1;
    }
    usleep(500 * 1000);
    pstkContext->receiveDataLen = 0;
    pstkContext->receiveDataOffset = 0;
    pstkContext->sendDataLen = 0;

    if (pstkContext->pOCData != NULL) {
        RLOGD("pstkContext->pOCData is not NULL");
        endConnectivity(pstkContext, socket_id);
        SendCloseChannelResponse(pstkContext, OK, socket_id);
    } else {
        RLOGD("closeChannel channel already closed");
        SendCloseChannelResponse(pstkContext, BIP_ERROR, socket_id);
    }
    pthread_mutex_unlock(&pstkContext->closeChannelMutex);
    return 1;
}
