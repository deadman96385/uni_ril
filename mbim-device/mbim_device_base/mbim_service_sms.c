#define LOG_TAG "MBIM-Device"

#include <stdlib.h>
#include <string.h>
#include <utils/Log.h>

#include "mbim_message_processer.h"
#include "mbim_service_sms.h"
#include "mbim_device_vendor.h"
#include "mbim_device_sms.h"

IndicateStatusInfo s_sms_indicate_status_info[MBIM_CID_SMS_LAST] = {
        {MBIM_CID_SMS_CONFIGURATION, sms_configuration_notify},
        {MBIM_CID_SMS_READ, sms_read_notify},
        {MBIM_CID_SMS_SEND, NULL},
        {MBIM_CID_SMS_DELETE, NULL},
        {MBIM_CID_SMS_MESSAGE_STORE_STATUS, sms_message_store_status_notify},
};

/******************************************************************************/
void
mbim_message_parser_sms(MbimMessageInfo *   messageInfo) {
    int ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
    char printBuf[PRINTBUF_SIZE] = {0};
    mbim_message_info_get_printtable(printBuf, messageInfo, LOG_TYPE_COMMAND);

    switch (messageInfo->cid) {
        case MBIM_CID_SMS_CONFIGURATION:
            ret = mbim_message_parser_sms_configuration(messageInfo, printBuf);
            break;
        case MBIM_CID_SMS_READ:
            ret = mbim_message_parser_sms_read(messageInfo, printBuf);
            break;
        case MBIM_CID_SMS_SEND:
            ret = mbim_message_parser_sms_send(messageInfo, printBuf);
            break;
        case MBIM_CID_SMS_DELETE:
            ret = mbim_message_parser_sms_delete(messageInfo, printBuf);
            break;
        case MBIM_CID_SMS_MESSAGE_STORE_STATUS:
            ret = mbim_message_parser_sms_message_store_status(messageInfo, printBuf);
            break;
        default:
            RLOGE("Unsupported sms cid");
            break;
    }

    if (ret == MBIM_ERROR_INVALID_COMMAND_TYPE) {
        RLOGE("unsupported sms cid or command type");
        mbim_device_command_done(messageInfo,
                MBIM_STATUS_ERROR_NO_DEVICE_SUPPORT, NULL, 0);
    }
}

/******************************************************************************/

/**
 * sms_configuration: query, set and notification supported
 *
 * query:
 *  @command:   the informationBuffer shall be null
 *  @response:  the informationBuffer shall be MbimSmsConfigurationInfo
 *
 * set:
 *  @command:   the informationBuffer shall be MbimSmsSetConfiguration
 *  @response:  the informationBuffer shall be MbimSmsConfigurationInfo
 *
 * notification:
 *  @command:   NA
 *  @response:  the informationBuffer shall be MbimSmsConfigurationInfo
 */

int
mbim_message_parser_sms_configuration(MbimMessageInfo * messageInfo,
                                      char *            printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            sms_configuration_query_operation(messageInfo, printBuf);
            break;
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            sms_configuration_set_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
sms_configuration_query_operation(MbimMessageInfo * messageInfo,
                                  char *            printBuf) {
    startCommand(printBuf);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = sms_configuration_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, NULL, 0);
}

void
sms_configuration_set_operation(MbimMessageInfo *   messageInfo,
                                char *              printBuf) {
    int32_t bufferLen = 0;
    MbimSmsSetConfiguration_2 *smsSetConfiguration =
            (MbimSmsSetConfiguration_2 *)calloc(1, sizeof(MbimSmsSetConfiguration_2));
    MbimSmsSetConfiguration *data = (MbimSmsSetConfiguration *)
            mbim_message_command_get_raw_information_buffer(
                    messageInfo->message, &bufferLen);

    smsSetConfiguration->format = data->format;

    uint32_t scAddressOffset = G_STRUCT_OFFSET(MbimSmsSetConfiguration, scAddressOffset);
    smsSetConfiguration->scAddress = mbim_message_read_string(
            messageInfo->message, 0, scAddressOffset);

    startCommand(printBuf);
    appendPrintBuf(printBuf, "%sformat: %s, scAddress: %s", printBuf,
            mbim_sms_format_get_printable(smsSetConfiguration->format),
            smsSetConfiguration->scAddress);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = sms_configuration_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, smsSetConfiguration, sizeof(MbimSmsSetConfiguration_2));

    memsetAndFreeStrings(1, smsSetConfiguration->scAddress);
    free(smsSetConfiguration);
}

