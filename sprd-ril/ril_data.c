/**
 * ril_data.c --- Data-related requests process functions implementation
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#define LOG_TAG "RIL"

#include <netutils/ifc.h>
#include <net/if.h>
#include <netinet/in.h>
#include "sprd_ril.h"
#include "ril_data.h"
#include "ril_network.h"
#include "ril_call.h"
#include "ril_sim.h"
#include "channel_controller.h"
#include "ril_stk.h"
#include "ril_utils.h"

#define APN_DELAY_PROP          "persist.radio.apn_delay"
#define DUALPDP_ALLOWED_PROP    "persist.sys.dualpdp.allowed"
#define DDR_STATUS_PROP         "persist.sys.ddr.status"
#define REUSE_DEFAULT_PDN       "persist.sys.pdp.reuse"
#define BIP_OPENCHANNEL         "persist.sys.bip.openchannel"

int s_failCount = 0;
int s_dataAllowed[SIM_COUNT];
int s_manualSearchNetworkId = -1;
int s_setNetworkId = -1;
/* for LTE, attach will occupy a cid for default PDP in CP */
bool s_LTEDetached[SIM_COUNT] = {0};
static int s_GSCid;
static int s_ethOnOff;
static int s_activePDN;
static int s_addedIPCid = -1;  /* for VoLTE additional business */
static int s_autoDetach = 1;  /* whether support auto detach */
static int s_pdpType = IPV4V6;

PDP_INFO pdp_info[MAX_PDP_NUM];
pthread_mutex_t s_psServiceMutex = PTHREAD_MUTEX_INITIALIZER;
static int s_extDataFd = -1;
static char s_SavedDns[IP_ADDR_SIZE] = {0};
static char s_SavedDns_IPV6[IP_ADDR_SIZE * 4] ={0};
static int s_swapCard = 0;
pthread_mutex_t s_signalBipPdpMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t s_signalBipPdpCond = PTHREAD_COND_INITIALIZER;

struct OpenchannelInfo s_openchannelInfo[6] = {
    {-1, CLOSE, false, 0},
    {-1, CLOSE, false, 0},
    {-1, CLOSE, false, 0},
    {-1, CLOSE, false, 0},
    {-1, CLOSE, false, 0},
    {-1, CLOSE, false, 0}
};
static int s_openchannelCid = -1;

static int s_curCid[SIM_COUNT];

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
struct PDPInfo s_PDP[SIM_COUNT][MAX_PDP] = {
    {{-1, -1, -1, false, PDP_IDLE, PTHREAD_MUTEX_INITIALIZER},
     {-1, -1, -1, false, PDP_IDLE, PTHREAD_MUTEX_INITIALIZER},
     {-1, -1, -1, false, PDP_IDLE, PTHREAD_MUTEX_INITIALIZER},
     {-1, -1, -1, false, PDP_IDLE, PTHREAD_MUTEX_INITIALIZER},
     {-1, -1, -1, false, PDP_IDLE, PTHREAD_MUTEX_INITIALIZER},
     {-1, -1, -1, false, PDP_IDLE, PTHREAD_MUTEX_INITIALIZER},}
#if (SIM_COUNT >= 2)
   ,{{-1, -1, -1, false, PDP_IDLE, PTHREAD_MUTEX_INITIALIZER},
     {-1, -1, -1, false, PDP_IDLE, PTHREAD_MUTEX_INITIALIZER},
     {-1, -1, -1, false, PDP_IDLE, PTHREAD_MUTEX_INITIALIZER},
     {-1, -1, -1, false, PDP_IDLE, PTHREAD_MUTEX_INITIALIZER},
     {-1, -1, -1, false, PDP_IDLE, PTHREAD_MUTEX_INITIALIZER},
     {-1, -1, -1, false, PDP_IDLE, PTHREAD_MUTEX_INITIALIZER},}
#endif
};
static PDNInfo s_PDN[MAX_PDP_CP] = {
    { -1, "", "", ""},
    { -1, "", "", ""},
    { -1, "", "", ""},
    { -1, "", "", ""},
    { -1, "", "", ""},
    { -1, "", "", ""},
    { -1, "", "", ""},
    { -1, "", "", ""},
    { -1, "", "", ""},
    { -1, "", "", ""},
    { -1, "", "", ""}
};
static void detachGPRS(int channelID, void *data, size_t datalen, RIL_Token t);
static bool isApnEqual(char *new, char *old);
static bool isProtocolEqual(char *new, char *old);
static bool isBipProtocolEqual(char *new, char *old);
static int getMaxPDPNum(void) {
    return isLte() ? MAX_PDP : MAX_PDP / 2;
}

static int getPDP(RIL_SOCKET_ID socket_id) {
    int ret = -1;
    int i;
    char prop[PROPERTY_VALUE_MAX] = {0};
    int maxPDPNum = getMaxPDPNum();

    for (i = 0; i < maxPDPNum; i++) {
        if ((s_roModemConfig == LWG_LWG || socket_id == s_multiModeSim) &&
            s_activePDN > 0 && s_PDN[i].nCid == (i + 1)) {
            continue;
        }
        pthread_mutex_lock(&s_PDP[socket_id][i].mutex);
        if (s_PDP[socket_id][i].state == PDP_IDLE && s_PDP[socket_id][i].cid == -1) {
            s_PDP[socket_id][i].state = PDP_BUSY;
            ret = i;
            pthread_mutex_unlock(&s_PDP[socket_id][i].mutex);
            RLOGD("get s_PDP[%d]", ret);
            RLOGD("PDP[0].state = %d, PDP[1].state = %d, PDP[2].state = %d",
                    s_PDP[socket_id][0].state, s_PDP[socket_id][1].state, s_PDP[socket_id][2].state);
            RLOGD("PDP[3].state = %d, PDP[4].state = %d, PDP[5].state = %d",
                    s_PDP[socket_id][3].state, s_PDP[socket_id][4].state, s_PDP[socket_id][5].state);
            return ret;
        }
        pthread_mutex_unlock(&s_PDP[socket_id][i].mutex);
    }
    return ret;
}

void putPDP(RIL_SOCKET_ID socket_id, int cid) {
    if (cid < 0 || cid >= MAX_PDP || socket_id < RIL_SOCKET_1 ||
        socket_id >= SIM_COUNT) {
        return;
    }

    pthread_mutex_lock(&s_PDP[socket_id][cid].mutex);
    if (s_PDP[socket_id][cid].state != PDP_BUSY) {
        goto done;
    }
    s_PDP[socket_id][cid].state = PDP_IDLE;

done:
    if ((s_PDP[socket_id][cid].secondary_cid > 0) &&
        (s_PDP[socket_id][cid].secondary_cid <= MAX_PDP)) {
        s_PDP[socket_id][s_PDP[socket_id][cid].secondary_cid - 1].secondary_cid = -1;
    }
    s_PDP[socket_id][cid].secondary_cid = -1;
    s_PDP[socket_id][cid].cid = -1;
    s_PDP[socket_id][cid].isPrimary = false;
    RLOGD("put s_PDP[%d]", cid);
    pthread_mutex_unlock(&s_PDP[socket_id][cid].mutex);
    pthread_mutex_lock(&s_signalBipPdpMutex);
    if (s_openchannelInfo[cid].count != 0) {
        s_openchannelInfo[cid].count = 0;
    }
    s_openchannelInfo[cid].pdpState = false;
    pthread_mutex_unlock(&s_signalBipPdpMutex);
}

static int getPDPByIndex(RIL_SOCKET_ID socket_id, int index) {
    if (index >= 0 && index < MAX_PDP) {  // cid: 1 ~ MAX_PDP
        pthread_mutex_lock(&s_PDP[socket_id][index].mutex);
        if (s_PDP[socket_id][index].state == PDP_IDLE) {
            s_PDP[socket_id][index].state = PDP_BUSY;
            pthread_mutex_unlock(&s_PDP[socket_id][index].mutex);
            RLOGD("getPDPByIndex[%d]", index);
            RLOGD("PDP[0].state = %d, PDP[1].state = %d, PDP[2].state = %d",
                   s_PDP[socket_id][0].state, s_PDP[socket_id][1].state, s_PDP[socket_id][2].state);
            RLOGD("PDP[3].state = %d, PDP[4].state = %d, PDP[5].state = %d",
                   s_PDP[socket_id][3].state, s_PDP[socket_id][4].state, s_PDP[socket_id][5].state);
            return index;
        }
        pthread_mutex_unlock(&s_PDP[socket_id][index].mutex);
    }
    return -1;
}

void putPDPByIndex(RIL_SOCKET_ID socket_id, int index) {
    if (index < 0 || index >= MAX_PDP) {
        return;
    }
    pthread_mutex_lock(&s_PDP[socket_id][index].mutex);
    if (s_PDP[socket_id][index].state == PDP_BUSY) {
        s_PDP[socket_id][index].state = PDP_IDLE;
    }
    pthread_mutex_unlock(&s_PDP[socket_id][index].mutex);
}

void putUnusablePDPCid(RIL_SOCKET_ID socket_id) {
    int i = 0;
    for (; i < MAX_PDP; i++) {
        pthread_mutex_lock(&s_PDP[socket_id][i].mutex);
        if (s_PDP[socket_id][i].cid == UNUSABLE_CID) {
            RLOGD("putUnusablePDPCid cid = %d", i + 1);
            s_PDP[socket_id][i].cid = -1;
        }
        pthread_mutex_unlock(&s_PDP[socket_id][i].mutex);
    }
}

int updatePDPCid(RIL_SOCKET_ID socket_id, int cid, int state) {
    int index = cid - 1;
    if (cid <= 0 || cid > MAX_PDP) {
        return 0;
    }
    pthread_mutex_lock(&s_PDP[socket_id][index].mutex);
    if (state == 1) {
        s_PDP[socket_id][index].cid = cid;
    } else if (state == 0) {
        s_PDP[socket_id][index].cid = -1;
    } else if (state == -1 && s_PDP[socket_id][index].cid == -1) {
        s_PDP[socket_id][index].cid = UNUSABLE_CID;
    }
    pthread_mutex_unlock(&s_PDP[socket_id][index].mutex);
    return 1;
}

int getPDPCid(RIL_SOCKET_ID socket_id, int index) {
    if (index >= MAX_PDP || index < 0) {
        return -1;
    } else {
        return s_PDP[socket_id][index].cid;
    }
}

int getActivedCid(RIL_SOCKET_ID socket_id) {
    int i = 0;
    for (; i < MAX_PDP; i++) {
        pthread_mutex_lock(&s_PDP[socket_id][i].mutex);
        if (s_PDP[socket_id][i].state == PDP_BUSY && s_PDP[socket_id][i].cid != -1) {
            RLOGD("get actived cid = %d", s_PDP[socket_id][i].cid);
            pthread_mutex_unlock(&s_PDP[socket_id][i].mutex);
            return i;
        }
        pthread_mutex_unlock(&s_PDP[socket_id][i].mutex);
    }
    return -1;
}

enum PDPState getPDPState(RIL_SOCKET_ID socket_id, int index) {
    if (index >= MAX_PDP || index < 0) {
        return PDP_IDLE;
    } else {
        return s_PDP[socket_id][index].state;
    }
}

int getFallbackCid(RIL_SOCKET_ID socket_id, int index) {
    if (index >= MAX_PDP || index < 0) {
        return -1;
    } else {
        return s_PDP[socket_id][index].secondary_cid;
    }
}

bool isPrimaryCid(RIL_SOCKET_ID socket_id, int index) {
    if (index >= MAX_PDP || index < 0) {
        return false;
    } else {
        return s_PDP[socket_id][index].isPrimary;
    }
}

int setPDPMapping(RIL_SOCKET_ID socket_id, int primary, int secondary) {
    RLOGD("setPDPMapping primary %d, secondary %d", primary, secondary);
    if (primary < 0 || primary >= MAX_PDP || secondary < 0 ||
        secondary >= MAX_PDP) {
        return 0;
    }
    pthread_mutex_lock(&s_PDP[socket_id][primary].mutex);
    s_PDP[socket_id][primary].cid = primary + 1;
    s_PDP[socket_id][primary].secondary_cid = secondary + 1;
    s_PDP[socket_id][primary].isPrimary = true;
    pthread_mutex_unlock(&s_PDP[socket_id][primary].mutex);

    pthread_mutex_lock(&s_PDP[socket_id][secondary].mutex);
    s_PDP[socket_id][secondary].cid = secondary + 1;
    s_PDP[socket_id][secondary].secondary_cid = primary + 1;
    s_PDP[socket_id][secondary].isPrimary = false;
    pthread_mutex_unlock(&s_PDP[socket_id][secondary].mutex);
    return 1;
}

