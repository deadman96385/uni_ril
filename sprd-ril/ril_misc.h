/**
 * ril_misc.h --- Any other requests besides data/sim/call...
 *                process functions/struct/variables declaration and definition
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef RIL_MISC_H_
#define RIL_MISC_H_

extern int s_maybeAddCall;
extern int s_screenState;
int processMiscRequests(int request, void *data, size_t datalen,
                           RIL_Token t, int channelID);
int processMiscUnsolicited(RIL_SOCKET_ID socket_id, const char *s);

#endif  // RIL_MISC_H_
