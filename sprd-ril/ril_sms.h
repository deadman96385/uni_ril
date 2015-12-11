/**
 * ril_sms.h --- SMS-related requests
 *               process functions/struct/variables declaration and definition
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef RIL_SMS_H_
#define RIL_SMS_H_

int processSmsRequests(int request, void *data, size_t datalen, RIL_Token t,
                          int channelID);
int processSmsUnsolicited(RIL_SOCKET_ID socket_id, const char *s,
                             const char *sms_pdu);

#endif  // RIL_SMS_H_