int isExistActivePdp(RIL_SOCKET_ID socket_id) {
    int cid;
    for (cid = 0; cid < MAX_PDP; cid++) {
        pthread_mutex_lock(&s_PDP[socket_id][cid].mutex);
        if (s_PDP[socket_id][cid].state == PDP_BUSY) {
            pthread_mutex_unlock(&s_PDP[socket_id][cid].mutex);
            RLOGD("PDP[0].state = %d, PDP[1].state = %d, PDP[2].state = %d",
                    s_PDP[socket_id][0].state, s_PDP[socket_id][1].state, s_PDP[socket_id][2].state);
            RLOGD("PDP[%d] is busy now", cid);
            return 1;
        }
        pthread_mutex_unlock(&s_PDP[socket_id][cid].mutex);
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

/**
 * return  -1: general failed;
 *          0: success;
 *          1: fail cause 288, need retry active;
 *          2: fail cause 128, need fall back;
 *          3: fail cause 253, need retry active for another cid;
 */
static int errorHandlingForCGDATA(int channelID, ATResponse *p_response,
                                      int err, int cid) {
    int failCause;
    int ret = DATA_ACTIVE_SUCCESS;
    char cmd[AT_COMMAND_LEN] = {0};
    char *line;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    if (err < 0 || p_response->success == 0) {
        ret = DATA_ACTIVE_FAILED;
        if (p_response != NULL &&
                strStartsWith(p_response->finalResponse, "+CME ERROR:")) {
            line = p_response->finalResponse;
            err = at_tok_start(&line);
            if (err >= 0) {
                err = at_tok_nextint(&line, &failCause);
                if (err >= 0) {
                    if (failCause == 288) {
                        ret = DATA_ACTIVE_NEED_RETRY;
                    } else if (failCause == 128) {  // 128: network reject
                        ret = DATA_ACTIVE_NEED_FALLBACK;
                    } else if (failCause == 253) {
                        ret = DATA_ACTIVE_NEED_RETRY_FOR_ANOTHER_CID;
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
        if (err == AT_ERROR_TIMEOUT || (p_response != NULL &&
                strStartsWith(p_response->finalResponse, "ERROR"))) {
            s_lastPDPFailCause[socket_id] = PDP_FAIL_ERROR_UNSPECIFIED;
            snprintf(cmd, sizeof(cmd), "AT+CGACT=0,%d", cid);
            at_send_command(s_ATChannels[channelID], cmd, NULL);
            cgact_deact_cmd_rsp(cid);
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
    int skip, active;
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
        int i, cid;
        int type;
        char *apn;
        char *attachApn;
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
        i = cid - 1;
        if (pdns->nCid > MAX_PDP || i < 0) {
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
            s_PDN[i].nCid = -1;
        }
        snprintf(s_PDN[i].strApn, sizeof(s_PDN[i].strApn),
                 "%s", apn);
        /* type */
        err = at_tok_nextint(&line, &type);
        if (err < 0) {
            s_PDN[i].nCid = -1;
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
        snprintf(s_PDN[cid - 1].strIPType, sizeof(s_PDN[i].strIPType),
                  "%s", strType);
        if (at_tok_hasmore(&line)) {
            err = at_tok_nextstr(&line, &attachApn);
            if (err >= 0) {
                snprintf(s_PDN[i].strAttachApn, sizeof(s_PDN[i].strAttachApn),
                     "%s", attachApn);
            }
        }
        if (active > 0) {
            RLOGI("active PDN: cid = %d, iptype = %s, apn = %s, attachApn = %s",
                  s_PDN[i].nCid, s_PDN[i].strIPType,
                  s_PDN[i].strApn, s_PDN[i].strAttachApn);
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
    ATResponse *p_newResponse = NULL;

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

    cgdcont_read_cmd_rsp(pdnResponse, &p_newResponse);
    for (pCur = p_newResponse->p_intermediates; pCur != NULL;
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
    at_response_free(p_newResponse);
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

char *getPDNAttachAPN(int index) {
    if (index >= MAX_PDP_CP || index < 0) {
        return NULL;
    } else {
        return s_PDN[index].strAttachApn;
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
 * return : -2: Active Cid success,but isnt fall back cid ip type;
 *          -1: Active Cid failed;
 *           0: Active Cid success;
 *           1: Active Cid failed, error 288, need retry;
 *           2: Active Cid failed, error 128, need do fall back;
 */
static int activeSpeciedCidProcess(int channelID, void *data, int cid,
                                       const char *pdp_type, int primaryCid) {
    int err;
    int ret = -1, ipType;
    int islte = s_isLTE;
    int failCause;
    char *line;
    char cmd[AT_COMMAND_LEN] = {0};
    char newCmd[AT_COMMAND_LEN] = {0};
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
    cgact_deact_cmd_rsp(cid);


    if (!strcmp(pdp_type, "IPV4+IPV6")) {
        snprintf(cmd, sizeof(cmd), "AT+CGDCONT=%d,\"IP\",\"%s\",\"\",0,0",
                  cid, apn);
    } else {
        snprintf(cmd, sizeof(cmd), "AT+CGDCONT=%d,\"%s\",\"%s\",\"\",0,0",
                  cid, pdp_type, apn);
    }
    err = cgdcont_set_cmd_req(cmd, newCmd);
    if (err == 0) {
        err = at_send_command(s_ATChannels[channelID], newCmd, &p_response);
        if (err < 0 || p_response->success == 0) {
            s_lastPDPFailCause[socket_id] = PDP_FAIL_ERROR_UNSPECIFIED;
            putPDP(socket_id, cid - 1);
            at_response_free(p_response);
            return ret;
        }
        AT_RESPONSE_FREE(p_response);
    }

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
    cgdata_set_cmd_req(cmd);
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    s_curCid[socket_id] = cid;
    cgdata_set_cmd_rsp(p_response, cid - 1, primaryCid, channelID);
    ret = errorHandlingForCGDATA(channelID, p_response, err, cid);
    AT_RESPONSE_FREE(p_response);
    if (ret != DATA_ACTIVE_SUCCESS) {
        putPDP(socket_id, cid - 1);
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
            cgact_deact_cmd_rsp(cid);
            at_response_free(p_response);
            putPDP(socket_id, cid - 1);
            ret = DATA_ACTIVE_FALLBACK_FAILED;
        } else {
            setPDPMapping(socket_id, primaryCid - 1, cid - 1);
            ret = DATA_ACTIVE_SUCCESS;
        }
    } else {
        updatePDPCid(socket_id, cid, 1);
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

static bool doIPV4_IPV6_Fallback(int channelID, int index, void *data) {
    bool ret = false;
    int err = 0;
    char *line;
    char cmd[AT_COMMAND_LEN] = {0};
    char newCmd[AT_COMMAND_LEN] = {0};
    const char *apn = NULL;
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    apn = ((const char **)data)[2];

    // active IPV4
    snprintf(cmd, sizeof(cmd), "AT+CGDCONT=%d,\"IP\",\"%s\",\"\",0,0",
            index + 1, apn);
    err = cgdcont_set_cmd_req(cmd, newCmd);
    if (err == 0) {
        err = at_send_command(s_ATChannels[channelID], newCmd, &p_response);
    }
    if (err < 0 || p_response->success == 0) {
        s_lastPDPFailCause[socket_id] = PDP_FAIL_ERROR_UNSPECIFIED;
        goto error;
    }
    AT_RESPONSE_FREE(p_response);

    if (s_isLTE) {
        snprintf(cmd, sizeof(cmd), "AT+CGDATA=\"M-ETHER\",%d", index + 1);
    } else {
        snprintf(cmd, sizeof(cmd), "AT+CGDATA=\"PPP\",%d", index + 1);
    }
    cgdata_set_cmd_req(cmd);
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    cgdata_set_cmd_rsp(p_response, index, 0, channelID);
    if (errorHandlingForCGDATA(channelID, p_response, err,index) !=
            DATA_ACTIVE_SUCCESS) {
        goto error;
    }

    updatePDPCid(socket_id, index + 1, 1);
    // active IPV6
    index = getPDP(socket_id);
    if (index < 0 || getPDPCid(socket_id, index) >= 0) {
        s_lastPDPFailCause[socket_id] = PDP_FAIL_ERROR_UNSPECIFIED;
        putPDP(socket_id, index);
    } else {
        activeSpeciedCidProcess(channelID, data, index + 1, "IPV6", 0);
    }
    ret = true;

error:
    at_response_free(p_response);
    return ret;
}

/*
 * check if IPv6 address is begain with FE80,if yes,return Ipv6 address begin with 0000
 */
void checkIpv6Address(char *oldIpv6Address, char *newIpv6Address, int len) {
    RLOGD("checkIpv6Address: old ipv6 address is: %s", oldIpv6Address);
    if (oldIpv6Address == NULL) {
        snprintf(newIpv6Address, len,"%s",
                "FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF");
        return;
    }
    strncpy(newIpv6Address, oldIpv6Address, strlen(oldIpv6Address));
    if (strncasecmp(newIpv6Address, "fe80:", sizeof("fe80:") - 1) == 0) {
    char *temp = strchr(newIpv6Address, ':');
       if (temp != NULL) {
           snprintf(newIpv6Address, len, "0000%s", temp);
       }
    }
    RLOGD("checkIpv6Address: ipv6 address is: %s",newIpv6Address);
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
    ATResponse *p_newResponse = NULL;
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
    cgdcont_read_cmd_rsp(p_response, &p_newResponse);
    for (p_cur = p_newResponse->p_intermediates; p_cur != NULL;
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
            if (getPDPState(socket_id, cid - 1) == PDP_IDLE) {
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
    AT_RESPONSE_FREE(p_newResponse);

    if ((t != NULL) && (cid > 0)) {
        RLOGD("requestOrSendDataCallList is called by SetupDataCall!");
        for (i = 0; i < MAX_PDP; i++) {
            if (responses[i].cid == cid) {
                if (responses[i].active) {
                    int fb_cid = getFallbackCid(socket_id, cid - 1);  // pdp fallback cid
                    RLOGD("called by SetupDataCall! fallback cid : %d", fb_cid);
                    if (islte && s_LTEDetached[socket_id]) {
                        RLOGD("Lte detached in the past2.");
                        char cmd[AT_COMMAND_LEN];
                        snprintf(cmd, sizeof(cmd), "AT+CGACT=0,%d", cid);
                        at_send_command(s_ATChannels[channelID], cmd, NULL);
                        cgact_deact_cmd_rsp(cid);
                        putPDP(socket_id, fb_cid -1);
                        putPDP(socket_id, cid - 1);
                        s_lastPDPFailCause[socket_id] =
                                PDP_FAIL_ERROR_UNSPECIFIED;
                        RIL_onRequestComplete(*t, RIL_E_GENERIC_FAILURE, NULL,
                                              0);
                    } else {
                        RIL_onRequestComplete(*t, RIL_E_SUCCESS, &responses[i],
                                sizeof(RIL_Data_Call_Response_v11));
                        /* send IP for volte addtional business */
                        if (islte && (s_roModemConfig == LWG_LWG ||
                                      socket_id == s_multiModeSim)) {
                            char cmd[AT_COMMAND_LEN] = {0};
                            char prop0[PROPERTY_VALUE_MAX] = {0};
                            char prop1[PROPERTY_VALUE_MAX] = {0};
                            char ipv6Address[PROPERTY_VALUE_MAX] = {0};
                            if (!strcmp(responses[i].type, "IPV4V6")) {
                                snprintf(cmd, sizeof(cmd), "net.%s%d.ip",
                                        eth, cid - 1);
                                property_get(cmd, prop0, NULL);

                                snprintf(cmd, sizeof(cmd), "net.%s%d.ipv6_ip",
                                        eth, cid - 1);
                                property_get(cmd, prop1, NULL);
                                checkIpv6Address(prop1, ipv6Address, sizeof(ipv6Address));
                                snprintf(cmd, sizeof(cmd),
                                          "AT+XCAPIP=%d,\"%s,[%s]\"", cid,
                                          prop0, ipv6Address);
                            } else if (!strcmp(responses[i].type, "IP")) {
                                snprintf(cmd, sizeof(cmd),
                                    "AT+XCAPIP=%d,\"%s,[FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF]\"",
                                    cid, responses[i].addresses);
                            } else {
                                checkIpv6Address(responses[i].addresses, ipv6Address, sizeof(ipv6Address));
                                snprintf(cmd, sizeof(cmd),
                                        "AT+XCAPIP=%d,\"0.0.0.0,[%s]\"", cid,
                                        ipv6Address);
                            }
                            at_send_command(s_ATChannels[channelID], cmd, NULL);
                            s_addedIPCid = responses[i].cid;
                        }
                    }
                } else {
                    putPDP(socket_id, getFallbackCid(socket_id, cid - 1) - 1);
                    putPDP(socket_id, cid - 1);
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
    s_curCid[socket_id] = 0;
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
static int reuseDefaultBearer(int channelID, void *data,
                               const char *type, RIL_Token t) {
    bool islte = s_isLTE;
    int err, ret = -1, cgdata_err;
    char strApnName[ARRAY_SIZE] = {0};
    char cmd[AT_COMMAND_LEN] = {0};
    char prop[PROPERTY_VALUE_MAX] = {0};
    ATResponse *p_response = NULL;
    int useDefaultPDN;

    const char *apn, *username, *password, *authtype;

    apn = ((const char **)data)[2];
    username = ((const char **)data)[3];
    password = ((const char **)data)[4];
    authtype = ((const char **)data)[5];

    property_get(REUSE_DEFAULT_PDN, prop, "0");
    useDefaultPDN = atoi(prop);
    RLOGD("useDefaultPDN = %d",useDefaultPDN);

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    if (islte && (s_roModemConfig == LWG_LWG ||
                  socket_id == s_multiModeSim)) {
        queryAllActivePDNInfos(channelID);
        int i, cid;
        if (s_activePDN > 0) {
            for (i = 0; i < MAX_PDP; i++) {
                cid = getPDNCid(i);
                if (cid == (i + 1)) {
                    RLOGD("s_PDP[%d].state = %d", i, getPDPState(socket_id, i));
                    if (i < MAX_PDP &&
                        (useDefaultPDN ||
                        ((isApnEqual((char *)apn, getPDNAPN(i)) || !strcasecmp((char *)apn, getPDNAttachAPN(i))) &&
                            isProtocolEqual((char *)type, getPDNIPType(i))) ||
                        s_singlePDNAllowed[socket_id] == 1)) {
                        if (getPDPState(socket_id, i) == PDP_IDLE) {
                            RLOGD("Using default PDN");
                            getPDPByIndex(socket_id, i);
                            cgact_deact_cmd_rsp(cid);
                            AT_RESPONSE_FREE(p_response);
                            if (strcmp(type, "IP") == 0) {
                                s_pdpType = IPV4;
                            } else if (strcmp(type, "IPV6") == 0){
                                s_pdpType = IPV6;
                            } else {
                                s_pdpType = IPV4V6;
                            }
                            char newCmd[AT_COMMAND_LEN] = {0};
                            char qosState[PROPERTY_VALUE_MAX] = {0};
                            snprintf(cmd, sizeof(cmd), "AT+CGDCONT=%d,\"%s\",\"%s\",\"\",0,0",
                                      cid, type, apn);
                            err = cgdcont_set_cmd_req(cmd, newCmd);
                            if (err == 0) {
                                err = at_send_command(s_ATChannels[channelID], newCmd, NULL);
                            }

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
                            snprintf(cmd, sizeof(cmd), "AT+CGDATA=\"M-ETHER\",%d",
                                      cid);
                            cgdata_set_cmd_req(cmd);
                            err = at_send_command(s_ATChannels[channelID], cmd,
                                                  &p_response);
                            s_curCid[socket_id] = cid;
                            cgdata_set_cmd_rsp(p_response, cid - 1, 0, channelID);
                            cgdata_err = errorHandlingForCGDATA(channelID,
                                    p_response, err, cid);
                            AT_RESPONSE_FREE(p_response);
                            if (cgdata_err == DATA_ACTIVE_SUCCESS) {
                                updatePDPCid(socket_id, i + 1, 1);
                                s_openchannelCid = cid;
                                pthread_mutex_lock(&s_signalBipPdpMutex);
                                s_openchannelInfo[i].count++;
                                s_openchannelInfo[i].pdpState = true;
                                pthread_cond_signal(&s_signalBipPdpCond);
                                pthread_mutex_unlock(&s_signalBipPdpMutex);
                                RLOGD("reuse count = %d", s_openchannelInfo[i].count);
                                requestOrSendDataCallList(channelID, cid, &t);
                                ret = 0;
                            } else if (cgdata_err ==
                                    DATA_ACTIVE_NEED_RETRY_FOR_ANOTHER_CID) {
                                updatePDPCid(socket_id, i + 1, -1);
                                putPDPByIndex(socket_id, i);
                            } else if (cgdata_err == DATA_ACTIVE_NEED_RETRY) {
                                /*bug849843 cSim1 data attach failed after call in dual LTE network mode*/
                                putPDPByIndex(socket_id, i);
                                /*bug837360 cgdata during ps attach,pdp active fail*/
                                ret = -2;
                            } else {
                                ret = cid;
                            }
                        } else {
                            if (s_openchannelInfo[i].state != CLOSE) {
                                s_openchannelCid = cid;
                                s_openchannelInfo[i].count++;
                                requestOrSendDataCallList(channelID, cid, &t);
                                RLOGD("reuse count = %d", s_openchannelInfo[cid - 1].count);
                                ret = 0;
                            }
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
    const char *pdpType = "IP";
    RIL_Data_Call_Response_v11 response;

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);


    if (isVoLteEnable() && !isExistActivePdp(socket_id)) {  // for ddr, power consumption
        char prop[PROPERTY_VALUE_MAX];
        property_get(DDR_STATUS_PROP, prop, "0");
        RLOGD("volte ddr power prop = %s", prop);
        if (!strcmp(prop, "1")) {
            at_send_command(s_ATChannels[channelID], "AT+SPVOOLTE=0", NULL);
        }
    }
RETRY:

    if (datalen > 6 * sizeof(char *)) {
        pdpType = ((const char **)data)[6];
    } else {
        pdpType = "IP";
    }

    s_LTEDetached[socket_id] = false;
    /* check if reuse default bearer or not */
    ret = reuseDefaultBearer(channelID, data, pdpType, t);
    if (ret == 0) {
        return;
    } else if (ret > 0) {
        primaryindex = ret - 1;
        goto error;
    } else if (ret == -2) {
        if (nRetryTimes < 5) {
            nRetryTimes++;
            goto RETRY;
        }
    }

    index = getPDP(socket_id);

    if (index < 0 || getPDPCid(socket_id, index) >= 0) {
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
    } else if ( ret == DATA_ACTIVE_NEED_FALLBACK) {
        if (doIPV4_IPV6_Fallback(channelID, index, data) == false) {
            goto error;
        } else {
            goto done;
        }
    } else if (ret == DATA_ACTIVE_NEED_RETRY_FOR_ANOTHER_CID) {
        updatePDPCid(socket_id, index + 1, -1);
        goto RETRY;
    } else if (ret == DATA_ACTIVE_SUCCESS &&
            (!strcmp(pdpType, "IPV4V6") || !strcmp(pdpType, "IPV4+IPV6")) &&
            s_isLTE ) {
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
        if (index < 0 || getPDPCid(socket_id, index) >= 0) {
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
    putUnusablePDPCid(socket_id);
    pthread_mutex_lock(&s_signalBipPdpMutex);
    s_openchannelCid = primaryindex + 1;
    s_openchannelInfo[primaryindex].count++;
    s_openchannelInfo[primaryindex].pdpState = true;
    pthread_cond_signal(&s_signalBipPdpCond);
    pthread_mutex_unlock(&s_signalBipPdpMutex);
    requestOrSendDataCallList(channelID, primaryindex + 1, &t);
    return;

error:
    if (primaryindex >= 0) {
        putPDP(socket_id, getFallbackCid(socket_id, primaryindex) - 1);
        putPDP(socket_id, primaryindex);
        s_openchannelInfo[primaryindex].pdpState = true;
    }
    pthread_mutex_lock(&s_signalBipPdpMutex);
    pthread_cond_signal(&s_signalBipPdpCond);
    pthread_mutex_unlock(&s_signalBipPdpMutex);
    putUnusablePDPCid(socket_id);

    response.status = s_lastPDPFailCause[socket_id];
    response.suggestedRetryTime = -1;
    response.cid = -1;
    response.active = -1;
    response.type = "";
    response.ifname = "";
    response.addresses = "";
    response.dnses = "";
    response.gateways = "";
    response.pcscf = "";
    response.mtu = 0;
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, &response,
            sizeof(RIL_Data_Call_Response_v11));
}

static void updateAdditionBusinessCid(int channelID) {
    char cmd[ARRAY_SIZE] = {0};
    char ethPropName[ARRAY_SIZE] = {0};
    char prop[PROPERTY_VALUE_MAX] = {0};
    char eth[PROPERTY_VALUE_MAX] = {0};
    char ipv4[PROPERTY_VALUE_MAX] = {0};
    char ipv6[PROPERTY_VALUE_MAX] = {0};
    int ipType = 0;

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
    int cidIndex = getActivedCid(socket_id);
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
    if (s_isLTE) {
        snprintf(cmd, sizeof(cmd), "AT+XCAPIP=%d,\"%s,[%s]\"", cidIndex + 1,
              ipv4, ipv6);
    }

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
    bool error = false;

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    p_cid = ((const char **)data)[0];
    cid = atoi(p_cid);
    if (cid < 1) {
        error = true;
        goto done;
    }

    RLOGD("deactivateDC s_in4G[%d]=%d, count = %d", socket_id, s_in4G[socket_id],
                s_openchannelInfo[cid - 1].count);
    if (s_openchannelInfo[cid - 1].count > 0) {
        s_openchannelInfo[cid - 1].count--;
    }
    if (getPDPState(socket_id, cid - 1) == PDP_IDLE) {
        RLOGD("deactive done!");
        goto done;
    }
    if (s_openchannelInfo[cid - 1].count != 0){
        error = true;
        goto done;
    }

    secondaryCid = getFallbackCid(socket_id, cid - 1);
    snprintf(cmd, sizeof(cmd), "AT+CGACT=0,%d", cid);
    at_send_command(s_ATChannels[channelID], cmd, &p_response);
    cgact_deact_cmd_rsp(cid);
    AT_RESPONSE_FREE(p_response);
    if (secondaryCid != -1) {
        RLOGD("dual PDP, do CGACT again, fallback cid = %d", secondaryCid);
        snprintf(cmd, sizeof(cmd), "AT+CGACT=0,%d", secondaryCid);
        at_send_command(s_ATChannels[channelID], cmd, &p_response);
        cgact_deact_cmd_rsp(secondaryCid);
    }

    putPDP(socket_id, secondaryCid - 1);
    putPDP(socket_id, cid - 1);
    at_response_free(p_response);

    // for ddr, power consumption
    if (isVoLteEnable() && !isExistActivePdp(socket_id)) {
        property_get(DDR_STATUS_PROP, prop, "0");
        RLOGD("volte ddr power prop = %s", prop);
        if (!strcmp(prop, "1")) {
            at_send_command(s_ATChannels[channelID], "AT+SPVOOLTE=1", NULL);
        }
    }
done:
    if (s_isSimPresent[socket_id] != PRESENT) {
        RLOGE("deactivateDataConnection: card is absent");
        RIL_onRequestComplete(t, RIL_E_INVALID_CALL_ID, NULL, 0);
        return;
    }
    if (t != NULL) {
        if (error) {
            RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        } else {
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        }
    }
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
        strcasecmp(old, "IPV4V6") == 0 ||
        strcasecmp(new, old) == 0) {
        ret = true;
    }
    return ret;
}

static bool isBipProtocolEqual(char *new, char *old) {
    bool ret = false;
    if (strcasecmp(new, "IPV4V6") == 0 ||
        strcasecmp(old, "IPV4V6") == 0 ||
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
    if (isStrEmpty(new->apn) && isStrEmpty(new->protocol)){
        ret = 2;
    } else if (isApnEqual(new->apn, old->apn) &&
        isStrEqual(new->protocol, old->protocol) &&
        isStrEqual(new->username, old->username) &&
        isStrEqual(new->password, old->password) &&
        new->authtype == old->authtype) {
        ret = 1;
    }
    return ret;
}

static void setDataProfile(RIL_InitialAttachApn *new, int cid,
                             int channelID, int socket_id) {
    char qosState[PROPERTY_VALUE_MAX] = {0};
    char cmd[AT_COMMAND_LEN] = {0};
    char newCmd[AT_COMMAND_LEN] = {0};
    snprintf(cmd, sizeof(cmd), "AT+CGDCONT=%d,\"%s\",\"%s\",\"\",0,0",
             cid, new->protocol, new->apn);
    int err = cgdcont_set_cmd_req(cmd, newCmd);
    if (err == 0) {
        at_send_command(s_ATChannels[channelID], newCmd, NULL);
    }

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
    char manualAttachProp[PROPERTY_VALUE_MAX] = {0};

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
                if(ret == 2){  // esm flag = 0, both apn and protocol are empty.
                    property_get(LTE_MANUAL_ATTACH_PROP, manualAttachProp, "0");
                    RLOGD("persist.radio.manual.attach: %s", manualAttachProp);
                    if(atoi(manualAttachProp)){
                        at_send_command(s_ATChannels[channelID], "AT+SPREATTACH", NULL);
                    }
                } else {
                    RLOGD("send APN information even though apn is same with network");
                    setDataProfile(pIAApn, initialAttachId, channelID, socket_id);
                }
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
            if (isSetReattach && (s_in4G[socket_id] == 1 ||
                s_PSRegStateDetail[socket_id] == RIL_NOT_REG_AND_NOT_SEARCHING ||
                s_PSRegStateDetail[socket_id] == RIL_NOT_REG_AND_SEARCHING ||
                s_PSRegStateDetail[socket_id] == RIL_UNKNOWN ||
                s_PSRegStateDetail[socket_id] == RIL_REG_DENIED)) {
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
    if ((int)socket_id < 0 || (int)socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        return;
    }
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
    if (s_swapCard != 0) {
        RLOGD("swapcard in processing, return!!");
        goto error;
    }
#if (SIM_COUNT == 2)
    if (s_autoDetach == 1) {
        doDetachGPRS(1 - socket_id, data, datalen, NULL);
    }
#endif

    if (islte) {
        if (s_roModemConfig == LWG_LWG) {
            if(s_modemConfig == LWG_LWG){
                char numToStr[ARRAY_SIZE];
                snprintf(numToStr, sizeof(numToStr), "%d", socket_id);
                property_set(PRIMARY_SIM_PROP, numToStr);
                s_multiModeSim = socket_id;
            }
            snprintf(cmd, sizeof(cmd), "AT+SPSWDATA");
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
    int state = PDP_IDLE;
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    if (islte) {
        for (i = 0; i < MAX_PDP; i++) {
            cid = getPDPCid(socket_id, i);
            state = getPDPState(socket_id, i);
            if (state == PDP_BUSY || cid > 0) {
                snprintf(cmd, sizeof(cmd), "AT+CGACT=0,%d", i + 1);
                at_send_command(s_ATChannels[channelID], cmd, &p_response);
                RLOGD("s_PDP[%d].state = %d", i, state);
                putPDP(socket_id, i);
                cgact_deact_cmd_rsp(i + 1);

                if (state == PDP_BUSY) {
                    requestOrSendDataCallList(channelID, i + 1, NULL);
                }
                AT_RESPONSE_FREE(p_response);
            }
        }
#if defined (ANDROID_MULTI_SIM)
        if (s_presentSIMCount == SIM_COUNT) {
            RLOGD("simID = %d", socket_id);
            if (s_roModemConfig == LWG_LWG) {
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
    if (s_desiredRadioState[socket_id] > 0 && isAttachEnable() &&
            !(s_radioState[socket_id] == RADIO_STATE_OFF ||
              s_radioState[socket_id] == RADIO_STATE_UNAVAILABLE)) {
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

    // L+L don't need auto detach.
    if (s_autoDetach == 1 && s_modemConfig == LWG_LWG) {
        s_autoDetach = 0;
    }

    switch (request) {
        case RIL_REQUEST_SETUP_DATA_CALL: {
            if (s_isSimPresent[socket_id] == SIM_ABSENT) {
                RIL_onRequestComplete(t, RIL_E_OP_NOT_ALLOWED_BEFORE_REG_TO_NW, NULL, 0);
                break;
            }
            if (s_manualSearchNetworkId >= 0 || s_swapCard != 0) {
                RLOGD("s_manualSearchNetworkId = %d, swapcard = %d", s_manualSearchNetworkId, s_swapCard);
                s_lastPDPFailCause[socket_id] = PDP_FAIL_ERROR_UNSPECIFIED;
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
                break;
            }
            if (s_desiredRadioState[socket_id] > 0 && isAttachEnable()) {
                if (s_isLTE) {
                    RLOGD("SETUP_DATA_CALL s_PSRegState[%d] = %d", socket_id,
                          s_PSRegState[socket_id]);
                    if (s_PSRegState[socket_id] == STATE_IN_SERVICE) {
#if (SIM_COUNT == 2)
                        /* bug813401 L+L version,only setup data call on data card @{ */
                        if (s_modemConfig == LWG_LWG && s_dataAllowed[socket_id] != 1) {
                            s_lastPDPFailCause[socket_id] =
                                    PDP_FAIL_ERROR_UNSPECIFIED;
                            RIL_onRequestComplete(t,
                                    RIL_E_GENERIC_FAILURE, NULL, 0);
                            break;
                        }
                        /* }@ */
#endif
                        requestSetupDataCall(channelID, data, datalen, t);
                        s_failCount = 0;
                    } else {
                        if (s_roModemConfig != LWG_LWG &&
                                s_multiModeSim != socket_id && s_dataAllowed[socket_id] == 1) {
                            requestSetupDataCall(channelID, data, datalen, t);
                        } else {
                            if (s_failCount < 5) {
                                s_lastPDPFailCause[socket_id] =
                                        PDP_FAIL_ERROR_UNSPECIFIED;
                                s_failCount++;
                            } else {
                                s_lastPDPFailCause[socket_id] =
                                        PDP_FAIL_SERVICE_OPTION_NOT_SUPPORTED;
                            }
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
            RLOGD("s_manualSearchNetworkId = %d,s_setNetworkId = %d",
                   s_manualSearchNetworkId,s_setNetworkId );
            if (s_modemConfig != LWG_LWG && (s_manualSearchNetworkId >= 0 || s_setNetworkId >= 0)) {
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
            char newCmd[AT_COMMAND_LEN] = {0};
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
                err = cgdcont_set_cmd_req(cmd, newCmd);
                if (err == 0) {
                    err = at_send_command(s_ATChannels[channelID], newCmd,
                                          &p_response);
                }
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
        case RIL_REQUEST_SET_SOS_INITIAL_ATTACH_APN: {
            char cmd[AT_COMMAND_LEN] = {0};
            char qosState[PROPERTY_VALUE_MAX] = {0};
            int initialAttachId = 9;  // use index of 9 for sos
            RIL_InitialAttachApn *initialAttachSOSApn = NULL;
            p_response = NULL;
            if (data != NULL) {
                initialAttachSOSApn = (RIL_InitialAttachApn *)data;

                snprintf(cmd, sizeof(cmd),
                        "AT+CGDCONT=%d,\"%s\",\"%s\",\"\",0,0",
                         initialAttachId, initialAttachSOSApn->protocol,
                         initialAttachSOSApn->apn);
                err = at_send_command(s_ATChannels[channelID],
                        cmd, &p_response);

                snprintf(cmd, sizeof(cmd), "AT+CGPCO=0,\"%s\",\"%s\",%d,%d",
                        initialAttachSOSApn->username,
                        initialAttachSOSApn->password,
                          initialAttachId, initialAttachSOSApn->authtype);
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
                RLOGD("INITIAL_ATTACH_SOS_APN data is null");
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
        /* }@ */
        case RIL_EXT_REQUEST_SET_SINGLE_PDN: {
            s_singlePDNAllowed[socket_id] = ((int *)data)[0];
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;
        }
        case RIL_EXT_REQUEST_SET_XCAP_IP_ADDR: {
            char *index = ((char **)data)[0];
            char *ipv4 = ((char **)data)[1];
            char *ipv6 = ((char **)data)[2];
            char ipv6Address[PROPERTY_VALUE_MAX] = {0};
            RLOGD("index = %s", index);
            /* send IP for volte addtional business */
            if (s_isLTE && index != NULL) {
                char cmd[AT_COMMAND_LEN] = {0};
                if (ipv4 == NULL || strlen(ipv4) <= 0) {
                    ipv4 = "0.0.0.0";
                }
                if (ipv6 == NULL || strlen(ipv6) <= 0) {
                    ipv6 = "FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF:FFFF";
                }
                checkIpv6Address(ipv6, ipv6Address, sizeof(ipv6Address));
                snprintf(cmd, sizeof(cmd), "AT+XCAPIP=%d,\"%s,[%s]\"",
                          atoi(index) + 1, ipv4, ipv6Address);
                at_send_command(s_ATChannels[channelID], cmd, NULL);
            }
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;
        }
        case RIL_REQUEST_GET_IMS_PCSCF_ADDR: {
            ATLine *p_cur = NULL;
            char *input;
            char *sskip;
            char *tmp;
            char *pcscf_prim_addr = NULL;
            int skip;

            err = at_send_command_multiline(s_ATChannels[channelID], "AT+CGCONTRDP=11",
                                            "+CGCONTRDP:", &p_response);

            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                for (p_cur = p_response->p_intermediates; p_cur != NULL;
                     p_cur = p_cur->p_next) {
                    input = p_cur->line;
                    if (findInBuf(input, strlen(p_cur->line), "+CGCONTRDP")) {
                         err = at_tok_flag_start(&input, ':');
                         if (err < 0) break;

                         err = at_tok_nextint(&input, &skip);  // cid
                         if (err < 0) break;

                         err = at_tok_nextint(&input, &skip);  // bearer_id
                         if (err < 0) break;

                         err = at_tok_nextstr(&input, &sskip);  // apn
                         if (err < 0) break;

                         if (at_tok_hasmore(&input)) {
                             err = at_tok_nextstr(&input, &sskip);  // local_addr_and_subnet_mask
                             if (err < 0) break;

                             if (at_tok_hasmore(&input)) {
                                 err = at_tok_nextstr(&input, &sskip);  // gw_addr
                                 if (err < 0) break;

                                 if (at_tok_hasmore(&input)) {
                                     err = at_tok_nextstr(&input, &sskip);  // dns_prim_addr
                                     if (err < 0) break;

                                     if (at_tok_hasmore(&input)) {
                                         err = at_tok_nextstr(&input, &sskip);  // dns_sec_addr
                                         if (err < 0) break;

                                         if (at_tok_hasmore(&input)) {  // PCSCF_prim_addr
                                             err = at_tok_nextstr(&input, &pcscf_prim_addr);
                                             if (err < 0) break;
                                         }
                                     }
                                 }
                             }
                         }
                         if (pcscf_prim_addr != NULL) {
                             RIL_onRequestComplete(t, RIL_E_SUCCESS, pcscf_prim_addr, strlen(pcscf_prim_addr) + 1);
                         } else {
                             RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
                         }
                         AT_RESPONSE_FREE(p_response);
                         break;
                    }  // CGCONTRDP
                }  // for
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            }  // success
            AT_RESPONSE_FREE(p_response);
            break;
        }
        case RIL_REQUEST_SET_IMS_PCSCF_ADDR: {
            int err;
            char cmd[AT_COMMAND_LEN] = {0};
            const char *strings = (const char *)data;

            if (datalen > 0 && strings != NULL && strlen(strings) > 0) {
                snprintf(cmd, sizeof(cmd), "AT+VOWIFIPCSCF=%s", strings);
                err = at_send_command(s_ATChannels[channelID], cmd , NULL);
                if (err != 0) {
                    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
                } else {
                    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
                }
            } else {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            }
            break;
        }
        case RIL_EXT_REQUEST_REATTACH: {
            at_send_command(s_ATChannels[channelID], "AT+SPREATTACH", NULL);
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;
        }
        case RIL_REQUEST_IMS_REGADDR: {
            p_response = NULL;
            err = at_send_command_singleline(s_ATChannels[channelID], "AT+SPIMSREGADDR?",
                                             "+SPIMSREGADDR:", &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                int count = 2;
                char *imsAddr[2] = {NULL, NULL};
                char *line = p_response->p_intermediates->line;
                if (findInBuf(line, strlen(line), "+SPIMSREGADDR")) {
                    err = at_tok_flag_start(&line, ':');
                    if (err < 0) break;

                    err = at_tok_nextstr(&line, &imsAddr[0]);
                    if (err < 0) break;

                    err = at_tok_nextstr(&line, &imsAddr[1]);
                    if (err < 0) break;

                    if ((imsAddr[0] != NULL) && (imsAddr[1] != NULL)) {
                        RIL_onRequestComplete(t, RIL_E_SUCCESS, imsAddr, count * sizeof(char*));
                    } else {
                        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
                    }
                } else {
                    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
                }
            }
            at_response_free(p_response);
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
    if ((int)cbPara->socket_id < 0 || (int)cbPara->socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", cbPara->socket_id);
        return;
    }
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
    ATResponse *p_response = NULL;

    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    if ((int)socket_id < 0 || (int)socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        return;
    }

    RLOGD("startGSPS cid  %d, eth state: %d", s_GSCid, s_ethOnOff);
    channelID = getChannel(socket_id);
    if (s_ethOnOff) {
        property_set(GSPS_ETH_UP_PROP, "1");
        snprintf(cmd, sizeof(cmd), "AT+CGDATA=\"M-ETHER\", %d", s_GSCid);
        cgdata_set_cmd_req(cmd);
    } else {
        property_set(GSPS_ETH_DOWN_PROP, "1");
        snprintf(cmd, sizeof(cmd), "AT+CGACT=0, %d", s_GSCid);
    }

    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (s_ethOnOff) {
        cgdata_set_cmd_rsp(p_response, s_GSCid - 1, 0, channelID);
    } else {
        cgact_deact_cmd_rsp(s_GSCid);
    }

    putChannel(channelID);
    at_response_free(p_response);
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
    if ((int)socket_id < 0 || (int)socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        FREEMEMORY(cbPara->para);
        FREEMEMORY(cbPara);
        return;
    }

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

    for (p = line; *p != '\0'; p++) {
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
            updatePDPCid(socket_id, cid, pdpState);
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
            pthread_mutex_lock(&s_callMutex[socket_id]
            );
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
                        s_PDP[socket_id][cid - 1].state == PDP_BUSY) {
                        if (cid == s_curCid[socket_id]) {
                            s_LTEDetached[socket_id] = true;
                        }
                        CallbackPara *cbPara =
                                (CallbackPara *)malloc(sizeof(CallbackPara));
                        if (cbPara != NULL) {
                            cbPara->para = (int *)malloc(sizeof(int));
                            *((int *)(cbPara->para)) = cid;
                            cbPara->socket_id = socket_id;
                        }
                        if (s_openchannelInfo[cid - 1].state != CLOSE) {
                            RLOGD("sendEvenLoopThread cid:%d", cid);
                            s_openchannelInfo[cid - 1].cid = -1;
                            s_openchannelInfo[cid - 1].state = CLOSE;
                            int secondaryCid = getFallbackCid(socket_id, cid - 1);
                            putPDP(socket_id, secondaryCid - 1);
                            putPDP(socket_id, cid - 1);
                            RIL_requestTimedCallback(sendEvenLoopThread, cbPara, NULL);
                        }
                        s_openchannelInfo[cid - 1].count = 0;
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

        err = at_tok_nextint(&tmp, &errCode);
        if (err < 0) goto out;

        if (errCode == 336) {
            RIL_onUnsolicitedResponse(RIL_EXT_UNSOL_CLEAR_CODE_FALLBACK, NULL,
                                      0, socket_id);
        }
        if ((type == 5) && (s_ussdRun[socket_id] == 1)) { // 5: for SS
            s_ussdError[socket_id] = 1;
        } else if (type == 10) { // ps business in this sim is rejected by network
            RIL_onUnsolicitedResponse(RIL_EXT_UNSOL_SIM_PS_REJECT, NULL, 0,
                    socket_id);
        } else if (type == 1) {
            setProperty(socket_id, "ril.sim.ps.reject", "1");
            if ((errCode == 3) || (errCode == 6) || (errCode == 7)
                    || (errCode == 8) || (errCode == 14)) {
                RIL_onUnsolicitedResponse(RIL_EXT_UNSOL_SIM_PS_REJECT, NULL, 0,
                        socket_id);
            }
        }
    } else if (strStartsWith(s, "+SPSWAPCARD:")) {
        int id = 0;
        char *tmp;
        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);
        err = at_tok_nextint(&tmp, &id);
        if (id == 1) {
            s_swapCard = id;
        } else {
            s_swapCard = 0;
        }
        if (err < 0) {
            RLOGD("%s fail", s);
        }
        RIL_onUnsolicitedResponse (RIL_EXT_UNSOL_SWITCH_PRIMARY_CARD, (void *)&id, sizeof(int), socket_id);
    } else {
        ret = 0;
    }

out:
    free(line);
    return ret;
}

/*
 * phoneserver used to process these AT Commands or its response
 *    # AT+CGACT=0 set command response process
 *    # AT+CGDATA= set command process
 *    # AT+CGDATA= set command response process
 *    # AT+CGDCONT= set command response process
 *    # AT+CGDCONT? read response process
 *
 * since phoneserver has been removed, its function should be realized in RIL,
 * so when used AT+CGACT=0, AT+CGDATA=, AT+CGDATA=, AT+CGDCONT= and AT+CGDCONT?,
 * please make sure to call the corresponding process functions.
 */

void setSockTimeout() {
    struct timeval writetm, recvtm;
    writetm.tv_sec = 1;  // write timeout: 1s
    writetm.tv_usec = 0;
    recvtm.tv_sec = 10;  // recv timeout: 10s
    recvtm.tv_usec = 0;

    if (setsockopt(s_extDataFd, SOL_SOCKET, SO_SNDTIMEO, &writetm,
                     sizeof(writetm)) == -1) {
        RLOGE("WARNING: Cannot set send timeout value on socket: %s",
                 strerror(errno));
    }
    if (setsockopt(s_extDataFd, SOL_SOCKET, SO_RCVTIMEO, &recvtm,
                     sizeof(recvtm)) == -1) {
        RLOGE("WARNING: Cannot set receive timeout value on socket: %s",
                 strerror(errno));
    }
}

void *listenExtDataThread(void) {
    int retryTimes = 0;
    RLOGD("try to connect socket ext_data...");

    do {
        s_extDataFd = socket_local_client(SOCKET_NAME_EXT_DATA,
                ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
        usleep(10 * 1000);  // wait for 10ms, try 10 times
        retryTimes++;
    } while (s_extDataFd < 0 && retryTimes < 10);

    if (s_extDataFd >= 0) {
        RLOGD("connect to ext_data socket success!");
        setSockTimeout();
    } else {
        RLOGE("connect to ext_data socket failed!");
    }
    return NULL;
}

void sendCmdToExtData(char cmd[]) {
    int ret;
    int retryTimes = 0;

RECONNECT:
    retryTimes = 0;
    while (s_extDataFd < 0 && retryTimes < 10) {
        s_extDataFd = socket_local_client(SOCKET_NAME_EXT_DATA,
                ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
        usleep(10 * 1000);  // wait for 10ms, try again
        retryTimes++;
    }
    if (s_extDataFd >= 0 && retryTimes != 0) {
        setSockTimeout();
    }

    if (s_extDataFd >= 0) {
        int len = strlen(cmd) + 1;
        if (TEMP_FAILURE_RETRY(write(s_extDataFd, cmd, len)) !=
                                      len) {
            RLOGE("Failed to write cmd to ext_data!");
            close(s_extDataFd);
            s_extDataFd = -1;
        } else {
            int error;
            if (TEMP_FAILURE_RETRY(read(s_extDataFd, &error, sizeof(error)))
                    <= 0) {
                RLOGE("read error from ext_data!");
                close(s_extDataFd);
                s_extDataFd = -1;
            }
        }
    }
}

void ps_service_init() {
    int i;
    int ret;
    pthread_t tid;
    pthread_attr_t attr;

    memset(pdp_info, 0x0, sizeof(pdp_info));
    for (i = 0; i < MAX_PDP_NUM; i++) {
        pdp_info[i].state = PDP_STATE_IDLE;
    }

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&tid, &attr, (void *)listenExtDataThread, NULL);
    if (ret < 0) {
        RLOGE("Failed to create listen_ext_data_thread errno: %d", errno);
    }
}

int ifc_set_noarp(const char *ifname) {
    struct ifreq ifr;
    int fd, err;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }

    memset(&ifr, 0, sizeof(struct ifreq));
    strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);

    if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
        close(fd);
        return -1;
    }

    ifr.ifr_flags = ifr.ifr_flags | IFF_NOARP;
    err = ioctl(fd, SIOCSIFFLAGS, &ifr);
    close(fd);
    return err;
}

int getIPV6Addr(const char *prop, int cidIndex) {
    char netInterface[NET_INTERFACE_LENGTH] = {0};
    const int maxRetry = 120;  // wait 12s
    int retry = 0;
    int setup_success = 0;
    char cmd[AT_COMMAND_LEN] = {0};
    const int ipv6AddrLen = 32;

    snprintf(netInterface, sizeof(netInterface), "%s%d", prop, cidIndex);
    RLOGD("query interface %s", netInterface);
    while (!setup_success) {
        char rawaddrstr[INET6_ADDRSTRLEN], addrstr[INET6_ADDRSTRLEN];
        unsigned int prefixlen;
        int lasterror = 0, i, j, ret;
        char ifname[NET_INTERFACE_LENGTH];  // Currently, IFNAMSIZ = 16.
        FILE *f = fopen("/proc/net/if_inet6", "r");
        if (!f) {
            return -errno;
        }

        // Format:
        // 20010db8000a0001fc446aa4b5b347ed 03 40 00 01    wlan0
        while (fscanf(f, "%32s %*02x %02x %*02x %*02x %63s\n", rawaddrstr,
                &prefixlen, ifname) == 3) {
            // Is this the interface we're looking for?
            if (strcmp(netInterface, ifname)) {
                continue;
            }

            // Put the colons the address
            // and add ':' to separate every 4 addr char
            for (i = 0, j = 0; i < ipv6AddrLen; i++, j++) {
                addrstr[j] = rawaddrstr[i];
                if (i % 4 == 3) {
                    addrstr[++j] = ':';
                }
            }
            addrstr[j - 1] = '\0';
            RLOGD("getipv6addr found ip %s", addrstr);
            // Don't add the link-local address
            if (strncmp(addrstr, "fe80:", sizeof("fe80:") - 1) == 0) {
                RLOGD("getipv6addr found fe80");
                continue;
            }
            snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ipv6_ip %s", prop,
                      cidIndex, addrstr);
            system(cmd);
            RLOGD("getipv6addr propset %s ", cmd);
            setup_success = 1;
            break;
        }

        fclose(f);
        if (!setup_success) {
            usleep(100 * 1000);
            retry++;
        }
        if (retry == maxRetry) {
            break;
        }
    }
    return setup_success;
}

IPType readIPAddr(char *raw, char *rsp) {
    int comma_count = 0;
    int num = 0, comma4_num = 0, comma16_num = 0;
    int space_num = 0;
    char *buf = raw;
    int len = 0;
    int ip_type = UNKNOWN;

    if (raw != NULL) {
        len = strlen(raw);
        for (num = 0; num < len; num++) {
            if (raw[num] == '.') {
                comma_count++;
            }

            if (raw[num] == ' ') {
                space_num = num;
                break;
            }

            if (comma_count == 4 && comma4_num == 0) {
                comma4_num = num;
            }

            if (comma_count > 7 && comma_count == 16) {
                comma16_num = num;
                break;
            }
        }

        if (space_num > 0) {
            buf[space_num] = '\0';
            ip_type = IPV6;
            memcpy(rsp, buf, strlen(buf) + 1);
        } else if (comma_count >= 7) {
            if (comma_count == 7) {  // ipv4
                buf[comma4_num] = '\0';
                ip_type = IPV4;
            } else {  // ipv6
                buf[comma16_num] = '\0';
                ip_type = IPV6;
            }
            memcpy(rsp, buf, strlen(buf) + 1);
        }
    }

    return ip_type;
}

void resetDNS2(char *out, size_t dataLen) {
    if (strlen(s_SavedDns) > 0) {
        RLOGD("Use saved DNS2 instead.");
        memcpy(out, s_SavedDns, sizeof(s_SavedDns));
    } else {
        RLOGD("Use default DNS2 instead.");
        snprintf(out, dataLen, "%s", DEFAULT_PUBLIC_DNS2);
    }
}

/* for AT+CGDCONT? read response process */
void cgdcont_read_cmd_rsp(ATResponse *p_response, ATResponse **pp_outResponse) {
    int ret, err;
    int tmpCid = 0;
    int respLen;
    char *out;
    char atCmdStr[MAX_AT_RESPONSE], ip[IP_ADDR_MAX], net[IP_ADDR_MAX];

    if (p_response == NULL) {
        return;
    }

    char *line = NULL;
    char *newLine = NULL;
    ATLine *p_cur = NULL;
    ATResponse *sp_response = at_response_new();
    for (p_cur = p_response->p_intermediates; p_cur != NULL;
         p_cur = p_cur->p_next) {
        line = p_cur->line;
        respLen = strlen(line);
        snprintf(atCmdStr, sizeof(atCmdStr), "%s", line);
        if (findInBuf(line, respLen, "+CGDCONT")) {
            do {
                err = at_tok_start(&line);
                if (err < 0) break;

                err = at_tok_nextint(&line, &tmpCid);
                if (err < 0) break;

                err = at_tok_nextstr(&line, &out);  // ip
                if (err < 0) break;

                snprintf(ip, sizeof(ip), "%s", out);

                err = at_tok_nextstr(&line, &out);  // cmnet
                if (err < 0) break;

                snprintf(net, sizeof(net), "%s", out);

                if (tmpCid <= MAX_PDP_NUM) {
                    if (pdp_info[tmpCid - 1].state == PDP_STATE_ACTIVE) {
                        if (pdp_info[tmpCid - 1].manual_dns == 1) {
                            snprintf(atCmdStr, sizeof(atCmdStr),
                                "+CGDCONT:%d,\"%s\",\"%s\",\"%s\",0,0,\"%s\",\"%s\"\r",
                                tmpCid, ip, net, pdp_info[tmpCid].ipladdr,
                                pdp_info[tmpCid - 1].userdns1addr,
                                pdp_info[tmpCid - 1].userdns2addr);
                        } else {
                            snprintf(atCmdStr, sizeof(atCmdStr),
                                "+CGDCONT:%d,\"%s\",\"%s\",\"%s\",0,0,\"%s\",\"%s\"\r",
                                tmpCid, ip, net, pdp_info[tmpCid - 1].ipladdr,
                                pdp_info[tmpCid - 1].dns1addr,
                                pdp_info[tmpCid - 1].dns2addr);
                        }
                    } else {
                        snprintf(atCmdStr, sizeof(atCmdStr),
                                "+CGDCONT:%d,\"%s\",\"%s\",\"%s\",0,0,\"%s\",\"%s\"\r",
                                tmpCid, ip, net, "0.0.0.0", "0.0.0.0", "0.0.0.0");
                    }
                }
            } while (0);
        }
        reWriteIntermediate(sp_response, atCmdStr);
    }

    if (pp_outResponse == NULL) {
        at_response_free(sp_response);
    } else {
        reverseNewIntermediates(sp_response);
        *pp_outResponse = sp_response;
    }
}

/* for AT+CGDCONT= set command response process */
int cgdcont_set_cmd_req(char *cmd, char *newCmd) {
    int tmpCid = 0;
    char *input = cmd;
    char ip[IP_ADDR_MAX], net[IP_ADDR_MAX],
         ipladdr[IP_ADDR_MAX], hcomp[IP_ADDR_MAX], dcomp[IP_ADDR_MAX];
    char *out;
    int err = 0, ret = 0;
    int maxPDPNum = MAX_PDP_NUM;

    if (cmd == NULL || newCmd == NULL) {
        goto error;
    }

    memset(ip, 0, IP_ADDR_MAX);
    memset(net, 0, IP_ADDR_MAX);
    memset(ipladdr, 0, IP_ADDR_MAX);
    memset(hcomp, 0, IP_ADDR_MAX);
    memset(dcomp, 0, IP_ADDR_MAX);

    err = at_tok_flag_start(&input, '=');
    if (err < 0) goto error;

    err = at_tok_nextint(&input, &tmpCid);
    if (err < 0) goto error;

    err = at_tok_nextstr(&input, &out);  // ip
    if (err < 0) goto exit;

    snprintf(ip, sizeof(ip), "%s", out);

    err = at_tok_nextstr(&input, &out);  // cmnet
    if (err < 0) goto exit;

    snprintf(net, sizeof(net), "%s", out);

    err = at_tok_nextstr(&input, &out);  // ipladdr
    if (err < 0) goto exit;

    snprintf(ipladdr, sizeof(ipladdr), "%s", out);

    err = at_tok_nextstr(&input, &out);  // dcomp
    if (err < 0) goto exit;

    snprintf(dcomp, sizeof(dcomp), "%s", out);

    err = at_tok_nextstr(&input, &out);  // hcomp
    if (err < 0) goto exit;

    snprintf(hcomp, sizeof(hcomp), "%s", out);

    // cp dns to pdp_info ?
    if (tmpCid <= maxPDPNum) {
        strncpy(pdp_info[tmpCid - 1].userdns1addr, "0.0.0.0",
                 sizeof("0.0.0.0"));
        strncpy(pdp_info[tmpCid - 1].userdns2addr, "0.0.0.0",
                 sizeof("0.0.0.0"));
        pdp_info[tmpCid - 1].manual_dns = 0;
    }

    // dns1, info used with cgdata
    err = at_tok_nextstr(&input, &out);
    if (err < 0) goto exit;

    if (tmpCid <= maxPDPNum && *out != 0) {
        strncpy(pdp_info[tmpCid - 1].userdns1addr, out,
                sizeof(pdp_info[tmpCid - 1].userdns1addr));
        pdp_info[tmpCid - 1].userdns1addr[
                sizeof(pdp_info[tmpCid - 1].userdns1addr) - 1] = '\0';
    }

    // dns2, info used with cgdata
    err = at_tok_nextstr(&input, &out);
    if (err < 0) goto exit;

    if (tmpCid <= maxPDPNum && *out != 0) {
        strncpy(pdp_info[tmpCid - 1].userdns2addr, out,
                sizeof(pdp_info[tmpCid - 1].userdns2addr));
        pdp_info[tmpCid - 1].userdns2addr[
                sizeof(pdp_info[tmpCid - 1].userdns2addr)- 1] = '\0';
    }

    // cp dns to pdp_info?
exit:
    if (tmpCid <= maxPDPNum) {
        if (strncasecmp(pdp_info[tmpCid - 1].userdns1addr, "0.0.0.0",
                strlen("0.0.0.0"))) {
            pdp_info[tmpCid - 1].manual_dns = 1;
        }
    }

    snprintf(newCmd, AT_COMMAND_LEN,
            "AT+CGDCONT=%d,\"%s\",\"%s\",\"%s\",%s,%s\r", tmpCid, ip, net,
            ipladdr, dcomp, hcomp);

    return AT_RESULT_OK;

error:
    return AT_RESULT_NG;
}

/* for AT+CGDATA= set command process */
int cgdata_set_cmd_req(char *cgdataCmd) {
    int cid, pdpIndex;
    int err;
    char *cmdStr, *out;
    char atBuffer[MAX_AT_RESPONSE];

    if (cgdataCmd == NULL || strlen(cgdataCmd) <= 0) {
        goto error;
    }

    cmdStr = atBuffer;
    snprintf(cmdStr, sizeof(atBuffer), "%s", cgdataCmd);

    err = at_tok_flag_start(&cmdStr, '=');
    if (err < 0) goto error;

    /* get L2P */
    err = at_tok_nextstr(&cmdStr, &out);
    if (err < 0) goto error;

    /* Get cid */
    err = at_tok_nextint(&cmdStr, &cid);
    if (err < 0) goto error;

    pdpIndex = cid - 1;
    pthread_mutex_lock(&s_psServiceMutex);
    pdp_info[pdpIndex].state = PDP_STATE_ACTING;
    pdp_info[pdpIndex].cid = cid;
    pdp_info[pdpIndex].error_num = -1;
    pthread_mutex_unlock(&s_psServiceMutex);
    return AT_RESULT_OK;

error:
    return AT_RESULT_NG;
}

int downNetcard(int cid, char *netinterface) {
    int index = cid - 1;
    int isAutoTest = 0;
    char linker[AT_COMMAND_LEN] = {0};
    char cmd[AT_COMMAND_LEN];
    char gspsprop[PROPERTY_VALUE_MAX] = {0};

    if (cid < 1 || cid >= MAX_PDP_NUM || netinterface == NULL) {
        return 0;
    }
    RLOGD("down cid %d, network interface %s ", cid, netinterface);
    snprintf(linker, sizeof(linker), "%s%d", netinterface, index);
    if (ifc_disable(linker)) {
        RLOGE("ifc_disable %s fail: %s\n", linker, strerror(errno));
    }
    if (ifc_clear_addresses(linker)) {
        RLOGE("ifc_clear_addresses %s fail: %s\n", linker, strerror(errno));
    }

    property_get(GSPS_ETH_DOWN_PROP, gspsprop, "0");
    isAutoTest = atoi(gspsprop);

    snprintf(cmd, sizeof(cmd), "<ifdown>%s;%s;%d", linker, "IPV4V6",
             isAutoTest);
    sendCmdToExtData(cmd);
    property_set(GSPS_ETH_DOWN_PROP, "0");

    RLOGD("data_off execute done");
    return 1;
}

int dispose_data_fallback(int masterCid, int secondaryCid) {
    int master_index = masterCid - 1;
    int secondary_index = secondaryCid - 1;
    char cmd[AT_COMMAND_LEN];
    char prop[PROPERTY_VALUE_MAX];
    char ETH_SP[PROPERTY_NAME_MAX];  // "ro.modem.*.eth"
    int count = 0;

    if (masterCid < 1|| masterCid >= MAX_PDP_NUM || secondaryCid <1 ||
        secondaryCid >= MAX_PDP_NUM) {
    // 1~11 is valid cid
        return 0;
    }
    snprintf(ETH_SP, sizeof(ETH_SP), "ro.modem.%s.eth", s_modem);
    property_get(ETH_SP, prop, "veth");
    RLOGD("master ip type %d, secondary ip type %d",
             pdp_info[master_index].ip_state,
             pdp_info[secondary_index].ip_state);
    // fallback get same type ip with master
    if (pdp_info[master_index].ip_state ==
        pdp_info[secondary_index].ip_state) {
        return 0;
    }
    if (pdp_info[master_index].ip_state == IPV4) {
        // down ipv4, because need set ipv6 firstly
        downNetcard(masterCid, prop);
        // copy secondary ppp to master ppp
        memcpy(pdp_info[master_index].ipv6laddr,
                pdp_info[secondary_index].ipv6laddr,
                sizeof(pdp_info[master_index].ipv6laddr));
        memcpy(pdp_info[master_index].ipv6dns1addr,
                pdp_info[secondary_index].ipv6dns1addr,
                sizeof(pdp_info[master_index].ipv6dns1addr));
        memcpy(pdp_info[master_index].ipv6dns2addr,
                pdp_info[secondary_index].ipv6dns2addr,
                sizeof(pdp_info[master_index].ipv6dns2addr));
        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ipv6_ip %s", prop,
                  master_index, pdp_info[master_index].ipv6laddr);
        system(cmd);
        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ipv6_dns1 %s", prop,
                  master_index, pdp_info[master_index].ipv6dns1addr);
        system(cmd);
        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ipv6_dns2 %s", prop,
                  master_index, pdp_info[master_index].ipv6dns2addr);
        system(cmd);
    } else if (pdp_info[master_index].ip_state == IPV6) {
        // copy secondary ppp to master ppp
        memcpy(pdp_info[master_index].ipladdr,
                pdp_info[secondary_index].ipladdr,
                sizeof(pdp_info[master_index].ipladdr));
        memcpy(pdp_info[master_index].dns1addr,
                pdp_info[secondary_index].dns1addr,
                sizeof(pdp_info[master_index].dns1addr));
        memcpy(pdp_info[master_index].dns2addr,
                pdp_info[secondary_index].dns2addr,
                sizeof(pdp_info[master_index].dns2addr));
        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ip %s", prop,
                  master_index, pdp_info[master_index].ipladdr);
        system(cmd);
        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.dns1 %s", prop,
                  master_index, pdp_info[master_index].dns1addr);
        system(cmd);
        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.dns2 %s", prop,
                  master_index, pdp_info[secondary_index].dns2addr);
        system(cmd);
    }
    snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ip_type %d", prop,
              master_index, IPV4V6);
    system(cmd);
    pdp_info[master_index].ip_state = IPV4V6;
    return 1;
}

/*
 * return value: 1: success
 *               0: getIpv6 header 64bit failed
 */
static int upNetInterface(int cidIndex, IPType ipType) {
    char linker[AT_COMMAND_LEN] = {0};
    char prop[PROPERTY_VALUE_MAX] = {0};
    char ETH_SP[PROPERTY_NAME_MAX] = {0};  // "ro.modem.*.eth"
    char gspsprop[PROPERTY_VALUE_MAX] = {0};
    char cmd[AT_COMMAND_LEN];
    IPType actIPType = ipType;
    int isAutoTest = 0;
    int err = -1;

    char ip[IP_ADDR_MAX], ip2[IP_ADDR_MAX], dns1[IP_ADDR_MAX], dns2[IP_ADDR_MAX];
    memset(ip, 0, sizeof(ip));
    memset(ip2, 0, sizeof(ip2));
    memset(dns1, 0, sizeof(dns1));
    memset(dns2, 0, sizeof(dns2));
    snprintf(ETH_SP, sizeof(ETH_SP), "ro.modem.%s.eth", s_modem);
    property_get(ETH_SP, prop, "veth");

    /* set net interface name */
    snprintf(linker, sizeof(linker), "%s%d", prop, cidIndex);
    property_set(SYS_NET_ADDR, linker);
    RLOGD("Net interface addr linker = %s", linker);

    property_get(GSPS_ETH_UP_PROP, gspsprop, "0");
    RLOGD("GSPS up prop = %s", gspsprop);
    isAutoTest = atoi(gspsprop);
    RLOGD("ipType = %d", ipType);

    if (ipType != IPV4) {
        actIPType = IPV6;
    }
    do {
        if (actIPType == IPV6) {
            property_set(SYS_IPV6_LINKLOCAL, pdp_info[cidIndex].ipv6laddr);
        }
        snprintf(cmd, sizeof(cmd), "<preifup>%s;%s;%d", linker,
                  actIPType == IPV4 ? "IPV4" : "IPV6", isAutoTest);
        sendCmdToExtData(cmd);

        /* config ip addr */
        if (actIPType != IPV4) {
            property_set(SYS_NET_ACTIVATING_TYPE, "IPV6");
            err = ifc_add_address(linker, pdp_info[cidIndex].ipv6laddr, 64);
        } else {
            property_set(SYS_NET_ACTIVATING_TYPE, "IPV4");
            err = ifc_add_address(linker, pdp_info[cidIndex].ipladdr, 32);
        }
        if (err != 0) {
            RLOGE("ifc_add_address %s fail: %s\n", linker, strerror(errno));
        }

        if (ifc_set_noarp(linker)) {
            RLOGE("ifc_set_noarp %s fail: %s\n", linker, strerror(errno));
        }

        /* up the net interface */
        if (ifc_enable(linker)) {
            RLOGE("ifc_enable %s fail: %s\n", linker, strerror(errno));
        }

        snprintf(cmd, sizeof(cmd), "<ifup>%s;%s;%d", linker,
                  actIPType == IPV4 ? "IPV4" : "IPV6", isAutoTest);
        sendCmdToExtData(cmd);

        /* Get IPV6 Header 64bit */
        if (actIPType != IPV4) {
            if (!getIPV6Addr(prop, cidIndex)) {
                RLOGD("get IPv6 address timeout, actIPType = %d", actIPType);
                if (ipType == IPV4V6) {
                    pdp_info[cidIndex].ip_state = IPV4;
                } else {
                    return 0;
                }
            }
        }

        /* if IPV4V6 actived, need set IPV4 again */
        if (ipType == IPV4V6 && actIPType != IPV4) {
            actIPType = IPV4;
        } else {
            break;
        }
    } while (ipType == IPV4V6);

    char bip[PROPERTY_VALUE_MAX];
    memset(bip, 0, sizeof(bip));
    property_get(BIP_OPENCHANNEL, bip, "0");
    if (strcmp(bip, "1") == 0) {
        if (ipType == IPV4) {
            snprintf(cmd, sizeof(cmd),"net.%s.ip", linker);
            property_get(cmd, ip, "");
            snprintf(cmd, sizeof(cmd),"net.%s.dns1", linker);
            property_get(cmd, dns1, "");
            snprintf(cmd, sizeof(cmd),"net.%s.dns2", linker);
            property_get(cmd, dns2, "");
            in_addr_t address = inet_addr(ip);
            err = ifc_create_default_route(linker, address);
            RLOGD("ifc_create_default_route address = %d, error = %d", address, err);
        } else if (ipType == IPV6) {
            snprintf(cmd, sizeof(cmd),"net.%s.ipv6_ip", linker);
            property_get(cmd, ip2, "");
            snprintf(cmd, sizeof(cmd),"net.%s.ipv6_dns1", linker);
            property_get(cmd, dns1, "");
            snprintf(cmd, sizeof(cmd),"net.%s.ipv6_dns2", linker);
            property_get(cmd, dns2, "");
            property_set("persist.sys.bip.ipv6_addr", ip2);
            int tableIndex = 0;
            ifc_init();
            RLOGD("linker = %s", linker);
            err = ifc_get_ifindex(linker, &tableIndex);
            RLOGD("index = %d, error = %d", tableIndex, err);
            ifc_close();
            tableIndex = tableIndex + 1000;
            sprintf(cmd, "setprop persist.sys.bip.table_index %d", tableIndex);
            system(cmd);
            property_set("ctl.start", "stk");
        } else {
            snprintf(cmd, sizeof(cmd),"net.%s.ip", linker);
            property_get(cmd, ip, "");
            snprintf(cmd, sizeof(cmd),"net.%s.ipv6_ip", linker);
            property_get(cmd, ip2, "");
            snprintf(cmd, sizeof(cmd),"net.%s.dns1", linker);
            property_get(cmd, dns1, "");
            snprintf(cmd, sizeof(cmd),"net.%s.ipv6_dns1", linker);
            property_get(cmd, dns2, "");
        }
        //snprintf(cmd, sizeof(cmd), "ip route add default via %s %s dev %s", ip, ip2, linker);
        //RLOGD("cmd = %s", cmd);
        //system(cmd);
        //snprintf(cmd, sizeof(cmd), "ndc resolver setnetdns %s \"\" %s %s", linker, dns1, dns2);
        //RLOGD("cmd = %s", cmd);
        system(cmd);
    }
    property_set(GSPS_ETH_UP_PROP, "0");

    return 1;
}

int cgcontrdp_set_cmd_rsp(ATResponse *p_response) {
    int err;
    char *input;
    int cid;
    char *local_addr_subnet_mask = NULL, *gw_addr = NULL;
    char *dns_prim_addr = NULL, *dns_sec_addr = NULL;
    char ip[IP_ADDR_SIZE * 4], dns1[IP_ADDR_SIZE * 4], dns2[IP_ADDR_SIZE * 4];
    char cmd[AT_COMMAND_LEN];
    char prop[PROPERTY_VALUE_MAX];
    int count = 0;
    char *sskip;
    char *tmp;
    int skip;
    int ip_type_num = 0;
    int ip_type;
    int maxPDPNum = MAX_PDP_NUM;
    char ETH_SP[PROPERTY_NAME_MAX];  // "ro.modem.*.eth"
    ATLine *p_cur = NULL;

    if (p_response == NULL) {
        RLOGE("leave cgcontrdp_set_cmd_rsp:AT_RESULT_NG");
        return AT_RESULT_NG;
    }

    memset(ip, 0, sizeof(ip));
    memset(dns1, 0, sizeof(dns1));
    memset(dns2, 0, sizeof(dns2));

    snprintf(ETH_SP, sizeof(ETH_SP), "ro.modem.%s.eth", s_modem);
    property_get(ETH_SP, prop, "veth");

    for (p_cur = p_response->p_intermediates; p_cur != NULL;
         p_cur = p_cur->p_next) {
        input = p_cur->line;
        if (findInBuf(input, strlen(p_cur->line), "+CGCONTRDP") ||
            findInBuf(input, strlen(p_cur->line), "+SIPCONFIG")) {
            do {
                err = at_tok_flag_start(&input, ':');
                if (err < 0) break;

                err = at_tok_nextint(&input, &cid);  // cid
                if (err < 0) break;

                err = at_tok_nextint(&input, &skip);  // bearer_id
                if (err < 0) break;

                err = at_tok_nextstr(&input, &sskip);  // apn
                if (err < 0) break;

                if (at_tok_hasmore(&input)) {
                    // local_addr_and_subnet_mask
                    err = at_tok_nextstr(&input, &local_addr_subnet_mask);
                    if (err < 0) break;

                    if (at_tok_hasmore(&input)) {
                        err = at_tok_nextstr(&input, &sskip);  // gw_addr
                        if (err < 0) break;

                        if (at_tok_hasmore(&input)) {
                            // dns_prim_addr
                            err = at_tok_nextstr(&input, &dns_prim_addr);
                            if (err < 0) break;

                            snprintf(dns1, sizeof(dns1), "%s", dns_prim_addr);

                            if (at_tok_hasmore(&input)) {
                                // dns_sec_addr
                                err = at_tok_nextstr(&input, &dns_sec_addr);
                                if (err < 0) break;

                                snprintf(dns2, sizeof(dns2), "%s", dns_sec_addr);
                            }
                        }
                    }
                }

                if ((cid < maxPDPNum) && (cid >= 1)) {
                    ip_type = readIPAddr(local_addr_subnet_mask, ip);
                    RLOGD("PS:cid = %d,ip_type = %d,ip = %s,dns1 = %s,dns2 = %s",
                             cid, ip_type, ip, dns1, dns2);

                    if (ip_type == IPV6) {  // ipv6
                        RLOGD("cgcontrdp_set_cmd_rsp: IPV6");
                        if (!strncasecmp(ip, "0000:0000:0000:0000",
                                strlen("0000:0000:0000:0000"))) {
                            // incomplete address
                            tmp = strchr(ip, ':');
                            if (tmp != NULL) {
                                snprintf(ip, sizeof(ip), "FE80%s", tmp);
                            }
                        }
                        memcpy(pdp_info[cid - 1].ipv6laddr, ip,
                                sizeof(pdp_info[cid - 1].ipv6laddr));
                        memcpy(pdp_info[cid - 1].ipv6dns1addr, dns1,
                                sizeof(pdp_info[cid - 1].ipv6dns1addr));

                        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ip_type %d",
                                  prop, cid - 1, IPV6);
                        system(cmd);
                        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ipv6_ip %s",
                                  prop, cid - 1, ip);
                        system(cmd);
                        snprintf(cmd, sizeof(cmd),
                                  "setprop net.%s%d.ipv6_dns1 %s", prop, cid - 1,
                                  dns1);
                        system(cmd);
                        if (strlen(dns2) != 0) {
                            if (!strcmp(dns1, dns2)) {
                                if (strlen(s_SavedDns_IPV6) > 0) {
                                    RLOGD("Use saved DNS2 instead.");
                                    memcpy(dns2, s_SavedDns_IPV6,
                                            sizeof(s_SavedDns_IPV6));
                                } else {
                                    RLOGD("Use default DNS2 instead.");
                                    snprintf(dns2, sizeof(dns2), "%s",
                                              DEFAULT_PUBLIC_DNS2_IPV6);
                                }
                            } else {
                                RLOGD("Backup DNS2");
                                memset(s_SavedDns_IPV6, 0,
                                        sizeof(s_SavedDns_IPV6));
                                memcpy(s_SavedDns_IPV6, dns2, sizeof(dns2));
                            }
                        } else {
                            RLOGD("DNS2 is empty!!");
                            memset(dns2, 0, IP_ADDR_SIZE * 4);
                            snprintf(dns2, sizeof(dns2), "%s",
                                      DEFAULT_PUBLIC_DNS2_IPV6);
                        }
                        memcpy(pdp_info[cid - 1].ipv6dns2addr, dns2,
                                sizeof(pdp_info[cid - 1].ipv6dns2addr));
                        snprintf(cmd, sizeof(cmd),
                                  "setprop net.%s%d.ipv6_dns2 %s", prop, cid - 1,
                                  dns2);
                        system(cmd);

                        pdp_info[cid - 1].ip_state = IPV6;
                        ip_type_num++;
                    } else if (ip_type == IPV4) {  // ipv4
                        RLOGD("cgcontrdp_set_cmd_rsp: IPV4");
                        memcpy(pdp_info[cid - 1].ipladdr, ip,
                                sizeof(pdp_info[cid - 1].ipladdr));
                        memcpy(pdp_info[cid - 1].dns1addr, dns1,
                                sizeof(pdp_info[cid - 1].dns1addr));

                        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ip_type %d",
                                  prop, cid - 1, IPV4);
                        system(cmd);
                        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ip %s", prop,
                                  cid - 1, ip);
                        system(cmd);
                        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.dns1 %s",
                                  prop, cid - 1, dns1);
                        system(cmd);
                        if (strlen(dns2) != 0) {
                            if (!strcmp(dns1, dns2)) {
                                RLOGD("Two DNS are the same, so need to reset"
                                         "dns2!!");
                                resetDNS2(dns2, sizeof(dns2));
                            } else {
                                RLOGD("Backup DNS2");
                                memset(s_SavedDns, 0, sizeof(s_SavedDns));
                                memcpy(s_SavedDns, dns2, IP_ADDR_SIZE);
                            }
                        } else {
                            RLOGD("DNS2 is empty!!");
                            memset(dns2, 0, IP_ADDR_SIZE);
                            resetDNS2(dns2, sizeof(dns2));
                        }
                        memcpy(pdp_info[cid - 1].dns2addr, dns2,
                                sizeof(pdp_info[cid - 1].dns2addr));
                        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.dns2 %s",
                                  prop, cid - 1, dns2);
                        system(cmd);

                        pdp_info[cid - 1].ip_state = IPV4;
                        ip_type_num++;
                    } else {  // unknown
                        pdp_info[cid - 1].state = PDP_STATE_EST_UP_ERROR;
                        RLOGD("PDP_STATE_EST_UP_ERROR: unknown ip type!");
                    }

                    if (ip_type_num > 1) {
                        RLOGD("cgcontrdp_set_cmd_rsp is IPV4V6, s_pdpType = %d", s_pdpType);
                        pdp_info[cid - 1].ip_state = s_pdpType;
                        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ip_type %d",
                                 prop, cid - 1, s_pdpType);
                        system(cmd);
                        s_pdpType = IPV4V6;
                    }
                    pdp_info[cid - 1].state = PDP_STATE_ACTIVE;
                    RLOGD("PDP_STATE_ACTIVE");
                }
            } while (0);
        }
    }
    return AT_RESULT_OK;
}

/* for AT+CGDATA= set command response process */
int cgdata_set_cmd_rsp(ATResponse *p_response, int pdpIndex, int primaryCid,
                       int channelID) {
    int rspType;
    char cmd[AT_COMMAND_LEN];
    char errStr[ARRAY_SIZE];
    char atCmdStr[AT_COMMAND_LEN];
    char *input;
    int err, error_num;
    ATResponse *p_rdpResponse = NULL;
    ATResponse *p_cgactResponse = NULL;

    if (p_response == NULL) {
        return AT_RESULT_NG;
    }
    if (pdpIndex < 0 || pdpIndex >= MAX_PDP_NUM) {
        return AT_RESULT_NG;
    }

    int cid = pdp_info[pdpIndex].cid;

    pthread_mutex_lock(&s_psServiceMutex);
    rspType = getATResponseType(p_response->finalResponse);
    if (rspType == AT_RSP_TYPE_CONNECT) {
        pdp_info[pdpIndex].state = PDP_STATE_CONNECT;
    } else if (rspType == AT_RSP_TYPE_ERROR) {
        RLOGE("PDP activate error");
        pdp_info[pdpIndex].state = PDP_STATE_ACT_ERROR;
        input = p_response->finalResponse;
        if (strStartsWith(input, "+CME ERROR:")) {
            err = at_tok_flag_start(&input, ':');
            if (err >= 0) {
                err = at_tok_nextint(&input, &error_num);
                if (err >= 0) {
                    if (error_num >= 0)
                        pdp_info[pdpIndex].error_num = error_num;
                }
            }
        }
    } else {
        goto error;
    }

    if (pdp_info[pdpIndex].state != PDP_STATE_CONNECT) {
        RLOGE("PDP activate error: %d", pdp_info[pdpIndex].state);
        // p_response->finalResponse stay unchanged
        pdp_info[pdpIndex].state = PDP_STATE_IDLE;
    } else {  // PDP_STATE_CONNECT
        pdp_info[pdpIndex].state = PDP_STATE_ESTING;
        pdp_info[pdpIndex].manual_dns = 0;

        if (s_isLTE) {
            snprintf(atCmdStr, sizeof(atCmdStr), "AT+CGCONTRDP=%d", cid);
            err = at_send_command_multiline(s_ATChannels[channelID], atCmdStr,
                                            "+CGCONTRDP:", &p_rdpResponse);
        } else {
            snprintf(atCmdStr, sizeof(atCmdStr), "AT+SIPCONFIG=%d", cid);
            err = at_send_command_multiline(s_ATChannels[channelID], atCmdStr,
                                            "+SIPCONFIG:", &p_rdpResponse);
        }
        if (err == AT_ERROR_TIMEOUT) {
            AT_RESPONSE_FREE(p_rdpResponse);
            RLOGE("Get IP address timeout");
            pdp_info[pdpIndex].state = PDP_STATE_DEACTING;
            snprintf(atCmdStr, sizeof(atCmdStr), "AT+CGACT=0,%d", cid);
            err = at_send_command(s_ATChannels[channelID], atCmdStr, NULL);
            if (err == AT_ERROR_TIMEOUT) {
                RLOGE("PDP deactivate timeout");
                goto error;
            }
        } else {
            cgcontrdp_set_cmd_rsp(p_rdpResponse);
            AT_RESPONSE_FREE(p_rdpResponse);
        }

        if (pdp_info[pdpIndex].state == PDP_STATE_ACTIVE) {
            RLOGD("PS connected successful");

            // if fallback, need map ipv4 and ipv6 to one net device
            if (dispose_data_fallback(primaryCid, cid)) {
                cid = primaryCid;
                pdpIndex = cid - 1;
            }

            RLOGD("PS ip_state = %d", pdp_info[pdpIndex].ip_state);
            if (upNetInterface(pdpIndex, pdp_info[pdpIndex].ip_state) == 0) {
                RLOGE("get IPv6 address timeout ");
                goto error;
            }
            RLOGD("data_on execute done");
        }
    }

    pthread_mutex_unlock(&s_psServiceMutex);
    return AT_RESULT_OK;

error:
    pthread_mutex_unlock(&s_psServiceMutex);
    free(p_response->finalResponse);
    p_response->finalResponse = strdup("ERROR");
    return AT_RESULT_NG;
}

/* for AT+CGACT=0 set command response process */
void cgact_deact_cmd_rsp(int cid) {
    char cmd[AT_COMMAND_LEN];
    char prop[PROPERTY_VALUE_MAX];
    char ETH_SP[PROPERTY_NAME_MAX];  // "ro.modem.*.eth"
    char ipv6_dhcpcd_cmd[AT_COMMAND_LEN] = {0};

    pthread_mutex_lock(&s_psServiceMutex);
    /* deactivate PDP connection */
    pdp_info[cid - 1].state = PDP_STATE_IDLE;

//    usleep(200 * 1000);
    snprintf(ETH_SP, sizeof(ETH_SP), "ro.modem.%s.eth", s_modem);
    property_get(ETH_SP, prop, "veth");
    downNetcard(cid, prop);

    if (pdp_info[cid - 1].ip_state == IPV6 ||
        pdp_info[cid - 1].ip_state == IPV4V6) {
        snprintf(ipv6_dhcpcd_cmd, sizeof(ipv6_dhcpcd_cmd),
                "dhcpcd_ipv6:%s%d", prop, cid - 1);
        property_set("ctl.stop", ipv6_dhcpcd_cmd);
    }

    snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ip_type %d", prop,
            cid - 1, UNKNOWN);
    system(cmd);

    pthread_mutex_unlock(&s_psServiceMutex);
}

int requestSetupDataConnection(int channelID, void *data, size_t datalen) {
    int i;
    int cid = 0;
    const char *pdpType = "IP";
    const char *apn = NULL;
    apn = ((const char **)data)[2];
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    RLOGD("s_dataAllowed[%d] = %d", socket_id, s_dataAllowed[socket_id]);
    if (s_dataAllowed[socket_id] != 1) {
        return -1;
    }
    if (datalen > 6 * sizeof(char *)) {
        pdpType = ((const char **)data)[6];
    } else {
        pdpType = "IP";
    }
    property_set(BIP_OPENCHANNEL, "1");
    s_openchannelCid = -1;
    queryAllActivePDNInfos(channelID);
    if (s_activePDN > 0) {
        for (i = 0; i < MAX_PDP; i++) {
            cid = getPDNCid(i);
            if (cid == (i + 1)) {
                RLOGD("s_PDP[%d].state = %d", i, getPDPState(socket_id, i));
                if (getPDPState(socket_id, i) == PDP_BUSY &&
                    isApnEqual((char *)apn, getPDNAPN(i)) &&
                    isBipProtocolEqual((char *)pdpType, getPDNIPType(i))) {
                    if (!(s_openchannelInfo[i].pdpState)) {
                        pthread_mutex_lock(&s_signalBipPdpMutex);
                        pthread_cond_wait(&s_signalBipPdpCond, &s_signalBipPdpMutex);
                        pthread_mutex_unlock(&s_signalBipPdpMutex);
                    }
                    s_openchannelInfo[i].cid = cid;
                    s_openchannelInfo[i].state = REUSE;
                    s_openchannelInfo[i].count++;
                    return s_openchannelInfo[i].cid;
                }
            }
        }
    }

    requestSetupDataCall(channelID, data, datalen, NULL);
    RLOGD("open channel cid = %d", s_openchannelCid);
    if (s_openchannelCid > 0) {
        i = s_openchannelCid -1;
        s_openchannelInfo[i].cid = s_openchannelCid;
        s_openchannelInfo[i].state = OPEN;
    }
    return s_openchannelCid;
}

void requestDeactiveDataConnection(int channelID, void *data, size_t datalen) {
    const char *p_cid = NULL;
    int cid;
    p_cid = ((const char **)data)[0];
    cid = atoi(p_cid);
    RLOGD("close channel cid = %d", cid);
    property_set(BIP_OPENCHANNEL, "0");
    if (cid > 0) {
        RLOGD("close channel state = %d", s_openchannelInfo[cid - 1].state);
        s_openchannelInfo[cid - 1].cid = -1;
        s_openchannelInfo[cid - 1].state = CLOSE;
        deactivateDataConnection(channelID, data, datalen, NULL);
    }
}

