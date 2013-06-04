/******************************************************************************
** File Name:      h264dec_biaridecod.h		                                 *
** Author:         Xiaowei Luo                                               *
** DATE:           12/06/2007                                                *
** Copyright:      2007 Spreatrum, Incoporated. All Rights Reserved.         *
** Description:    common define for video codec.	     			          *
*****************************************************************************/
/******************************************************************************
**                   Edit    History                                         *
**---------------------------------------------------------------------------* 
** DATE          NAME            DESCRIPTION                                 * 
** 11/20/2007    Xiaowei Luo     Create.                                     *
*****************************************************************************/
#ifndef _H264DEC_BIARIDECOD_H_
#define _H264DEC_BIARIDECOD_H_

/*----------------------------------------------------------------------------*
**                        Dependencies                                        *
**---------------------------------------------------------------------------*/
#include "video_common.h"
/**---------------------------------------------------------------------------*
**                        Compiler Flag                                       *
**---------------------------------------------------------------------------*/
#ifdef   __cplusplus
extern   "C" 
{
#endif

uint32 ff_init_cabac_decoder(DEC_IMAGE_PARAMS_T *img_ptr);
uint32 get_cabac (CABACContext *c, uint8 * const state);
int32 get_cabac_terminate (DEC_IMAGE_PARAMS_T *img_ptr);
int32 get_cabac_bypass_sign(CABACContext *c, int val);
int32 get_cabac_bypass(CABACContext *c);

/**---------------------------------------------------------------------------*
**                         Compiler Flag                                      *
**---------------------------------------------------------------------------*/
#ifdef   __cplusplus
    }
#endif
/**---------------------------------------------------------------------------*/
// End 
#endif  //_H264DEC_BIARIDECOD_H_
