/*
 * THIS IS AN UNPUBLISHED WORK CONTAINING D2 TECHNOLOGIES, INC. CONFIDENTIAL AND
 * PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
 * APPLIES: "COPYRIGHT 2005-2010 D2 TECHNOLOGIES, INC. ALL RIGHTS RESERVED"
 *
 * $D2Tech$ $Rev: 12523 $ $Date: 2010-07-13 18:57:44 -0400 (Tue, 13 Jul 2010) $
 *
 */

#include "_ve_private.h"

/*
 * ======== _VE_rtcpCname() ========
 *
 * Generate a SDES-CNAME packet that is placed at the current offset in the
 * message payload buffer.
 */
vint _VE_rtcpCname(
    _VE_NetObj     *net_ptr,
    uvint             infc,
    uvint             streamId,
    _VTSP_RtcpCmdMsg *msg_ptr,
    vint              offset,
    uint32            ssrc)
{
    uint32 *header_ptr;
    uint32 *data_ptr;
    vint    loop;
    vint    length;

    /*
     * Add CNAME to the end of the compound message.
     *
     */
    header_ptr = &(msg_ptr->msg.payload[offset]);
    length = net_ptr->rtcpCname.length;
    data_ptr = net_ptr->rtcpCname.cname;

    for (loop = 0; loop < length; loop++) {
        *header_ptr++ = *data_ptr++;
    }
    /*
     * Chunk 1 starts with the ssrc.
     */
    msg_ptr->msg.payload[offset + 1] = ssrc;
    msg_ptr->payloadSize += length;
    offset += length;

    return (offset);
}

/*
 * ======== _VE_rtcpExtendedStatSummBlock() ========
 *
 * Section of Extended Report (XR) packet (RFC 3611).
 * Create SS block; Statistics Sumary Report Block (RFC 3611 Section 4.6).
 */
vint _VE_rtcpExtendedStatSummBlock(
    _VE_StreamObj  *stream_ptr,
    _VE_RtpObject *vtspRtp_ptr,
    _VTSP_RtcpCmdMsg *msg_ptr,
    vint              offset,
    uint32            ssrc)
{
    uint32               temp32;

    temp32  = _VTSP_RTCP_PTYPE_XR_BLOCK_SS << 24; /* 6 */
    temp32 |= (1 << 23); /* lost OK */
    temp32 |= (1 << 21); /* jit OK */

    temp32 |= (9);              /* block length = 9 */
    msg_ptr->msg.payload[offset++] = OSAL_netHtonl(temp32);
    msg_ptr->msg.payload[offset++] = (ssrc);
    temp32  = ((0) & 0xffff) << 16;      /* begin */
    temp32 |= ((0) & 0xffff0000) >> 16;  /* end */
    msg_ptr->msg.payload[offset++] = OSAL_netHtonl(temp32);         /* begin|end_seq */
    msg_ptr->msg.payload[offset++] = OSAL_netHtonl((0)); /* lost_packets */
    msg_ptr->msg.payload[offset++] = OSAL_netHtonl((0)); /* dup_packets */
    msg_ptr->msg.payload[offset++] = OSAL_netHtonl((0)); /* min_jitter */
    msg_ptr->msg.payload[offset++] = OSAL_netHtonl((0)); /* max_jitter */
    msg_ptr->msg.payload[offset++] = OSAL_netHtonl((0)); /* mean_jitter */
    msg_ptr->msg.payload[offset++] = OSAL_netHtonl((0)); /* dev_jitter */
    msg_ptr->msg.payload[offset++] = 0; /* min_ttl_... ... (unsupported) */


    return (offset);
}
/*
 * ======== _VE_rtcpExtendedMetricsBlock() ========
 *
 * Section of Extended Report (XR) packet (RFC 3611).
 * Create MR block; VoIP Metrics Report Block (RFC 3611 Section 4.7)
 */
