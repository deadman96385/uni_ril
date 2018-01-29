# Copyright 2006 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES :=$(LOCAL_PATH)/../include

LOCAL_SRC_FILES:= \
    sprd_ril.cpp \
    ril_event.cpp \
    sprd_thread_pool.cpp \
    RilSocket.cpp \
    RilATCISocket.cpp

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libutils \
    libbinder \
    libcutils \
    libhardware_legacy \
    libsprdrilutils

LOCAL_PROTOC_OPTIMIZE_TYPE := lite


LOCAL_CFLAGS := -DRIL_SHLIB

ifeq ($(BOARD_SPRD_RIL),true)
LOCAL_CFLAGS += -DRIL_SPRD_EXTENSION
endif

LOCAL_CFLAGS += -DRIL_SUPPORT_CALL_BLACKLIST

LOCAL_CFLAGS += -DRIL_SUPPORTED_OEMSOCKET

ifeq ($(BOARD_SAMSUNG_RIL),true)
LOCAL_CFLAGS += -DGLOBALCONFIG_RIL_SAMSUNG_LIBRIL_INTF_EXTENSION
endif

LOCAL_MODULE:= librilsprd
LOCAL_MODULE_TAGS := optional
#LOCAL_LDLIBS += -lpthread
include $(BUILD_SHARED_LIBRARY)


# For RdoServD which needs a static library
# =========================================
ifneq ($(ANDROID_BIONIC_TRANSITION),)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    sprd_ril.cpp\
    sprd_thread_pool.cpp

LOCAL_STATIC_LIBRARIES := \
    libutils_static \
    libcutils \
    librilutils_static \

LOCAL_CFLAGS := -DRIL_SHLIB

ifeq ($(BOARD_SPRD_RIL),true)
LOCAL_CFLAGS += -DRIL_SPRD_EXTENSION
endif

ifeq ($(BOARD_SAMSUNG_RIL),true)
LOCAL_CFLAGS += -DGLOBALCONFIG_RIL_SAMSUNG_LIBRIL_INTF_EXTENSION
endif

LOCAL_MODULE:= libril_spstatic
LOCAL_MODULE_TAGS := optional

#LOCAL_LDLIBS += -lpthread

include $(BUILD_STATIC_LIBRARY)
endif # ANDROID_BIONIC_TRANSITION