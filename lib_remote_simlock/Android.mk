LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES := $(LOCAL_PATH)

LOCAL_MODULE := lib_remote_simlock

LOCAL_SRC_FILES := \
                   at_toc.c \
                   subsidy.cpp \
                   subsidylock_jni.cpp


LOCAL_MODULE_TAGS := optional
LOCAL_SHARED_LIBRARIES := libcutils \
                          libutils \
                          libatci

LOCAL_LDFLAGS   := -llog -ldl
LOCAL_CPPFLAGS += $(JNI_CFLAGS)

include $(BUILD_SHARED_LIBRARY)
