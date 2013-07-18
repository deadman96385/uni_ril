/******************************************************************************
 ** File Name:      mp4dec_vop.h                                           *
 ** Author:         Xiaowei Luo                                               *
 ** DATE:           12/14/2006                                                *
 ** Copyright:      2006 Spreatrum, Incoporated. All Rights Reserved.         *
 ** Description:    This file defines the operation interfaces of vop         *
 **                 of mp4 deccoder.                                          *
 *****************************************************************************/
/******************************************************************************
 **                   Edit    History                                         *
 **---------------------------------------------------------------------------* 
 ** DATE          NAME            DESCRIPTION                                 * 
 ** 12/14/2006    Xiaowei Luo     Create.                                     *
 *****************************************************************************/
#ifndef _MP4DEC_VOP_H_
#define _MP4DEC_VOP_H_
/*----------------------------------------------------------------------------*
**                        Dependencies                                        *
**---------------------------------------------------------------------------*/
#include "mp4_basic.h"
#include "mp4dec_mode.h"
#include "mp4dec_bitstream.h"
/**---------------------------------------------------------------------------*
**                        Compiler Flag                                       *
**---------------------------------------------------------------------------*/
#ifdef   __cplusplus
    extern   "C" 
    {
#endif

void Mp4Dec_exit_picture(MP4DecObject *vd, DEC_VOP_MODE_T *vop_mode_ptr);
void Mp4Dec_ExchangeMBMode (DEC_VOP_MODE_T * vop_mode_ptr);
void MP4Dec_JudgeDecMode (DEC_VOP_MODE_T * vop_mode_ptr);
MMDecRet Mp4Dec_InitVop(MP4DecObject *vd, DEC_VOP_MODE_T *vop_mode_ptr, MMDecInput * dec_input_ptr);

void write_display_frame(DEC_VOP_MODE_T *vop_mode_ptr,DEC_FRM_BFR *pDecFrame);
void Mp4Dec_ExtendFrame(DEC_VOP_MODE_T *vop_mode_ptr );
void Mp4Dec_output_one_frame (MP4DecObject *vd, MMDecOutput *dec_output_ptr, DEC_VOP_MODE_T *vop_mode_ptr);

MMDecRet Mp4Dec_frm_level_sync_hw_sw_pipeline (MP4DecObject *vd, MMDecInput *dec_input_ptr, MMDecOutput *dec_output_ptr, DEC_VOP_MODE_T *vop_mode_ptr);
MMDecRet Mp4Dec_frm_level_sync_hw_sw_normal (MP4DecObject *vd, MMDecInput *dec_input_ptr, MMDecOutput *dec_output_ptr, DEC_VOP_MODE_T *vop_mode_ptr);

MMDecRet Mp4Dec_DecIVOP_sw(MP4DecObject *vd, DEC_VOP_MODE_T *vop_mode_ptr);
MMDecRet Mp4Dec_DecPVOP_sw(MP4DecObject *vd, DEC_VOP_MODE_T *vop_mode_ptr);
MMDecRet Mp4Dec_DecBVOP_sw(MP4DecObject *vd, DEC_VOP_MODE_T *vop_mode_ptr);

MMDecRet Mp4Dec_DecIVOP_hw(MP4DecObject *vd, DEC_VOP_MODE_T *vop_mode_ptr);
MMDecRet Mp4Dec_DecPVOP_hw(MP4DecObject *vd, DEC_VOP_MODE_T *vop_mode_ptr);
MMDecRet Mp4Dec_DecBVOP_hw(MP4DecObject *vd, DEC_VOP_MODE_T *vop_mode_ptr);

MMDecRet Mp4Dec_DecIVOP_vt(MP4DecObject *vd, DEC_VOP_MODE_T *vop_mode_ptr);
MMDecRet Mp4Dec_DecPVOP_vt(MP4DecObject *vd, DEC_VOP_MODE_T *vop_mode_ptr);
 MMDecRet Mp4Dec_DecBVOP_vt(MP4DecObject *vd, DEC_VOP_MODE_T * vop_mode_ptr);

PUBLIC void Mp4Dec_Deblock_vop(MP4DecObject *vd, DEC_VOP_MODE_T *vop_mode_ptr);

/**---------------------------------------------------------------------------*
**                         Compiler Flag                                      *
**---------------------------------------------------------------------------*/
#ifdef   __cplusplus
    }
#endif
/**---------------------------------------------------------------------------*/
// End 
#endif  //_MP4DEC_VOP_H_