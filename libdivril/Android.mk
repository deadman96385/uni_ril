# Copyright
#
#
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_VENDOR_MODULE := true

LOCAL_SRC_FILES := \
    divril.c

LOCAL_SHARED_LIBRARIES := \
    liblog libcutils libutils libsprd-ril libnetutils

LOCAL_MODULE:= libDivRIL
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)

