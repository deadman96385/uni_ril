#!/bin/make
# THIS IS AN UNPUBLISHED WORK CONTAINING D2 TECHNOLOGIES, INC. CONFIDENTIAL
# AND PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
# APPLIES: "COPYRIGHT 2013 D2 TECHNOLOGIES, INC. ALL RIGHTS RESERVED"
#
# $D2Tech$ $Rev$ $Date$
#

#
# This file is for module level configuration
#
# -------- You may modify this file to include customizations
#

#export CDEBUG  = -g

#
export MODULE_SUB_DIRS		:= 	\
		../test\

export MODULE_PREFIX	= VPMD_TEST_ECHO
ifeq ($(VPORT_OS),threadx)
export MODULE_OUTPUT    = rlib
else
export MODULE_OUTPUT    = bare-exe
endif
export MODULE_OTHER_LIBS        +=    \
        $(LIB_DIR)/osal_user.lib   \
		$(LIB_DIR)/modemDriver_vpmd.lib \
		$(LIB_DIR)/modemDriver_vpmd_mux.lib 

ifeq ($(VPORT_OS),linux) 
export MODULE_CDEFINES	= $(CDEFINES_ADD_FOR_USERLAND)
export MODULE_CFLAGS	= $(CFLAGS_ADD_FOR_USERLAND)
endif

ifeq ($(VPORT_OS),linux_pc)
export MODULE_CDEFINES	= $(CDEFINES_ADD_FOR_USERLAND)
export MODULE_CFLAGS	= $(CFLAGS_ADD_FOR_USERLAND)
endif

# END OF MAKEFILE