vint _VE_rtcpExtendedMetricsBlock(
    _VE_StreamObj  *stream_ptr,
    _VE_RtpObject *rtp_ptr,
    _VTSP_RtcpCmdMsg *msg_ptr,
    vint              offset,
    uint32            ssrc)
{
    uint32               temp32;
    uvint                delay;


    temp32  = _VTSP_RTCP_PTYPE_XR_BLOCK_MR << 24;   /* BT=7 */
    temp32 |= (8);                                  /* block length */
    msg_ptr->msg.payload[offset++] = OSAL_netHtonl(temp32);

    msg_ptr->msg.payload[offset++] = (ssrc);

    temp32  = ((0) & 0xff) << 24;           /* loss rate */
    temp32 |= ((0) & 0xff) << 16;           /* discard rate */
    msg_ptr->msg.payload[offset++] = OSAL_netHtonl(temp32);

    msg_ptr->msg.payload[offset++] = 0; /* burst, gap dur: unsupported */

    /* end sys delay = jit + decode + encode + playout delays(VHW) */
    delay   = (0);
    temp32  = (delay & 0xff);                       /* round trip delay */

    temp32 |= ((0)) << 16;        /* end sys delay */
    msg_ptr->msg.payload[offset++] = OSAL_netHtonl(temp32);

    /* signal level, noise level, Gmin unsupported */
    temp32 = 127;

    msg_ptr->msg.payload[offset++] = OSAL_netHtonl(temp32);

    /* R factor, ... MOS-CQ: unsupported */
    msg_ptr->msg.payload[offset++] = 0;

    /* RX config: unsupported */
    temp32  = ((0) & 0xffff);              /* JB nominal */
    msg_ptr->msg.payload[offset++] = OSAL_netHtonl(temp32);

    temp32  = ((0) & 0xffff) << 16;        /* JB maximum */
    temp32 |= ((0) & 0xffff);              /* JB abs max */
    msg_ptr->msg.payload[offset++] = OSAL_netHtonl(temp32);

    return (offset);
}

/*
 * ======== _VE_rtcpNullReport() ========
 *
 * Generate a complete NULL report. This consists of a NULL RR packet followed
 * by an SDES-CNAME packet.
 */
void _VE_rtcpNullReport(
    _VE_NetObj     *net_ptr,
    uvint             infc,
    uvint             streamId,
    _VTSP_RtcpCmdMsg *msg_ptr)
{
    uint32            ssrc;
    uint32            temp32;
    vint              offset;
    vint              payloadType;
    _VE_RtpObject *rtp_ptr;

    rtp_ptr = _VE_streamIdToRtpPtr(net_ptr, infc, streamId);
    /*
     * A NULL report is simply an empty RR report packet followed by a CNAME
     * packet. The next section creates the NULL RR report.
     */
    ssrc = OSAL_netHtonl(rtp_ptr->sendRtpObj.pkt.rtpMinHdr.ssrc);
    payloadType = _VTSP_RTCP_PTYPE_RR;
    for (offset = 2; offset < 8; offset++) {
        msg_ptr->msg.payload[offset] = 0;
    }
    msg_ptr->payloadSize = 8;

    temp32  = 0x2 << 30; /* version number */
    temp32 |= payloadType << 16; /* packet type */
    temp32 |= msg_ptr->payloadSize - 1;
    msg_ptr->msg.payload[0] = OSAL_netHtonl(temp32);
    msg_ptr->msg.payload[1] = ssrc; /* SSRC of sender */

    /*
     * Add a CNAME packet to the compound packet.
     */
    offset = _VE_rtcpCname(net_ptr, infc, streamId, msg_ptr, offset, ssrc);

    /*
     * Update the total message size that consists of the NULL RR packet
     * followed by an SDES-CNAME packet.
     */
    msg_ptr->payloadSize *= sizeof(uint32);
}
/*
 * ======== _VE_rtcpReceiverBlock() ========
 *
 * This adds a Receiver block in the current RR or SR packet. The statistics are
 * taken from the RTP object.
 */
