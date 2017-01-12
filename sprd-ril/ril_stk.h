/**
 * ril_stk.h --- Requests related to stk
 *               process functions/struct/variables declaration and definition
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef RIL_STK_H_
#define RIL_STK_H_

extern bool s_stkServiceRunning[SIM_COUNT];

int processStkRequests(int request, void *data, size_t datalen, RIL_Token t,
        int channelID);
int processStkUnsolicited(RIL_SOCKET_ID socket_id, const char *s);

#endif  // RIL_STK_H_
