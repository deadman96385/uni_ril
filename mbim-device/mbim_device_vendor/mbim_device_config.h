
#ifndef MBIM_DEVICE_CONFIG_H_
#define MBIM_DEVICE_CONFIG_H_

#include <stdbool.h>

#ifndef ANDROID_MULTI_SIM
#define SIM_COUNT 1
#elif defined (ANDROID_SIM_COUNT_3)
#define SIM_COUNT 3
#elif defined (ANDROID_SIM_COUNT_4)
#define SIM_COUNT 4
#else
#define SIM_COUNT 2
#endif

typedef enum {
    RIL_SOCKET_1,
#if (SIM_COUNT >= 2)
    RIL_SOCKET_2,
#if (SIM_COUNT >= 3)
    RIL_SOCKET_3,
#endif
#if (SIM_COUNT >= 4)
    RIL_SOCKET_4,
#endif
#endif
    RIL_SOCKET_NUM
} RIL_SOCKET_ID;

typedef enum {
    MODEM_TYPE_UNKOWN,
    MODEM_TYPE_T,
    MODEM_TYPE_W,
    MODEM_TYPE_L,
    MODEM_TYPE_TL,
    MODEM_TYPE_LF
} ModemType;

typedef enum {
    LWG_G = 0,
    LWG_WG = 1,
    W_G = 2,
    LWG_LWG = 3,
    LG_G = 4,
    WG_WG = 5,
} ModemConfig;

typedef enum {
    UNKNOWN = 0,
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
    PRIMARY_WCDMA_ONLY = 18,
    PRIMARY_TD_ONLY = 19,
    PRIMARY_TD_AND_WCDMA = 20,
    PRIMARY_WCDMA_AND_GSM = 22,
    NONE = 254,
    TD_AND_WCDMA = 255,
} RadioFeatures;

#define MODEM_TYPE_PROP         "ro.radio.modemtype"
#define MODEM_CONFIG_PROP       "persist.radio.modem.config"
#define MODEM_WORKMODE_PROP     "persist.radio.modem.workmode"

extern bool s_isLTE;
extern ModemType s_modemType;
extern ModemConfig s_modemConfig;
extern RadioFeatures s_workMode[SIM_COUNT];

bool            isLte();
ModemType       getModemType();
ModemConfig     getModemConfig();
RadioFeatures   getWorkMode(RIL_SOCKET_ID socket_id);


#endif  // MBIM_DEVICE_CONFIG_H_
