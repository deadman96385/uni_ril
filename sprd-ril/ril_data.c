/**
 * ril_data.c --- Data-related requests process functions implementation
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#define LOG_TAG "RIL"

#include "sprd-ril.h"
#include "ril_data.h"
#include "ril_network.h"
#include "ril_call.h"

int s_dataAllowed[SIM_COUNT];
/* for LTE, attach will occupy a cid for default PDP in CP */
bool s_LTEDetached[SIM_COUNT] = {0};

static int s_activePDN;
static int s_addedIPCid = -1;  /* for VoLTE additional business */
static RIL_InitialAttachApn *s_initialAttachAPNs[SIM_COUNT] = {0};
/* Last PDP fail cause, obtained by *ECAV */
static int s_lastPDPFailCause[SIM_COUNT] = {
        PDP_FAIL_ERROR_UNSPECIFIED
#if (SIM_COUNT >= 2)
        ,PDP_FAIL_ERROR_UNSPECIFIED
#if (SIM_COUNT >= 3)
        ,PDP_FAIL_ERROR_UNSPECIFIED
#if (SIM_COUNT >= 4)
        ,PDP_FAIL_ERROR_UNSPECIFIED
#endif
#endif
#endif
};

struct PDPInfo s_PDP[MAX_PDP] = {
    { -1, -1, false, PDP_IDLE, PTHREAD_MUTEX_INITIALIZER},
    { -1, -1, false, PDP_IDLE, PTHREAD_MUTEX_INITIALIZER},
    { -1, -1, false, PDP_IDLE, PTHREAD_MUTEX_INITIALIZER},
    { -1, -1, false, PDP_IDLE, PTHREAD_MUTEX_INITIALIZER},
    { -1, -1, false, PDP_IDLE, PTHREAD_MUTEX_INITIALIZER},
    { -1, -1, false, PDP_IDLE, PTHREAD_MUTEX_INITIALIZER}
};

static PDNInfo s_PDN[MAX_PDP_CP] = {
    { -1, "", ""},
    { -1, "", ""},
    { -1, "", ""},
    { -1, "", ""},
    { -1, "", ""},
    { -1, "", ""},
    { -1, "", ""},
    { -1, "", ""},
    { -1, "", ""},
    { -1, "", ""},
    { -1, "", ""}
};

static int getPDP(RIL_SOCKET_ID socket_id) {
    int ret = -1;
    int i;
    char prop[PROPERTY_VALUE_MAX] = {0};

    /* TODO: debug  s_workMode? is open channel */
    for (i = 0; i < MAX_PDP; i++) {
        if (s_workMode[socket_id] != 10 && s_activePDN > 0 &&
            s_PDN[i].nCid == (i + 1)) {
            continue;
        }
        pthread_mutex_lock(&s_PDP[i].mutex);
        if (s_PDP[i].state == PDP_IDLE && s_PDP[i].cid == -1) {
            s_PDP[i].state = PDP_BUSY;
            ret = i;
            pthread_mutex_unlock(&s_PDP[i].mutex);
            RLOGD("get s_PDP[%d]", ret);
            RLOGD("PDP[0].state = %d, PDP[1].state = %d, PDP[2].state = %d",
                    s_PDP[0].state, s_PDP[1].state, s_PDP[2].state);
            RLOGD("PDP[3].state = %d, PDP[4].state = %d, PDP[5].state = %d",
                    s_PDP[3].state, s_PDP[4].state, s_PDP[5].state);
            return ret;
        }
        pthread_mutex_unlock(&s_PDP[i].mutex);
    }
    return ret;
}

void putPDP(int cid) {
    if (cid < 0 || cid >= MAX_PDP) {
        return;
    }

    pthread_mutex_lock(&s_PDP[cid].mutex);
    if (s_PDP[cid].state != PDP_BUSY) {
        goto done;
    }
    if (s_addedIPCid == (cid + 1)) {
        s_addedIPCid = -1;
    }
    s_PDP[cid].state = PDP_IDLE;

done:
    if ((s_PDP[cid].secondary_cid > 0) &&
        (s_PDP[cid].secondary_cid <= MAX_PDP)) {
        s_PDP[s_PDP[cid].secondary_cid - 1].secondary_cid = -1;
    }
    s_PDP[cid].secondary_cid = -1;
    s_PDP[cid].cid = -1;
    s_PDP[cid].isPrimary = false;
    RLOGD("put s_PDP[%d]", cid);
    RLOGD("s_PDP[0].state = %d,s_PDP[1].state = %d,s_PDP[2].state = %d",
           s_PDP[0].state, s_PDP[1].state, s_PDP[2].state);
    RLOGD("s_PDP[3].state = %d,s_PDP[4].state = %d,s_PDP[5].state = %d",
           s_PDP[3].state, s_PDP[4].state, s_PDP[5].state);
    pthread_mutex_unlock(&s_PDP[cid].mutex);
}

static int getPDPByIndex(int index) {
    if (index >= 0 && index < MAX_PDP) {  // cid: 1 ~ MAX_PDP
        pthread_mutex_lock(&s_PDP[index].mutex);
        if (s_PDP[index].state == PDP_IDLE) {
            s_PDP[index].state = PDP_BUSY;
            pthread_mutex_unlock(&s_PDP[index].mutex);
            RLOGD("getPDPByIndex[%d]", index);
            RLOGD("PDP[0].state = %d, PDP[1].state = %d, PDP[2].state = %d",
                   s_PDP[0].state, s_PDP[1].state, s_PDP[2].state);
            RLOGD("PDP[3].state = %d, PDP[4].state = %d, PDP[5].state = %d",
                   s_PDP[3].state, s_PDP[4].state, s_PDP[5].state);
            return index;
        }
        pthread_mutex_unlock(&s_PDP[index].mutex);
    }
    return -1;
}

int updatePDPCid(int cid, int state) {
    int index = cid - 1;
    if (cid <= 0 || cid > MAX_PDP) {
        return 0;
    }
    pthread_mutex_lock(&s_PDP[index].mutex);
    if (state != 0) {
        s_PDP[index].cid = cid;
    } else {
        s_PDP[index].cid = -1;
    }
    pthread_mutex_unlock(&s_PDP[index].mutex);
    return 1;
}

int getPDPCid(int index) {
    if (index >= MAX_PDP || index < 0) {
        return -1;
    } else {
        return s_PDP[index].cid;
    }
}

enum PDPState getPDPState(int index) {
    if (index >= MAX_PDP || index < 0) {
        return PDP_IDLE;
    } else {
        return s_PDP[index].state;
    }
}

int getFallbackCid(int index) {
    if (index >= MAX_PDP || index < 0) {
        return -1;
    } else {
        return s_PDP[index].secondary_cid;
    }
}

bool isPrimaryCid(int index) {
    if (index >= MAX_PDP || index < 0) {
        return false;
    } else {
        return s_PDP[index].isPrimary;
    }
}

int setPDPMapping(int primary, int secondary) {
    RLOGD("setPDPMapping primary %d, secondary %d", primary, secondary);
    if (primary < 0 || primary >= MAX_PDP || secondary < 0 ||
        secondary >= MAX_PDP) {
        return 0;
    }
    pthread_mutex_lock(&s_PDP[primary].mutex);
    s_PDP[primary].cid = primary + 1;
    s_PDP[primary].secondary_cid = secondary + 1;
    s_PDP[primary].isPrimary = true;
    pthread_mutex_unlock(&s_PDP[primary].mutex);

    pthread_mutex_lock(&s_PDP[secondary].mutex);
    s_PDP[secondary].cid = secondary + 1;
    s_PDP[secondary].secondary_cid = primary + 1;
    s_PDP[secondary].isPrimary = false;
    pthread_mutex_unlock(&s_PDP[secondary].mutex);
    return 1;
}

int isExistActivePdp() {
    int cid;
    for (cid = 0; cid < MAX_PDP; cid++) {
        pthread_mutex_lock(&s_PDP[cid].mutex);
        if (s_PDP[cid].state == PDP_BUSY) {
            pthread_mutex_unlock(&s_PDP[cid].mutex);
            RLOGD("PDP[0].state = %d, PDP[1].state = %d, PDP[2].state = %d",
                    s_PDP[0].state, s_PDP[1].state, s_PDP[2].state);
            RLOGD("PDP[%d] is busy now", cid);
            return 1;
        }
        pthread_mutex_unlock(&s_PDP[cid].mutex);
    }

    return 0;
}

