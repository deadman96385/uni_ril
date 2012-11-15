/******************************************************************************
 ** File Name:    mp4dec_mv.c                                              *
 ** Author:       Xiaowei Luo                                                 *
 ** DATE:         12/14/2006                                                  *
 ** Copyright:    2006 Spreatrum, Incoporated. All Rights Reserved.           *
 ** Description:                                                              *
 *****************************************************************************/
/******************************************************************************
 **                   Edit    History                                         *
 **---------------------------------------------------------------------------* 
 ** DATE          NAME            DESCRIPTION                                 * 
 ** 12/14/2006    Xiaowei Luo     Create.                                     *
 *****************************************************************************/
/*----------------------------------------------------------------------------*
**                        Dependencies                                        *
**---------------------------------------------------------------------------*/
#include "sc8810_video_header.h"
/*lint -save -e744 -e767*/
/**---------------------------------------------------------------------------*
**                        Compiler Flag                                       *
**---------------------------------------------------------------------------*/
#ifdef   __cplusplus
    extern   "C" 
    {
#endif
/*----------------------------------------------------------------------------*
**                            Mcaro Definitions                               *
**---------------------------------------------------------------------------*/

/**----------------------------------------------------------------------------*
**                           Function Prototype                               **
**----------------------------------------------------------------------------*/
/*****************************************************************************
 **	Name : 			Mp4Dec_Get16x16MVPred
 ** Description:	Get motion vector prediction in one mv condition. 
 ** Author:			Xiaowei Luo
 **	Note:
 *****************************************************************************/
PUBLIC void Mp4Dec_Get16x16MVPred(DEC_VOP_MODE_T *vop_mode_ptr, DEC_MB_MODE_T *mb_mode_ptr, MOTION_VECTOR_T *mvPred, int32 TotalMbNumX)
{
	int32 nInBound = 0;
	MOTION_VECTOR_T VecCand[3];
	DEC_MB_BFR_T *mb_cache_ptr = vop_mode_ptr->mb_cache_ptr;
	DEC_MB_MODE_T *pMb_mode_Left, *pMb_mode_Top, *pMb_mode_Right;
	
	((int32 *)VecCand)[0] = 0;
	((int32 *)VecCand)[1] = 0;
	((int32 *)VecCand)[2] = 0;
		
	if(mb_cache_ptr->bLeftMBAvail)
	{
		pMb_mode_Left = mb_mode_ptr - 1;
		VecCand[0] = pMb_mode_Left->mv[1];
		
		nInBound++;
	}
	
	if(mb_cache_ptr->bTopMBAvail)
	{
		pMb_mode_Top = mb_mode_ptr - TotalMbNumX;
		VecCand[nInBound] = pMb_mode_Top->mv[2];
		nInBound++;
	}
	
	if(mb_cache_ptr->rightAvail)
	{
		pMb_mode_Right = mb_mode_ptr - TotalMbNumX + 1;
		VecCand[nInBound] = pMb_mode_Right->mv[2];
		
		nInBound++;
	}
	
	if(nInBound == 1)
	{
		((int32 *)mvPred)[0] = ((int32 *)(VecCand))[0];
		return;
	}	
	
	mvPred->x = (int16)Mp4_GetMedianofThree(VecCand[0].x, VecCand[1].x, VecCand[2].x);
	mvPred->y = (int16)Mp4_GetMedianofThree(VecCand[0].y, VecCand[1].y, VecCand[2].y);	
}

/*****************************************************************************
 **	Name : 			Mp4Dec_Get8x8MVPredAtBndry
 ** Description:	Get motion vector prediction in 4mv condition when at boundary. 
 ** Author:			Xiaowei Luo
 **	Note:
 *****************************************************************************/
PUBLIC void Mp4Dec_Get8x8MVPredAtBndry(DEC_MB_MODE_T *mb_mode_ptr, DEC_MB_BFR_T *mb_cache_ptr, int32 blk_num, MOTION_VECTOR_T *mvPred, int32 TotalMbNumX)
{
	int32 nInBound = 0;
	MOTION_VECTOR_T VecCand[3];//, zeroMv = {0, 0};
	DEC_MB_MODE_T *pMb_mode_Left, *pMb_mode_Top, *pMb_mode_Right;
//	DEC_VOP_MODE_T *vop_mode_ptr = Mp4Dec_GetVopmode();

	((int32 *)VecCand)[0] = 0;
	((int32 *)VecCand)[1] = 0;
	((int32 *)VecCand)[2] = 0;

	pMb_mode_Left = mb_mode_ptr - 1;
	pMb_mode_Top = mb_mode_ptr - TotalMbNumX;
	pMb_mode_Right = pMb_mode_Top + 1;

	switch(blk_num)
	{
	case 0:
		if(mb_cache_ptr->bLeftMBAvail)
		{
			((int32 *)VecCand)[0] = ((int32 *)(pMb_mode_Left->mv))[1];
			nInBound++;			
		}

		if(mb_cache_ptr->bTopMBAvail)
		{
			((int32 *)VecCand)[nInBound] = ((int32 *)(pMb_mode_Top->mv))[2];
			nInBound++;
		}

		if(mb_cache_ptr->rightAvail)
		{
			((int32 *)VecCand)[nInBound] = ((int32 *)(pMb_mode_Right->mv))[2];
			nInBound++;
		}  
		break;

	case 1:
		((int32 *)VecCand)[0] = ((int32 *)(mb_mode_ptr->mv))[0];
		nInBound++;

		if(mb_cache_ptr->bTopMBAvail)
		{
			((int32 *)VecCand)[nInBound] = ((int32 *)(pMb_mode_Top->mv))[3];
			nInBound++;
		}

		if(mb_cache_ptr->rightAvail)
		{
			((int32 *)VecCand)[nInBound] = ((int32 *)(pMb_mode_Right->mv))[2];
			nInBound++;
		}		
		
		break;

	case 2:
		if(mb_cache_ptr->bLeftMBAvail)
		{
			((int32 *)VecCand)[0] = ((int32 *)(pMb_mode_Left->mv))[3];
			nInBound++;
		}

		((int32 *)VecCand)[nInBound] = ((int32 *)(mb_mode_ptr->mv))[0];
		nInBound++;		
		
		((int32 *)VecCand)[nInBound] = ((int32 *)(mb_mode_ptr->mv))[1];
		nInBound++;		

		break;

	case 3:
		((int32 *)VecCand)[0] = ((int32 *)(mb_mode_ptr->mv))[2];
		((int32 *)VecCand)[1] = ((int32 *)(mb_mode_ptr->mv))[0];
		((int32 *)VecCand)[2] = ((int32 *)(mb_mode_ptr->mv))[1];

		nInBound = 3;
		
		break;

	default:

		break;
	}

	if(nInBound == 1)
	{
		((int32 *)mvPred)[0] = ((int32 *)VecCand)[0];
		return;
	}	
	
	mvPred->x = (int16)Mp4_GetMedianofThree(VecCand[0].x, VecCand[1].x, VecCand[2].x);
	mvPred->y = (int16)Mp4_GetMedianofThree(VecCand[0].y, VecCand[1].y, VecCand[2].y);
}

/*****************************************************************************
 **	Name : 			Mp4Dec_Get8x8MVPredNotAtBndry
 ** Description:	Get motion vector prediction in 4mv condition when not at boundary. 
 ** Author:			Xiaowei Luo
 **	Note:
 *****************************************************************************/
PUBLIC void Mp4Dec_Get8x8MVPredNotAtBndry(DEC_MB_MODE_T *mb_mode_ptr, int32 blk_num, MOTION_VECTOR_T *mvPred, int32 TotalMbNumX)
{
	MOTION_VECTOR_T VecCand[3], zeroMv = {0, 0};
	DEC_MB_MODE_T *pMb_mode_Left, *pMb_mode_Top, *pMb_mode_Right;
	
	((int32 *)VecCand)[0] = 0;
	((int32 *)VecCand)[1] = 0;
	((int32 *)VecCand)[2] = 0;

	pMb_mode_Left = mb_mode_ptr - 1;
	pMb_mode_Top = mb_mode_ptr - TotalMbNumX;
	pMb_mode_Right = pMb_mode_Top + 1;

	switch(blk_num)
	{
	case 0:
		((int32 *)VecCand)[0] = ((int32 *)(pMb_mode_Left->mv))[1];		//left
		((int32 *)VecCand)[1] = ((int32 *)(pMb_mode_Top->mv))[2];		//top
		((int32 *)VecCand)[2] = ((int32 *)(pMb_mode_Right->mv))[2];//top right
		break;

	case 1:
		((int32 *)VecCand)[0] = ((int32 *)(mb_mode_ptr->mv))[0];
		((int32 *)VecCand)[1] = ((int32 *)(pMb_mode_Top->mv))[3];
		((int32 *)VecCand)[2] = ((int32 *)(pMb_mode_Right->mv))[2];
		break;

	case 2:
		((int32 *)VecCand)[0] = ((int32 *)(pMb_mode_Left->mv))[3];
		((int32 *)VecCand)[1] = ((int32 *)(mb_mode_ptr->mv))[0];
		((int32 *)VecCand)[2] = ((int32 *)(mb_mode_ptr->mv))[1];
		break;

	case 3:
		((int32 *)VecCand)[0] = ((int32 *)(mb_mode_ptr->mv))[2];
		((int32 *)VecCand)[1] = ((int32 *)(mb_mode_ptr->mv))[0];
		((int32 *)VecCand)[2] = ((int32 *)(mb_mode_ptr->mv))[1];
		break;

	default:
		break;
	}

	mvPred->x = (int16)Mp4_GetMedianofThree(VecCand[0].x, VecCand[1].x, VecCand[2].x);
	mvPred->y = (int16)Mp4_GetMedianofThree(VecCand[0].y, VecCand[1].y, VecCand[2].y);	
}

/*****************************************************************************
 **	Name : 			Mp4Dec_Get8x8MVPred
 ** Description:	Get motion vector prediction in 4mv condition. 
 ** Author:			Xiaowei Luo
 **	Note:
 *****************************************************************************/
PUBLIC void Mp4Dec_Get8x8MVPred(DEC_VOP_MODE_T *vop_mode_ptr, DEC_MB_MODE_T *mb_mode_ptr, int32 blk_num, MOTION_VECTOR_T *mvPred)
{
	DEC_MB_BFR_T *mb_cache_ptr = vop_mode_ptr->mb_cache_ptr;

	if(mb_cache_ptr->bLeftMBAvail && mb_cache_ptr->bTopMBAvail && mb_cache_ptr->rightAvail)
	{
		Mp4Dec_Get8x8MVPredNotAtBndry(mb_mode_ptr, blk_num, mvPred, vop_mode_ptr->MBNumX);
	}else
	{
		Mp4Dec_Get8x8MVPredAtBndry(mb_mode_ptr, mb_cache_ptr, blk_num, mvPred, vop_mode_ptr->MBNumX);
	}
}

/*****************************************************************************
 **	Name : 			Mp4Dec_DeScaleMVD
 ** Description:	DeScale motion vector. 
 ** Author:			Xiaowei Luo
 **	Note:
 *****************************************************************************/
PUBLIC void Mp4Dec_DeScaleMVD(int32 long_vectors, int32 f_code, MOTION_VECTOR_T *pMv, MOTION_VECTOR_T *pPredMv)
{
	int16 *pVector;
	int32 pred_vector;
	int32 comp;

	if (!long_vectors)
	{
		int32 r_size = f_code-1;
		int32 scale_factor = 1<<r_size;
		int32 range = 32*scale_factor;
		int32 low   = -range;
		int32 high  =  range-1;

		//x
		pVector = &(pMv->x);
		pred_vector = pPredMv->x;

		for (comp = 2; comp > 0; comp--)
		{
			*pVector = (int16)(pred_vector + *pVector);

			if (*pVector < low)
			{
				*pVector += (int16)(2*range);
			}else if (*pVector > high)
			{
				*pVector -= (int16)(2*range);
			}

			//y
			pVector = &(pMv->y);
			pred_vector = pPredMv->y;			
		}
	}else
	{
		//x 
		pVector = &(pMv->x);
		pred_vector = pPredMv->x;

		for (comp = 2; comp > 0; comp--)
		{
			if (*pVector > 31) 
			{
				*pVector -= 64;
			}

			*pVector = (int16)(pred_vector + *pVector);

			if (pred_vector < -31 && *pVector < -63)
			{
				*pVector += 64;
			}

			if (pred_vector > 32 && *pVector > 63)
			{
				  *pVector -= 64;
			}

			//y
			pVector = &(pMv->y);
			pred_vector = pPredMv->y;
		}
	}
}

/*****************************************************************************
 **	Name : 			Mp4Dec_DecodeOneMVD
 ** Description:	Get one motion vector from bitstream. 
 ** Author:			Xiaowei Luo
 **	Note:
 *****************************************************************************/
PUBLIC void Mp4Dec_DecodeOneMVD(DEC_VOP_MODE_T *vop_mode_ptr, MOTION_VECTOR_T *pMv, int32 fcode)
{
	int32 mvData;
	int32 mvResidual;
	int32 r_size = fcode-1;
	int32 scale_factor = 1<<r_size;
	DEC_BS_T *bitstrm_ptr = vop_mode_ptr->bitstrm_ptr;
	int32 sign;

	/*mv x*/
	mvData = Mp4Dec_VlcDecMV(vop_mode_ptr, bitstrm_ptr);

	if(fcode > 1 && mvData != 0)
	{
		mvResidual = (int32)Mp4Dec_ReadBits(bitstrm_ptr, (uint32)(fcode-1));//, "mv residual"
		sign = (mvData < 0) ? (-1) : 1;
		pMv->x = ((mvData - sign)<<r_size) + sign * (mvResidual + 1);		
	}else
	{
		pMv->x = mvData;
	}

   	 /*mv y*/
	mvData = Mp4Dec_VlcDecMV(vop_mode_ptr, bitstrm_ptr);
	
	if(fcode > 1 && mvData != 0)
	{
		mvResidual = (int32)Mp4Dec_ReadBits(bitstrm_ptr, (uint32)(fcode-1));//, "mv residual"
		sign = (mvData < 0) ? (-1) : 1;
		pMv->y = ((mvData - sign)<<r_size) + sign * (mvResidual + 1);	
	}else
	{
		pMv->y = mvData;
	}
}

/*****************************************************************************
 **	Name : 			Mp4Dec_DecodeOneMV
 ** Description:	Get one motion vector from bitstream. 
 ** Author:			Xiaowei Luo
 **	Note:
 *****************************************************************************/
PUBLIC void Mp4Dec_DecodeOneMV(DEC_VOP_MODE_T *vop_mode_ptr, MOTION_VECTOR_T *pMv, MOTION_VECTOR_T *pPredMv, int32 fcode)
{
	Mp4Dec_DecodeOneMVD(vop_mode_ptr, pMv, fcode);	/*get mv difference*/	
	Mp4Dec_DeScaleMVD(vop_mode_ptr->long_vectors, fcode, pMv, pPredMv);
}

/**---------------------------------------------------------------------------*
**                         Compiler Flag                                      *
**---------------------------------------------------------------------------*/
#ifdef   __cplusplus
    }
#endif
/**---------------------------------------------------------------------------*/
// End 
