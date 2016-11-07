/*
 * THIS IS AN UNPUBLISHED WORK CONTAINING D2 TECHNOLOGIES, INC. CONFIDENTIAL AND
 * PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
 * APPLIES: "COPYRIGHT 2005-2010 D2 TECHNOLOGIES, INC. ALL RIGHTS RESERVED"
 *
 * $D2Tech$ $Rev: 12730 $ $Date: 2010-08-09 20:55:01 -0400 (Mon, 09 Aug 2010) $
 *
 */

#include "_ve_private.h"

/*
 * ======== _VE_rtpInit() ========
 *
 */
vint _VE_rtpInit(
    _VE_RtpObject *rtp_ptr)
{
    void *sendRtpObjContext_ptr;
    void *recvRtpObjContext_ptr;

    /*
     * Clear all values in object. Initialize the RTP objects.
     */
    OSAL_memSet(rtp_ptr, 0, sizeof(_VE_RtpObject));

    /*
     * Allocate RTP context
     */
    sendRtpObjContext_ptr = OSAL_memAlloc(RTP_getContextSize(), 0);
    recvRtpObjContext_ptr = OSAL_memAlloc(RTP_getContextSize(), 0);
    rtp_ptr->sendRtpObj.context_ptr = sendRtpObjContext_ptr;
    rtp_ptr->recvRtpObj.context_ptr = recvRtpObjContext_ptr;
    rtp_ptr->sendRtpObj.tStamp    = 0;
    rtp_ptr->sendRtpObj.seqRandom = 0;
    rtp_ptr->recvRtpObj.tStamp    = 0;
    rtp_ptr->recvRtpObj.seqRandom = 0;

    RTP_init(&(rtp_ptr->sendRtpObj));
    RTP_init(&(rtp_ptr->recvRtpObj));
    RTP_redunInit(&(rtp_ptr->sendRtpObj));
    RTP_redunInit(&(rtp_ptr->recvRtpObj));

    rtp_ptr->inUse      = _VE_RTP_NOT_BOUND;
    rtp_ptr->sendActive = _VE_RTP_NOTREADY;
    rtp_ptr->recvActive = _VE_RTP_NOTREADY;
    rtp_ptr->tos        = _VE_RTP_IP_TOS;
    rtp_ptr->tsRandom   = 1;
    rtp_ptr->seqRandom  = 1;

    return (_VE_RTP_OK);
}