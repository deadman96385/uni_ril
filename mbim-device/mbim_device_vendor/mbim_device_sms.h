
#ifndef MBIM_DEVICE_SMS_H_
#define MBIM_DEVICE_SMS_H_

#include "mbim_enums.h"
#include "mbim_message.h"
/**
 * MBIM_CID_SMS_CONFIGURATION
 */
typedef struct {
    MbimSmsFormat           format;
    char *                  scAddress;  /* scAddress represents the service center (SC) address */
} MbimSmsSetConfiguration_2;

typedef struct  {
    MbimSmsStorageState     smsStorageState;
    MbimSmsFormat           format;
    uint32_t                maxMessages;
    uint32_t                cdmaShortMessageSize;
    char *                  scAddress;
} MbimSmsConfigurationInfo_2;

/**
 * MBIM_CID_SMS_READ
 */
typedef struct  {
    uint32_t                messageIndex;
    MbimSmsMessageStatus    messageStatus;
    uint32_t                pduDataSize;
    char *                  pduData;
} MbimSmsPduRecord_2;

typedef struct {
    MbimSmsFormat           format;
    uint32_t                elementCount;
    MbimSmsPduRecord_2 **   smsPduRecords;
} MbimSmsReadInfo_2;

/**
 * MBIM_CID_SMS_SEND
 */
typedef struct {
    MbimSmsFormat           smsFormat;
    uint32_t                pduDataSize;
    char *                  pduData;
} MbimSetSmsSend_2;

typedef struct {
    int messageRef;   /* TP-Message-Reference for GSM,
                         and BearerData MessageId for CDMA
                         (See 3GPP2 C.S0015-B, v2.0, table 4.5-1). */
    char *ackPDU;     /* or NULL if n/a */
    int errorCode;    /* See 3GPP 27.005, 3.2.5 for GSM/UMTS,
                         3GPP2 N.S0005 (IS-41C) Table 171 for CDMA,
                         -1 if unknown or not applicable*/
} RIL_SMS_Response;

void
mbim_device_sms           (Mbim_Token                token,
                           uint32_t                  cid,
                           MbimMessageCommandType    commandType,
                           void *                    data,
                           size_t                    dataLen);

void
sms_read                  (Mbim_Token                token,
                           MbimMessageCommandType    commandType,
                           void *                    data,
                           size_t                    dataLen);

void
query_sms_read            (Mbim_Token                token,
                           void *                    data,
                           size_t                    dataLen);

void
sms_delete                (Mbim_Token                token,
                           MbimMessageCommandType    commandType,
                           void *                    data,
                           size_t                    dataLen);

void
set_sms_delete            (Mbim_Token                token,
                           void *                    data,
                           size_t dataLen);

void
sms_send                  (Mbim_Token                token,
                           MbimMessageCommandType    commandType,
                           void *                    data,
                           size_t                    dataLen);

void
set_sms_send              (Mbim_Token                token,
                           void *                    data,
                           size_t                    dataLen);

void
sms_message_store_status  (Mbim_Token                token,
                           MbimMessageCommandType    commandType,
                           void *                    data,
                           size_t                    dataLen);

void
query_sms_message_store_status (Mbim_Token            token,
                                void *                data,
                                size_t                dataLen);

void
sms_configuration         (Mbim_Token                token,
                           MbimMessageCommandType    commandType,
                           void *                    data,
                           size_t                    dataLen);

void
query_sms_configuration   (Mbim_Token                token,
                           void *                    data,
                           size_t                    dataLen);

void
set_sms_configuration     (Mbim_Token                token,
                           void *                    data,
                           size_t                    dataLen);

int
processSmsUnsolicited     (const char *s, const char *sms_pdu);

#endif  // MBIM_DEVICE_SMS_H_
