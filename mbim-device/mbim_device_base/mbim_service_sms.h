
#ifndef MBIM_SERVICE_SMS_H_
#define MBIM_SERVICE_SMS_H_

#include "mbim_message.h"
#include "mbim_enums.h"
#include "mbim_cid.h"

/*****************************************************************************/
/* 'sms' struct */

/**
 * MBIM_CID_SMS_CONFIGURATION:
 *                      Set                         Query                Notification
 * Command    MBIM_SET_SMS_CONFIGURATION            Empty                     NA
 * Response   MBIM_SMS_CONFIGURATION_IFNO  MBIM_SMS_CONFIGURATION_IFNO  MBIM_SMS_CONFIGURATION_IFNO
 *
 */
struct _MbimSmsSetConfiguration {
    MbimSmsFormat           format;
    uint32_t                scAddressOffset;    /* scAddress represents the service center (SC) address */
    uint32_t                scAddressSize;
    uint8_t                 dataBuffer[];       /* scAddress */
} __attribute__((packed));
typedef struct _MbimSmsSetConfiguration MbimSmsSetConfiguration;

struct _MbimSmsConfigurationInfo {
    MbimSmsStorageState     smsStorageState;
    MbimSmsFormat           format;
    uint32_t                maxMessages;
    uint32_t                cdmaShortMessageSize;
    uint32_t                scAddressOffset;
    uint32_t                scAddressSize;
    uint8_t                 dataBuffer[];       /* scAddress */
} __attribute__((packed));
typedef struct _MbimSmsConfigurationInfo MbimSmsConfigurationInfo;

/**
 * MBIM_CID_SMS_READ:
 *            Set        Query           Notification
 * Command    NA   MBIM_SMS_READ_REQ          NA
 * Response   NA   MBIM_SMS_READ_INFO  MBIM_SMS_READ_INFO
 *
 */
struct _MbimSmsPduRecord {
    uint32_t                messageIndex;
    MbimSmsMessageStatus    messageStatus;
    uint32_t                pduDataOffset;
    uint32_t                pduDataSize;
    uint8_t                 dataBuffer[];       /* pduData */
} __attribute__((packed));
typedef struct _MbimSmsPduRecord MbimSmsPduRecord;

struct _MbimSmsCmdaRecord {
    uint32_t                messageIndex;
    MbimSmsMessageStatus    messageStatus;
    uint32_t                addressOffset;
    uint32_t                addressSize;
    uint32_t                timeStampOffset;
    uint32_t                timeStampSize;
    MbimSmsCdmaEncoding     encodingId;
    MbimSmsCdmaLang         languageId;
    uint32_t                encodedMessageOffset;
    uint32_t                sizeInBytes;
    uint32_t                sizeInCharacters;
    uint8_t                 dataBuffer[];       /* address, timeStamp, EncodedMessage */
} __attribute__((packed));
typedef struct _MbimSmsCmdaRecord MbimSmsCmdaRecord;

struct _MbimSmsReadReq {
    MbimSmsFormat           smsFormat;
    MbimSmsFlag             flag;
    uint32_t                MessageIndex;
} __attribute__((packed));
typedef struct _MbimSmsReadReq MbimSmsReadReq;

struct _MbimSmsReadInfo {
    MbimSmsFormat           format;
    uint32_t                elementCount;
    OffsetLengthPairList    smsRefList[];
//    uint8_t                 dataBuffer[];       /* MbimSmsPduRecord or MbimSmsCmdaRecord */
} __attribute__((packed));
typedef struct _MbimSmsReadInfo MbimSmsReadInfo;


/**
 * MBIM_CID_SMS_SEND:
 *                   Set         Query    Notification
 * Command    MBIM_SET_SMS_SEND    NA          NA
 * Response   MBIM_SMS_SEND_INFO   NA          NA
 *
 */
struct _MbimSmsSendPdu {
    uint32_t                pduDataOffset;
    uint32_t                pduDataSize;
    uint8_t                 dataBuffer[];       /* pduData*/
} __attribute__((packed));
typedef struct _MbimSmsSendPdu MbimSmsSendPdu;

struct _MbimSmsSendCdma {
    MbimSmsCdmaEncoding     encodingId;
    MbimSmsCdmaLang         languageId;
    uint32_t                addressOffset;
    uint32_t                addressSize;
    uint32_t                encodedMessageOffset;
    uint32_t                sizeInBytes;
    uint32_t                sizeInCharacters;
    uint8_t                 dataBuffer[];       /* address, encodedMessage */
} __attribute__((packed));
typedef struct _MbimSmsSendCdma MbimSmsSendCdma;

struct _MbimSetSmsSend {
    MbimSmsFormat           smsFormat;
    uint8_t                 dataBuffer[];       /* depending on the value pf smsFormat,
                                                   this field is either MbimSmsSendPdu or
                                                   MbimSmsSendCdma */
} __attribute__((packed));
typedef struct _MbimSetSmsSend MbimSetSmsSend;

