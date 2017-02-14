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

bool s_stkServiceRunning[SIM_COUNT];
static char *s_stkUnsolResponse[SIM_COUNT];

static void requestDefaultNetworkName(int channelID, RIL_Token t) {
    ATResponse *p_response = NULL;
    ATLine *p_cur;
    int err;
    char *apn = NULL;

    err = at_send_command_multiline(s_ATChannels[channelID],
                "AT+CGDCONT?", "+CGDCONT:", &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    for (p_cur = p_response->p_intermediates; p_cur != NULL;
            p_cur = p_cur->p_next) {
        char *line = p_cur->line;
        int ncid;

        err = at_tok_start(&line);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &ncid);
        if (err < 0) goto error;

        if (ncid == 1) {
            skipNextComma(&line);
            err = at_tok_nextstr(&line, &apn);
            if (err < 0 || strlen(apn) == 0) goto error;
            break;
        }
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, apn, strlen(apn) + 1);
    AT_RESPONSE_FREE(p_response);
    return;
error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    AT_RESPONSE_FREE(p_response);
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

int processStkUnsolicited(RIL_SOCKET_ID socket_id, const char *s) {
    int err;
    char *line = NULL;

    if (strStartsWith(s, "+SPUSATENDSESSIONIND")) {
        RIL_onUnsolicitedResponse(RIL_UNSOL_STK_SESSION_END, NULL, 0,
                socket_id);
    } else if (strStartsWith(s, "+SPUSATPROCMDIND:")) {
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
        err = at_tok_nextstr(&tmp, &response->aid);
        if (err < 0) {
            RLOGD("%s fail", s);
            goto out;
        }
        response->result = result;
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