static void convertFailCause(RIL_SOCKET_ID socket_id, int cause) {
    int failCause = cause;

    switch (failCause) {
        case MN_GPRS_ERR_NO_SATISFIED_RESOURCE:
        case MN_GPRS_ERR_INSUFF_RESOURCE:
        case MN_GPRS_ERR_MEM_ALLOC:
        case MN_GPRS_ERR_LLC_SND_FAILURE:
        case MN_GPRS_ERR_OPERATION_NOT_ALLOWED:
        case MN_GPRS_ERR_SPACE_NOT_ENOUGH:
        case MN_GPRS_ERR_TEMPORARILY_BLOCKED:
            s_lastPDPFailCause[socket_id] = PDP_FAIL_INSUFFICIENT_RESOURCES;
            break;
        case MN_GPRS_ERR_SERVICE_OPTION_OUTOF_ORDER:
        case MN_GPRS_ERR_OUT_OF_ORDER_SERVICE_OPTION:
            s_lastPDPFailCause[socket_id] =
                    PDP_FAIL_SERVICE_OPTION_OUT_OF_ORDER;
            break;
        case MN_GPRS_ERR_PDP_AUTHENTICATION_FAILED:
        case MN_GPRS_ERR_AUTHENTICATION_FAILURE:
            s_lastPDPFailCause[socket_id] = PDP_FAIL_USER_AUTHENTICATION;
            break;
        case MN_GPRS_ERR_NO_NSAPI:
        case MN_GPRS_ERR_PDP_TYPE:
        case MN_GPRS_ERR_PDP_ID:
        case MN_GPRS_ERR_NSAPI:
        case MN_GPRS_ERR_UNKNOWN_PDP_ADDR_OR_TYPE:
        case MN_GPRS_ERR_INVALID_TI:
            s_lastPDPFailCause[socket_id] = PDP_FAIL_UNKNOWN_PDP_ADDRESS_TYPE;
            break;
        case MN_GPRS_ERR_SERVICE_OPTION_NOT_SUPPORTED:
        case MN_GPRS_ERR_UNSUPPORTED_SERVICE_OPTION:
        case MN_GPRS_ERR_FEATURE_NOT_SUPPORTED:
        case MN_GPRS_ERR_QOS_NOT_ACCEPTED:
        case MN_GPRS_ERR_ATC_PARAM:
        case MN_GPRS_ERR_PERMENANT_PROBLEM:
        case MN_GPRS_ERR_READ_TYPE:
        case MN_GPRS_ERR_STARTUP_FAILURE:
            s_lastPDPFailCause[socket_id] =
                    PDP_FAIL_SERVICE_OPTION_NOT_SUPPORTED;
            break;
        case MN_GPRS_ERR_ACTIVE_REJCET:
        case MN_GPRS_ERR_REQUEST_SERVICE_OPTION_NOT_SUBSCRIBED:
        case MN_GPRS_ERR_UNSUBSCRIBED_SERVICE_OPTION:
            s_lastPDPFailCause[socket_id] =
                    PDP_FAIL_SERVICE_OPTION_NOT_SUBSCRIBED;
            break;
        case MN_GPRS_ERR_ACTIVATION_REJ_GGSN:
            s_lastPDPFailCause[socket_id] = PDP_FAIL_ACTIVATION_REJECT_GGSN;
            break;
        case MN_GPRS_ERR_ACTIVATION_REJ:
        case MN_GPRS_ERR_MODIFY_REJ:
        case MN_GPRS_ERR_SM_ERR_UNSPECIFIED:
            s_lastPDPFailCause[socket_id] =
                    PDP_FAIL_ACTIVATION_REJECT_UNSPECIFIED;
            break;
        case MN_GPRS_ERR_MISSING_OR_UNKOWN_APN:
        case MN_GPRS_ERR_UNKNOWN_APN:
            s_lastPDPFailCause[socket_id] = PDP_FAIL_MISSING_UKNOWN_APN;
            break;
        case MN_GPRS_ERR_SAME_PDP_CONTEXT:
        case MN_GPRS_ERR_NSAPI_ALREADY_USED:
            s_lastPDPFailCause[socket_id] = PDP_FAIL_NSAPI_IN_USE;
            break;
        case MN_GPRS_ERR_OPERATOR_DETERMINE_BAR:
            s_lastPDPFailCause[socket_id] = PDP_FAIL_OPERATOR_BARRED;
            break;
        case MN_GPRS_ERR_INCORRECT_MSG:
        case MN_GPRS_ERR_SYNTACTICAL_ERROR_IN_TFT_OP:
        case MN_GPRS_ERR_SEMANTIC_ERROR_IN_PACKET_FILTER:
        case MN_GPRS_ERR_SYNTAX_ERROR_IN_PACKET_FILTER:
        case MN_GPRS_ERR_PDP_CONTEXT_WO_TFT_ALREADY_ACT:
        case MN_GPRS_ERR_CONTEXT_CAUSE_CONDITIONAL_IE_ERROR:
        case MN_GPRS_ERR_UNIMPLE_MSG_TYPE:
        case MN_GPRS_ERR_UNIMPLE_IE:
        case MN_GPRS_ERR_INCOMP_MSG_PROTO_STAT:
        case MN_GPRS_ERR_SEMANTIC_ERROR_IN_TFT_OP:
        case MN_GPRS_ERR_INCOMPAT_MSG_TYP_PROTO_STAT:
        case MN_GPRS_ERR_UNKNOWN_PDP_CONTEXT:
        case MN_GPRS_ERR_NO_PDP_CONTEXT:
        case MN_GPRS_ERR_PDP_CONTEXT_ACTIVATED:
        case MN_GPRS_ERR_INVALID_MAND_INFO:
        case MN_GPRS_ERR_PRIMITIVE:
            s_lastPDPFailCause[socket_id] = PDP_FAIL_PROTOCOL_ERRORS;
            break;
        case MN_GPRS_ERR_SENDER:
        case MN_GPRS_ERR_RETRYING:
        case MN_GPRS_ERR_UNKNOWN_ERROR:
        case MN_GPRS_ERR_REGULAR_DEACTIVATION:
        case MN_GPRS_ERR_REACTIVATION_REQD:
        case MN_GPRS_ERR_UNSPECIFIED:
            s_lastPDPFailCause[socket_id] = PDP_FAIL_ERROR_UNSPECIFIED;
            break;
        default:
            s_lastPDPFailCause[socket_id] = PDP_FAIL_ERROR_UNSPECIFIED;
            break;
    }
}

/**
 * return  0: success;
 *         1: general failed;
 *         2: fail cause 288, need retry active;
 *         3: fail cause 128
 */
static int errorHandlingForCGDATA(int channelID, ATResponse *p_response,
                                      int err, int cid) {
    int failCause = DATA_ACTIVE_SUCCESS;
    int ret = 0;
    char cmd[AT_COMMAND_LEN] = {0};
    char *line;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    if (err < 0 || p_response->success == 0) {
        ret = DATA_ACTIVE_FAILED;
        if (strStartsWith(p_response->finalResponse, "+CME ERROR:")) {
            line = p_response->finalResponse;
            err = at_tok_start(&line);
            if (err >= 0) {
                err = at_tok_nextint(&line, &failCause);
                if (err >= 0) {
                    RLOGD("Data Active failed with error cause: %d", failCause);
                    if (failCause == 288 || failCause == 128) {
                         ret = DATA_ACTIVE_NEED_RETRY;  // 128: network reject
                    } else {
                        convertFailCause(socket_id, failCause);
                    }
                } else {
                    s_lastPDPFailCause[socket_id] = PDP_FAIL_ERROR_UNSPECIFIED;
                }
            }
        } else {
            s_lastPDPFailCause[socket_id] = PDP_FAIL_ERROR_UNSPECIFIED;
        }
        // when cgdata timeout then send deactive to modem
        if (strStartsWith(p_response->finalResponse, "ERROR")) {
            s_lastPDPFailCause[socket_id] = PDP_FAIL_ERROR_UNSPECIFIED;
            snprintf(cmd, sizeof(cmd), "AT+CGACT=0,%d", cid);
            at_send_command(s_ATChannels[channelID], cmd, NULL);
        }
    }
    return ret;
}

