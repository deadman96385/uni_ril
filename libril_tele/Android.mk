

LOCAL_PATH:= $(call my-dir)
# HAL module implemenation stored in
# hw/<COPYPIX_HARDWARE_MODULE_ID>.<ro.board.platform>.so
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
        ITelephonyEx.cpp \
        TelephonyEx.cpp \

LOCAL_SHARED_LIBRARIES := libcutils libbinder libutils liblog

LOCAL_PROPRIETARY_MODULE := true

LOCAL_MODULE := libril_tele

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
