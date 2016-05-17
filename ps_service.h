/**
 * ps_service.h --- cmux implementation for the phoneserver
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef PS_SERVICE_H_
#define PS_SERVICE_H_

#include <stdbool.h>
#include "adapter.h"

#define IP_ADDR_SIZE               16
#define IPV6_ADDR_SIZE             64
#define IP_ADDR_MAX                128
#define MAX_PDP_NUM                12
#define MAX_CMD                    128
#define PROPERTY_NAME_MAX          32
#define FILE_BUFFER_LENGTH         1024
#define NET_INTERFACE_LENGTH       128

#define PDP_STATE_IDLE             1
#define PDP_STATE_ACTING           2
#define PDP_STATE_CONNECT          3
#define PDP_STATE_ESTING           4
#define PDP_STATE_ACTIVE           5
#define PDP_STATE_DESTING          6
#define PDP_STATE_DEACTING         7
#define PDP_STATE_ACT_ERROR        8
#define PDP_STATE_EST_ERROR        9
#define PDP_STATE_EST_UP_ERROR     10


#define PDP_DEACT_TIMEOUT          150
#define PDP_ACT_TIMEOUT            150
#define PDP_QUERY_TIMEOUT          30
#define PDP_UP_TIMEOUT             130
#define PDP_DOWN_TIMEOUT           30

typedef enum {
    UNKNOWN = 0,
    IPV4    = 1,
    IPV6    = 2,
    IPV4V6  = 3
} IP_TYPE;

struct PDP_INFO {
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
    IP_TYPE ip_state;
    int state;
    cmux_t *cmux;
    pty_t *pty;
    mutex mutex_timeout;
    cond cond_timeout;
    int cid;
    int manual_dns;
    int error_num;
};

void ps_service_init(void);
int cvt_cgdcont_read_req(AT_CMD_REQ_T * req);
int cvt_cgdcont_set_req(AT_CMD_REQ_T * req);
int cvt_cgdata_set_rsp(AT_CMD_RSP_T * rsp, uintptr_t user_data);
int cvt_cgact_deact_req(AT_CMD_REQ_T * req);
int cvt_cgact_act_rsp(AT_CMD_RSP_T * rsp, uintptr_t user_data);
int cvt_cgdcont_read_rsp(AT_CMD_RSP_T * rsp, uintptr_t user_data);
int cvt_cgact_deact_rsp(AT_CMD_RSP_T * rsp, uintptr_t user_data);
int cvt_cgdata_set_req(AT_CMD_REQ_T * req);
int cvt_cgact_act_req(AT_CMD_REQ_T * req);
int cvt_cgact_deact_rsp2(AT_CMD_RSP_T * rsp, uintptr_t user_data);
int cvt_cgact_deact_rsp1(AT_CMD_RSP_T * rsp, uintptr_t user_data);
int cvt_cgdcont_set_rsp(AT_CMD_RSP_T * rsp, uintptr_t user_data);
int cvt_sipconfig_rsp(AT_CMD_RSP_T * rsp, uintptr_t user_data);
int cvt_cgcontrdp_rsp(AT_CMD_RSP_T * rsp, uintptr_t user_data);
bool isLTE(void);
#endif  // PS_SERVICE_H_