int
sms_configuration_set_or_query_ready(Mbim_Token         token,
                                     MbimStatusError    error,
                                     void *             response,
                                     size_t             responseLen,
                                     char *             printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimSmsConfigurationInfo_2)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimSmsConfigurationInfo_2));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    startResponse(printBuf);

    uint32_t informationBufferSize = 0;
    MbimSmsConfigurationInfo *smsConfigurationInfo =
            sms_configuration_response_builder(response, responseLen,
                    &informationBufferSize, printBuf);

    closeResponse(printBuf);
    printResponse(printBuf);

    mbim_command_complete(token, error, smsConfigurationInfo, informationBufferSize);

    free(smsConfigurationInfo);

    return 0;
}

int
sms_configuration_notify(void *      data,
                         size_t      dataLen,
                         void **     response,
                         size_t *    responseLen,
                         char *      printBuf) {
    if (data == NULL || dataLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (dataLen != sizeof(MbimSmsConfigurationInfo_2)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)dataLen, (int)sizeof(MbimSmsConfigurationInfo_2));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    startResponse(printBuf);

    uint32_t informationBufferSize = 0;
    MbimSmsConfigurationInfo *smsConfigurationInfo =
            sms_configuration_response_builder(data, dataLen,
                    &informationBufferSize, printBuf);

    closeResponse(printBuf);
    printResponse(printBuf);

    *response = (void *)smsConfigurationInfo;
    *responseLen = informationBufferSize;

    return 0;
}

MbimSmsConfigurationInfo *
sms_configuration_response_builder(void *         response,
                                   size_t         responseLen,
                                   uint32_t *     outSize,
                                   char *         printBuf) {
    MBIM_UNUSED_PARM(responseLen);

    MbimSmsConfigurationInfo_2 *resp = (MbimSmsConfigurationInfo_2 *)response;

    appendPrintBuf(printBuf, "%ssmsStorageState: %s, format: %s, maxMessages: %d,"
            " cdmaShortMessageSize: %d, scAddress: %s",
            printBuf,
            mbim_sms_storage_state_get_printable(resp->smsStorageState),
            mbim_sms_format_get_printable(resp->format),
            resp->maxMessages, resp->cdmaShortMessageSize, resp->scAddress);
    closeResponse(printBuf);

    /* scAddress */
    uint32_t scAddressSize = 0;
    uint32_t scAddressPadSize = 0;
    unichar2 *scAddressInUtf16 = NULL;
    if (resp->scAddress == NULL || strlen(resp->scAddress) == 0) {
        scAddressSize = 0;
    } else {
        scAddressSize = strlen(resp->scAddress) * 2;
        scAddressPadSize = PAD_SIZE(scAddressSize);
        scAddressInUtf16 = (unichar2 *)calloc
                (scAddressPadSize / sizeof(unichar2) + 1, sizeof(unichar2));
        utf8_to_utf16(scAddressInUtf16, resp->scAddress, scAddressSize / 2, scAddressSize / 2);
    }

    uint32_t informationBufferSize = sizeof(MbimSmsConfigurationInfo) + scAddressPadSize;
    MbimSmsConfigurationInfo *smsConfigurationInfo =
            (MbimSmsConfigurationInfo *)calloc(1, informationBufferSize);
    uint32_t dataBufferOffset = G_STRUCT_OFFSET(MbimSmsConfigurationInfo, dataBuffer);

    smsConfigurationInfo->smsStorageState = resp->smsStorageState;
    smsConfigurationInfo->format = resp->format;
    smsConfigurationInfo->maxMessages = resp->maxMessages;
    smsConfigurationInfo->cdmaShortMessageSize = resp->cdmaShortMessageSize;
    smsConfigurationInfo->scAddressOffset = (scAddressSize == 0 ? 0 : dataBufferOffset);
    smsConfigurationInfo->scAddressSize = scAddressSize;

    if (smsConfigurationInfo->scAddressSize != 0) {
        memcpy((uint8_t *)smsConfigurationInfo + smsConfigurationInfo->scAddressOffset,
                (uint8_t *)scAddressInUtf16, scAddressPadSize);
    }

    memsetAndFreeUnichar2Arrays(1, scAddressInUtf16);

    *outSize = informationBufferSize;
    return smsConfigurationInfo;
}
/******************************************************************************/