static int getSPACTFBcause(int channelID) {
    int err = 0, cause = -1;
    char *line;
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    err = at_send_command_singleline(s_ATChannels[channelID],
            "AT+SPACTFB?", "+SPACTFB:", &p_response);
    if (err < 0 || p_response->success == 0) {
        s_lastPDPFailCause[socket_id] = PDP_FAIL_ERROR_UNSPECIFIED;
    } else {
        line = p_response->p_intermediates->line;
        err = at_tok_start(&line);
        if (err < 0) return cause;
        err = at_tok_nextint(&line, &cause);
        if (err < 0) return cause;
    }
    at_response_free(p_response);
    RLOGD("getSPACTFBcause cause = %d", cause);
    return cause;
}

static int queryAllActivePDN(int channelID) {
    int err = 0;
    int n, skip, active;
    char *line;
    ATLine *pCur;
    PDNInfo *pdns = s_PDN;
    ATResponse *pdnResponse = NULL;

    s_activePDN = 0;
    err = at_send_command_multiline(s_ATChannels[channelID], "AT+CGACT?",
                                    "+CGACT:", &pdnResponse);
    for (pCur = pdnResponse->p_intermediates; pCur != NULL;
         pCur = pCur->p_next) {
        line = pCur->line;
        err = at_tok_start(&line);
        if (err < 0) {
            pdns->nCid = -1;
        }
        err = at_tok_nextint(&line, &pdns->nCid);
        if (err < 0) {
            pdns->nCid = -1;
        }
        RLOGD("queryAllActivePDN CGACT? cid= %d", pdns->nCid);
        err = at_tok_nextint(&line, &active);
        if (err < 0 || active == 0) {
            pdns->nCid = -1;
        }
        RLOGD("queryAllActivePDN CGACT? active= %d", active);
        if (active == 1) {
            s_activePDN++;
        }
        pdns++;
    }
    RLOGD("queryAllActivePDN s_activePDN= %d", s_activePDN);
    AT_RESPONSE_FREE(pdnResponse);
    err = at_send_command_multiline(s_ATChannels[channelID], "AT+CGDCONT?",
                                    "+CGDCONT:", &pdnResponse);
    if (err != 0 || pdnResponse->success == 0) {
        at_response_free(pdnResponse);
        return s_activePDN;
    }

    for (pCur = pdnResponse->p_intermediates; pCur != NULL;
         pCur = pCur->p_next) {
        line = pCur->line;
        int cid;
        char *type;
        char *apn;
        err = at_tok_start(&line);
        if (err < 0) {
            RLOGI("queryAllActivePDN CGDCONT? read line failed!");
            continue;
        }
        err = at_tok_nextint(&line, &cid);
        if ((err < 0) || (cid != s_PDN[cid-1].nCid)) {
            RLOGI("queryAllActivePDN CGDCONT? read cid failed!");
            continue;
        }

        /* type */
        err = at_tok_nextstr(&line, &type);
        if (err < 0) {
            s_PDN[cid-1].nCid = -1;
        }
        strcpy(s_PDN[cid-1].strIPType, type);
        /* apn */
        err = at_tok_nextstr(&line, &apn);
        if (err < 0) {
            s_PDN[cid-1].nCid = -1;
        }
        strcpy(s_PDN[cid-1].strApn, apn);
        RLOGI("queryAllActivePDN active s_PDN: cid = %d, iptype = %s, apn = %s",
              s_PDN[cid-1].nCid, s_PDN[cid-1].strIPType, s_PDN[cid-1].strApn);
    }
    at_response_free(pdnResponse);
    return s_activePDN;
}

int getPDNCid(int index) {
    if (index >= MAX_PDP_CP || index < 0) {
        return -1;
    } else {
        return s_PDN[index].nCid;
    }
}

char *getPDNIPType(int index) {
    if (index >= MAX_PDP_CP || index < 0) {
        return NULL;
    } else {
        return s_PDN[index].strIPType;
    }
}

char *getPDNAPN(int index) {
    if (index >= MAX_PDP_CP || index < 0) {
        return NULL;
    } else {
        return s_PDN[index].strApn;
    }
}

static int checkCmpAnchor(char *apn) {
    if (apn == NULL || strlen(apn) == 0) {
        return 0;
    }

    const int len = strlen(apn);
    int i;
    int nDotCount = 0;
    char strApn[ARRAY_SIZE] = {0};
    char tmp[ARRAY_SIZE] = {0};
    static char *str[] = {".GPRS", ".MCC", ".MNC"};

    // if the length of apn is less than "mncxxx.mccxxx.gprs",
    // we would not continue to check.
    if (len <= MINIMUM_APN_LEN) {
        return len;
    }

    strcpy(strApn, apn);
    RLOGD("getOrgApnlen: apn = %s, strApn = %s, len = %d", apn, strApn, len);

    strncpy(tmp, apn + (len - 5), 5);
    RLOGD("getOrgApnlen: tmp = %s", tmp);
    if (strcasecmp(str[0], tmp)) {
        return len;
    }
    memset(tmp, 0, sizeof(tmp));

    strncpy(tmp, apn + (len - 12), strlen(str[1]));
    RLOGD("getOrgApnlen: tmp = %s", tmp);
    if (strcasecmp(str[1], tmp)) {
        return len;
    }
    memset(tmp, 0, sizeof(tmp));

    strncpy(tmp, apn + (len - MINIMUM_APN_LEN), strlen(str[2]));
    RLOGD("getOrgApnlen: tmp = %s", tmp);
    if (strcasecmp(str[2], tmp)) {
        return len;
    }
    return (len - MINIMUM_APN_LEN);
}

static void dumpDataResponse(RIL_Data_Call_Response_v11 *pDest) {
    RLOGD("status=%d", pDest->status);
    RLOGD("suggestedRetryTime=%d", pDest->suggestedRetryTime);
    RLOGD("cid=%d", pDest->cid);
    RLOGD("active = %d", pDest->active);
    RLOGD("type=%s", pDest->type);
    RLOGD("ifname = %s", pDest->ifname);
    RLOGD("address=%s", pDest->addresses);
    RLOGD("dns=%s", pDest->dnses);
    RLOGD("gateways = %s", pDest->gateways);
}

static int deactivateLteDataConnection(int channelID, char *cmd) {
    int err = 0, failCause = 0;
    int ret = -1;
    char *line;
    ATResponse *p_response = NULL;

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    if (cmd == NULL) {
        RLOGD("deactivateLteDataConnection cmd is NULL!! return -1");
        return ret;
    }

    RLOGD("deactivateLteDataConnection cmd = %s", cmd);
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (err < 0 || p_response->success == 0) {
        if (p_response->finalResponse != NULL &&
            strStartsWith(p_response->finalResponse, "+CME ERROR:")) {
            line = p_response->finalResponse;
            err = at_tok_start(&line);
            if (err >= 0) {
                err = at_tok_nextint(&line, &failCause);
                // TODO: 151? meaning-less
                if (err >= 0 && failCause == 151) {
                    ret = 1;
                    RLOGD("get 151 error, do detach! s_workMode = %d",
                          s_workMode[socket_id]);
                    if (s_workMode[socket_id] != 10) {
                        at_send_command(s_ATChannels[channelID],
                                        "AT+CLSSPDT = 1", NULL);
                    } else {
                        at_send_command(s_ATChannels[channelID], "AT+SGFD",
                                        NULL);
                    }
                    pthread_mutex_lock(&s_LTEAttachMutex[socket_id]);
                    s_LTERegState[socket_id] = STATE_OUT_OF_SERVICE;
                    pthread_mutex_unlock(&s_LTEAttachMutex[socket_id]);
                    RLOGD("set s_LTERegState: OUT OF SERVICE.");
                }
            }
        }
    } else {
        ret = 1;
    }
    at_response_free(p_response);
    RLOGD("deactivateLteDataConnection ret = %d", ret);
    return ret;
}

/*
 * return : -1: Active Cid success,but isnt fall back cid ip type;
 *           0: Active Cid success;
 *           1: Active Cid failed;
 *           2: Active Cid failed, error 288, need retry ;
 *           3: Active Cid failed, error 128, need do fall back ? need_debug ;
 */
