/**
 * ril_stk_parser.c --- stk tlv parser functions implementation
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#define LOG_TAG "RIL_STK_PARSER"

#include "ril_stk_parser.h"
#include "ril_stk_bip.h"
#include "ril_utils.h"

#define FUNCTION_TRACE_START(f)     RLOGD(#f"_start")
#define FUNCTION_TRACE_END(f)       RLOGD(#f"_end")

#define ADDRESS_TYPE_IPV4           0X21

int decode(char *data, ComprehensionTlv *pTlv) {
    int tag = 0;
    int cr = 0;
    int length;
    int temp = 0;

    if (data == NULL || pTlv == NULL) {
        return CMD_DATA_NOT_UNDERSTOOD;
    }

    char *pByt = data;
    memset(pTlv, 0, sizeof(TLV));

    temp = *(pByt++) & 0xff;
    switch (temp) {
        case 0:
            tag = 0;
            cr = 0;
            break;
        case 0xff:
        case 0x80:
            return CMD_DATA_NOT_UNDERSTOOD;
        case 0x7f:  // tag is in thread-byte format
            tag = (*pByt & 0xff) << 8;
            tag |= *(pByt++) & 0xff;
            cr = (tag & 0x8000) == 0 ? 0 : 1;
            tag &= ~0x8000;
            pByt += 2;
            break;
        default:  // tag is in single-byte format
            tag = temp;
            cr = (tag & 0x80) == 0 ? 0 : 1;
            tag &= ~0x80;
            break;
    }

    /* length */
    temp = *(pByt++) & 0xff;
    if (temp < 0x80) {
        length = temp;
    } else if (temp == 0x81) {
        length = *(pByt++) & 0xff;
        if (length < 0x80) {
            return CMD_DATA_NOT_UNDERSTOOD;
        }
    } else if (temp == 0x82) {
        length = (*pByt & 0xff) << 8;
        length |= *(pByt++) & 0xff;
        pByt += 2;
        if (length < 0x100) {
            return CMD_DATA_NOT_UNDERSTOOD;
        }
    } else if (temp == 0x83) {
        length = (*pByt & 0xff) << 16;
        length |= (*(pByt++) & 0xff) << 8;
        pByt += 2;
        length |= *pByt & 0xff;
        pByt += 3;
        if (length < 0x10000) {
            return CMD_DATA_NOT_UNDERSTOOD;
        }
    } else {
        return CMD_DATA_NOT_UNDERSTOOD;
    }

    char *pBytData = NULL;
    pBytData = (char *)calloc(length, sizeof(char));

    memcpy(pBytData, pByt, length);

    pTlv->tTlv.tag = tag;
    pTlv->tTlv.length = length;
    pTlv->tTlv.cr = cr;
    pTlv->tTlv.value = pBytData;

    return OK;
}

