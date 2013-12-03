/*************************************************************************
** File Name:      ps_dec.c                                              *
** Author:         Reed zhang                                            *
** Date:           18/04/2006                                            *
** Copyright:      2001 Spreatrum, Incoporated. All Rights Reserved.     *
** Description:    realize SBR signal analyzing                          *               
**                                                                       *
**                        Edit History                                   *
** ----------------------------------------------------------------------*
** DATE           NAME             DESCRIPTION                           *
** 18/04/2006     Reed zhang       Create.                               *
**************************************************************************/
#include "aac_common.h"
#include "AAC_ps_dec.h"
#include "AAC_ps_decorrelate.h"
#include "AAC_ps_mix_phase.h"
#include "aac_structs.h"
#include "AAC_sbr_qmf.h"


extern const uint16 group_border20[23];
extern const uint16 map_group2bk20[22];

static void PsDataDecode(AAC_PS_INFO_T *ps_ptr);
static void DeltaDecode( uint8 enable, 
						 int8  *index, 
						 int8  *index_prev,
                         uint8 dt_flag, 
						 uint8 nr_par, 
						 uint8 stride,
                         int8  min_index,
						 int8  max_index);
#ifndef _AAC_PS_BASE_LINE_
static void DeltaModuloDecode(uint8 enable,
								int8  *index, 
								int8  *index_prev,
                                uint8 dt_flag, 
								uint8 nr_par,
								uint8 stride,
                                int8  log2modulo);
#endif
extern void AAC_DECORR_ReconstructAsm(AAC_PS_INFO_T *ps_ptr,
                                                                        aac_complex   *X_left_ptr,
                                                                        aac_complex   *X_hybrid_peft_ptr);
extern void AAC_PsPhaseMixAsm(AAC_PS_INFO_T *ps_ptr,
                              aac_complex   *X_left_ptr,
                              aac_complex   *X_hybrid_left_ptr);
extern void HybridSynthesisAsm(aac_complex    *X_left, 
                               aac_complex    *X_hybrid_left);
extern void AAC_PS_HybridAnalysisAsm(aac_complex   *overlap_ptr, 
                                     aac_complex    * X_ptr,        // S16.3 the input data and store the output left channel data 
                                     aac_complex    * X_hybrid_ptr, // S16.3 the input data and store the output hybrid channel data
                                     int16  slot);
/* Hybrid analysis: further split up QMF subbands
* to improve frequency resolution */

extern int32 g_frm_counter;


