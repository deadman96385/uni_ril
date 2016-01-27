/*
 *---------------------------------------------------------------------------*
 *                                                                           *
 *          COPYRIGHT 2003-2011 SAMSUNG ELECTRONICS CO., LTD.                *
 *                          ALL RIGHTS RESERVED                              *
 *                                                                           *
 *   Permission is hereby granted to licensees of Samsung Electronics        *
 *   Co., Ltd. products to use or abstract this computer program only in     *
 *   accordance with the terms of the NAND FLASH MEMORY SOFTWARE LICENSE     *
 *   AGREEMENT for the sole purpose of implementing a product based on       *
 *   Samsung Electronics Co., Ltd. products. No other rights to reproduce,   *
 *   use, or disseminate this computer program, whether in part or in        *
 *   whole, are granted.                                                     *
 *                                                                           *
 *   Samsung Electronics Co., Ltd. makes no representation or warranties     *
 *   with respect to the performance of this computer program, and           *
 *   specifically disclaims any responsibility for any damages,              *
 *   special or consequential, connected with the use of this program.       *
 *                                                                           *
 *---------------------------------------------------------------------------*
*/
/**
 *  @version 	LinuStoreIII_1.2.0_b038-FSR_1.2.1p1_b139_RTM
 *  @file	include/linux/fsr_if.h
 *  @brief	FSR interface to export commands and macros to utils, fat
 *
 *
 */

#ifndef _FSR_IF_H_
#define _FSR_IF_H_

#define LINUSTOREIII_VERSION_STRING      "LinuStoreIII_1.2.0_b038"

#ifndef __KERNEL__
/*Warning*/
/*If you modify BML, you must check this definition*/
/*****************************************************************************/
/* Partition Entry ID of BML_LoadPIEntry()                                   */
/* Partition Entry ID from 0 to 0x0FFFFFFF is reserved in BML                */
/* Following ID is the pre-defined value and User can use Partition Entry ID */
/* from PARTITION_USER_DEF_BASE                                              */
/*****************************************************************************/

#define     PARTITION_ID_NBL			0		///< NAND bootloader stage 1, 2
#define     PARTITION_ID_BOOTLOADER		1		///< NAND bootloader stage 3
#define     PARTITION_ID_BOOT_PARAMETER		2		///< NAND bootloader parameter of stage 3
#define     PARTITION_ID_COPIEDOS		3		///< OS image copied from NAND flash memory to RAM
#define     PARTITION_ID_ROOT_FILESYSTEM	4		///< OS image loaded on demand
#define     PARTITION_ID_BMLAREA5		5		///< BML area 5
#define     PARTITION_ID_BMLAREA6		6		///< .........6
#define     PARTITION_ID_BMLAREA7		7		///< .........7
#define     PARTITION_ID_BMLAREA8		8		///<  ........8
#define     PARTITION_ID_BMLAREA9		9		///< ........ 9
#define     PARTITION_ID_BMLAREA10		10		///< ........10
#define     PARTITION_ID_BMLAREA11		11		///< ........11
#define     PARTITION_ID_BMLAREA12		12		///< ........12
#define     PARTITION_ID_BMLAREA13		13		///< ........13
#define     PARTITION_ID_BMLAREA14		14		///< ........14
#define     PARTITION_ID_BMLAREA15		15		///< ........15
#define     PARTITION_ID_BMLAREA16		16		///< ........16
#define     PARTITION_ID_BMLAREA17		17		///< ........17
#define     PARTITION_ID_BMLAREA18		18		///< ........18
#define     PARTITION_ID_BMLAREA19		19		///< BML area 19
#define     PARTITION_ID_FILESYSTEM0		20		///< file system 0
#define     PARTITION_ID_FILESYSTEM1		21		///< ........... 1
#define     PARTITION_ID_FILESYSTEM2		22		///< ........... 2
#define     PARTITION_ID_FILESYSTEM3		23		///< ........... 3
#define     PARTITION_ID_FILESYSTEM4		24		///< ............4
#define     PARTITION_ID_FILESYSTEM5		25		///< ........... 5
#define     PARTITION_ID_FILESYSTEM6		26		///< ........... 6
#define     PARTITION_ID_FILESYSTEM7		27		///< file system 7
#define     PARTITION_ID_FILESYSTEM8		28		///< BML area 18
#define     PARTITION_ID_FILESYSTEM9		29		///< BML area 19
#define     PARTITION_ID_FILESYSTEM10		30		///< file system 0
#define     PARTITION_ID_FILESYSTEM11		31		///< ........... 1
#define     PARTITION_ID_FILESYSTEM12		32		///< ........... 2
#define     PARTITION_ID_FILESYSTEM13		33		///< ........... 3
#define     PARTITION_ID_FILESYSTEM14		34		///< ........... 4
#define     PARTITION_ID_FILESYSTEM15		35		///< file system 5

