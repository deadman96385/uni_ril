LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

#ifeq ($(strip $(TARGET_BOARD_PLATFORM)),sc8810)
#include $(LOCAL_PATH)/sc8810/Android.mk
#endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM)),sc7710)
include $(LOCAL_PATH)/sc8810/Android.mk
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM)),sc8825)
include $(LOCAL_PATH)/sc8825/Android.mk
endif

ifeq ($(strip $(TARGET_BOARD_PLATFORM)),sc8830)
include $(LOCAL_PATH)/sc8830/Android.mk
endif