//////////////////////////////////////////////////////////////////////////
uint8 PsDecode( AAC_PS_INFO_T *ps_ptr, 
				aac_complex   X_left[38][64],   // S16.0 the input data and store the output left channel data
				void   *aac_dec_mem_ptr
				  )
{	
    AAC_PLUS_DATA_STRUC_T *aac_dec_struc_ptr = (AAC_PLUS_DATA_STRUC_T *) aac_dec_mem_ptr;
    int32   *tmp_shared_buffer_ptr = aac_dec_struc_ptr->g_shared_buffer;
    uint32 *P_eng_ptr;
    int32  *G_TransientRatio_ptr, *tmp_v;
    aac_complex    *X_hybrid_left_ptr, *X_hybrid_right_ptr;	
    aac_complex    *syn_right_ptr;
    int16  slot = 0, env = 0;
    AAC_PS_MIX_PHASETMP_VAR_T  ps_mix_phase_t;
    int32  *x_right_ptr;
    x_right_ptr = (int32 *)(aac_dec_struc_ptr->g_sbr_info.Xsbr) + 5120 + 4192;	
    ps_mix_phase_t.H11      = x_right_ptr;
    ps_mix_phase_t.H12      = x_right_ptr + 50;
    ps_mix_phase_t.H21      = x_right_ptr + 100;
    ps_mix_phase_t.H22      = x_right_ptr + 150;
    ps_mix_phase_t.deltaH11 = x_right_ptr + 200;
    ps_mix_phase_t.deltaH12 = x_right_ptr + 250;
    ps_mix_phase_t.deltaH21 = x_right_ptr + 300;
    ps_mix_phase_t.deltaH22 = x_right_ptr + 350;
    ps_mix_phase_t.opt_h11  = x_right_ptr + 400;
    ps_mix_phase_t.opt_h12  = x_right_ptr + 450;
    ps_mix_phase_t.opt_h21  = x_right_ptr + 500;
    ps_mix_phase_t.opt_h22  = x_right_ptr + 550;
	
    X_hybrid_left_ptr       = (aac_complex *) (x_right_ptr + 600);   // [32][2]
    X_hybrid_right_ptr      = (aac_complex *) (x_right_ptr + 664);   // [32][2]total size: 792 DWORDS
    syn_right_ptr           = (aac_complex *) (x_right_ptr + 728);   // [64][2]total size: 920 DWORDS
    P_eng_ptr               = (uint32 *) x_right_ptr + 856;
    G_TransientRatio_ptr    = (int32 *) (x_right_ptr + 856+32);
    /* delta decoding of the bitstream data */
    PsDataDecode(ps_ptr);  
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    tmp_v = tmp_shared_buffer_ptr + 4096;	
    AAC_DEC_MEMCPY(tmp_v, aac_dec_struc_ptr->g_qmf_syn[1], 1280 * sizeof(int32));
    AAC_DEC_MEMSET(syn_right_ptr, 0, (64) * sizeof(aac_complex));
#ifdef PS_TEST_DATA
    if (g_frm_counter > TEST_FRAME)
	{
		//FILE *fp = fopen("PS_in.dat", "wb");
		int cc, ll;        
		for (cc = 0; cc < 32; cc ++)
		{
			//fprintf(fp, "Line %d, \n", cc);
			for (ll = 0; ll < 32; ll ++)
			{
				int32 l_low_re, l_low_im, l_high_re, l_hign_im;
                                X_left[cc][ll][0]    = cc * 98242 + ll * 19123 + 3445;
                                X_left[cc][ll][1]    = cc * 18242 + ll * 99123 + 13445;	
                                X_left[cc][ll+32][0] = cc * 38242 + ll * 89123 + 21445;
                                X_left[cc][ll+32][1] = cc * 88242 + ll * 21912 + 98445;
				l_low_re  = X_left[cc][ll][0];
				l_low_im  = X_left[cc][ll][1];				
				l_high_re = X_left[cc][ll+32][0];
				l_hign_im = X_left[cc][ll+32][1];		
				
				/*fprintf(fp, "%10d, %10d, %10d, %10d, \n",
					l_low_re, l_low_im, l_high_re, l_hign_im);*/				
			}	
		}
		for (cc = 0; cc < 44; cc ++)			 
		{
			ps_ptr->hyb.buffer[0][cc][0] = cc* 1334 + 2342;
			ps_ptr->hyb.buffer[0][cc][1] = cc* 4433 + 1312;

			ps_ptr->hyb.buffer[1][cc][0] = cc* 5334 + 5342;
			ps_ptr->hyb.buffer[1][cc][1] = cc* 9433 + 6312;

			ps_ptr->hyb.buffer[2][cc][0] = cc* 8334 + 7342;
			ps_ptr->hyb.buffer[2][cc][1] = cc* 7433 + 8312;
		}/*
		fclose(fp);*/
	}
#endif
    for (slot = 0; slot < 32; slot++)
    {	    
        if (slot == ps_ptr->border_position[env])
        {
            AAC_ClacPsMixPhaseEnvelope(ps_ptr, 				
                                    &ps_mix_phase_t,
                                    slot,
                                    env);
           env++;		 
        }
#ifdef PS_TEST_DATA
        AAC_DEC_MEMSET(X_hybrid_left_ptr, 0, 32 * sizeof(int32));
#endif
        /* Perform further analysis on the lowest subbands to get a higher frequency resolution */
        AAC_PS_HybridAnalysisAsm((aac_complex   *)ps_ptr->hyb.buffer, 
                                 X_left[slot+6],        // S16.3 the input data and store the output left channel data 
                                 X_hybrid_left_ptr, // S16.3 the input data and store the output hybrid channel data
                                 slot);
#ifdef PS_TEST_DATA
		if (g_frm_counter > TEST_FRAME)
		{  
			int32 ti;
                       FILE   *ps_decorrelation_ptr;
			//if (ps_decorrelation_ptr == NULL)
			{
				ps_decorrelation_ptr = fopen("ps_decorrelate_out.txt", "wb");
			}			
			AAC_DEC_MEMSET(syn_right_ptr,  0,  64*4);
			for (ti = 0; ti < 12; ti ++)
			{
				int32 re0, im0;				
				re0 = X_hybrid_left_ptr[ti][0];
				im0 = X_hybrid_left_ptr[ti][1];							
				fprintf(ps_decorrelation_ptr, "Line: %10d,  0x%08x,  0x%08x\n", ti, (uint32 )re0, (uint32 )im0);
			}
			for (ti = 0; ti < 44; ti ++)
			{
				int32 re0, im0, re1, im1, re2, im2;
				re0 = ps_ptr->hyb.buffer[0][ti][0];
				im0 = ps_ptr->hyb.buffer[0][ti][1];							

				re1 = ps_ptr->hyb.buffer[1][ti][0];
				im1 = ps_ptr->hyb.buffer[1][ti][1];							

				re2 = ps_ptr->hyb.buffer[2][ti][0];
				im2 = ps_ptr->hyb.buffer[2][ti][1];							
				fprintf(ps_decorrelation_ptr, "Line: %10d,  0x%08x,  0x%08x,  0x%08x,  0x%08x,  0x%08x,  0x%08x\n", ti, 
					        (uint32 )re0, (uint32 )im0, (uint32 )re1, (uint32 )im1, (uint32 )re2, (uint32 )im2);
			}			
			fclose(ps_decorrelation_ptr);
			ps_decorrelation_ptr = NULL;
		}
		
#endif
        AAC_DECORR_ReconstructAsm(ps_ptr,
                      X_left[slot],
                      X_hybrid_left_ptr);
#ifdef PS_TEST_DATA
        if (g_frm_counter > TEST_FRAME)
		{
			int32 ti;
			FILE *ps_mix_phase_ptr = NULL;
			//if (NULL ==ps_mix_phase_ptr)
			{
				ps_mix_phase_ptr = fopen("PsDecorrelate.dat", "wb");
			}
			fprintf(ps_mix_phase_ptr, "SLOT_Group %d,  Index,  X_hybrid_left_re, X_hybrid_left_im,  X_hybrid_right_re, X_hybrid_right_im, \n", slot);
			/* left hybrid */
			for (ti = 0; ti < 32; ti ++)
			{
				int32 d_re, d_im;
				int32 dr_re, dr_im;
				//re_low_ptr[i] = re_low_ptr[i] + i * 10;
				d_re  = X_hybrid_left_ptr[ti][0];
				d_im  = X_hybrid_left_ptr[ti][1];
				dr_re = X_hybrid_right_ptr[ti][0];
				dr_im = X_hybrid_right_ptr[ti][1];
				
				fprintf(ps_mix_phase_ptr, "   LINE:%10d, %10d, %10d, %10d, %10d\n", ti, d_re, d_im, dr_re, dr_im);
				
			}
			
			fprintf(ps_mix_phase_ptr, "SLOT_Group %d,  Index,  X_left_low_re, X_left_low_im,  X_left_high_re, X_left_high_im, \n", slot);
			/* left */
			for (ti = 0; ti < 32; ti ++)
			{
				int32 d_re, d_im;
				int32 dr_re, dr_im;
				//re_low_ptr[i] = re_low_ptr[i] + i * 10;
				d_re   = X_left[slot][ti][0];
				d_im   = X_left[slot][ti][1];
				dr_re  = X_left[slot][ti+32][0];
				dr_im  = X_left[slot][ti+32][1];
				fprintf(ps_mix_phase_ptr, "   LINE:%10d, %10d, %10d, %10d, %10d\n", ti, d_re, d_im, dr_re, dr_im);
				
			}
			fprintf(ps_mix_phase_ptr, "SLOT_Group %d,  Index,  X_r_low_re, X_r_low_im,  X_r_high_re, X_r_high_im, \n", slot);
			/* right */
			for (ti = 0; ti < 32; ti ++)
			{
				int32 d_re, d_im;
				int32 dr_re, dr_im;
				//re_low_ptr[i] = re_low_ptr[i] + i * 10;
				d_re   = syn_right_ptr[ti][0];;
				d_im   = syn_right_ptr[ti][1];;
				dr_re  = syn_right_ptr[ti+32][0];;
				dr_im  = syn_right_ptr[ti+32][1];;
				fprintf(ps_mix_phase_ptr, "   LINE:%10d, %10d, %10d, %10d, %10d\n", ti, d_re, d_im, dr_re, dr_im);
				
			}
			
			fclose(ps_mix_phase_ptr);
			ps_mix_phase_ptr = NULL;
		}
#endif

	    /* apply mixing and phase parameters */        
        AAC_PsPhaseMixAsm(ps_ptr,
                      X_left[slot],
                      X_hybrid_left_ptr);
#ifdef PS_TEST_DATA
        if (g_frm_counter > TEST_FRAME)
		{
			int32 ti;
			FILE *ps_mix_phase_ptr = NULL;
			//if (NULL ==ps_mix_phase_ptr)
			{
				ps_mix_phase_ptr = fopen("Ps_mix_phase_out.dat", "wb");
			}
			fprintf(ps_mix_phase_ptr, "SLOT_Group %d,  Index,  X_hybrid_left_re, X_hybrid_left_im,  X_hybrid_right_re, X_hybrid_right_im, \n", slot);
			/* left hybrid */
			for (ti = 0; ti < 32; ti ++)
			{
				int32 d_re, d_im;
				int32 dr_re, dr_im;
				//re_low_ptr[i] = re_low_ptr[i] + i * 10;
				d_re  = X_hybrid_left_ptr[ti][0];
				d_im  = X_hybrid_left_ptr[ti][1];
				dr_re = X_hybrid_right_ptr[ti][0];
				dr_im = X_hybrid_right_ptr[ti][1];

				fprintf(ps_mix_phase_ptr, "   LINE:%10d, %10d, %10d, %10d, %10d\n", ti, d_re, d_im, dr_re, dr_im);
				
			}
			
			fprintf(ps_mix_phase_ptr, "SLOT_Group %d,  Index,  X_left_low_re, X_left_low_im,  X_left_high_re, X_left_high_im, \n", slot);
			/* left */
			for (ti = 0; ti < 32; ti ++)
			{
				int32 d_re, d_im;
				int32 dr_re, dr_im;
				//re_low_ptr[i] = re_low_ptr[i] + i * 10;
				d_re   = X_left[slot][ti][0];
				d_im   = X_left[slot][ti][1];
				dr_re  = X_left[slot][ti+32][0];
				dr_im  = X_left[slot][ti+32][1];
				fprintf(ps_mix_phase_ptr, "   LINE:%10d, %10d, %10d, %10d, %10d\n", ti, d_re, d_im, dr_re, dr_im);
				
			}
			fprintf(ps_mix_phase_ptr, "SLOT_Group %d,  Index,  X_r_low_re, X_r_low_im,  X_r_high_re, X_r_high_im, \n", slot);
			/* right */
			for (ti = 0; ti < 32; ti ++)
			{
				int32 d_re, d_im;
				int32 dr_re, dr_im;
				//re_low_ptr[i] = re_low_ptr[i] + i * 10;
				d_re   = syn_right_ptr[ti][0];;
				d_im   = syn_right_ptr[ti][1];;
				dr_re  = syn_right_ptr[ti+32][0];;
				dr_im  = syn_right_ptr[ti+32][1];;
				fprintf(ps_mix_phase_ptr, "   LINE:%10d, %10d, %10d, %10d, %10d\n", ti, d_re, d_im, dr_re, dr_im);
				
			}
			
			fclose(ps_mix_phase_ptr);
			ps_mix_phase_ptr = NULL;
		}
#endif

        HybridSynthesisAsm(X_left[slot],
                                        X_hybrid_left_ptr);	
#ifdef PS_TEST_DATA
        if (g_frm_counter > TEST_FRAME)
        {  
            int32 ti;
            FILE   *ps_decorrelation_ptr;
            ps_decorrelation_ptr = fopen("ps_out.txt", "wb");            
            for (ti = 0; ti < 64; ti ++)
            {
                int32 re0, im0, re1, im1;
                
                //re_low_ptr[i] = re_low_ptr[i] + i * 10;
                re0 = X_left[slot][ti][0];
                im0 = X_left[slot][ti][1];				
                
                re1 = syn_right_ptr[ti][0];
                im1 = syn_right_ptr[ti][1];				

                fprintf(ps_decorrelation_ptr, "Line: %10d,  %8d,  %8d,  %8d,  %8d\n", ti, re0, im0, re1, im1);
            }
            
            fclose(ps_decorrelation_ptr);
            ps_decorrelation_ptr = NULL;
        }
        
#endif
        /* right channel syn */		    	   
        AAC_SbrQmfSynthesis(1, 
                                              tmp_v,
                                              (aac_complex (*)[64])syn_right_ptr, //aac_dec_struc_ptr->g_sbr_info.Xsbr[1],   // S17.0
                                              aac_dec_struc_ptr->pcm_out_r_ptr + slot * 64,
                                              tmp_v); 
        tmp_v -= 128;		   
    }
    AAC_DEC_MEMCPY(aac_dec_struc_ptr->g_qmf_syn[1], tmp_v, 1280 * sizeof(int32));
    /* left channel syn */
    AAC_SbrQmfSynthesis(32, 
                                         aac_dec_struc_ptr->g_qmf_syn[0],
                                         X_left,
                                         aac_dec_struc_ptr->pcm_out_l_ptr,
                                         tmp_shared_buffer_ptr); 
    /* for hybrid analysis filter-bank */
    for (slot = 0; slot < 3; slot++)
    {     
        /* store samples */
        AAC_DEC_MEMCPY(ps_ptr->hyb.buffer[slot], ps_ptr->hyb.buffer[slot] + ps_ptr->hyb.frame_len, 12 * sizeof(aac_complex));	
    }	
    return 0;
}
void map34indexto20(int8 *index, int16 bins)
{
    index[0] =  (int8) ((2*index[0]+index[1])/3);
    index[1] =  (int8) ((index[1]+2*index[2])/3);
    index[2] =  (int8) ((2*index[3]+index[4])/3);
    index[3] =  (int8) ((index[4]+2*index[5])/3);
    index[4] =  (int8) ((index[6]+index[7])/2);
    index[5] =  (int8) ((index[8]+index[9])/2);
    index[6] = index[10];
    index[7] = index[11];
    index[8] =  (int8) ((index[12]+index[13])/2);
    index[9] =  (int8) ((index[14]+index[15])/2);
    index[10] = index[16];
    if (bins == 34)
    {
        index[11] = index[17];
        index[12] = index[18];
        index[13] = index[19];
        index[14] = (int8) ((index[20]+index[21])/2);
        index[15] = (int8) ((index[22]+index[23])/2);
        index[16] = (int8) ((index[24]+index[25])/2);
        index[17] = (int8) ((index[26]+index[27])/2);
        index[18] = (int8) ((index[28]+index[29]+index[30]+index[31])/4);
        index[19] = (int8) ((index[32]+index[33])/2);
    }
}

