LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := graphics.c

LOCAL_C_INCLUDES +=\
    external/libpng\
    external/zlib

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libatminui
include $(BUILD_STATIC_LIBRARY)