static int activeSpeciedCidProcess(int channelID, void *data, int cid,
                                       const char *pdp_type, int primaryCid) {
    int err;
    int ret = 1, ipType;
    int islte = isLte();
    int failCause;
    char *line;
    char cmd[AT_COMMAND_LEN] = {0};
    char qosState[PROPERTY_VALUE_MAX] = {0};
    char eth[PROPERTY_VALUE_MAX] = {0};
    char prop[PROPERTY_VALUE_MAX] = {0};
    const char *apn, *username, *password, *authtype;
    ATResponse *p_response = NULL;

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    apn = ((const char **)data)[2];
    username = ((const char **)data)[3];
    password = ((const char **)data)[4];
    authtype = ((const char **)data)[5];

    snprintf(cmd, sizeof(cmd), "AT+CGACT=0,%d", cid);
    if (!islte) {
        at_send_command(s_ATChannels[channelID], cmd, NULL);
    } else {
        if (deactivateLteDataConnection(channelID, cmd) < 0) {
            s_lastPDPFailCause[socket_id] = PDP_FAIL_ERROR_UNSPECIFIED;
            putPDP(cid - 1);
            return ret;
        }
    }

    if (!strcmp(pdp_type, "IPV4+IPV6")) {
        snprintf(cmd, sizeof(cmd), "AT+CGDCONT=%d,\"IP\",\"%s\",\"\",0,0",
                  cid, apn);
    } else {
        snprintf(cmd, sizeof(cmd), "AT+CGDCONT=%d,\"%s\",\"%s\",\"\",0,0",
                  cid, pdp_type, apn);
    }
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (err < 0 || p_response->success == 0) {
        s_lastPDPFailCause[socket_id] = PDP_FAIL_ERROR_UNSPECIFIED;
        putPDP(cid - 1);
        at_response_free(p_response);
        return ret;
    }
    AT_RESPONSE_FREE(p_response);

    snprintf(cmd, sizeof(cmd), "AT+CGPCO=0,\"%s\",\"%s\",%d,%d", username,
              password, cid, atoi(authtype));
    at_send_command(s_ATChannels[channelID], cmd, NULL);
    /* Set required QoS params to default */
    property_get("persist.sys.qosstate", qosState, "0");
    if (!strcmp(qosState, "0")) {
        snprintf(cmd, sizeof(cmd),
                  "AT+CGEQREQ=%d,2,0,0,0,0,2,0,\"1e4\",\"0e0\",3,0,0", cid);
        at_send_command(s_ATChannels[channelID], cmd, NULL);
    }

    if (islte) {
        if (primaryCid > 0) {
            snprintf(cmd, sizeof(cmd), "AT+CGDATA=\"M-ETHER\",%d, %d", cid,
                      primaryCid);
        } else {
            snprintf(cmd, sizeof(cmd), "AT+CGDATA=\"M-ETHER\",%d", cid);
        }
    } else {
        if (primaryCid > 0) {
            snprintf(cmd, sizeof(cmd), "AT+CGDATA=\"PPP\",%d, %d", cid,
                      primaryCid);
        } else {
            snprintf(cmd, sizeof(cmd), "AT+CGDATA=\"PPP\",%d", cid);
        }
    }
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    ret = errorHandlingForCGDATA(channelID, p_response, err, cid);
    AT_RESPONSE_FREE(p_response);
    if (ret != DATA_ACTIVE_SUCCESS) {
        putPDP(cid - 1);
        return ret;
    }
    if (primaryCid > 0) {
        /* Check ip type after fall back  */
        char ethPropName[ARRAY_SIZE] = {0};
        snprintf(ethPropName, sizeof(ethPropName), "ro.modem.%s.eth", s_modem);
        property_get(ethPropName, eth, "veth");
        snprintf(cmd, sizeof(cmd), "net.%s%d.ip_type", eth, primaryCid - 1);
        property_get(cmd, prop, "0");
        ipType = atoi(prop);
        RLOGD("Fallback 2 s_PDP: prop = %s, fb_ip = %d", prop, ipType);
        if (ipType != IPV4V6) {
            RLOGD("Fallback s_PDP type mismatch, do deactive");
            snprintf(cmd, sizeof(cmd), "AT+CGACT=0,%d", cid);
            at_send_command(s_ATChannels[channelID], cmd, &p_response);
            at_response_free(p_response);
            putPDP(cid - 1);
            ret = DATA_ACTIVE_FALLBACK_FAILED;
        } else {
            setPDPMapping(primaryCid - 1, cid - 1);
            ret = DATA_ACTIVE_SUCCESS;
        }
    } else {
        updatePDPCid(cid, 1);
        ret = DATA_ACTIVE_SUCCESS;
    }
    return ret;
}

/*
 * return  NULL :  Dont need fallback
 *         other:  FallBack s_PDP type
 */
static const char *checkNeedFallBack(int channelID, char *pdp_type,
                                         int cidIndex) {
    int fbCause, ipType;
    char *ret = NULL;
    char cmd[AT_COMMAND_LEN] = {0};
    char prop[PROPERTY_VALUE_MAX] = {0};
    char eth[PROPERTY_VALUE_MAX] = {0};
    char ethPropName[ARRAY_SIZE] = {0};

    /* Check if need fall back or not */
    snprintf(ethPropName, sizeof(ethPropName), "ro.modem.%s.eth", s_modem);
    property_get(ethPropName, eth, "veth");
    snprintf(cmd, sizeof(cmd), "net.%s%d.ip_type", eth, cidIndex);
    property_get(cmd, prop, "0");
    ipType = atoi(prop);

    if (!strcmp(pdp_type, "IPV4V6") && ipType != IPV4V6) {
        fbCause = getSPACTFBcause(channelID);
        RLOGD("requestSetupDataCall fall Back Cause = %d", fbCause);
        if (fbCause == 52) {
            if (ipType == IPV4) {
                ret = "IPV6";
            } else if (ipType == IPV6) {
                ret = "IP";
            }
        }
    }
    return ret;
}

