# Copyright (C) 2017 Spreadtrum Communications Inc.

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    request_threads.c \
    thread_pool.cpp \

LOCAL_SHARED_LIBRARIES := \
    liblog libcutils libutils librilsprd

LOCAL_C_INCLUDES += vendor/sprd/proprietories-source/ril/include

LOCAL_PROPRIETARY_MODULE := true

#build shared library
LOCAL_MODULE:= libril_threads
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)

