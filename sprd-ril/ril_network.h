/**
 * ril_network.h --- Network-related requests
 *                   process functions/struct/variables declaration and definition
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef RIL_NETWORK_H_
#define RIL_NETWORK_H_

/*  LTE PS registration state */
typedef enum {
    STATE_OUT_OF_SERVICE = 0,
    STATE_IN_SERVICE = 1
} LTE_PS_REG_STATE;

typedef struct {
    bool s_sim_busy;
    pthread_mutex_t s_sim_busy_mutex;
    pthread_cond_t s_sim_busy_cond;
} SimBusy;

#define RIL_SIGNALSTRENGTH_INVALID 0x7FFFFFFF

#define RIL_SIGNALSTRENGTH_INIT(ril_signalstrength) do {                                               \
    ril_signalstrength.GW_SignalStrength.signalStrength     = 99;                                       \
    ril_signalstrength.GW_SignalStrength.bitErrorRate       = -1;                                       \
    ril_signalstrength.CDMA_SignalStrength.dbm              = -1;                                       \
    ril_signalstrength.CDMA_SignalStrength.ecio             = -1;                                       \
    ril_signalstrength.EVDO_SignalStrength.dbm              = -1;                                       \
    ril_signalstrength.EVDO_SignalStrength.ecio             = -1;                                       \
    ril_signalstrength.EVDO_SignalStrength.signalNoiseRatio = -1;                                       \
    ril_signalstrength.LTE_SignalStrength.signalStrength    = 99;                                       \
    ril_signalstrength.LTE_SignalStrength.rsrp              = RIL_SIGNALSTRENGTH_INVALID;               \
    ril_signalstrength.LTE_SignalStrength.rsrq              = RIL_SIGNALSTRENGTH_INVALID;               \
    ril_signalstrength.LTE_SignalStrength.rssnr             = RIL_SIGNALSTRENGTH_INVALID;               \
    ril_signalstrength.LTE_SignalStrength.cqi               = RIL_SIGNALSTRENGTH_INVALID;               \
} while (0);

#define RIL_SIGNALSTRENGTH_INIT_LTE(ril_signalstrength) do {                          \
    ril_signalstrength.LTE_SignalStrength.signalStrength = 99;                         \
    ril_signalstrength.LTE_SignalStrength.rsrp           = RIL_SIGNALSTRENGTH_INVALID; \
    ril_signalstrength.LTE_SignalStrength.rsrq           = RIL_SIGNALSTRENGTH_INVALID; \
    ril_signalstrength.LTE_SignalStrength.rssnr          = RIL_SIGNALSTRENGTH_INVALID; \
    ril_signalstrength.LTE_SignalStrength.cqi            = RIL_SIGNALSTRENGTH_INVALID; \
} while (0);

extern bool s_oneSimOnly;
extern int s_in4G[SIM_COUNT];
extern int s_workMode[SIM_COUNT];
extern int s_desiredRadioState[SIM_COUNT];
extern int s_PSAttachAllowed[SIM_COUNT];
extern LTE_PS_REG_STATE s_PSRegState[SIM_COUNT];
extern pthread_mutex_t s_LTEAttachMutex[SIM_COUNT];

int processNetworkRequests(int request, void *data, size_t datalen, RIL_Token t, int channelID);
int processNetworkUnsolicited(RIL_SOCKET_ID socket_id, const char *s);

#endif  // RIL_NETWORK_H_
