ifeq ($(MBIM_DEVICE_MODULE),true)
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    mbim_device_base/mbim_cid.c \
    mbim_device_base/mbim_uuid.c \
    mbim_device_base/mbim_enum.c \
    mbim_device_base/mbim_message.c \
    mbim_device_base/mbim_message_processer.c \
    mbim_device_base/mbim_message_threads.c \
    mbim_device_base/mbim_service_basic_connect.c \
    mbim_device_base/mbim_service_sms.c \
    mbim_device_base/mbim_service_oem.c \
    mbim_device_base/thread_pool.c \

LOCAL_SRC_FILES += \
    mbim_device_vendor/misc.c \
    mbim_device_vendor/at_tok.c \
    mbim_device_vendor/at_channel.c \
    mbim_device_vendor/time_event_handler.c \
    mbim_device_vendor/mbim_device_config.c \
    mbim_device_vendor/mbim_device_vendor.c \
    mbim_device_vendor/mbim_device_basic_connect.c \
    mbim_device_vendor/mbim_device_oem.c \
    mbim_device_vendor/mbim_utils.c \
    mbim_device_vendor/mbim_device_sqlite.c \
    mbim_device_vendor/mbim_device_sms.c \
    mbim_device_vendor/tinyxml2.cpp \
    mbim_device_vendor/parse_apn.cpp \


LOCAL_SHARED_LIBRARIES := \
    liblog libcutils libutils libnetutils librilutils libsqlite

ifeq ($(PLATFORM_VERSION),$(filter $(PLATFORM_VERSION),4.4 4.4.1 4.4.2 4.4.3i 4.4.4))
include external/stlport/libstlport.mk
endif

LOCAL_C_INCLUDES += $(LOCAL_PATH)/mbim_device_base
LOCAL_C_INCLUDES += $(LOCAL_PATH)/mbim_device_vendor
LOCAL_C_INCLUDES    +=  external/sqlite/dist/

# for asprinf
LOCAL_CFLAGS := -D_GNU_SOURCE

LOCAL_MODULE:= libmbim-device-i-l
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -std=c99
include $(BUILD_SHARED_LIBRARY)

#################################
include $(CLEAR_VARS)
LOCAL_SRC_FILES:= \
    mbim_device_daemon/mbim_device_daemon.c

LOCAL_SHARED_LIBRARIES := \
    liblog libcutils libmbim-device-i-l libsqlite

ifeq ($(PLATFORM_VERSION),$(filter $(PLATFORM_VERSION),4.4 4.4.1 4.4.2 4.4.3i 4.4.4))
include external/stlport/libstlport.mk
endif

LOCAL_MODULE:= mbim-device
LOCAL_CFLAGS += -std=c99
include $(BUILD_EXECUTABLE)

endif    # !MBIM_DEVICE_MODULE