vint _VE_rtcpReceiverBlock(
    _VE_RtpObject  *rtp_ptr,
    _VTSP_RtcpCmdMsg  *msg_ptr,
    vint               offset)
{
    uint32 temp32;
    vint   extendedMax;
    vint   expected;
    vint   lost;
    vint   expectedInterval;
    vint   receivedInterval;
    vint   lostInterval;
    uvint  fraction;
    /*
     * Calculated the number of lost packets using the algorithm in A.3 of
     * RFC 3550.
     */
    extendedMax = rtp_ptr->cycles + rtp_ptr->maxSequence;
    expected    = extendedMax - rtp_ptr->baseSequence + 1;
    lost        = expected - rtp_ptr->received;
    if (lost > 0x7fffff) {
        lost = 0x7fffff;
    }
    else if (lost < (-0x7fffff)) {
        lost = 0x800000;
    }
    /*
     * Calculate the fraction lost using the same algorithm.
     */
    expectedInterval       = expected - rtp_ptr->expectedPrior;
    rtp_ptr->expectedPrior = expected;
    receivedInterval       = rtp_ptr->received - rtp_ptr->receivedPrior;
    rtp_ptr->receivedPrior = rtp_ptr->received;
    lostInterval = expectedInterval - receivedInterval;
    if (( 0 == expectedInterval) || (lostInterval <= 0)) {
        fraction = 0;
    }
    else {
        fraction = (lostInterval << 8) / expectedInterval;
    }
    temp32 = ((fraction & 0xff) << 24) | lost;

    /*
     * Build up the message.
     *
     * SSRC_1 of first source
     */
    /* payload[offset + 0] */
    msg_ptr->msg.payload[offset++] =
        OSAL_netHtonl(rtp_ptr->recvRtpObj.pkt.rtpMinHdr.ssrc);
    /*
     * fraction lost, cumulative lost
     */
    /* payload[offset + 1] */
    msg_ptr->msg.payload[offset++] = OSAL_netHtonl(temp32); 
    /*
     * extended hiqhest sequence number received
     */
    /* payload[offset + 2] */
    msg_ptr->msg.payload[offset++] =
        OSAL_netHtonl(rtp_ptr->recvRtpObj.pkt.rtpMinHdr.seqn);
    /*
     * interarrival jitter
     */
    /* payload[offset + 3] */
    msg_ptr->msg.payload[offset++] = OSAL_netHtonl(rtp_ptr->jitter);

    /*
     * bug 1347
     * do not check the rtp_ptr->lastSR, otherwise DSLR in rtcp packet
     * will be 0 always
     */
        msg_ptr->msg.payload[offset++] = OSAL_netHtonl(rtp_ptr->lastSR);
        temp32 = rtp_ptr->receiveTime - rtp_ptr->recvLastSR;
        temp32 = (temp32 * 67109) >> 13;
        msg_ptr->msg.payload[offset++] = OSAL_netHtonl(temp32);

    msg_ptr->payloadSize += 6;

    return (offset);
}


/*
 * ======== _VE_rtcpReceiverReport() ========
 *
 * This function creates a complete receiver report packet followed by an
 * SDES-CNAME packet.
 */
void _VE_rtcpReceiverReport(
    _VE_NetObj     *net_ptr,
    uvint             infc,
    uvint             streamId,
    _VTSP_RtcpCmdMsg *msg_ptr)
{
    vint              offset;
    uint32            temp32;
    vint              payloadType;
    uint8             rrCount;
    uint32            ssrc;
    _VE_RtpObject *rtp_ptr;

    rtp_ptr = _VE_streamIdToRtpPtr(net_ptr, infc, streamId);

    ssrc = OSAL_netHtonl(rtp_ptr->sendRtpObj.pkt.rtpMinHdr.ssrc); 
    offset = 2;
    msg_ptr->payloadSize = 2;
    rrCount = 0;
    payloadType = _VTSP_RTCP_PTYPE_RR;
    rrCount += 1;

    offset = _VE_rtcpReceiverBlock(rtp_ptr, msg_ptr, offset);

    temp32  = 0x2 << 30; /* version number */
    temp32 |= 0 << 29;   /* padding */
    temp32 |= rrCount << 24;
    temp32 |= payloadType << 16; /* packet type */
    temp32 |= msg_ptr->payloadSize - 1;
    msg_ptr->msg.payload[0] = OSAL_netHtonl(temp32);
    msg_ptr->msg.payload[1] = ssrc; /* SSRC of sender */

    /*
     * Add a CNAME packet to the compound packet.
     */
    offset = _VE_rtcpCname(net_ptr, infc, streamId, msg_ptr, offset, ssrc);

    /*
     * Update the total message size that consists of the RR packet
     * followed by an SDES-CNAME packet.
     */
    msg_ptr->payloadSize *= sizeof(uint32);
}

/*
 * ======== _VE_rtcpSenderReport() ========
 *
 * This function creates a complete sender report including both the sender and
 * receiver blocks followed by the SDES-CNAME packet.
 */
