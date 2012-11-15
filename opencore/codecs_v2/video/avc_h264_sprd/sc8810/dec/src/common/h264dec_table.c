/******************************************************************************
 ** File Name:    h264dec_table.c                                             *
 ** Author:       Xiaowei.Luo                                                 *
 ** DATE:         03/29/2010                                                  *
 ** Copyright:    2010 Spreatrum, Incoporated. All Rights Reserved.           *
 ** Description:                                                              *
 *****************************************************************************/
/******************************************************************************
 **                   Edit    History                                         *
 **---------------------------------------------------------------------------* 
 ** DATE          NAME            DESCRIPTION                                 * 
 ** 03/29/2010    Xiaowei.Luo     Create.                                     *
 *****************************************************************************/
/*----------------------------------------------------------------------------*
**                        Dependencies                                        *
**---------------------------------------------------------------------------*/
#include "sc8810_video_header.h"
/**---------------------------------------------------------------------------*
**                        Compiler Flag                                       *
**---------------------------------------------------------------------------*/
#ifdef   __cplusplus
    extern   "C" 
    {
#endif

/**----------------------------------------------------------------------------*
**                           Function Prototype                               **
**----------------------------------------------------------------------------*/

DECLARE_ASM_CONST (4, int8, g_ICBP_TBL[6]) = {0, 16, 32, 15, 31, 47};

DECLARE_ASM_CONST (4, uint8, g_QP_SCALER_CR_TBL[52]) =
{
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,
	12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,
	28,29,29,30,31,32,32,33,34,34,35,35,36,36,37,37,
	37,38,38,38,39,39,39,39		
};

/*cbp table for intra MB*/
DECLARE_ASM_CONST (4, uint8, g_cbp_intra_tbl [48]) = 
{
	47, 31, 15,  0, 23, 27, 29, 30,  7, 11, 13, 14,
	39, 43, 45, 46, 16,  3,  5, 10, 12, 19, 21, 26,
	28, 35, 37, 42, 44,  1,  2,  4,  8, 17, 18, 20, 
	24,  6,  9, 22, 25, 32, 33, 34, 36, 40, 38, 41,
};

/*cbp table for inter MB*/
DECLARE_ASM_CONST (4, uint8, g_cbp_inter_tbl [48]) = 
{
	 0, 16,  1,  2,  4,  8, 32,  3,
	 5, 10, 12, 15, 47,  7, 11, 13, 
	14,  6,  9, 31, 35, 37, 42, 44, 
	33, 34, 36, 40, 39, 43, 45, 46, 
	17, 18, 20, 24, 19, 21, 26, 28, 
	23, 27, 29, 30, 22, 25, 38, 41,
};

//qp_per<<8 | (qp_rem<<0)
DECLARE_ASM_CONST (4, uint16, g_qpPerRem_tbl [52]) = 
{
	((0<<8)|0), ((0<<8)|1), ((0<<8)|2), ((0<<8)|3), ((0<<8)|4), ((0<<8)|5),
	((1<<8)|0), ((1<<8)|1), ((1<<8)|2), ((1<<8)|3), ((1<<8)|4), ((1<<8)|5),
	((2<<8)|0), ((2<<8)|1), ((2<<8)|2), ((2<<8)|3), ((2<<8)|4), ((2<<8)|5),
	((3<<8)|0), ((3<<8)|1), ((3<<8)|2), ((3<<8)|3), ((3<<8)|4), ((3<<8)|5),
	((4<<8)|0), ((4<<8)|1), ((4<<8)|2), ((4<<8)|3), ((4<<8)|4), ((4<<8)|5),
	((5<<8)|0), ((5<<8)|1), ((5<<8)|2), ((5<<8)|3), ((5<<8)|4), ((5<<8)|5),
	((6<<8)|0), ((6<<8)|1), ((6<<8)|2), ((6<<8)|3), ((6<<8)|4), ((6<<8)|5),
	((7<<8)|0), ((7<<8)|1), ((7<<8)|2), ((7<<8)|3), ((7<<8)|4), ((7<<8)|5),
	((8<<8)|0), ((8<<8)|1), ((8<<8)|2), ((8<<8)|3)
};

/*block order map from decoder order to context cache order*/
DECLARE_ASM_CONST (4, uint8, g_blk_order_map_tbl[16+2 * 4]) = 
{
    	1 *12+4, 1 *12+5, 2 *12+4, 2 *12+5,  //first one block8x8
	1 *12+6, 1 *12+7, 2 *12+6, 2 *12+7, 
	3 *12+4, 3 *12+5, 4 *12+4, 4 *12+5, 
	3 *12+6, 3 *12+7, 4 *12+6, 4 *12+7, 

	6 *12+4, 6 *12+5, 7 *12+4, 7 *12+5,  //U's 4 block4x4
	6 *12+8, 6 *12+9, 7 *12+8, 7 *12+9, 
};

DECLARE_ASM_CONST (4, uint32, g_huff_tab_token [69]) = 
{
	0x0041c701, 0x4100c641, 0x8200c500, 0x0000c400, 0x42c4c302, 0x01c38242, 0xc3824182, 0xc3820000, 
	0xc5c64503, 0x83838543, 0xc4434483, 0xc40184c3, 0xc6c54304, 0x84c5c844, 0x43428384, 0x024242c4, 
	0xc7c70305, 0x85848745, 0x44444785, 0x030202c5, 0xc8c8c906, 0x86858646, 0x45454686, 0x040301c6, 
	0xc9050707, 0x87860647, 0x46468987, 0x050405c7, 0x00c9ca08, 0x00878848, 0x00474888, 0x000604c8, 
	0x08cbcc09, 0x89898b49, 0x48494a89, 0x070809c9, 0xcacacb0a, 0x88888a4a, 0x4748498a, 0x060708ca, 
	0xcc0b0c0b, 0x8b8b8d4b, 0x4a4b4c8b, 0x0a0a0bcb, 0xcbcccd0c, 0x8a8a8c4c, 0x494a4b8c, 0x09090acc, 
	0xcece4f0d, 0x8d8d0e4d, 0x4c4dce8d, 0x0c0d8ecd, 0xcdcd4e0e, 0x8c8c0d4e, 0x4b4c4d8e, 0x0b0c4dce, 
	0xd04f500f, 0x8f0f0f4f, 0x4f8fcf8f, 0x0e4e8fcf, 0xcf8ed010, 0x8e8e9050, 0x4e0e1090, 0x0d0e00d0, 
	0x10d00000, 0x90900000, 0x50500000, 0x0f100000, 0x4dcf0000
};

DECLARE_ASM_CONST (4, uint8, g_b8_offset[4]) = { 0, 2, CTX_CACHE_WIDTH_X2, CTX_CACHE_WIDTH_X2+2};
DECLARE_ASM_CONST (4, uint8, g_b4_offset[4]) = {0, 1, CTX_CACHE_WIDTH, CTX_CACHE_WIDTH+1};

DECLARE_ASM_CONST (4, uint8, g_b8map[16]) = 
{
	0, 0, 1, 1,
	0, 0, 1, 1, 
	2, 2, 3, 3,
	2, 2, 3, 3,
};

DECLARE_ASM_CONST (4, uint32, g_msk[33]) =
{
	0x00000000, 0x00000001, 0x00000003, 0x00000007,
	0x0000000f, 0x0000001f, 0x0000003f, 0x0000007f,
	0x000000ff, 0x000001ff, 0x000003ff, 0x000007ff,
	0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff,
	0x0000ffff, 0x0001ffff, 0x0003ffff, 0x0007ffff,
	0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff,
	0x00ffffff, 0x01ffffff, 0x03ffffff, 0x07ffffff,
	0x0fffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff,
	0xffffffff
};

DECLARE_ASM_CONST(4, uint8,dequant4_coeff_init[6][3])={
  {10,13,16},
  {11,14,18},
  {13,16,20},
  {14,18,23},
  {16,20,25},
  {18,23,29},
};

DECLARE_ASM_CONST(4, uint8, dequant8_coeff_init_scan[16]) = {
  0,3,4,3, 3,1,5,1, 4,5,2,5, 3,1,5,1
};
DECLARE_ASM_CONST(4, uint8, dequant8_coeff_init[6][6])={
  {20,18,32,19,25,24},
  {22,19,35,21,28,26},
  {26,23,42,24,33,31},
  {28,25,45,26,35,33},
  {32,28,51,30,40,38},
  {36,32,58,34,46,43},
};

DECLARE_ASM_CONST (4, int8, g_inverse_zigzag_tbl[16]) =
{
	0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15,
};

DECLARE_ASM_CONST (4, int8, g_inverse_zigzag_cabac_I16_ac_tbl[15]) =
{
	1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15,
};

DECLARE_ASM_CONST (4, int8, g_inverse_8x8_zigzag_tbl[64]) =
{
#if 0 //normal
	0, 1, 8, 16, 9, 2, 3, 10,
	17, 24, 32, 25, 18, 11, 4, 5,
	12, 19, 26, 33, 40, 48, 41, 34,
	27, 20, 13, 6, 7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63,
#else
	0,	8,	1,	2,	9,	16,	24,	17,	
	10,	3,	4,	11,	18,	25,	32,	40,	
	33,	26,	19,	12,	5,	6,	13,	20,	
	27,	34,	41,	48,	56,	49,	42,	35,	
	28,	21,	14,	7,	15,	22,	29,	36,	
	43,	50,	57,	58,	51,	44,	37,	30,	
	23,	31,	38,	45,	52,	59,	60,	53,	
	46,	39,	47,	54,	61,	62,	55,	63,
#endif
};

// zigzag_scan8x8_cavlc[i] = zigzag_scan8x8[(i/4) + 16*(i%4)]
DECLARE_ASM_CONST (4, int8, g_inverse_8x8_zigzag_tbl_cavlc[64]) =
{
#if 0 //normal
	0+0*8, 1+1*8, 1+2*8, 2+2*8,
 	4+1*8, 0+5*8, 3+3*8, 7+0*8,
	3+4*8, 1+7*8, 5+3*8, 6+3*8,
 	2+7*8, 6+4*8, 5+6*8, 7+5*8,
 	1+0*8, 2+0*8, 0+3*8, 3+1*8,
 	3+2*8, 0+6*8, 4+2*8, 6+1*8,
 	2+5*8, 2+6*8, 6+2*8, 5+4*8,
 	3+7*8, 7+3*8, 4+7*8, 7+6*8,
 	0+1*8, 3+0*8, 0+4*8, 4+0*8,
 	2+3*8, 1+5*8, 5+1*8, 5+2*8,
 	1+6*8, 3+5*8, 7+1*8, 4+5*8,
	4+6*8, 7+4*8, 5+7*8, 6+7*8,
 	0+2*8, 2+1*8, 1+3*8, 5+0*8,
 	1+4*8, 2+4*8, 6+0*8, 4+3*8,
 	0+7*8, 4+4*8, 7+2*8, 3+6*8,
 	5+5*8, 6+5*8, 6+6*8, 7+7*8,
#else
	0+0*8, 1+1*8, 2+1*8, 2+2*8,
 	1+4*8, 5+0*8, 3+3*8, 0+7*8,
	4+3*8, 7+1*8, 3+5*8, 3+6*8,
 	7+2*8, 4+6*8, 6+5*8, 5+7*8,
 	0+1*8, 0+2*8, 3+0*8, 1+3*8,
 	2+3*8, 6+0*8, 2+4*8, 1+6*8,
 	5+2*8, 6+2*8, 2+6*8, 4+5*8,
 	7+3*8, 3+7*8, 7+4*8, 6+7*8,
 	1+0*8, 0+3*8, 4+0*8, 0+4*8,
 	3+2*8, 5+1*8, 1+5*8, 2+5*8,
 	6+1*8, 5+3*8, 1+7*8, 5+4*8,
	6+4*8, 4+7*8, 7+5*8, 7+6*8,
 	2+0*8, 1+2*8, 3+1*8, 0+5*8,
 	4+1*8, 4+2*8, 0+6*8, 3+4*8,
 	7+0*8, 4+4*8, 2+7*8, 6+3*8,
 	5+5*8, 5+6*8, 6+6*8, 7+7*8,
#endif
};

DECLARE_ASM_CONST (4, uint8, g_clip_tbl[1024+256]) =
{  
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
	0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15, 
	16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31, 
	32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47, 
	48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63, 
	64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79, 
	80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95, 
	96,  97,  98,  99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 
	112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 
	128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 
	144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 
	160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 
	176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 
	192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 
	208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 
	224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 
	240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
	255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 
};

DECLARE_ASM_CONST (4, uint8, g_blKIndex [16]) =
{
	0, 1, 4, 5, 
	2, 3, 6, 7, 
	8, 9, 12, 13,	
	10, 11, 14, 15,
};

DECLARE_ASM_CONST (4, int8, g_blkC_avaiable_tbl[16]) =
{
	1, 1, 1, 1, 
	1, 0, 1, 0, 
	1, 1, 1, 0, 
	1, 0, 1, 0,
};

DECLARE_ASM_CONST (4, uint8, g_mbcache_addr_map_tbl[16+4]) =
{
	//y
	16*0 + 0, 16*0 + 4 , 16*4 + 0, 16*4 + 4,
	16*0 + 8, 16*0 + 12, 16*4 + 8, 16*4 + 12, 
	16*8 + 0, 16*8 + 4 , 16*12 + 0, 16*12 + 4,
	16*8 + 8, 16*8 + 12, 16*12 + 8, 16*12 + 12, 

	//uv
	0, 4, 8*4, 8*4 + 4,
};

DECLARE_ASM_CONST (4, uint8, g_dec_order_to_scan_order_map_tbl[16]) =
{
	0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15
};

DECLARE_ASM_CONST (4, uint8, weightscale4x4_intra_default[16]) = 
{
	 6,13,20,28,
	13,20,28,32,
	20,28,32,37,
	28,32,37,42
};

DECLARE_ASM_CONST (4, uint8, weightscale4x4_inter_default[16]) = 
{
	10,14,20,24,
	14,20,24,27,
	20,24,27,30,
	24,27,30,34
};

DECLARE_ASM_CONST (4, uint8, weightscale8x8_intra_default[64]) = 
{
	 6,10,13,16,18,23,25,27,
	10,11,16,18,23,25,27,29,
	13,16,18,23,25,27,29,31,
	16,18,23,25,27,29,31,33,
	18,23,25,27,29,31,33,36,
	23,25,27,29,31,33,36,38,
	25,27,29,31,33,36,38,40,
	27,29,31,33,36,38,40,42
};

DECLARE_ASM_CONST (4, uint8, weightscale8x8_inter_default[64]) = 
{
	 9,13,15,17,19,21,22,24,
	13,13,17,19,21,22,24,25,
	15,17,19,21,22,24,25,27,
	17,19,21,22,24,25,27,28,
	19,21,22,24,25,27,28,30,
	21,22,24,25,27,28,30,32,
	22,24,25,27,28,30,32,33,
	24,25,27,28,30,32,33,35
};

/**---------------------------------------------------------------------------*
**                         Compiler Flag                                      *
**---------------------------------------------------------------------------*/
#ifdef   __cplusplus
    }
#endif
/**---------------------------------------------------------------------------*/
// End 
