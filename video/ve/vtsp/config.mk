# THIS IS AN UNPUBLISHED WORK CONTAINING D2 TECHNOLOGIES, INC. CONFIDENTIAL
# AND PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
# APPLIES: "COPYRIGHT 2004 D2 TECHNOLOGIES, INC. ALL RIGHTS RESERVED"
#
# $D2Tech$ $Rev: 28520 $ $Date: 2014-08-27 19:26:41 +0800 (Wed, 27 Aug 2014) $
#

#
# This file is for module level configuration
#
# -------- You may modify this file to include customizations
#

#
# List dirs in order for making
MY_MODULE_SUB_DIRS := 	\
	vtsp_public \
	vtsp_private

MY_CFLAGS := \
	-DOSAL_PTHREADS \
	-DANDROID_ICS

# END OF MAKEFILE
