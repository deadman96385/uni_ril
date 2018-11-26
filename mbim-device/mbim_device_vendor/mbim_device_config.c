#define LOG_TAG "MBIM-Device"

#include <string.h>
#include <stdlib.h>
#include <utils/Log.h>
#include <cutils/properties.h>

#include "mbim_device_config.h"
#include "misc.h"

bool            s_isLTE = false;
ModemType       s_modemType = MODEM_TYPE_UNKOWN;
ModemConfig     s_modemConfig = 0;
RadioFeatures   s_workMode[SIM_COUNT];

ModemType getModemType() {
    ModemType modemType = MODEM_TYPE_UNKOWN;
    char prop[PROPERTY_VALUE_MAX] = {0};

    property_get(MODEM_TYPE_PROP, prop, "");
    RLOGE("modem type: %s", prop);
    if (strcmp(prop, "l") == 0) {
        modemType = MODEM_TYPE_L;
    } else if (strcmp(prop, "tl") == 0) {
        modemType = MODEM_TYPE_TL;
    } else if (strcmp(prop, "lf") == 0) {
        modemType = MODEM_TYPE_LF;
    } else if (strcmp(prop, "t") == 0) {
        modemType = MODEM_TYPE_T;
    }  else if (strcmp(prop, "w") == 0) {
        modemType = MODEM_TYPE_W;
    }

    return modemType;
}

ModemConfig getModemConfig() {
    char prop[PROPERTY_VALUE_MAX] = {0};
    int modemConfig = 0;

    property_get(MODEM_CONFIG_PROP, prop, "");
    if (strcmp(prop, "TL_LF_TD_W_G,W_G") == 0) {
        modemConfig = LWG_WG;
    } else if (strcmp(prop, "TL_LF_TD_W_G,TL_LF_TD_W_G") == 0) {
        modemConfig = LWG_LWG;
    } else if (strcmp(prop, "TL_LF_G,G") == 0) {
        modemConfig = LG_G;
    } else if (strcmp(prop, "W_G,G") == 0) {
        modemConfig = W_G;
    } else if (strcmp(prop, "W_G,W_G") == 0) {
        modemConfig = WG_WG;
    }
    return modemConfig;
}

RadioFeatures getWorkMode(RIL_SOCKET_ID socket_id) {
    char prop[PROPERTY_VALUE_MAX] = {0};
    RadioFeatures workMode = UNKNOWN;

    getProperty(socket_id, MODEM_WORKMODE_PROP, prop, "10");

    workMode = atoi(prop);

    RLOGD("getWorkMode socket_id = %d, workMode = %d", socket_id, workMode);
    return workMode;
}

bool isLte(void) {
    if (s_modemType == MODEM_TYPE_L || s_modemType == MODEM_TYPE_TL ||
        s_modemType == MODEM_TYPE_LF) {
        return true;
    }

    return false;
}
