/******************************************************************************
** File Name:      h264dec_isqt.h                                            *
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
#ifndef _H264DEC_ISQT_H_
#define _H264DEC_ISQT_H_

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


void itrans_lumaDC (H264DecContext *vo, int16 *DCCoeff, int16 *pCoeffIq, int32 qp);
void itrans_4x4 (int16 *coff, uint8 *pred, int32 width_p, uint8 *rec, int32 width_r);
void itrans_8x8 (int16 *coff, uint8 *pred, int32 width_p, uint8 *rec, int32 width_r);
/**---------------------------------------------------------------------------*
**                         Compiler Flag                                      *
**---------------------------------------------------------------------------*/
#ifdef   __cplusplus
}
#endif
/**---------------------------------------------------------------------------*/
// End
#endif  //_H264DEC_ISQT_H_