LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_CLANG := true
LOCAL_CPPFLAGS := -std=c++11 -Wall -Werror
LOCAL_MODULE := cndaemon

LOCAL_SHARED_LIBRARIES := \
        libcrypto \
        libcutils \
        libdl \
        libhardware_legacy \
        liblog \
        liblogwrap \
        libmdnssd \
        libnetutils \
        libsysutils \

LOCAL_SRC_FILES := \
        main.cpp \

include $(BUILD_EXECUTABLE)

# Use the folloing include to make our bin.
include $(call all-makefiles-under,$(LOCAL_PATH))