int parseProCommand(const char * const rawData, const int length, BerTlv *berTlv) {
    FUNCTION_TRACE_START(parseProCommand);

    int proCmdTag = 0;
    int proCmdLen = 0;
    int temp = 0;
    int ret = RESULT_EXCEPTION;
    bool isLengthValid = true;
    char *pProCmd = NULL;
    char *pStr = NULL;

    if (rawData == NULL || length <= 0) {
        RLOGE("invalid parameters: rawData or length");
        return REQUIRED_VALUES_MISSING;
    }

    pProCmd = (char *)calloc(length / 2, sizeof(char));
    RLOGD("parseProCommand length: %d", length);
    RLOGD("parseProCommand rawData: %s", rawData);
    if (RESULT_EXCEPTION == convertHexToBin(rawData, length, pProCmd)) {
        // TODO: need to send TR to SIM?
        ret = RESULT_EXCEPTION;
        goto EXIT;
    }

    // Decode the proactive command
    pStr = pProCmd;

    // TLV-Tag: First segment comes the proactive command TAG
    proCmdTag = *(pStr++) & 0xff;
    if (PROACTIVE_COMMAND_TAG != proCmdTag) {
        ret = CMD_DATA_NOT_UNDERSTOOD;
        goto EXIT;
    }

    /* TLV-length*/
    temp = *(pStr++) & 0xff;
    if (temp < 0x80) { // missed 0x80?
        proCmdLen = temp;
    } else if (temp == 0x81) {
        temp = *(pStr++) & 0xff;
        if (temp < 0x80) {
            ret = CMD_DATA_NOT_UNDERSTOOD;
            goto EXIT;
        }
        proCmdLen = temp;
    } else {
        ret = CMD_DATA_NOT_UNDERSTOOD;
        goto EXIT;
    }

    if (proCmdLen < 9) {
        ret = CMD_DATA_NOT_UNDERSTOOD;
        goto EXIT;
    }

    ComprehensionTlv *head = NULL, *tem = NULL, *tail = NULL;
    int count = 0;
    int ii = 1;

    while (count < temp) {
        tem = (ComprehensionTlv *)calloc(1, sizeof(ComprehensionTlv));
        if (tem == NULL) {
            break;
        } else {
            if (CMD_DATA_NOT_UNDERSTOOD == decode(pStr, tem)) {
                goto EXIT;
            }
            pStr += tem->tTlv.length;
            if (tem->tTlv.length >= 0x80 && tem->tTlv.length <= 0xFF) {
                pStr += 3;
            } else if (tem->tTlv.length >= 0 && tem->tTlv.length < 0x80) {
                pStr += 2;
            }
            count += tem->tTlv.length;
            if (tem->tTlv.length >= 0x80 && tem->tTlv.length <= 0xFF) {
                count += 3;
            } else if (tem->tTlv.length >= 0 && tem->tTlv.length < 0x80) {
                count += 2;
            }

            RLOGD("tem->tTlv[%d].tag is: %d", ii, tem->tTlv.tag);
            RLOGD("tem->tTlv[%d].length is: %d", ii, tem->tTlv.length);
            RLOGD("tem->tTlv[%d].cr is: %d", ii, tem->tTlv.cr);

            ii++;

            tem->next = NULL;
            if (head == NULL) {
                head = tail = tem;
            } else {
                tail->next = tem;
                tail = tem;
            }
        }
    }

    ComprehensionTlv *tempCtlv = NULL;
    tempCtlv = head;

    int totalLength = 0;
    while (tempCtlv != NULL) {
        int itemLength = tempCtlv->tTlv.length;
        if (itemLength >= 0x80 && itemLength <= 0xFF) {
            totalLength += itemLength + 3;  // 3: 'tag'(1 byte) and 'length'(2 bytes).
        } else if (itemLength >= 0 && itemLength < 0x80) {
            totalLength += itemLength + 2;  // 2: 'tag'(1 byte) and 'length'(1 byte).
        } else {
            isLengthValid = false;
            break;
        }
        tempCtlv = tempCtlv->next;
     }

    if (proCmdLen != totalLength) {
        isLengthValid = false;
    }

    berTlv->tag = proCmdTag;
    berTlv->compTlvs = head;
    berTlv->lengthValid = isLengthValid;
    ret = OK;
    goto EXIT;

EXIT:
    free(pProCmd);
    FUNCTION_TRACE_END(parseProCommand);
    return ret;
}

int processCommandDetails(ComprehensionTlv *comprehensionTlv, CommandDetails *cmdDet) {
    int length = 0;
    TLV *tlv = NULL;
    char *pStr = NULL;

    while (comprehensionTlv != NULL) {
        RLOGD("processCommandDetails: %d", comprehensionTlv->tTlv.tag);
        if (COMMAND_DETAILS == comprehensionTlv->tTlv.tag) {
            length = comprehensionTlv->tTlv.length;
            RLOGD("processCommandDetails-length: %d", comprehensionTlv->tTlv.length);
            if (length < 3) {
                return -1;
            }

            pStr = comprehensionTlv->tTlv.value;

            cmdDet->compRequired = comprehensionTlv->tTlv.cr;
            cmdDet->commandNumber = *(pStr++) & 0xff;
            cmdDet->typeOfCommand = *(pStr++) & 0xff;
            cmdDet->commandQualifier = (*pStr) & 0xff;
            RLOGD("compRequired: %d", comprehensionTlv->tTlv.cr);
            RLOGD("commandNumber: %d", cmdDet->commandNumber);
            RLOGD("typeOfCommand: %d", cmdDet->typeOfCommand);
            RLOGD("commandQualifier: %d", cmdDet->commandQualifier);
            return 0;
        }
        comprehensionTlv = comprehensionTlv->next;
    }
    return -1;
}