/**
 * sms_read: query and notification supported
 *
 * query:
 *  @command:   the informationBuffer shall be MbimSmsReadReq
 *  @response:  the informationBuffer shall be MbimSmsReadInfo
 *
 * notification:
 *  @command:   NA
 *  @response:  the informationBuffer shall be MbimSmsReadInfo
 *
 * note:
 *  sms pdu record supported
 */

int
mbim_message_parser_sms_read(MbimMessageInfo *  messageInfo,
                             char *             printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            sms_read_query_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
sms_read_query_operation(MbimMessageInfo *  messageInfo,
                         char *             printBuf) {
    int32_t bufferLen = 0;
    MbimSmsReadReq *data = (MbimSmsReadReq *)
            mbim_message_command_get_raw_information_buffer(
                    messageInfo->message, &bufferLen);

    startCommand(printBuf);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = sms_read_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, data, sizeof(MbimSmsReadReq));
}

int
sms_read_query_ready(Mbim_Token         token,
                     MbimStatusError    error,
                     void *             response,
                     size_t             responseLen,
                     char *             printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimSmsReadInfo_2)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimSmsReadInfo_2));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimSmsReadInfo_2 *resp = (MbimSmsReadInfo_2 *)response;

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%selementCount: %d",
            printBuf, resp->elementCount);

    uint32_t informationBufferSize[resp->elementCount];
    uint32_t informationBufferSizeInTotal = 0;
    MbimSmsPduRecord **smsPduRecords = (MbimSmsPduRecord **)
            calloc(resp->elementCount, sizeof(MbimSmsPduRecord *));
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        smsPduRecords[i] = sms_read_response_builder(resp->smsPduRecords[i],
                sizeof(MbimSmsPduRecord_2), &informationBufferSize[i], printBuf);
        informationBufferSizeInTotal += informationBufferSize[i];
    }

    informationBufferSizeInTotal += sizeof(MbimSmsReadInfo) +
            sizeof(OffsetLengthPairList) * resp->elementCount;

    MbimSmsReadInfo *smsReadInfo =
            (MbimSmsReadInfo *)calloc(1, informationBufferSizeInTotal);

    uint32_t dataBufferOffset = G_STRUCT_OFFSET(MbimSmsReadInfo, smsRefList) +
            sizeof(OffsetLengthPairList) * resp->elementCount;
    smsReadInfo->elementCount = resp->elementCount;
    uint32_t offset = dataBufferOffset;
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        smsReadInfo->smsRefList[i].offset = offset;
        smsReadInfo->smsRefList[i].length = informationBufferSize[i];
        offset += informationBufferSize[i];

        memcpy((uint8_t *)smsReadInfo + smsReadInfo->smsRefList[i].offset,
                (uint8_t *)(smsPduRecords[i]), informationBufferSize[i]);
    }

    closeResponse(printBuf);
    printResponse(printBuf);

    mbim_command_complete(token, error, smsReadInfo, informationBufferSizeInTotal);
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        free(smsPduRecords[i]);
    }
    free(smsPduRecords);
    free(smsReadInfo);

    return 0;
}

int
sms_read_notify(void *      data,
                size_t      dataLen,
                void **     response,
                size_t *    responseLen,
                char *      printBuf) {
    if (data == NULL || dataLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (dataLen != sizeof(MbimSmsReadInfo_2)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)dataLen, (int)sizeof(MbimSmsReadInfo_2));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimSmsReadInfo_2 *resp = (MbimSmsReadInfo_2 *)response;

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%selementCount: %d",
            printBuf, resp->elementCount);

    uint32_t informationBufferSize[resp->elementCount];
    uint32_t informationBufferSizeInTotal = 0;
    MbimSmsPduRecord **smsPduRecords = (MbimSmsPduRecord **)
            calloc(resp->elementCount, sizeof(MbimSmsPduRecord *));
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        smsPduRecords[i] = sms_read_response_builder(resp->smsPduRecords[i],
                sizeof(MbimSmsPduRecord_2), &informationBufferSize[i], printBuf);
        informationBufferSizeInTotal += informationBufferSize[i];
    }

    informationBufferSizeInTotal += sizeof(MbimSmsReadInfo) +
            sizeof(OffsetLengthPairList) * resp->elementCount;

    MbimSmsReadInfo *smsReadInfo =
            (MbimSmsReadInfo *)calloc(1, informationBufferSizeInTotal);

    uint32_t dataBufferOffset = G_STRUCT_OFFSET(MbimSmsReadInfo, smsRefList) +
            sizeof(OffsetLengthPairList) * resp->elementCount;
    smsReadInfo->elementCount = resp->elementCount;
    uint32_t offset = dataBufferOffset;
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        smsReadInfo->smsRefList[i].offset = offset;
        smsReadInfo->smsRefList[i].length = informationBufferSize[i];
        offset += informationBufferSize[i];

        memcpy((uint8_t *)smsReadInfo + smsReadInfo->smsRefList[i].offset,
                (uint8_t *)(smsPduRecords[i]), informationBufferSize[i]);
    }

    closeResponse(printBuf);
    printResponse(printBuf);

    *response = smsReadInfo;
    *responseLen = informationBufferSizeInTotal;

    return 0;
}

