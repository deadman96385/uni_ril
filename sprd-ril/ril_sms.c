/**
 * ril_sms.c --- SMS-related requests process functions implementation
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */
#define LOG_TAG "RIL"

#include "sprd-ril.h"
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

    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(RIL_SMS_Response));
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
        // TODO: errorCode meaningless
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

    if (err != 0 || p_response->success == 0) {
        goto error;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);
    return;

error:
    if (strStartsWith(p_response->finalResponse, "+CMS ERROR:")) {
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

static int setSmsBroadcastConfigValue(int value, char *out_value) {
    RLOGI("Reference-ril. setSmsBroadcastConfigValue value %d", value);

    if (value == 0xffff) {
        return 0;
    } else {
        RLOGI("Reference-ril. setSmsBroadcastConfigValue value %d", value);
        sprintf(out_value, "%d", value);
    }
    RLOGI("Reference-ril. setSmsBroadcastConfigValue out_value %s",
            out_value);

    return 1;
}

static void setSmsBroadcastConfigData(int data, int idx, int isFirst,
        char *toStr, int *strLength, char *retStr) {
    RIL_UNUSED_PARM(toStr);

    int len = 0;
    char str[10] = {0};
    char comma = 0x2c;  //,
    char quotes = 0x22;  //"
    char line = 0x2d;  //-

    memset(str, 0, 10);
    if (setSmsBroadcastConfigValue(data, str) > 0) {
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

static void requestSetSmsBroadcastConfig(int channelID, void *data,
                                              size_t datalen, RIL_Token t) {
    int err;
    int ret = -1;
    int enable = 0;
    int i = 0, j = 0;
    int count = datalen / sizeof(RIL_GSM_BroadcastSmsConfigInfo *);
    int channelLen = 0;
    int langLen = 0;
    int len = 0;
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

    RLOGD("requestSetSmsBroadcastConfig %d ,count %d", datalen, count);

    channel = (char *)alloca(datalen * 16 * sizeof(char));
    lang = (char *)alloca(datalen * 16 * sizeof(char));
    memset(channel, 0, datalen * 16);
    memset(lang, 0, datalen * 16);

    for (i = 0; i < count; i++) {
        gsmBci = *(RIL_GSM_BroadcastSmsConfigInfo *)(gsmBciPtrs[i]);
        if (i == 0) {
            enable = gsmBci.selected;
        }
        memset(tmp, 0, 20);
        setSmsBroadcastConfigData(gsmBci.fromServiceId, i, 1, channel, &len, tmp);
        memcpy(channel + channelLen, tmp, strlen(tmp));
        channelLen += len;
        RLOGI("SetSmsBroadcastConfig channel %s ,%d ", channel, channelLen);

        memset(tmp, 0, 20);
        setSmsBroadcastConfigData(gsmBci.toServiceId, i, 0, channel, &len, tmp);
        memcpy(channel + channelLen, tmp, strlen(tmp));
        channelLen += len;
        RLOGI("SetSmsBroadcastConfig channel %s ,%d", channel, channelLen);

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
        sprintf(lang, "%c", quotes);
    }
    if (channelLen == 1) {
        sprintf(channel + channelLen, "%c", quotes);
    }
    if (channelLen == 0) {
        sprintf(channel, "%c", quotes);
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
    RLOGI("SetSmsBroadcastConfig err %d ,success %d", err, p_response->success);
    if (err < 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    }
    at_response_free(p_response);
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
            RIL_onRequestComplete(t, RIL_E_SUCCESS, sc_line,
                                  strlen(sc_line) + 1);
        } else {
            RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        }
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
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

            ret = asprintf(&cmd, "AT+CSCA=\"%s\"", (char *)(data));
            if (ret < 0) {
                RLOGE("Failed to allocate memory");
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
        case RIL_REQUEST_REPORT_SMS_MEMORY_STATUS: {
            char cmd[AT_COMMAND_LEN] = { 0 };
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
        RLOGD("[unsl]cmti: location = %d", location);

        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NEW_SMS_ON_SIM, &location,
                                  sizeof(location), socket_id);
    } else if (strStartsWith(s, "+CBM:")) {
        char *pdu_bin = NULL;
        RLOGD("CBM sss %s ,len  %d", s, strlen(s));
        RLOGD("CBM  %s ,len  %d", sms_pdu, strlen(sms_pdu));
        pdu_bin = (char *)alloca(strlen(sms_pdu) / 2);
        memset(pdu_bin, 0, strlen(sms_pdu) / 2);
        if (!convertHexToBin(sms_pdu, strlen(sms_pdu), pdu_bin)) {
            RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NEW_BROADCAST_SMS,
                                      pdu_bin, strlen(sms_pdu) / 2, socket_id);
        } else {
            RLOGE("Convert hex to bin failed for SMSCB");
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
            RLOGD("[sms]RIL_UNSOL_SIM_SMS_STORAGE_FULL");
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