void retrieveIconId(char *data, IconId *iconId) {
    char *pStr = data;
    memset(iconId, 0, sizeof(IconId));

    iconId->selfExplanatory = ((*(pStr++) & 0xff) == 0x00);
    iconId->recordNumber = (*(pStr) & 0xff);
    RLOGD("[retrieveIconId]selfExplanatory: %d", iconId->selfExplanatory);
    RLOGD("[retrieveIconId]recordNumber: %d", iconId->recordNumber);
}

void processOpenChannel(ComprehensionTlv *comprehensionTlv,
        OpenChannelData *openChannelData, RilMessage *rilMessage) {
    RLOGD("processOpenChannel");

    int length = 0;
    int textStringCount = 0;
    int otherAddressCount = 0;
    char *pStr = NULL;

    while (comprehensionTlv != NULL) {
        RLOGD("[ril_stk_parser]tag: %d", comprehensionTlv->tTlv.tag);
        if (ALPHA_ID == comprehensionTlv->tTlv.tag) {
            length = comprehensionTlv->tTlv.length;
            RLOGD("ALPHA_ID-length: %d", length);
            if (length <= 0) {
                openChannelData->channelData.text = NULL;
                openChannelData->channelData.isNullAlphaId = true;
            } else {
                pStr = comprehensionTlv->tTlv.value;
                if (pStr != NULL) {
                    unsigned char *temp = (unsigned char *)
                            calloc(strlen(pStr) + 1, sizeof(unsigned char));
                    convertUcsToUtf8((unsigned char *)pStr, length, temp);
                    openChannelData->channelData.text = temp;
                    RLOGD("openChannelData-text: %s", openChannelData->channelData.text);
                    if (openChannelData->channelData.text != NULL) {
                        openChannelData->alphaIdLength = length;
                        openChannelData->channelData.isNullAlphaId = false;
                        RLOGD("OpenChannel Alpha identifier done");
                    } else {
                        openChannelData->channelData.isNullAlphaId = true;
                        RLOGD("OpenChannel null Alpha id");
                    }
                 }
             }
        } else if (ICON_ID == comprehensionTlv->tTlv.tag) {
            length = comprehensionTlv->tTlv.length;
            RLOGD("ICON_ID-length: %d", length);
            if (length > 0) {
                pStr = comprehensionTlv->tTlv.value;
                if (pStr != NULL) {
                    IconId *iconId = (IconId *)alloca(sizeof(IconId));
                    retrieveIconId(pStr, iconId);
                    openChannelData->channelData.iconSelfExplanatory =
                            iconId->selfExplanatory;
                    RLOGD("OpenChannel Icon identifier done");
                 }
            }
        } else if (BEARER_DESCRIPTION == comprehensionTlv->tTlv.tag) {
            length = comprehensionTlv->tTlv.length;
            RLOGD("BEARER_DESCRIPTION-length: %d", length);
            if (length <= 0) {
                rilMessage->resCode = CMD_DATA_NOT_UNDERSTOOD;
                break;
            }
            pStr = comprehensionTlv->tTlv.value;
            if (pStr != NULL) {
                openChannelData->BearerType = *(pStr++);
                RLOGD("BearerType: %d", openChannelData->BearerType);
                if (length > 1) {
                    unsigned char temp[ARRAY_SIZE] = {0};
                    convertBinToHex(pStr,(length -1), temp);
                    snprintf(openChannelData->BearerParam,
                            sizeof(openChannelData->BearerParam), "%s", temp);
                    RLOGD("BearerParam: %s", openChannelData->BearerParam);
                    RLOGD("OpenChannel Bearer description done");
                }
            }
        } else if (BUFFER_SIZE == comprehensionTlv->tTlv.tag) {
            length = comprehensionTlv->tTlv.length;
            RLOGD("BUFFER_SIZE-length: %d", length);
            if (length <= 0) {
               rilMessage->resCode = REQUIRED_VALUES_MISSING;
               break;
            }
            pStr = comprehensionTlv->tTlv.value;
            if (pStr != NULL) {
                int bufferSize;
                if (length < 2) {
                    rilMessage->resCode = CMD_DATA_NOT_UNDERSTOOD;
                    break;
                }
                bufferSize = (*pStr++ & 0xff) << 8;
                bufferSize |= (*pStr & 0xff);
                openChannelData->bufferSize = bufferSize;
                RLOGD("bufferSize: %d", openChannelData->bufferSize);
                RLOGD("OpenChannel Buffer size done");
            }
        } else if (NETWORK_ACCESS_NAME == comprehensionTlv->tTlv.tag) {
            length = comprehensionTlv->tTlv.length;
            RLOGD("NETWORK_ACCESS_NAME-length: %d", length);
            if (length <= 0) {
                memset(openChannelData->NetAccessName, 0,
                        sizeof(openChannelData->NetAccessName));
            } else {
                pStr = comprehensionTlv->tTlv.value;
                if (pStr != NULL) {
                    int index = 0;
                    int tempLen = 0;
                    int i = 0, j = 0;
                    int result = 0;
                    unsigned char tmp[ARRAY_SIZE * 2] = {0};
                    while (length > 1) {
                        tempLen = *(pStr++);
                        if (tempLen < length) {
                            for (i = 0; i < tempLen; i++) {
                                result += utf8Package(tmp, result, *(pStr++));
                            }
                            length = length - (tempLen + 1);
                            if (length > 1) {
                                result += utf8Package(tmp, result, 46);
                            } else {
                                break;
                            }
                        } else {
                            for (j = 0; i < length - 1; i++) {
                                result += utf8Package(tmp, result, *(pStr++));
                            }
                            break;
                        }
                    }
                    snprintf(openChannelData->NetAccessName,
                            sizeof(openChannelData->NetAccessName), "%s", tmp);
                    RLOGD("NetAccessName: %s", openChannelData->NetAccessName);
                    RLOGD("OpenChannel Network Access Name done");
                }
            }
        } else if (OTHER_ADDRESS == comprehensionTlv->tTlv.tag) {
            RLOGD("OTHER_ADDRESS count: %d", otherAddressCount);
            char temp[ARRAY_SIZE] = {0};
            if (otherAddressCount == 0) {
                length = comprehensionTlv->tTlv.length;
                RLOGD("OTHER_ADDRESS length: %d", length);
                if (length > 0) {
                    pStr = comprehensionTlv->tTlv.value;
                    if (pStr != NULL) {
                        if (length > 1) {
                            openChannelData->OtherAddressType = *(pStr++);
                            RLOGD("OtherAddressType: %d", openChannelData->OtherAddressType);
                            openChannelData->DataDstAddressType = openChannelData->OtherAddressType;
                            RLOGD("DataDstAddressType: %d", openChannelData->DataDstAddressType);
                            if (openChannelData->OtherAddressType ==
                                    ADDRESS_TYPE_IPV4 && ((length - 1) == 4)) {
                                int net[4] = {0};
                                int i = 0;
                                for (i = 0; i < 4; i++, pStr++) {
                                    net[i] = *(pStr) & 0xff;
                                }
                                snprintf(temp, sizeof(temp), "%d.%d.%d.%d",
                                        net[0], net[1], net[2],net[3]);
                                memcpy(openChannelData->OtherAddress, temp,
                                        strlen((const char *)temp) + 1);
                                memcpy(openChannelData->DataDstAddress, temp,
                                        strlen((const char *)temp) + 1);
                                RLOGD("OtherAddress: %s", openChannelData->OtherAddress);
                                RLOGD("DataDstAddress: %s", openChannelData->DataDstAddress);
                                RLOGD("OpenChannel Network Access IPV4 Name done");
                            } else if (openChannelData->OtherAddressType ==
                                    ADDRESS_TYPE_IPV6 && ((length - 1) == 16)) {
                                int i,j;
                                char pStrtmp[48] = {0};
                                convertBinToHex(pStr, length - 1, pStrtmp);
                                for (i = 0, j = 0; i < 32; i++, j++) {
                                    temp[j] = pStrtmp[i];
                                    if (i % 4 == 3) {
                                        temp[++j] = ':';
                                    }
                                }
                                temp[j - 1] = '\0';
                                memcpy(openChannelData->OtherAddress, temp, strlen((const char *)temp) + 1);
                                memcpy(openChannelData->DataDstAddress, temp, strlen((const char *)temp) + 1);
                                RLOGD("OtherAddress: %s", openChannelData->OtherAddress);
                                RLOGD("DataDstAddress: %s", openChannelData->DataDstAddress);
                                RLOGD("OpenChannel Network Access IPV6 Name done");
                            } else {
                                memset(openChannelData->OtherAddress, 0,
                                        sizeof(openChannelData->OtherAddress));
                                openChannelData->OtherAddressType = 0;
                                RLOGD("OpenChannel local Address is not ipv4 format");
                            }
                        }
                    }
                } else {
                    RLOGD("OpenChannel local address tag length error");
                }
                otherAddressCount++;
            } else {
                length = comprehensionTlv->tTlv.length;
                RLOGD("DataDstAddress length: %d", length);
                if (length > 0) {
                    pStr = comprehensionTlv->tTlv.value;
                    if (pStr != NULL) {
                        if (length > 1) {
                            openChannelData->DataDstAddressType = *(pStr++);
                            RLOGD("DataDstAddressType: %d", openChannelData->DataDstAddressType);
                            if (openChannelData->DataDstAddressType ==
                                    ADDRESS_TYPE_IPV4 && ((length - 1) == 4)) {
                                int net[4] = {0};
                                int i = 0;
                                for (i = 0; i < 4; i++, pStr++) {
                                    net[i] = *(pStr) & 0xff;
                                }
                                snprintf(temp, sizeof(temp), "%d.%d.%d.%d",
                                        net[0], net[1], net[2],net[3]);
                                snprintf(openChannelData->DataDstAddress,
                                        sizeof(openChannelData->DataDstAddress), "%s", temp);
                                RLOGD("DataDstAddress: %s", openChannelData->DataDstAddress);
                                RLOGD("OpenChannel Data destination IPV4 address done");
                            } else if (openChannelData->DataDstAddressType ==
                                    ADDRESS_TYPE_IPV6 && ((length - 1) == 16)) {
                                int i,j;
                                char pStrtmp[48] = {0};
                                convertBinToHex(pStr, length - 1, pStrtmp);
                                for (i = 0, j = 0; i < 32; i++, j++) {
                                    temp[j] = pStrtmp[i];
                                    if (i % 4 == 3) {
                                        temp[++j] = ':';
                                    }
                                }
                                temp[j - 1] = '\0';
                                memcpy(openChannelData->DataDstAddress, temp, strlen((const char *)temp) + 1);
                                RLOGD("DataDstAddress: %s", openChannelData->DataDstAddress);
                                RLOGD("OpenChannel Data destination IPV6 Name done");
                            } else {
                                memset(openChannelData->DataDstAddress, 0,
                                        sizeof(openChannelData->DataDstAddress));
                                openChannelData->DataDstAddressType = 0;
                                RLOGD("OpenChannel Data destination address is not ipv4 format");
                            }
                        }
                    }
                } else {
                    RLOGD("OpenChannel Data destination address tag length error");
                }
                otherAddressCount--;
            }
        } else if (TEXT_STRING == comprehensionTlv->tTlv.tag) {
            RLOGD("TEXT_STRING: %d", textStringCount);
            if (textStringCount == 0) {
                length = comprehensionTlv->tTlv.length;
                RLOGD("LoginStr-length: %d", length);
                if (length <= 0) {
                    memset(openChannelData->LoginStr, 0,
                            sizeof(openChannelData->LoginStr));
                } else {
                    pStr = comprehensionTlv->tTlv.value;
                    if (pStr != NULL) {
                        unsigned char temp[ARRAY_SIZE / 4] = {0};
                        convertUcsToUtf8((unsigned char *)(++pStr), (length - 1), temp);
                        memcpy(openChannelData->LoginStr, temp, strlen((const char *)temp) + 1);
                        RLOGD("LoginStr: %s", openChannelData->LoginStr);
                        RLOGD("OpenChannel User login done");
                    }
                }
                textStringCount++;
            } else {
                length = comprehensionTlv->tTlv.length;
                RLOGD("PwdStr-length: %d", length);
                if (length <= 0) {
                    memset(openChannelData->PwdStr, 0,
                            sizeof(openChannelData->PwdStr));
                } else {
                    pStr = comprehensionTlv->tTlv.value;
                    if (pStr != NULL) {
                        unsigned char temp[ARRAY_SIZE / 4] = {0};
                        convertUcsToUtf8((unsigned char *)(++pStr), (length - 1), temp);
                        memcpy(openChannelData->PwdStr, temp, strlen((const char *)temp) + 1);
                        RLOGD("PwdStr: %s", openChannelData->PwdStr);
                        RLOGD("OpenChannel User password done");
                    }
                }
                textStringCount--;
            }
        } else if (TRANSPORT_LEVEL == comprehensionTlv->tTlv.tag) {
            length = comprehensionTlv->tTlv.length;
            RLOGD("TRANSPORT_LEVEL length: %d", length);
            if (length > 2) {
                pStr = comprehensionTlv->tTlv.value;
                if (pStr != NULL) {
                    openChannelData->transportType = *(pStr++);
                    RLOGD("transportType: %d", openChannelData->transportType);
                    int tempBuffer;
                    tempBuffer = (*(pStr++) & 0xff) << 8;
                    tempBuffer |= (*(pStr) & 0xff);
                    openChannelData->portNumber = tempBuffer;
                    RLOGD("portNumber: %d", openChannelData->portNumber);
                    RLOGD("OpenChannel transport level done");
                }
            }
        }
        comprehensionTlv = comprehensionTlv->next;
    }
    rilMessage->resCode = OK;
    return;
}

