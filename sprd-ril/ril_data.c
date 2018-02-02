/**
 * ril_data.c --- Data-related requests process functions implementation
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#define LOG_TAG "RIL"

#include "sprd_ril.h"
#include "ril_data.h"
#include "ril_network.h"
#include "ril_call.h"

#define APN_DELAY_PROP          "persist.radio.apn_delay"
#define DUALPDP_ALLOWED_PROP    "persist.sys.dualpdp.allowed"
#define DDR_STATUS_PROP         "persist.sys.ddr.status"
#define REUSE_DEFAULT_PDN          "persist.sys.pdp.reuse"

int s_dataAllowed[SIM_COUNT];
int s_manualSearchNetworkId = -1;
/* for LTE, attach will occupy a cid for default PDP in CP */
bool s_LTEDetached[SIM_COUNT] = {0};
static int s_GSCid;
static int s_ethOnOff;
static int s_activePDN;
static int s_addedIPCid = -1;  /* for VoLTE additional business */
static int s_autoDetach = 1;  /* whether support auto detach */
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
static int s_trafficClass[SIM_COUNT] = {
        TRAFFIC_CLASS_DEFAULT
#if (SIM_COUNT >= 2)
        ,TRAFFIC_CLASS_DEFAULT
#if (SIM_COUNT >= 3)
        ,TRAFFIC_CLASS_DEFAULT
#if (SIM_COUNT >= 4)
        ,TRAFFIC_CLASS_DEFAULT
#endif
#endif
#endif
};

static int s_singlePDNAllowed[SIM_COUNT] = {
        0
#if (SIM_COUNT >= 2)
        ,0
#if (SIM_COUNT >= 3)
        ,0
#if (SIM_COUNT >= 4)
        ,0
#endif
#endif
#endif
};

