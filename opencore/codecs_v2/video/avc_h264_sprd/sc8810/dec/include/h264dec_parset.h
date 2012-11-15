/******************************************************************************
** File Name:      h264dec_parset.h                                          *
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
#ifndef _H264DEC_PARSET_H_
#define _H264DEC_PARSET_H_

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

PUBLIC void H264Dec_use_parameter_set (DEC_IMAGE_PARAMS_T *img_ptr, int32 pps_id);
PUBLIC void H264Dec_interpret_sei_message (void);
PUBLIC MMDecRet H264Dec_process_sps (DEC_IMAGE_PARAMS_T *img_ptr);
PUBLIC MMDecRet H264Dec_process_pps (DEC_IMAGE_PARAMS_T *img_ptr);

/**---------------------------------------------------------------------------*
**                         Compiler Flag                                      *
**---------------------------------------------------------------------------*/
#ifdef   __cplusplus
    }
#endif
/**---------------------------------------------------------------------------*/
// End 
#endif  //_H264DEC_PARSET_H_