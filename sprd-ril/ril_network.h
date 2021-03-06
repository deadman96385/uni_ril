/**
 * ril_network.h --- Network-related requests
 *                process functions/struct/variables declaration and definition
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef RIL_NETWORK_H_
#define RIL_NETWORK_H_

#define SIG_POOL_SIZE           10
#define MODEM_WORKMODE_PROP     "persist.vendor.radio.modem.workmode"
#define MODEM_CAPABILITY        "persist.vendor.radio.modem.capability"
#define OVERSEA_VERSION         "ro.carrier"

/*  LTE PS registration state */
typedef enum {
    STATE_OUT_OF_SERVICE = 0,
    STATE_IN_SERVICE = 1
} LTE_PS_REG_STATE;

typedef enum {
    NETWORK_MODE_WCDMA_PREF     = 0,  /* GSM/WCDMA (WCDMA preferred) */
    NETWORK_MODE_GSM_ONLY       = 1,  /* GSM only */
    NETWORK_MODE_WCDMA_ONLY     = 2,  /* WCDMA only */
    NETWORK_MODE_GSM_UMTS       = 3,  /* GSM/WCDMA (auto mode, according to PRL)
                                         AVAILABLE Application Settings menu */
    NETWORK_MODE_CDMA           = 4,  /* CDMA and EvDo (auto mode, according to PRL)
                                         AVAILABLE Application Settings menu */
    NETWORK_MODE_CDMA_NO_EVDO   = 5,  /* CDMA only */
    NETWORK_MODE_EVDO_NO_CDMA   = 6,  /* EvDo only */
    NETWORK_MODE_GLOBAL         = 7,  /* GSM/WCDMA, CDMA, and EvDo (auto mode, according to PRL)
                                         AVAILABLE Application Settings menu */
    NETWORK_MODE_LTE_CDMA_EVDO  = 8,  /* LTE, CDMA and EvDo */
    NETWORK_MODE_LTE_GSM_WCDMA  = 9,  /* LTE, GSM/WCDMA */
    NETWORK_MODE_LTE_CDMA_EVDO_GSM_WCDMA = 10,  /* LTE, CDMA, EvDo, GSM/WCDMA */
    NETWORK_MODE_LTE_ONLY       = 11,  /* LTE Only mode. */
    NETWORK_MODE_LTE_WCDMA      = 12,  /* LTE/WCDMA */
    NETWORK_MODE_LTE_GSM        = 23,  /* LTE/GSM */

    NETWORK_MODE_BASE = 50,
    NT_TD_LTE = NETWORK_MODE_BASE + 1,
    NT_LTE_FDD = NETWORK_MODE_BASE + 2,
    NT_LTE_FDD_TD_LTE = NETWORK_MODE_BASE + 3,
    NT_LTE_FDD_WCDMA_GSM = NETWORK_MODE_BASE + 4,
    NT_TD_LTE_WCDMA_GSM = NETWORK_MODE_BASE + 5,
    NT_LTE_FDD_TD_LTE_WCDMA_GSM = NETWORK_MODE_BASE + 6,
    NT_TD_LTE_TDSCDMA_GSM = NETWORK_MODE_BASE + 7,
    NT_LTE_FDD_TD_LTE_TDSCDMA_GSM = NETWORK_MODE_BASE + 8,
    NT_LTE_FDD_TD_LTE_WCDMA_TDSCDMA_GSM = NETWORK_MODE_BASE + 9,
    NT_GSM = NETWORK_MODE_BASE + 10,
    NT_WCDMA = NETWORK_MODE_BASE + 11,
    NT_TDSCDMA = NETWORK_MODE_BASE + 12,
    NT_TDSCDMA_GSM = NETWORK_MODE_BASE + 13,
    NT_WCDMA_GSM = NETWORK_MODE_BASE + 14,
    NT_WCDMA_TDSCDMA_GSM = NETWORK_MODE_BASE + 15,
    NT_LTE_FDD_TD_LTE_GSM = NETWORK_MODE_BASE + 16,
    NT_LTE_FDD_TD_LTE_WCDMA= NETWORK_MODE_BASE + 17,
} NetworkMode;