/* parse the bitstream data decoded in ps_data() */
static void PsDataDecode(AAC_PS_INFO_T *ps_ptr)
{
    uint8 env, bin;
	uint8 tmp_var;
	int8  t0;
    /* ps_ptr data not available, use data from previous frame */
    if (ps_ptr->ps_data_available == 0)
    {
        ps_ptr->num_env = 0;
    }

    for (env = 0; env < ps_ptr->num_env; env++)
    {
        int8 *iid_index_prev;
        int8 *icc_index_prev;
        int32 num_iid_steps = (ps_ptr->iid_mode < 3) ? 7 : 15 /*fine quant*/;

        if (env == 0)
        {
            /* take last envelope from previous frame */
            iid_index_prev = ps_ptr->iid_index_prev;
            icc_index_prev = ps_ptr->icc_index_prev;
        } else {
            /* take index values from previous envelope */
            iid_index_prev = ps_ptr->iid_index[env - 1];
            icc_index_prev = ps_ptr->icc_index[env - 1];
        }

        /* delta decode iid parameters */
        tmp_var = (int8) (((ps_ptr->iid_mode == 0 || ps_ptr->iid_mode == 3) ? 2 : 1));
        t0      = (int8) (-num_iid_steps);
        DeltaDecode(ps_ptr->enable_iid,
					 ps_ptr->iid_index[env], 
					 iid_index_prev,
					 ps_ptr->iid_dt[env], 
					 ps_ptr->nr_iid_par,
					 tmp_var,
					 t0,
                             (int8)num_iid_steps);
        /* delta decode icc parameters */
		tmp_var = (int8) ((ps_ptr->icc_mode == 0 || ps_ptr->icc_mode == 3) ? 2 : 1);
        DeltaDecode(ps_ptr->enable_icc, 
					 ps_ptr->icc_index[env], 
					 icc_index_prev,
                     ps_ptr->icc_dt[env],
					 ps_ptr->nr_icc_par,
					 tmp_var,
					 0, 
					 7);

    }

    /* handle error case */
    if (ps_ptr->num_env == 0)
    {
        /* force to 1 */
        ps_ptr->num_env = 1;

        if (ps_ptr->enable_iid)
        {
            for (bin = 0; bin < 34; bin++)
                ps_ptr->iid_index[0][bin] = ps_ptr->iid_index_prev[bin];
        } else {
            for (bin = 0; bin < 34; bin++)
                ps_ptr->iid_index[0][bin] = 0;
        }

        if (ps_ptr->enable_icc)
        {
            for (bin = 0; bin < 34; bin++)
                ps_ptr->icc_index[0][bin] = ps_ptr->icc_index_prev[bin];
        } else {
            for (bin = 0; bin < 34; bin++)
                ps_ptr->icc_index[0][bin] = 0;
        }
    }

    /* update previous indices */
    for (bin = 0; bin < 34; bin++)
        ps_ptr->iid_index_prev[bin] = ps_ptr->iid_index[ps_ptr->num_env-1][bin];
    for (bin = 0; bin < 34; bin++)
        ps_ptr->icc_index_prev[bin] = ps_ptr->icc_index[ps_ptr->num_env-1][bin];
    ps_ptr->ps_data_available = 0;
    if (ps_ptr->frame_class == 0)
    {
        ps_ptr->border_position[0] = 0;
        for (env = 1; env < ps_ptr->num_env; env++)
        {
            ps_ptr->border_position[env] = (int8) ((env * 32) / ps_ptr->num_env);
        }
        ps_ptr->border_position[ps_ptr->num_env] = 32 /* 30 for 960? */;
    } else 
	{
        ps_ptr->border_position[0] = 0;
        if (ps_ptr->border_position[ps_ptr->num_env] < 32 /* 30 for 960? */)
        {
            ps_ptr->num_env++;
            ps_ptr->border_position[ps_ptr->num_env] = 32 /* 30 for 960? */;
            for (bin = 0; bin < 34; bin++)
            {
                ps_ptr->iid_index[ps_ptr->num_env][bin] = ps_ptr->iid_index[ps_ptr->num_env-1][bin];
                ps_ptr->icc_index[ps_ptr->num_env][bin] = ps_ptr->icc_index[ps_ptr->num_env-1][bin];
            }
        }
        for (env = 1; env < ps_ptr->num_env; env++)
        {
            int8 thr = (int8) (32 - (ps_ptr->num_env - env));

            if (ps_ptr->border_position[env] > thr)
            {
                ps_ptr->border_position[env] = thr;
            } else {
                thr = (int8) (ps_ptr->border_position[env-1]+1);
                if (ps_ptr->border_position[env] < thr)
                {
                    ps_ptr->border_position[env] = thr;
                }
            }
        }
    }

    /* make sure that the indices of all parameters can be mapped
     * to the same hybrid synthesis filterbank */
	ps_ptr->use34hybrid_bands = 0;
    for (env = 0; env < ps_ptr->num_env; env++)
    {
       if (ps_ptr->iid_mode == 2 || ps_ptr->iid_mode == 5)
	      map34indexto20(ps_ptr->iid_index[env], 34);
       if (ps_ptr->icc_mode == 2 || ps_ptr->icc_mode == 5)
          map34indexto20(ps_ptr->icc_index[env], 34);		
    }
    ps_ptr->group_border      = (uint16 *)group_border20;
    ps_ptr->map_group2bk      = (uint16 *)map_group2bk20;
    ps_ptr->num_groups        = 22;
    ps_ptr->num_hybrid_groups = 10;
    ps_ptr->nr_par_bands      = 20;
    ps_ptr->decay_cutoff      = 3;
        
    
}