#define	    MAX_STL_PARTITIONS			(PARTITION_ID_FILESYSTEM7 - PARTITION_ID_FILESYSTEM0 + 1)
#endif

/* Device major number*/
#define BLK_DEVICE_BML				137		///< BML block device major number
#define BLK_DEVICE_STL				138		///< STL block device major number
#define BLK_DEVICE_TINY_FSR			139		///< Tiny FSR block device major number

/* BML level ioctl commands */
#define BML_SET_PART_TAB			0x8A23		///< set partition information table
#define BML_FORMAT				0x8A24		///< execute BML level format
#define BML_ERASE				0x8A25		///< BML erase partition or chip
#define BML_ERASE_PARTITION			0x8A26
#define BML_DUMP				0x8A27		///< BML dump partition or chip
#define BML_RESTORE				0x8A28		///< BML restore partition or chip
#define BML_UNLOCK_ALL				0x8A29		///< unlock all partition RO -> RW
#define BML_SET_RW_AREA				0x8A2A
#define BML_FSR_DUMP				0x8A2B		///< flexia image dump
#define BML_GET_MINOR_NUMBER			0x8A2D		///< get BML device major number
#define BML_GET_MAJOR_NUMBER			0x8A2E		///< get BML device minor number
#define BML_GET_PART_ATTR			0x8A2F		///< get BML partition attribute
#define BML_SET_PART_ATTR			0x8A30		///< set BML partition attribute

/* LLD level ioctl command */
#define LLD_GET_DEV_INFO			0x8A31		///< get lld device information
#define LLD_BOOT_READ				0x8A32		///< fsr bootloader read operation
#define LLD_BOOT_WRITE				0x8A33		///< fsr bootloader write operation
#define LLD_DGS_READ				0x8A36		///< DGS head info read operation

/* OTP Operation commnad */
#define BML_OTP_READ                            0x8A41          ///< OTP Read
#define BML_OTP_WRITE                           0x8A42          ///< OTP Write
#define BML_OTP_LOCK                            0x8A43          ///< OTP Lock
#define BML_OTP_INFO                            0x8A44          ///< OTP Info

/* distinguish chip and partition during dump and restore */
#define FSR_CHIP				0xaabb		///< chip device constant
#define FSR_PART				0xaacc		///< partition device constant
#define MAGIC_STR_SIZE				8		///< string size

/* STL level ioctl commands */
#define STL_FORMAT				0x8A01		///< STL format
#define STL_GET_NUM_OF_SECTORS			0x8A02  	///< number of STL sectors in the partition
#define STL_CLEAN				0x8A03		///< STL clean
#define STL_SYNC				0x8A13		///< STL sync
#define STL_MAPDESTROY				0x8A14		///< STL mapdestroy
#define STL_RESTORE				0x8A16		///< STL layer restore
#define STL_DUMP				0x8A17		///< STL layer dump
#define STL_INIT_VARIABLES			0x8A18		///< STL variables initialize
#define STL_CHANGE_PART_ATTR			0x8A19		///< STL layer change attribute
#define STL_GET_MINOR_NUMBER			0x8A1A		///< get STL layer minor number
#define STL_GET_MAJOR_NUMBER			0x8A1B		///< get STL layer major number
#define STL_DISCARD				0x8A1C		///< discard STL mapping deleted in FileSystem
#define STL_OPEN				0x8A1D		///< STL open
#define STL_CLOSE				0x8A1E		///< STL close

/* this is support 30 partition*/
#define MASK(x)					((1U << (x)) -1)		///< partition bit mask
#define PARTITION_BITS				5				///< partition bit
#define PARTITION_MASK				MASK(PARTITION_BITS)		///< partition bit mask macro
#define MAX_FLASH_PARTITIONS			((0x1 << PARTITION_BITS) + 4)	///< calculate number of partitions
#define MAX_PAGE_SIZE 				4096				///< page size
#define MAX_STL_OPERATION_BUFFER_SIZE		(128 * 1024)			///< STL dump, restore buffer size
#define MAX_VERSION_STR_LENTH			30
#define MAX_FILE_NAME_LENTH			32  				///< Device file name max length
#define	HEADER_INCLUDED				0x31 				///< Dump image has header information
#define	MEGA_BYTE				1024 * 1024 			///< MEGA_BYTE
#define SECTOR_BITS				9				///< SECTOR SHIFT BIT
#define SCTS_PER_PAGE_SHFT			3				///< Sector per page shift bit for FSR