struct PDPInfo s_PDP[MAX_PDP] = {
    {-1, -1, -1, false, PDP_IDLE, PTHREAD_MUTEX_INITIALIZER},
    {-1, -1, -1, false, PDP_IDLE, PTHREAD_MUTEX_INITIALIZER},
    {-1, -1, -1, false, PDP_IDLE, PTHREAD_MUTEX_INITIALIZER},
    {-1, -1, -1, false, PDP_IDLE, PTHREAD_MUTEX_INITIALIZER},
    {-1, -1, -1, false, PDP_IDLE, PTHREAD_MUTEX_INITIALIZER},
    {-1, -1, -1, false, PDP_IDLE, PTHREAD_MUTEX_INITIALIZER}
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
static void detachGPRS(int channelID, void *data, size_t datalen, RIL_Token t);
static bool isApnEqual(char *new, char *old);
static bool isProtocolEqual(char *new, char *old);
static int getMaxPDPNum(void) {
    return isLte() ? MAX_PDP : MAX_PDP / 2;
}

static int getPDP(RIL_SOCKET_ID socket_id) {
    int ret = -1;
    int i;
    char prop[PROPERTY_VALUE_MAX] = {0};
    int maxPDPNum = getMaxPDPNum();

    for (i = 0; i < maxPDPNum; i++) {
        if ((s_modemConfig == LWG_LWG || socket_id == s_multiModeSim) &&
            s_activePDN > 0 && s_PDN[i].nCid == (i + 1)) {
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

void putPDPByIndex(int index) {
    if (index < 0 || index >= MAX_PDP) {
        return;
    }
    pthread_mutex_lock(&s_PDP[index].mutex);
    if (s_PDP[index].state == PDP_BUSY) {
        s_PDP[index].state = PDP_IDLE;
    }
    pthread_mutex_unlock(&s_PDP[index].mutex);
}

void putUnusablePDPCid() {
    int i = 0;
    for (; i < MAX_PDP; i++) {
        pthread_mutex_lock(&s_PDP[i].mutex);
        if (s_PDP[i].cid == UNUSABLE_CID) {
            RLOGD("putUnusablePDPCid cid = %d", i + 1);
            s_PDP[i].cid = -1;
        }
        pthread_mutex_unlock(&s_PDP[i].mutex);
    }
}
int updatePDPSocketId(int cid, int socketId) {
    int index = cid - 1;
    if (cid <= 0 || cid > MAX_PDP) {
        return 0;
    }
    pthread_mutex_lock(&s_PDP[index].mutex);
    s_PDP[index].socketId = socketId;
    pthread_mutex_unlock(&s_PDP[index].mutex);
    return 1;
}

int getPDPSocketId(int index) {
    if (index >= MAX_PDP || index < 0) {
        return -1;
    } else {
        return s_PDP[index].socketId;
    }
}
int updatePDPCid(int cid, int state) {
    int index = cid - 1;
    if (cid <= 0 || cid > MAX_PDP) {
        return 0;
    }
    pthread_mutex_lock(&s_PDP[index].mutex);
    if (state == 1) {
        s_PDP[index].cid = cid;
    } else if (state == 0) {
        s_PDP[index].cid = -1;
    } else if (state == -1 && s_PDP[index].cid == -1) {
        s_PDP[index].cid = UNUSABLE_CID;
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

int getActivedCid() {
    int i = 0;
    for (; i < MAX_PDP; i++) {
        pthread_mutex_lock(&s_PDP[i].mutex);
        if (s_PDP[i].state == PDP_BUSY && s_PDP[i].cid != -1) {
            RLOGD("get actived cid = %d", s_PDP[i].cid);
            pthread_mutex_unlock(&s_PDP[i].mutex);
            return i;
        }
        pthread_mutex_unlock(&s_PDP[i].mutex);
    }
    return -1;
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
        case MN_GPRS_ERR_MAX_ACTIVE_PDP_REACHED:
            s_lastPDPFailCause[socket_id] = PDP_FAIL_MAX_ACTIVE_PDP_CONTEXT_REACHED;
            break;
        default:
            s_lastPDPFailCause[socket_id] = PDP_FAIL_ERROR_UNSPECIFIED;
            break;
    }
}

static bool doIPV4_IPV6_Fallback(int channelID, int index, void *data)
{
    ATResponse *p_response = NULL;
    char *line;
    int err = 0;
    int failCause = 0;
    const char *apn = NULL;
    const char *username = NULL;
    const char *password = NULL;
    const char *authtype = NULL;
    char cmd[128] = {0};
    int fb_ip_type = -1;
    char prop[PROPERTY_VALUE_MAX] = {0};
    char eth[PROPERTY_VALUE_MAX] = {0};
    char qosState[PROPERTY_VALUE_MAX] = {0};

    apn = ((const char **)data)[2];
    username = ((const char **)data)[3];
    password = ((const char **)data)[4];
    authtype = ((const char **)data)[5];

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    //IPV4
    snprintf(cmd, sizeof(cmd), "AT+CGACT=0,%d", index+1);
    at_send_command(s_ATChannels[channelID], cmd, NULL);

    snprintf(cmd, sizeof(cmd), "AT+CGDCONT=%d,\"IP\",\"%s\",\"\",0,0", index+1, apn);
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (err < 0 || p_response->success == 0){
        goto retryIPV6;
    }

    snprintf(cmd, sizeof(cmd), "AT+CGDATA=\"PPP\",%d", index + 1);
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (err < 0 || p_response->success == 0) {
        goto retryIPV6;
    }else{
        goto done;
    }

retryIPV6:

    snprintf(cmd, sizeof(cmd), "AT+CGACT=0,%d", index+1);
    at_send_command(s_ATChannels[channelID], cmd, NULL);

    snprintf(cmd, sizeof(cmd), "AT+CGDCONT=%d,\"IPV6\",\"%s\",\"\",0,0", index+1, apn);
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (err < 0 || p_response->success == 0){
        goto error;
    }

    snprintf(cmd, sizeof(cmd), "AT+CGPCO=0,\"%s\",\"%s\",%d,%d", username, password,index+1,atoi(authtype));
    at_send_command(s_ATChannels[channelID], cmd, NULL);

    /* Set required QoS params to default */
    property_get("persist.sys.qosstate", qosState, "0");
    if(!strcmp(qosState, "0")) {
        snprintf(cmd, sizeof(cmd), "AT+CGEQREQ=%d,0,0,0,0,0,2,0,\"0\",\"0e0\",3,0,0", index+1);
        at_send_command(s_ATChannels[channelID], cmd, NULL);
        snprintf(cmd, sizeof(cmd), "AT+CGQREQ=%d,0,0,0,0,0", index + 1);
        at_send_command(s_ATChannels[channelID], cmd, NULL);
    }

    snprintf(cmd, sizeof(cmd), "AT+CGDATA=\"PPP\",%d", index + 1);
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (err < 0 || p_response->success == 0) {
        RLOGD("both V4 and V6 are failed");
        goto error;
    }

done:
    updatePDPCid(index + 1, 1);
    updatePDPSocketId(index + 1, socket_id);
    AT_RESPONSE_FREE(p_response);
    return true;
error:
    AT_RESPONSE_FREE(p_response);
    return false;
}

/**
 * return -2: fail cause 253, need retry active for another cid;
 *         0: success;
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
                    if (failCause == 288 || failCause == 128) {
                        ret = DATA_ACTIVE_NEED_RETRY;
                    } else if (failCause == 253) {
                        ret = DATA_ACTIVE_NEED_RETRY_FOR_ANOTHER_CID;
                    } else if (failCause == 128 || failCause == 38) {// 128: network reject
                        ret = DATA_ACTIVE_NEED_FALLBACK;
                        if (failCause == 128){
                            s_lastPDPFailCause[socket_id] = PDP_FAIL_UNKNOWN_PDP_ADDRESS_TYPE;
                        }else if (failCause == 38){
                            s_lastPDPFailCause[socket_id] = PDP_FAIL_NETWORK_FAILURE;
                        }
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
    return cause;
}

static void queryAllActivePDNInfos(int channelID) {
    int err = 0;
    int n, skip, active;
    char *line;
    ATLine *pCur;
    PDNInfo *pdns = s_PDN;
    ATResponse *pdnResponse = NULL;

    s_activePDN = 0;
    err = at_send_command_multiline(s_ATChannels[channelID], "AT+SPIPCONTEXT?",
                                    "+SPIPCONTEXT:", &pdnResponse);
    if (err < 0 || pdnResponse->success == 0) goto done;
    for (pCur = pdnResponse->p_intermediates; pCur != NULL;
         pCur = pCur->p_next) {
        int cid;
        int type;
        char *apn;
        line = pCur->line;
        err = at_tok_start(&line);
        if (err < 0) {
            pdns->nCid = -1;
        }
        err = at_tok_nextint(&line, &pdns->nCid);
        if (err < 0) {
            pdns->nCid = -1;
        }
        cid = pdns->nCid;
        if (pdns->nCid > MAX_PDP) {
            continue;
        }
        err = at_tok_nextint(&line, &active);
        if (err < 0 || active == 0) {
            pdns->nCid = -1;
        }
        if (active == 1) {
            s_activePDN++;
        }
        /* apn */
        err = at_tok_nextstr(&line, &apn);
        if (err < 0) {
            s_PDN[cid - 1].nCid = -1;
        }
        snprintf(s_PDN[cid - 1].strApn, sizeof(s_PDN[cid - 1].strApn),
                 "%s", apn);
        /* type */
        err = at_tok_nextint(&line, &type);
        if (err < 0) {
            s_PDN[cid - 1].nCid = -1;
        }
        char *strType = NULL;
        switch (type) {
            case IPV4:
                strType = "IP";
                break;
            case IPV6:
                strType = "IPV6";
                break;
            case IPV4V6:
                strType = "IPV4V6";
                break;
            default:
                strType = "IP";
                break;
        }
        snprintf(s_PDN[cid - 1].strIPType, sizeof(s_PDN[cid - 1].strIPType),
                  "%s", strType);
        if (active > 0) {
            RLOGI("active PDN: cid = %d, iptype = %s, apn = %s",
                  s_PDN[cid - 1].nCid, s_PDN[cid - 1].strIPType,
                  s_PDN[cid - 1].strApn);
        }
        pdns++;
    }
done:
    at_response_free(pdnResponse);
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
        if (pdns->nCid > MAX_PDP) {
            continue;
        }
        err = at_tok_nextint(&line, &active);
        if (err < 0 || active == 0) {
            pdns->nCid = -1;
        }
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
            continue;
        }
        err = at_tok_nextint(&line, &cid);
        if ((err < 0) || (cid != s_PDN[cid-1].nCid) || cid > MAX_PDP) {
            continue;
        }

        /* type */
        err = at_tok_nextstr(&line, &type);
        if (err < 0) {
            s_PDN[cid-1].nCid = -1;
        }
        snprintf(s_PDN[cid-1].strIPType, sizeof(s_PDN[cid-1].strIPType),
                  "%s", type);
        /* apn */
        err = at_tok_nextstr(&line, &apn);
        if (err < 0) {
            s_PDN[cid-1].nCid = -1;
        }
        snprintf(s_PDN[cid-1].strApn, sizeof(s_PDN[cid-1].strApn), "%s", apn);
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

    snprintf(strApn, sizeof(strApn), "%s", apn);
    RLOGD("getOrgApnlen: apn = %s, strApn = %s, len = %d", apn, strApn, len);

    memset(tmp, 0, sizeof(tmp));
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

static bool isAttachEnable() {
    char prop[PROPERTY_VALUE_MAX] = {0};
    property_get(ATTACH_ENABLE_PROP, prop, "true");
    RLOGD("isAttachEnable: prop = %s", prop);
    if (!strcmp(prop, "false")) {
        return false;
    }
    return true;
}
/**
 * delete deactivateLteDataConnection method
 */

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
    int islte = s_isLTE;
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
    at_send_command(s_ATChannels[channelID], cmd, NULL);


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
                  "AT+CGEQREQ=%d,%d,0,0,0,0,2,0,\"1e4\",\"0e0\",3,0,0",
                  cid, s_trafficClass[socket_id]);
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
        if (ret == DATA_ACTIVE_NEED_FALLBACK) {
            return ret;
        }
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
            updatePDPSocketId(primaryCid, socket_id);
            ret = DATA_ACTIVE_SUCCESS;
        }
    } else {
        updatePDPCid(cid, 1);
        updatePDPSocketId(cid, socket_id);
        ret = DATA_ACTIVE_SUCCESS;
    }
    s_trafficClass[socket_id] = TRAFFIC_CLASS_DEFAULT;
    return ret;
}