void _VE_rtcpSenderReport(
    _VE_NetObj     *net_ptr,
    uvint             infc,
    uvint             streamId,
    _VTSP_RtcpCmdMsg *msg_ptr,
    VTSP_StreamDir    dir)
{
    vint              offset;
    uint32            temp32;
    vint              payloadType;
    uint8             rrCount;
    uint32            ssrc;
    _VE_RtpObject *rtp_ptr;

    rtp_ptr = _VE_streamIdToRtpPtr(net_ptr, infc, streamId);
    ssrc = OSAL_netHtonl(rtp_ptr->sendRtpObj.pkt.rtpMinHdr.ssrc); 
    offset = 2;
    msg_ptr->payloadSize = 2;
    rrCount = 0;
    payloadType = _VTSP_RTCP_PTYPE_SR;
    msg_ptr->msg.payload[offset++] = OSAL_netHtonl(0);  /* NTP Timestamp MSW */
    msg_ptr->msg.payload[offset++] = OSAL_netHtonl(0);  /* NTP Timestamp LSW */
    msg_ptr->msg.payload[offset++] =
        OSAL_netHtonl(rtp_ptr->sendRtpObj.pkt.rtpMinHdr.ts);  /* RTP Timestamp */
    msg_ptr->msg.payload[offset++] =
        OSAL_netHtonl(rtp_ptr->sendPacketCount);  /* Sender's packet count */
    msg_ptr->msg.payload[offset++] =
        OSAL_netHtonl(rtp_ptr->sendOctetCount);  /* Sender's octet count */
    msg_ptr->payloadSize += 5;
    /*
     * If RECV is active, create a receive report (RR).
     */
    if ((VTSP_STREAM_DIR_RECVONLY == dir) || (VTSP_STREAM_DIR_SENDRECV == dir)) {
        rrCount += 1;
        offset = _VE_rtcpReceiverBlock(rtp_ptr, msg_ptr, offset);
    }

    temp32  = 0x2 << 30; /* version number */
    temp32 |= 0 << 29;   /* padding */
    temp32 |= rrCount << 24;
    temp32 |= payloadType << 16; /* packet type */
    temp32 |= msg_ptr->payloadSize - 1;
    msg_ptr->msg.payload[0] = OSAL_netHtonl(temp32);
    msg_ptr->msg.payload[1] = ssrc; /* SSRC of sender */

    /*
     * Add CNAME to the end of the compound message.
     */
    offset = _VE_rtcpCname(net_ptr, infc, streamId, msg_ptr, offset, ssrc);
    /*
     * Update the total message size that consists of the SR
     * followed by an SDES-CNAME packet.
     */
    msg_ptr->payloadSize *= sizeof(uint32);
}

/*
 * ======== _VE_rtcpExtendedReport() ========
 *
 * This function creates a complete Extended report (XR)
 * (RFC 3611 Section 2).
 */
void _VE_rtcpExtendedReport(
    _VE_StreamObj  *stream_ptr,
    _VE_RtpObject *rtp_ptr,
    _VTSP_RtcpCmdMsg *msg_ptr,
    VTSP_StreamDir    dir)
{
    vint              offset;
    uint32            temp32;
    uint32            ssrc;

    ssrc = OSAL_netHtonl(rtp_ptr->sendRtpObj.pkt.rtpMinHdr.ssrc);
    msg_ptr->msg.payload[1] = (ssrc); /* SSRC of sender */
    offset = 2;


    offset = _VE_rtcpExtendedMetricsBlock(
            stream_ptr, rtp_ptr, msg_ptr, offset, ssrc);

    offset = _VE_rtcpExtendedStatSummBlock(
            stream_ptr, rtp_ptr, msg_ptr, offset, ssrc);

    temp32  = 0x2 << 30; /* version number */
    temp32 |= 0 << 29;   /* padding and reserved */
    temp32 |= _VTSP_RTCP_PTYPE_XR << 16; /* packet type */
    temp32 |= (offset) * sizeof(uint32); /* total length */
    msg_ptr->msg.payload[0] = OSAL_netHtonl(temp32);

    /*
     * Update the total message size
     */
    msg_ptr->payloadSize = offset * sizeof(uint32);
}


/*
 * ======== _VE_rtcpBye() ========
 *
 */
vint _VE_rtcpBye(
    _VE_Queues *q_ptr,
    _VE_NetObj *net_ptr,
    uvint         infc,
    uvint         streamId)
{
    _VTSP_RtcpCmdMsg   message;
    uint32             temp32;
    uint32             ssrc;
    vint               offset;
    _VE_RtpObject  *rtp_ptr;
    _VE_RtcpObject *rtcp_ptr;

    rtp_ptr     = _VE_streamIdToRtpPtr(net_ptr, infc, streamId);
    rtcp_ptr    = _VE_streamIdToRtcpPtr(net_ptr, infc, streamId);

    /*
     * Determine whether it is appropriate to send the BYE packet. A packet may
     * only be sent if either an RTP or RTCP packet has been sent.
     */
    if ((0 == rtp_ptr->sendPacketCount) &&
            (0 == rtcp_ptr->sendPacketCount)) {
        return (_VE_RTP_OK);
    }
        
    ssrc = OSAL_netHtonl(rtp_ptr->sendRtpObj.pkt.rtpMinHdr.ssrc); 
    /*
     * Create next message.
     */
    message.command = _VTSP_RTCP_CMD_SEND;
    message.infc = infc;
    message.streamId = streamId;

    /*
     *  Start with a NULL Report.
     */
    _VE_rtcpNullReport(net_ptr, infc, streamId, &message);

    offset = message.payloadSize >> 2;

    temp32  = 0x2 << 30; /* version number */
    temp32 |= 0 << 29;   /* padding */
    temp32 |= 1 << 24;
    temp32 |= _VTSP_RTCP_PTYPE_BYE << 16; /* packet type */
    temp32 |= 1;
    message.msg.payload[offset++] = OSAL_netHtonl(temp32);
    message.msg.payload[offset++] = ssrc; /* SSRC of sender */

    message.payloadSize += 2 * sizeof(uint32);

    /*
     * Send message
     */
    if (OSAL_SUCCESS != OSAL_msgQSend(q_ptr->rtcpMsg, (char *)&message,
                sizeof(_VTSP_RtcpCmdMsg), OSAL_NO_WAIT, NULL)) {
        _VE_TRACE(__FILE__, __LINE__);
        return (_VE_RTP_ERROR);
    }

    return (_VE_RTP_OK);
}
/*
 * ======== _VE_rtcpSend() ========
 *
 * This function is used to exchange data with an RTCP stream.
 */