#define SECTOR_BASE_OFFSET			(16)
#define VALID_MARK_OFFSET			(14)
#define GARBAGE_MARK_OFFSET			(6)
#define FST_VALID_UNIT_MARK 			(0xF0)
#define SND_VALID_UNIT_MARK			(0x00)
#define VALID_UNIT_MARK				(0xFF)

/* FSR Common ioctl commands */
#define FSR_GET_PART_INFO			0x8A21 		///< get BML device information
#define FSR_GET_PART_TAB			0x8A22		///< get partition information table
#define FSR_IS_WHOLE_DEV			0x8A2C		///< whether selected partition one or whole partition
#define FSR_GET_LS_VERSION			0x8A35		///< get LinuStore version string

#ifdef CONFIG_FSR_DUAL_VOLUME
	#define FSR_MAX_VOLUMES			2		///< number of volume
#else
	#define FSR_MAX_VOLUMES			1		///< number of volume
#endif

/**
 * @brief       store buffer information between application and device driver
 * @remark      buffer size set to page size
 */
typedef struct {
	unsigned int    lock_flag;                              ///< lock flag
	unsigned int    offset;                                 ///< offset from start of partition
	unsigned char   mbuf[MAX_PAGE_SIZE];                    ///< buffer between app and device driver
} OTP_PAGEINFO_T;
/**
 * @brief	store buffer information between application and device driver
 * @remark	buffer size set to page size
 */
typedef struct {
	unsigned int	offset;					///< offset from start of partition
	unsigned char 	mbuf[MAX_PAGE_SIZE];			///< buffer between app and device driver
} PAGEINFO_T;


/**
 * @brief	LLD layer device information
 * @remark	using in fsrbootreader, fsrbootwriter, fsrflxdump
 */
typedef struct {
	unsigned int msize_page;				///< main page size
	unsigned int pages_blk;					///< number of pages in the block
	unsigned int msize_blk;					///< main page size in the block
} LLD_DEVINFO_T;




/**
 * @brief	BML level dump information table
 * @remark	selected partition table
 */
typedef struct {
	unsigned int    status;					///< dump status
	unsigned int    dump_size;				///< written size to file
	unsigned int    dump_parts[MAX_FLASH_PARTITIONS];	///< selected partition
	unsigned char   *dump_pBuf;				///< Buffer between app and device driver
} BML_DUMP_T;

/**
 * @brief	STL level information
 * @remark	total sectors, page size
 */
typedef struct {
	unsigned int total_sectors;				///< total sectors in the stl partition
	unsigned int page_size;					///< page size in the stl partition
}stl_info_t;

/**
 * @brief	STL level format information
 * @remark	STLFmt, nTotalSectors
 */
typedef struct {
	unsigned int nAvgECnt;					///< It's from FSRStlFmtInfo FSR_STL.h
	unsigned int nNumOfEcnt;				///< It's from FSRStlFmtInfo FSR_STL.h
	unsigned int *pnECnt;					///< It's from FSRStlFmtInfo FSR_STL.h
	unsigned int nTotalSectors;				///< partition's total number of sector
	unsigned int nNumOfInitFreeUnits;			///< The number of initial free units
}STL_FORMAT_INFO_T;

/**
 * @brief	BML level partition information table
 * @remark	fsr contains MLC and SLC partition
 */
typedef struct {
	int    num_parts;					///< number of partitions
	int    part_size[MAX_FLASH_PARTITIONS];			///< size of each partition
	int    part_id[MAX_FLASH_PARTITIONS];			///< ID of each partition
	int    part_attr[MAX_FLASH_PARTITIONS]; 		///< attribute of each partition
	int    part_addr[MAX_FLASH_PARTITIONS]; 		///< address of each partition
} BML_PARTTAB_T;
/**
 * @brief	STL level information
 * @remark	total sectors, page size
 */
typedef struct {
	unsigned char	driver_version[MAX_VERSION_STR_LENTH];	///< driver version string
}VERSION_CHECK_T;

/**
 * @brief	BML level device information table
 * @remark	This structure contains size of variables
 */
typedef struct {
	unsigned int	phy_unit_size;  			///< in bytes expect spare
	unsigned int	num_units;				///< number of units
	unsigned int	page_msize;				///< main area page size
	unsigned int 	page_ssize;				///< spare area page size
	unsigned int 	dump_size; 				///< dump size
} BML_DEVINFO_T;

#endif /* _FSR_IF_H_ */
