/**
 * ril_data.h --- Data-related requests
 *                process functions/struct/variables declaration and definition
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef RIL_DATA_H_
#define RIL_DATA_H_

#include <stdbool.h>
#include <pthread.h>
#include "sprd_atchannel.h"

#define MAX_PDP                     6
#define MAX_PDP_CP                  11
#define MINIMUM_APN_LEN             19

#define DATA_ACTIVE_FALLBACK_FAILED             -2
#define DATA_ACTIVE_FAILED                      -1
#define DATA_ACTIVE_SUCCESS                     0
#define DATA_ACTIVE_NEED_RETRY                  1
#define DATA_ACTIVE_NEED_FALLBACK               2
#define DATA_ACTIVE_NEED_RETRY_FOR_ANOTHER_CID  3

#define TRAFFIC_CLASS_DEFAULT       2
#define UNUSABLE_CID                0

#define TRUE 1
#define FALSE 0

#define IP_ADDR_SIZE                16
#define IPV6_ADDR_SIZE              64
#define IP_ADDR_MAX                 128
#define MAX_PDP_NUM                 12
#define PROPERTY_NAME_MAX           32
#define FILE_BUFFER_LENGTH          1024
#define NET_INTERFACE_LENGTH        128
#define RETRY_MAX_COUNT             1000

#define PDP_STATE_IDLE              1
#define PDP_STATE_ACTING            2
#define PDP_STATE_CONNECT           3
#define PDP_STATE_ESTING            4
#define PDP_STATE_ACTIVE            5
#define PDP_STATE_DESTING           6
#define PDP_STATE_DEACTING          7
#define PDP_STATE_ACT_ERROR         8
#define PDP_STATE_EST_ERROR         9
#define PDP_STATE_EST_UP_ERROR      10

#define SYS_IFCONFIG_UP "sys.ifconfig.up"
#define SYS_IP_SET "sys.data.setip"
#define SYS_MTU_SET "sys.data.setmtu"
#define SYS_NO_ARP "sys.data.noarp"
#define SYS_NO_ARP_IPV6  "sys.data.noarp.ipv6"
#define SYS_IPV6_DISABLE "sys.data.IPV6.disable"
#define SYS_IPV6_ON  "sys.data.ipv6.on"

#define SYS_IP_CLEAR                "sys.data.clearip"
#define SYS_IFCONFIG_DOWN           "sys.ifconfig.down"
#define SYS_NET_ADDR                "sys.data.net.addr"
#define SYS_NET_ACTIVATING_TYPE     "sys.data.activating.type"
#define SYS_IPV6_LINKLOCAL          "sys.data.ipv6.linklocal"
#define DEFAULT_PUBLIC_DNS2         "204.117.214.10"
// Due to real network limited,
// 2409:8084:8000:0010:2002:4860:4860:8888 maybe not correct
#define DEFAULT_PUBLIC_DNS2_IPV6    "2409:8084:8000:0010:2002:4860:4860:8888"
#define MODEM_CONFIG                "persist.radio.modem.config"

#define GSPS_IPV4_ADDR_HEADER       "192.168."
#define DHCP_DNSMASQ_LEASES_FILE    "/data/misc/dhcp/dnsmasq.leases"

#define GSPS_ETH_UP_PROP            "ril.gsps.eth.up"
#define GSPS_ETH_DOWN_PROP          "ril.gsps.eth.down"
#define ATTACH_ENABLE_PROP          "persist.sys.attach.enable"
#define SYS_GSPS_ETH_LOCALIP_PROP   "sys.gsps.eth.localip"
#define SYS_GSPS_ETH_PEERIP_PROP    "sys.gsps.eth.peerip"
#define DHCP_DNSMASQ_LEASES_FILE    "/data/misc/dhcp/dnsmasq.leases"

#define SOCKET_NAME_EXT_DATA        "ext_data"


// Default MTU value
#define DEFAULT_MTU 1500
typedef enum {
    UNKNOWN = 0,
    IPV4    = 1,
    IPV6    = 2,
    IPV4V6  = 3
} IPType;

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

typedef struct PDP_INFO {
    char dns1addr[IP_ADDR_SIZE];            /* IPV4 Primary MS DNS entries */
    char dns2addr[IP_ADDR_SIZE];            /* IPV4 secondary MS DNS entries */
    char userdns1addr[IP_ADDR_SIZE];        /* IPV4 Primary MS DNS entries */
    char userdns2addr[IP_ADDR_SIZE];        /* IPV4 secondary MS DNS entries */
    char ipladdr[IP_ADDR_SIZE];             /* IPV4 address local */
    char ipraddr[IP_ADDR_SIZE];             /* IPV4 address remote */
    char ipv6dns1addr[IPV6_ADDR_SIZE];      /* IPV6 Primary MS DNS entries */
    char ipv6dns2addr[IPV6_ADDR_SIZE];      /* IPV6 secondary MS DNS entries */
    char ipv6userdns1addr[IPV6_ADDR_SIZE];  /* IPV6 Primary MS DNS entries */
    char ipv6userdns2addr[IPV6_ADDR_SIZE];  /* IPV6 secondary MS DNS entries */
    char ipv6laddr[IPV6_ADDR_SIZE];         /* IPV6 address local */
    char ipv6raddr[IPV6_ADDR_SIZE];         /* IPV6 address remote */
    IPType ip_state;
    int state;
    int cid;
    int manual_dns;
    int error_num;
} PDP_INFO;

extern int s_manualSearchNetworkId;
extern struct PDPInfo pdp[MAX_PDP];

static void putPDP(int cid);
int isExistActivePdp();

void ps_service_init();

/* for AT+CGACT=0 set command response process */
void cgact_deact_cmd_rsp(int cid);

/* for AT+CGDATA= set command process */
int cgdata_set_cmd_req(char *cgdataCmd);

/* for AT+CGDCONT= set command response process */
int cgdcont_set_cmd_req(char *cmd, char *newCmd);

/* for AT+CGDCONT? read response process */
void cgdcont_read_cmd_rsp(ATResponse *p_response, ATResponse **pp_outResponse);

/* for AT+CGDATA= set command response process */
int cgdata_set_cmd_rsp(ATResponse *p_response, int pdpIndex, int primaryCid,
                       int channelID);

#endif  // RIL_DATA_H_
