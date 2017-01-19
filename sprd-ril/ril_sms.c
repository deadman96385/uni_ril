/**
 * ril_sms.c --- SMS-related requests process functions implementation
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */
#define LOG_TAG "RIL"

#include "sprd_ril.h"
#include "ril_sms.h"
#include "ril_utils.h"

static void requestSendSMS(int channelID, void *data, size_t datalen,
                              RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err, ret;
    int tpLayerLength;
    const char *smsc;
    const char *pdu;
    char *cmd1, *cmd2;
    char *line = NULL;
    RIL_SMS_Response response;
    ATResponse *p_response = NULL;

    memset(&response, 0, sizeof(RIL_SMS_Response));
    smsc = ((const char **)data)[0];
    pdu = ((const char **)data)[1];

    tpLayerLength = strlen(pdu) / 2;

    /* "NULL for default SMSC" */
    if (smsc == NULL) {
        smsc = "00";
    }

    ret = asprintf(&cmd1, "AT+CMGS=%d", tpLayerLength);
    if (ret < 0) {
        RLOGE("Failed to allocate memory");
        cmd1 = NULL;
        goto error1;
    }
    ret = asprintf(&cmd2, "%s%s", smsc, pdu);
    if (ret < 0) {
        RLOGE("Failed to allocate memory");
        free(cmd1);
        cmd2 = NULL;
        goto error1;
    }

    err = at_send_command_sms(s_ATChannels[channelID], cmd1, cmd2, "+CMGS:",
                              &p_response);
    free(cmd1);
    free(cmd2);
    if (err != 0 || p_response->success == 0) {
        goto error;
    }

    /* FIXME fill in messageRef and ackPDU */
    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0) goto error1;

    err = at_tok_nextint(&line, &response.messageRef);
    if (err < 0) goto error1;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response,
            sizeof(RIL_SMS_Response));
    at_response_free(p_response);
    return;

