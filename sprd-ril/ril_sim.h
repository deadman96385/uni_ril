/**
 * ril_sim.h --- SIM-related requests
 *               process functions/struct/variables declaration and definition
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef RIL_SIM_H_
#define RIL_SIM_H_

typedef enum {
    UNLOCK_PIN   = 0,
    UNLOCK_PIN2  = 1,
    UNLOCK_PUK   = 2,
    UNLOCK_PUK2  = 3
} SimUnlockType;

typedef enum {
    SIM_ABSENT = 0,
    SIM_NOT_READY = 1,
    SIM_READY = 2,  /* SIM_READY means radio state is RADIO_STATE_SIM_READY */
    SIM_PIN = 3,
    SIM_PUK = 4,
    SIM_NETWORK_PERSONALIZATION = 5,
    RUIM_ABSENT = 6,
    RUIM_NOT_READY = 7,
    RUIM_READY = 8,
    RUIM_PIN = 9,
    RUIM_PUK = 10,
    RUIM_NETWORK_PERSONALIZATION = 11
} SimStatus;

extern int s_imsInitISIM[SIM_COUNT];

int processSimRequests(int request, void *data, size_t datalen, RIL_Token t,
                          int channelID);
int processSimUnsolicited(RIL_SOCKET_ID socket_id, const char *s);
SimStatus getSIMStatus(int channelID);
RIL_AppType getSimType(int channelID);
void dispatchCLCK(RIL_Token t, void *data, void *resp);

#endif  // RIL_SIM_H_
