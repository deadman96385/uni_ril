#!/bin/make
#
#  THIS IS AN UNPUBLISHED WORK CONTAINING D2 TECHNOLOGIES, INC. CONFIDENTIAL
#  AND PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
#  APPLIES:
#  "COPYRIGHT 2004 D2 TECHNOLOGIES, INC. ALL RIGHTS RESERVED"
#
# $D2Tech$ $Rev: 20936 $ $Date: 2013-06-05 11:47:54 +0800 (Wed, 05 Jun 2013) $
#

# C Source files
CSRC    = 	\
		osal_log.c \
		osal_mem.c \
		osal_task.c \
		osal_random.c \
		osal_net.c \
		osal_select.c \
		osal_string.c \
		osal_tmr.c \
		osal_sem.c \
		osal_cond.c \
		osal_msg.c \
		osal_time.c \
		osal_file.c \
		osal_sys.c \
		osal_ipsec.c \
		osal_crypto.c

# Assembly files
SSRC    = \

# Private Header files
PRIVATE_HEADERS = \

# Files to export to INCLUDE_DIR
PUBLIC_HEADERS  =  \

OUTPUT        = archive

include $(TOOL_DIR)/rules.mk

# Build Rule - add custom build commands below this rule
build: default_build

# Clean Rule - add custom clean commands below this rule
clean: default_clean

# END OF MAKEFILE

