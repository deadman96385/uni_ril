/**
 * ril_custom.c --- Compatible between sprd and custom
 *
 * Copyright (C) 2016 Spreadtrum Communications Inc.
 */

#define LOG_TAG "CUSTOM"

#include "sprd-ril.h"
#include "ril_custom.h"

/* Bug 523208 set PIN/PUK remain times to prop. @{ */
void setPinPukRemainTimes(int type, int remainTimes, RIL_SOCKET_ID socketId) {
    char num[ARRAY_SIZE];  // max remain times is 10
    char prop[PROPERTY_VALUE_MAX];

    snprintf(prop, sizeof(prop), "%s",
        (type == UNLOCK_PIN_TYPE ? PIN_REMAIN_TIMES_PROP : PUK_REMAIN_TIMES_PROP));

    RLOGD("setPinPukRemainTimes -> prop = %s, remainTimes = %s for SIM%d", prop,
          num, socketId);

    snprintf(num, sizeof(num), "%d", remainTimes);
    setProperty(socketId, prop, num);
}
/* }@ */