typedef struct {
    bool s_sim_busy;
    pthread_mutex_t s_sim_busy_mutex;
    pthread_cond_t s_sim_busy_cond;
} SimBusy;

typedef enum {
    LWG_G = 0,
    LWG_WG = 1,
    W_G = 2,
    LWG_LWG = 3,
    LG_G = 4,
    WG_WG = 5,
} ModemConfig;

typedef struct OperatorInfoList {
    char *plmn;
    char *operatorName;
    struct OperatorInfoList *next;
    struct OperatorInfoList *prev;
} OperatorInfoList;

typedef struct SetPropPara {
    int socketId;
    char *propName;
    char *propValue;
    pthread_mutex_t *mutex;
} SetPropPara;

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
    ril_signalstrength.LTE_SignalStrength.timingAdvance     = RIL_SIGNALSTRENGTH_INVALID;               \
    ril_signalstrength.TD_SCDMA_SignalStrength.rscp         = RIL_SIGNALSTRENGTH_INVALID;               \
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
extern int s_in2G[SIM_COUNT];
extern int s_workMode[SIM_COUNT];
extern int s_sessionId[SIM_COUNT];
extern int s_desiredRadioState[SIM_COUNT];
extern int s_imsRegistered[SIM_COUNT];  // 0 == unregistered
extern int s_imsBearerEstablished[SIM_COUNT];
extern int rxlev[SIM_COUNT];
extern int rscp[SIM_COUNT];
extern int rsrp[SIM_COUNT];
extern int rssi[SIM_COUNT];
extern LTE_PS_REG_STATE s_PSRegState[SIM_COUNT];
extern pthread_mutex_t s_LTEAttachMutex[SIM_COUNT];
extern RIL_RegState s_CSRegStateDetail[SIM_COUNT];
extern RIL_RegState s_PSRegStateDetail[SIM_COUNT];
extern pthread_mutex_t s_radioPowerMutex[SIM_COUNT];
extern SimBusy s_simBusy[SIM_COUNT];
extern OperatorInfoList s_operatorInfoList[SIM_COUNT];
extern OperatorInfoList s_operatorXmlInfoList;
extern pthread_mutex_t s_signalProcessMutex;
extern pthread_cond_t s_signalProcessCond;

void onModemReset_Network();
int processNetworkRequests(int request, void *data, size_t datalen,
                              RIL_Token t, int channelID);
int processNetworkUnsolicited(RIL_SOCKET_ID socket_id, const char *s);
void setSimPresent(RIL_SOCKET_ID socket_id, int hasSim);
int isSimPresent(RIL_SOCKET_ID socket_id);
uint64_t ril_nano_time();
void initPrimarySim();

void dispatchSPTESTMODE(RIL_Token t, void *data, void *resp);

/* for AT+CSQ execute command response process */
void csq_execute_cmd_rsp(ATResponse *p_response, ATResponse **p_newResponse);

/* for AT+CESQ execute command response process */
void cesq_execute_cmd_rsp(ATResponse *p_response, ATResponse **p_newResponse);

/* for +CSQ: unsol response process */
int csq_unsol_rsp(char *line, RIL_SOCKET_ID socket_id, char *newLine);

/* for +CESQ: unsol response process */
int cesq_unsol_rsp(char *line, RIL_SOCKET_ID socket_id, char *newLine);

extern int updatePlmn(int slotId, const char *mncmcc, char *resp, size_t respLen);
extern int updateNetworkList(int slotId, char **networkList, size_t datalen,
                      char *resp, size_t respLen);
extern int RIL_getONS(char *updatePlmn, char *plmn);

#endif  // RIL_NETWORK_H_
