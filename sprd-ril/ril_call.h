/**
 * ril_call.h --- Call-related requests
 *                process functions/struct/variables declaration and definition
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef RIL_CALL_H_
#define RIL_CALL_H_

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

typedef enum {
    MEDIA_REQUEST_DEFAULT = 0,
    MEDIA_REQUEST_AUDIO_UPGRADE_VIDEO_BIDIRECTIONAL = 1,
    MEDIA_REQUEST_AUDIO_UPGRADE_VIDEO_TX = 2,
    MEDIA_REQUEST_AUDIO_UPGRADE_VIDEO_RX = 3,
    MEDIA_REQUEST_VIDEO_TX_UPGRADE_VIDEO_BIDIRECTIONAL = 4,
    MEDIA_REQUEST_VIDEO_RX_UPGRADE_VIDEO_BIDIRECTIONAL = 5,
    MEDIA_REQUEST_VIDEO_BIDIRECTIONAL_DOWNGRADE_AUDIO = 6,
    MEDIA_REQUEST_VIDEO_TX_DOWNGRADE_AUDIO = 7,
    MEDIA_REQUEST_VIDEO_RX_DOWNGRADE_AUDIO = 8,
    MEDIA_REQUEST_VIDEO_BIDIRECTIONAL_DOWNGRADE_VIDEO_TX = 9,
    MEDIA_REQUEST_VIDEO_BIDIRECTIONAL_DOWNGRADE_VIDEO_RX = 10,
} RIL_VoLTE_MEDIA_REQUEST;

typedef enum {
    VIDEO_CALL_MEDIA_DESCRIPTION_INVALID = 1000,
    VIDEO_CALL_MEDIA_DESCRIPTION_SENDRECV = 1001,  // "m=video\a=sendrecv" or "m=video"
    VIDEO_CALL_MEDIA_DESCRIPTION_SENDONLY = 1002,  // "m=video\a=sendonly"
    VIDEO_CALL_MEDIA_DESCRIPTION_RECVONLY = 1003,  // "m=video\a=recvonly"
} RIL_VoLTE_RESPONSE_MEDIA_CHANGE;

typedef struct Srvccpendingrequest {
    char *cmd;
    struct Srvccpendingrequest *p_next;
} SrvccPendingRequest;

extern ListNode s_DTMFList[SIM_COUNT];

void onModemReset_Call();

int processCallRequest(int request, void *data, size_t datalen, RIL_Token t,
                          int channelID);
int processCallUnsolicited(RIL_SOCKET_ID socket_id, const char *s);

int all_calls(int channelID, int do_mute);

void list_init(ListNode *node);

#endif  // RIL_CALL_H_