error:
    if (p_response == NULL) {
        goto error1;
    }
    line = p_response->finalResponse;
    err = at_tok_start(&line);
    if (err < 0) goto error1;

    err = at_tok_nextint(&line, &response.errorCode);
    if (err < 0) goto error1;

    if (response.errorCode == 313) {
        RIL_onRequestComplete(t, RIL_E_SMS_SEND_FAIL_RETRY, NULL, 0);
    } else if (response.errorCode == 512  || response.errorCode == 128 ||
               response.errorCode == 254 || response.errorCode == 514 ||
               response.errorCode == 515) {
        RIL_onRequestComplete(t, RIL_E_FDN_CHECK_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
    at_response_free(p_response);
    return;

error1:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
    return;
}

static void requestSendIMSSMS(int channelID, void *data, size_t datalen,
                                  RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err, ret;
    int tpLayerLength;
    char *pdu;
    char *line;
    char *cmd1, *cmd2;
    const char *smsc;
    RIL_SMS_Response response;
    ATResponse *p_response = NULL;
    RIL_IMS_SMS_Message *sms = NULL;

    sms = (RIL_IMS_SMS_Message *)data;
    if (sms->tech == RADIO_TECH_3GPP) {
        memset(&response, 0, sizeof(RIL_SMS_Response));

        smsc = ((char **)(sms->message.gsmMessage))[0];
        pdu = ((char **)(sms->message.gsmMessage))[1];
        if (sms->retry > 0) {
            /*
             * per TS 23.040 Section 9.2.3.6:  If TP-MTI SMS-SUBMIT (0x01) type
             * TP-RD (bit 2) is 1 for retry
             * and TP-MR is set to previously failed sms TP-MR
             */
            if (((0x01 & pdu[0]) == 0x01)) {
                pdu[0] |= 0x04;  // TP-RD
                pdu[1] = sms->messageRef;  // TP-MR
            }
        }

        tpLayerLength = strlen(pdu) / 2;
        /* "NULL for default SMSC" */
        if (smsc == NULL) {
            smsc = "00";
        }

        ret = asprintf(&cmd1, "AT+CMGS=%d", tpLayerLength);
        if (ret < 0) {
            RLOGE("Failed to allocate memory");
            cmd1 = NULL;
            goto error1;
        }
        ret = asprintf(&cmd2, "%s%s", smsc, pdu);
        if (ret < 0) {
            RLOGE("Failed to allocate memory");
            free(cmd1);
            cmd2 = NULL;
            goto error1;
        }

        err = at_send_command_sms(s_ATChannels[channelID], cmd1, cmd2, "+CMGS:",
                                  &p_response);
        free(cmd1);
        free(cmd2);
        if (err != 0 || p_response->success == 0)
            goto error;

        /* FIXME fill in messageRef and ackPDU */
        line = p_response->p_intermediates->line;
        err = at_tok_start(&line);
        if (err < 0) goto error1;

        err = at_tok_nextint(&line, &response.messageRef);
        if (err < 0) goto error1;

        RIL_onRequestComplete(t, RIL_E_SUCCESS, &response,
                              sizeof(RIL_SMS_Response));
        at_response_free(p_response);
        return;

error:
        if (p_response == NULL) {
            goto error1;
        }
        line = p_response->finalResponse;
        err = at_tok_start(&line);
        if (err < 0) goto error1;

        err = at_tok_nextint(&line, &response.errorCode);
        if (err < 0) goto error1;
        if ((response.errorCode != 313) && (response.errorCode != 512)) {
            goto error1;
        }
        if (response.errorCode == 313) {
            RIL_onRequestComplete(t, RIL_E_SMS_SEND_FAIL_RETRY, NULL, 0);
        } else if (response.errorCode == 512 || response.errorCode == 128 ||
                    response.errorCode == 254) {
            RIL_onRequestComplete(t, RIL_E_FDN_CHECK_FAILURE, NULL, 0);
        }
        at_response_free(p_response);
        return;

error1:
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        at_response_free(p_response);
        return;
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
}

static void requestSMSAcknowledge(int channelID, void *data,
                                      size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int ackSuccess;
    int err;

    ackSuccess = ((int *)data)[0];

    if (ackSuccess == 1) {
        err = at_send_command(s_ATChannels[channelID], "AT+CNMA=1", NULL);
    } else if (ackSuccess == 0) {
        err = at_send_command(s_ATChannels[channelID], "AT+CNMA=2", NULL);
    } else {
        RLOGE("Unsupported arg to RIL_REQUEST_SMS_ACKNOWLEDGE\n");
        goto error;
    }
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    return;
}

static void requestWriteSmsToSim(int channelID, void *data, size_t datalen,
                                     RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int length;
    int err, ret;
    int errorNum;
    int index = 0;
    char *cmd, *cmd1;
    const char *smsc;
    char *line = NULL;
    ATResponse *p_response = NULL;
    RIL_SMS_WriteArgs *p_args;

    p_args = (RIL_SMS_WriteArgs *)data;

    length = strlen(p_args->pdu) / 2;

    smsc = (const char *)(p_args->smsc);
    /* "NULL for default SMSC" */
    if (smsc == NULL) {
        smsc = "00";
    }

    ret = asprintf(&cmd, "AT+CMGW=%d,%d", length, p_args->status);
    if (ret < 0) {
        RLOGE("Failed to allocate memory");
        cmd = NULL;
        goto error1;
    }
    ret = asprintf(&cmd1, "%s%s", smsc, p_args->pdu);
    if (ret < 0) {
        RLOGE("Failed to allocate memory");
        free(cmd);
        cmd1 = NULL;
        goto error1;
    }

    err = at_send_command_sms(s_ATChannels[channelID], cmd, cmd1, "+CMGW:",
                              &p_response);
    free(cmd);
    free(cmd1);

    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &index);
    if (err < 0) goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &index, sizeof(index));
    at_response_free(p_response);
    return;

error:
    if (p_response != NULL &&
            strStartsWith(p_response->finalResponse, "+CMS ERROR:")) {
        line = p_response->finalResponse;
        err = at_tok_start(&line);
        if (err < 0) goto error1;

        err = at_tok_nextint(&line, &errorNum);
        if (err < 0) goto error1;

        if (errorNum != 322) {
            goto error1;
        }
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        at_response_free(p_response);
        return;
    }

error1:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static int setSmsBroadcastConfigValue(int value, char *out_value,
        size_t out_len ) {
    if (value == 0xffff) {
        return 0;
    } else {
        snprintf(out_value, out_len, "%d", value);
    }
    return 1;
}

static void setSmsBroadcastConfigData(int data, int idx, int isFirst,
        char *toStr, int *strLength, char *retStr) {
    RIL_UNUSED_PARM(toStr);

    int len = 0;
    char str[10] = {0};
    char comma = 0x2c;  // ,
    char quotes = 0x22;  // "
    char line = 0x2d;  // -

    memset(str, 0, 10);
    if (setSmsBroadcastConfigValue(data, str, 10) > 0) {
        if (idx == 0 && 1 == isFirst) {
            retStr[len] = quotes;
            len += 1;
        } else if (2 == isFirst) {
            retStr[0] = line;
            len += 1;
        } else {
            retStr[0] = comma;
            len += 1;
        }
        memcpy(retStr + len, str, strlen(str));
        len += strlen(str);
    }
    *strLength = len;
    RLOGI("setSmsBroadcastConfigData ret_char %s, len %d", retStr, *strLength);
}

// Add for command SPPWS
static void skipFirstQuotes(char **p_cur) {
    if (*p_cur == NULL) return;

    if (**p_cur == '"') {
        (*p_cur)++;
    }
}

static void requestSetSmsBroadcastConfig(int channelID, void *data,
                                              size_t datalen, RIL_Token t) {
    int err;
    int ret = -1;
    int enable = 0;
    int i = 0;
    int count = datalen / sizeof(RIL_GSM_BroadcastSmsConfigInfo *);
    int channelLen = 0;
    int langLen = 0;
    int len = 0;
    int serviceId[ARRAY_SIZE] = {0};
    int index = 0;
    char pre_colon = 0x22;
    char *cmd;
    char *channel;
    char *lang;
    char get_char[10] = {0};
    char comma = 0x2c;
    char tmp[20] = {0};
    char quotes = 0x22;
    ATResponse *p_response = NULL;

    RIL_GSM_BroadcastSmsConfigInfo **gsmBciPtrs =
            (RIL_GSM_BroadcastSmsConfigInfo **)data;
    RIL_GSM_BroadcastSmsConfigInfo gsmBci;

    RLOGD("requestSetSmsBroadcastConfig %zu, count %d", datalen, count);
    int size = datalen * 16 * sizeof(char);
    channel = (char *)alloca(size);
    lang = (char *)alloca(size);

    memset(channel, 0, datalen * 16);
    memset(lang, 0, datalen * 16);

    for (i = 0; i < count; i++) {
        gsmBci = *(RIL_GSM_BroadcastSmsConfigInfo *)(gsmBciPtrs[i]);
        if (i == 0) {
            enable = gsmBci.selected ? 0 : 1;
        }
        /**
         * AT+CSCB = <mode>, <mids>, <dcss>
         * When mids are all different possible combinations of CBM message
         * identifiers, we send the range to modem. e.g. AT+CSCB=0,"4373-4383"
         */
        memset(tmp, 0, 20);
        setSmsBroadcastConfigData(gsmBci.fromServiceId, i, 1, channel, &len,
                tmp);
        memcpy(channel + channelLen, tmp, strlen(tmp));
        channelLen += len;
        RLOGI("SetSmsBroadcastConfig channel %s, %d ", channel, channelLen);
        serviceId[index++] = gsmBci.fromServiceId;
        if (gsmBci.fromServiceId != gsmBci.toServiceId) {
            memset(tmp, 0, 20);
            setSmsBroadcastConfigData(gsmBci.toServiceId, i, 2, channel, &len,
                                      tmp);
            memcpy(channel + channelLen, tmp, strlen(tmp));
            channelLen += len;
            RLOGI("SetSmsBroadcastConfig channel %s, %d", channel, channelLen);
        }
        serviceId[index++] = gsmBci.toServiceId;

        memset(tmp, 0, 20);
        setSmsBroadcastConfigData(gsmBci.fromCodeScheme, i, 1, lang, &len, tmp);
        memcpy(lang + langLen, tmp, strlen(tmp));
        langLen += len;
        RLOGI("SetSmsBroadcastConfig lang %s, %d", lang, langLen);

        memset(tmp, 0, 20);
        setSmsBroadcastConfigData(gsmBci.toCodeScheme, i, 2, lang, &len, tmp);
        memcpy(lang + langLen, tmp, strlen(tmp));
        langLen += len;
        RLOGI("SetSmsBroadcastConfig lang %s, %d", lang, langLen);
    }
    if (langLen == 0) {
        snprintf(lang, size, "%c", quotes);
    }
    if (channelLen == 1) {
        snprintf(channel + channelLen, size - channelLen, "%c", quotes);
    }
    if (channelLen == 0) {
        snprintf(channel, size, "%c", quotes);
    }

    ret = asprintf(&cmd, "AT+CSCB=%d%c%s%c%c%s%c", enable, comma, channel,
                    quotes, comma, lang, quotes);
    if (ret < 0) {
        RLOGE("Failed to allocate memory");
        cmd = NULL;
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        return;
    }

    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    free(cmd);
    if (err < 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    }
    AT_RESPONSE_FREE(p_response);

    /* Add for at command SPPWS @{ */
    RLOGD("requestSetSmsBroadcastConfig enable %d", enable);
    if (enable == 0) {
        int cmas = 0;
        int etws = 0;
        int etwsTest = 0;
        int current = 0;

        RLOGD("index %d", index);
        for (current = 0; current < index; current = current + 2) {
            // cmas message under LTE, channel is from 4370 to 6400
            if (serviceId[current] >= 4370 && serviceId[current] <= 6400 &&
                    serviceId[current + 1] >= 4370 &&
                    serviceId[current + 1] <= 6400) {
                cmas++;
            } else if ((serviceId[current] >= 4352 && serviceId[current] <= 4359
                    && serviceId[current + 1] >= 4352 &&
                    serviceId[current + 1] <= 4359) &&
                    (serviceId[current] != 4355 &&
                            serviceId[current + 1] != 4355)) {
                 // etws primary and second message under LTE, channel is from
                 // 4352 to 4359 except 4355.
                 etws++;
            } else if (serviceId[current] == 4355 &&
                    serviceId[current + 1] ==4355) {
                // etws test message under LTE, channel is 4355
                etwsTest++;
            }
        }

        if (0 != etwsTest) {  // enable etws test message
            at_send_command(s_ATChannels[channelID], "AT+SPPWS=2,2,1,2", NULL);
        }
        if (0 != cmas && 0 != etws) {  // enable etws and cmas message
            at_send_command(s_ATChannels[channelID], "AT+SPPWS=1,1,2,1", NULL);
        } else {
            if (0 != cmas) {  // enable cmas message
                at_send_command(s_ATChannels[channelID], "AT+SPPWS=2,2,2,1",
                        NULL);
            } else if (0 != etws) {  // enable etws message
               at_send_command(s_ATChannels[channelID], "AT+SPPWS=1,1,2,2",
                       NULL);
            }
        }
    }
    /* }@ Add for at command SPPWS end */
}

static void requestGetSmsBroadcastConfig(int channelID, void *data,
                                              size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    ATResponse *p_response = NULL;
    int err;
    char *response;

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+CSCB=?",
                                     "+CSCB:", &p_response);
    if (err < 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        response = p_response->p_intermediates->line;
        RIL_onRequestComplete(t, RIL_E_SUCCESS, response, strlen(response) + 1);
    }
    at_response_free(p_response);
}