static void requestOrSendDataCallList(int channelID, int cid,
                                           RIL_Token *t) {
    int err;
    int i, n = 0;
    int count = 0;
    char *out;
    char *line;
    char eth[PROPERTY_VALUE_MAX] = {0};
    bool islte = isLte();
    ATLine *p_cur;
    ATResponse *p_response;
    RIL_Data_Call_Response_v11 *responses;
    RIL_Data_Call_Response_v11 *response;

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    RLOGD("requestOrSendDataCallList, cid: %d", cid);
    err = at_send_command_multiline(s_ATChannels[channelID], "AT+CGACT?",
                                    "+CGACT:", &p_response);
    if (err != 0 || p_response->success == 0) {
        if (t != NULL) {
            RIL_onRequestComplete(*t, RIL_E_GENERIC_FAILURE, NULL, 0);
        } else {
            RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED, NULL, 0,
                                      socket_id);
        }
        at_response_free(p_response);
        return;
    }
    for (p_cur = p_response->p_intermediates; p_cur != NULL;
         p_cur = p_cur->p_next) {
        n++;
    }

    responses = alloca(n * sizeof(RIL_Data_Call_Response_v11));
    for (i = 0; i < n; i++) {
        responses[i].status = -1;
        responses[i].suggestedRetryTime = -1;
        responses[i].cid = -1;
        responses[i].active = -1;
        responses[i].type = "";
        responses[i].ifname = "";
        responses[i].addresses = "";
        responses[i].dnses = "";
        responses[i].gateways = "";
        responses[i].pcscf = "";
        responses[i].mtu = 0;
    }
    response = responses;
    for (p_cur = p_response->p_intermediates; p_cur != NULL;
         p_cur = p_cur->p_next) {
        line = p_cur->line;
        err = at_tok_start(&line);
        if (err < 0) goto error;
        err = at_tok_nextint(&line, &response->cid);
        if (err < 0) goto error;
        err = at_tok_nextint(&line, &response->active);
        if (err < 0) goto error;

        response++;
    }
    AT_RESPONSE_FREE(p_response);

    err = at_send_command_multiline(s_ATChannels[channelID], "AT+CGDCONT?",
                                    "+CGDCONT:", &p_response);
    if (err != 0 || p_response->success == 0) {
        if (t != NULL) {
            RIL_onRequestComplete(*t, RIL_E_GENERIC_FAILURE, NULL, 0);
        } else {
            RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED, NULL, 0,
                                      socket_id);
        }
        at_response_free(p_response);
        return;
    }
    for (p_cur = p_response->p_intermediates; p_cur != NULL;
         p_cur = p_cur->p_next) {
        char *line = p_cur->line;
        int ncid;
        int nn;
        int ipType = 0;
        char *type;
        char *apn;
        char *address;
        char cmd[AT_COMMAND_LEN] = {0};
        char prop[PROPERTY_VALUE_MAX] = {0};
        char ethPropName[ARRAY_SIZE] = {0};
        const int IPListSize = 180;
        const int DNSListSize = 180;
        char *iplist = NULL;
        char *dnslist = NULL;
        const char *separator = "";

        err = at_tok_start(&line);
        if (err < 0) goto error;

        err = at_tok_nextint(&line, &ncid);
        if (err < 0) goto error;

        if ((ncid == cid) && (t == NULL)) {  // for bug407591
            RLOGD("No need to get IP, FWK will do deact by check IP");
            responses[cid - 1].status = PDP_FAIL_OPERATOR_BARRED;
            if (getPDPState(cid - 1) == PDP_IDLE) {
                responses[cid - 1].active = 0;
            }
            continue;
        }

        for (i = 0; i < n; i++) {
            if (responses[i].cid == ncid) {
                break;
            }
        }

        if (i >= n) {
            /* details for a context we didn't hear about in the last request */
            continue;
        }
        /* Assume no error */
        responses[i].status = PDP_FAIL_NONE;

        /* type */
        err = at_tok_nextstr(&line, &out);
        if (err < 0) goto error;

        responses[i].type = alloca(strlen(out) + 1);
        strcpy(responses[i].type, out);

        /* APN ignored for v5 */
        err = at_tok_nextstr(&line, &out);
        if (err < 0) goto error;

        snprintf(ethPropName, sizeof(ethPropName), "ro.modem.%s.eth", s_modem);
        property_get(ethPropName, eth, "veth");

        snprintf(cmd, sizeof(cmd), "%s%d", eth, ncid - 1);
        responses[i].ifname = alloca(strlen(cmd) + 1);
        strcpy(responses[i].ifname, cmd);

        snprintf(cmd, sizeof(cmd), "net.%s%d.ip_type", eth, ncid - 1);
        property_get(cmd, prop, "0");
        ipType = atoi(prop);
        RLOGE("prop = %s, ipType = %d", prop, ipType);
        dnslist = alloca(DNSListSize);

        if (ipType == IPV4) {
            snprintf(cmd, sizeof(cmd), "net.%s%d.ip", eth, ncid - 1);
            property_get(cmd, prop, NULL);
            RLOGE("IPV4 cmd=%s, prop = %s", cmd, prop);
            responses[i].addresses = alloca(strlen(prop) + 1);
            responses[i].gateways = alloca(strlen(prop) + 1);
            strcpy(responses[i].addresses, prop);
            strcpy(responses[i].gateways, prop);

            dnslist[0] = 0;
            for (nn = 0; nn < 2; nn++) {
                snprintf(cmd, sizeof(cmd), "net.%s%d.dns%d", eth, ncid - 1,
                          nn + 1);
                property_get(cmd, prop, NULL);
                /* Append the DNS IP address */
                strlcat(dnslist, separator, DNSListSize);
                strlcat(dnslist, prop, DNSListSize);
                separator = " ";
            }
            responses[i].dnses = dnslist;
        } else if (ipType == IPV6) {
            snprintf(cmd, sizeof(cmd), "net.%s%d.ipv6_ip", eth, ncid - 1);
            property_get(cmd, prop, NULL);
            RLOGE("IPV6 cmd=%s, prop = %s", cmd, prop);
            responses[i].addresses = alloca(strlen(prop) + 1);
            responses[i].gateways = alloca(strlen(prop) + 1);
            strcpy(responses[i].addresses, prop);
            strcpy(responses[i].gateways, prop);
            dnslist[0] = 0;
            for (nn = 0; nn < 2; nn++) {
                snprintf(cmd, sizeof(cmd), "net.%s%d.ipv6_dns%d", eth, ncid-1,
                          nn + 1);
                property_get(cmd, prop, NULL);

                /* Append the DNS IP address */
                strlcat(dnslist, separator, DNSListSize);
                strlcat(dnslist, prop, DNSListSize);
                separator = " ";
            }
            responses[i].dnses = dnslist;
        } else if (ipType == IPV4V6) {
            responses[i].type = alloca(strlen("IPV4V6") + 1);
            // for Fallback, change two net interface to one
            strcpy(responses[i].type, "IPV4V6");
            iplist = alloca(IPListSize);
            separator = " ";
            iplist[0] = 0;
            snprintf(cmd, sizeof(cmd), "net.%s%d.ip", eth, ncid - 1);
            property_get(cmd, prop, NULL);
            strlcat(iplist, prop, IPListSize);
            strlcat(iplist, separator, IPListSize);
            RLOGE("IPV4V6 cmd = %s, prop = %s, iplist = %s", cmd, prop, iplist);

            snprintf(cmd, sizeof(cmd), "net.%s%d.ipv6_ip", eth, ncid - 1);
            property_get(cmd, prop, NULL);
            strlcat(iplist, prop, IPListSize);
            responses[i].addresses = iplist;
            responses[i].gateways = iplist;
            RLOGE("IPV4V6 cmd = %s, prop = %s, iplist = %s", cmd, prop, iplist);
            separator = "";
            dnslist[0] = 0;
            for (nn = 0; nn < 2; nn++) {
                snprintf(cmd, sizeof(cmd), "net.%s%d.dns%d", eth, ncid - 1,
                          nn + 1);
                property_get(cmd, prop, NULL);

                /* Append the DNS IP address */
                strlcat(dnslist, separator, DNSListSize);
                strlcat(dnslist, prop, DNSListSize);
                separator = " ";
                RLOGE("IPV4V6 cmd = %s, prop = %s, dnslist = %s", cmd, prop,
                      dnslist);
            }
            for (nn = 0; nn < 2; nn++) {
                snprintf(cmd, sizeof(cmd), "net.%s%d.ipv6_dns%d", eth,
                          ncid - 1, nn + 1);
                property_get(cmd, prop, NULL);

                /* Append the DNS IP address */
                strlcat(dnslist, separator, DNSListSize);
                strlcat(dnslist, prop, DNSListSize);
                separator = " ";
                RLOGE("IPV4V6 cmd=%s, prop = %s,dnslist = %s", cmd, prop,
                      dnslist);
            }
            responses[i].dnses = dnslist;
        } else {
            RLOGE("Unknown IP type!");
        }
        responses[i].mtu = DEFAULT_MTU;
        if ((cid != -1) && (t == NULL)) {
             RLOGE("i = %d", i);
             if ((!responses[i].active) &&
                 strcmp(responses[i].addresses, "")) {
                 responses[i].status = PDP_FAIL_OPERATOR_BARRED;
                 RLOGE("responses[i].addresses = %s", responses[i].addresses);
             }
          }

        RLOGD("status=%d", responses[i].status);
        RLOGD("suggestedRetryTime=%d", responses[i].suggestedRetryTime);
        RLOGD("cid=%d", responses[i].cid);
        RLOGD("active = %d", responses[i].active);
        RLOGD("type=%s", responses[i].type);
        RLOGD("ifname = %s", responses[i].ifname);
        RLOGD("address=%s", responses[i].addresses);
        RLOGD("dns=%s", responses[i].dnses);
        RLOGD("gateways = %s", responses[i].gateways);
    }

    AT_RESPONSE_FREE(p_response);

    if ((t != NULL) && (cid > 0)) {
        RLOGD("requestOrSendDataCallList is called by SetupDataCall!");
        for (i = 0; i < MAX_PDP; i++) {
            if ((responses[i].cid == cid)) {
                if (responses[i].active) {
                    int fb_cid = getFallbackCid(cid - 1);  // pdp fallback cid
                    RLOGD("called by SetupDataCall! fallback cid : %d", fb_cid);
                    if (islte && s_LTEDetached[socket_id]) {
                        RLOGD("Lte detached in the past2.");
                        putPDP(fb_cid -1);
                        putPDP(cid - 1);
                        s_lastPDPFailCause[socket_id] =
                                PDP_FAIL_ERROR_UNSPECIFIED;
                        RIL_onRequestComplete(*t, RIL_E_GENERIC_FAILURE, NULL,
                                              0);
                    } else {
                        RIL_onRequestComplete(*t, RIL_E_SUCCESS, &responses[i],
                                sizeof(RIL_Data_Call_Response_v11));
                        /* send IP for volte addtional business */
                        if (s_addedIPCid ==0 &&
                            !(islte && s_LTEDetached[socket_id]) &&
                            isVoLteEnable()) {
                            char cmd[AT_COMMAND_LEN] = {0};  // TODO: debug
                            char prop0[PROPERTY_VALUE_MAX] = {0};
                            char prop1[PROPERTY_VALUE_MAX] = {0};
                            if (!strcmp(responses[i].type, "IPV4V6")) {
                                snprintf(cmd, sizeof(cmd), "net.%s%d.ip",
                                        eth, cid - 1);
                                property_get(cmd, prop0, NULL);

                                snprintf(cmd, sizeof(cmd), "net.%s%d.ipv6_ip",
                                        eth, cid - 1);
                                property_get(cmd, prop1, NULL);
                                snprintf(cmd, sizeof(cmd),
                                          "AT+XCAPIP=%d,\"%s,[%s]\"", cid,
                                          prop0, prop1);
                            } else if (!strcmp(responses[i].type, "IP")) {
                                snprintf(cmd, sizeof(cmd),
                                    "AT+XCAPIP=%d,\"%s,[FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF]\"",
                                    cid, responses[i].addresses);
                            } else {
                                snprintf(cmd, sizeof(cmd),
                                        "AT+XCAPIP=%d,\"0.0.0.0,[%s]\"", cid,
                                        responses[i].addresses);
                            }
                            at_send_command(s_ATChannels[channelID], cmd, NULL);
                            s_addedIPCid = responses[i].cid;
                        }
                    }
                }
                return;
            } else {
                putPDP(getFallbackCid(cid - 1) - 1);
                putPDP(cid - 1);
            }
        }

        if (i >= MAX_PDP) {
            s_lastPDPFailCause[socket_id] = PDP_FAIL_ERROR_UNSPECIFIED;
            RIL_onRequestComplete(*t, RIL_E_GENERIC_FAILURE, NULL, 0);
            return;
        }
    }

    if (t != NULL) {
        RIL_onRequestComplete(*t, RIL_E_SUCCESS, responses,
                              n * sizeof(RIL_Data_Call_Response_v11));
    } else {
        RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED,
                responses, n * sizeof(RIL_Data_Call_Response_v11), socket_id);
    }
    s_LTEDetached[socket_id] = false;
    return;

