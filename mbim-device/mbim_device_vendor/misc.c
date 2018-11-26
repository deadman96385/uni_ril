/* //vendor/sprd/proprietories-source/ril/sprd-ril/misc.c
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#define LOG_TAG "MBIM-device"

#include <string.h>
#include <utils/Log.h>
#include <cutils/properties.h>
#include <sys/system_properties.h>
#include "misc.h"


/** returns 1 if line starts with prefix, 0 if it does not */
int
strStartsWith(const char *  line,
              const char *  prefix) {
    for ( ; *line != '\0' && *prefix != '\0' ; line++, prefix++) {
        if (*line != *prefix) {
            return 0;
        }
    }

    return *prefix == '\0';
}

void
getProperty(RIL_SOCKET_ID       socket_id,
            const char *        property,
            char *              value,
            const char *        defaultVal) {
    int simId = 0;
    char prop[PROPERTY_VALUE_MAX];
    int len = property_get(property, prop, "");
    char *p[RIL_SOCKET_NUM];
    char *buf = prop;
    char *ptr = NULL;
    RLOGD("get sim%d [%s] property: %s", socket_id, property, prop);

    if (value == NULL) {
        RLOGE("The memory to save prop is NULL!");
        return;
    }

    memset(p, 0, RIL_SOCKET_NUM * sizeof(char *));
    if (len > 0) {
        for (simId = 0; simId < RIL_SOCKET_NUM; simId++) {
            ptr = strsep(&buf, ",");
            p[simId] = ptr;
        }

        if (socket_id >= RIL_SOCKET_1 && socket_id < RIL_SOCKET_NUM &&
                (p[socket_id] != NULL) && strcmp(p[socket_id], "")) {/*lint !e676 */
            memcpy(value, p[socket_id], strlen(p[socket_id]) + 1);/*lint !e676 */
            return;
        }
    }

    if (defaultVal != NULL) {
        len = strlen(defaultVal);
        memcpy(value, defaultVal, len);
        value[len] = '\0';
    }
}

void
setProperty(RIL_SOCKET_ID   socket_id,
            const char *    property,
            const char *    value) {
    char prop[PROPERTY_VALUE_MAX];
    char propVal[PROPERTY_VALUE_MAX];
    int len = property_get(property, prop, "");
    int i, simId = 0;
    char *p[RIL_SOCKET_NUM];
    char *buf = prop;
    char *ptr = NULL;

    if (socket_id < RIL_SOCKET_1 || socket_id >= RIL_SOCKET_NUM) {
        RLOGE("setProperty: invalid socket id = %d, property = %s",
                socket_id, property);
        return;
    }

    RLOGD("set sim%d [%s] property: %s", socket_id, property, value);
    memset(p, 0, RIL_SOCKET_NUM * sizeof(char *));
    if (len > 0) {
        for (simId = 0; simId < RIL_SOCKET_NUM; simId++) {
            ptr = strsep(&buf, ",");
            p[simId] = ptr;
        }
    }

    memset(propVal, 0, sizeof(propVal));
    for (i = 0; i < (int)socket_id; i++) {
        if (p[i] != NULL) {
            strncat(propVal, p[i], strlen(p[i]));
        }
        strncat(propVal, ",", 1);
    }

    if (value != NULL) {
        strncat(propVal, value, strlen(value));
    }

    for (i = socket_id + 1; i < RIL_SOCKET_NUM; i++) {
        strncat(propVal, ",", 1);
        if (p[i] != NULL) {
            strncat(propVal, p[i], strlen(p[i]));
        }
    }

    property_set(property, propVal);
}

void *noopRemoveWarning(void *a) {
    return a;
}


/**
 * Some fields (like ICC ID) in GSM SIMs are stored as nibble-swizzled BCH
 */
void
bchToString(char *data) {
    char *cp = data;
    char c;

    for (cp = data; *cp && *(cp + 1); cp += 2) {
        c = *cp;
        *cp = *(cp + 1);
        *(cp + 1) = c;
    }
}
