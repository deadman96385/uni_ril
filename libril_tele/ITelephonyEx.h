/**
 * Copyright (C) 2016 Spreadtrum Communications Inc.
 */

#ifndef ITELEPHONYEX_H_
#define ITELEPHONYEX_H_

#include <binder/IInterface.h>
#include <binder/Parcel.h>

namespace android {

class ITelephonyEx: public IInterface {
 public:
    DECLARE_META_INTERFACE(TelephonyEx);

    virtual int updatePlmn(int simId, const char *mncmcc,
                              char *updatedPlmn) = 0;
    virtual int updateNetworkList(int simId, char **networkList,
                                      size_t len, char *updatedNetList) = 0;
};

class BnTelephonyEx: public BnInterface<ITelephonyEx> {
 public:
    virtual status_t onTransact(uint32_t code, const Parcel& data,
            Parcel* reply, uint32_t flags = 0);
};

};  // namespace android

#endif  // ITELEPHONYEX_H_
