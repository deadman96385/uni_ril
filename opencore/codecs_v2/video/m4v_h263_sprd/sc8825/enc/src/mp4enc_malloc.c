/******************************************************************************
 ** File Name:    mp4dec_malloc.c                                             *
 ** Author:       Xiaowei Luo                                                 *
 ** DATE:         01/23/2007                                                  *
 ** Copyright:    2006 Spreatrum, Incoporated. All Rights Reserved.           *
 ** Description:                                                              *
 *****************************************************************************/
/******************************************************************************
 **                   Edit    History                                         *
 **---------------------------------------------------------------------------* 
 ** DATE          NAME            DESCRIPTION                                 * 
 ** 01/23/2007    Xiaowei Luo     Create.                                     *
 *****************************************************************************/
/*----------------------------------------------------------------------------*
**                        Dependencies                                        *
**---------------------------------------------------------------------------*/
#include "sc8825_video_header.h"
/**---------------------------------------------------------------------------*
**                        Compiler Flag                                       *
**---------------------------------------------------------------------------*/
#ifdef   __cplusplus
    extern   "C" 
    {
#endif

//for mp4 encoder memory. 4Mbyte,extra
//MP4_LOCAL uint32 s_Mp4EncExtraMemUsed = 0x0;
//MP4_LOCAL uint32 s_Mp4EncExtraMemSize = 0x0; // = 0x400000;  //16M

//for mp4 decoder memory. 4Mbyte,inter
//MP4_LOCAL uint32 s_Mp4EncInterMemUsed = 0x0;
//MP4_LOCAL uint32 s_Mp4EncInterMemSize = 0x0; //50kbyte; // = 0x400000;

//MP4_LOCAL uint8 *s_pEnc_Extra_buffer = NULL; 
//MP4_LOCAL uint8 *s_pEnc_Inter_buffer = NULL;

#ifdef _VSP_LINUX_
//LOCAL uint8 *s_extra_mem_bfr_phy_ptr = NULL;
PUBLIC uint8 *Mp4Enc_ExtraMem_V2Phy(MP4EncHandle* mp4Handle,uint8 *vAddr)
{
	Mp4EncObject*vd = (Mp4EncObject *) mp4Handle->videoEncoderData;
	return (vAddr-vd->s_pEnc_Extra_buffer)+vd->s_extra_mem_bfr_phy_ptr;
}
#endif
/*****************************************************************************
 **	Name : 			Mp4Enc_ExtraMemAlloc
 ** Description:	Alloc the common memory for mp4 decoder. 
 ** Author:			Xiaowei Luo
 **	Note:
 *****************************************************************************/
void *Mp4Enc_ExtraMemAlloc(MP4EncHandle* mp4Handle,uint32 mem_size)
{
	Mp4EncObject*vd = (Mp4EncObject *) mp4Handle->videoEncoderData;
	uint8 *pMem;
	mem_size = ((mem_size + 3) &(~3));

	if((0 == mem_size)||(mem_size> (vd->s_Mp4EncExtraMemSize-vd->s_Mp4EncExtraMemUsed)))
	{
		SCI_TRACE_LOW("Mp4Enc_ExtraMemAlloc failed");
		SCI_ASSERT(0);	
		return 0;
	}
	
	pMem = vd->s_pEnc_Extra_buffer + vd->s_Mp4EncExtraMemUsed;
	vd->s_Mp4EncExtraMemUsed += mem_size;
	
	return pMem;
}

/*****************************************************************************
 **	Name : 			Mp4Enc_ExtraMemAlloc_64WordAlign
 ** Description:	Alloc the common memory for mp4 encoder. 
 ** Author:			Xiaowei Luo
 **	Note:
 *****************************************************************************/
PUBLIC void *Mp4Enc_ExtraMemAlloc_64WordAlign(MP4EncHandle* mp4Handle,uint32 mem_size)
{
	Mp4EncObject*vd = (Mp4EncObject *) mp4Handle->videoEncoderData;
	uint32 CurrAddr, _64WordAlignAddr;
		
	CurrAddr = (uint32)vd->s_pEnc_Extra_buffer + vd->s_Mp4EncExtraMemUsed;

	_64WordAlignAddr = ((CurrAddr + 255) >>8)<<8;

	mem_size += (_64WordAlignAddr - CurrAddr);

	if((0 == mem_size)||(mem_size> (vd->s_Mp4EncExtraMemSize-vd->s_Mp4EncExtraMemUsed)))
	{
		SCI_TRACE_LOW("Mp4Enc_ExtraMemAlloc_64WordAlign failed");
		SCI_ASSERT(0);
		return 0;
	}
	
	vd->s_Mp4EncExtraMemUsed += mem_size;
	
	return (void *)_64WordAlignAddr;
}

/*****************************************************************************
 **	Name : 			Mp4Enc_InterMemAlloc
 ** Description:	Alloc the common memory for mp4 decoder. 
 ** Author:			Xiaowei Luo
 **	Note:
 *****************************************************************************/
void *Mp4Enc_InterMemAlloc(MP4EncHandle* mp4Handle,uint32 mem_size)
{
	Mp4EncObject*vd = (Mp4EncObject *) mp4Handle->videoEncoderData;
	uint8 *pMem;
	mem_size = ((mem_size + 3) &(~3));

	if((0 == mem_size)||(mem_size> (vd->s_Mp4EncInterMemSize-vd->s_Mp4EncInterMemUsed)))
	{
		SCI_TRACE_LOW("Mp4Enc_InterMemAlloc failed");
		SCI_ASSERT(0);
		return 0;
	}
	
	pMem = vd->s_pEnc_Inter_buffer + vd->s_Mp4EncInterMemUsed;
	vd->s_Mp4EncInterMemUsed += mem_size;
	
	return pMem;
}

/*****************************************************************************
 **	Name : 			Mp4Enc_MemFree
 ** Description:	Free the common memory for mp4 encoder.  
 ** Author:			Xiaowei Luo
 **	Note:
 *****************************************************************************/
void Mp4Enc_MemFree(MP4EncHandle* mp4Handle) 
{ 	
	Mp4EncObject*vd = (Mp4EncObject *) mp4Handle->videoEncoderData;
	vd->s_Mp4EncExtraMemUsed = 0;
	vd->s_Mp4EncInterMemUsed = 0;
}

/*****************************************************************************
 **	Name : 			Mp4Enc_InitMem
 ** Description:	
 ** Author:			Xiaowei Luo
 **	Note:	
 *****************************************************************************/
void Mp4Enc_InitMem(MP4EncHandle* mp4Handle,MMCodecBuffer *pInterMemBfr, MMCodecBuffer *pExtaMemBfr)
{
	Mp4EncObject*vd = (Mp4EncObject *) mp4Handle->videoEncoderData;
	vd->s_pEnc_Inter_buffer = pInterMemBfr->common_buffer_ptr;
	vd->s_Mp4EncInterMemSize = pInterMemBfr->size;

	vd->s_pEnc_Extra_buffer = pExtaMemBfr->common_buffer_ptr;
	vd->s_Mp4EncExtraMemSize = pExtaMemBfr->size;
#ifdef _VSP_LINUX_
	vd->s_extra_mem_bfr_phy_ptr = pExtaMemBfr->common_buffer_ptr_phy;
#endif

	//reset memory used count
	vd->s_Mp4EncInterMemUsed = 0;
	vd->s_Mp4EncExtraMemUsed = 0;
}
/**---------------------------------------------------------------------------*
**                         Compiler Flag                                      *
**---------------------------------------------------------------------------*/
#ifdef   __cplusplus
    }
#endif
/**---------------------------------------------------------------------------*/
// End 