void retrieveDeviceIdentities(char *data, DeviceIdentities *deviceIdentities) {
     char *pStr = data;
     memset(deviceIdentities, 0, sizeof(DeviceIdentities));

     deviceIdentities->sourceId = *(pStr++) & 0xff;
     deviceIdentities->destinationId = *pStr & 0xff;
     RLOGD("[retrieveDeviceIdentities]: %d", deviceIdentities->sourceId);
     RLOGD("[retrieveDeviceIdentities]: %d", deviceIdentities->destinationId);
}

void processSendData(ComprehensionTlv *comprehensionTlv,
        SendChannelData *sendData, RilMessage *rilMessage) {
    RLOGD("process SendData");
    int length = 0;
    char *pStr = NULL;
    IconId *iconId = NULL;

    while (comprehensionTlv != NULL) {
        if (DEVICE_IDENTITIES == comprehensionTlv->tTlv.tag) {
            length = comprehensionTlv->tTlv.length;
            RLOGD("DEVICE_IDENTITIES: %d",length);
            if (length < 2) {
                rilMessage->resCode = REQUIRED_VALUES_MISSING;
                break;
            }
            pStr = comprehensionTlv->tTlv.value;
            if (pStr != NULL) {
                DeviceIdentities *deviceIdentities = NULL;
                deviceIdentities = (DeviceIdentities *)alloca(sizeof(DeviceIdentities));
                retrieveDeviceIdentities(pStr, deviceIdentities);
                sendData->channelId = (deviceIdentities->destinationId & 0x0f);
                sendData->deviceIdentities.sourceId = deviceIdentities->sourceId;
                sendData->deviceIdentities.destinationId = deviceIdentities->destinationId;
                RLOGD("process device identities done");
            }
        } else if (ALPHA_ID == comprehensionTlv->tTlv.tag) {
            length = comprehensionTlv->tTlv.length;
            RLOGD("ALPHA_ID-length: %d", length);
            if (length <= 0) {
                sendData->channelData.text = NULL;
            } else {
                pStr = comprehensionTlv->tTlv.value;
                if (pStr != NULL) {
                    unsigned char *temp = (unsigned char *)
                            calloc(strlen(pStr) + 1, sizeof(unsigned char));
                    convertUcsToUtf8((unsigned char *)pStr, length, temp);
                    sendData->channelData.text = temp;
                    RLOGD("sendData-text: %s", sendData->channelData.text);
                 }
            }
        } else if (ICON_ID == comprehensionTlv->tTlv.tag) {
            length = comprehensionTlv->tTlv.length;
            RLOGD("ICON_ID-length: %d", length);
            if (length > 0) {
                pStr = comprehensionTlv->tTlv.value;
                if (pStr != NULL) {
                    iconId = (IconId *)alloca(sizeof(IconId));
                    retrieveIconId(pStr, iconId);
                    sendData->channelData.iconSelfExplanatory = iconId->selfExplanatory;
                    RLOGD("sendData Icon identifier done");
                 }
            }
        } else if (CHANNEL_DATA == comprehensionTlv->tTlv.tag) {
            length = comprehensionTlv->tTlv.length;
            RLOGD("CHANNEL_DATA length: %d", length);
            if (length < 0) {
                rilMessage->resCode = REQUIRED_VALUES_MISSING;
                break;
            }
            pStr = comprehensionTlv->tTlv.value;
            if (pStr != NULL) {
                unsigned char *hexAuthData = (unsigned char *)
                        calloc(length * 2 + 1, sizeof(unsigned char));
                convertBinToHex(pStr, length, hexAuthData);
                sendData->sendDataStr = hexAuthData;
                RLOGD("sendDataStr: %s", sendData->sendDataStr);
                RLOGD("SendData channel data done");
            }
        }
        comprehensionTlv = comprehensionTlv->next;
    }
    rilMessage->resCode = OK;
    return;
}