/*
 * return  NULL :  Dont need fallback
 *         other:  FallBack s_PDP type
 */
static const char *checkNeedFallBack(int channelID, const char *pdp_type,
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

    char isDualpdpAllowed[PROPERTY_VALUE_MAX];
    memset(isDualpdpAllowed, 0, sizeof(isDualpdpAllowed));
    property_get(DUALPDP_ALLOWED_PROP, isDualpdpAllowed, "false");

    if (!strcmp(pdp_type, "IPV4V6") && ipType != IPV4V6) {
        fbCause = getSPACTFBcause(channelID);
        RLOGD("requestSetupDataCall fall Back Cause = %d", fbCause);
        if (fbCause == 52 &&
                ((strcmp(isDualpdpAllowed, "true")) || cidIndex == 0)) {
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
    bool islte = s_isLTE;
    ATLine *p_cur;
    ATResponse *p_response = NULL;
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

        /* APN ignored for v5 */
        err = at_tok_nextstr(&line, &out);
        if (err < 0) goto error;

        snprintf(ethPropName, sizeof(ethPropName), "ro.modem.%s.eth", s_modem);
        property_get(ethPropName, eth, "veth");

        snprintf(cmd, sizeof(cmd), "%s%d", eth, ncid - 1);
        responses[i].ifname = alloca(strlen(cmd) + 1);
        snprintf(responses[i].ifname, strlen(cmd) + 1, "%s", cmd);

        snprintf(cmd, sizeof(cmd), "net.%s%d.ip_type", eth, ncid - 1);
        property_get(cmd, prop, "0");
        ipType = atoi(prop);
        if (responses[i].active > 0) {
            RLOGD("prop = %s, ipType = %d", prop, ipType);
        }
        dnslist = alloca(DNSListSize);

        if (ipType == IPV4) {
            responses[i].type = alloca(strlen("IP") + 1);
            strncpy(responses[i].type, "IP", sizeof("IP"));
            snprintf(cmd, sizeof(cmd), "net.%s%d.ip", eth, ncid - 1);
            property_get(cmd, prop, NULL);
            RLOGD("IPV4 cmd=%s, prop = %s", cmd, prop);
            responses[i].addresses = alloca(strlen(prop) + 1);
            responses[i].gateways = alloca(strlen(prop) + 1);
            snprintf(responses[i].addresses, strlen(prop) + 1, "%s", prop);
            snprintf(responses[i].gateways, strlen(prop) + 1, "%s", prop);

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
            responses[i].type = alloca(strlen("IPV6") + 1);
            strncpy(responses[i].type, "IPV6", sizeof("IPV6"));
            snprintf(cmd, sizeof(cmd), "net.%s%d.ipv6_ip", eth, ncid - 1);
            property_get(cmd, prop, NULL);
            RLOGD("IPV6 cmd=%s, prop = %s", cmd, prop);
            responses[i].addresses = alloca(strlen(prop) + 1);
            responses[i].gateways = alloca(strlen(prop) + 1);
            snprintf(responses[i].addresses, strlen(prop) + 1, "%s", prop);
            snprintf(responses[i].gateways, strlen(prop) + 1, "%s", prop);

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
            strncpy(responses[i].type, "IPV4V6", sizeof("IPV4V6") );
            iplist = alloca(IPListSize);
            separator = " ";
            iplist[0] = 0;
            snprintf(cmd, sizeof(cmd), "net.%s%d.ip", eth, ncid - 1);
            property_get(cmd, prop, NULL);
            strlcat(iplist, prop, IPListSize);
            strlcat(iplist, separator, IPListSize);
            RLOGD("IPV4V6 cmd = %s, prop = %s, iplist = %s", cmd, prop, iplist);

            snprintf(cmd, sizeof(cmd), "net.%s%d.ipv6_ip", eth, ncid - 1);
            property_get(cmd, prop, NULL);
            strlcat(iplist, prop, IPListSize);
            responses[i].addresses = iplist;
            responses[i].gateways = iplist;
            RLOGD("IPV4V6 cmd = %s, prop = %s, iplist = %s", cmd, prop, iplist);
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
                RLOGD("IPV4V6 cmd = %s, prop = %s, dnslist = %s", cmd, prop,
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
                RLOGD("IPV4V6 cmd=%s, prop = %s,dnslist = %s", cmd, prop,
                      dnslist);
            }
            responses[i].dnses = dnslist;
        } else {
            if (responses[i].active > 0) {
                RLOGE("Unknown IP type!");
            }
        }
        //responses[i].mtu = DEFAULT_MTU;
        if ((cid != -1) && (t == NULL)) {
             if (responses[i].active > 0) {
                 RLOGE("i = %d", i);
             }
             if ((!responses[i].active) &&
                 strcmp(responses[i].addresses, "")) {
                 responses[i].status = PDP_FAIL_OPERATOR_BARRED;
                 RLOGE("responses[i].addresses = %s", responses[i].addresses);
             }
          }
        if (responses[i].active > 0) {
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
    }

    AT_RESPONSE_FREE(p_response);

    if ((t != NULL) && (cid > 0)) {
        RLOGD("requestOrSendDataCallList is called by SetupDataCall!");
        for (i = 0; i < MAX_PDP; i++) {
            if (responses[i].cid == cid) {
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
                        /* send IP for volte addtional business */
                        if (islte && (s_modemConfig == LWG_LWG ||
                                      socket_id == s_multiModeSim)) {
                            char cmd[AT_COMMAND_LEN] = {0};
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
                        RIL_onRequestComplete(*t, RIL_E_SUCCESS, &responses[i],
                                sizeof(RIL_Data_Call_Response_v11));
                    }
                } else {
                    putPDP(getFallbackCid(cid - 1) - 1);
                    putPDP(cid - 1);
                    s_lastPDPFailCause[socket_id] = PDP_FAIL_ERROR_UNSPECIFIED;
                    RIL_onRequestComplete(*t, RIL_E_GENERIC_FAILURE, NULL, 0);
                }
                return;
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
                               const char *type, RIL_Token t) {
    bool islte = s_isLTE;
    int err, ret = -1, cgdata_err;
    char strApnName[ARRAY_SIZE] = {0};
    char cmd[AT_COMMAND_LEN] = {0};
    char prop[PROPERTY_VALUE_MAX] = {0};
    ATResponse *p_response = NULL;
    int useDefaultPDN;

    property_get(REUSE_DEFAULT_PDN, prop, "0");
    useDefaultPDN = atoi(prop);
    RLOGD("useDefaultPDN = %d",useDefaultPDN);

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    if (islte && (s_modemConfig == LWG_LWG ||
                  socket_id == s_multiModeSim)) {
        queryAllActivePDNInfos(channelID);
        int i, cid;
        if (s_activePDN > 0) {
            for (i = 0; i < MAX_PDP; i++) {
                cid = getPDNCid(i);
                if (cid == (i + 1)) {
                    RLOGD("s_PDP[%d].state = %d", i, getPDPState(i));
                    if (i < MAX_PDP && (getPDPState(i) == PDP_IDLE) &&
                        (useDefaultPDN ||
                        (isApnEqual((char *)apn, getPDNAPN(i)) &&
                        isProtocolEqual((char *)type, getPDNIPType(i))) ||
                        s_singlePDNAllowed[socket_id] == 1)) {
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
                        cgdata_err = errorHandlingForCGDATA(channelID, p_response, err,
                                cid);
                        if (cgdata_err == 0) {  // success
                            updatePDPCid(i + 1, 1);
                            updatePDPSocketId(cid, socket_id);
                            requestOrSendDataCallList(channelID, cid, &t);
                            ret = 0;
                            at_response_free(p_response);
                        } else if (cgdata_err > 0) {  // no retry
                            ret = cid;
                            at_response_free(p_response);
                        } else {  // need retry active for another cid
                            updatePDPCid(i + 1, -1);
                            putPDPByIndex(i);
                            AT_RESPONSE_FREE(p_response);
                        }
                    }
                } /*else if (i < MAX_PDP) {//for bug704303
                    putPDP(i);
                }
*/            }
        } /*else {
            for (i = 0; i < MAX_PDP; i++) {
                putPDP(i);
            }
        }*/
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

    apn = ((const char **)data)[2];

    if (isVoLteEnable() && !isExistActivePdp()) {  // for ddr, power consumption
        char prop[PROPERTY_VALUE_MAX];
        property_get(DDR_STATUS_PROP, prop, "0");
        RLOGD("volte ddr power prop = %s", prop);
        if (!strcmp(prop, "1")) {
            at_send_command(s_ATChannels[channelID], "AT+SPVOOLTE=0", NULL);
        }
    }
RETRY:

    if (datalen > 6 * sizeof(char *)) {
        pdpType = ((const char **) data)[6];
    } else {
        pdpType = "IP";
    }
    s_LTEDetached[socket_id] = false;
    /* check if reuse default bearer or not */
    ret = reuseDefaultBearer(channelID, apn, pdpType, t);
    if (ret == 0) {
        return;
    } else if (ret > 0) {
        primaryindex = ret - 1;
        goto error;
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
    } else if (ret == DATA_ACTIVE_NEED_FALLBACK) {
        if (doIPV4_IPV6_Fallback(channelID, index, data) == false) {
            goto error;
        } else {
            goto done;
        }
    }  else if (ret == DATA_ACTIVE_NEED_RETRY_FOR_ANOTHER_CID) {
        updatePDPCid(index + 1, -1);
        goto RETRY;
    } else if (ret == DATA_ACTIVE_SUCCESS &&
            (!strcmp(pdpType, "IPV4V6") || !strcmp(pdpType, "IPV4+IPV6")) && s_isLTE ) {
        const char *tmpType = NULL;
        /* Check if need fall back or not */
        if (!strcmp(pdpType, "IPV4+IPV6")) {
            tmpType = "IPV6";
        } else if (!strcmp(pdpType, "IPV4V6")) {
            tmpType = checkNeedFallBack(channelID, pdpType, index);
            isFallback = 1;
        }
        if (tmpType == NULL) {  // don't need fallback
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
    putUnusablePDPCid();
    requestOrSendDataCallList(channelID, primaryindex + 1, &t);
    return;

error:
    if (primaryindex >= 0) {
        putPDP(getFallbackCid(primaryindex) - 1);
        putPDP(primaryindex);
    }
    putUnusablePDPCid();
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

static void updateAdditionBusinessCid(int channelID) {
    char cmd[ARRAY_SIZE] = {0};
    char ethPropName[ARRAY_SIZE] = {0};
    char prop[PROPERTY_VALUE_MAX] = {0};
    char eth[PROPERTY_VALUE_MAX] = {0};
    char ipv4[PROPERTY_VALUE_MAX] = {0};
    char ipv6[PROPERTY_VALUE_MAX] = {0};
    int ipType = 0;
    int cidIndex = getActivedCid();

    if (cidIndex < 0 || cidIndex > MAX_PDP) {
        RLOGD("No actived cid");
        return;
    }

    snprintf(ethPropName, sizeof(ethPropName), "ro.modem.%s.eth", s_modem);
    property_get(ethPropName, eth, "veth");
    snprintf(prop, sizeof(prop), "net.%s%d.ip_type", eth, cidIndex);
    property_get(prop, cmd, "0");
    ipType = atoi(cmd);

    if (ipType & IPV4) {
        snprintf(prop, sizeof(prop), "net.%s%d.ip", eth, cidIndex);
        property_get(prop, ipv4, "");
    } else {
        strncpy(ipv4, "0.0.0.0", sizeof("0.0.0.0"));
    }

    if (ipType & IPV6) {
        snprintf(prop, sizeof(prop), "net.%s%d.ipv6_ip", eth, cidIndex);
        property_get(prop, ipv6, "");
    } else {
        strncpy(ipv6, "FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF",
                sizeof("FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF"));
    }

    snprintf(cmd, sizeof(cmd), "AT+XCAPIP=%d,\"%s,[%s]\"", cidIndex + 1, ipv4,
              ipv6);
    RLOGD("Addition business cmd = %s", cmd);
    at_send_command(s_ATChannels[channelID], cmd, NULL);
    s_addedIPCid = cidIndex + 1;
}

static void deactivateDataConnection(int channelID, void *data,
                                          size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    bool islte = s_isLTE;
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
    snprintf(cmd, sizeof(cmd), "AT+CGACT=0,%d", cid);
    at_send_command(s_ATChannels[channelID], cmd, &p_response);
    AT_RESPONSE_FREE(p_response);
    updatePDPSocketId(cid, -1);
    if (secondaryCid != -1) {
        RLOGD("dual PDP, do CGACT again, fallback cid = %d", secondaryCid);
        snprintf(cmd, sizeof(cmd), "AT+CGACT=0,%d", secondaryCid);
        at_send_command(s_ATChannels[channelID], cmd, &p_response);
    }

done:
    putPDP(secondaryCid - 1);
    putPDP(cid - 1);
    at_response_free(p_response);
    if (islte && cid == s_addedIPCid) {
        updateAdditionBusinessCid(channelID);
    }
    // for ddr, power consumption
    if (isVoLteEnable() && !isExistActivePdp()) {
        property_get(DDR_STATUS_PROP, prop, "0");
        RLOGD("volte ddr power prop = %s", prop);
        if (!strcmp(prop, "1")) {
            at_send_command(s_ATChannels[channelID], "AT+SPVOOLTE=1", NULL);
        }
    }
error:
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    return;
}

static bool isStrEmpty(char *str) {
    if (NULL == str || strcmp(str, "") == 0) {
        return true;
    }
    return false;
}

static int getPco(int channelID, RIL_InitialAttachApn *response,
                  int cid) {
    ATResponse *pdnResponse = NULL;
    char *line;
    int skip;
    int err = -1;
    ATLine *pCur;
    int curr_cid =0;
    char *username = NULL;
    char *password = NULL;
    err = at_send_command_multiline(s_ATChannels[channelID], "AT+CGPCO?",
                                    "+CGPCO:", &pdnResponse);
    if (err < 0 || pdnResponse->success == 0) goto done;
    for (pCur = pdnResponse->p_intermediates; pCur != NULL;
         pCur = pCur->p_next) {
        line = pCur->line;
        err = at_tok_start(&line);
        if (err < 0) {
            goto done;
        }
        err = at_tok_nextint(&line, &skip);
        if (err < 0) {
            goto done;
        }
        err = at_tok_nextstr(&line, &username);
        if (err < 0) {
            goto done;
        }
        err = at_tok_nextstr(&line, &password);
        if (err < 0) {
            goto done;
        }
        err = at_tok_nextint(&line, &curr_cid);
        if (err < 0) {
            goto done;
        }
        if (curr_cid != cid) {
            continue;
        }
        snprintf(response->username, ARRAY_SIZE,
                 "%s", username);
        snprintf(response->password, ARRAY_SIZE,
                 "%s", password);
        err = at_tok_nextint(&line, &response->authtype);
    }
done:
    at_response_free(pdnResponse);
    return err;
}

static int getDataProfile(RIL_InitialAttachApn *response,
                          int channelID, int cid) {
    int ret = -1;
    if (cid < 1) {
        return ret;
    }
    queryAllActivePDNInfos(channelID);
    if (s_PDN[cid - 1].nCid == cid) {
        snprintf(response->apn, ARRAY_SIZE,
                 "%s", s_PDN[cid - 1].strApn);
        snprintf(response->protocol, 16,
                 "%s", s_PDN[cid - 1].strIPType);
        ret = getPco(channelID, response, cid);
    }
    return ret;
}

static bool isStrEqual(char *new, char *old) {
    bool ret = false;
    if (isStrEmpty(old) && isStrEmpty(new)) {
        ret = true;
    } else if (!isStrEmpty(old) && !isStrEmpty(new)) {
        if (strcasecmp(old, new) == 0) {
            ret = true;
        } else {
            RLOGD("isStrEqual old=%s, new=%s", old, new);
        }
    } else {
        RLOGD("isStrEqual old or new is empty!");
    }
    return ret;
}

static bool isApnEqual(char *new, char *old) {
    char strApnName[ARRAY_SIZE] = {0};
    strncpy(strApnName, old, checkCmpAnchor(old));
    strApnName[strlen(strApnName)] = '\0';
    if (isStrEmpty(new) || isStrEqual(new, old) ||
        isStrEqual(strApnName, new)) {
        return true;
    }
    return false;
}

static bool isProtocolEqual(char *new, char *old) {
    bool ret = false;
    if (strcasecmp(new, "IPV4V6") == 0 ||
        strcasecmp(new, old) == 0) {
        ret = true;
    }
    return ret;
}

static int compareApnProfile(RIL_InitialAttachApn *new,
                             RIL_InitialAttachApn *old) {
    int ret = -1;
    int AUTH_NONE = 0;
    if (isStrEmpty(new->username) || isStrEmpty(new->password) ||
        new->authtype <= 0) {
        new->authtype = AUTH_NONE;
        if (new->username != NULL) {
             memset(new->username, 0, strlen(new->username));
        }
        if (new->password != NULL) {
             memset(new->password, 0, strlen(new->password));
        }
    }
    if (isStrEmpty(old->username) || isStrEmpty(old->password) ||
        old->authtype <= 0) {
        RLOGD("old profile is empty");
        old->authtype = AUTH_NONE;
        if (old->username != NULL) {
             memset(old->username, 0, strlen(old->username));
        }
        if (old->password != NULL) {
            memset(old->password, 0, strlen(old->password));
        }
    }
    if ((isStrEmpty(new->apn) && isStrEmpty(new->protocol)) ||
            (isApnEqual(new->apn, old->apn) &&
        isStrEqual(new->protocol, old->protocol) &&
        isStrEqual(new->username, old->username) &&
        isStrEqual(new->password, old->password) &&
        new->authtype == old->authtype)) {
        ret = 1;
    }
    return ret;
}

static void setDataProfile(RIL_InitialAttachApn *new, int cid,
                             int channelID, int socket_id) {
    char qosState[PROPERTY_VALUE_MAX] = {0};
    char cmd[AT_COMMAND_LEN] = {0};
    snprintf(cmd, sizeof(cmd), "AT+CGDCONT=%d,\"%s\",\"%s\",\"\",0,0",
             cid, new->protocol, new->apn);
    at_send_command(s_ATChannels[channelID], cmd, NULL);

    snprintf(cmd, sizeof(cmd), "AT+CGPCO=0,\"%s\",\"%s\",%d,%d",
             new->username, new->password, cid, new->authtype);
    at_send_command(s_ATChannels[channelID], cmd, NULL);

    /* Set required QoS params to default */
    property_get("persist.sys.qosstate", qosState, "0");
    if (!strcmp(qosState, "0")) {
        snprintf(cmd, sizeof(cmd),
                  "AT+CGEQREQ=%d,%d,0,0,0,0,2,0,\"1e4\",\"0e0\",3,0,0",
                  cid, s_trafficClass[socket_id]);
        at_send_command(s_ATChannels[channelID], cmd, NULL);
    }
}

/**
 * as RIL_REQUEST_ALLOW_DATA is necessary before active data connection,
 * we can get data connection card id by s_dataAllowed
 */
int getDefaultDataCardId() {
    int i = 0;
    int ret = -1;
    for (i = 0; i < SIM_COUNT; i++) {
        if (s_dataAllowed[i] == 1) {
            ret = i;
        }
    }
    return ret;
}

static void requestSetInitialAttachAPN(int channelID, void *data,
                                            size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(datalen);
    int initialAttachId = 1;
    int ret = -1;
    bool isSetReattach = false;
    char prop[PROPERTY_VALUE_MAX] = {0};

    RIL_InitialAttachApn *response =
            (RIL_InitialAttachApn *)calloc(1, sizeof(RIL_InitialAttachApn));
    response->apn = (char *)calloc(ARRAY_SIZE, sizeof(char));
    response->protocol = (char *)calloc(16, sizeof(char));
    response->username = (char *)calloc(ARRAY_SIZE, sizeof(char));
    response->password = (char *)calloc(ARRAY_SIZE, sizeof(char));
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    if (data != NULL) {
        RIL_InitialAttachApn *pIAApn = (RIL_InitialAttachApn *)data;
        if (s_isLTE) {
            ret = getDataProfile(response, channelID, initialAttachId);
            ret = compareApnProfile(pIAApn, response);
            if (ret > 0) {
                goto done;
            } else {
                setDataProfile(pIAApn, initialAttachId, channelID, socket_id);
            }
            RLOGD("get_data_profile s_PSRegStateDetail=%d, s_in4G=%d",
                   s_PSRegStateDetail[socket_id], s_in4G[socket_id]);
            /*bug769723 CMCC version reattach on data card*/
            property_get("ro.radio.spice", prop, "0");
            if (!strcmp(prop, "1")) {
                if (socket_id == getDefaultDataCardId()) {
                    isSetReattach = true;
                }
            } else {
                isSetReattach = socket_id == s_multiModeSim;
            }

            getProperty(socket_id, "gsm.sim.operator.numeric", prop, "");
            RLOGD("prop = %s", prop);
            if (isSetReattach && ((s_in4G[socket_id] == 1 ||
                s_PSRegStateDetail[socket_id] == RIL_REG_STATE_NOT_REG ||
                s_PSRegStateDetail[socket_id] == RIL_REG_STATE_ROAMING ||
                s_PSRegStateDetail[socket_id] == RIL_REG_STATE_SEARCHING ||
                s_PSRegStateDetail[socket_id] == RIL_REG_STATE_UNKNOWN ||
                s_PSRegStateDetail[socket_id] == RIL_REG_STATE_DENIED) ||
                (strcmp(prop, "732101") == 0))) {
                at_send_command(s_ATChannels[channelID], "AT+SPREATTACH", NULL);
            }
        } else {
            setDataProfile(pIAApn, initialAttachId, channelID, socket_id);
        }
    }
done:
    free(response->apn);
    free(response->protocol);
    free(response->username);
    free(response->password);
    free(response);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    s_trafficClass[socket_id] = TRAFFIC_CLASS_DEFAULT;
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

static void doDetachGPRS(RIL_SOCKET_ID socket_id, void *data, size_t datalen,
                         RIL_Token t) {
    int detachChannelID = getChannel(socket_id);
    RLOGD("doDetachGPRS socket_id = %d", socket_id);
    detachGPRS(detachChannelID, data, datalen, t);
    putChannel(detachChannelID);
}

static void attachGPRS(int channelID, void *data, size_t datalen,
                          RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int err;
    char cmd[AT_COMMAND_LEN];
    bool islte = s_isLTE;
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

#if (SIM_COUNT == 2)
    if (s_autoDetach == 1) {
        doDetachGPRS(1 - socket_id, data, datalen, NULL);
    }
#endif

    if (islte) {
        if (s_modemConfig == LWG_LWG) {
            snprintf(cmd, sizeof(cmd), "AT+SPSWITCHDATACARD");
            at_send_command(s_ATChannels[channelID], cmd, NULL);
        } else {
            if (s_sessionId[socket_id] != 0) {
                RLOGD("setRadioCapability is on going during attach, return!!");
                goto error;
            }
            if (socket_id != s_multiModeSim ) {
                snprintf(cmd, sizeof(cmd), "AT+SPSWITCHDATACARD=%d,1",
                         socket_id);
                at_send_command(s_ATChannels[channelID], cmd, NULL);
                err = at_send_command(s_ATChannels[channelID], "AT+CGATT=1",
                                       &p_response);
                if (err < 0 || p_response->success == 0) {
                    at_send_command(s_ATChannels[channelID], "AT+SGFD", NULL);
                    goto error;
                }
            } else {
                snprintf(cmd, sizeof(cmd), "AT+SPSWITCHDATACARD=%d,0",
                         1 - socket_id);
                at_send_command(s_ATChannels[channelID], cmd, NULL);
            }
        }
    } else {
        if (s_sessionId[socket_id] != 0) {
            RLOGD("setRadioCapability is on going, return!!");
            goto error;
        }
        err = at_send_command(s_ATChannels[channelID], "AT+CGATT=1",
                              &p_response);
        if (err < 0 || p_response->success == 0) {
            goto error;
        }
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);
    return;

error:
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    return;
}

static void detachGPRS(int channelID, void *data, size_t datalen,
                          RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int ret;
    int err, i;
    int cid;
    char cmd[AT_COMMAND_LEN];
    bool islte = s_isLTE;
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    if (islte) {
        for (i = 0; i < MAX_PDP; i++) {
            cid = getPDPCid(i);
            if (cid > 0 && getPDPSocketId(i) == socket_id) {
                snprintf(cmd, sizeof(cmd), "AT+CGACT=0,%d", cid);
                at_send_command(s_ATChannels[channelID], cmd, &p_response);
                updatePDPSocketId(cid, -1);
                RLOGD("s_PDP[%d].state = %d", i, getPDPState(i));
                if (s_PDP[i].state == PDP_BUSY) {
                    putPDP(i);
                    requestOrSendDataCallList(channelID, cid, NULL);
                }
                AT_RESPONSE_FREE(p_response);
            }
        }
#if defined (ANDROID_MULTI_SIM)
        if (s_presentSIMCount == SIM_COUNT) {
            RLOGD("simID = %d", socket_id);
            if (s_modemConfig == LWG_LWG) {
                // ap do nothing when detach on L+L version
            } else {
                if (s_sessionId[socket_id] != 0) {
                    RLOGD("setRadioCapability is on going during detach, return!!");
                    goto error;
                }
                if (socket_id != s_multiModeSim) {
                    err = at_send_command(s_ATChannels[channelID], "AT+SGFD",
                                          &p_response);
                    if (err < 0 || p_response->success == 0) {
                        goto error;
                    }
                    snprintf(cmd, sizeof(cmd), "AT+SPSWITCHDATACARD=%d,0",
                             socket_id);
                } else {
                    snprintf(cmd, sizeof(cmd), "AT+SPSWITCHDATACARD=%d,1",
                             1 - socket_id);
                }
                err = at_send_command(s_ATChannels[channelID], cmd, NULL);
            }
        }
#endif
    } else {
        err = at_send_command(s_ATChannels[channelID], "AT+SGFD", &p_response);
        if (err < 0 || p_response->success == 0) {
            goto error;
        }
    }

    if (t != NULL) {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    }
    at_response_free(p_response);
    return;

error:
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    return;
}

void requestAllowData(int channelID, void *data, size_t datalen,
                         RIL_Token t) {
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
    s_dataAllowed[socket_id] = ((int *)data)[0];
    RLOGD("s_desiredRadioState[%d] = %d, s_autoDetach = %d", socket_id,
          s_desiredRadioState[socket_id], s_autoDetach);
    if (s_desiredRadioState[socket_id] > 0 && isAttachEnable()) {
        if (s_dataAllowed[socket_id]) {
            attachGPRS(channelID, data, datalen, t);
        } else {
            if (s_autoDetach == 1) {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            } else {
                detachGPRS(channelID, data, datalen, t);
            }
        }
    } else {
        if (s_dataAllowed[socket_id] == 0) {
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        } else {
            RLOGD("Failed allow data due to radiooff or engineer mode disable");
            RIL_onRequestComplete(t, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
        }
    }
}

int processDataRequest(int request, void *data, size_t datalen, RIL_Token t,
                          int channelID) {
    int ret = 1;
    int err;
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
    switch (request) {
        case RIL_REQUEST_SETUP_DATA_CALL: {
            if (s_manualSearchNetworkId >= 0) {
                RLOGD("s_manualSearchNetworkId = %d",s_manualSearchNetworkId);
                s_lastPDPFailCause[socket_id] = PDP_FAIL_ERROR_UNSPECIFIED;
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
                break;
            }
            if (s_desiredRadioState[socket_id] > 0 && isAttachEnable()) {
                if (s_isLTE) {
                    RLOGD("SETUP_DATA_CALL s_PSRegState[%d] = %d", socket_id,
                          s_PSRegState[socket_id]);
                    if (s_PSRegState[socket_id] == STATE_IN_SERVICE) {
                        requestSetupDataCall(channelID, data, datalen, t);
                    } else {
                        if (s_modemConfig != LWG_LWG &&
                                s_multiModeSim != socket_id && s_dataAllowed[socket_id] == 1) {
                            requestSetupDataCall(channelID, data, datalen, t);
                        } else {
                            s_lastPDPFailCause[socket_id] =
                                    PDP_FAIL_SERVICE_OPTION_NOT_SUPPORTED;
                            RIL_onRequestComplete(t,
                                    RIL_E_GENERIC_FAILURE, NULL, 0);
                        }
                    }
                } else {
                    requestSetupDataCall(channelID, data, datalen, t);
                }
            } else {
                RLOGD("SETUP_DATA_CALL attach not enable by engineer mode");
                s_lastPDPFailCause[socket_id] = PDP_FAIL_ERROR_UNSPECIFIED;
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
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
        case RIL_REQUEST_ALLOW_DATA: {
            if (s_manualSearchNetworkId >= 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
                break;
            }
            requestAllowData(channelID, data, datalen, t);
            break;
        }
        case RIL_REQUEST_SET_INITIAL_ATTACH_APN:
            requestSetInitialAttachAPN(channelID, data, datalen, t);
            break;
        /* IMS request @{ */
        case RIL_REQUEST_SET_IMS_INITIAL_ATTACH_APN: {
            char cmd[AT_COMMAND_LEN] = {0};
            char qosState[PROPERTY_VALUE_MAX] = {0};
            int initialAttachId = 11;  // use index of 11
            RIL_InitialAttachApn *initialAttachIMSApn = NULL;
            p_response = NULL;
            if (data != NULL) {
                initialAttachIMSApn = (RIL_InitialAttachApn *)data;

                snprintf(cmd, sizeof(cmd),
                        "AT+CGDCONT=%d,\"%s\",\"%s\",\"\",0,0",
                         initialAttachId, initialAttachIMSApn->protocol,
                         initialAttachIMSApn->apn);
                err = at_send_command(s_ATChannels[channelID],
                        cmd, &p_response);

                snprintf(cmd, sizeof(cmd), "AT+CGPCO=0,\"%s\",\"%s\",%d,%d",
                          initialAttachIMSApn->username,
                          initialAttachIMSApn->password,
                          initialAttachId, initialAttachIMSApn->authtype);
                err = at_send_command(s_ATChannels[channelID], cmd, NULL);

                /* Set required QoS params to default */
                property_get("persist.sys.qosstate", qosState, "0");
                if (!strcmp(qosState, "0")) {
                    snprintf(cmd, sizeof(cmd),
                        "AT+CGEQREQ=%d,%d,0,0,0,0,2,0,\"1e4\",\"0e0\",3,0,0",
                        initialAttachId, s_trafficClass[socket_id]);
                    err = at_send_command(s_ATChannels[channelID], cmd, NULL);
                }
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
                at_response_free(p_response);
            } else {
                RLOGD("INITIAL_ATTACH_IMS_APN data is null");
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            }
            break;
        }
        /* }@ */
        case RIL_EXT_REQUEST_TRAFFIC_CLASS: {
            s_trafficClass[socket_id] = ((int *)data)[0];
            if (s_trafficClass[socket_id] < 0) {
                s_trafficClass[socket_id] = TRAFFIC_CLASS_DEFAULT;
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            break;
        }
        /* For data clear code @{ */
        case RIL_EXT_REQUEST_ENABLE_LTE: {
            int err;
            int value = ((int *)data)[0];
            char cmd[AT_COMMAND_LEN];
            p_response = NULL;

            snprintf(cmd, sizeof(cmd), "AT+SPEUTRAN=%d", value);
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_EXT_REQUEST_ATTACH_DATA: {
            int err;
            int value = ((int *)data)[0];
            char cmd[AT_COMMAND_LEN];
            p_response = NULL;

            snprintf(cmd, sizeof(cmd), "AT+CGATT=%d", value);
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_EXT_REQUEST_FORCE_DETACH: {
            int err;
            p_response = NULL;
            err = at_send_command(s_ATChannels[channelID], "AT+CLSSPDT=1",
                                  &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_EXT_REQUEST_ENABLE_RAU_NOTIFY: {
            // set RAU SUCCESS report to AP
            at_send_command(s_ATChannels[channelID], "AT+SPREPORTRAU=1", NULL);
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;
        }
        case RIL_EXT_REQUEST_SET_COLP: {
            char cmd[AT_COMMAND_LEN];
            snprintf(cmd, sizeof(cmd), "AT+COLP=%d", ((int *)data)[0]);
            at_send_command(s_ATChannels[channelID], cmd, NULL);
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;
        }
        /* }@ */
        case RIL_EXT_REQUEST_SET_SINGLE_PDN: {
            s_singlePDNAllowed[socket_id] = ((int *)data)[0];
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;
        }
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

static void startGSPS(void *param) {
    int channelID;
    int err;
    char cmd[AT_COMMAND_LEN];
    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);

    RLOGD("startGSPS cid  %d, eth state: %d", s_GSCid, s_ethOnOff);
    channelID = getChannel(socket_id);
    if (s_ethOnOff) {
        property_set(GSPS_ETH_UP_PROP, "1");
        snprintf(cmd, sizeof(cmd), "AT+CGDATA=\"M-ETHER\", %d", s_GSCid);
    } else {
        property_set(GSPS_ETH_DOWN_PROP, "1");
        snprintf(cmd, sizeof(cmd), "AT+CGACT=0, %d", s_GSCid);
    }

    err = at_send_command(s_ATChannels[channelID], cmd, NULL);
    putChannel(channelID);
}

static void queryVideoCid(void *param) {
    int channelID;
    int err = -1;
    int commas = 0, i;
    int cid;
    int *response = NULL;
    char cmd[32] = {0};
    char *line = NULL, *p = NULL;
    ATResponse *p_response = NULL;
    CallbackPara *cbPara = (CallbackPara *)param;
    RIL_SOCKET_ID socket_id = cbPara->socket_id;

    channelID = getChannel(socket_id);
    cid = *((int *)(cbPara->para));

    FREEMEMORY(cbPara->para);
    FREEMEMORY(cbPara);

    snprintf(cmd, sizeof(cmd), "AT+CGEQOSRDP=%d", cid);
    err = at_send_command_singleline(s_ATChannels[channelID], cmd,
                                     "+CGEQOSRDP:", &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0) goto error;

    for (p = line ; *p != '\0'; p++) {
        if (*p == ',') {
            commas++;
        }
    }

    response = (int *)malloc((commas + 1) * sizeof(int));

    /**
     * +CGEQOSRDP: <cid>,<QCI>,<DL_GBR>,<UL_GBR>,<DL_MBR>,
     * <UL_MBR>,<DL_AMBR>,<UL_AMBR>
     */
    for (i = 0; i <= commas; i++) {
        err = at_tok_nextint(&line, &response[i]);
        if (err < 0)  goto error;
    }

    if (commas >= 1) {
        RLOGD("queryVideoCid param = %d, commas = %d, cid = %d, QCI = %d", cid,
              commas, response[0], response[1]);
        if (response[1] == 2) {
            RLOGD("QCI is 2, %d is video cid", response[0]);
            RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_VIDEO_QUALITY,
                    response, (commas + 1) * sizeof(int), socket_id);
        }
    }

error:
    putChannel(channelID);
    at_response_free(p_response);
    free(response);
}

int processDataUnsolicited(RIL_SOCKET_ID socket_id, const char *s) {
    int err;
    int ret = 1;
    char *line = NULL;

    if (strStartsWith(s, "+CGEV:")) {
        char *tmp;
        char *pCommaNext = NULL;
        static int activeCid = -1;
        int pdpState = 1;
        int cid = -1;
        int networkChangeReason = -1;
        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);
        if (strstr(tmp, "NW PDN ACT")) {
            tmp += strlen(" NW PDN ACT ");
        } else if (strstr(tmp, "NW ACT ")) {
            tmp += strlen(" NW ACT ");
            for (pCommaNext = tmp; *pCommaNext != '\0'; pCommaNext++) {
                if (*pCommaNext == ',') {
                    pCommaNext += 1;
                    break;
                }
            }
            activeCid = atoi(pCommaNext);
            RLOGD("activeCid = %d, networkChangeReason = %d", activeCid,
                  networkChangeReason);
            CallbackPara *cbPara =
                    (CallbackPara *)malloc(sizeof(CallbackPara));
            if (cbPara != NULL) {
                cbPara->para = (int *)malloc(sizeof(int));
                *((int *)(cbPara->para)) = activeCid;
                cbPara->socket_id = socket_id;
                RIL_requestTimedCallback(queryVideoCid, cbPara, NULL);
            }
        } else if (strstr(tmp, "NW PDN DEACT")) {
            tmp += strlen(" NW PDN DEACT ");
            pdpState = 0;
        } else if (strstr(tmp, " NW MODIFY ")) {
            tmp += strlen(" NW MODIFY ");
            activeCid = atoi(tmp);
            for (pCommaNext = tmp; *pCommaNext != '\0'; pCommaNext++) {
                if (*pCommaNext == ',') {
                    pCommaNext += 1;
                    break;
                }
            }
            networkChangeReason = atoi(pCommaNext);
            RLOGD("activeCid = %d, networkChangeReason = %d", activeCid,
                  networkChangeReason);
            if (networkChangeReason == 2 || networkChangeReason == 3) {
                CallbackPara *cbPara =
                        (CallbackPara *)malloc(sizeof(CallbackPara));
                if (cbPara != NULL) {
                    cbPara->para = (int *)malloc(sizeof(int));
                    *((int *)(cbPara->para)) = activeCid;
                    cbPara->socket_id = socket_id;
                    RIL_requestTimedCallback(queryVideoCid, cbPara, NULL);
                }
            }
            goto out;
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
                }
                RIL_onUnsolicitedResponse(
                        RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED, NULL, 0,
                        socket_id);
            }
        }
    } else if (strStartsWith(s, "+SPGS:")) {
        char *tmp;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextint(&tmp, &s_GSCid);
        if (err < 0) goto out;

        err = at_tok_nextint(&tmp, &s_ethOnOff);
        if (err < 0) goto out;

        RIL_requestTimedCallback(startGSPS,
                (void *)&s_socketId[socket_id], NULL);
    } else if (strStartsWith(s,"+SPREPORTRAU:")) {
        char *response = NULL;
        char *tmp;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextstr(&tmp, &response);
        if (err < 0)  goto out;

        if (!strcmp(response, "RAU SUCCESS")) {
            RIL_onUnsolicitedResponse(RIL_EXT_UNSOL_RAU_SUCCESS, NULL, 0,
                                      socket_id);
        }
    } else if (strStartsWith(s, "+SPERROR:")) {
        int type;
        int errCode;
        char *tmp;
        extern int s_ussdError[SIM_COUNT];
        extern int s_ussdRun[SIM_COUNT];

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextint(&tmp, &type);
        if (err < 0) goto out;

        err = at_tok_nexthexint(&tmp, &errCode);
        if (err < 0) goto out;

        if (errCode == 336) {
            RIL_onUnsolicitedResponse(RIL_EXT_UNSOL_CLEAR_CODE_FALLBACK, NULL,
                                      0, socket_id);
        }
        if ((type == 5) && (s_ussdRun[socket_id] == 1)) {  // 5: for SS
            s_ussdError[socket_id] = 1;
        }
    } else {
        ret = 0;
    }

out:
    free(line);
    return ret;
}