vint _VE_rtcpSend(
    _VE_Obj          *ve_ptr,
    _VE_Queues       *q_ptr,
    _VE_Dsp          *dsp_ptr,
    _VE_StreamObj    *stream_ptr,
    vint                infc,
    vint                streamId)
{
    _VTSP_RtcpCmdMsg   message;
    _VE_RtcpObject *rtcp_ptr;
    _VE_NetObj      *net_ptr;
    VTSP_StreamDir     dir;
    _VE_RtpObject  *rtp_ptr;


    net_ptr     = ve_ptr->net_ptr;
    rtcp_ptr    = _VE_streamIdToRtcpPtr(net_ptr, infc, streamId);

    /*
     * Determine whether it is time to send a packet.
     */
    if (++(rtcp_ptr->currentCount) >= rtcp_ptr->sendCount) {
        rtcp_ptr->currentCount = 0;
        _VE_rtcpNextInterval(rtcp_ptr);
    }
    else {
        return (_VE_RTP_OK);
    }
        
    /*
     * Create next message.
     */
    message.command  = _VTSP_RTCP_CMD_SEND;
    message.infc     = infc;
    message.streamId = streamId;

    /*
     * If SEND is active, create a sender report (SR). Note that in the case of
     * SENDRECV a sender report contains the receiver report.
     */
    dir = stream_ptr->streamParam.dir;

    if (rtcp_ptr->enableMask & VTSP_MASK_RTCP_SR) {
        if ((VTSP_STREAM_DIR_SENDONLY == dir) || (VTSP_STREAM_DIR_SENDRECV == dir)) {
            _VE_rtcpSenderReport(net_ptr, infc, streamId, &message, dir);
        }
        /*
         * If RECV is active, create a receive report (RR).
         */
        if (VTSP_STREAM_DIR_RECVONLY == dir) {
            _VE_rtcpReceiverReport(net_ptr, infc, streamId, &message);
        }

        /*
         * If neither is active, send a NULL report (RR).
         */
        if (VTSP_STREAM_DIR_INACTIVE == dir) {
            _VE_rtcpNullReport(net_ptr, infc, streamId, &message);
        }

        /*
         * Send message
         */
        rtcp_ptr->sendPacketCount += 1;
        if (OSAL_SUCCESS != OSAL_msgQSend(q_ptr->rtcpMsg, (char *)&message,
                    sizeof(_VTSP_RtcpCmdMsg), OSAL_NO_WAIT, NULL)) {
            _VE_TRACE(__FILE__, __LINE__);
            return (_VE_RTP_ERROR);
        }
    }

    if (rtcp_ptr->enableMask & VTSP_MASK_RTCP_XR) {
        /* 
         * Send Extended Report if stream is active or not
         */
        /*
         * Send Extended Report if stream is active or not
         */
        rtp_ptr    = _VE_streamIdToRtpPtr(net_ptr, infc, streamId);
        _VE_rtcpExtendedReport(stream_ptr,
                rtp_ptr, &message, dir);
        rtcp_ptr->sendPacketCount += 1;
        if (OSAL_SUCCESS != OSAL_msgQSend(q_ptr->rtcpMsg, (char *)&message,
                    sizeof(_VTSP_RtcpCmdMsg), OSAL_NO_WAIT, NULL)) {
            _VTSP_TRACE(__FILE__, __LINE__);
            return (_VE_RTP_ERROR);
        }

    }

    return (_VE_RTP_OK);
}
