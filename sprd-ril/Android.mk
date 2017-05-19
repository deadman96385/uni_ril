# Copyright 2006 The Android Open Source Project

# XXX using libutils for simulator build only...
#
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    sprd_ril.c \
    atchannel.c \
    misc.c \
    at_tok.c \
    ril_sim.c \
    ril_network.c \
    ril_data.c \
    ril_call.c \
    ril_ss.c \
    ril_sms.c \
    ril_misc.c \
    ril_stk.c \
    ril_utils.c \
    custom/ril_custom.c \
    ril_async_cmd_handler.c \
    channel_controller.c \
    request_threads.c \

LOCAL_SHARED_LIBRARIES := \
    liblog libcutils libutils librilsprd librilutils libril_tele libnetutils

# for asprinf
LOCAL_CFLAGS := -D_GNU_SOURCE

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../libril_tele

LOCAL_CFLAGS += -DSIM_AUTO_POWERON  -DRIL_EXTENSION

ifneq ($(SIM_COUNT), 1)
    LOCAL_CFLAGS += -DANDROID_MULTI_SIM
endif

ifeq ($(SIM_COUNT), 2)
    LOCAL_CFLAGS += -DANDROID_SIM_COUNT_2
endif

LOCAL_PROPRIETARY_MODULE := true

ifeq (foo,foo)
#build shared library
LOCAL_SHARED_LIBRARIES += \
        libcutils libutils
LOCAL_CFLAGS += -DRIL_SHLIB
LOCAL_MODULE:= libsprd-ril
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
else
#build executable
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE:= libsprd-ril
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_STEM_32 := libsprd-ril
LOCAL_MODULE_STEM_64 := libsprd-ril64
LOCAL_MULTILIB := both
include $(BUILD_EXECUTABLE)
endif
