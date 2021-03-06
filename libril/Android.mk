# Copyright 2006 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_VENDOR_MODULE := true

LOCAL_SRC_FILES:= \
    ril.cpp \
    ril_event.cpp\
    RilSapSocket.cpp \
    ril_service.cpp \
    sap_service.cpp \
    ril_ons.cpp \

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libutils \
    libcutils \
    libhardware_legacy \
    librilutils \
    android.hardware.radio@1.0 \
    android.hardware.radio@1.1 \
    android.hardware.radio.deprecated@1.0 \
    libhidlbase  \
    libhidltransport \
    libhwbinder \
    vendor.sprd.hardware.radio@1.0 \

LOCAL_STATIC_LIBRARIES := \
    libprotobuf-c-nano-enable_malloc \
    libtinyxml2 \

LOCAL_CFLAGS += -Wno-unused-parameter

LOCAL_CFLAGS += -DANDROID_MULTI_SIM

ifeq ($(SIM_COUNT), 2)
    LOCAL_CFLAGS += -DANDROID_SIM_COUNT_2
endif

ifneq ($(DISABLE_RILD_OEM_HOOK),)
    LOCAL_CFLAGS += -DOEM_HOOK_DISABLED
endif
LOCAL_C_INCLUDES += external/nanopb-c
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../include \
    external/tinyxml2
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/../include
LOCAL_MODULE:= librilsprd
LOCAL_SANITIZE := integer
include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)

LOCAL_VENDOR_MODULE := true

LOCAL_SRC_FILES:= \
    ril.cpp \
    ril_event.cpp\
    RilSapSocket.cpp \
    ril_service.cpp \
    sap_service.cpp \
    ril_ons.cpp \

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libutils \
    libcutils \
    libhardware_legacy \
    librilutils \
    android.hardware.radio@1.0 \
    android.hardware.radio@1.1 \
    android.hardware.radio.deprecated@1.0 \
    libhidlbase  \
    libhidltransport \
    libhwbinder \
    vendor.sprd.hardware.radio@1.0 \

LOCAL_STATIC_LIBRARIES := \
    libprotobuf-c-nano-enable_malloc \
    libtinyxml2 \

LOCAL_CFLAGS += -Wno-unused-parameter

ifneq ($(DISABLE_RILD_OEM_HOOK),)
    LOCAL_CFLAGS += -DOEM_HOOK_DISABLED
endif
LOCAL_C_INCLUDES += external/nanopb-c
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../include \
    external/tinyxml2
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/../include
LOCAL_MODULE:= librilsprd-single
LOCAL_SANITIZE := integer

include $(BUILD_SHARED_LIBRARY)