/* delta decode array */
static void DeltaDecode(uint8 enable, 
						 int8  *index,
						 int8  *index_prev,
                         uint8 dt_flag, 
						 uint8 nr_par, 
						 uint8 stride,
                         int8  min_index,
						 int8  max_index)
{
    int8 i;
    int8 temp;
    if (enable == 1)
    {
        if (dt_flag == 0)
        {
            /* delta coded in frequency direction */
            temp = (int8) (0 + index[0]);
            index[0] = (int8) (SBR_CLIP(temp, min_index, max_index));
			
            for (i = 1; i < nr_par; i++)
            {
                temp = (int8) (index[i-1] + index[i]);
                index[i] = (int8) (SBR_CLIP(temp, min_index, max_index));
            }
        } else 
		{
            /* delta coded in time direction */
            for (i = 0; i < nr_par; i++)
            {
                temp = (int8) (index_prev[i*stride] + index[i]);               
                index[i] = (int8) (SBR_CLIP(temp, min_index, max_index));	
            }
        }
    } else 
	{
        /* set indices to zero */
        for (i = 0; i < nr_par; i++)
        {
            index[i] = 0;
        }
    }	
    /* coarse */
    if (stride == 2)
    {
        for (i = (int8) ((nr_par<<1)-1); i > 0; i--)
        {
            index[i] = index[i>>1];
        }

	
			
	

	
    }
}



