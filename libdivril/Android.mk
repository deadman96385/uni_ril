# Copyright
#
#
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_VENDOR_MODULE := true

LOCAL_SRC_FILES := \
    divril.cpp

LOCAL_SHARED_LIBRARIES := \
    libdl \
    liblog \
    libutils \
    libcutils \
    libsprd-ril \
    libnetutils \
    libhardware_legacy \
    android.hardware.radio@1.0 \
    android.hardware.radio@1.1 \
    android.hardware.radio.deprecated@1.0 \
    libhidlbase  \
    libhidltransport \
    libhwbinder \
    vendor.sprd.hardware.radio@1.0 \
    vendor.sprd.hardware.radio.flavor@1.0 \

LOCAL_MODULE:= libDivRIL
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)

