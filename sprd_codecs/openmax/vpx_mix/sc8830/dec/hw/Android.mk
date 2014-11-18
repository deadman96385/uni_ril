LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	src/vp8dec_bfrctrl.c \
	src/vp8dec_dboolhuff.c \
	src/vp8dec_dequant.c \
	src/vp8dec_frame.c \
	src/vp8dec_global.c \
	src/vp8dec_init.c \
	src/vp8dec_interface.c \
	src/vp8dec_malloc.c \
	src/vp8dec_table.c	\
	src/vp8dec_vld.c \
	src/vsp_drv_sc8830.c


LOCAL_MODULE := libomx_vpxdec_hw_sprd
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS :=  -fno-strict-aliasing  -DVP8_DEC -D_VSP_LINUX_  -D_VSP_  -DCHIP_ENDIAN_LITTLE  -DCHIP_8830

LOCAL_ARM_MODE := arm

LOCAL_SHARED_LIBRARIES := \
	libutils liblog

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include

include $(BUILD_SHARED_LIBRARY)