MbimSmsPduRecord *
sms_read_response_builder(void *        response,
                          size_t        responseLen,
                          uint32_t *    outSize,
                          char *        printBuf) {
    MBIM_UNUSED_PARM(responseLen);

    MbimSmsPduRecord_2 *resp = (MbimSmsPduRecord_2 *)response;

    appendPrintBuf(printBuf, "%s[MessageIndex: %d, messageStatus: %s, "
            "pduDataSize: %d, pduData: %s", printBuf, resp->messageIndex,
            mbim_sms_message_status_get_printable(resp->messageStatus),
            resp->pduDataSize, resp->pduData);

    uint32_t pduDataPadSize = PAD_SIZE(resp->pduDataSize);
    uint32_t informationBufferSize = sizeof(MbimSmsPduRecord) + pduDataPadSize;

    uint32_t dataBufferOffset = G_STRUCT_OFFSET(MbimSmsPduRecord, dataBuffer);

    MbimSmsPduRecord *smsPduRecord =
            (MbimSmsPduRecord *)calloc(1, informationBufferSize);
    smsPduRecord->messageIndex = resp->messageIndex;
    smsPduRecord->messageStatus = resp->messageStatus;
    smsPduRecord->pduDataOffset = dataBufferOffset;
    smsPduRecord->pduDataSize = resp->pduDataSize;
    memcpy((uint8_t *)smsPduRecord->dataBuffer, (uint8_t *)(resp->pduData), resp->pduDataSize);

    *outSize = informationBufferSize;
    return smsPduRecord;
}

/******************************************************************************/

/**
 * sms_send: set only
 *
 * set:
 *  @command:   the informationBuffer shall be MbimSetSmsSend_2
 *  @response:  the informationBuffer shall be MbimSmsSendInfo
 *
 */

int
mbim_message_parser_sms_send(MbimMessageInfo *  messageInfo,
                             char *             printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            sms_send_set_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
sms_send_set_operation(MbimMessageInfo *    messageInfo,
                       char *               printBuf) {
    int32_t bufferLen = 0;
    MbimSetSmsSend *data = (MbimSetSmsSend *)
            mbim_message_command_get_raw_information_buffer(
                    messageInfo->message, &bufferLen);

    MbimSetSmsSend_2 *setSmsSend = (MbimSetSmsSend_2 *)calloc(1, sizeof(MbimSetSmsSend_2));
    setSmsSend->smsFormat = data->smsFormat;

    uint32_t information_buffer_offset =
            mbim_message_get_information_buffer_offset(messageInfo->message);
    uint32_t dataBufferOffset = G_STRUCT_OFFSET(MbimSetSmsSend, dataBuffer);

    MbimSmsSendPdu *smsSendPdu = (MbimSmsSendPdu *)G_STRUCT_MEMBER_P(
            messageInfo->message->data, (information_buffer_offset + dataBufferOffset));
    setSmsSend->pduDataSize = smsSendPdu->pduDataSize;

    char *pduData = (char *)calloc(smsSendPdu->pduDataSize * 2 + 1, sizeof(char));
    bytesToHexString((uint8_t *)smsSendPdu + smsSendPdu->pduDataOffset,
            smsSendPdu->pduDataSize, pduData);
    setSmsSend->pduData = pduData;

    startCommand(printBuf);
    appendPrintBuf(printBuf, "%ssmsFormat: %s, pduDataSize: %d, pduData: %s",
            printBuf, mbim_sms_format_get_printable(setSmsSend->smsFormat),
            setSmsSend->pduDataSize, setSmsSend->pduData);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = sms_send_set_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, setSmsSend, sizeof(MbimSetSmsSend_2));

    free(setSmsSend->pduData);
    free(setSmsSend);
}

