/******************************************************************************
 ** File Name:      vsp_dct.h	                                              *
 ** Author:         Xiaowei Luo                                               *
 ** DATE:           06/11/2009                                                *
 ** Copyright:      2009 Spreatrum, Incoporated. All Rights Reserved.         *
 ** Description:    VSP DCT Module Driver									  *
 *****************************************************************************/
/******************************************************************************
 **                   Edit    History                                         *
 **---------------------------------------------------------------------------* 
 ** DATE          NAME            DESCRIPTION                                 * 
 ** 06/11/2009    Xiaowei Luo     Create.                                     *
 *****************************************************************************/
#ifndef _VSP_DCT_H_
#define _VSP_DCT_H_
/*----------------------------------------------------------------------------*
**                        Dependencies                                        *
**---------------------------------------------------------------------------*/

/**---------------------------------------------------------------------------*
**                        Compiler Flag                                       *
**---------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C"
{
#endif 
#define VSP_DCT_REG_BASE	(VSP_CTRL_BASE + 0x1000)
#define VSP_DCT_REG_SIZE	0x48

#define DCT_CONFIG_OFF						0x00
#define DCT_IN_CBP_OFF						0x04
#define DCT_Y_QUAN_PARA_OFF					0x08
#define DCT_UV_QUAN_PARA_OFF				0x0C
#define DCT_STATUS_OFF						0x10
#define DCT_OUT_CBP_OFF						0x14
#define DCT_QUANT_PARA_OFF					0x18
#define DCT_START_OFF						0x2C
#define DCT_MPEG4_DQUANT_PARA_OFF			0x30
//#define DCT_DC_PARA_OFF						0x34
#define IICT_CFG0_OFF						0x34
#define IICT_CFG1_OFF						0x38
#define IICT_CTRL_OFF						0x3c
#define RIDCT_CFG0_OFF					0x40
#define RIDCT_CFG1_OFF					0x44

#define DCT_CONFIG_WOFF						0x00
#define DCT_IN_CBP_WOFF						0x01
#define DCT_Y_QUAN_PARA_WOFF				0x02
#define DCT_UV_QUAN_PARA_WOFF				0x03
#define DCT_STATUS_WOFF						0x04
#define DCT_OUT_CBP_WOFF					0x05
#define DCT_QUANT_PARA_WOFF					0x06
#define DCT_START_WOFF						0x0B
#define DCT_MPEG4_DQUANT_PARA_WOFF			0x0C
#define IICT_CFG0_WOFF						0x0D
#define IICT_CFG1_WOFF						0x0E
#define IICT_CTRL_WOFF						0x0F
#define RIDCT_CFG0_WOFF						0x10
#define RIDCT_CFG1_WOFF						0x11

typedef struct  
{
	volatile uint32 DCT_CONFIG;					//[8]: dct_quant_en, 1'b1: quant enable	1'b0: quant disable		
												//[7]: Asp_mode, Asp mode enable
												//[6]: Asp_quan_type, 0: first type. With quan table	1: second type. Without quan table		
												//[5]: Inter_mode_en, For Mpeg4 H.263 used	1: Inter mode		0: Intra mode		
												//[4]: Mpeg4_quant_mode, 1: H.263 mode	0: Mpeg4 mode (first method)		
												//[1]: dct_run_mode, DCT/IDCT run mode.	1: Auto mode	0: Manual mode		
												//[0]: F_trans_en, For Jpeg and Mpeg H.263 used.	1: DCT mode		0: IDCT mode		
	
	volatile uint32 IDCT_IN_CBP;				//[5:0]:  

	volatile uint32 Y_Quan_para;				//[27:16]: Y_Quan_inv_shift, In doing DCT the 1/(2XQP) and shift the 1 to the highest bit.
												//[3:0]: Y_Quan_shift, The shift value of quan_inv_shift

	volatile uint32 UV_Quan_para;				//[27:16]: UV_Quan_inv_shift, In doing DCT the 1/(2XQP) and shift the 1 to the highest bit.
												//[3:0]: The shift value of quan_inv_shift

	volatile uint32 DCT_status;					//[4]: DCT_DONE, In menu mode indicate the DCT finished;
												//[0]: DCT_IDLE, DCT in IDLE mode

	volatile uint32 DCT_OUT_CBP;				//[5:0]: dct_out_cbp, 

	volatile uint32 DCT_Quan_Para;				//[20:9], Dct_quan_inv_shift, In doing DCT the 1/(2XQP) and shift the 1 to the highest bit.
												//[8:4], Dct_quan_value, QP value
												//[3:0], Dct_quan_shift, The shift value of quan_inv_shift
	volatile uint32 rsv[4];

	volatile uint32 Dct_start;					//[0]: Write a 1 to this register can start the dct begin to work in menu mode.	Read this register always get a 0.
		

	volatile uint32 Mpeg4_dequant_para;			//[20:16]: Quantiser_scale, Other coefficient dequant
												//[13:8]: UV_dc_scaler, Intra Chroma DC coeff dequant
												//[5:0]:  Y_dc_scaler, Intra luma DC coeff dequant

//	volatile uint32 Dc_parameter[3];			//Dc_parameter[0]: [31:16]- Dc_parameter_1, Block 1 DC parameter [15:0]: Dc_parameter_0, Block 0 DC parameter
												//Dc_parameter[1]: [31:16]- Dc_parameter_3, Block 3 DC parameter [15:0]: Dc_parameter_2, Block 2 DC parameter
												//Dc_parameter[2]: [31:16]- Dc_parameter_5, Block 5 DC parameter [15:0]: Dc_parameter_4, Block 4 DC parameter
	volatile uint32 iict_cfg0;					//[29:28]: mb_type, H264: b'00: i16x16 mb, b'01: skip mb, b'10: ipcm mb, b'11: other type
												//					Real: 0x00: intra 16x16 mb, 0x01: inter 16x16 ,0x11:other type 
												//[25:0]: cbp: bit23:0, 4x4 block, bit0 for block0, 
												//			   bit24: 1, if cbp>15, else 0
												//			   bit25: 1, if cbp>31, else 0

	volatile uint32 iict_cfg1;					//[16:12]: qp_per = qp_y/6
												//[11:8]:  qp_per_c = qp_c/6
												//[7:4]:   qp_rem = qp_y%6
												//[3:0]:   qp_rem_c = qp_c%6

	volatile uint32 iict_ctrl;					//[1]: iict_done,
												//[0]: iict_start_p, write only, self clear

	volatile uint32 ridct_cfg0;					//[29]: Rv_blk16_mode, Current MB is inter or intra block 16x16 mode, active high
												//[27:16]: Rv_y_quant, Luma AC bquant value
												//[11:0]: Rv_y_dc_bquant, Luma DC bquant value

	volatile uint32 ridct_cfg1;					//[27:16]: Rv_uv_quant, Chroma AC bquant value
												//[11:0]: Rv_uv_dc_bquant, Chroma DC bquant value

}VSP_DCT_REG_T;

/*DCT OR IDCT */
typedef enum {
		IDCT_MODE = 0, 
		DCT_MODE  
		}DCT_MODE_E;	

/*QUANT MODE*/
typedef enum {
		DCT_QUANT_DISABLE = 0, 
		DCT_QUANT_EN  
		}DCT_QUANT_ENABLE_E;

/*QUANT MODE*/
typedef enum {
		DCT_QUANT_WITH_TBL = 0, 
		DCT_QUANT_WIDHOUT_TBL  
		}DCT_QUANT_TYPE_E;

/*dct work mode*/	
typedef enum {
		DCT_MANUAL_MODE = 0, 
		DCT_AUTO_MODE,  
		DCT_FREE_MODE
		}DCT_WORK_MODE_E;

/*mpeg4/h263 mb type*/
typedef enum {
		DCT_INTRA_MB = 0, 
		DCT_INTER_MB  
		}DCT_MB_TYPE_E;

#define IQT_I16		(0 << 28)
#define IQT_SKIP 	(1 << 28)
#define IQT_PCM 	(2 << 28)
#define IQT_OTHER 	(3 << 28)

/**---------------------------------------------------------------------------*
**                         Compiler Flag                                      *
**---------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif 
/**---------------------------------------------------------------------------*/
// End 
#endif //_VSP_DCT_H_