void processReceiveData(ComprehensionTlv *comprehensionTlv,
        ReceiveChannelData *receiveData, RilMessage *rilMessage) {
    RLOGD("processReceiveData");
    int length = 0;
    char *pStr = NULL;
    IconId *iconId = NULL;
    DeviceIdentities *deviceIdentities = NULL;

    while (comprehensionTlv != NULL) {
        if (DEVICE_IDENTITIES == comprehensionTlv->tTlv.tag) {
            length = comprehensionTlv->tTlv.length;
            RLOGD("DEVICE_IDENTITIES: %d", length);
            if (length < 2) {
                rilMessage->resCode = CMD_DATA_NOT_UNDERSTOOD;
                break;
            }
            pStr = comprehensionTlv->tTlv.value;
            if (pStr != NULL) {
                DeviceIdentities *deviceIdentities = NULL;
                deviceIdentities = (DeviceIdentities *)alloca(sizeof(DeviceIdentities));
                retrieveDeviceIdentities(pStr, deviceIdentities);
                receiveData->channelId = (deviceIdentities->destinationId & 0x0f);
                receiveData->deviceIdentities.sourceId = deviceIdentities->sourceId;
                receiveData->deviceIdentities.destinationId = deviceIdentities->destinationId;
                RLOGD("process device identities done");
            }
        } else if (ALPHA_ID == comprehensionTlv->tTlv.tag) {
            length = comprehensionTlv->tTlv.length;
            RLOGD("ALPHA_ID-length: %d", length);
            if (length <= 0) {
                receiveData->channelData.text = NULL;
            } else {
                pStr = comprehensionTlv->tTlv.value;
                if (pStr != NULL) {
                    unsigned char *temp = (unsigned char *)
                            calloc(strlen(pStr) + 1, sizeof(unsigned char));
                    convertUcsToUtf8((unsigned char *)pStr, length, temp);
                    receiveData->channelData.text = temp;
                    RLOGD("receiveData-text: %s", receiveData->channelData.text);
                 }
            }
        } else if (ICON_ID == comprehensionTlv->tTlv.tag) {
            length = comprehensionTlv->tTlv.length;
            RLOGD("ICON_ID-length: %d", length);
            if (length > 0) {
                pStr = comprehensionTlv->tTlv.value;
                if (pStr != NULL) {
                    iconId = (IconId *)alloca(sizeof(IconId));
                    retrieveIconId(pStr, iconId);
                    receiveData->channelData.iconSelfExplanatory = iconId->selfExplanatory;
                    RLOGD("receiveData Icon identifier done");
                 }
            }
        } else if (CHANNEL_DATA_LENGTH == comprehensionTlv->tTlv.tag) {
            length = comprehensionTlv->tTlv.length;
            RLOGD("CHANNEL_DATA length: %d", length);
            if (length < 0) {
                rilMessage->resCode = REQUIRED_VALUES_MISSING;
                break;
            }
            pStr = comprehensionTlv->tTlv.value;
            if (pStr != NULL) {
                receiveData->channelDataLength = *pStr & 0xff;
                RLOGD("channelDataLength: %d", receiveData->channelDataLength);
                RLOGD("receiveData channelDataLength done");
            }
        }
        comprehensionTlv = comprehensionTlv->next;
    }
    rilMessage->resCode = OK;
    return;
}