error:
    if (t != NULL) {
        RIL_onRequestComplete(*t, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED, NULL, 0,
                                  socket_id);
    }
    at_response_free(p_response);
}

/*
 * return : -1: Dont reuse defaulte bearer;
 *           0: Reuse defaulte bearer success;
 *          >0: Reuse failed, the failed cid;
 */
static int reuseDefaultBearer(int channelID, const char *apn,
                                  RIL_Token t) {
    bool islte = isLte();
    int err, ret = -1;
    char strApnName[ARRAY_SIZE] = {0};
    char cmd[AT_COMMAND_LEN] = {0};
    char prop[PROPERTY_VALUE_MAX] = {0};
    ATResponse *p_response = NULL;

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    if (islte && s_workMode[socket_id] != 10) {
        queryAllActivePDN(channelID);
        int i, cid;
        if (s_activePDN > 0) {
            for (i = 0; i < MAX_PDP_CP; i++) {
                cid = getPDNCid(i);
                if (cid == (i + 1)) {
                    strncpy(strApnName, getPDNAPN(i),
                             checkCmpAnchor(s_PDN[i].strApn));
                    strApnName[strlen(strApnName)] = '\0';
                    if (i < MAX_PDP) {
                        RLOGD("s_PDP[%d].state = %d", i, getPDPState(i));
                    }
                    if (i < MAX_PDP && (!strcasecmp(getPDNAPN(i), apn) ||
                        !strcasecmp(strApnName, apn)) &&
                        (getPDPState(i) == PDP_IDLE)) {
                        RLOGD("Using default PDN");
                        getPDPByIndex(i);
                        snprintf(cmd, sizeof(cmd), "AT+CGACT=0,%d,%d", cid,
                                  0);
                        at_send_command(s_ATChannels[channelID], cmd,
                                        &p_response);
                        AT_RESPONSE_FREE(p_response);
                        snprintf(cmd, sizeof(cmd), "AT+CGDATA=\"M-ETHER\",%d",
                                  cid);
                        err = at_send_command(s_ATChannels[channelID], cmd,
                                              &p_response);
                        if (errorHandlingForCGDATA(channelID, p_response, err,
                                                   cid) != 0) {
                            ret = i;
                            at_response_free(p_response);
                            return ret;
                        }
                        updatePDPCid(i + 1, 1);
                        requestOrSendDataCallList(channelID, cid, &t);
                        ret = 0;
                        at_response_free(p_response);
                    }
                } else if (i < MAX_PDP) {
                    putPDP(i);
                }
            }
        } else {
            for (i = 0; i < MAX_PDP; i++) {
                putPDP(i);
            }
        }
    }
    return ret;
}

static void requestSetupDataCall(int channelID, void *data, size_t datalen,
                                     RIL_Token t) {
    int err;
    int index = -1, primaryindex = -1;
    int nRetryTimes = 0;
    int ret;
    int isFallback = 0;
    const char *pdpType;
    const char *apn = NULL;

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    RLOGD("requestSetupDataCall data[0] '%s'", ((const char **)data)[0]);
    RLOGD("requestSetupDataCall data[1] '%s'", ((const char **)data)[1]);
    RLOGD("requestSetupDataCall data[2] '%s'", ((const char **)data)[2]);
    RLOGD("requestSetupDataCall data[3] '%s'", ((const char **)data)[3]);
    RLOGD("requestSetupDataCall data[4] '%s'", ((const char **)data)[4]);
    RLOGD("requestSetupDataCall data[5] '%s'", ((const char **)data)[5]);
    apn = ((const char **)data)[2];

    if ((strstr(apn, "wap") == NULL) && (s_addedIPCid == -1)) {
        s_addedIPCid = 0;
    }
    /* for DDR power consumption */
    if (isVoLteEnable() && !isExistActivePdp()) {
        at_send_command(s_ATChannels[channelID], "AT+SPVOOLTE=0", NULL);
    }

RETRY:
    s_LTEDetached[socket_id] = false;
    /* check if reuse defaulte bearer or not */
    ret = reuseDefaultBearer(channelID, apn, t);
    if (ret == 0) {
        return;
    } else if (ret > 0) {
        primaryindex = ret;
        goto error;
    }
    /* Dont need reuse defaulte bearer */
    if (datalen > 6 * sizeof(char *)) {
        pdpType = ((const char **) data)[6];
    } else {
        pdpType = "IP";
    }

    index = getPDP(socket_id);
    if (index < 0 || getPDPCid(index) >= 0) {
        s_lastPDPFailCause[socket_id] = PDP_FAIL_ERROR_UNSPECIFIED;
        goto error;
    }
    primaryindex = index;
    ret = activeSpeciedCidProcess(channelID, data, index + 1, pdpType, 0);
    if (ret == DATA_ACTIVE_NEED_RETRY) {
        if (nRetryTimes < 5) {
            nRetryTimes++;
            goto RETRY;
        }
    } else if (ret == DATA_ACTIVE_SUCCESS &&
            (!strcmp(pdpType, "IPV4V6") || !strcmp(pdpType, "IPV4+IPV6"))) {
        const char *tmpType = NULL;
        /* Check if need fall back or not */
        if (!strcmp(pdpType, "IPV4+IPV6")) {  // TODO: debug
            tmpType = "IPV6";
        } else if (!strcmp(pdpType, "IPV4V6")) {
            tmpType = checkNeedFallBack(channelID, pdpType, index);
            isFallback = 1;
        }
        if (tmpType == NULL) {  // dont need fallback
            goto done;
        }
        index = getPDP(socket_id);
        if (index < 0 || getPDPCid(index) >= 0) {
            /* just use actived IP */
            goto done;
        }
        if (isFallback == 1) {
            activeSpeciedCidProcess(channelID, data, index + 1, tmpType,
                                    primaryindex + 1);
        } else {  // IPV4+IPV6
            activeSpeciedCidProcess(channelID, data, index+1, tmpType, 0);
        }
    } else if (ret != DATA_ACTIVE_SUCCESS) {
        goto error;
    }

done:
    requestOrSendDataCallList(channelID, primaryindex + 1, &t);
    return;

error:
    if (primaryindex >= 0) {
        putPDP(getFallbackCid(primaryindex) - 1);
        putPDP(primaryindex);
    }
    if (s_addedIPCid == 0) {
        s_addedIPCid = -1;
    }
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}



