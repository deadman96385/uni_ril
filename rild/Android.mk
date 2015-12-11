# Copyright 2006 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES :=$(LOCAL_PATH)/../include

LOCAL_SRC_FILES:= \
    rild.c


LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils \
    libril \
    libdl

# temporary hack for broken vendor rils
LOCAL_WHOLE_STATIC_LIBRARIES := \
    librilutils_static

LOCAL_CFLAGS := -DRIL_SHLIB

LOCAL_MODULE:= rild
LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/libril
LOCAL_MODULE_STEM_32 := rild_32
LOCAL_MODULE_STEM_64 := rild_64
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

LOCAL_MODULE:= radiooptions
LOCAL_MODULE_TAGS := debug
LOCAL_MODULE_STEM_32 := radiooptions_32
LOCAL_MODULE_STEM_64 := radiooptions_64
LOCAL_MULTILIB := both

include $(BUILD_EXECUTABLE)