void processCloseChannel(ComprehensionTlv *comprehensionTlv,
        CloseChannelData *closeChannelData, RilMessage *rilMessage) {
    RLOGD("processCloseChannel");
    int length = 0;
    char *pStr = NULL;
    IconId *iconId = NULL;
    DeviceIdentities *deviceIdentities = NULL;

    while (comprehensionTlv != NULL) {
        if (DEVICE_IDENTITIES == comprehensionTlv->tTlv.tag) {
            length = comprehensionTlv->tTlv.length;
            RLOGD("DEVICE_IDENTITIES: %d", length);
            if (length < 2) {
                rilMessage->resCode = REQUIRED_VALUES_MISSING;
                break;
            }
            pStr = comprehensionTlv->tTlv.value;
            if (pStr != NULL) {
                DeviceIdentities *deviceIdentities = NULL;
                deviceIdentities = (DeviceIdentities *)alloca(sizeof(DeviceIdentities));
                retrieveDeviceIdentities(pStr, deviceIdentities);
                closeChannelData->channelId = (deviceIdentities->destinationId & 0x0f);
                closeChannelData->deviceIdentities.sourceId = deviceIdentities->sourceId;
                closeChannelData->deviceIdentities.destinationId = deviceIdentities->destinationId;
                RLOGD("process device identities done");
            }
        } else if (ALPHA_ID == comprehensionTlv->tTlv.tag) {
            length = comprehensionTlv->tTlv.length;
            RLOGD("ALPHA_ID-length: %d", length);
            if (length <= 0) {
                closeChannelData->channelData.text = NULL;
            } else {
                pStr = comprehensionTlv->tTlv.value;
                if (pStr != NULL) {
                    unsigned char *temp =(unsigned char *)
                            calloc(strlen(pStr) + 1, sizeof(unsigned char));
                    convertUcsToUtf8((unsigned char *)pStr, length, temp);
                    closeChannelData->channelData.text = temp;
                    RLOGD("closeChannelData-text: %s", closeChannelData->channelData.text);
                 }
            }
        } else if (ICON_ID == comprehensionTlv->tTlv.tag) {
            length = comprehensionTlv->tTlv.length;
            RLOGD("ICON_ID-length: %d", length);
            if (length > 0) {
                pStr = comprehensionTlv->tTlv.value;
                if (pStr != NULL) {
                    iconId = (IconId *)alloca(sizeof(IconId));
                    retrieveIconId(pStr, iconId);
                    closeChannelData->channelData.iconSelfExplanatory = iconId->selfExplanatory;
                    RLOGD("closeChannelData Icon identifier done");
                 }
            }
        }
        comprehensionTlv = comprehensionTlv->next;
    }
    RLOGD("processCloseChannel EXIT");
    rilMessage->resCode = OK;
    return;
}

void processGetChannelStatus(RilMessage *rilMessage) {
    RLOGD("processGetChannelStatus");
    rilMessage->resCode = OK;
    return;
}
