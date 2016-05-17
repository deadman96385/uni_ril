/**
 * ril_data.h --- Data-related requests
 *                process functions/struct/variables declaration and definition
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef RIL_DATA_H_
#define RIL_DATA_H_

#define MAX_PDP                         6
#define MAX_PDP_CP                      11
#define MINIMUM_APN_LEN                 19

#define DATA_ACTIVE_FALLBACK_FAILED     -1
#define DATA_ACTIVE_SUCCESS             0
#define DATA_ACTIVE_FAILED              1
#define DATA_ACTIVE_NEED_RETRY          2

#define GSPS_ETH_UP_PROP                "ril.gsps.eth.up"
#define GSPS_ETH_DOWN_PROP              "ril.gsps.eth.down"
#define ATTACH_ENABLE_PROP              "persist.sys.attach.enable"

// Default MTU value
#define DEFAULT_MTU 1500
enum IPType {
    UNKNOWN = 0,
    IPV4    = 1,
    IPV6    = 2,
    IPV4V6  = 3
};

enum PDPState {
    PDP_IDLE,
    PDP_BUSY,
};

struct PDPInfo {
    int cid;
    int secondary_cid;  // for fallback cid
    bool isPrimary;
    enum PDPState state;
    pthread_mutex_t mutex;
};

typedef struct {
    int nCid;
    char strIPType[64];
    char strApn[64];
} PDNInfo;

/* data call fail cause mapping */
typedef enum {
    MN_GPRS_ERR_OPERATOR_DETERMINE_BAR             = 0x08,
    MN_GPRS_ERR_LLC_SND_FAILURE                    = 0x19,
    MN_GPRS_ERR_INSUFF_RESOURCE                    = 0x1A,
    MN_GPRS_ERR_UNKNOWN_APN                        = 0x1B,
    MN_GPRS_ERR_UNKNOWN_PDP_ADDR_OR_TYPE           = 0x1C,
    MN_GPRS_ERR_AUTHENTICATION_FAILURE             = 0x1D,
    MN_GPRS_ERR_ACTIVATION_REJ_GGSN                = 0x1E,
    MN_GPRS_ERR_ACTIVATION_REJ                     = 0x1F,
    MN_GPRS_ERR_UNSUPPORTED_SERVICE_OPTION         = 0x20,
    MN_GPRS_ERR_UNSUBSCRIBED_SERVICE_OPTION        = 0x21,
    MN_GPRS_ERR_OUT_OF_ORDER_SERVICE_OPTION        = 0x22,
    MN_GPRS_ERR_NSAPI_ALREADY_USED                 = 0x23,
    MN_GPRS_ERR_REGULAR_DEACTIVATION               = 0x24,
    MN_GPRS_ERR_QOS_NOT_ACCEPTED                   = 0x25,
    MN_GPRS_ERR_NETWORK_FAIL                       = 0x26,
    MN_GPRS_ERR_REACTIVATION_REQD                  = 0x27,
    MN_GPRS_ERR_FEATURE_NOT_SUPPORTED              = 0x28,
    MN_GPRS_ERR_SEMANTIC_ERROR_IN_TFT_OP           = 0x29,
    MN_GPRS_ERR_SYNTACTICAL_ERROR_IN_TFT_OP        = 0x2A,
    MN_GPRS_ERR_UNKNOWN_PDP_CONTEXT                = 0x2B,
    MN_GPRS_ERR_SEMANTIC_ERROR_IN_PACKET_FILTER    = 0x2C,
    MN_GPRS_ERR_SYNTAX_ERROR_IN_PACKET_FILTER      = 0x2D,
    MN_GPRS_ERR_PDP_CONTEXT_WO_TFT_ALREADY_ACT     = 0x2E,
    MN_GPRS_ERR_SM_ERR_UNSPECIFIED                 = 0x2F,
    MN_GPRS_ERR_INVALID_TI                         = 0x51,
    MN_GPRS_ERR_INCORRECT_MSG                      = 0x5F,
    MN_GPRS_ERR_INVALID_MAND_INFO                  = 0x60,
    MN_GPRS_ERR_UNIMPLE_MSG_TYPE                   = 0x61,
    MN_GPRS_ERR_INCOMPAT_MSG_TYP_PROTO_STAT        = 0x62,
    MN_GPRS_ERR_UNIMPLE_IE                         = 0x63,
    MN_GPRS_ERR_CONTEXT_CAUSE_CONDITIONAL_IE_ERROR = 0x64,
    MN_GPRS_ERR_INCOMP_MSG_PROTO_STAT              = 0x65,
    MN_GPRS_ERR_UNSPECIFIED                        = 0x6F,
    MN_GPRS_ERR_STARTUP_FAILURE                    = 0x70,
    MN_GPRS_ERR_START                              = 0xff,
    MN_GPRS_ERR_PRIMITIVE,
    MN_GPRS_ERR_MEM_ALLOC,
    MN_GPRS_ERR_NO_NSAPI,
    MN_GPRS_ERR_SENDER,
    MN_GPRS_ERR_PDP_TYPE,
    MN_GPRS_ERR_ATC_PARAM,
    MN_GPRS_ERR_PDP_ID,
    MN_GPRS_ERR_SPACE_NOT_ENOUGH,
    MN_GPRS_ERR_ACTIVE_REJCET,
    MN_GPRS_ERR_SAME_PDP_CONTEXT,
    MN_GPRS_ERR_NSAPI,
    MN_GPRS_ERR_MODIFY_REJ,
    MN_GPRS_ERR_READ_TYPE,
    MN_GPRS_ERR_PDP_CONTEXT_ACTIVATED,
    MN_GPRS_ERR_NO_PDP_CONTEXT,
    MN_GPRS_ERR_PERMENANT_PROBLEM,
    MN_GPRS_ERR_TEMPORARILY_BLOCKED,
    MN_GPRS_ERR_RETRYING,
    MN_GPRS_ERR_UNKNOWN_ERROR,
    MN_GPRS_ERR_SERVICE_OPTION_NOT_SUPPORTED,
    MN_GPRS_ERR_REQUEST_SERVICE_OPTION_NOT_SUBSCRIBED,
    MN_GPRS_ERR_SERVICE_OPTION_OUTOF_ORDER,
    MN_GPRS_ERR_PDP_AUTHENTICATION_FAILED,
    MN_GPRS_ERR_MISSING_OR_UNKOWN_APN,
    MN_GPRS_ERR_OPERATION_NOT_ALLOWED,
    MN_GPRS_ERR_NO_SATISFIED_RESOURCE,
} RIL_CP_DataCallFailCause;

extern int s_dataAllowed[SIM_COUNT];
extern bool s_LTEDetached[SIM_COUNT];
extern struct PDPInfo s_PDP[MAX_PDP];

void putPDP(int cid);
int isExistActivePdp();
int processDataRequest(int request, void *data, size_t datalen, RIL_Token t,
                          int channelID);
int processDataUnsolicited(RIL_SOCKET_ID socket_ID, const char *s);

#endif  // RIL_DATA_H_
