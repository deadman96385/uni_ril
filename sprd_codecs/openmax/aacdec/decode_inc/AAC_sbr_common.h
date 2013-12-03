/*************************************************************************
** File Name:      sbr_common.h                                          *
** Author:         Reed zhang                                            *
** Date:           24/01/2006                                            *
** Copyright:      2001 Spreatrum, Incoporated. All Rights Reserved.     *
** Description:    This file defines the common function and variation   *
**                 for SBR dec.                                          *
**                        Edit History                                   *
** ----------------------------------------------------------------------*
** DATE           NAME             DESCRIPTION                           *
** 24/01/2006     Reed zhang       Create.                               *
**************************************************************************/
#ifndef __SBR_COMMON_H__
#define __SBR_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif
#include "aac_common.h"

	
#define BIT_BOTTOM_VALUE (1<<8)
#define FIXED_POINT_1    (1<<8)

#define MAX_M       49
#define MAX_L_E      5
#define REAL_BITS 14
#define TABLE_BITS 6
	/* just take the maximum number of bits for interpolation */
#define INTERP_BITS (REAL_BITS-TABLE_BITS)


uint8 GetBitCount(int32 *data);
#define E31  (1<<31 - 1)
#define E16  (1<<16)
#define E15  (1<<15)
#define E23  (1<<23)
#define FUN_BIT_SHIFT(D, bit) ((bit > 0)? (D <<= bit): (D>>=(-bit)))



#ifdef AAC_SBR_LOW_POWER
#define DCT_BIT   5
#else
#define DCT_BIT   6
#endif


int32 AAC_DEC_MultiplyShiftR32(int32 x, int32 y);

/*
 the input data:  S5.14
 the output data: S32.0
*/
extern int32 AAC_DEC_ARM_Pow2IntAsm(
                             int32 val,
                             int16 *table_ptr);
/*
 the input data:  S18.14
 the output data: S18.14  
*/                             
extern int32 AAC_DEC_ARM_Log2IntAsm(
                             int32 val,
                             int16 *table_ptr);
/*
 the input data:  S5.14
 the output data: S18.14  
*/
extern int32 AAC_DEC_ARM_Pow2IntOut14bitAsm(
                             int32 val,
                             int16 *table_ptr);

/* perform the square operation a^0.5 */
int32 AAC_DEC_Sqrt(int32 a);
int32 AAC_DEC_Normalize(int16 nbit, int32 b);
#ifdef __cplusplus
}
#endif
#endif