/*
 * THIS IS AN UNPUBLISHED WORK CONTAINING D2 TECHNOLOGIES, INC. CONFIDENTIAL AND
 * PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
 * APPLIES: "COPYRIGHT 2005-2010 D2 TECHNOLOGIES, INC. ALL RIGHTS RESERVED"
 *
 * $D2Tech$ $Rev: 12523 $ $Date: 2010-07-14 06:57:44 +0800 (Wed, 14 Jul 2010) $
 *
 */

#include <osal.h>
#include <_vtspr_rtp.h>

/*
 * ======== _VTSPR_rtpClose() ========
 *
 * This function is used to close an RTP stream.
 */
vint _VTSPR_rtpClose(
    _VTSPR_RtpObject *rtp_ptr)
{
    /*
     * If the socket is open, close it. Then reopen it for next time use.
     */
        if (_VTSPR_netClose(rtp_ptr->socket) != _VTSPR_RTP_OK) {
            _VTSP_TRACE(__FILE__, __LINE__);
            _VTSP_TRACE("net close failed", 0);
            return (_VTSPR_RTP_ERROR);
        }

        /*
     * Mark the stream no longer in use.
     */
    rtp_ptr->inUse      = _VTSPR_RTP_NOT_BOUND;
    rtp_ptr->sendActive = _VTSPR_RTP_NOTREADY;
    rtp_ptr->recvActive = _VTSPR_RTP_NOTREADY;

    /*
     * Clear any old addresses.
     */
    OSAL_memSet(&rtp_ptr->remoteAddr, 0, sizeof(OSAL_NetAddress));
    OSAL_memSet(&rtp_ptr->localAddr, 0, sizeof(OSAL_NetAddress));
    rtp_ptr->open = 0;

    return (_VTSPR_RTP_OK);
}
