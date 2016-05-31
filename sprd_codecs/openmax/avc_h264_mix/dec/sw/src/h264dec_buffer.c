/******************************************************************************
 ** File Name:    h264dec_buffer.c                                            *
 ** Author:       Xiaowei.Luo                                                 *
 ** DATE:         03/29/2010                                                  *
 ** Copyright:    2010 Spreatrum, Incoporated. All Rights Reserved.           *
 ** Description:                                                              *
 *****************************************************************************/
/******************************************************************************
 **                   Edit    History                                         *
 **---------------------------------------------------------------------------*
 ** DATE          NAME            DESCRIPTION                                 *
 ** 03/29/2010    Xiaowei Luo     Create.                                     *
 *****************************************************************************/
/*----------------------------------------------------------------------------*
**                        Dependencies                                        *
**---------------------------------------------------------------------------*/
#include "h264dec_video_header.h"
/**---------------------------------------------------------------------------*
**                        Compiler Flag                                       *
**---------------------------------------------------------------------------*/
#ifdef   __cplusplus
extern   "C"
{
#endif

PUBLIC DEC_FRAME_STORE_T *H264Dec_search_frame_from_dpb(H264DecContext *vo, DEC_STORABLE_PICTURE_T *frame)
{
    uint32 i;
    DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr=  vo->g_dpb_ptr;

    for (i = 0; i < (MAX_REF_FRAME_NUMBER+1); i++) {
        if (frame == dpb_ptr->fs[i]->frame) {
            return dpb_ptr->fs[i];
        }
    }

    return NULL;
}

PUBLIC void H264Dec_clear_delayed_buffer(H264DecContext *vo)
{
    DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr = vo->g_dpb_ptr;
    int32 i;

    if (dpb_ptr->delayed_pic_ptr) {
        H264DEC_UNBIND_FRAME(vo, dpb_ptr->delayed_pic_ptr);
        dpb_ptr->delayed_pic_ptr = NULL;
    }

    for (i = 0; dpb_ptr->delayed_pic[i]; i++) {
        int32 j;
        DEC_FRAME_STORE_T * fs = NULL;

        if ((fs = H264Dec_search_frame_from_dpb(vo, dpb_ptr->delayed_pic[i])) != NULL) {
            if(fs->is_reference == DELAYED_PIC_REF) {
                fs->is_reference = 0;
                H264DEC_UNBIND_FRAME(vo, fs->frame);
            }
        }

        dpb_ptr->delayed_pic[i] = NULL;
        dpb_ptr->delayed_pic_num--;
    }

    if( 0 != dpb_ptr->delayed_pic_num ) {
        SPRD_CODEC_LOGW ("%s: delayed_pic_num is %d\n", __FUNCTION__, dpb_ptr->delayed_pic_num);
    }
}

PUBLIC MMDecRet H264Dec_init_img_buffer (H264DecContext *vo)
{
    int32 mb_num_x = vo->frame_width_in_mbs;
    int32 total_mb_num = mb_num_x * vo->frame_height_in_mbs;
    void* reserved_mem_ptr;

    vo->g_halfPixTemp = (int16 *)H264Dec_MemAlloc(vo, 24*16*sizeof(int16), 16, SW_CACHABLE);
    CHECK_MALLOC(vo->g_halfPixTemp, "vo->g_halfPixTemp");

    reserved_mem_ptr = H264Dec_MemAlloc (vo, 2*mb_num_x*sizeof(DEC_MB_INFO_T), 4, SW_CACHABLE); //for posy = -1 mb.
    CHECK_MALLOC(reserved_mem_ptr, "reserved_mem_ptr");

    vo->mb_info = (DEC_MB_INFO_T *)H264Dec_MemAlloc (vo, (uint32)total_mb_num*sizeof(DEC_MB_INFO_T), 4, SW_CACHABLE);
    CHECK_MALLOC(vo->mb_info, "vo->mb_info");

    vo->i4x4pred_mode_ptr = (int8 *)H264Dec_MemAlloc (vo, total_mb_num*sizeof(int8)*16, 4, SW_CACHABLE);
    CHECK_MALLOC(vo->i4x4pred_mode_ptr, "vo->i4x4pred_mode_ptr");

    vo->direct_ptr = (int8 *)H264Dec_MemAlloc (vo, total_mb_num*sizeof(int8)*16, 4, SW_CACHABLE);
    CHECK_MALLOC(vo->direct_ptr, "vo->direct_ptr");

    vo->nnz_ptr= (int8 *)H264Dec_MemAlloc (vo, total_mb_num*sizeof(int8)*24, 4, SW_CACHABLE);	/// 16 y  + 4 u +4v
    CHECK_MALLOC(vo->nnz_ptr, "vo->nnz_ptr");

    vo->mvd_ptr[0] = (int16 *)H264Dec_MemAlloc (vo, total_mb_num*sizeof(int16)*16*2, 4, SW_CACHABLE);
    CHECK_MALLOC(vo->mvd_ptr[0], "vo->mvd_ptr[0]");

    vo->mvd_ptr[1] = (int16 *)H264Dec_MemAlloc (vo, total_mb_num*sizeof(int16)*16*2, 4, SW_CACHABLE);
    CHECK_MALLOC(vo->mvd_ptr[1] , "vo->mvd_ptr[1] ");

    vo->slice_nr_ptr = (int32 *)H264Dec_MemAlloc (vo, total_mb_num*sizeof(int32), 4, SW_CACHABLE);
    CHECK_MALLOC(vo->slice_nr_ptr, "vo->slice_nr_ptr");

    return MMDEC_OK;
}

PUBLIC MMDecRet H264Dec_init_dpb (H264DecContext *vo)
{
    int32 i;
    int32 ext_frm_size = vo->ext_width * vo->ext_height;
    int32 frm_size = vo->width * vo->height;
    int32 frm_size_in_blk = (frm_size >> 4);
    DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr = vo->g_dpb_ptr;

    dpb_ptr->used_size = 0;
    dpb_ptr->ref_frames_in_buffer = 0;
    dpb_ptr->ltref_frames_in_buffer = 0;

    for (i = 0; i < MAX_REF_FRAME_NUMBER+1; i++) {
        //each storable_picture buffer is bonding to a frame store
        dpb_ptr->fs[i]->frame->mv_ptr[0] = (int16 *)H264Dec_MemAlloc(vo, (frm_size_in_blk << 1) * sizeof(int16), 4, SW_CACHABLE);
        CHECK_MALLOC(dpb_ptr->fs[i]->frame->mv_ptr[0], "dpb_ptr->fs[i]->frame->mv_ptr[0]");

        dpb_ptr->fs[i]->frame->mv_ptr[1] = (int16 *)H264Dec_MemAlloc(vo, (frm_size_in_blk << 1) * sizeof(int16), 4, SW_CACHABLE);
        CHECK_MALLOC(dpb_ptr->fs[i]->frame->mv_ptr[1], "dpb_ptr->fs[i]->frame->mv_ptr[1]");

        dpb_ptr->fs[i]->frame->ref_idx_ptr[0] = (int8 *)H264Dec_MemAlloc(vo, frm_size_in_blk, 16, SW_CACHABLE);
        CHECK_MALLOC(dpb_ptr->fs[i]->frame->ref_idx_ptr[0], "dpb_ptr->fs[i]->frame->ref_idx_ptr[0]");

        dpb_ptr->fs[i]->frame->ref_idx_ptr[1] = (int8 *)H264Dec_MemAlloc(vo, frm_size_in_blk, 16, SW_CACHABLE);
        CHECK_MALLOC(dpb_ptr->fs[i]->frame->ref_idx_ptr[1], "dpb_ptr->fs[i]->frame->ref_idx_ptr[1]");

        dpb_ptr->fs[i]->frame->ref_pic_id_ptr[0] = (int32 *)H264Dec_MemAlloc(vo, frm_size_in_blk * sizeof(int32), 4, SW_CACHABLE);
        CHECK_MALLOC(dpb_ptr->fs[i]->frame->ref_pic_id_ptr[0], "dpb_ptr->fs[i]->frame->ref_pic_id_ptr[0]");

        dpb_ptr->fs[i]->frame->ref_pic_id_ptr[1] = (int32 *)H264Dec_MemAlloc(vo, frm_size_in_blk * sizeof(int32), 4, SW_CACHABLE);
        CHECK_MALLOC(dpb_ptr->fs[i]->frame->ref_pic_id_ptr[1], "dpb_ptr->fs[i]->frame->ref_pic_id_ptr[1]");

        if (vo->yuv_format == YUV420SP_NV12 || vo->yuv_format == YUV420SP_NV21) {
            dpb_ptr->fs[i]->frame->imgYUV[0] = (uint8 *)H264Dec_MemAlloc(vo, ext_frm_size, 256, SW_CACHABLE);
            CHECK_MALLOC(dpb_ptr->fs[i]->frame->imgYUV[0], "dpb_ptr->fs[i]->frame->imgYUV[0]");

            dpb_ptr->fs[i]->frame->imgYUV[1] = (uint8 *)H264Dec_MemAlloc(vo, ext_frm_size>>2, 256, SW_CACHABLE);
            CHECK_MALLOC(dpb_ptr->fs[i]->frame->imgYUV[1], "dpb_ptr->fs[i]->frame->imgYUV[1]");

            dpb_ptr->fs[i]->frame->imgYUV[2] = (uint8 *)H264Dec_MemAlloc(vo, ext_frm_size>>2, 256, SW_CACHABLE);
            CHECK_MALLOC(dpb_ptr->fs[i]->frame->imgYUV[2], "dpb_ptr->fs[i]->frame->imgYUV[2]");
        } else {//only for thumbnail
            dpb_ptr->fs[i]->frame->imgYUV[0] = PNULL;
            dpb_ptr->fs[i]->frame->imgYUV[1] = PNULL;
            dpb_ptr->fs[i]->frame->imgYUV[2] = PNULL;
        }

#ifdef _SIMUATION_  //ony for simulation
        dpb_ptr->fs[i]->frame->imgY = (uint8 *)H264Dec_MemAlloc(vo, frm_size, 256, SW_CACHABLE);
        CHECK_MALLOC(dpb_ptr->fs[i]->frame->imgY, "dpb_ptr->fs[i]->frame->imgY");

        dpb_ptr->fs[i]->frame->imgU = (uint8 *)H264Dec_MemAlloc(vo, frm_size/4, 256, SW_CACHABLE);
        CHECK_MALLOC(dpb_ptr->fs[i]->frame->imgU, "dpb_ptr->fs[i]->frame->imgU");

        dpb_ptr->fs[i]->frame->imgV = (uint8 *)H264Dec_MemAlloc(vo, frm_size/4, 256, SW_CACHABLE);
        CHECK_MALLOC(dpb_ptr->fs[i]->frame->imgV, "dpb_ptr->fs[i]->frame->imgV");
#else
        dpb_ptr->fs[i]->frame->imgY = PNULL;
        dpb_ptr->fs[i]->frame->imgU = PNULL;
        dpb_ptr->fs[i]->frame->pBufferHeader = PNULL;
#endif

        dpb_ptr->fs[i]->frame->dec_ref_pic_marking_buffer = NULL;
        dpb_ptr->fs_ref[i] = NULL;
        dpb_ptr->fs_ltref[i] = NULL;

        //init
        dpb_ptr->fs[i]->is_reference = 0;
        dpb_ptr->fs[i]->is_long_term = 0;
        dpb_ptr->fs[i]->is_short_term = 0;
        dpb_ptr->fs[i]->long_term_frame_idx = 0;
        dpb_ptr->fs[i]->disp_status = 0;
        dpb_ptr->fs[i]->frame_num = 0;
        dpb_ptr->fs[i]->poc = 0;
    }

    for(i = 0; i < MAX_DELAYED_PIC_NUM; i++) {
        dpb_ptr->delayed_pic[i] = NULL;
    }
    dpb_ptr->delayed_pic_ptr= NULL;
    dpb_ptr->delayed_pic_num = 0;

    dpb_ptr->size = MAX_REF_FRAME_NUMBER;

    return MMDEC_OK;
}

LOCAL void H264Dec_unmark_for_reference (H264DecContext *vo, DEC_FRAME_STORE_T *fs_ptr)
{
    DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr = vo->g_dpb_ptr;
    int32 i;

    fs_ptr->is_reference = 0;

    for (i = 0; dpb_ptr->delayed_pic[i]; i++) {
        if (fs_ptr->frame == dpb_ptr->delayed_pic[i]) {
            fs_ptr->is_reference = DELAYED_PIC_REF;
        }
    }

    if(!fs_ptr->is_reference) {
        H264DEC_UNBIND_FRAME(vo, fs_ptr->frame);
    }

    fs_ptr->is_long_term = 0;
    fs_ptr->is_short_term = 0;

    return;
}

LOCAL void H264Dec_get_smallest_poc (H264DecContext *vo, int32 *poc, int32 * pos, DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr)
{
    int32 i;

    if (dpb_ptr->used_size<1) {
        SPRD_CODEC_LOGE ("Cannot determine smallest POC, DPB empty.\n");
        vo->error_flag |= ER_REF_FRM_ID;
        return;
    }

    *pos=-1;
    *poc = SINT_MAX;
    for (i=0; i<dpb_ptr->used_size; i++) {
        if (*poc > dpb_ptr->fs[i]->poc) {
            *poc = dpb_ptr->fs[i]->poc;
            *pos=i;
        }
    }

    return;
}

LOCAL void H264Dec_output_one_frame_from_dpb (H264DecContext *vo, DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr)
{
    int32 poc, pos;
    DEC_FRAME_STORE_T **fs = dpb_ptr->fs;
    DEC_STORABLE_PICTURE_T *frame;

    H264Dec_get_smallest_poc(vo, &poc, &pos, dpb_ptr);
    if (pos < 0) {
        vo->error_flag |= ER_REF_FRM_ID;
        return;
    }

    frame = fs[pos]->frame;

    if (!fs[pos]->is_reference) {
        h264Dec_remove_frame_from_dpb(vo, dpb_ptr, pos);
    }
}

PUBLIC void H264Dec_flush_dpb (H264DecContext *vo, DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr)
{
    int32 i;
    int32 disp_num = 0;
    DEC_FRAME_STORE_T *tmp_fs_ptr;

    //mark all frame unused
    for (i = 0; i < dpb_ptr->used_size; i++) {
        H264Dec_unmark_for_reference (vo, dpb_ptr->fs[i]);

        if (dpb_ptr->fs[i]->is_reference != DELAYED_PIC_REF) {
            tmp_fs_ptr = dpb_ptr->fs[i];
            dpb_ptr->fs[i] = dpb_ptr->fs[disp_num];
            dpb_ptr->fs[disp_num] = tmp_fs_ptr;
            disp_num++;
        }
    }

    dpb_ptr->used_size = disp_num;

    while(dpb_ptr->used_size) {
        H264Dec_output_one_frame_from_dpb(vo, dpb_ptr);
    }

    return;
}

LOCAL void H264Dec_idr_memory_management (H264DecContext *vo, DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr, DEC_STORABLE_PICTURE_T *picture_ptr)
{
    if (picture_ptr->no_output_of_prior_pics_flag) {
        //nothing
    } else {
        H264Dec_flush_dpb (vo, dpb_ptr);
    }

    if (vo->long_term_reference_flag) {
        picture_ptr->is_long_term = 1;
        picture_ptr->long_term_frame_idx = 0;
        picture_ptr->long_term_pic_num = 0;
    }

    return;
}

LOCAL __inline int32 H264Dec_get_pic_num_x (DEC_STORABLE_PICTURE_T *picture_ptr, int32 difference_of_pic_nums_minus1)
{
    int32 curr_pic_num = picture_ptr->frame_num;

    return (curr_pic_num - (difference_of_pic_nums_minus1 + 1));
}

LOCAL void H264Dec_mm_unmark_short_term_for_reference (H264DecContext *vo, DEC_STORABLE_PICTURE_T *picture_ptr, int32 difference_of_pic_nums_minus1, DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr)
{
    int32 pic_num_x;
    int32 i;

    pic_num_x = H264Dec_get_pic_num_x (picture_ptr, difference_of_pic_nums_minus1);

    for (i = 0; i < dpb_ptr->ref_frames_in_buffer; i++) {
        if ((dpb_ptr->fs_ref[i]->is_reference) && (dpb_ptr->fs_ref[i]->is_long_term == 0)) {
            if (dpb_ptr->fs_ref[i]->frame->pic_num == pic_num_x) {
                H264Dec_unmark_for_reference(vo, dpb_ptr->fs_ref[i]);

                return;
            }
        }
    }

    return;
}

LOCAL void H264Dec_mm_unmark_long_term_for_reference (H264DecContext *vo, int32 long_term_pic_num, DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr)
{
    int32 i;

    for (i = 0; i < dpb_ptr->ltref_frames_in_buffer; i++) {
        if ((dpb_ptr->fs_ltref[i]->is_reference) && (dpb_ptr->fs_ltref[i]->is_long_term)) {
            if (dpb_ptr->fs_ltref[i]->frame->long_term_pic_num == long_term_pic_num) {
                H264Dec_unmark_for_reference (vo, dpb_ptr->fs_ltref[i]);
                return;
            }
        }
    }

    return;
}

LOCAL void H264Dec_unmark_long_term_for_reference_by_frame_idx (H264DecContext *vo, int32 long_term_frame_idx, DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr)
{
    int32 i;

    for (i = 0; i < dpb_ptr->ltref_frames_in_buffer; i++) {
        if (dpb_ptr->fs_ltref[i]->long_term_frame_idx == long_term_frame_idx) {
            H264Dec_unmark_for_reference (vo, dpb_ptr->fs_ltref[i]);
        }
    }

    return;
}

LOCAL void H264Dec_mark_pic_long_term (int32 long_term_frame_idx, int32 pic_num_x, DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr)
{
    int32 i;

    for (i = 0; i < dpb_ptr->ref_frames_in_buffer; i++) {
        if (dpb_ptr->fs_ref[i]->is_reference) {
            if ((!dpb_ptr->fs_ref[i]->frame->is_long_term) && (dpb_ptr->fs_ref[i]->frame->pic_num == pic_num_x)) {
                dpb_ptr->fs_ref[i]->long_term_frame_idx = long_term_frame_idx;
                dpb_ptr->fs_ref[i]->is_long_term = 1;
                dpb_ptr->fs_ref[i]->is_short_term = 0;
                dpb_ptr->fs_ref[i]->frame->long_term_frame_idx = long_term_frame_idx;
                dpb_ptr->fs_ref[i]->frame->long_term_pic_num = long_term_frame_idx;
                dpb_ptr->fs_ref[i]->frame->is_long_term = 1;

                return;
            }
        }
    }

    return;
}

LOCAL void H264Dec_mm_assign_long_term_frame_idx (H264DecContext *vo, DEC_STORABLE_PICTURE_T *picture_ptr, int32 difference_of_pic_nums_minus1, int32 long_term_frame_idx, DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr)
{
    int32 pic_num_x;

    pic_num_x = H264Dec_get_pic_num_x (picture_ptr, difference_of_pic_nums_minus1);

    H264Dec_unmark_long_term_for_reference_by_frame_idx (vo, long_term_frame_idx, dpb_ptr);
    H264Dec_mark_pic_long_term (long_term_frame_idx, pic_num_x, dpb_ptr);
}

LOCAL void H264Dec_update_ref_list (DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr)
{
    int32 i,j;
    int32 max_pos;
    DEC_FRAME_STORE_T *tmp_frame_ptr;

    for (i = 0, j = 0; i < dpb_ptr->used_size; i++) {
        if (dpb_ptr->fs[i]->is_reference && dpb_ptr->fs[i]->is_short_term) {
            dpb_ptr->fs_ref[j++] = dpb_ptr->fs[i];
        }
    }

    dpb_ptr->ref_frames_in_buffer = j;

    //sort
    for (j = 0; j < dpb_ptr->ref_frames_in_buffer; j++) {
        max_pos = j;

        for (i = j+1; i < dpb_ptr->ref_frames_in_buffer; i++) {
            if (dpb_ptr->fs_ref[i]->pic_num > dpb_ptr->fs_ref[max_pos]->pic_num) {
                max_pos = i;
            }
        }

        //exchange
        tmp_frame_ptr = dpb_ptr->fs_ref[j];
        dpb_ptr->fs_ref[j] = dpb_ptr->fs_ref[max_pos];
        dpb_ptr->fs_ref[max_pos] = tmp_frame_ptr;
    }

    return;
}

LOCAL void H264Dec_update_ltref_list (DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr)
{
    int32 i,j;
    int32 min_pos;
    DEC_FRAME_STORE_T *tmp_frame_ptr;

    for (i = 0, j = 0; i < dpb_ptr->used_size; i++) {
        if (dpb_ptr->fs[i]->is_reference && dpb_ptr->fs[i]->is_long_term) {
            dpb_ptr->fs_ltref[j++] = dpb_ptr->fs[i];
        }
    }

    dpb_ptr->ltref_frames_in_buffer = j;

    //sort
    for (j = 0; j < dpb_ptr->ltref_frames_in_buffer; j++) {
        min_pos = j;

        for (i = j+1; i < dpb_ptr->ltref_frames_in_buffer; i++) {
            if (dpb_ptr->fs_ltref[i]->long_term_frame_idx < dpb_ptr->fs_ltref[min_pos]->long_term_frame_idx) {
                min_pos = i;
            }
        }

        //exchange
        tmp_frame_ptr = dpb_ptr->fs_ltref[j];
        dpb_ptr->fs_ltref[j] = dpb_ptr->fs_ltref[min_pos];
        dpb_ptr->fs_ltref[min_pos] = tmp_frame_ptr;
    }

    return;
}

//set new max long_term_frame_idx
LOCAL void H264Dec_mm_update_max_long_term_frame_idx (H264DecContext *vo, int32 max_long_term_frame_idx_plus1, DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr)
{
    int32 i;

    dpb_ptr->max_long_term_pic_idx = max_long_term_frame_idx_plus1 -1;

    //check for invalid frames
    for (i = 0; i < dpb_ptr->ltref_frames_in_buffer; i++) {
        if (dpb_ptr->fs_ltref[i]->long_term_frame_idx > dpb_ptr->max_long_term_pic_idx) {
            H264Dec_unmark_for_reference (vo, dpb_ptr->fs_ltref[i]);
        }
    }

    return;
}

//mark all short term reference pictures unused for reference
LOCAL void H264Dec_mm_unmark_all_short_term_for_reference (H264DecContext *vo, DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr)
{
    int32 i;

    for (i = 0; i < dpb_ptr->ref_frames_in_buffer; i++) {
        H264Dec_unmark_for_reference(vo, dpb_ptr->fs_ref[i]);
    }

    H264Dec_update_ref_list (dpb_ptr);

    return;
}

LOCAL void H264Dec_mm_unmark_all_long_term_for_reference (H264DecContext *vo, DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr)
{
    H264Dec_mm_update_max_long_term_frame_idx(vo, 0, dpb_ptr);
}

//mark the current picture used for long term reference
LOCAL void H264Dec_mm_mark_current_picture_long_term (H264DecContext *vo, DEC_STORABLE_PICTURE_T *picture_ptr, int32 long_term_frame_idx, DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr)
{
    //remove long term pictures with same long_term_frame_idx
    H264Dec_unmark_long_term_for_reference_by_frame_idx (vo, long_term_frame_idx, dpb_ptr);

    picture_ptr->is_long_term = 1;
    picture_ptr->long_term_frame_idx = long_term_frame_idx;
    picture_ptr->long_term_pic_num = long_term_frame_idx;

    return;
}

LOCAL void H264Dec_adaptive_memory_management (H264DecContext *vo, DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr, DEC_STORABLE_PICTURE_T *picture_ptr)
{
    DEC_DEC_REF_PIC_MARKING_T *tmp_drpm_ptr;
    int32 i = 0;

    vo->last_has_mmco_5 = 0;

    while (i < vo->g_dec_ref_pic_marking_buffer_size) {
        tmp_drpm_ptr = &(picture_ptr->dec_ref_pic_marking_buffer[i]);

        switch (tmp_drpm_ptr->memory_management_control_operation) {
        case 0:
            if (i != vo->g_dec_ref_pic_marking_buffer_size-1) {
                vo->error_flag |= ER_REF_FRM_ID;
                SPRD_CODEC_LOGE("memory_management_control_operation = 0 not last operation buffer\n");
                return;
            }
            break;
        case 1:
            H264Dec_mm_unmark_short_term_for_reference (vo, picture_ptr, tmp_drpm_ptr->difference_of_pic_nums_minus1, dpb_ptr);
            H264Dec_update_ref_list(dpb_ptr);
            break;
        case 2:
            H264Dec_mm_unmark_long_term_for_reference (vo, tmp_drpm_ptr->long_term_pic_num, dpb_ptr);
            H264Dec_update_ltref_list (dpb_ptr);
            break;
        case 3:
            H264Dec_mm_assign_long_term_frame_idx (vo, picture_ptr, tmp_drpm_ptr->difference_of_pic_nums_minus1, tmp_drpm_ptr->long_term_frame_idx, dpb_ptr);
            H264Dec_update_ref_list (dpb_ptr);
            H264Dec_update_ltref_list(dpb_ptr);
            break;
        case 4:
            H264Dec_mm_update_max_long_term_frame_idx (vo, tmp_drpm_ptr->max_long_term_frame_idx_plus1, dpb_ptr);
            H264Dec_update_ltref_list (dpb_ptr);
            break;
        case 5:
            H264Dec_mm_unmark_all_short_term_for_reference (vo, dpb_ptr);
            H264Dec_mm_unmark_all_long_term_for_reference (vo, dpb_ptr);
            vo->last_has_mmco_5 = 1;
            break;
        case 6:
            H264Dec_mm_mark_current_picture_long_term (vo, picture_ptr, tmp_drpm_ptr->long_term_frame_idx, dpb_ptr);
            if((int32)(dpb_ptr->ltref_frames_in_buffer +dpb_ptr->ref_frames_in_buffer)>(mmax(1, dpb_ptr->num_ref_frames))) {
                vo->error_flag |= ER_REF_FRM_ID;
                SPRD_CODEC_LOGE ("max.number of reference frame exceed. invalid stream.\n");
                return;
            }
            break;
        default:
            vo->error_flag |= ER_REF_FRM_ID;
            SPRD_CODEC_LOGE ("invalid memory_management_control_operation in buffer\n");
            return;
        }
        i++;
    }

    if(vo->last_has_mmco_5) {
        picture_ptr->pic_num = picture_ptr->frame_num = 0;
        vo->toppoc -= picture_ptr->poc;
        vo->bottompoc -= picture_ptr->poc;
        picture_ptr->poc -= picture_ptr->poc;
        vo->framepoc = picture_ptr->poc;
        vo->ThisPOC = picture_ptr->poc;
        H264Dec_flush_dpb(vo, dpb_ptr);
    }

    return;
}

//mark the oldest short term reference to unref
LOCAL void H264Dec_sliding_window_memory_management (H264DecContext *vo, DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr, DEC_STORABLE_PICTURE_T *picture_ptr)
{
    int32 i;

    if (dpb_ptr->ref_frames_in_buffer >= (dpb_ptr->num_ref_frames - dpb_ptr->ltref_frames_in_buffer)) {
        for (i = 0; i < dpb_ptr->used_size; i++) {
            if ((dpb_ptr->fs[i]->is_reference) && (dpb_ptr->fs[i]->is_short_term)) {
                H264Dec_unmark_for_reference (vo, dpb_ptr->fs[i]);
                break;
            }
        }
    }

    picture_ptr->is_long_term = 0;

    return;
}

//if current frame is used as reference frame, put it to first unused store frame in dpb
LOCAL void H264Dec_insert_picture_in_dpb (H264DecContext *vo, DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr, DEC_FRAME_STORE_T *curr_fs_ptr, DEC_STORABLE_PICTURE_T *picture_ptr)
{
    int32 used_size = dpb_ptr->used_size;
    DEC_FRAME_STORE_T *tmp_fs_ptr;

    if (picture_ptr->used_for_reference) {
        curr_fs_ptr->is_reference = 1;
        H264DEC_BIND_FRAME(vo, curr_fs_ptr->frame);

        if (picture_ptr->is_long_term) {
            curr_fs_ptr->is_long_term = 1;
            curr_fs_ptr->is_short_term = 0;
            curr_fs_ptr->long_term_frame_idx = picture_ptr->long_term_frame_idx;
        } else {
            curr_fs_ptr->is_short_term = 1;
            curr_fs_ptr->is_long_term = 0;
        }
    } else {
        curr_fs_ptr->is_short_term = 0;
        curr_fs_ptr->is_long_term = 0;
    }

    curr_fs_ptr->frame = picture_ptr;
    curr_fs_ptr->frame_num = picture_ptr->frame_num;
    curr_fs_ptr->poc = picture_ptr->poc;

    //put the current frame store to the first unused frame store of dpb
    tmp_fs_ptr = dpb_ptr->fs[used_size];
    dpb_ptr->fs[used_size] = curr_fs_ptr;
    curr_fs_ptr = tmp_fs_ptr;

    dpb_ptr->fs[MAX_REF_FRAME_NUMBER] = tmp_fs_ptr;
    dpb_ptr->used_size++;

    return;
}

LOCAL void dump_dpb(H264DecContext *vo)
{
    DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr = vo->g_dpb_ptr;
#if 0//DUMP_DPB
    unsigned i;
    PRINTF ("\n");
    for (i=0; i < (dpb_ptr->used_size); i++) {
        PRINTF("(");
        PRINTF("fn=%d  ", dpb_ptr->fs[i]->frame_num);
        PRINTF("F: poc=%d  ", dpb_ptr->fs[i]->frame->poc);
        PRINTF("G: poc=%d)  ", dpb_ptr->fs[i]->poc);

        if (dpb_ptr->fs[i]->is_reference) printf ("ref (%d) ", dpb_ptr->fs[i]->is_reference);

        if (dpb_ptr->fs[i]->is_long_term) printf ("lt_ref (%d) ", dpb_ptr->fs[i]->is_reference);

        if (dpb_ptr->fs[i]->disp_status) printf ("out ");

        if (dpb_ptr->fs[i]->frame->non_existing) printf ("non_existing  ");

        PRINTF ("\n");
    }
#endif
}

PUBLIC void H264Dec_store_picture_in_dpb (H264DecContext *vo, DEC_STORABLE_PICTURE_T *picture_ptr, DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr)
{
    vo->last_has_mmco_5 = 0;

    if (picture_ptr->idr_flag) {
        H264Dec_idr_memory_management (vo, dpb_ptr, picture_ptr);
    } else {
        if (picture_ptr->used_for_reference && (picture_ptr->adaptive_ref_pic_buffering_flag)) {
            H264Dec_adaptive_memory_management (vo, dpb_ptr, picture_ptr);
        }
    }

    if (vo->error_flag) {
        return;
    }

    if ((!picture_ptr->idr_flag) && (picture_ptr->used_for_reference && (!picture_ptr->adaptive_ref_pic_buffering_flag))) {
        H264Dec_sliding_window_memory_management (vo, dpb_ptr, picture_ptr);
    }
    SCI_TRACE_LOW_DPB("%s, %d, %d used vs total %d", __FUNCTION__, __LINE__,dpb_ptr->used_size ,  dpb_ptr->size);

    if (dpb_ptr->used_size >= dpb_ptr->size) {
        // first try to remove unused frames
        if(!H264Dec_remove_unused_frame_from_dpb(vo, dpb_ptr)) {
            H264Dec_remove_delayed_frame_from_dpb(vo, dpb_ptr);
        }
    }
    SCI_TRACE_LOW_DPB("%s, %d, %d used vs total %d", __FUNCTION__, __LINE__,dpb_ptr->used_size ,  dpb_ptr->size);

    H264Dec_insert_picture_in_dpb (vo, dpb_ptr, dpb_ptr->fs[MAX_REF_FRAME_NUMBER], picture_ptr);

    H264Dec_update_ref_list (dpb_ptr);
    H264Dec_update_ltref_list (dpb_ptr);

    if (vo->last_has_mmco_5) {
        vo->pre_frame_num = 0;
    }

//	dump_dpb(img_ptr);

    return;
}

LOCAL DEC_STORABLE_PICTURE_T *H264Dec_get_short_term_pic (H264DecContext *vo, int32 pic_num, DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr)
{
    int32 i;

    for (i = 0; i < (dpb_ptr->ref_frames_in_buffer); i++) {
#if _H264_PROTECT_ & _LEVEL_HIGH_
        if (dpb_ptr->fs_ref[i] == NULL) {
            SPRD_CODEC_LOGE("%s, %d, ER_REF_FRM_ID\n", __FUNCTION__, __LINE__);
            vo->error_flag |= ER_GET_SHORT_REF_ID;
            vo->return_pos |= (1<<3);
            return NULL;
        }
#endif
        if (dpb_ptr->fs_ref[i]->is_reference) {
#if _H264_PROTECT_ & _LEVEL_HIGH_
            if (dpb_ptr->fs_ref[i]->frame == NULL) {
                SPRD_CODEC_LOGE("%s, %d, ER_REF_FRM_ID\n", __FUNCTION__, __LINE__);
                vo->error_flag |= ER_GET_SHORT_REF_ID;
                vo->return_pos |= (1<<4);
                return NULL;
            }
#endif
            if ((!dpb_ptr->fs_ref[i]->frame->is_long_term) && (dpb_ptr->fs_ref[i]->frame->pic_num == pic_num)) {
                return dpb_ptr->fs_ref[i]->frame;
            }
        }
    }

    return vo->g_no_reference_picture_ptr;
}

LOCAL void H264Dec_reorder_short_term (H264DecContext *vo, DEC_STORABLE_PICTURE_T **ref_picture_listX_ptr, int32 num_ref_idx_lX_active_minus1, int32 pic_num_lx, int32 *ref_idx_lx)
{
    int32 c_idx, n_idx;
    DEC_STORABLE_PICTURE_T *pic_lx_ptr;

    pic_lx_ptr = H264Dec_get_short_term_pic (vo, pic_num_lx, vo->g_dpb_ptr);

    for (c_idx = (num_ref_idx_lX_active_minus1+1); c_idx > *ref_idx_lx; c_idx--) {
        ref_picture_listX_ptr[c_idx] = ref_picture_listX_ptr[c_idx-1];
    }

    ref_picture_listX_ptr[(*ref_idx_lx)++] = pic_lx_ptr;

    n_idx = *ref_idx_lx;

    for (c_idx = (*ref_idx_lx); c_idx <= (num_ref_idx_lX_active_minus1+1); c_idx++) {
        if ((ref_picture_listX_ptr[c_idx]->is_long_term) || (ref_picture_listX_ptr[c_idx]->pic_num != pic_num_lx)) {
            ref_picture_listX_ptr[n_idx++] = ref_picture_listX_ptr[c_idx];
        }
    }

    return;
}

LOCAL DEC_STORABLE_PICTURE_T *H264Dec_get_long_term_pic (H264DecContext *vo, int32 long_term_pic_num)
{
    int32 i;
    DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr = vo->g_dpb_ptr;

    for (i = 0; i < (dpb_ptr->ltref_frames_in_buffer); i++) {
        if (dpb_ptr->fs_ltref[i]->is_reference) {
            if ((dpb_ptr->fs_ltref[i]->frame->is_long_term) && (dpb_ptr->fs_ltref[i]->frame->long_term_pic_num == long_term_pic_num)) {
                return dpb_ptr->fs_ltref[i]->frame;
            }
        }
    }

    return vo->g_no_reference_picture_ptr;//NULL;
}

LOCAL void H264Dec_reorder_long_term (H264DecContext *vo,
                                      DEC_STORABLE_PICTURE_T **ref_picture_listX_ptr,
                                      int32 num_ref_idx_lX_active_minus1,
                                      int32 long_term_pic_num,
                                      int32 *ref_idx_lx)
{
    int32 c_idx, n_idx;
    DEC_STORABLE_PICTURE_T *pic_lx_ptr;

    pic_lx_ptr = H264Dec_get_long_term_pic (vo, long_term_pic_num);

    for (c_idx = (num_ref_idx_lX_active_minus1+1); c_idx > *ref_idx_lx; c_idx--) {
        ref_picture_listX_ptr[c_idx] = ref_picture_listX_ptr[c_idx-1];
    }

    ref_picture_listX_ptr[(*ref_idx_lx)++] = pic_lx_ptr;

    n_idx = *ref_idx_lx;

    for (c_idx = (*ref_idx_lx); c_idx <= (num_ref_idx_lX_active_minus1+1); c_idx++) {
        if ((!ref_picture_listX_ptr[c_idx]->is_long_term) || (ref_picture_listX_ptr[c_idx]->long_term_pic_num != long_term_pic_num)) {
            ref_picture_listX_ptr[n_idx++] = ref_picture_listX_ptr[c_idx];
        }
    }

    return;
}

LOCAL void H264Dec_reorder_ref_pic_list (H264DecContext *vo,
        DEC_STORABLE_PICTURE_T **picture_list_ptr,
        int32 num_ref_idx_lX_active_minus1,
        int32 *remapping_of_pic_nums_idc,
        int32 *abs_diff_pic_num_minus1,
        int32 *long_term_pic_idx)
{
    int32 i;
    int32 max_pic_num, curr_pic_num, pic_num_lx_no_wrap, pic_num_lx_pred, pic_num_lx;
    int32 ref_idx_lx = 0;

    max_pic_num = vo->max_frame_num;
    curr_pic_num = vo->frame_num;

    pic_num_lx_pred = curr_pic_num;

    for (i = 0; remapping_of_pic_nums_idc[i] != 3; i++) {
#if _H264_PROTECT_ & _LEVEL_HIGH_
        if (remapping_of_pic_nums_idc[i]>3) {
            PRINTF ("Invalid remapping_of_pic_nums_idc command");
            vo->error_flag |= ER_REORD_REF_PIC_ID;
            vo->return_pos |= (1<<6);
            return;
        }
#endif

        if (remapping_of_pic_nums_idc[i]<2) {
            if (remapping_of_pic_nums_idc[i] == 0) {
                if ((pic_num_lx_pred-(abs_diff_pic_num_minus1[i]+1))<0) {
                    pic_num_lx_no_wrap = pic_num_lx_pred - (abs_diff_pic_num_minus1[i]+1) + max_pic_num;
                } else {
                    pic_num_lx_no_wrap = pic_num_lx_pred - (abs_diff_pic_num_minus1[i]+1);
                }
            } else {//(remapping_of_pic_nums_idc[i]==1)
                if ((pic_num_lx_pred + (abs_diff_pic_num_minus1[i]+1)) >= max_pic_num) {
                    pic_num_lx_no_wrap = pic_num_lx_pred + (abs_diff_pic_num_minus1[i]+1) - max_pic_num;
                } else {
                    pic_num_lx_no_wrap = pic_num_lx_pred + (abs_diff_pic_num_minus1[i]+1);
                }
            }

            pic_num_lx_pred = pic_num_lx_no_wrap;

            if (pic_num_lx_no_wrap > curr_pic_num) {
                pic_num_lx = pic_num_lx_no_wrap - max_pic_num;
            } else {
                pic_num_lx = pic_num_lx_no_wrap;
            }

            H264Dec_reorder_short_term (vo, picture_list_ptr, num_ref_idx_lX_active_minus1, pic_num_lx, &ref_idx_lx);
        } else {//(remapping_of_pic_nums_idc[i]==2)
#if _H264_PROTECT_ & _LEVEL_HIGH_
            if (vo->g_dpb_ptr->fs_ltref == PNULL) {
                PRINTF("Invalid long term reference frame");
                vo->error_flag |= ER_REORD_REF_PIC_ID;
                vo->return_pos |= (1<<7);
                return;
            }
#endif
            H264Dec_reorder_long_term(vo, picture_list_ptr, num_ref_idx_lX_active_minus1, long_term_pic_idx[i], &ref_idx_lx);
        }
    }

    return;
}

PUBLIC void H264Dec_reorder_list (H264DecContext *vo, DEC_SLICE_T *currSlice)
{
    int32 currSliceType = vo->type;

    if (currSliceType != I_SLICE) {
        if (currSlice->ref_pic_list_reordering_flag_l0) {
            H264Dec_reorder_ref_pic_list (vo, vo->g_list[0], vo->ref_count[0]-1,
                                          currSlice->remapping_of_pic_nums_idc_l0,
                                          currSlice->abs_diff_pic_num_minus1_l0,
                                          currSlice->long_term_pic_idx_l0);
            vo->g_list_size[0] = vo->ref_count[0];
        }

        if (currSliceType == B_SLICE) {
            if (currSlice->ref_pic_list_reordering_flag_l1) {
                H264Dec_reorder_ref_pic_list (vo, vo->g_list[1], vo->ref_count[1]-1,
                                              currSlice->remapping_of_pic_nums_idc_l1,
                                              currSlice->abs_diff_pic_num_minus1_l1,
                                              currSlice->long_term_pic_idx_l1);
                vo->g_list_size[1] = vo->ref_count[1];
            }
        }
    }

    return;
}
#define SYS_QSORT	1

#if SYS_QSORT
/*!
 ************************************************************************
 * \brief
 *    compares two stored pictures by picture number for qsort in descending order
 *
 ************************************************************************
 */
static int compare_pic_by_pic_num_desc( const void *arg1, const void *arg2 )
{
    if ( (*(DEC_STORABLE_PICTURE_T**)arg1)->pic_num < (*(DEC_STORABLE_PICTURE_T**)arg2)->pic_num) {
        return 1;
    }

    if ( (*(DEC_STORABLE_PICTURE_T**)arg1)->pic_num > (*(DEC_STORABLE_PICTURE_T**)arg2)->pic_num) {
        return -1;
    } else {
        return 0;
    }
}

/*!
 ************************************************************************
 * \brief
 *    compares two stored pictures by picture number for qsort in descending order
 *
 ************************************************************************
 */
static int compare_pic_by_lt_pic_num_asc( const void *arg1, const void *arg2 )
{
    if ( (*(DEC_STORABLE_PICTURE_T**)arg1)->long_term_pic_num < (*(DEC_STORABLE_PICTURE_T**)arg2)->long_term_pic_num) {
        return -1;
    }

    if ( (*(DEC_STORABLE_PICTURE_T**)arg1)->long_term_pic_num > (*(DEC_STORABLE_PICTURE_T**)arg2)->long_term_pic_num) {
        return 1;
    } else {
        return 0;
    }
}

/*!
 ************************************************************************
 * \brief
 *    compares two stored pictures by poc for qsort in ascending order
 *
 ************************************************************************
 */
static int compare_pic_by_poc_asc( const void *arg1, const void *arg2 )
{
    if ( (*(DEC_STORABLE_PICTURE_T**)arg1)->poc < (*(DEC_STORABLE_PICTURE_T**)arg2)->poc) {
        return -1;
    }

    if ( (*(DEC_STORABLE_PICTURE_T**)arg1)->poc > (*(DEC_STORABLE_PICTURE_T**)arg2)->poc) {
        return 1;
    } else {
        return 0;
    }
}

/*!
 ************************************************************************
 * \brief
 *    compares two stored pictures by poc for qsort in descending order
 *
 ************************************************************************
 */
static int compare_pic_by_poc_desc( const void *arg1, const void *arg2 )
{
    if ( (*(DEC_STORABLE_PICTURE_T**)arg1)->poc < (*(DEC_STORABLE_PICTURE_T**)arg2)->poc) {
        return 1;
    }

    if ( (*(DEC_STORABLE_PICTURE_T**)arg1)->poc > (*(DEC_STORABLE_PICTURE_T**)arg2)->poc) {
        return -1;
    } else {
        return 0;
    }
}
#else

/*!
 ************************************************************************
 * \brief
 *    compares two stored pictures by picture number for qsort in descending order
 *
 ************************************************************************
 */
static int compare_pic_by_pic_num_desc(DEC_STORABLE_PICTURE_T *list_ptr[], int16 len)
{
    int16 i, j;

    for (i = 0; i < len; i++) {
        for(j = i+1; j < len; j++) {
            int32 t1, t2;
            t1 = list_ptr[i]->pic_num;
            t2 = list_ptr[j]->pic_num;
            if (t1 < t2) {
                DEC_STORABLE_PICTURE_T *tmp_s;

                tmp_s = list_ptr[i];
                list_ptr[i] = list_ptr[j];
                list_ptr[j] = tmp_s;
            }
        }
    }
    return 0;

}

/*!
 ************************************************************************
 * \brief
 *    compares two stored pictures by picture number for qsort in descending order
 *
 ************************************************************************
 */
static int compare_pic_by_lt_pic_num_asc(DEC_STORABLE_PICTURE_T *list_ptr[], int16 len)
{
    int16 i, j;

    for (i = 0; i < len; i++) {
        for(j = i+1; j < len; j++) {
            int32 t1, t2;

            t1 = list_ptr[i]->long_term_pic_num;
            t2 = list_ptr[j]->long_term_pic_num;
            if (t1 > t2) {
                DEC_STORABLE_PICTURE_T *tmp_s;

                tmp_s = list_ptr[i];
                list_ptr[i] = list_ptr[j];
                list_ptr[j] = tmp_s;
            }
        }
    }
    return 0;

}

/*!
 ************************************************************************
 * \brief
 *    compares two stored pictures by poc for qsort in ascending order
 *
 ************************************************************************
 */
static int compare_pic_by_poc_asc(DEC_STORABLE_PICTURE_T *list_ptr[], int16 len)
{
    int16 i, j;

    for (i = 0; i < len; i++) {
        for(j = i+1; j < len; j++) {
            int32 t1, t2;

            t1 = list_ptr[i]->poc;
            t2 = list_ptr[j]->poc;
            if (t1 > t2) {
                DEC_STORABLE_PICTURE_T *tmp_s;

                tmp_s = list_ptr[i];
                list_ptr[i] = list_ptr[j];
                list_ptr[j] = tmp_s;
            }
        }
    }
    return 0;

}


/*!
 ************************************************************************
 * \brief
 *    compares two stored pictures by poc for qsort in descending order
 *
 ************************************************************************
 */
static int compare_pic_by_poc_desc(DEC_STORABLE_PICTURE_T *list_ptr[], int16 len)
{
    int16 i, j;

    for (i = 0; i < len; i++) {
        for(j = i+1; j < len; j++) {
            int32 t1, t2;

            t1 = list_ptr[i]->poc;
            t2 = list_ptr[j]->poc;
            if (t1 < t2) {
                DEC_STORABLE_PICTURE_T *tmp_s;

                tmp_s = list_ptr[i];
                list_ptr[i] = list_ptr[j];
                list_ptr[j] = tmp_s;
            }
        }
    }

    return 0;
}
#endif

PUBLIC void H264Dec_init_list (H264DecContext *vo, int32 curr_slice_type)
{
    int32 i;
    int32 list0idx = 0;
    int32 list0idx_1 = 0;
    int32 listltidx = 0;
    DEC_DECODED_PICTURE_BUFFER_T *dpb_ptr = vo->g_dpb_ptr;
    DEC_STORABLE_PICTURE_T **list = vo->g_list[0];
    int32 max_frame_num = (1<<(vo->g_active_sps_ptr->log2_max_frame_num_minus4+4));

    for (i = 0; i < dpb_ptr->ref_frames_in_buffer; i++) {
        DEC_FRAME_STORE_T *fs_ref = dpb_ptr->fs_ref[i];

        if ((fs_ref->frame->used_for_reference)&&(!fs_ref->frame->is_long_term)) {
            if (fs_ref->frame_num > vo->frame_num) {
                fs_ref->frame_num_wrap = fs_ref->frame_num - max_frame_num;
            } else {
                fs_ref->frame_num_wrap = fs_ref->frame_num;
            }
        }

        fs_ref->frame->pic_num = fs_ref->frame_num_wrap;
    }

    //update long_term_pic_num
    for (i = 0; i < dpb_ptr->ltref_frames_in_buffer; i++) {
        DEC_STORABLE_PICTURE_T *frame = dpb_ptr->fs_ltref[i]->frame;

        if (frame->is_long_term) {
            frame->long_term_pic_num = frame->long_term_frame_idx;
        }
    }

    if (curr_slice_type == I_SLICE) {
        vo->g_list_size[0] = 0;
        vo->g_list_size[1] = 0;
        return;
    }

    if (curr_slice_type == P_SLICE) {
        /*p slice, put short and long term reference frame into list
        short term is in descending mode, long term is in ascending mode*/
        for (i = 0; i < dpb_ptr->ref_frames_in_buffer; i++) {
            if ((dpb_ptr->fs_ref[i]->frame->used_for_reference) && (!dpb_ptr->fs_ref[i]->frame->is_long_term)) {
                list[list0idx] = dpb_ptr->fs_ref[i]->frame;
                list0idx++;
            }
        }
        // order list 0 by PicNum
#if SYS_QSORT
        qsort((void*)list, list0idx, sizeof(DEC_STORABLE_PICTURE_T*), compare_pic_by_pic_num_desc);
#else
        compare_pic_by_pic_num_desc(list, list0idx);
#endif
        vo->g_list_size[0] = list0idx;

        //long term handling
        for(i = 0; i < dpb_ptr->ltref_frames_in_buffer; i++) {
            if (dpb_ptr->fs_ltref[i]->frame->is_long_term) {
                dpb_ptr->fs_ltref[i]->frame->long_term_pic_num = dpb_ptr->fs_ltref[i]->frame->long_term_frame_idx;
                list[list0idx] = dpb_ptr->fs_ltref[i]->frame;
                list0idx++;
            }
        }
#if SYS_QSORT
        qsort((void*)&list[vo->g_list_size[0]], list0idx-vo->g_list_size[0], sizeof(DEC_STORABLE_PICTURE_T*), compare_pic_by_lt_pic_num_asc);
#else
        compare_pic_by_lt_pic_num_asc(&list[vo->g_list_size[0]], list0idx-vo->g_list_size[0]);
#endif

        vo->g_list_size[0] = list0idx;
        vo->g_list_size[1] = 0;

        for (i = vo->g_list_size[0]; i < MAX_REF_FRAME_NUMBER+1; i++) {
            list[i] = vo->g_no_reference_picture_ptr;
        }
    } else {	//B-slice
        for (i = 0; i < dpb_ptr->ref_frames_in_buffer; i++) {
            DEC_FRAME_STORE_T *fs_ref = dpb_ptr->fs_ref[i];

            if ((fs_ref->frame->used_for_reference)&&(!fs_ref->frame->is_long_term)) {
                if (vo->framepoc > fs_ref->frame->poc) {
                    list[list0idx] = dpb_ptr->fs_ref[i]->frame;
                    list0idx++;
                }
            }
        }
#if SYS_QSORT
        qsort((void*)list, list0idx, sizeof(DEC_STORABLE_PICTURE_T*), compare_pic_by_poc_desc);
#else
        compare_pic_by_poc_desc(list, list0idx);
#endif
        list0idx_1 = list0idx;

        for (i = 0; i < dpb_ptr->ref_frames_in_buffer; i++) {
            if ((dpb_ptr->fs_ref[i]->frame->used_for_reference) && (!dpb_ptr->fs_ref[i]->frame->is_long_term)) {
                if (vo->framepoc < dpb_ptr->fs_ref[i]->frame->poc) {
                    list[list0idx] = dpb_ptr->fs_ref[i]->frame;
                    list0idx++;
                }
            }
        }
#if SYS_QSORT
        qsort((void*)&list[list0idx_1], list0idx-list0idx_1, sizeof(DEC_STORABLE_PICTURE_T*), compare_pic_by_poc_asc);
#else
        compare_pic_by_poc_asc(&list[list0idx_1], list0idx-list0idx_1);
#endif

        for (i = 0; i < list0idx_1; i++) {
            vo->g_list[1][list0idx-list0idx_1+i] = vo->g_list[0][i];
        }

        for (i = list0idx_1; i < list0idx; i++) {
            vo->g_list[1][i-list0idx_1] = vo->g_list[0][i];
        }
        vo->g_list_size[0] = vo->g_list_size[1] = list0idx;

        //long term handling
        for (i = 0; i < dpb_ptr->ltref_frames_in_buffer; i++) {
            if (dpb_ptr->fs_ltref[i]->frame->is_long_term) {
                dpb_ptr->fs_ltref[i]->frame->long_term_pic_num = dpb_ptr->fs_ltref[i]->frame->long_term_frame_idx;

                vo->g_list[0][list0idx] = dpb_ptr->fs_ltref[i]->frame;
                vo->g_list[1][list0idx++] = dpb_ptr->fs_ltref[i]->frame;
            }
        }
#if SYS_QSORT
        qsort((void *)&((vo->g_list)[0][vo->g_list_size[0]]), list0idx-vo->g_list_size[0], sizeof(DEC_STORABLE_PICTURE_T*), compare_pic_by_lt_pic_num_asc);
#else
        compare_pic_by_lt_pic_num_asc(&((vo->g_list)[0][vo->g_list_size[0]]), list0idx-vo->g_list_size[0]);
#endif

#if SYS_QSORT
        qsort((void *)&((vo->g_list)[1][vo->g_list_size[0]]), list0idx-vo->g_list_size[0], sizeof(DEC_STORABLE_PICTURE_T*), compare_pic_by_lt_pic_num_asc);
#else
        compare_pic_by_lt_pic_num_asc(&((vo->g_list)[1][vo->g_list_size[0]]), list0idx-vo->g_list_size[0]);
#endif
        vo->g_list_size[0] = vo->g_list_size[1] = list0idx;
    }

    if ((vo->g_list_size[0] == vo->g_list_size[1]) && (vo->g_list_size[0] > 1)) {
        // check if lists are identical, if yes swap first two elements of listX[1]	//???
        int32 diff = 0;

        for (i = 0; i < vo->g_list_size[0]; i++) {
            if (vo->g_list[0][i] != vo->g_list[1][i]) {
                diff = 1;
            }
        }

        if (!diff) {
            DEC_STORABLE_PICTURE_T *tmp_s;

            tmp_s = vo->g_list[1][0];
            vo->g_list[1][0] = vo->g_list[1][1];
            vo->g_list[1][1] = tmp_s;
        }
    }
    //set max size
    vo->g_list_size[0] = mmin(vo->g_list_size[0], vo->ref_count[0]);
    vo->g_list_size[1] = mmin(vo->g_list_size[1], vo->ref_count[1]);

    //set the unsed list entries to NULL
    for (i = vo->g_list_size[0]; i < MAX_REF_FRAME_NUMBER+1; i++) {
        vo->g_list[0][i] = vo->g_no_reference_picture_ptr;
    }

    for (i = vo->g_list_size[1]; i < MAX_REF_FRAME_NUMBER+1; i++) {
        vo->g_list[1][i] = vo->g_no_reference_picture_ptr;
    }

    return;
}

/**---------------------------------------------------------------------------*
**                         Compiler Flag                                      *
**---------------------------------------------------------------------------*/
#ifdef   __cplusplus
}
#endif
/**---------------------------------------------------------------------------*/
// End