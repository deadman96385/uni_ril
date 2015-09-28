LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	main.c \
	cluster.c \
	io.c \
	log.c \
	lookup.c \
	mount.c \
	node.c \
	time.c \
	utf.c \
	utils.c

LOCAL_CFLAGS:= -O2 -g -W -Wall -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DHAVE_CONFIG_H
LOCAL_MODULE:= exfatfsck
LOCAL_MODULE_TAGS:=
LOCAL_SYSTEM_SHARED_LIBRARIES:= libc

include $(BUILD_EXECUTABLE)

CUSTOM_MODULES += exfatfsck