struct _MbimSmsSendInfo {
    uint32_t                messageReference;
} __attribute__((packed));
typedef struct _MbimSmsSendInfo MbimSmsSendInfo;


/**
 * MBIM_CID_SMS_DELETE:
 *                   Set            Query    Notification
 * Command    MBIM_SET_SMS_DELETE    NA          NA
 * Response         Empty            NA          NA
 *
 */
struct _MbimSetSmsDelete {
    MbimSmsFlag             flags;
    uint32_t                messageIndex;
} __attribute__((packed));
typedef struct _MbimSetSmsDelete MbimSetSmsDelete;


/**
 * MBIM_CID_SMS_MESSAGE_STORE_STATUS:
 *            Set        Query               Notification
 * Command    NA         Empty                    NA
 * Response   NA   MBIM_SMS_STATUS_INFO   MBIM_SMS_STATUS_INFO
 *
 */
struct _MbimSmsStatusInfo {
    MbimSmsStatusFlag       flag;
    uint32_t                messageIndex;
} __attribute__((packed));
typedef struct _MbimSmsStatusInfo MbimSmsStatusInfo;

/*****************************************************************************/

void
mbim_message_parser_sms(MbimMessageInfo *   messageInfo);

extern IndicateStatusInfo s_sms_indicate_status_info[MBIM_CID_SMS_LAST];

/*****************************************************************************/
/**
 * sms service's packing and unpacking interfaces
 */

/* sms_configuration */
int
mbim_message_parser_sms_configuration   (MbimMessageInfo *      messageInfo,
                                         char *                 printBuf);
void
sms_configuration_query_operation       (MbimMessageInfo *      messageInfo,
                                         char *                 printBuf);
void
sms_configuration_set_operation         (MbimMessageInfo *      messageInfo,
                                         char *                 printBuf);
int
sms_configuration_set_or_query_ready    (Mbim_Token             token,
                                         MbimStatusError        error,
                                         void *                 response,
                                         size_t                 responseLen,
                                         char *                 printBuf);
int
sms_configuration_notify                (void *                 data,
                                         size_t                 dataLen,
                                         void **                response,
                                         size_t *               responseLen,
                                         char *                 printBuf);
MbimSmsConfigurationInfo *
sms_configuration_response_builder      (void *                 response,
                                         size_t                 responseLen,
                                         uint32_t *             outSize,
                                         char *                 printBuf);

/* sms_read */
int
mbim_message_parser_sms_read        (MbimMessageInfo *          messageInfo,
                                     char *                     printBuf);
void
sms_read_query_operation            (MbimMessageInfo *          messageInfo,
                                     char *                     printBuf);
int
sms_read_query_ready                (Mbim_Token                 token,
                                     MbimStatusError            error,
                                     void *                     response,
                                     size_t                     responseLen,
                                     char *                     printBuf);
MbimSmsPduRecord *
sms_read_response_builder           (void *                     response,
                                     size_t                     responseLen,
                                     uint32_t *                 outSize,
                                     char *                     printBuf);
int
sms_read_notify                     (void *                     data,
                                     size_t                     dataLen,
                                     void **                    response,
                                     size_t *                   responseLen,
                                     char *                     printBuf);

/* sms_send */
int
mbim_message_parser_sms_send        (MbimMessageInfo *          messageInfo,
                                     char *                     printBuf);
void
sms_send_set_operation              (MbimMessageInfo *          messageInfo,
                                     char *                     printBuf);
int
sms_send_set_ready                  (Mbim_Token                 token,
                                     MbimStatusError            error,
                                     void *                     response,
                                     size_t                     responseLen,
                                     char *                     printBuf);

/* sms_delete */
int
mbim_message_parser_sms_delete      (MbimMessageInfo *          messageInfo,
                                     char *                     printBuf);
void
sms_delete_set_operation            (MbimMessageInfo *          messageInfo,
                                     char *                     printBuf);
int
sms_delete_set_ready                (Mbim_Token                 token,
                                     MbimStatusError            error,
                                     void *                     response,
                                     size_t                     responseLen,
                                     char *                     printBuf);

/* sms_message_store_status */
int
mbim_message_parser_sms_message_store_status(MbimMessageInfo *  messageInfo,
                                             char *             printBuf);
void
sms_message_store_status_query_operation    (MbimMessageInfo *  messageInfo,
                                             char *             printBuf) ;
int
sms_message_store_status_query_ready        (Mbim_Token         token,
                                             MbimStatusError    error,
                                             void *             response,
                                             size_t             responseLen,
                                             char *             printBuf);
int
sms_message_store_status_notify             (void *             data,
                                             size_t             dataLen,
                                             void **            response,
                                             size_t *           responseLen,
                                             char *             printBuf);

/*****************************************************************************/

#endif  // MBIM_SERVICE_SMS_H_
