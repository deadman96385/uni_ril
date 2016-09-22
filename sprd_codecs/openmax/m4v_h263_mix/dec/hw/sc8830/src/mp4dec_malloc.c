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
#include "mp4dec_video_header.h"
/**---------------------------------------------------------------------------*
**                        Compiler Flag                                       *
**---------------------------------------------------------------------------*/
#ifdef   __cplusplus
extern   "C"
{
#endif

#define MP4DEC_MALLOC_PRINT   //ALOGD

LOCAL MMDecRet Init_Mem (Mp4DecObject *vo, MMCodecBuffer *pMem, int32 type)
{
    if (type >= MAX_MEM_TYPE)
    {
        return MMENC_ERROR;
    } else
    {
        int32 dw_aligned = (((uint_32or64)(pMem->common_buffer_ptr) + 7) & (~7)) - ((uint_32or64)(pMem->common_buffer_ptr));

        vo->mem[type].used_size = 0;
        vo->mem[type].v_base = (uint_32or64)(pMem->common_buffer_ptr) + dw_aligned;
        vo->mem[type].p_base = (uint_32or64)(pMem->common_buffer_ptr_phy) + dw_aligned;
        vo->mem[type].total_size = pMem->size - dw_aligned;

        CHECK_MALLOC((void *)(vo->mem[type].v_base), "vo->mem[type].v_base");
        SCI_MEMSET((void *)(vo->mem[type].v_base), 0, vo->mem[type].total_size);

        MP4DEC_MALLOC_PRINT("%s: type:%d, dw_aligned, %d, v_base: 0x%lx, p_base: 0x%lx, mem_size:%d\n",
                            __FUNCTION__, type, dw_aligned, vo->mem[type].v_base, vo->mem[type].p_base, vo->mem[type].total_size);
    }

    return MMDEC_OK;
}

PUBLIC MMDecRet Mp4Dec_InitInterMem (Mp4DecObject *vo, MMCodecBuffer *pInterMemBfr)
{
    return Init_Mem(vo, pInterMemBfr, INTER_MEM);
}

/*****************************************************************************/
//  Description:   Init mpeg4 decoder	memory
//	Global resource dependence:
//  Author:
//	Note:
/*****************************************************************************/
PUBLIC MMDecRet MP4DecMemInit(MP4Handle *mp4Handle, MMCodecBuffer *pBuffer)
{
    Mp4DecObject *vo = (Mp4DecObject *) mp4Handle->videoDecoderData;
    int32 type = HW_NO_CACHABLE;

    return Init_Mem(vo, &(pBuffer[type]), type);
}

/*****************************************************************************
 ** Note:	Alloc the needed memory
 *****************************************************************************/
PUBLIC void *Mp4Dec_MemAlloc (Mp4DecObject *vo, uint32 need_size, int32 aligned_byte_num, int32 type)
{
    CODEC_BUF_T *pMem = &(vo->mem[type]);
    uint_32or64 CurrAddr, AlignedAddr;

    CurrAddr = pMem->v_base + pMem->used_size;
    AlignedAddr = (CurrAddr + aligned_byte_num-1) & (~(aligned_byte_num -1));
    need_size += (AlignedAddr - CurrAddr);

    MP4DEC_MALLOC_PRINT("%s: mem_size:%d, AlignedAddr: %lx, type: %d\n", __FUNCTION__, need_size, AlignedAddr, type);

    if((0 == need_size)||(need_size >  (pMem->total_size -pMem->used_size)))
    {
        SPRD_CODEC_LOGE ("%s  failed, total_size:%d, used_size: %d, need_size:%d, type: %d\n", __FUNCTION__, pMem->total_size, pMem->used_size,need_size, type);
        return NULL;
    }

    pMem->used_size += need_size;

    return (void *)AlignedAddr;
}

/*****************************************************************************
 ** Note:	 mapping from virtual to physical address
 *****************************************************************************/
PUBLIC uint_32or64 Mp4Dec_MemV2P(Mp4DecObject *vo, uint8 *vAddr, int32 type)
{
    if (type >= MAX_MEM_TYPE)
    {
        SPRD_CODEC_LOGE ("%s, memory type is error!", __FUNCTION__);
        return (uint_32or64)NULL;
    } else
    {
        CODEC_BUF_T *pMem = &(vo->mem[type]);

        return ((uint_32or64)(vAddr)-pMem->v_base+pMem->p_base);
    }
}

/**---------------------------------------------------------------------------*
**                         Compiler Flag                                      *
**---------------------------------------------------------------------------*/
#ifdef   __cplusplus
}
#endif
/**---------------------------------------------------------------------------*/
// End