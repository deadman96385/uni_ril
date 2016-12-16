LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-java-files-under, src)
#LOCAL_SRC_FILES += \
#        src/com/sprd/customizedNet/ICustomizedNetAdapter.aidl


# LOCAL_JAVA_LIBRARIES := telephony-common
# LOCAL_JAVA_LIBRARIES += telephony-common2

LOCAL_PACKAGE_NAME := CustomizedNet
LOCAL_CERTIFICATE := platform

LOCAL_PROGUARD_ENABLED := disabled

LOCAL_DEX_PREOPT := false

include $(BUILD_PACKAGE)
#include $(BUILD_JAVA_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))
