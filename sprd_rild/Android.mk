# Copyright 2006 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES :=$(LOCAL_PATH)/../include

LOCAL_SRC_FILES:= \
	sprd_rild.c


LOCAL_SHARED_LIBRARIES := \
	liblog \
	libcutils \
	librilsprd \
	libdl

# temporary hack for broken vendor rils
LOCAL_WHOLE_STATIC_LIBRARIES := \
	librilutils_static

LOCAL_CFLAGS := -DRIL_SHLIB

LOCAL_MODULE:= sprdrild
LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/librilsprd
LOCAL_MODULE_STEM_32 := sprdrild
LOCAL_MODULE_STEM_64 := sprdrild64
LOCAL_MULTILIB := both
include $(BUILD_EXECUTABLE)

# For radiooptions binary
# =======================
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	radiooptions.c

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libcutils \

LOCAL_CFLAGS := \

LOCAL_MODULE:= radiooptions_sp
LOCAL_MODULE_TAGS := debug
LOCAL_MODULE_STEM_32 := radiooptions_sp
LOCAL_MODULE_STEM_64 := radiooptions_sp64
LOCAL_MULTILIB := both

include $(BUILD_EXECUTABLE)
