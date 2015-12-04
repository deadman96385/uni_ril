# Copyright 2006 The Android Open Source Project

# XXX using libutils for simulator build only...
#
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    reference-ril.c \
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
    ril_utils.c

LOCAL_SHARED_LIBRARIES := \
    liblog libcutils libutils libril librilutils

# for asprinf
LOCAL_CFLAGS := -D_GNU_SOURCE

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../include

LOCAL_CFLAGS += -DSIM_AUTO_POWERON

ifneq ($(SIM_COUNT), 1)
LOCAL_CFLAGS += -DANDROID_MULTI_SIM
endif

ifeq ($(SIM_COUNT), 2)
    LOCAL_CFLAGS += -DANDROID_SIM_COUNT_2
endif

ifeq (foo,foo)
#build shared library
LOCAL_SHARED_LIBRARIES += \
        libcutils libutils
LOCAL_CFLAGS += -DRIL_SHLIB
LOCAL_MODULE:= libreference-ril
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
else
#build executable
LOCAL_MODULE:= reference-ril
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_STEM_32 := reference-ril
LOCAL_MODULE_STEM_64 := reference-ril
LOCAL_MULTILIB := both
include $(BUILD_EXECUTABLE)
endif
