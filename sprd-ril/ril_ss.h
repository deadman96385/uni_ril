/**
 * ril_ss.h --- SS-related requests
 *              process functions/struct/variables declaration and definition
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef RIL_SS_H_
#define RIL_SS_H_

int processSSRequests(int request, void *data, size_t datalen, RIL_Token t,
                         int channelID);
int processSSUnsolicited(RIL_SOCKET_ID socket_id, const char *s);

#endif  // RIL_SS_H_