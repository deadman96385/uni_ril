/**
 * ril_custom.h --- Compatible between sprd and custom
 *               =process functions/struct/variables declaration and definition
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef RIL_CUSTOM_H_
#define RIL_CUSTOM_H_

#define UNLOCK_PIN_TYPE         0
#define PIN_REMAIN_TIMES_PROP   "gsm.sim.pin.remaintimes"
#define PUK_REMAIN_TIMES_PROP   "gsm.sim.puk.remaintimes"

void setPinPukRemainTimes(int type, int remainTimes, RIL_SOCKET_ID socketId);

#endif  // RIL_CUSTOM_H_