static void deactivateDataConnection(int channelID, void *data,
                                          size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    bool needfake = false;
    bool islte = isLte();
    int err = 0, i = 0;
    int cid;
    int secondaryCid = -1;
    char cmd[AT_COMMAND_LEN];
    char prop[PROPERTY_VALUE_MAX] = {0};
    const char *p_cid = NULL;
    ATResponse *p_response = NULL;

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    p_cid = ((const char **)data)[0];
    cid = atoi(p_cid);
    if (cid < 1) {
        goto error;
    }

    RLOGD("deactivateDC s_in4G[%d]=%d", socket_id, s_in4G[socket_id]);
    secondaryCid = getFallbackCid(cid - 1);
    if (s_in4G[socket_id]) {
        queryAllActivePDN(channelID);
        /* s_in4G, if current is actived, don't deactive the last pdp */
        if (s_activePDN == 2 && secondaryCid != -1) {
            needfake = (getPDPCid(cid - 1) != -1) &&
                       (getPDPCid(secondaryCid - 1) != -1);
        } else if (s_activePDN == 1 && getPDPCid(cid - 1) != -1) {
            needfake = true;
        } else if (s_activePDN > 1 && s_PDN[cid - 1].nCid != -1) {
            if (s_initialAttachAPNs[socket_id] != NULL &&
                    s_initialAttachAPNs[socket_id]->apn != NULL &&
                    (!strcasecmp(s_PDN[cid - 1].strApn,
                                  s_initialAttachAPNs[socket_id]->apn) ||
                    !strcasecmp(strtok(s_PDN[cid - 1].strApn, "."),
                                 s_initialAttachAPNs[socket_id]->apn))) {
                needfake = true;
            }
        }
    }
    if (needfake) {
        snprintf(cmd, sizeof(cmd), "AT+CGACT=0,%d,%d", cid, 0);
    } else {
        snprintf(cmd, sizeof(cmd), "AT+CGACT=0,%d", cid);
    }
    at_send_command(s_ATChannels[channelID], cmd, &p_response);
    AT_RESPONSE_FREE(p_response);

    if (secondaryCid != -1) {
        RLOGD("dual PDP, do CGACT again, fallback cid = %d", secondaryCid);
        snprintf(cmd, sizeof(cmd), "AT+CGACT=0,%d", secondaryCid);
        if (islte) {
            deactivateLteDataConnection(channelID, cmd);
        } else {
            at_send_command(s_ATChannels[channelID], cmd, &p_response);
        }
    }

done:
    putPDP(secondaryCid - 1);
    putPDP(cid - 1);
    at_response_free(p_response);
    /* for DDR power consumption */
    if (isVoLteEnable() && !isExistActivePdp()) {
        at_send_command(s_ATChannels[channelID], "AT+SPVOOLTE=1", NULL);
    }

error:
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    return;
}

static void requestSetInitialAttachAPN(int channelID, void *data,
                                            size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int initialAttachId = 1;
    int needIPChange = 0;
    char cmd[AT_COMMAND_LEN] = {0};
    char qosState[PROPERTY_VALUE_MAX] = {0};
    ATResponse *p_response = NULL;

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    RIL_InitialAttachApn *initialAttachApn = s_initialAttachAPNs[socket_id];
    if (initialAttachApn == NULL) {
        s_initialAttachAPNs[socket_id] =
                (RIL_InitialAttachApn *)malloc(sizeof(RIL_InitialAttachApn));
        initialAttachApn = s_initialAttachAPNs[socket_id];
        memset(initialAttachApn, 0, sizeof(RIL_InitialAttachApn));
    }

    if (data != NULL) {
        RIL_InitialAttachApn *pIAApn = (RIL_InitialAttachApn *)data;
        if (pIAApn->apn != NULL) {
            if ((initialAttachApn->apn != NULL) &&
                (strcmp(initialAttachApn->apn, pIAApn->apn) == 0)) {
                needIPChange = 1;
                free(initialAttachApn->apn);
            }
            initialAttachApn->apn = (char *)malloc(
                    strlen(pIAApn->apn) + 1);
            strcpy(initialAttachApn->apn, pIAApn->apn);
        }

        if (pIAApn->protocol != NULL) {
            if (needIPChange && (initialAttachApn->protocol != NULL)
                    && strcmp(initialAttachApn->protocol,
                            pIAApn->protocol)) {
                needIPChange = 2;
                free(initialAttachApn->protocol);
            }
            initialAttachApn->protocol = (char *)malloc(
                    strlen(pIAApn->protocol) + 1);

            strcpy(initialAttachApn->protocol, pIAApn->protocol);
        }

        initialAttachApn->authtype = pIAApn->authtype;

        if (pIAApn->username != NULL) {
            initialAttachApn->username = (char *)malloc(
                    strlen(pIAApn->username) + 1);
            strcpy(initialAttachApn->username, pIAApn->username);
        }

        if (pIAApn->password != NULL) {
            initialAttachApn->password = (char *)malloc(
                    strlen(pIAApn->password) + 1);
            strcpy(initialAttachApn->password, pIAApn->password);
        }
    }

    RLOGD("RIL_REQUEST_SET_INITIAL_ATTACH_APN apn = %s",
            initialAttachApn->apn);
    RLOGD("RIL_REQUEST_SET_INITIAL_ATTACH_APN protocol = %s",
            initialAttachApn->protocol);
    RLOGD("RIL_REQUEST_SET_INITIAL_ATTACH_APN authtype = %d",
            initialAttachApn->authtype);
    RLOGD("RIL_REQUEST_SET_INITIAL_ATTACH_APN username = %s",
            initialAttachApn->username);
    RLOGD("RIL_REQUEST_SET_INITIAL_ATTACH_APN password = %s",
            initialAttachApn->password);

    snprintf(cmd, sizeof(cmd), "AT+CGDCONT=%d,\"%s\",\"%s\",\"\",0,0",
              initialAttachId, initialAttachApn->protocol,
              initialAttachApn->apn);
    at_send_command(s_ATChannels[channelID], cmd, &p_response);

    snprintf(cmd, sizeof(cmd), "AT+CGPCO=0,\"%s\",\"%s\",%d,%d",
              initialAttachApn->username, initialAttachApn->password,
              initialAttachId, initialAttachApn->authtype);
    at_send_command(s_ATChannels[channelID], cmd, NULL);

    /* Set required QoS params to default */
    property_get("persist.sys.qosstate", qosState, "0");
    if (!strcmp(qosState, "0")) {
        snprintf(cmd, sizeof(cmd),
                  "AT+CGEQREQ=%d,2,0,0,0,0,2,0,\"1e4\",\"0e0\",3,0,0",
                  initialAttachId);
        at_send_command(s_ATChannels[channelID], cmd, NULL);
    }
    if (needIPChange == 2) {
        at_send_command(s_ATChannels[channelID], "AT+SPIPTYPECHANGE=1", NULL);
    }
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);
}

/**
 * RIL_REQUEST_LAST_PDP_FAIL_CAUSE
 * Requests the failure cause code for the most recently failed PDP
 * context activate.
 */
void requestLastDataFailCause(int channelID, void *data, size_t datalen,
                                  RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int response = PDP_FAIL_ERROR_UNSPECIFIED;
    RIL_SOCKET_ID socekt_id = getSocketIdByChannelID(channelID);
    response = s_lastPDPFailCause[socekt_id];
    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(int));
}

static void requestDataCallList(int channelID, void *data, size_t datalen,
                                    RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    requestOrSendDataCallList(channelID, -1, &t);
}

