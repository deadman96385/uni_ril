/**
 * ril_custom.h --- Compatible between sprd and custom
 *               =process functions/struct/variables declaration and definition
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef RIL_CUSTOM_H_
#define RIL_CUSTOM_H_

#define PIN_PUK_REMAIN_TIMES_PROP "vendor.sim.%s.remaintimes"
void setPinPukRemainTimes(int type, int remainTimes, RIL_SOCKET_ID socketId);

#endif  // RIL_CUSTOM_H_
