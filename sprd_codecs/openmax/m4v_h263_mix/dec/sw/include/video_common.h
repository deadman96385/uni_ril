/******************************************************************************
** File Name:      video_commmon.h                                           *
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
#ifndef _VIDEO_COMMON_H_
#define _VIDEO_COMMON_H_

/*----------------------------------------------------------------------------*
**                        Dependencies                                        *
**---------------------------------------------------------------------------*/
#include "sci_types.h"
/**---------------------------------------------------------------------------*
**                        Compiler Flag                                       *
**---------------------------------------------------------------------------*/
#ifdef   __cplusplus
extern   "C"
{
#endif

#ifdef _VSP_LINUX_
#define SCI_MEMSET  memset
#define SCI_MEMCPY	memcpy
#define SCI_ASSERT(...)
#define PRINTF(...)
#endif

#define ABS(x) ((x) > 0 ? (x) : -(x))
#define Clip3(min,max,val) (((val)<(min))?(min):(((val)>(max))?(max):(val)))

//for macroblock
#define MB_SIZE					16
#define MB_CHROMA_SIZE			8
#define MB_SQUARE_SIZE			256

//for block
#define BLOCK_SIZE				8
#define BLOCK_SQUARE_SIZE		64
#define BLOCK_CNT				6

#define mmax(aa,bb)		(((aa) > (bb)) ? (aa) : (bb))
#define mmin(aa,bb)		(((aa) < (bb)) ? (aa) : (bb))
#define ssign(x)		((x) > 0 ? 1 : (-1))
#define ssignof(x)		(((x) > 0) ? 1 : 0)
#define rounded(x)		(((x) > 0) ? (x + 0.5) :(x - 0.5))
#define IClip(Min, Max, Val) (((Val)<(Min))? (Min):(((Val)>(Max))? (Max):(Val)))
#define MEDIAN(aa, bb, cc) ((aa-bb)*(aa-cc)<=0)?aa :(((bb-cc)*(bb-aa)<=0)?bb:cc)

/**---------------------------------------------------------------------------*
**                         Compiler Flag                                      *
**---------------------------------------------------------------------------*/
#ifdef   __cplusplus
}
#endif
/**---------------------------------------------------------------------------*/
// End
#endif  //_VIDEO_COMMON_H_