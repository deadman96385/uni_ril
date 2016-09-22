#!/bin/make
#
#  THIS IS AN UNPUBLISHED WORK CONTAINING D2 TECHNOLOGIES, INC. CONFIDENTIAL
#  AND PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
#  APPLIES:
#  "COPYRIGHT 2007 D2 TECHNOLOGIES, INC. ALL RIGHTS RESERVED"
#
# $D2Tech$ $Rev: 5072 $ $Date: 2008-01-08 11:40:09 -0600 (Tue, 08 Jan 2008) $
#

# C Source files
CSRC	= \
		fsm_base.c \
		_fsm_active.c \
		_fsm_authFail.c \
		_fsm_error.c \
		_fsm_login.c \
		_fsm_reset.c \
		_fsm_logout.c
		
# Assembly files
SSRC	= \

# Private Header files
PRIVATE_HEADERS	= \

# Files to export to INCLUDE_DIR
PUBLIC_HEADERS	=  \
		fsm.h \

# Files to export to OBJ_DIR
OUTPUT		= archive

include $(TOOL_DIR)/rules.mk

# Build Rule - add custom build commands below this rule
build: default_build

# Clean Rule - add custom clean commands below this rule
clean: default_clean


# END OF MAKEFILE
