/*
 * THIS IS AN UNPUBLISHED WORK CONTAINING D2 TECHNOLOGIES, INC. CONFIDENTIAL
 * AND PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
 * APPLIES: "COPYRIGHT 2004 D2 TECHNOLOGIES, INC. ALL RIGHTS RESERVED"
 *
 * $D2Tech$ $Rev: 12486 $ $Date: 2010-07-08 06:10:49 +0800 (Thu, 08 Jul 2010) $
 *
 */

#include <osal.h>

#ifndef __XCAP_DBG_H_
#define __XCAP_DBG_H_

#ifdef XCAP_DBG_LOG
#define _XCAP_DBG(x,y) OSAL_logMsg("%s:%d\n",x,y)
#else
#define _XCAP_DBG(x,y)
#endif

#endif