static void attachGPRS(int channelID, void *data, size_t datalen,
                          RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int err;
    char cmd[AT_COMMAND_LEN];
    bool islte = isLte();
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

#if defined (ANDROID_MULTI_SIM)
    if (islte && !s_oneSimOnly && s_workMode[socket_id] == 10) {
         RLOGD("attachGPRS simID = %d", socket_id);
         snprintf(cmd, sizeof(cmd), "AT+SPSWITCHDATACARD=%d,1", socket_id);
         at_send_command(s_ATChannels[channelID], cmd, NULL);
         err = at_send_command(s_ATChannels[channelID], "AT+CGATT=1",
                               &p_response);
         if (err < 0 || p_response->success == 0) {
             at_response_free(p_response);
             RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
             return;
         }
    }
#endif
    if (!islte) {
        err = at_send_command(s_ATChannels[channelID], "AT+CGATT=1",
                              &p_response);
        if (err < 0 || p_response->success == 0) {
            at_response_free(p_response);
            RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            return;
        }
    }
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);
    return;
}

static void detachGPRS(int channelID, void *data, size_t datalen,
                          RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int ret;
    int err, i;
    char cmd[AT_COMMAND_LEN];
    bool islte = isLte();
    ATResponse *p_response = NULL;

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    if (islte) {
        for (i = 0; i < MAX_PDP; i++) {
            if (getPDPCid(i) > 0) {
                snprintf(cmd, sizeof(cmd), "AT+CGACT=0,%d", getPDPCid(i));
                at_send_command(s_ATChannels[channelID], cmd, &p_response);
                RLOGD("s_PDP[%d].state = %d", i, getPDPState(i));
                if (s_PDP[i].state == PDP_BUSY) {
                    int cid = getPDPCid(i);
                    putPDP(i);
                    requestOrSendDataCallList(channelID, cid, NULL);
                }
                AT_RESPONSE_FREE(p_response);
            }
        }
#if defined (ANDROID_MULTI_SIM)
        if (!s_oneSimOnly && s_workMode[socket_id] == 10) {
            RLOGD("simID = %d", socket_id);
            snprintf(cmd, sizeof(cmd), "AT+SPSWITCHDATACARD=%d,0", socket_id);
            err = at_send_command(s_ATChannels[channelID], cmd, NULL);
        }
#endif
    }

    err = at_send_command(s_ATChannels[channelID], "AT+SGFD", &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    return;

error:
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    return;
}

void requestAllowData(int channelID, void *data, size_t datalen,
                         RIL_Token t) {
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
    s_dataAllowed[socket_id] = ((int*)data)[0];
    RLOGD("s_desiredRadioState[%d] = %d", socket_id,
          s_desiredRadioState[socket_id]);
    if (s_desiredRadioState[socket_id] > 0) {
        if (s_dataAllowed[socket_id]) {
            attachGPRS(channelID, data, datalen, t);
        } else {
            detachGPRS(channelID, data, datalen, t);
        }
    } else {
        RLOGD("Failed allow data due to radio is off");
        RIL_onRequestComplete(t, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
    }
}

int processDataRequest(int request, void *data, size_t datalen, RIL_Token t,
                          int channelID) {
    int ret = 1;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
    switch (request) {
        case RIL_REQUEST_SETUP_DATA_CALL: {
            if (isLte()) {
                RLOGD("SETUP_DATA_CALL s_LTERegState[%d] = %d", socket_id,
                      s_LTERegState[socket_id]);
                if (s_workMode[socket_id] == 10 ||
                    s_LTERegState[socket_id] == STATE_IN_SERVICE) {
                    requestSetupDataCall(channelID, data, datalen, t);
                } else {
                    s_lastPDPFailCause[socket_id] =
                            PDP_FAIL_SERVICE_OPTION_NOT_SUPPORTED;
                    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
                }
            } else {
                requestSetupDataCall(channelID, data, datalen, t);
            }
            break;
        }
        case RIL_REQUEST_DEACTIVATE_DATA_CALL:
            deactivateDataConnection(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE:
            requestLastDataFailCause(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_DATA_CALL_LIST:
            requestDataCallList(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_ALLOW_DATA:
            requestAllowData(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_SET_INITIAL_ATTACH_APN:
            requestSetInitialAttachAPN(channelID, data, datalen, t);
            break;
        // TODO: for now, not realized
        // case RIL_REQUEST_SET_DATA_PROFILE:
        //     break;
        default :
            ret = 0;
            break;
    }
    return ret;
}

static void onDataCallListChanged(void *param) {
    int channelID;
    CallbackPara *cbPara = (CallbackPara *)param;
    channelID = getChannel(cbPara->socket_id);
    if (cbPara->para == NULL) {
        requestOrSendDataCallList(channelID, -1, NULL);
    } else {
        requestOrSendDataCallList(channelID, *((int *)(cbPara->para)), NULL);
    }

    if (cbPara != NULL) {
        if (cbPara->para != NULL) {
            free(cbPara->para);
        }
        free(cbPara);
    }
    putChannel(channelID);
}

int processDataUnsolicited(RIL_SOCKET_ID socket_id, const char *s) {
    int err;
    int ret = 1;
    char *line = NULL;

    if (strStartsWith(s, "+CGEV:")) {
        char *tmp;
        int pdpState = 1;
        int cid = -1;
        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);
        if (strstr(tmp, "NW PDN ACT")) {
            tmp += strlen(" NW PDN ACT ");
        } else if (strstr(tmp, "NW ACT ")) {
            tmp += strlen(" NW ACT ");
        } else if (strstr(tmp, "NW PDN DEACT")) {
            tmp += strlen(" NW PDN DEACT ");
            pdpState = 0;
        } else {
            RLOGD("Invalid CGEV");
            goto out;
        }
        cid = atoi(tmp);
        if (cid > 0 && cid <= MAX_PDP) {
            RLOGD("update cid %d ", cid);
            updatePDPCid(cid, pdpState);
        }
    } else if (strStartsWith(s, "^CEND:")) {
        int commas;
        int cid =-1;
        int endStatus;
        int ccCause;
        char *p;
        char *tmp;
        extern pthread_mutex_t s_callMutex[];
        extern int s_callFailCause[];

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        commas = 0;
        for (p = tmp; *p != '\0'; p++) {
            if (*p == ',') commas++;
        }
        err = at_tok_nextint(&tmp, &cid);
        if (err < 0) goto out;
        skipNextComma(&tmp);
        err = at_tok_nextint(&tmp, &endStatus);
        if (err < 0) goto out;
        err = at_tok_nextint(&tmp, &ccCause);
        if (err < 0) goto out;
        if (commas == 3) {
            pthread_mutex_lock(&s_callMutex[socket_id]);
            s_callFailCause[socket_id] = ccCause;
            pthread_mutex_unlock(&s_callMutex[socket_id]);
            RLOGD("The last call fail cause: %d", s_callFailCause[socket_id]);
        }
        if (commas == 4) {  /* GPRS reply 5 parameters */
            /* as endStatus 21 means: PDP reject by network,
             * so we not do onDataCallListChanged */
            if (endStatus != 29 && endStatus != 21) {
                if (endStatus == 104) {
                    if (cid > 0 && cid <= MAX_PDP &&
                        s_PDP[cid - 1].state == PDP_BUSY) {
                        CallbackPara *cbPara =
                                (CallbackPara *)malloc(sizeof(CallbackPara));
                        if (cbPara != NULL) {
                            cbPara->para = (int *)malloc(sizeof(int));
                            *((int *)(cbPara->para)) = cid;
                            cbPara->socket_id = socket_id;
                        }
                        RIL_requestTimedCallback(onDataCallListChanged, cbPara,
                                                 NULL);
                    }
                } else {
                    CallbackPara *cbPara =
                            (CallbackPara *)malloc(sizeof(CallbackPara));
                    if (cbPara != NULL) {
                        cbPara->para = NULL;
                        cbPara->socket_id = socket_id;
                    }
                    RIL_requestTimedCallback(onDataCallListChanged, cbPara,
                                             NULL);
                }  // TODO: need_debug, why report voice change?
                RIL_onUnsolicitedResponse(
                        RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED, NULL, 0,
                        socket_id);
            }
        }
    } else {
        ret = 0;
    }

out:
    free(line);
    return ret;
}
