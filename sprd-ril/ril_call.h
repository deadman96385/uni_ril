/**
 * ril_call.h --- Call-related requests
 *                process functions/struct/variables declaration and definition
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef RIL_CALL_H_
#define RIL_CALL_H_

#define ECC_LIST_PROP       "ril.ecclist"
#define ECC_LIST_REAL_PROP  "ril.ecclist.real"
#define ECC_LIST_FAKE_PROP  "ril.ecclist.fake"

typedef struct ListNode {
    char data;
    struct ListNode *next;
    struct ListNode *prev;
} ListNode;

/* add for VoLTE to handle +CLCCS */
typedef enum {
    VOLTE_CALL_IDEL = 1,
    VOLTE_CALL_CALLING_MO = 2,
    VOLTE_CALL_CONNECTING_MO = 3,
    VOLTE_CALL_ALERTING_MO = 4,
    VOLTE_CALL_ALERTING_MT = 5,
    VOLTE_CALL_ACTIVE = 6,
    VOLTE_CALL_RELEASED_MO = 7,
    VOLTE_CALL_RELEASED_MT = 8,
    VOLTE_CALL_USER_BUSY = 9,
    VOLTE_CALL_USER_DETERMINED_BUSY = 10,
    VOLTE_CALL_WAITING_MO = 11,
    VOLTE_CALL_WAITING_MT = 12,
    VOLTE_CALL_HOLD_MO = 13,
    VOLTE_CALL_HOLD_MT = 14
} RIL_VoLTE_CallState;

/* add for VoLTE to handle SRVCC */
typedef enum {
    SRVCC_PS_TO_CS_START = 0,
    SRVCC_PS_TO_CS_SUCCESS = 1,
    SRVCC_PS_TO_CS_CANCELED = 2,
    SRVCC_PS_TO_CS_FAILED = 3,
    VSRVCC_PS_TO_CS_START = 4,
    VSRVCC_PS_TO_CS_SUCCESS = 5,
    SRVCC_CS_TO_PS_START = 6,
    SRVCC_CS_TO_PS_CANCELED = 7,
    SRVCC_CS_TO_PS_FAILED = 8,
    SRVCC_CS_TO_PS_SUCCESS = 9,
} RIL_VoLTE_SrvccState;

typedef struct Srvccpendingrequest {
    char *cmd;
    struct Srvccpendingrequest *p_next;
} SrvccPendingRequest;

extern ListNode s_DTMFList[SIM_COUNT];

int processCallRequest(int request, void *data, size_t datalen, RIL_Token t,
                          int channelID);
int processCallUnsolicited(RIL_SOCKET_ID socket_id, const char *s);

#endif  // RIL_CALL_H_
