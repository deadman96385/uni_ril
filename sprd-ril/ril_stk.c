/**
 * ril_stk.c --- STK-related requests process functions implementation
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */
#define LOG_TAG "RIL"

#include "sprd-ril.h"
#include <ril_stk.h>

int processStkRequests(int request, void *data, size_t datalen, RIL_Token t,
                          int channelID) {
    RIL_UNUSED_PARM(datalen);

    int err;
    ATResponse *p_response = NULL;

    switch (request) {
        case RIL_REQUEST_STK_GET_PROFILE: {
            char *line;
            p_response = NULL;
            err = at_send_command(s_ATChannels[channelID], "AT+SPUSATPROFILE?",
                                  &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                line = p_response->p_intermediates->line;
                RIL_onRequestComplete(t, RIL_E_SUCCESS, line, strlen(line) + 1);
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
        case RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM: {
            int value = ((int *)data)[0];
            if (value == 0) {
                RLOGD(" cancel STK call ");
                // s_isstkcall = 0;
                err = at_send_command(s_ATChannels[channelID],
                                      "AT+SPUSATCALLSETUP=0", &p_response);
                if (err < 0 || p_response->success == 0) {
                    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
                } else {
                    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
                }
                at_response_free(p_response);
            } else {
                RLOGD(" confirm STK call ");
                /* STK SETUP CALL feature support @{*/
                // s_isstkcall = 1;
                err = at_send_command(s_ATChannels[channelID],
                                      "AT+SPUSATCALLSETUP=1", &p_response);
                if (err < 0 || p_response->success == 0) {
                    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
                } else {
                    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
                }
                at_response_free(p_response);
                /* @} */
            }
            break;
        }
        case RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING: {
            int response = 0;
            err = at_send_command(s_ATChannels[channelID], "AT+SPUSATPROFILE?",
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

int processStkUnsolicited(RIL_SOCKET_ID socket_id, const char *s) {
    int err;
    char *line = NULL;

    if (strStartsWith(s, "+SPUSATENDSESSIONIND")) {
        RIL_onUnsolicitedResponse(RIL_UNSOL_STK_SESSION_END, NULL, 0, socket_id);
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

        RIL_onUnsolicitedResponse(RIL_UNSOL_STK_PROACTIVE_COMMAND, response,
                                  strlen(response) + 1, socket_id);
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
    } else {
        return 0;
    }

out:
    free(line);
    return 1;
}
