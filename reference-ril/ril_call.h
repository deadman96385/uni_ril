/**
 * ril_call.h --- Call-related requests
 *                process functions/struct/variables declaration and definition
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef RIL_CALL_H_
#define RIL_CALL_H_

#define ECC_LIST_PROP "ril.ecclist"

typedef struct EccList {
    char * number;
    int category;
    struct EccList *next;
    struct EccList *prev;
} EccList;

/* Used by RIL_UNSOL_VIDEOPHONE_DSCI */
typedef struct {
    int id;
    int idr;
    int stat;
    int type;
    int mpty;
    char *number;
    int num_type;
    int bs_type;
    int cause;
    int location; /* if cause = 57 and location <= 2,
                  * it means current sim hasn't start vt service
                  */
} RIL_VideoPhone_DSCI;

typedef struct ListNode {
    char data;
    struct ListNode *next;
    struct ListNode *prev;
} ListNode;

extern ListNode s_DTMFList[SIM_COUNT];

int processCallRequest(int request, void *data, size_t datalen, RIL_Token t,
                          int channelID);
int processCallUnsolicited(RIL_SOCKET_ID socket_id, const char *s);

#endif  // RIL_CALL_H_
