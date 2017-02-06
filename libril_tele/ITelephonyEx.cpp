/**
 * Copyright (C) 2016 Spreadtrum Communications Inc.
 */

#define LOG_TAG "RIL-TELE"

#include <ITelephonyEx.h>
#include <binder/Parcel.h>
#include <cutils/jstring.h>

namespace android {
#define PACKAGE_NAME    "com.android.internal.telephony.ITelephonyEx"

enum {
    TRANSACTION_updatePlmn = IBinder::FIRST_CALL_TRANSACTION + 0,
    TRANSACTION_updateNetworkList = IBinder::FIRST_CALL_TRANSACTION + 1
};

static char *strdupReadString(Parcel &p) {
    size_t stringlen;
    const char16_t *s16;

    s16 = p.readString16Inplace(&stringlen);

    return strndup16to8(s16, stringlen);
}

class BpTelephonyEx: public BpInterface<ITelephonyEx> {
 public:
    explicit BpTelephonyEx(const sp<IBinder>& impl)
        : BpInterface<ITelephonyEx>(impl) {
    }

    int updatePlmn(int simId, const char *mncmcc, char *updatedPlmn) {
        Parcel data, reply;
        data.writeInterfaceToken(ITelephonyEx::getInterfaceDescriptor());
        data.writeInt32(simId);
        data.writeString16(String16(mncmcc));
        if (remote()->transact(TRANSACTION_updatePlmn, data, &reply) !=
                NO_ERROR) {
            RLOGD("updatePlmn could not contact remote");
            return -1;
        }
        int32_t err = reply.readExceptionCode();
        if (err < 0) {
            RLOGD("updatePlmn caught exception %d", err);
            return -1;
        }
        const char *plmn = strdupReadString(reply);
        if (updatedPlmn != NULL) {
            memcpy(updatedPlmn, plmn, strlen(plmn) + 1);
        }
        free((void *)plmn);
        return 0;
    }

    int updateNetworkList(int simId, char **networkList, size_t len,
                             char *updatedNetList) {
        Parcel data, reply;
        char **p_cur = (char **)networkList;
        int numStrings = len / sizeof(char *);

        data.writeInterfaceToken(ITelephonyEx::getInterfaceDescriptor());
        data.writeInt32(simId);
        data.writeInt32(numStrings);
        for (int count = 0; count < numStrings; count++) {
            data.writeString16(String16(p_cur[count]));
        }

        if (remote()->transact(TRANSACTION_updateNetworkList, data, &reply) !=
                NO_ERROR) {
            RLOGD("updateNetworkList could not contact remote");
            return -1;
        }
        int32_t err = reply.readExceptionCode();
        if (err < 0) {
            RLOGD("updateNetworkList caught exception %d", err);
            return -1;
        }
        const char *netList = strdupReadString(reply);
        if (updatedNetList != NULL) {
            memcpy(updatedNetList, netList, strlen(netList) + 1);
        }
        free((void *)netList);
        return 0;
    }
};

IMPLEMENT_META_INTERFACE(TelephonyEx, PACKAGE_NAME);
};  // namespace android
