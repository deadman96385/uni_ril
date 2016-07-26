/**
 * ril_network.h --- Network-related requests
 *                process functions/struct/variables declaration and definition
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

typedef enum {
    TD_LTE = 1,
    LTE_FDD = 2,
    TD_LTE_AND_LTE_FDD = 3,
    LTE_FDD_AND_W_AND_GSM_CSFB = 4,
    TD_LTE_AND_W_AND_GSM_CSFB = 5,
    TD_LTE_AND_LTE_FDD_AND_W_AND_GSM_CSFB = 6,
    TD_LTE_AND_TD_AND_GSM_CSFB = 7,
    TD_LTE_AND_LTE_FDD_AND_TD_AND_GSM_CSFB = 8,
    TD_LTE_AND_LTE_FDD_AND_W_AND_TD_AND_GSM_CSFB = 9,
    GSM_ONLY = 10,
    WCDMA_ONLY = 11,
    TD_ONLY = 12,
    TD_AND_GSM = 13,
    WCDMA_AND_GSM = 14,
    PRIMARY_GSM_ONLY = 15,
    NONE = 254,
    TD_AND_WCDMA = 255,
} RadioFeatures;

typedef struct {
    bool s_sim_busy;
    pthread_mutex_t s_sim_busy_mutex;
    pthread_cond_t s_sim_busy_cond;
} SimBusy;

typedef enum {
    RIL_REG_STATE_NOT_REG                           = 0,   /* Not registered, MT is not currently searching */
                                                           /* a new operator to register */
    RIL_REG_STATE_HOME                              = 1,   /* Registered, home network */
    RIL_REG_STATE_SEARCHING                         = 2,   /* Not registered, but MT is currently searching */
                                                           /* a new operator to register */
    RIL_REG_STATE_DENIED                            = 3,   /* Registration denied */
    RIL_REG_STATE_UNKNOWN                           = 4,   /* Unknown */
    RIL_REG_STATE_ROAMING                           = 5,   /* Registered, roaming */
    RIL_REG_STATE_NOT_REG_EMERGENCY_CALL_ENABLED    = 10,  /* Same as 0, but indicates that emergency calls */
                                                           /* are enabled. */
    RIL_REG_STATE_SEARCHING_EMERGENCY_CALL_ENABLED  = 12,  /* Same as 2, but indicates that emergency calls */
                                                           /* are enabled. */
    RIL_REG_STATE_DENIED_EMERGENCY_CALL_ENABLED     = 13,  /* Same as 3, but indicates that emergency calls */
                                                           /* are enabled. */
    RIL_REG_STATE_UNKNOWN_EMERGENCY_CALL_ENABLED    = 14   /* Same as 4, but indicates that emergency calls */
                                                           /* are enabled. */
} RIL_RegState;

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

extern int s_presentSIMCount;
extern int s_in4G[SIM_COUNT];
extern int s_workMode[SIM_COUNT];
extern int s_sessionId[SIM_COUNT];
extern int s_desiredRadioState[SIM_COUNT];
extern int s_imsRegistered[SIM_COUNT];  // 0 == unregistered
extern int s_imsBearerEstablished[SIM_COUNT];
extern LTE_PS_REG_STATE s_PSRegState[SIM_COUNT];
extern pthread_mutex_t s_LTEAttachMutex[SIM_COUNT];
extern RIL_RegState s_CSRegStateDetail[SIM_COUNT];
extern RIL_RegState s_PSRegStateDetail[SIM_COUNT];
extern pthread_mutex_t s_radioPowerMutex[SIM_COUNT];
extern SimBusy s_simBusy[SIM_COUNT];

int processNetworkRequests(int request, void *data, size_t datalen,
                              RIL_Token t, int channelID);
int processNetworkUnsolicited(RIL_SOCKET_ID socket_id, const char *s);
void setSimPresent(RIL_SOCKET_ID socket_id, bool hasSim);
int isSimPresent(RIL_SOCKET_ID socket_id);
uint64_t ril_nano_time();

#endif  // RIL_NETWORK_H_
