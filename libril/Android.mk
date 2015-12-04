# Copyright 2006 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES :=$(LOCAL_PATH)/../include

LOCAL_SRC_FILES:= \
    ril.cpp \
    ril_event.cpp \
    RilSocket.cpp \
    RilSapSocket.cpp \
    thread_pool.cpp \

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libutils \
    libbinder \
    libcutils \
    libhardware_legacy \
    librilutils

LOCAL_STATIC_LIBRARIES := \
    libprotobuf-c-nano-enable_malloc \

LOCAL_CFLAGS := -DRIL_SHLIB

ifneq ($(SIM_COUNT), 1)
LOCAL_CFLAGS += -DANDROID_MULTI_SIM
endif

ifeq ($(SIM_COUNT), 2)
    LOCAL_CFLAGS += -DANDROID_SIM_COUNT_2
endif

LOCAL_C_INCLUDES += $(TARGET_OUT_HEADER)/librilutils
LOCAL_C_INCLUDES += external/nanopb-c

LOCAL_MODULE:= libril
LOCAL_MODULE_TAGS := optional

LOCAL_COPY_HEADERS_TO := libril
LOCAL_COPY_HEADERS := ril_ex.h

include $(BUILD_SHARED_LIBRARY)

# For RdoServD which needs a static library
# =========================================
ifneq ($(ANDROID_BIONIC_TRANSITION),)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    ril.cpp\
    thread_pool.cpp

LOCAL_STATIC_LIBRARIES := \
    libutils_static \
    libcutils \
    librilutils_static \
    libprotobuf-c-nano-enable_malloc

LOCAL_CFLAGS := -DRIL_SHLIB

LOCAL_MODULE:= libril_static
LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)
endif # ANDROID_BIONIC_TRANSITION