static void requestSmsBroadcastActivation(int channelID, void *data,
                                               size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err;
    int *active = (int *)data;
    char cmd[AT_COMMAND_LEN] = {0};
    ATResponse *p_response = NULL;

    snprintf(cmd, sizeof(cmd), "AT+CSCB=%d", active[0]);
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (err < 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    }
    at_response_free(p_response);
}

static void requestGetSmscAddress(int channelID, void *data,
                                      size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int err;
    char *sc_line;
    ATResponse *p_response = NULL;

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+CSCA?",
                                     "+CSCA:", &p_response);
    if (err >= 0 && p_response->success) {
        char *line = p_response->p_intermediates->line;
        err = at_tok_start(&line);
        if (err >= 0) {
            err = at_tok_nextstr(&line, &sc_line);
            char *decidata = (char *)calloc((strlen(sc_line) / 2 + 1),
                    sizeof(char));
            convertHexToBin(sc_line, strlen(sc_line), decidata);
            RIL_onRequestComplete(t, RIL_E_SUCCESS, decidata,
                                  strlen(decidata) + 1);
            free(decidata);
        } else {
            RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        }
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
    at_response_free(p_response);
}

static void requestGetSIMCapacity(int channelID, void *data,
                                  size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int i;
    int err;
    int response[2] = {-1, -1};
    char *line, *skip;
    char *responseStr[2] = {NULL, NULL};
    char res[2][20];
    ATResponse *p_response = NULL;

    for (i = 0; i < 2; i++) {
        responseStr[i] = res[i];
    }

    err = at_send_command_singleline(s_ATChannels[channelID],
                                     "AT+CPMS?", "+CPMS:", &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextstr(&line, &skip);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &response[0]);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &response[1]);
    if (err < 0) goto error;

    if (response[0] == -1 || response[1] == -1) {
        goto error;
    }

    snprintf(res[0], sizeof(res[0]), "%d", response[0]);
    snprintf(res[1], sizeof(res[1]), "%d", response[1]);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, responseStr, 2 * sizeof(char *));
    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static void requestStoreSmsToSim(int channelID, void *data,
        size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err, commas;
    char *line = NULL;
    char cmd[128];
    char *memoryRD = NULL;  // memory for read and delete
    char *memoryWS = NULL;  // memory for write and send
    ATResponse *p_response = NULL;
    int value = ((int *)data)[0];

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+CPMS?",
                "+CPMS:", &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextstr(&line, &memoryRD);
    if (err < 0) goto error;

    skipNextComma(&line);
    skipNextComma(&line);

    err = at_tok_nextstr(&line, &memoryWS);
    if (err < 0) goto error;

    if (value == 1) {
        at_send_command(s_ATChannels[channelID], "AT+CNMI=3,1,2,1,1", NULL);
        snprintf(cmd, sizeof(cmd), "AT+CPMS=\"%s\",\"%s\",\"SM\"", memoryRD,
                memoryWS);
    } else {
        at_send_command(s_ATChannels[channelID], "AT+CNMI=3,2,2,1,1", NULL);
        snprintf(cmd, sizeof(cmd), "AT+CPMS=\"%s\",\"%s\",\"ME\"", memoryRD,
                memoryWS);
    }

    AT_RESPONSE_FREE(p_response);
    err = at_send_command_singleline(s_ATChannels[channelID], cmd, "+CPMS:",
                                     &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static void requestQuerySmsStorageMode(int channelID, void *data,
        size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int err = -1;
    int commas = 0;
    char *response = NULL;
    char *line = NULL;
    ATResponse *p_response = NULL;

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+CPMS?",
            "+CPMS:", &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0) goto error;

    // +CPMS:"ME",0,20,"ME",0,20, "SM",0,20
    // +CPMS:"ME",0,20,"ME",0,20, "ME",0,20
    for (commas = 0; commas < 6; commas++) {
        skipNextComma(&line);
    }
    err = at_tok_nextstr(&line, &response);
    if (err < 0) goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, response, strlen(response) + 1);
    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

int processSmsRequests(int request, void *data, size_t datalen, RIL_Token t,
                          int channelID) {
    int err;
    ATResponse *p_response = NULL;

    switch (request) {
        case RIL_REQUEST_SEND_SMS:
        case RIL_REQUEST_SEND_SMS_EXPECT_MORE:
            requestSendSMS(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_IMS_SEND_SMS:
            requestSendIMSSMS(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_SMS_ACKNOWLEDGE:
            requestSMSAcknowledge(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_WRITE_SMS_TO_SIM:
            requestWriteSmsToSim(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_DELETE_SMS_ON_SIM: {
            char cmd[AT_COMMAND_LEN] = {0};
            snprintf(cmd, sizeof(cmd), "AT+CMGD=%d", ((int *)data)[0]);
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_REQUEST_GSM_SMS_BROADCAST_ACTIVATION:
            requestSmsBroadcastActivation(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG:
            requestSetSmsBroadcastConfig(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG:
            requestGetSmsBroadcastConfig(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_GET_SMSC_ADDRESS:
            requestGetSmscAddress(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_SET_SMSC_ADDRESS: {
            char *cmd;
            int ret;

            int hexLen = strlen(data) * 2 + 1;
            unsigned char *hexData =
                    (unsigned char *)calloc(hexLen, sizeof(unsigned char));
            convertBinToHex(data, strlen(data), hexData);

            ret = asprintf(&cmd, "AT+CSCA=\"%s\"", hexData);
            if (ret < 0) {
                RLOGE("Failed to allocate memory");
                free(hexData);
                cmd = NULL;
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
                break;
            }
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
            free(cmd);
            free(hexData);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_REQUEST_REPORT_SMS_MEMORY_STATUS: {
            char cmd[AT_COMMAND_LEN] = {0};
            snprintf(cmd, sizeof(cmd), "AT+SPSMSFULL=%d", !((int *)data)[0]);
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        /* IMS request @{ */
        case RIL_REQUEST_SET_IMS_SMSC: {
            char *cmd;
            int ret;
            p_response = NULL;
            RLOGD("[sms]RIL_REQUEST_SET_IMS_SMSC (%s)", (char *)(data));
            ret = asprintf(&cmd, "AT+PSISMSC=\"%s\"", (char *)(data));
            if (ret < 0) {
                RLOGE("Failed to allocate memory!");
                cmd = NULL;
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
                break;
            }
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
            free(cmd);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        /* }@ */
        case RIL_EXT_REQUEST_GET_SIM_CAPACITY: {
            requestGetSIMCapacity(channelID, data, datalen, t);
            break;
        }
        case RIL_EXT_REQUEST_STORE_SMS_TO_SIM:
            requestStoreSmsToSim(channelID, data, datalen, t);
            break;
        case RIL_EXT_REQUEST_QUERY_SMS_STORAGE_MODE:
            requestQuerySmsStorageMode(channelID, data, datalen, t);
            break;
        default:
            return 0;
    }

    return 1;
}

int processSmsUnsolicited(RIL_SOCKET_ID socket_id, const char *s,
                             const char *sms_pdu) {
    char *line = NULL;
    int err;

    if (strStartsWith(s, "+CMT:")) {
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NEW_SMS, sms_pdu,
                                  strlen(sms_pdu) + 1, socket_id);
    } else if (strStartsWith(s, "+CDS:")) {
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT,
                                  sms_pdu, strlen(sms_pdu) + 1, socket_id);
    } else if (strStartsWith(s, "+CMGR:")) {
        if (sms_pdu != NULL) {
            RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NEW_SMS, sms_pdu,
                                      strlen(sms_pdu) + 1, socket_id);
        } else {
            RLOGD("[cmgr] sms_pdu is NULL");
        }
    } else if (strStartsWith(s, "+CMTI:")) {
        /* can't issue AT commands here -- call on main thread */
        int location;
        char *response = NULL;
        char *tmp;
        int *p_index;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextstr(&tmp, &response);
        if (err < 0) {
            RLOGD("sms request fail");
            goto out;
        }
        if (strcmp(response, "SM")) {
            RLOGD("sms request arrive but it is not a new sms");
            goto out;
        }

        /* Read the memory location of the sms */
        err = at_tok_nextint(&tmp, &location);
        if (err < 0) {
            RLOGD("error parse location");
            goto out;
        }
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NEW_SMS_ON_SIM, &location,
                                  sizeof(location), socket_id);
    } else if (strStartsWith(s, "+CBM:")) {
        char *pdu_bin = NULL;
        RLOGD("CBM sss %s, len  %d", s, (int)strlen(s));
        RLOGD("CBM  %s, len  %d", sms_pdu, (int)strlen(sms_pdu));
        pdu_bin = (char *)alloca(strlen(sms_pdu) / 2);
        memset(pdu_bin, 0, strlen(sms_pdu) / 2);
        if (!convertHexToBin(sms_pdu, strlen(sms_pdu), pdu_bin)) {
            RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NEW_BROADCAST_SMS,
                                      pdu_bin, strlen(sms_pdu) / 2, socket_id);
        } else {
            RLOGE("Convert hex to bin failed for SMSCB");
        }
    } else if (strStartsWith(s, "+SPWRN:")) {
        int skip;
        int segmentId;
        int totalSegments;
        static int count = 0;
        static int dataLen = 0;

        static char **pdus;
        char *msg = NULL;
        char *tmp = NULL;
        char *data = NULL;
        char *binData = NULL;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextint(&tmp, &segmentId);
        if (err < 0) goto out;

        err = at_tok_nextint(&tmp, &totalSegments);
        if (err < 0) goto out;

        err = at_tok_nextint(&tmp, &skip);
        if (err < 0) goto out;

        err = at_tok_nextint(&tmp, &skip);
        if (err < 0) goto out;

        err = at_tok_nextint(&tmp, &skip);
        if (err < 0) goto out;

        err = at_tok_nextint(&tmp, &skip);
        if (err < 0) goto out;

        err = at_tok_nextstr(&tmp, &data);
        if (err < 0) goto out;

        /* Max length of SPWRN message is
         * 9600 byte and each time ATC can only send 1k. When 9600 is divided
         * by 1024, the quotient is 9 with a remainder of 1.
         */

        if (totalSegments < 10 && count == 0) {
            pdus = (char **)calloc(totalSegments, sizeof(char *));
            if (pdus == NULL) goto out;
        }

        if (segmentId <= totalSegments) {
            pdus[segmentId -1] =
                    (char *)calloc((strlen(data + 1)), sizeof(char));
            snprintf(pdus[segmentId -1], strlen(data) + 1, "%s", data);

            count++;
            dataLen += strlen(data);
        }

        // To make sure no missing pages, then concat all pages.
        if (count == totalSegments) {
            msg = (char *)calloc((dataLen + sizeof("01")), sizeof(char));
            int index = 0;
            strncat(msg, "01", sizeof("01"));  // concat message_type and msg
            for (; index < count; index++) {
                if (pdus[index] != NULL) {
                    strncat(msg, pdus[index], strlen(pdus[index]));
                }
            }
            RLOGD("concat pdu: %s", msg);
            /* +SPWRN:1,N,<xx>,<xx>,<xx>,<xx>,<data1>
             * +SPWRN:2,N,<xx>,<xx>,<xx>,<xx>,<data2>
             * ...
             * +SPWRN:N,N,<xx>,<xx>,<xx>,<xx>,<dataN>
             * Response message_type + data1 + data2 + ... + dataN to framework
             */
            binData = (char *)calloc(strlen(msg) / 2, sizeof(char));
            if (!convertHexToBin(msg, strlen(msg), binData)) {
                RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NEW_BROADCAST_SMS,
                        binData, strlen(msg) / 2, socket_id);
            } else {
                RLOGD("Convert hex to bin failed for SPWRN");
            }
            free(msg);
            free(binData);
            for (index = 0; index < count; index++) {
                 free(pdus[index]);
            }
            free(pdus);
            pdus = NULL;
            dataLen = 0;
            count = 0;
        }
    } else if (strStartsWith(s, "^SMOF:")) {
        int value;
        char *tmp;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextint(&tmp, &value);
        if (err < 0) goto out;

        if (value == 2) {
            RIL_onUnsolicitedResponse(RIL_UNSOL_SIM_SMS_STORAGE_FULL, NULL, 0,
                                      socket_id);
        }
    } else {
        return 0;
    }

out:
    free(line);
    return 1;
}

