/******************************************************************************
** File Name:      h264dec_mb.h				                                 *
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
#ifndef _H264DEC_MB_H_
#define _H264DEC_MB_H_

/*----------------------------------------------------------------------------*
**                        Dependencies                                        *
**---------------------------------------------------------------------------*/
#include "video_common.h"
#if _CMODEL_
#include "bsm_global.h"
#include "common_global.h"
#endif //_CMODEL_
/**---------------------------------------------------------------------------*
**                        Compiler Flag                                       *
**---------------------------------------------------------------------------*/
#ifdef   __cplusplus
extern   "C" 
{
#endif

PUBLIC void fill_cache (DEC_IMAGE_PARAMS_T *img_ptr, DEC_MB_INFO_T *mb_info_ptr, DEC_MB_CACHE_T *mb_cache_ptr);
PUBLIC void H264Dec_fill_intraMB_context (DEC_IMAGE_PARAMS_T *img_ptr, DEC_MB_INFO_T *mb_info_ptr, DEC_MB_CACHE_T *mb_cache_ptr);
PUBLIC void H264Dec_interpret_mb_mode_B (DEC_IMAGE_PARAMS_T *img_ptr, DEC_MB_INFO_T *mb_info_ptr, DEC_MB_CACHE_T *mb_cache_ptr);
PUBLIC void H264Dec_read_CBPandDeltaQp(DEC_IMAGE_PARAMS_T *img_ptr, DEC_MB_INFO_T *mb_info_ptr, DEC_MB_CACHE_T *mb_cache_ptr);
PUBLIC void H264Dec_set_IPCM_nnz(DEC_MB_CACHE_T *mb_cache_ptr);
PUBLIC void H264Dec_interpret_mb_mode_I (DEC_MB_INFO_T *mb_info_ptr, DEC_MB_CACHE_T *mb_cache_ptr);
PUBLIC void H264Dec_interpret_mb_mode_P (DEC_IMAGE_PARAMS_T *img_ptr, DEC_MB_INFO_T *mb_info_ptr, DEC_MB_CACHE_T *mb_cache_ptr);

PUBLIC void H264Dec_start_macroblock (DEC_IMAGE_PARAMS_T *img_ptr, DEC_MB_INFO_T *mb_info_ptr, DEC_MB_CACHE_T *mb_cache_ptr);
PUBLIC void H264Dec_read_one_macroblock_ISlice_hw (DEC_IMAGE_PARAMS_T *img_ptr, DEC_MB_INFO_T *mb_info_ptr, DEC_MB_CACHE_T *mb_cache_ptr);
PUBLIC void H264Dec_read_one_macroblock_PSlice_hw (DEC_IMAGE_PARAMS_T *img_ptr, DEC_MB_INFO_T *mb_info_ptr, DEC_MB_CACHE_T *mb_cache_ptr);
PUBLIC void H264Dec_read_one_macroblock_BSlice_hw (DEC_IMAGE_PARAMS_T *img_ptr, DEC_MB_INFO_T *mb_info_ptr, DEC_MB_CACHE_T *mb_cache_ptr);
PUBLIC void H264Dec_read_one_macroblock_ISlice_sw (DEC_IMAGE_PARAMS_T *img_ptr, DEC_MB_INFO_T *mb_info_ptr, DEC_MB_CACHE_T *mb_cache_ptr);
PUBLIC void H264Dec_read_one_macroblock_PSlice_sw (DEC_IMAGE_PARAMS_T *img_ptr, DEC_MB_INFO_T *mb_info_ptr, DEC_MB_CACHE_T *mb_cache_ptr);
PUBLIC void H264Dec_read_one_macroblock_BSlice_sw (DEC_IMAGE_PARAMS_T *img_ptr, DEC_MB_INFO_T *mb_info_ptr, DEC_MB_CACHE_T *mb_cache_ptr);
PUBLIC void H264Dec_start_vld_macroblock (DEC_IMAGE_PARAMS_T *img_ptr, DEC_MB_INFO_T *mb_info_ptr, DEC_MB_CACHE_T *mb_cache_ptr);
PUBLIC void H264Dec_start_iqt_macroblock (DEC_IMAGE_PARAMS_T *img_ptr, DEC_MB_INFO_T *mb_info_ptr, DEC_MB_CACHE_T *mb_cache_ptr);
PUBLIC int32 H264Dec_exit_macroblock (DEC_IMAGE_PARAMS_T *img_ptr, DEC_MB_INFO_T *mb_info_ptr, DEC_MB_CACHE_T *mb_cache_ptr);
PUBLIC void H264Dec_start_mbc_macroblock (DEC_IMAGE_PARAMS_T *img_ptr, DEC_MB_INFO_T *mb_info_ptr, DEC_MB_CACHE_T *mb_cache_ptr);
PUBLIC uint32 uvlc_startcode_follows (DEC_IMAGE_PARAMS_T *img_ptr);

PUBLIC int32 decode_cavlc_mb_type (DEC_IMAGE_PARAMS_T *img_ptr, DEC_MB_INFO_T *mb_info_ptr, DEC_MB_CACHE_T *mb_cache_ptr);
PUBLIC int32 decode_cabac_mb_type (DEC_IMAGE_PARAMS_T *img_ptr, DEC_MB_INFO_T *mb_info_ptr, DEC_MB_CACHE_T *mb_cache_ptr);

PUBLIC uint32 readB8_typeInfo_cavlc (DEC_IMAGE_PARAMS_T *img_ptr, DEC_MB_CACHE_T *mb_cache_ptr, int32 blk8x8Nr);

/**---------------------------------------------------------------------------*
**                         Compiler Flag                                      *
**---------------------------------------------------------------------------*/
#ifdef   __cplusplus
    }
#endif
/**---------------------------------------------------------------------------*/
// End 
#endif  //_H264DEC_MB_H_
