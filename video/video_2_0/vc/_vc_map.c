/*
 * THIS IS AN UNPUBLISHED WORK CONTAINING D2 TECHNOLOGIES, INC. CONFIDENTIAL
 * AND PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
 * APPLIES: "COPYRIGHT 2004-2010 D2 TECHNOLOGIES, INC. ALL RIGHTS RESERVED"
 *
 * $D2Tech$ $Rev: 12522 $ $Date: 2010-07-13 18:52:44 -0400 (Tue, 13 Jul 2010) $
 *
 */

/*
 * This file contains helper functions for interface, channel, and stream
 * number mapping.  These mappings are for between software layers and between
 * hardware-software interface.
 *
 */
#include "_vc_private.h"



/*
 * ======== _VC_localToDynamicEncoder() ========
 *
 * Convert a local encoder type to a dynamic encoder type for 
 * given stream param.
 */
uvint _VC_localToDynamicEncoder(
    VTSP_StreamVideo *streamParam_ptr,
    vint         local)
{
    if (local >= VTSP_ENCODER_VIDEO_NUM) {
        /* Should never get here */
        _VTSP_TRACE(__FILE__, __LINE__);
        return (0);
    }
    return (streamParam_ptr->encodeType[local]);

}

/*
 * ======== _VC_dynamicToLocalDecoder() ========
 *
 * Convert a dynamic decoder type to a local decoder type for 
 * given stream param.
 */
uvint _VC_dynamicToLocalDecoder(
    VTSP_StreamVideo *streamParam_ptr,
    vint         dynamic)
{
    vint  i;
    vint  count;
    vint *coder_ptr;

    coder_ptr = streamParam_ptr->decodeType;
    count     = VTSP_DECODER_VIDEO_NUM;

    /*
     * Now map dynamic type to local type.
     */
    for (i = 0; i < count; i++, coder_ptr++) {
        if (*coder_ptr == dynamic) {
            return (i);
        }
    }
    return (VTSP_CODER_UNAVAIL);
}

/*
 * ======== _VC_streamIdToStreamPtr() ========
 *
 * Get stream pointer from DSP object using interface and stream id.
 *
 * RETURN: stream_ptr
 */
OSAL_INLINE _VC_StreamObj *_VC_streamIdToStreamPtr(
        _VC_Dsp *dsp_ptr,
        vint       streamId)
{
    _VC_StreamObj *stream_ptr;
    vint             index;

    index = streamId;

    stream_ptr = dsp_ptr->streamObj_ptr[index];
    
    return (stream_ptr);
}


/*
 * ======== _VC_streamIdToRtcpPtr() ========
 *
 * Get RTCP pointer from NET object using interface and stream id.
 *
 * RETURN: _VC_RtcpObject *rtcp_ptr
 */
OSAL_INLINE _VC_RtcpObject *_VC_streamIdToRtcpPtr(
        _VC_NetObj *net_ptr,
        uvint         streamId)
{
    _VC_RtcpObject *rtcp_ptr;
    vint               index;

    index = streamId;

    rtcp_ptr = &(net_ptr->rtcpObj[index]);
    
    return (rtcp_ptr);
}

/*
 * ======== _VC_streamIdToRtpPtr() ========
 *
 * Get RTP pointer from NET object using interface and stream id.
 *
 * RETURN: _VC_RtpObject *rtp_ptr
 */
OSAL_INLINE _VC_RtpObject *_VC_streamIdToRtpPtr(
        _VC_NetObj *net_ptr,
        uvint         streamId)
{
    _VC_RtpObject *rtp_ptr;
    vint              index;

    index = streamId;

    rtp_ptr = &(net_ptr->rtpObj[index]);
    
    return (rtp_ptr);
}