int
sms_send_set_ready(Mbim_Token           token,
                   MbimStatusError      error,
                   void *               response,
                   size_t               responseLen,
                   char *               printBuf) {
    //TODO
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimSmsSendInfo)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimSmsSendInfo));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimSmsSendInfo *smsSendInfo = (MbimSmsSendInfo *)response;

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%smessageReference: %d", printBuf,
            smsSendInfo->messageReference);
    closeResponse(printBuf);
    printResponse(printBuf);

    mbim_command_complete(token, error, response, responseLen);

    return 0;
}

/******************************************************************************/
/**
 * sms_delete: set only
 *
 * set:
 *  @command:   the informationBuffer shall be MbimSetSmsDelete
 *  @response:  the informationBuffer shall be null
 *
 */

int
mbim_message_parser_sms_delete(MbimMessageInfo *    messageInfo,
                               char *               printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            sms_delete_set_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
sms_delete_set_operation(MbimMessageInfo *  messageInfo,
                         char *             printBuf) {
    int32_t bufferLen = 0;
    MbimSetSmsDelete *data = (MbimSetSmsDelete *)
            mbim_message_command_get_raw_information_buffer(
                    messageInfo->message, &bufferLen);

    startCommand(printBuf);
    appendPrintBuf(printBuf, "%sflags: %s, messageIndex: %d", printBuf,
            mbim_sms_flag_get_printable(data->flags), data->messageIndex);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = sms_delete_set_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, data, sizeof(MbimSetSmsDelete));
}

int
sms_delete_set_ready(Mbim_Token         token,
                     MbimStatusError    error,
                     void *             response,
                     size_t             responseLen,
                     char *             printBuf) {
    MBIM_UNUSED_PARM(response);
    MBIM_UNUSED_PARM(responseLen);

    startResponse(printBuf);
    closeResponse(printBuf);
    printResponse(printBuf);

    mbim_command_complete(token, error, NULL, 0);

    return 0;
}

/******************************************************************************/
/**
 * sms_message_store_status: query and notification supported
 *
 * query:
 *  @command:   the informationBuffer shall be null
 *  @response:  the informationBuffer shall be MbimSmsStatusInfo
 *
 * notification:
 *  @command:   NA
 *  @response:  the informationBuffer shall be MbimSmsStatusInfo
 */

int
mbim_message_parser_sms_message_store_status(MbimMessageInfo *  messageInfo,
                                             char *             printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            sms_message_store_status_query_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
sms_message_store_status_query_operation(MbimMessageInfo *  messageInfo,
                                         char *             printBuf) {
    startCommand(printBuf);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = sms_message_store_status_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, NULL, 0);
}

int
sms_message_store_status_query_ready(Mbim_Token         token,
                                     MbimStatusError    error,
                                     void *             response,
                                     size_t             responseLen,
                                     char *             printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimSmsStatusInfo)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimSmsStatusInfo));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimSmsStatusInfo *smsStatusInfo = (MbimSmsStatusInfo *)response;

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%sflags: %s, messageIndex: %d", printBuf,
            mbim_sms_status_flag_get_printable(smsStatusInfo->flag),
            smsStatusInfo->messageIndex);
    closeResponse(printBuf);
    printResponse(printBuf);

    mbim_command_complete(token, error, response, responseLen);

    return 0;
}

int
sms_message_store_status_notify(void *      data,
                                size_t      dataLen,
                                void **     response,
                                size_t *    responseLen,
                                char *      printBuf) {
    if (data == NULL || dataLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (dataLen != sizeof(MbimSmsStatusInfo)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)dataLen, (int)sizeof(MbimSmsStatusInfo));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimSmsStatusInfo *resp = (MbimSmsStatusInfo *)response;

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%sflags: %s, messageIndex: %d", printBuf,
            mbim_sms_status_flag_get_printable(resp->flag),
            resp->messageIndex);
    closeResponse(printBuf);
    printResponse(printBuf);

    MbimSmsStatusInfo *smsStatusInfo =
            (MbimSmsStatusInfo *)calloc(1, sizeof(MbimSmsStatusInfo));
    memcpy(smsStatusInfo, resp, sizeof(MbimSmsStatusInfo));

    *response = smsStatusInfo;
    *responseLen = sizeof(MbimSmsStatusInfo);

    return 0;
}
/******************************************************************************/

