/**
 * Copyright (C) 2016 Spreadtrum Communications Inc.
 */

#define LOG_TAG "RIL-Tele"

#include <android/log.h>
#include <binder/IServiceManager.h>
#include <string.h>
#include <utils/String8.h>
#include <utils/String16.h>
#include <cutils/properties.h>
#include "ITelephonyEx.h"
#include "TelephonyEx.h"

using namespace android;

#define SERVICE_NAME    "phone_ex"

int updatePlmn(int simId, const char *mncmcc, char *updatedPlmn) {
    sp <IServiceManager> sm = NULL;
    sm = defaultServiceManager();
    if (sm == NULL) {
        RLOGE("Failed to get default ServiceManager");
        return -1;
    }

    sp <ITelephonyEx> telephonyEx = NULL;
    String16 serviceName = String16(SERVICE_NAME);

    telephonyEx = interface_cast<ITelephonyEx>(sm->getService(serviceName));
    if (telephonyEx == NULL) {
        RLOGE("Failed to get connection to %s, errno: %s",
                String8(serviceName).string(), strerror(errno));
        return -1;
    }
    return telephonyEx->updatePlmn(simId, mncmcc, updatedPlmn);
}

int updateNetworkList(int simId, char **networkList, size_t len,
                         char *updatedNetList) {
    sp <IServiceManager> sm = NULL;
    sm = defaultServiceManager();
    if (sm == NULL) {
        RLOGE("Failed to get default ServiceManager");
        return -1;
    }

    sp <ITelephonyEx> telephonyEx = NULL;
    String16 serviceName = String16(SERVICE_NAME);

    telephonyEx = interface_cast<ITelephonyEx>(sm->getService(serviceName));
    if (telephonyEx == NULL) {
        RLOGE("Failed to get connection to %s, errno: %s",
                String8(serviceName).string(), strerror(errno));
        return -1;
    }
    return telephonyEx->updateNetworkList(simId, networkList, len,
                                           updatedNetList);
}
