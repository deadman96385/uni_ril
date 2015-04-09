/*
 * THIS IS AN UNPUBLISHED WORK CONTAINING D2 TECHNOLOGIES, INC. CONFIDENTIAL
 * AND PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
 * APPLIES: "COPYRIGHT 2005-2009 D2 TECHNOLOGIES, INC. ALL RIGHTS RESERVED"
 *
 * $D2Tech$ $Rev: 11231 $ $Date: 2010-01-21 16:03:10 -0800 (Thu, 21 Jan 2010) $
 */

#ifndef _VHW_H_
#define _VHW_H_

#include <osal_types.h>
#include <osal.h>

/*
 * This file describes interfaces to the low level hardware of the target
 * processor.
 */

/*
 * Define the channel buffer size in samples per processing period
 */
#define _VHW_SAMPLE_RATE         (16000)

#define _VHW_FRAMES_PER_SEC      (100)

/*
 * Number of samples in a PCM buffer.
 */
#define _VHW_PCM_BUF_SZ          (_VHW_SAMPLE_RATE / _VHW_FRAMES_PER_SEC)

/*
 * The physical size of a PCM buffer in octets.
 */
#define _VHW_PCM_BUFFER_SIZE     (_VHW_PCM_BUF_SZ * sizeof(int))

/*
 * Number of PCM channels supported.
 */ 
#define _VHW_NUM_PCM_CH          (1)

/*
 * Set physical interfaces audio characteristics
 */
#define VHW_AUDIO_SHIFT          (2)

#undef  VHW_AUDIO_MULAW

/*
 * This macro returns start address of buffer for a channel 'ch' given data_ptr 
 * points to the start address of buffer for ch0.
 * Returns NULL if buffer is not available.
 */
#define VHW_GETBUF(data_ptr, ch)      \
        (ch > (_VHW_NUM_PCM_CH - 1)) ? \
         NULL : (((vint *)data_ptr) + (_VHW_PCM_BUF_SZ * ch))

/* 
 * Function prototypes
 */
#ifdef __cplusplus
extern "C" {
#endif
int VHW_init(
    void);

void VHW_start(
    void);

void VHW_shutdown(
    void);

void VHW_exchange(
    int **tx_ptr,
    int **rx_ptr);

void _VHW_attach(
    void);

void _VHW_detach(
    void);

int VHW_isSleeping(
    void);

#ifdef __cplusplus
}
#endif

#endif
