LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
        channel_manager.c \
        at_tok.c \
        cmux.c \
        pty.c \
        send_thread.c \
        receive_thread.c \
        adapter.c \
        ps_service.c

LOCAL_SHARED_LIBRARIES := \
        libcutils \
        libhardware_legacy \
        libnetutils

LOCAL_CFLAGS := -DANDROID_CHANGES -DEBUG
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS_arm := -marm

ifeq ($(SIM_COUNT), 1)
    LOCAL_CFLAGS += -DSIM_COUNT_PHONESERVER_1
endif

ifeq ($(SIM_COUNT), 2)
    LOCAL_CFLAGS += -DSIM_COUNT_PHONESERVER_2
endif

LOCAL_MODULE := phoneserver
LOCAL_INIT_RC := phoneserver.rc

include $(BUILD_EXECUTABLE)
