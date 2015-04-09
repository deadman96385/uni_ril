#!/bin/make
#
#  THIS IS AN UNPUBLISHED WORK CONTAINING D2 TECHNOLOGIES, INC. CONFIDENTIAL
#  AND PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
#  APPLIES:
#  "COPYRIGHT 2004 D2 TECHNOLOGIES, INC. ALL RIGHTS RESERVED"
#
# $D2Tech$ $Date: 2012-09-11 05:18:23 +0800 (Tue, 11 Sep 2012) $ $Revision: 18134 $
#

# C Source files
CSRC	= \
		_isi_server.c \
		isi_server.c 
ifeq ($(ISI_RPC_MSG_Q),y)
CSRC	+= \
		_isi_server_rpc_msg.c
else
CSRC	+= \
		_isi_server_rpc_vpmd.c
endif

# Assembly files
SSRC	= \

# Private Header files
PRIVATE_HEADERS	= \
		_isi_server.h \

# Files to export to INCLUDE_DIR
PUBLIC_HEADERS	=  \

# Files to export to OBJ_DIR
OUTPUT			= archive

include $(TOOL_DIR)/rules.mk

# Build Rule - add custom build commands below this rule
build: default_build

# Clean Rule - add custom clean commands below this rule
clean: default_clean


# END OF MAKEFILE

