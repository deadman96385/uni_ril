
#define LOG_TAG "MBIM-Device"


#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <utils/Log.h>

#include "mbim_service_basic_connect.h"
#include "mbim_device_basic_connect.h"
#include "mbim_message_processer.h"
#include "mbim_device_vendor.h"

IndicateStatusInfo
s_basic_connect_indicate_status_info[MBIM_CID_BASIC_CONNECT_LAST] = {
        {MBIM_CID_BASIC_CONNECT_DEVICE_CAPS, NULL},
        {MBIM_CID_BASIC_CONNECT_SUBSCRIBER_READY_STATUS,
                basic_connect_subscriber_ready_status_notify},
        {MBIM_CID_BASIC_CONNECT_RADIO_STATE,
                basic_connect_radio_state_notify},
        {MBIM_CID_BASIC_CONNECT_PIN, NULL},
        {MBIM_CID_BASIC_CONNECT_PIN_LIST, NULL},
        {MBIM_CID_BASIC_CONNECT_HOME_PROVIDER, NULL},
        {MBIM_CID_BASIC_CONNECT_PREFERRED_PROVIDERS,
                basic_connect_preferred_providers_notify},
        {MBIM_CID_BASIC_CONNECT_VISIBLE_PROVIDERS, NULL},
        {MBIM_CID_BASIC_CONNECT_REGISTER_STATE,
                basic_connect_register_state_notify},
        {MBIM_CID_BASIC_CONNECT_PACKET_SERVICE,
                basic_connect_packet_service_notify},
        {MBIM_CID_BASIC_CONNECT_SIGNAL_STATE,
                basic_connect_signal_state_notify},
        {MBIM_CID_BASIC_CONNECT_CONNECT,
                basic_connect_connect_notify},
        {MBIM_CID_BASIC_CONNECT_PROVISIONED_CONTEXTS,
                basic_connect_provisioned_contexts_notify},
        {MBIM_CID_BASIC_CONNECT_SERVICE_ACTIVATION, NULL},
        {MBIM_CID_BASIC_CONNECT_IP_CONFIGURATION,
                basic_connect_ip_configuration_notify},
        {MBIM_CID_BASIC_CONNECT_DEVICE_SERVICES, NULL},
        {0, NULL},  /* 17, 18 reserved */
        {0, NULL},  /* 17, 18 reserved */
        {MBIM_CID_BASIC_CONNECT_DEVICE_SERVICE_SUBSCRIBE_LIST, NULL},
        {MBIM_CID_BASIC_CONNECT_PACKET_STATISTICS, NULL},
        {MBIM_CID_BASIC_CONNECT_NETWORK_IDLE_HINT, NULL},
        {MBIM_CID_BASIC_CONNECT_EMERGENCY_MODE,
                basic_connect_emergency_mode_notify},
        {MBIM_CID_BASIC_CONNECT_IP_PACKET_FILTERS, NULL},
        {MBIM_CID_BASIC_CONNECT_MULTICARRIER_PROVIDERS,
                basic_connect_multicarrier_providers_notify},
};

void
mbim_message_parser_basic_connect(MbimMessageInfo *messageInfo) {
    int ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
    char printBuf[PRINTBUF_SIZE] = {0};
    mbim_message_info_get_printtable(printBuf, messageInfo, LOG_TYPE_COMMAND);

    switch (messageInfo->cid) {
        case MBIM_CID_BASIC_CONNECT_DEVICE_CAPS:
            ret = mbim_message_parser_basic_connect_device_caps(
                    messageInfo, printBuf);
            break;
        case MBIM_CID_BASIC_CONNECT_SUBSCRIBER_READY_STATUS:
            ret = mbim_message_parser_basic_connect_subscriber_ready_status(
                    messageInfo, printBuf);
            break;
        case MBIM_CID_BASIC_CONNECT_RADIO_STATE:
            ret = mbim_message_parser_basic_connect_radio_state(
                    messageInfo, printBuf);
            break;
        case MBIM_CID_BASIC_CONNECT_PIN:
            ret = mbim_message_parser_basic_connect_pin(
                    messageInfo, printBuf);
            break;
        case MBIM_CID_BASIC_CONNECT_PIN_LIST:
            ret = mbim_message_parser_basic_connect_pin_list(
                    messageInfo, printBuf);
            break;
        case MBIM_CID_BASIC_CONNECT_HOME_PROVIDER:
            ret = mbim_message_parser_basic_connect_home_provider(
                    messageInfo, printBuf);
            break;
        case MBIM_CID_BASIC_CONNECT_PREFERRED_PROVIDERS:
            ret = mbim_message_parser_basic_connect_preferred_providers(
                    messageInfo, printBuf);
            break;
        case MBIM_CID_BASIC_CONNECT_VISIBLE_PROVIDERS:
            ret = mbim_message_parser_basic_connect_visible_providers(
                    messageInfo, printBuf);
            break;
        case MBIM_CID_BASIC_CONNECT_REGISTER_STATE:
            ret = mbim_message_parser_basic_connect_register_state(
                    messageInfo, printBuf);
            break;
        case MBIM_CID_BASIC_CONNECT_PACKET_SERVICE:
            ret = mbim_message_parser_basic_connect_packet_service(
                    messageInfo, printBuf);
            break;
        case MBIM_CID_BASIC_CONNECT_SIGNAL_STATE:
            ret = mbim_message_parser_basic_connect_signal_state_service(
                    messageInfo, printBuf);
            break;
        case MBIM_CID_BASIC_CONNECT_CONNECT:
            ret = mbim_message_parser_basic_connect_connect(
                    messageInfo, printBuf);
            break;
        case MBIM_CID_BASIC_CONNECT_PROVISIONED_CONTEXTS:
            ret = mbim_message_parser_basic_connect_provisioned_contexts(
                    messageInfo, printBuf);
            break;
        case MBIM_CID_BASIC_CONNECT_SERVICE_ACTIVATION:
            ret = mbim_message_parser_basic_connect_service_activiation(
                    messageInfo, printBuf);
            break;
        case MBIM_CID_BASIC_CONNECT_IP_CONFIGURATION:
            ret = mbim_message_parser_basic_connect_ip_configuration(
                    messageInfo, printBuf);
            break;
        case MBIM_CID_BASIC_CONNECT_DEVICE_SERVICES:
            ret = mbim_message_parser_basic_connect_device_services(
                    messageInfo, printBuf);
            break;
        case MBIM_CID_BASIC_CONNECT_DEVICE_SERVICE_SUBSCRIBE_LIST:
            ret = mbim_message_parser_basic_connect_device_service_subscribe_list(
                    messageInfo, printBuf);
            break;
        case MBIM_CID_BASIC_CONNECT_PACKET_STATISTICS:
            ret = mbim_message_parser_basic_connect_packet_statistics(
                    messageInfo, printBuf);
            break;
        case MBIM_CID_BASIC_CONNECT_NETWORK_IDLE_HINT:
            ret = mbim_message_parser_basic_connect_network_idle_hint(
                    messageInfo, printBuf);
            break;
        case MBIM_CID_BASIC_CONNECT_EMERGENCY_MODE:
            ret = mbim_message_parser_basic_connect_emergency_mode(
                    messageInfo, printBuf);
            break;
        case MBIM_CID_BASIC_CONNECT_IP_PACKET_FILTERS:
            ret = mbim_message_parser_basic_connect_ip_packet_filters(
                    messageInfo, printBuf);
            break;
        case MBIM_CID_BASIC_CONNECT_MULTICARRIER_PROVIDERS:
            ret = mbim_message_parser_basic_connect_multicarrier_providers(
                    messageInfo, printBuf);
            break;
        default:
            RLOGE("Unsupported basic connect cid");
            break;
    }

    if (ret == MBIM_ERROR_INVALID_COMMAND_TYPE) {
        RLOGE("unsupported basic connect cid or command type");
        mbim_device_command_done(messageInfo,
                MBIM_STATUS_ERROR_NO_DEVICE_SUPPORT, NULL, 0);
    }
}

/******************************************************************************/
/**
 * basic_connect_device_caps: query only
 *
 * query:
 *  @command:   the informationBuffer shall be null
 *  @response:  the informationBuffer shall be MbimDeviceCapsInfo
 */
int
mbim_message_parser_basic_connect_device_caps(MbimMessageInfo * messageInfo,
                                              char *            printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            basic_connect_device_caps_query_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
basic_connect_device_caps_query_operation(MbimMessageInfo * messageInfo,
                                          char *            printBuf) {
    startCommand(printBuf);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_device_caps_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, NULL, 0);
}

int
basic_connect_device_caps_query_ready(Mbim_Token            token,
                                      MbimStatusError       error,
                                      void *                response,
                                      size_t                responseLen,
                                      char *                printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimDeviceCapsInfo_2)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimDeviceCapsInfo_2));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimDeviceCapsInfo_2 *resp = (MbimDeviceCapsInfo_2 *)response;

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%sdeviceType: %s, cellularClass: %s, "
            "voiceClass: %s, simClass: %s, dataClass: %d, smsCaps: %d, "
            "controlCaps: %d, maxSessions: %d, customDataClass: %s, "
            "deviceId: %s, firmwareInfo: %s, hardwareInfo: %s",
            printBuf,
            mbim_device_type_get_printable(resp->deviceType),
            mbim_cellular_class_get_printable(resp->cellularClass),
            mbim_voice_class_get_printable(resp->voiceClass),
            mbim_sim_class_get_printable(resp->simClass),
            resp->dataClass, resp->smsCaps, resp->controlCaps,
            resp->maxSessions, resp->customDataClass, resp->deviceId,
            resp->firmwareInfo, resp->hardwareInfo);
    closeResponse(printBuf);
    printResponse(printBuf);

    /* customDataClass */
    uint32_t customDataClassSize = 0;
    uint32_t customDataClassPadSize = 0;
    unichar2 *customDataClassInUtf16 = NULL;
    if (resp->customDataClass == NULL || strlen(resp->customDataClass) == 0) {
        customDataClassSize = 0;
    } else {
        customDataClassSize = strlen(resp->customDataClass) * 2;
        customDataClassPadSize = PAD_SIZE(customDataClassSize);
        customDataClassInUtf16 = (unichar2 *)calloc
                (customDataClassPadSize / sizeof(unichar2) + 1, sizeof(unichar2));
        utf8_to_utf16(customDataClassInUtf16, resp->customDataClass,
                customDataClassSize / 2, customDataClassSize / 2);
    }

    /* deviceId */
    uint32_t deviceIdSize = 0;
    uint32_t deviceIdPadSize = 0;
    unichar2 *deviceIdInUtf16 = NULL;
    if (resp->deviceId == NULL || strlen(resp->deviceId) == 0) {
        deviceIdSize = 0;
    } else {
        deviceIdSize = strlen(resp->deviceId) * 2;
        deviceIdPadSize = PAD_SIZE(deviceIdSize);
        deviceIdInUtf16 = (unichar2 *)calloc
                (deviceIdPadSize / sizeof(unichar2) + 1, sizeof(unichar2));
        utf8_to_utf16(deviceIdInUtf16, resp->deviceId, deviceIdSize / 2, deviceIdSize / 2);
    }

    /* firmwareInfo */
    uint32_t firmwareInfoSize = 0;
    uint32_t firmwareInfoPadSize = 0;
    unichar2 *firmwareInfoInUtf16 = NULL;
    if (resp->firmwareInfo == NULL || strlen(resp->firmwareInfo) == 0) {
        firmwareInfoSize = 0;
    } else {
        firmwareInfoSize = strlen(resp->firmwareInfo) * 2;
        firmwareInfoPadSize = PAD_SIZE(firmwareInfoSize);
        firmwareInfoInUtf16 = (unichar2 *)calloc
                (firmwareInfoPadSize / sizeof(unichar2) + 1, sizeof(unichar2));
        utf8_to_utf16(firmwareInfoInUtf16, resp->firmwareInfo,
                firmwareInfoSize / 2, firmwareInfoSize / 2);
    }

    /* hardwareInfo */
    uint32_t hardwareInfoSize = 0;
    uint32_t hardwareInfoPadSize = 0;
    unichar2 *hardwareInfoInUtf16 = NULL;
    if (resp->hardwareInfo == NULL || strlen(resp->hardwareInfo) == 0) {
        hardwareInfoSize = 0;
    } else {
        hardwareInfoSize = strlen(resp->hardwareInfo) * 2;
        hardwareInfoPadSize = PAD_SIZE(hardwareInfoSize);
        hardwareInfoInUtf16 = (unichar2 *)calloc
                (hardwareInfoPadSize / sizeof(unichar2) + 1, sizeof(unichar2));
        utf8_to_utf16(hardwareInfoInUtf16, resp->hardwareInfo,
                hardwareInfoSize / 2, hardwareInfoSize / 2);
    }

    uint32_t informationBufferSize = sizeof(MbimDeviceCapsInfo) + customDataClassPadSize +
            deviceIdPadSize + firmwareInfoPadSize + hardwareInfoPadSize;
    MbimDeviceCapsInfo *deviceCapsInfo = (MbimDeviceCapsInfo *)calloc(1, informationBufferSize);
    uint32_t dataBufferOffset = G_STRUCT_OFFSET(MbimDeviceCapsInfo, dataBuffer);

    uint32_t offset = dataBufferOffset;

    deviceCapsInfo->deviceType = resp->deviceType;
    deviceCapsInfo->cellularClass = resp->cellularClass;
    deviceCapsInfo->voiceClass = resp->voiceClass;
    deviceCapsInfo->simClass = resp->simClass;
    deviceCapsInfo->dataClass = resp->dataClass;
    deviceCapsInfo->smsCaps = resp->smsCaps;
    deviceCapsInfo->controlCaps = resp->controlCaps;
    deviceCapsInfo->maxSessions = resp->maxSessions;

    deviceCapsInfo->customDataClassOffset = OFFSET(customDataClassSize, offset);
    deviceCapsInfo->customDataClassSize = customDataClassSize;
    offset += customDataClassPadSize;

    deviceCapsInfo->deviceIdOffset = (deviceIdSize == 0 ? 0 : offset);
    deviceCapsInfo->deviceIdSize = deviceIdSize;
    offset += deviceIdPadSize;

    deviceCapsInfo->firmwareInfoOffset = OFFSET(firmwareInfoSize, offset);
    deviceCapsInfo->firmwareInfoSize = firmwareInfoSize;
    offset += firmwareInfoPadSize;

    deviceCapsInfo->hardwareInfoOffset = OFFSET(hardwareInfoSize, offset);
    deviceCapsInfo->hardwareInfoSize = hardwareInfoSize;

    if (deviceCapsInfo->customDataClassSize != 0) {
        memcpy((uint8_t *)deviceCapsInfo + deviceCapsInfo->customDataClassOffset,
                (uint8_t *)customDataClassInUtf16, customDataClassPadSize);
    }
    if (deviceCapsInfo->deviceIdSize != 0) {
        memcpy((uint8_t *)deviceCapsInfo + deviceCapsInfo->deviceIdOffset,
                (uint8_t *)deviceIdInUtf16, deviceIdPadSize);
    }
    if (deviceCapsInfo->firmwareInfoSize != 0) {
        memcpy((uint8_t *)deviceCapsInfo + deviceCapsInfo->firmwareInfoOffset,
                (uint8_t *)firmwareInfoInUtf16, firmwareInfoPadSize);
    }
    if (deviceCapsInfo->hardwareInfoSize != 0) {
        memcpy((uint8_t *)deviceCapsInfo + deviceCapsInfo->hardwareInfoOffset,
                (uint8_t *)hardwareInfoInUtf16, hardwareInfoPadSize);
    }

    mbim_command_complete(token, error, deviceCapsInfo, informationBufferSize);
    memsetAndFreeUnichar2Arrays(4, customDataClassInUtf16, deviceIdInUtf16,
            firmwareInfoInUtf16, hardwareInfoInUtf16);
    free(deviceCapsInfo);

    return 0;
}

/******************************************************************************/

/**
 * basic_connect_subscriber_ready_status: query and notification supported
 *
 * query:
 *  @command:   the informationBuffer shall be null
 *  @response:  the informationBuffer shall be MbimSubscriberReadyInfo_2
 *
 * notification:
 *  @command:   NA
 *  @response:  the informationBuffer shall be MbimSubscriberReadyInfo_2
 */
int
mbim_message_parser_basic_connect_subscriber_ready_status(MbimMessageInfo * messageInfo,
                                                          char *            printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            basic_connect_subscriber_ready_status_query_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
basic_connect_subscriber_ready_status_query_operation(MbimMessageInfo * messageInfo,
                                                      char *            printBuf) {
    startCommand(printBuf);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_subscriber_ready_status_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, NULL, 0);
}

int
basic_connect_subscriber_ready_status_query_ready(Mbim_Token            token,
                                                  MbimStatusError       error,
                                                  void *                response,
                                                  size_t                responseLen,
                                                  char *                printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimSubscriberReadyInfo_2)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimSubscriberReadyInfo_2));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    startResponse(printBuf);

    uint32_t informationBufferSize = 0;

    MbimSubscriberReadyInfo *subscriberReadyInfo =
            subscriber_ready_status_response_builder(response, responseLen,
                    &informationBufferSize, printBuf);

    closeResponse(printBuf);
    printResponse(printBuf);

    mbim_command_complete(token, error, subscriberReadyInfo, informationBufferSize);

    free(subscriberReadyInfo);

    return 0;
}

int
basic_connect_subscriber_ready_status_notify(void *     data,
                                             size_t     dataLen,
                                             void **    response,
                                             size_t *   responseLen,
                                             char *     printBuf) {
    if (data == NULL || dataLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (dataLen != sizeof(MbimSubscriberReadyInfo_2)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)dataLen, (int)sizeof(MbimSubscriberReadyInfo_2));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    startResponse(printBuf);

    uint32_t informationBufferSize = 0;
    MbimSubscriberReadyInfo *subscriberReadyInfo =
            subscriber_ready_status_response_builder(data, dataLen,
                    &informationBufferSize, printBuf);

    closeResponse(printBuf);
    printResponse(printBuf);

    *response = (void *)subscriberReadyInfo;
    *responseLen = informationBufferSize;

    return 0;
}

MbimSubscriberReadyInfo *
subscriber_ready_status_response_builder(void *         response,
                                         size_t         responseLen,
                                         uint32_t *     outSize,
                                         char *         printBuf) {
    MBIM_UNUSED_PARM(responseLen);

    MbimSubscriberReadyInfo_2 *resp = (MbimSubscriberReadyInfo_2 *)response;

    appendPrintBuf(printBuf, "%sreadyState: %s, subscriberId: %s, "
            "simIccId: %s, readyInfo: %s, elementCount: %d, telephonyNumbers: ",
            printBuf,
            mbim_suscriber_ready_status_get_printable(resp->readyState),
            resp->subscriberId, resp->simIccId,
            mbim_ready_info_flag_get_printable(resp->readyInfo),
            resp->elementCount);
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        appendPrintBuf(printBuf, "%s%s;", printBuf, resp->telephoneNumbers[i]);
    }

    /* subscriberId */
    uint32_t subscriberIdSize = 0;
    uint32_t subscriberIdPadSize = 0;
    unichar2 *subscriberIdInUtf16 = NULL;
    if (resp->subscriberId == NULL || strlen(resp->subscriberId) == 0) {
        subscriberIdSize = 0;
    } else {
        subscriberIdSize = strlen(resp->subscriberId) * 2;
        subscriberIdPadSize = PAD_SIZE(subscriberIdSize);
        subscriberIdInUtf16 = (unichar2 *)calloc
                (subscriberIdPadSize / 2 + 1, sizeof(unichar2));
        utf8_to_utf16(subscriberIdInUtf16, resp->subscriberId,
                subscriberIdSize / 2, subscriberIdSize / 2);
    }

    /* simIccId */
    uint32_t simIccIdSize = 0;
    uint32_t simIccIdPadSize = 0;
    unichar2 *simIccIdInUtf16 = NULL;
    if (resp->simIccId == NULL || strlen(resp->simIccId) == 0) {
        simIccIdSize = 0;
    } else {
        simIccIdSize = strlen(resp->simIccId) * 2;
        simIccIdPadSize = PAD_SIZE(simIccIdSize);
        simIccIdInUtf16 = (unichar2 *)calloc
                (simIccIdPadSize / sizeof(unichar2) + 1, sizeof(unichar2));
        utf8_to_utf16(simIccIdInUtf16, resp->simIccId,
                simIccIdSize / 2, simIccIdSize / 2);
    }

    /* telephoneNumbers */
    uint32_t telephoneNumberSize[resp->elementCount];
    uint32_t telephoneNumberPadSize[resp->elementCount];
    uint32_t telephoneNumbersTotalSize = 0;
    unichar2 **telephoneNumbersInUtf16 = NULL;
    if (resp->elementCount > 0) {
        char **telephoneNumbers = resp->telephoneNumbers;
        telephoneNumbersInUtf16 = (unichar2 **)calloc(resp->elementCount, sizeof(unichar2 *));
        for (uint32_t i = 0; i < resp->elementCount; i++) {
            telephoneNumberSize[i] = strlen(resp->telephoneNumbers[i]) * 2;
            telephoneNumberPadSize[i] = PAD_SIZE(telephoneNumberSize[i]);
            telephoneNumbersTotalSize += telephoneNumberPadSize[i];
            telephoneNumbersInUtf16[i] = (unichar2 *)
                    calloc(telephoneNumberPadSize[i] / sizeof(unichar2) + 1, sizeof(unichar2));
            utf8_to_utf16(telephoneNumbersInUtf16[i], resp->telephoneNumbers[i],
                    telephoneNumberSize[i] / 2, telephoneNumberSize[i] / 2);
        }
    }

    uint32_t informationBufferSize = sizeof(MbimSubscriberReadyInfo) + subscriberIdPadSize +
            simIccIdPadSize + sizeof(OffsetLengthPairList) * resp->elementCount +
            telephoneNumbersTotalSize;
    uint32_t dataBufferOffset = G_STRUCT_OFFSET(MbimSubscriberReadyInfo, telephoneNumbersRefList) +
            sizeof(OffsetLengthPairList) * resp->elementCount;
    MbimSubscriberReadyInfo *subscriberReadyInfo = (MbimSubscriberReadyInfo *)calloc(1, informationBufferSize);

    subscriberReadyInfo->readyState = resp->readyState;
    subscriberReadyInfo->subscriberIdSize = subscriberIdSize;
    subscriberReadyInfo->subscriberIdOffset = dataBufferOffset;
    subscriberReadyInfo->simIccIdSize = simIccIdSize;
    subscriberReadyInfo->simIccIdOffset = subscriberReadyInfo->subscriberIdOffset + subscriberIdPadSize;
    subscriberReadyInfo->readyInfo = resp->readyInfo;
    subscriberReadyInfo->elementCount = resp->elementCount;
    if (resp->elementCount > 0) {
        uint32_t baseOffset = subscriberReadyInfo->simIccIdOffset + simIccIdPadSize;
        for (uint32_t i = 0; i < resp->elementCount; i++) {
            if (i > 0) {
                baseOffset += telephoneNumberPadSize[i - 1];
            }
            subscriberReadyInfo->telephoneNumbersRefList[i].length = telephoneNumberSize[i];
            subscriberReadyInfo->telephoneNumbersRefList[i].offset = baseOffset;
            memcpy((uint8_t *)subscriberReadyInfo + subscriberReadyInfo->telephoneNumbersRefList[i].offset,
                    (uint8_t *)telephoneNumbersInUtf16[i], telephoneNumberPadSize[i]);
        }
    }
    if (subscriberIdSize != 0) {
        memcpy((uint8_t *)subscriberReadyInfo + subscriberReadyInfo->subscriberIdOffset,
                (uint8_t *)subscriberIdInUtf16, subscriberIdPadSize);
    }
    if (simIccIdSize != 0) {
        memcpy((uint8_t *)subscriberReadyInfo + subscriberReadyInfo->simIccIdOffset,
                (uint8_t *)simIccIdInUtf16, simIccIdPadSize);
    }

    memsetAndFreeUnichar2Arrays(2, subscriberIdInUtf16, simIccIdInUtf16);
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        memsetAndFreeStrings(1, telephoneNumbersInUtf16[i]);
    }
    free(telephoneNumbersInUtf16);

    *outSize = informationBufferSize;
    return subscriberReadyInfo;
}

/******************************************************************************/
/**
 * basic_connect_radio_state: query, set and notification supported
 *
 * query:
 *  @command:   the informationBuffer shall be null
 *  @response:  the informationBuffer shall be MbimRadioStateInfo
 *
 * set:
 *  @command:   the informationBuffer shall be MbimSetRadioState
 *  @response:  the informationBuffer shall be MbimRadioStateInfo
 *
 * notification:
 *  @command:   NA
 *  @response:  the informationBuffer shall be MbimRadioStateInfo
 */
int
mbim_message_parser_basic_connect_radio_state(MbimMessageInfo * messageInfo,
                                              char *            printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            basic_connect_radio_state_query_operation(messageInfo, printBuf);
            break;
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            basic_connect_radio_state_set_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
basic_connect_radio_state_query_operation(MbimMessageInfo * messageInfo,
                                          char *            printBuf) {
    startCommand(printBuf);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_radio_state_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, NULL, 0);
}

void
basic_connect_radio_state_set_operation(MbimMessageInfo *   messageInfo,
                                        char *              printBuf) {
    int32_t bufferLen = 0;
    MbimSetRadioState *radioState = (MbimSetRadioState *)calloc(1, sizeof(MbimSetRadioState));
    MbimSetRadioState *data = (MbimSetRadioState *)
            mbim_message_command_get_raw_information_buffer(messageInfo->message, &bufferLen);

    radioState->radioState = data->radioState;

    startCommand(printBuf);
    appendPrintBuf(printBuf, "%s%s", printBuf,
            mbim_radio_switch_state_get_printable(radioState->radioState));
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_radio_state_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, radioState, sizeof(MbimSetRadioState));
    free(radioState);
}

int
basic_connect_radio_state_set_or_query_ready(Mbim_Token         token,
                                             MbimStatusError    error,
                                             void *             response,
                                             size_t             responseLen,
                                             char *             printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimRadioStateInfo)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimRadioStateInfo));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimRadioStateInfo *resp = (MbimRadioStateInfo *)response;

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%shwRadioState: %s, swRadioState: %s", printBuf,
            mbim_radio_switch_state_get_printable(resp->hwRadioState),
            mbim_radio_switch_state_get_printable(resp->swRadioState));
    closeResponse(printBuf);
    printResponse(printBuf);

    mbim_command_complete(token, error, response, responseLen);

    return 0;
}

int
basic_connect_radio_state_notify(void *      data,
                                 size_t      dataLen,
                                 void **     response,
                                 size_t *    responseLen,
                                 char *      printBuf) {
    if (data == NULL || dataLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (dataLen != sizeof(MbimRadioStateInfo)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)dataLen, (int)sizeof(MbimRadioStateInfo));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimRadioStateInfo *radioStateInfo =
            (MbimRadioStateInfo *)calloc(1, sizeof(MbimRadioStateInfo));
    memcpy(radioStateInfo, (MbimRadioStateInfo *)data, dataLen);

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%shwRadioState: %s, swRadioState: %s", printBuf,
            mbim_radio_switch_state_get_printable(radioStateInfo->hwRadioState),
            mbim_radio_switch_state_get_printable(radioStateInfo->swRadioState));
    closeResponse(printBuf);
    printResponse(printBuf);

    *response = radioStateInfo;
    *responseLen = dataLen;

    return 0;
}

/******************************************************************************/
/**
 * basic_connect_pin: query and set supported
 *
 * query:
 *  @command:   the informationBuffer shall be null
 *  @response:  the informationBuffer shall be MbimPinInfo
 *
 * set:
 *  @command:   the informationBuffer shall be MbimSetPin
 *  @response:  the informationBuffer shall be MbimPinInfo
 *
 */
int
mbim_message_parser_basic_connect_pin(MbimMessageInfo * messageInfo,
                                      char *            printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            basic_connect_pin_query_operation(messageInfo, printBuf);
            break;
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            basic_connect_pin_set_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
basic_connect_pin_query_operation(MbimMessageInfo * messageInfo,
                                  char *            printBuf) {
    startCommand(printBuf);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_pin_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, NULL, 0);
}

void
basic_connect_pin_set_operation(MbimMessageInfo *   messageInfo,
                                char *              printBuf) {
    int32_t bufferLen = 0;
    MbimSetPin_2 *setPin = (MbimSetPin_2 *)calloc(1, sizeof(MbimSetPin_2));
    MbimSetPin *data = (MbimSetPin *)
            mbim_message_command_get_raw_information_buffer(messageInfo->message, &bufferLen);

    setPin->pinType = data->pinType;
    setPin->pinOperation = data->pinOperation;

    uint32_t pinOffset = G_STRUCT_OFFSET(MbimSetPin, pinOffset);
    char *pin = mbim_message_read_string(messageInfo->message, 0, pinOffset);
    setPin->pin = pin;

    uint32_t newPinOffset = G_STRUCT_OFFSET(MbimSetPin, newPinOffset);
    char *newPin = mbim_message_read_string(messageInfo->message, 0, newPinOffset);
    setPin->newPin = newPin;

    startCommand(printBuf);
    appendPrintBuf(printBuf, "%spinType: %s, pinOperation: %s, pin: %s, newPin: %s",
            printBuf,
            mbim_pin_type_get_printable(setPin->pinType),
            mbim_pin_operation_get_printable(setPin->pinOperation),
            setPin->pin,
            setPin->newPin);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_pin_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, setPin, sizeof(MbimSetPin_2));

    memsetAndFreeStrings(2, pin, newPin);
    free(setPin);
}

int
basic_connect_pin_set_or_query_ready(Mbim_Token         token,
                                     MbimStatusError    error,
                                     void *             response,
                                     size_t             responseLen,
                                     char *             printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimPinInfo)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimPinInfo));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimPinInfo *pinInfo = (MbimPinInfo *)response;

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%spinType= %s, pinState: %s, remainingAttemps: %d",
            printBuf,
            mbim_pin_type_get_printable(pinInfo->pinType),
            mbim_pin_state_get_printable(pinInfo->pinState),
            pinInfo->remainingAttemps);
    closeResponse(printBuf);
    printResponse(printBuf);

    mbim_command_complete(token, error, response, responseLen);

    return 0;
}

/******************************************************************************/
/**
 * basic_connect_pin_list: query only
 *
 * query:
 *  @command:   the informationBuffer shall be null
 *  @response:  the informationBuffer shall be MbimPinListInfo
 *
 */
int
mbim_message_parser_basic_connect_pin_list(MbimMessageInfo *    messageInfo,
                                           char *               printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            basic_connect_pin_list_query_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
basic_connect_pin_list_query_operation(MbimMessageInfo *    messageInfo,
                                       char *               printBuf) {
    startCommand(printBuf);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_pin_list_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, NULL, 0);
}

int
basic_connect_pin_list_query_ready(Mbim_Token           token,
                                   MbimStatusError      error,
                                   void *               response,
                                   size_t               responseLen,
                                   char *               printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimPinListInfo)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimPinListInfo));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimPinListInfo *pinListInfo = (MbimPinListInfo *)response;
    startResponse(printBuf);
    appendPrintBuf(printBuf, "%spinDescPin1: [pinMode: %s, pinFormat: %s, "
            "pinLengthMin: %d, pinLengthMax: %d] ",
            printBuf,
            mbim_pin_mode_get_printable(pinListInfo->pinDescPin1.pinMode),
            mbim_pin_format_get_printable(pinListInfo->pinDescPin1.pinFormat),
            pinListInfo->pinDescPin1.pinLengthMin,
            pinListInfo->pinDescPin1.pinLengthMax);
    appendPrintBuf(printBuf, "%spinDescPin2: [pinMode: %s, pinFormat: %s, "
            "pinLengthMin: %d, pinLengthMax: %d] ",
            printBuf,
            mbim_pin_mode_get_printable(pinListInfo->pinDescPin2.pinMode),
            mbim_pin_format_get_printable(pinListInfo->pinDescPin2.pinFormat),
            pinListInfo->pinDescPin2.pinLengthMin,
            pinListInfo->pinDescPin2.pinLengthMax);
    appendPrintBuf(printBuf, "%spinDescDeviceSimPin: [pinMode: %s, pinFormat: %s, "
            "pinLengthMin: %d, pinLengthMax: %d] ",
            printBuf,
            mbim_pin_mode_get_printable(pinListInfo->pinDescDeviceSimPin.pinMode),
            mbim_pin_format_get_printable(pinListInfo->pinDescDeviceSimPin.pinFormat),
            pinListInfo->pinDescDeviceSimPin.pinLengthMin,
            pinListInfo->pinDescDeviceSimPin.pinLengthMax);
    appendPrintBuf(printBuf, "%spinDescDeviceFirstPin: [pinMode: %s, pinFormat: %s, "
            "pinLengthMin: %d, pinLengthMax: %d] ",
            printBuf,
            mbim_pin_mode_get_printable(pinListInfo->pinDescDeviceFirstPin.pinMode),
            mbim_pin_format_get_printable(pinListInfo->pinDescDeviceFirstPin.pinFormat),
            pinListInfo->pinDescDeviceFirstPin.pinLengthMin,
            pinListInfo->pinDescDeviceFirstPin.pinLengthMax);
    appendPrintBuf(printBuf, "%spinDescNetwrokPin: [pinMode: %s, pinFormat: %s, "
            "pinLengthMin: %d, pinLengthMax: %d] ",
            printBuf,
            mbim_pin_mode_get_printable(pinListInfo->pinDescNetwrokPin.pinMode),
            mbim_pin_format_get_printable(pinListInfo->pinDescNetwrokPin.pinFormat),
            pinListInfo->pinDescNetwrokPin.pinLengthMin,
            pinListInfo->pinDescNetwrokPin.pinLengthMax);
    appendPrintBuf(printBuf, "%spinDescNetworkSubsetPin: [pinMode: %s, pinFormat: %s, "
            "pinLengthMin: %d, pinLengthMax: %d] ",
            printBuf,
            mbim_pin_mode_get_printable(pinListInfo->pinDescNetworkSubsetPin.pinMode),
            mbim_pin_format_get_printable(pinListInfo->pinDescNetworkSubsetPin.pinFormat),
            pinListInfo->pinDescNetworkSubsetPin.pinLengthMin,
            pinListInfo->pinDescNetworkSubsetPin.pinLengthMax);
    appendPrintBuf(printBuf, "%spinDescServiceProviderPin: [pinMode: %s, pinFormat: %s, "
            "pinLengthMin: %d, pinLengthMax: %d] ",
            printBuf,
            mbim_pin_mode_get_printable(pinListInfo->pinDescServiceProviderPin.pinMode),
            mbim_pin_format_get_printable(pinListInfo->pinDescServiceProviderPin.pinFormat),
            pinListInfo->pinDescServiceProviderPin.pinLengthMin,
            pinListInfo->pinDescServiceProviderPin.pinLengthMax);
    appendPrintBuf(printBuf, "%spinDescCorporatePin: [pinMode: %s, pinFormat: %s, "
            "pinLengthMin: %d, pinLengthMax: %d] ",
            printBuf,
            mbim_pin_mode_get_printable(pinListInfo->pinDescCorporatePin.pinMode),
            mbim_pin_format_get_printable(pinListInfo->pinDescCorporatePin.pinFormat),
            pinListInfo->pinDescCorporatePin.pinLengthMin,
            pinListInfo->pinDescCorporatePin.pinLengthMax);
    appendPrintBuf(printBuf, "%spinDescSubsidyLock: [pinMode: %s, pinFormat: %s, "
            "pinLengthMin: %d, pinLengthMax: %d] ",
            printBuf,
            mbim_pin_mode_get_printable(pinListInfo->pinDescSubsidyLock.pinMode),
            mbim_pin_format_get_printable(pinListInfo->pinDescSubsidyLock.pinFormat),
            pinListInfo->pinDescSubsidyLock.pinLengthMin,
            pinListInfo->pinDescSubsidyLock.pinLengthMax);
    appendPrintBuf(printBuf, "%spinDescCustom: [pinMode: %s, pinFormat: %s, "
            "pinLengthMin: %d, pinLengthMax: %d] ",
            printBuf,
            mbim_pin_mode_get_printable(pinListInfo->pinDescCustom.pinMode),
            mbim_pin_format_get_printable(pinListInfo->pinDescCustom.pinFormat),
            pinListInfo->pinDescCustom.pinLengthMin,
            pinListInfo->pinDescCustom.pinLengthMax);
    closeResponse(printBuf);
    printResponse(printBuf);

    mbim_command_complete(token, error, response, responseLen);

    return 0;
}

/******************************************************************************/
/**
 * basic_connect_home_provider: query and set supported
 *
 * query:
 *  @command:   the informationBuffer shall be null
 *  @response:  the informationBuffer shall be MbimProvider
 *
 * set:
 *  @command:   the informationBuffer shall be MbimProvider
 *  @response:  the informationBuffer shall be MbimProvider
 *
 */
int
mbim_message_parser_basic_connect_home_provider(MbimMessageInfo *   messageInfo,
                                                char *              printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            basic_connect_home_provider_query_operation(messageInfo, printBuf);
            break;
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            basic_connect_home_provider_set_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
basic_connect_home_provider_query_operation(MbimMessageInfo *   messageInfo,
                                            char *              printBuf) {
    startCommand(printBuf);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_home_provider_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, NULL, 0);
}

void
basic_connect_home_provider_set_operation(MbimMessageInfo * messageInfo,
                                          char *            printBuf) {
    int32_t bufferLen = 0;
    MbimProvider_2 *provider = (MbimProvider_2 *)calloc(1, sizeof(MbimProvider_2));
    MbimProvider *data = (MbimProvider *)
            mbim_message_command_get_raw_information_buffer(messageInfo->message, &bufferLen);

    uint32_t providerIdOffset = G_STRUCT_OFFSET(MbimProvider, providerIdOffset);
    char *providerId = mbim_message_read_string(messageInfo->message, 0, providerIdOffset);
    provider->providerId = providerId;

    uint32_t providerNameOffset = G_STRUCT_OFFSET(MbimProvider, providerNameOffset);
    char *providerName = mbim_message_read_string(messageInfo->message, 0, providerNameOffset);
    provider->providerName = providerName;

    provider->providerState = data->providerState;
    provider->celluarClass = data->celluarClass;

    startCommand(printBuf);
    appendPrintBuf(printBuf, "%sproviderId: %s, providerState: %s, providerName: %s, celluarClass: %s",
            printBuf,
            provider->providerId,
            mbim_provider_state_get_printable(provider->providerState),
            provider->providerName,
            mbim_cellular_class_get_printable(provider->celluarClass));
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_home_provider_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, provider, sizeof(MbimProvider_2));

    memsetAndFreeStrings(2, providerId, providerName);
    free(provider);
}

int
basic_connect_home_provider_set_or_query_ready(Mbim_Token           token,
                                               MbimStatusError      error,
                                               void *               response,
                                               size_t               responseLen,
                                               char *               printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimProvider_2)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimProvider_2));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    startResponse(printBuf);

    uint32_t informationBufferSize = 0;
    MbimProvider *provider = home_provider_response_builder(
            response, responseLen, &informationBufferSize, printBuf);

    closeResponse(printBuf);
    printResponse(printBuf);

    mbim_command_complete(token, error, provider, informationBufferSize);
    free(provider);

    return 0;
}

MbimProvider *
home_provider_response_builder(void *       response,
                               size_t       responseLen,
                               uint32_t *   outSize,
                               char *       printBuf) {
    MBIM_UNUSED_PARM(responseLen);

    MbimProvider_2 *resp = (MbimProvider_2 *)response;

    appendPrintBuf(printBuf, "%s[providerId: %s, providerState: %s, "
            "providerName: %s, cellularClass: %s, rssi: %d, errorRate: %d]",
            printBuf, resp->providerId,
            mbim_provider_state_get_printable(resp->providerState),
            resp->providerName,
            mbim_cellular_class_get_printable(resp->celluarClass),
            resp->rssi, resp->errorRate);

    /* providerId */
    uint32_t providerIdSize = 0;
    uint32_t providerIdPadSize = 0;
    unichar2 *providerIdInUtf16 = NULL;
    if (resp->providerId == NULL || strlen(resp->providerId) == 0) {
        providerIdSize = 0;
    } else {
        providerIdSize = strlen(resp->providerId) * 2;
        providerIdPadSize = PAD_SIZE(providerIdSize);
        providerIdInUtf16 = (unichar2 *)
                calloc(providerIdPadSize / sizeof(unichar2) + 1, sizeof(unichar2));
        utf8_to_utf16(providerIdInUtf16, resp->providerId,
                providerIdSize / 2, providerIdSize / 2);
    }

    /* providerName */
    uint32_t providerNameSize = 0;
    uint32_t providerNamePadSize = 0;
    unichar2 *providerNameInUtf16 = NULL;
    if (resp->providerName == NULL || strlen(resp->providerName) == 0) {
        providerNameSize = 0;
    } else {
        providerNameSize = strlen(resp->providerName) * 2;
        providerNamePadSize = PAD_SIZE(providerNameSize);
        providerNameInUtf16 = (unichar2 *)calloc
                (providerNamePadSize / sizeof(unichar2) + 1, sizeof(unichar2));
        utf8_to_utf16(providerNameInUtf16, resp->providerName,
                providerNameSize / 2, providerNameSize / 2);
    }

    uint32_t informationBufferSize = sizeof(MbimProvider) + providerIdPadSize + providerNamePadSize;
    uint32_t dataBufferOffset = G_STRUCT_OFFSET(MbimProvider, dataBuffer);

    MbimProvider *provider = (MbimProvider *)calloc(1, informationBufferSize);
    provider->providerIdOffset = dataBufferOffset;
    provider->providerIdSize = providerIdSize;
    provider->providerState = resp->providerState;
    provider->providerNameOffset = provider->providerIdOffset + providerIdPadSize;
    provider->providerNameSize = providerNameSize;
    provider->celluarClass = resp->celluarClass;
    provider->rssi = resp->rssi;
    provider->errorRate = resp->errorRate;

    if (provider->providerIdSize != 0) {
        memcpy((uint8_t *)provider + provider->providerIdOffset,
                (uint8_t *)providerIdInUtf16, providerIdPadSize);
    }
    if (provider->providerNameSize != 0) {
        memcpy((uint8_t *)provider + provider->providerNameOffset,
                (uint8_t *)providerNameInUtf16, providerNamePadSize);
    }

    memsetAndFreeUnichar2Arrays(2, providerIdInUtf16, providerNameInUtf16);

    *outSize = informationBufferSize;
    return provider;
}

/******************************************************************************/
/**
 * basic_connect_preferred_providers: query, set and notification supported
 *
 * query:
 *  @command:   the informationBuffer shall be null
 *  @response:  the informationBuffer shall be MbimProviders
 *
 * set:
 *  @command:   the informationBuffer shall be MbimProviders
 *  @response:  the informationBuffer shall be MbimProviders
 *
 * notification:
 *  @command:   NA
 *  @response:  the informationBuffer shall be MbimProviders
 *
 */
int
mbim_message_parser_basic_connect_preferred_providers(MbimMessageInfo * messageInfo,
                                                      char *            printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            basic_connect_preferred_providers_query_operation(messageInfo, printBuf);
            break;
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            basic_connect_preferred_providers_set_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
basic_connect_preferred_providers_query_operation(MbimMessageInfo * messageInfo,
                                                  char *            printBuf) {
    startCommand(printBuf);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_preferred_providers_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, NULL, 0);
}

void basic_connect_preferred_providers_set_operation(MbimMessageInfo *  messageInfo,
                                                     char *             printBuf) {
    int32_t bufferLen = 0;

    MbimProviders *data = (MbimProviders *)
            mbim_message_command_get_raw_information_buffer(messageInfo->message, &bufferLen);

    uint32_t elementCount = data->elementCount;
    uint32_t information_buffer_offset = mbim_message_get_information_buffer_offset(messageInfo->message);

    MbimProviders_2 *providers = (MbimProviders_2 *)calloc(1, sizeof(MbimProviders_2));
    providers->elementCount = elementCount;
    providers->provider = (MbimProvider_2 **)calloc(elementCount, sizeof(MbimProvider_2 *));

    startCommand(printBuf);
    appendPrintBuf(printBuf, "%selementCount = %d", printBuf, elementCount);

    for (uint32_t i = 0; i < elementCount; i++) {
        providers->provider[i] = (MbimProvider_2 *)calloc(1, sizeof(MbimProvider_2));

        uint32_t offset = data->providerRefList[i].offset;

        MbimProvider *providerData = (MbimProvider *)
                G_STRUCT_MEMBER_P(messageInfo->message->data, (information_buffer_offset + offset));

        uint32_t providerIdOffset = G_STRUCT_OFFSET(MbimProvider, providerIdOffset);
        char *providerId = mbim_message_read_string(messageInfo->message, offset, providerIdOffset);
        providers->provider[i]->providerId = providerId;

        uint32_t providerNameOffset = G_STRUCT_OFFSET(MbimProvider, providerNameOffset);
        char *providerName = mbim_message_read_string(messageInfo->message, offset, providerNameOffset);
        providers->provider[i]->providerName = providerName;

        providers->provider[i]->providerState = providerData->providerState;
        providers->provider[i]->celluarClass = providerData->celluarClass;
        providers->provider[i]->rssi = providerData->rssi;
        providers->provider[i]->errorRate = providerData->errorRate;

        appendPrintBuf(printBuf, "%s[providerId: %s, providerState: %s, providerName: %s,"
                " celluarClass: %s, rssi = %d, errorRate = %d]",
                printBuf,
                providers->provider[i]->providerId,
                mbim_provider_state_get_printable(providers->provider[i]->providerState),
                providers->provider[i]->providerName,
                mbim_cellular_class_get_printable(providers->provider[i]->celluarClass),
                providers->provider[i]->rssi,
                providers->provider[i]->errorRate);
    }


    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_preferred_providers_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, providers, sizeof(MbimProviders_2));

    for (uint32_t i = 0; i < elementCount; i++) {
        memsetAndFreeStrings(2, providers->provider[i]->providerId,
                providers->provider[i]->providerName);
        free(providers->provider[i]);
    }
    free(providers->provider);
    free(providers);
}

int
basic_connect_preferred_providers_set_or_query_ready(Mbim_Token         token,
                                                     MbimStatusError    error,
                                                     void *             response,
                                                     size_t             responseLen,
                                                     char *             printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimProviders_2)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimProviders_2));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimProviders_2 *resp = (MbimProviders_2 *)response;

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%selementCount: %d", printBuf, resp->elementCount);

    uint32_t informationBufferSize[resp->elementCount];
    uint32_t informationBufferSizeInTotal = 0;

    MbimProvider **provider = (MbimProvider **)calloc(resp->elementCount, sizeof(MbimProvider));
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        provider[i] = home_provider_response_builder(resp->provider[i],
                sizeof(MbimProvider_2), &informationBufferSize[i], printBuf);
        informationBufferSizeInTotal += informationBufferSize[i];
    }

    informationBufferSizeInTotal += sizeof(MbimProviders) +
            sizeof(OffsetLengthPairList) * resp->elementCount;

    MbimProviders *providers = (MbimProviders *)calloc(1, informationBufferSizeInTotal);
    uint32_t dataBufferOffset = G_STRUCT_OFFSET(MbimProviders, providerRefList) +
            sizeof(OffsetLengthPairList) * resp->elementCount;
    providers->elementCount = resp->elementCount;
    uint32_t offset = dataBufferOffset;
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        providers->providerRefList[i].offset = offset;
        providers->providerRefList[i].length = informationBufferSize[i];
        offset += informationBufferSize[i];

        memcpy((uint8_t *)providers + providers->providerRefList[i].offset,
                (uint8_t *)(provider[i]), informationBufferSize[i]);
    }

    closeResponse(printBuf);
    printResponse(printBuf);

    mbim_command_complete(token, error, providers, informationBufferSizeInTotal);
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        free(provider[i]);
    }
    free(provider);
    free(providers);

    return 0;
}

int basic_connect_preferred_providers_notify(void *      data,
                                             size_t      dataLen,
                                             void **     response,
                                             size_t *    responseLen,
                                             char *      printBuf) {
    if (data == NULL || dataLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (dataLen != sizeof(MbimProviders_2)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)dataLen, (int)sizeof(MbimProviders_2));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimProviders_2 *resp = (MbimProviders_2 *)data;

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%selementCount: %d", printBuf, resp->elementCount);

    uint32_t informationBufferSize[resp->elementCount];
    uint32_t informationBufferSizeInTotal = 0;

    MbimProvider **provider = (MbimProvider **)calloc(resp->elementCount, sizeof(MbimProvider));
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        provider[i] = home_provider_response_builder(resp->provider[i],
                sizeof(MbimProvider_2), &informationBufferSize[i], printBuf);
        informationBufferSizeInTotal += informationBufferSize[i];
    }

    informationBufferSizeInTotal += sizeof(MbimProviders) +
            sizeof(OffsetLengthPairList) * resp->elementCount;

    MbimProviders *providers = (MbimProviders *)calloc(1, informationBufferSizeInTotal);
    uint32_t dataBufferOffset = G_STRUCT_OFFSET(MbimProviders, providerRefList) +
            sizeof(OffsetLengthPairList) * resp->elementCount;
    providers->elementCount = resp->elementCount;
    uint32_t offset = dataBufferOffset;
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        providers->providerRefList[i].offset = offset;
        providers->providerRefList[i].length = informationBufferSize[i];
        offset += informationBufferSize[i];

        memcpy((uint8_t *)providers + providers->providerRefList[i].offset,
                (uint8_t *)(provider[i]), informationBufferSize[i]);
    }

    closeResponse(printBuf);
    printResponse(printBuf);

    *response = providers;
    *responseLen = informationBufferSizeInTotal;

    return 0;
}

/******************************************************************************/
/**
 * basic_connect_visible_providers: query only
 *
 * query:
 *  @command:   the informationBuffer shall be MbimVisibleProvidersReq
 *  @response:  the informationBuffer shall be MbimProviders
 *
 */
int
mbim_message_parser_basic_connect_visible_providers(MbimMessageInfo *   messageInfo,
                                                    char *              printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            basic_connect_visible_providers_query_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
basic_connect_visible_providers_query_operation(MbimMessageInfo *   messageInfo,
                                                char *              printBuf) {
    int32_t bufferLen = 0;
    MbimVisibleProvidersReq *data = (MbimVisibleProvidersReq *)
            mbim_message_command_get_raw_information_buffer(messageInfo->message, &bufferLen);

    startCommand(printBuf);
    appendPrintBuf(printBuf, "%saction: %s", printBuf,
            mbim_visible_providers_action_get_printable(data->action));
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_visible_providers_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, data, sizeof(MbimVisibleProvidersReq));
}

int
basic_connect_visible_providers_query_ready(Mbim_Token          token,
                                            MbimStatusError     error,
                                            void *              response,
                                            size_t              responseLen,
                                            char *              printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimProviders_2)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimProviders_2));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimProviders_2 *resp = (MbimProviders_2 *)response;

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%selementCount: %d", printBuf, resp->elementCount);

    uint32_t informationBufferSize[resp->elementCount];
    uint32_t informationBufferSizeInTotal = 0;

    MbimProvider **provider = (MbimProvider **)calloc(resp->elementCount, sizeof(MbimProvider *));
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        provider[i] = home_provider_response_builder(resp->provider[i],
                sizeof(MbimProvider_2), &informationBufferSize[i], printBuf);
        informationBufferSizeInTotal += informationBufferSize[i];
    }

    informationBufferSizeInTotal += sizeof(MbimProviders) +
            sizeof(OffsetLengthPairList) * resp->elementCount;

    MbimProviders *providers = (MbimProviders *)calloc(1, informationBufferSizeInTotal);
    uint32_t dataBufferOffset = G_STRUCT_OFFSET(MbimProviders, providerRefList) +
            sizeof(OffsetLengthPairList) * resp->elementCount;
    providers->elementCount = resp->elementCount;
    uint32_t offset = dataBufferOffset;
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        providers->providerRefList[i].offset = offset;
        providers->providerRefList[i].length = informationBufferSize[i];
        offset += informationBufferSize[i];

        memcpy((uint8_t *)providers + providers->providerRefList[i].offset,
                (uint8_t *)(provider[i]), informationBufferSize[i]);
    }

    closeResponse(printBuf);
    printResponse(printBuf);

    mbim_command_complete(token, error, providers, informationBufferSizeInTotal);
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        free(provider[i]);
    }
    free(provider);
    free(providers);

    return 0;
}

/******************************************************************************/
/**
 * basic_connect_register_state: query, set and notification supported
 *
 * query:
 *  @command:   the informationBuffer shall be null
 *  @response:  the informationBuffer shall be MbimRegistrationStateInfo
 *
 * set:
 *  @command:   the informationBuffer shall be MbimSetRegistrationState
 *  @response:  the informationBuffer shall be MbimRegistrationStateInfo
 *
 * notification:
 *  @command:   NA
 *  @response:  the informationBuffer shall be MbimRegistrationStateInfo
 */
int
mbim_message_parser_basic_connect_register_state(MbimMessageInfo *  messageInfo,
                                                 char *             printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            basic_connect_register_state_query_operation(messageInfo, printBuf);
            break;
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            basic_connect_register_state_set_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
basic_connect_register_state_query_operation(MbimMessageInfo *  messageInfo,
                                             char *             printBuf) {
    startCommand(printBuf);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_register_state_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, NULL, 0);
}

void
basic_connect_register_state_set_operation(MbimMessageInfo *    messageInfo,
                                           char *               printBuf) {
    int32_t bufferLen = 0;
    MbimSetRegistrationState *data = (MbimSetRegistrationState *)
            mbim_message_command_get_raw_information_buffer(messageInfo->message, &bufferLen);
    MbimSetRegistrationState_2 *setRegistrationState =
            (MbimSetRegistrationState_2 *)calloc(1, sizeof(MbimSetRegistrationState_2));

    setRegistrationState->registerAction = data->registerAction;
    setRegistrationState->dataClass = data->dataClass;
    setRegistrationState->providerId = mbim_message_read_string(messageInfo->message, 0, 0);

    startCommand(printBuf);
    appendPrintBuf(printBuf, "%sproviderId: %s, registerAction: %s, dataClass: %s",
            printBuf,
            setRegistrationState->providerId,
            mbim_register_action_get_printable(setRegistrationState->registerAction),
            mbim_data_class_get_printable(setRegistrationState->dataClass));
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_register_state_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, setRegistrationState, sizeof(MbimSetRegistrationState_2));

    free(setRegistrationState->providerId);
    free(setRegistrationState);
}

int
basic_connect_register_state_set_or_query_ready(Mbim_Token          token,
                                                MbimStatusError     error,
                                                void *              response,
                                                size_t              responseLen,
                                                char *              printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimRegistrationStateInfo_2)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimRegistrationStateInfo_2));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    startResponse(printBuf);

    uint32_t informationBufferSize = 0;
    MbimRegistrationStateInfo *registrationStateInfo =
            register_state_response_builder(response, responseLen, &informationBufferSize, printBuf);

    closeResponse(printBuf);
    printResponse(printBuf);

    mbim_command_complete(token, error, registrationStateInfo, informationBufferSize);
    free(registrationStateInfo);

    return 0;
}

int
basic_connect_register_state_notify(void *      data,
                                    size_t      dataLen,
                                    void **     response,
                                    size_t *    responseLen,
                                    char *      printBuf) {
    if (data == NULL || dataLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (dataLen != sizeof(MbimRegistrationStateInfo_2)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)dataLen, (int)sizeof(MbimRegistrationStateInfo_2));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    startResponse(printBuf);

    MbimRegistrationStateInfo *registrationStateInfo =
            register_state_response_builder(data, dataLen, responseLen, printBuf);

    closeResponse(printBuf);
    printResponse(printBuf);

    *response = registrationStateInfo;

    return 0;
}

MbimRegistrationStateInfo *
register_state_response_builder(void *       response,
                                size_t       responseLen,
                                uint32_t *   outSize,
                                char *       printBuf) {
    MBIM_UNUSED_PARM(responseLen);

    MbimRegistrationStateInfo_2 *resp = (MbimRegistrationStateInfo_2 *)response;

    appendPrintBuf(printBuf, "%snwError: %s, registerState: %s, registerMode: %s,"
            " availableDataClasses: %s, currentCellularClass: %s, providerId: %s,"
            " providerName: %s, roamingText: %s, registrationFlag: %s",
            printBuf,
            mbim_network_error_get_printable(resp->nwError),
            mbim_register_state_get_printable(resp->registerState),
            mbim_register_mode_get_printable(resp->registerMode),
            mbim_data_class_get_printable(resp->availableDataClasses),
            mbim_cellular_class_get_printable(resp->currentCellularClass),
            resp->providerId, resp->providerName, resp->roamingText,
            mbim_registration_flag_get_printable(resp->registrationFlag));

    /* providerId */
    uint32_t providerIdSize = 0;
    uint32_t providerIdPadSize = 0;
    unichar2 *providerIdInUtf16 = NULL;
    if (resp->providerId == NULL || strlen(resp->providerId) == 0) {
        providerIdSize = 0;
    } else {
        providerIdSize = strlen(resp->providerId) * 2;
        providerIdPadSize = PAD_SIZE(providerIdSize);
        providerIdInUtf16 = (unichar2 *)calloc
                (providerIdPadSize / sizeof(unichar2) + 1, sizeof(unichar2));
        utf8_to_utf16(providerIdInUtf16, resp->providerId,
                providerIdSize / 2, providerIdSize / 2);
    }

    /* providerName */
    uint32_t providerNameSize = 0;
    uint32_t providerNamePadSize = 0;
    unichar2 *providerNameInUtf16 = NULL;
    if (resp->providerName == NULL || strlen(resp->providerName) == 0) {
        providerNameSize = 0;
    } else {
        providerNameSize = strlen(resp->providerName) * 2;
        providerNamePadSize = PAD_SIZE(providerNameSize);
        providerNameInUtf16 = (unichar2 *)calloc
                (providerNamePadSize / sizeof(unichar2) + 1, sizeof(unichar2));
        utf8_to_utf16(providerNameInUtf16, resp->providerName,
                providerNameSize / 2, providerNameSize / 2);
    }

    /* roamingText */
    uint32_t roamingTextSize = 0;
    uint32_t roamingTextPadSize = 0;
    unichar2 *roamingTextInUtf16 = NULL;
    if (resp->roamingText == NULL || strlen(resp->roamingText) == 0) {
        roamingTextSize = 0;
    } else {
        roamingTextSize = strlen(resp->roamingText) * 2;
        roamingTextPadSize = PAD_SIZE(roamingTextSize);
        roamingTextInUtf16 = (unichar2 *)calloc
                (roamingTextPadSize / sizeof(unichar2) + 1, sizeof(unichar2));
        utf8_to_utf16(roamingTextInUtf16, resp->roamingText,
                roamingTextSize / 2, roamingTextSize / 2);
    }

    uint32_t informationBufferSize = sizeof(MbimRegistrationStateInfo) +
            providerIdPadSize + providerNamePadSize + roamingTextPadSize;
    uint32_t dataBufferOffset = G_STRUCT_OFFSET(MbimRegistrationStateInfo, dataBuffer);

    MbimRegistrationStateInfo *registrationStateInfo =
            (MbimRegistrationStateInfo *)calloc(1, informationBufferSize);
    registrationStateInfo->nwError = resp->nwError;
    registrationStateInfo->registerState = resp->registerState;
    registrationStateInfo->registerMode = resp->registerMode;
    registrationStateInfo->availableDataClasses = resp->availableDataClasses;
    registrationStateInfo->currentCellularClass = resp->currentCellularClass;
    registrationStateInfo->providerIdOffset = dataBufferOffset;
    registrationStateInfo->providerIdSize = providerIdSize;
    registrationStateInfo->providerNameOffset =
            registrationStateInfo->providerIdOffset + providerIdPadSize;
    registrationStateInfo->providerNameSize = providerNameSize;
    registrationStateInfo->roamingTextOffset =
            registrationStateInfo->providerNameOffset + providerNamePadSize;
    registrationStateInfo->roamingTextSize = roamingTextSize;
    registrationStateInfo->registrationFlag = resp->registrationFlag;

    if (registrationStateInfo->providerIdSize != 0) {
        memcpy((uint8_t *)registrationStateInfo + registrationStateInfo->providerIdOffset,
                (uint8_t *)providerIdInUtf16, providerIdPadSize);
    }
    if (registrationStateInfo->providerNameSize != 0) {
        memcpy((uint8_t *)registrationStateInfo + registrationStateInfo->providerNameOffset,
                (uint8_t *)providerNameInUtf16, providerNamePadSize);
    }
    if (registrationStateInfo->roamingTextSize != 0) {
        memcpy((uint8_t *)registrationStateInfo + registrationStateInfo->roamingTextOffset,
                (uint8_t *)roamingTextInUtf16, roamingTextPadSize);
    }

    memsetAndFreeUnichar2Arrays(3, providerIdInUtf16, providerNameInUtf16, roamingTextInUtf16);

    *outSize = informationBufferSize;
    return registrationStateInfo;
}

/******************************************************************************/
/**
 * basic_connect_packet_service: query, set and notification supported
 *
 * query:
 *  @command:   the informationBuffer shall be null
 *  @response:  the informationBuffer shall be MbimPacketServiceInfo
 *
 * set:
 *  @command:   the informationBuffer shall be MbimSetPacketService
 *  @response:  the informationBuffer shall be MbimPacketServiceInfo
 *
 * notification:
 *  @command:   NA
 *  @response:  the informationBuffer shall be MbimPacketServiceInfo
 */
int
mbim_message_parser_basic_connect_packet_service(MbimMessageInfo *  messageInfo,
                                                 char *             printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            basic_connect_packet_service_query_operation(messageInfo, printBuf);
            break;
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            basic_connect_packet_service_set_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
basic_connect_packet_service_query_operation(MbimMessageInfo *  messageInfo,
                                             char *             printBuf) {
    startCommand(printBuf);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_packet_service_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, NULL, 0);
}

void
basic_connect_packet_service_set_operation(MbimMessageInfo *    messageInfo,
                                           char *               printBuf) {
    int32_t bufferLen = 0;
    MbimSetPacketService *data = (MbimSetPacketService *)
            mbim_message_command_get_raw_information_buffer(messageInfo->message, &bufferLen);

    startCommand(printBuf);
    appendPrintBuf(printBuf, "%spacketServiceAction: %s", printBuf,
            mbim_packet_service_action_get_printable(data->packetServiceAction));
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_packet_service_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, data, sizeof(MbimSetPacketService));
}

int
basic_connect_packet_service_set_or_query_ready(Mbim_Token          token,
                                                MbimStatusError     error,
                                                void *              response,
                                                size_t              responseLen,
                                                char *              printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimPacketServiceInfo)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimPacketServiceInfo));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimPacketServiceInfo * packetServiceInfo= (MbimPacketServiceInfo *)response;

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%snwError: %s, packetServiceState: %s, "
            "highestAvailableDataClass: %s, upLinkSpeed: %lld, downLinkSpeed: %lld",
            printBuf,
            mbim_network_error_get_printable(packetServiceInfo->nwError),
            mbim_packet_service_state_get_printable(packetServiceInfo->packetServiceState),
            mbim_data_class_get_printable(packetServiceInfo->highestAvailableDataClass),
            packetServiceInfo->upLinkSpeed, packetServiceInfo->downLinkSpeed);
    closeResponse(printBuf);
    printResponse(printBuf);

    mbim_command_complete(token, error, response, responseLen);

    return 0;
}

int
basic_connect_packet_service_notify(void *      data,
                                    size_t      dataLen,
                                    void **     response,
                                    size_t *    responseLen,
                                    char *      printBuf) {
    if (data == NULL || dataLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (dataLen != sizeof(MbimPacketServiceInfo)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)dataLen, (int)sizeof(MbimPacketServiceInfo));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimPacketServiceInfo *packetServiceInfo =
            (MbimPacketServiceInfo *)calloc(1, sizeof(MbimPacketServiceInfo));
    memcpy(packetServiceInfo, (MbimPacketServiceInfo *)data, dataLen);

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%snwError: %s, packetServiceState: %s, "
            "highestAvailableDataClass: %s, upLinkSpeed: %lld, downLinkSpeed: %lld",
            printBuf,
            mbim_network_error_get_printable(packetServiceInfo->nwError),
            mbim_packet_service_state_get_printable(packetServiceInfo->packetServiceState),
            mbim_data_class_get_printable(packetServiceInfo->highestAvailableDataClass),
            packetServiceInfo->upLinkSpeed, packetServiceInfo->downLinkSpeed);
    closeResponse(printBuf);
    printResponse(printBuf);

    *response = packetServiceInfo;
    *responseLen = dataLen;

    return 0;
}

/******************************************************************************/
/**
 * basic_connect_signal_state: query, set and notification supported
 *
 * query:
 *  @command:   the informationBuffer shall be null
 *  @response:  the informationBuffer shall be MbimSignalStateInfo
 *
 * set:
 *  @command:   the informationBuffer shall be MbimSetSignalState
 *  @response:  the informationBuffer shall be MbimSignalStateInfo
 *
 * notification:
 *  @command:   NA
 *  @response:  the informationBuffer shall be MbimSignalStateInfo
 */

int
mbim_message_parser_basic_connect_signal_state_service(MbimMessageInfo *    messageInfo,
                                                       char *               printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            basic_connect_signal_state_query_operation(messageInfo, printBuf);
            break;
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            basic_connect_signal_state_set_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
basic_connect_signal_state_query_operation(MbimMessageInfo *        messageInfo,
                                           char *                   printBuf) {
    startCommand(printBuf);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_signal_state_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, NULL, 0);
}

void basic_connect_signal_state_set_operation(MbimMessageInfo *     messageInfo,
                                              char *                printBuf) {
    int32_t bufferLen = 0;
    MbimSetSignalState *data = (MbimSetSignalState *)
            mbim_message_command_get_raw_information_buffer(messageInfo->message, &bufferLen);

    startCommand(printBuf);
    appendPrintBuf(printBuf, "%ssignalStrengthInterval: %d, rssiThreshold: %d, errorRateThreshold: %d",
            printBuf, data->signalStrengthInterval, data->rssiThreshold,
            data->errorRateThreshold);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_signal_state_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, data, sizeof(MbimSetSignalState));
}

int
basic_connect_signal_state_set_or_query_ready(Mbim_Token            token,
                                              MbimStatusError       error,
                                              void *                response,
                                              size_t                responseLen,
                                              char *                printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimSignalStateInfo)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimSignalStateInfo));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimSignalStateInfo *signalStateInfo = (MbimSignalStateInfo *)response;

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%srssi: %d, errorRate: %d, "
            "signalStrengthInterval: %d, rssiThreshold: %d, "
            "errorRateThreshold: %d", printBuf,
            signalStateInfo->rssi, signalStateInfo->errorRate,
            signalStateInfo->signalStrengthInterval,
            signalStateInfo->rssiThreshold, signalStateInfo->errorRateThreshold);
    closeResponse(printBuf);
    printResponse(printBuf);

    mbim_command_complete(token, error, response, responseLen);

    return 0;
}

int
basic_connect_signal_state_notify(void *      data,
                                  size_t      dataLen,
                                  void **     response,
                                  size_t *    responseLen,
                                  char *      printBuf) {
    if (data == NULL || dataLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (dataLen != sizeof(MbimSignalStateInfo)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)dataLen, (int)sizeof(MbimSignalStateInfo));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimSignalStateInfo *indicateInfo = (MbimSignalStateInfo *)data;

    MbimSignalStateInfo *signalStateInfo =
            (MbimSignalStateInfo *)calloc(1, sizeof(MbimSignalStateInfo));
    signalStateInfo->rssi = indicateInfo->rssi;
    signalStateInfo->errorRate = indicateInfo->errorRate;
    signalStateInfo->signalStrengthInterval = indicateInfo->signalStrengthInterval;
    signalStateInfo->rssiThreshold = indicateInfo->rssiThreshold;
    signalStateInfo->errorRateThreshold = indicateInfo->errorRateThreshold;

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%srssi: %d, errorRate: %d, "
            "signalStrengthInterval: %d, rssiThreshold: %d, "
            "errorRateThreshold: %d", printBuf,
            indicateInfo->rssi, indicateInfo->errorRate,
            indicateInfo->signalStrengthInterval,
            indicateInfo->rssiThreshold, indicateInfo->errorRateThreshold);
    closeResponse(printBuf);
    printResponse(printBuf);

    *response = signalStateInfo;
    *responseLen = dataLen;
    return 0;
}

/******************************************************************************/
/**
 * basic_connect_connect: query, set and notification supported
 *
 * query:
 *  @command:   the informationBuffer shall be MbimConnectInfo
 *  @response:  the informationBuffer shall be MbimConnectInfo
 *
 * set:
 *  @command:   the informationBuffer shall be MbimSetSignalState
 *  @response:  the informationBuffer shall be MbimConnectInfo
 *
 * notification:
 *  @command:   NA
 *  @response:  the informationBuffer shall be MbimConnectInfo
 *
 * Note:
 *  for query, the only relevant field of MbimConnectInfo in command's
 *  informationBuffer is the sessionId.
 */
int
mbim_message_parser_basic_connect_connect(MbimMessageInfo *     messageInfo,
                                          char *                printBuf) {
    int ret = 0;
    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            basic_connect_connect_query_operation(messageInfo, printBuf);
            break;
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            basic_connect_connect_set_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
basic_connect_connect_query_operation(MbimMessageInfo *         messageInfo,
                                      char *                    printBuf) {
    int32_t bufferLen = 0;
    MbimConnectInfo *data = (MbimConnectInfo *)
            mbim_message_command_get_raw_information_buffer(messageInfo->message, &bufferLen);

    startCommand(printBuf);
    appendPrintBuf(printBuf, "%ssessionId = %d", printBuf, data->sessionId);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_connect_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, &(data->sessionId), sizeof(uint32_t));
}

void
basic_connect_connect_set_operation(MbimMessageInfo *           messageInfo,
                                    char *                      printBuf) {
    int32_t bufferLen = 0;
    MbimSetConnect *data = (MbimSetConnect *)
            mbim_message_command_get_raw_information_buffer(messageInfo->message, &bufferLen);

    MbimSetConnect_2 *setConnect = (MbimSetConnect_2 *)calloc(1, sizeof(MbimSetConnect));
    setConnect->sessionId = data->sessionId;
    setConnect->activationCommand = data->activationCommand;
    setConnect->compression = data->compression;
    setConnect->authProtocol = data->authProtocol;
    setConnect->IPType = data->IPType;
    setConnect->contextType = mbim_uuid_to_context_type(&(data->contextType));

    uint32_t accessStringOffset = G_STRUCT_OFFSET(MbimSetConnect, accessStringOffset);
    setConnect->accessString = mbim_message_read_string(messageInfo->message, 0, accessStringOffset);

    uint32_t usernameOffset = G_STRUCT_OFFSET(MbimSetConnect, usernameOffset);
    setConnect->username = mbim_message_read_string(messageInfo->message, 0, usernameOffset);

    uint32_t passwordOffset = G_STRUCT_OFFSET(MbimSetConnect, passwordOffset);
    setConnect->password = mbim_message_read_string(messageInfo->message, 0, passwordOffset);

    startCommand(printBuf);
    appendPrintBuf(printBuf, "%ssessionId: %d, activationCommand: %s, accessString: %s,"
            " username: %s, password: %s, compression: %s, authProtocol: %s,"
            " IPType: %s, contextType: %s",
            printBuf,
            setConnect->sessionId,
            mbim_activation_command_get_printable(setConnect->activationCommand),
            setConnect->accessString, setConnect->username, setConnect->password,
            mbim_compression_get_printable(setConnect->compression),
            mbim_auth_protocol_get_printable(setConnect->authProtocol),
            mbim_context_ip_type_get_printable(setConnect->IPType),
            mbim_context_type_get_printable(setConnect->contextType));
    closeCommand(printBuf);
    printCommand(printBuf);


    messageInfo->callback = basic_connect_connect_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, setConnect, sizeof(MbimSetConnect));

    memsetAndFreeStrings(3, setConnect->accessString, setConnect->username,
            setConnect->password);
    free(setConnect);
}

int
basic_connect_connect_set_or_query_ready(Mbim_Token         token,
                                         MbimStatusError    error,
                                         void *             response,
                                         size_t             responseLen,
                                         char *             printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimConnectInfo)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimConnectInfo));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimConnectInfo *connectInfo = (MbimConnectInfo *)response;
    startResponse(printBuf);
    appendPrintBuf(printBuf, "%ssessionId: %d, activationState: %s,"
            "voiceCallState: %s, IPType: %s, contextType: %s, nwError: %s",
            printBuf, connectInfo->sessionId,
            mbim_activation_state_get_printable(connectInfo->activationState),
            mbim_voice_call_state_get_printable(connectInfo->voiceCallState),
            mbim_context_ip_type_get_printable(connectInfo->IPType),
            mbim_context_type_get_printable(
                    mbim_uuid_to_context_type(&(connectInfo->contextType))),
            mbim_network_error_get_printable(connectInfo->nwError));
    closeResponse(printBuf);
    printResponse(printBuf);

    mbim_command_complete(token, error, response, responseLen);

    return 0;
}

int
basic_connect_connect_notify(void *      data,
                             size_t      dataLen,
                             void **     response,
                             size_t *    responseLen,
                             char *      printBuf) {
    if (data == NULL && responseLen != 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (dataLen != sizeof(MbimConnectInfo)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)dataLen, (int)sizeof(MbimConnectInfo));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimConnectInfo *connectInfo = (MbimConnectInfo *)calloc(1, sizeof(MbimConnectInfo));
    memcpy(connectInfo, data, dataLen);

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%ssessionId: %d, activationState: %s,"
            "voiceCallState: %s, IPType: %s, contextType: %s, nwError: %s",
            printBuf, connectInfo->sessionId,
            mbim_activation_state_get_printable(connectInfo->activationState),
            mbim_voice_call_state_get_printable(connectInfo->voiceCallState),
            mbim_context_ip_type_get_printable(connectInfo->IPType),
            mbim_context_type_get_printable(
                    mbim_uuid_to_context_type(&(connectInfo->contextType))),
            mbim_network_error_get_printable(connectInfo->nwError));
    closeResponse(printBuf);
    printResponse(printBuf);

    *response = connectInfo;
    *responseLen = dataLen;

    return 0;
}

/******************************************************************************/
/**
 * basic_connect_provisioned_contexts: query, set and notification supported
 *
 * query:
 *  @command:   the informationBuffer shall be null
 *  @response:  the informationBuffer shall be MbimProvisionedContextsInfo
 *
 * set:
 *  @command:   the informationBuffer shall be MbimSetProvisionedContext
 *  @response:  the informationBuffer shall be MbimProvisionedContextsInfo
 *
 * notification:
 *  @command:   NA
 *  @response:  the informationBuffer shall be MbimProvisionedContextsInfo
 */
int
mbim_message_parser_basic_connect_provisioned_contexts(MbimMessageInfo *    messageInfo,
                                                       char *               printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            basic_connect_provisioned_contexts_query_operation(messageInfo, printBuf);
            break;
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            basic_connect_provisioned_contexts_set_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
basic_connect_provisioned_contexts_query_operation(MbimMessageInfo *    messageInfo,
                                                   char *               printBuf) {
    startCommand(printBuf);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_provisioned_contexts_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, NULL, 0);
}

void
basic_connect_provisioned_contexts_set_operation(MbimMessageInfo *  messageInfo,
                                                 char *             printBuf) {
    int32_t bufferLen = 0;
    MbimSetProvisionedContext *data = (MbimSetProvisionedContext *)
            mbim_message_command_get_raw_information_buffer(messageInfo->message, &bufferLen);

    MbimSetProvisionedContext_2 *setProvisionedContext =
            (MbimSetProvisionedContext_2 *)calloc(1, sizeof(MbimSetProvisionedContext_2));
    setProvisionedContext->contextId = data->contextId;
    setProvisionedContext->contextType = mbim_uuid_to_context_type(&data->contextType);
    setProvisionedContext->compression = data->compression;
    setProvisionedContext->authProtocol = data->authProtocol;

    uint32_t accessStringOffset = G_STRUCT_OFFSET(MbimSetProvisionedContext, accessStringOffset);
    setProvisionedContext->accessString =
            mbim_message_read_string(messageInfo->message, 0, accessStringOffset);

    uint32_t usernameOffset = G_STRUCT_OFFSET(MbimSetProvisionedContext, usernameOffset);
    setProvisionedContext->username =
            mbim_message_read_string(messageInfo->message, 0, usernameOffset);

    uint32_t passwordOffset = G_STRUCT_OFFSET(MbimSetProvisionedContext, passwordOffset);
    setProvisionedContext->password =
            mbim_message_read_string(messageInfo->message, 0, passwordOffset);

    uint32_t providerIdOffset = G_STRUCT_OFFSET(MbimSetProvisionedContext, providerIdOffset);
    setProvisionedContext->providerId =
            mbim_message_read_string(messageInfo->message, 0, providerIdOffset);

    startCommand(printBuf);
    appendPrintBuf(printBuf, "%scontextId: %d, contextType: %s, accessString: %s,"
            " username: %s, password: %s, compression: %s, authProtocol: %s, providerId: %s",
            printBuf,
            setProvisionedContext->contextId,
            mbim_context_type_get_printable(setProvisionedContext->contextType),
            setProvisionedContext->accessString,
            setProvisionedContext->username,
            setProvisionedContext->password,
            mbim_compression_get_printable(setProvisionedContext->compression),
            mbim_auth_protocol_get_printable(setProvisionedContext->authProtocol),
            setProvisionedContext->providerId);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_provisioned_contexts_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, setProvisionedContext, sizeof(MbimSetProvisionedContext));

    memsetAndFreeStrings(4, setProvisionedContext->accessString,
            setProvisionedContext->username, setProvisionedContext->password,
            setProvisionedContext->providerId);
    free(setProvisionedContext);
}

int
basic_connect_provisioned_contexts_set_or_query_ready(Mbim_Token        token,
                                                      MbimStatusError   error,
                                                      void *            response,
                                                      size_t            responseLen,
                                                      char *            printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimProvisionedContextsInfo_2)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimProvisionedContextsInfo_2));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimProvisionedContextsInfo_2 *resp = (MbimProvisionedContextsInfo_2 *)response;

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%selementCount: %d",
            printBuf, resp->elementCount);

    uint32_t informationBufferSize[resp->elementCount];
    uint32_t informationBufferSizeInTotal = 0;

    MbimContext **context = (MbimContext **)calloc(resp->elementCount, sizeof(MbimContext *));
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        context[i] = provisioned_context_response_builder(resp->context[i],
                sizeof(MbimContext_2), &informationBufferSize[i], printBuf);
        informationBufferSizeInTotal += informationBufferSize[i];
    }

    informationBufferSizeInTotal += sizeof(MbimProvisionedContextsInfo) +
            sizeof(OffsetLengthPairList) * resp->elementCount;

    MbimProvisionedContextsInfo *provisionedContextsInfo = (MbimProvisionedContextsInfo *)
            calloc(1, informationBufferSizeInTotal);
    uint32_t dataBufferOffset = G_STRUCT_OFFSET(
            MbimProvisionedContextsInfo, provisionedContestRefList) +
            sizeof(OffsetLengthPairList) * resp->elementCount;
    provisionedContextsInfo->elementCount = resp->elementCount;
    uint32_t offset = dataBufferOffset;
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        provisionedContextsInfo->provisionedContestRefList[i].offset = offset;
        provisionedContextsInfo->provisionedContestRefList[i].length = informationBufferSize[i];
        offset += informationBufferSize[i];

        memcpy((uint8_t *)provisionedContextsInfo +
                provisionedContextsInfo->provisionedContestRefList[i].offset,
                (uint8_t *)(context[i]), informationBufferSize[i]);
    }

    closeResponse(printBuf);
    printResponse(printBuf);

    mbim_command_complete(token, error, provisionedContextsInfo,
            informationBufferSizeInTotal);
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        free(context[i]);
    }
    free(context);
    free(provisionedContextsInfo);

    return 0;
}

int
basic_connect_provisioned_contexts_notify(void *      data,
                                          size_t      dataLen,
                                          void **     response,
                                          size_t *    responseLen,
                                          char *      printBuf) {
    if (data == NULL || dataLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (dataLen != sizeof(MbimProvisionedContextsInfo_2)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)dataLen, (int)sizeof(MbimProvisionedContextsInfo_2));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimProvisionedContextsInfo_2 *resp = (MbimProvisionedContextsInfo_2 *)data;

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%selementCount: %d",
            printBuf, resp->elementCount);

    uint32_t informationBufferSize[resp->elementCount];
    uint32_t informationBufferSizeInTotal = 0;

    MbimContext **context = (MbimContext **)calloc(resp->elementCount, sizeof(MbimContext *));
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        context[i] = provisioned_context_response_builder(resp->context[i],
                sizeof(MbimContext_2), &informationBufferSize[i], printBuf);
        informationBufferSizeInTotal += informationBufferSize[i];
    }

    informationBufferSizeInTotal += sizeof(MbimProvisionedContextsInfo) +
            sizeof(OffsetLengthPairList) * resp->elementCount;

    MbimProvisionedContextsInfo *provisionedContextsInfo = (MbimProvisionedContextsInfo *)
            calloc(1, informationBufferSizeInTotal);
    uint32_t dataBufferOffset = G_STRUCT_OFFSET(MbimProvisionedContextsInfo, provisionedContestRefList) +
            sizeof(OffsetLengthPairList) * resp->elementCount;
    provisionedContextsInfo->elementCount = resp->elementCount;
    uint32_t offset = dataBufferOffset;
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        provisionedContextsInfo->provisionedContestRefList[i].offset = offset;
        provisionedContextsInfo->provisionedContestRefList[i].length = informationBufferSize[i];
        offset += informationBufferSize[i];

        memcpy((uint8_t *)provisionedContextsInfo + provisionedContextsInfo->provisionedContestRefList[i].offset,
                (uint8_t *)(context[i]), informationBufferSize[i]);
    }

    closeResponse(printBuf);
    printResponse(printBuf);

    *response = provisionedContextsInfo;
    *responseLen = informationBufferSizeInTotal;

    return 0;
}

MbimContext *
provisioned_context_response_builder(void *         response,
                                     size_t         responseLen,
                                     uint32_t *     outSize,
                                     char *         printBuf) {
    MBIM_UNUSED_PARM(responseLen);

    MbimContext_2 *resp = (MbimContext_2 *)response;

    appendPrintBuf(printBuf, "%s[contextId: %d, contextType: %s, "
            "accessString: %s, username: %s, password: %s, compression: %s, "
            "authProtocol: %s]", printBuf, resp->contextId,
            mbim_context_type_get_printable(resp->contextType),
            resp->accessString, resp->username, resp->password,
            mbim_compression_get_printable(resp->compression),
            mbim_auth_protocol_get_printable(resp->authProtocol));

    /* accessString */
    uint32_t accessStringSize = 0;
    uint32_t accessStringPadSize = 0;
    unichar2 *accessStringInUtf16 = NULL;
    if (resp->accessString == NULL || strlen(resp->accessString) == 0) {
        accessStringSize = 0;
    } else {
        accessStringSize = strlen(resp->accessString) * 2;
        accessStringPadSize = PAD_SIZE(accessStringSize);
        accessStringInUtf16 = (unichar2 *)calloc
                (accessStringPadSize / sizeof(unichar2) + 1, sizeof(unichar2));
        utf8_to_utf16(accessStringInUtf16, resp->accessString,
                accessStringSize / 2, accessStringSize / 2);
    }

    /* username */
    uint32_t usernameSize = 0;
    uint32_t usernamePadSize = 0;
    unichar2 *usernameInUtf16 = NULL;
    if (resp->username == NULL || strlen(resp->username) == 0) {
        usernameSize = 0;
    } else {
        usernameSize = strlen(resp->username) * 2;
        usernamePadSize = PAD_SIZE(usernameSize);
        usernameInUtf16 = (unichar2 *)calloc
                (usernamePadSize / sizeof(unichar2) + 1, sizeof(unichar2));
        utf8_to_utf16(usernameInUtf16, resp->username,
                usernameSize / 2, usernameSize / 2);
    }

    /* password */
    uint32_t passwordSize = 0;
    uint32_t passwordPadSize = 0;
    unichar2 *passwordInUtf16 = NULL;
    if (resp->password == NULL || strlen(resp->password) == 0) {
        passwordSize = 0;
    } else {
        passwordSize = strlen(resp->password) * 2;
        passwordPadSize = PAD_SIZE(passwordSize);
        passwordInUtf16 = (unichar2 *)calloc
                (passwordPadSize / sizeof(unichar2) + 1, sizeof(uint8_t));
        utf8_to_utf16(passwordInUtf16, resp->password,
                passwordSize / 2, passwordSize / 2);
    }


    uint32_t informationBufferSize = sizeof(MbimContext) + accessStringPadSize +
            usernamePadSize + passwordPadSize;
    uint32_t dataBufferOffset = G_STRUCT_OFFSET(MbimContext, dataBuffer);

    MbimContext *context = (MbimContext *)calloc(1, informationBufferSize);
    context->contextId = resp->contextId;
    context->contextType = *mbim_uuid_from_context_type(resp->contextType);
    context->compression = resp->compression;
    context->authProtocol = resp->authProtocol;
    context->accessStringOffset = dataBufferOffset;
    context->accessStringSize = accessStringSize;
    context->usernameOffset = context->accessStringOffset + accessStringPadSize;
    context->usernameSize = usernameSize;
    context->passwordOffset = context->usernameOffset + usernamePadSize;
    context->passwordSize = passwordSize;

    if (context->accessStringSize != 0) {
        memcpy((uint8_t *)context + context->accessStringOffset,
                (uint8_t *)accessStringInUtf16, accessStringPadSize);
    }
    if (context->usernameSize != 0) {
        memcpy((uint8_t *)context + context->usernameOffset,
                (uint8_t *)usernameInUtf16, usernamePadSize);
    }
    if (context->passwordSize != 0) {
        memcpy((uint8_t *)context + context->passwordOffset,
                (uint8_t *)passwordInUtf16, passwordPadSize);
    }

    memsetAndFreeUnichar2Arrays(3, accessStringInUtf16, usernameInUtf16, passwordInUtf16);

    *outSize = informationBufferSize;
    return context;
}

/******************************************************************************/
/**
 * basic_connect_service_activiation: set only
 *
 * set:
 *  @command:   the informationBuffer shall be MbimSetServiceActivation
 *  @response:  the informationBuffer shall be MbimServiceActivationInfo
 *
 */
int
mbim_message_parser_basic_connect_service_activiation(MbimMessageInfo * messageInfo,
                                                      char *            printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            basic_connect_service_activiation_set_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
basic_connect_service_activiation_set_operation(MbimMessageInfo *   messageInfo,
                                                char *              printBuf) {
    int32_t bufferLen = 0;
    void *vendorSpecificBuffer = (void *)
            mbim_message_command_get_raw_information_buffer(messageInfo->message, &bufferLen);

    startCommand(printBuf);
    appendPrintBuf(printBuf, "%svendorSpecificBuffer: %s", printBuf, vendorSpecificBuffer);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_service_activiation_set_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, vendorSpecificBuffer, bufferLen);
}

int
basic_connect_service_activiation_set_ready(Mbim_Token          token,
                                            MbimStatusError     error,
                                            void *              response,
                                            size_t              responseLen,
                                            char *              printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimServiceActivationInfo_2)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimServiceActivationInfo_2));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimServiceActivationInfo_2 *resp = (MbimServiceActivationInfo_2 *)response;

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%snwError: %s, vendorSpecificBuffer: %s",
            printBuf, mbim_network_error_get_printable(resp->nwError),
            resp->vendorSpecificBuffer);
    closeResponse(printBuf);
    printResponse(printBuf);

    uint32_t vendorSpecificBufferSize =
            resp->vendorSpecificBuffer == NULL ? 0 : strlen(resp->vendorSpecificBuffer);
    MbimServiceActivationInfo *serviceActivationInfo = (MbimServiceActivationInfo *)
            calloc(1, sizeof(MbimServiceActivationInfo) + vendorSpecificBufferSize);

    serviceActivationInfo->nwError = resp->nwError;
    memcpy((uint8_t *)serviceActivationInfo->dataBuffer,
            (uint8_t *)resp->vendorSpecificBuffer, vendorSpecificBufferSize);

    mbim_command_complete(token, error, serviceActivationInfo,
            sizeof(MbimServiceActivationInfo) + vendorSpecificBufferSize);
    free(serviceActivationInfo);

    return 0;
}

/******************************************************************************/
/**
 * basic_connect_ip_configuration: query and notification supported
 *
 * query:
 *  @command:   the informationBuffer shall be MbimIPConfigurationInfo
 *  @response:  the informationBuffer shall be MbimIPConfigurationInfo
 *
 * notification:
 *  @command:   NA
 *  @response:  the informationBuffer shall be MbimIPConfigurationInfo
 *
 * Note:
 *  for query, the only relevant field of MbimIPConfigurationInfo in command's
 *  informationBuffer is the sessionId.
 */
int
mbim_message_parser_basic_connect_ip_configuration(MbimMessageInfo *    messageInfo,
                                                   char *               printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            basic_connect_ip_configuration_query_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
basic_connect_ip_configuration_query_operation(MbimMessageInfo *    messageInfo,
                                               char *               printBuf) {
    int32_t bufferLen = 0;
    MbimIPConfigurationInfo *IPConfigurationInfo = (MbimIPConfigurationInfo *)
            mbim_message_command_get_raw_information_buffer(messageInfo->message, &bufferLen);

    startCommand(printBuf);
    appendPrintBuf(printBuf, "%ssessionId: %d", printBuf, IPConfigurationInfo->sessionId);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_ip_configuration_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, &(IPConfigurationInfo->sessionId), sizeof(uint32_t));
}

int
basic_connect_ip_configuration_query_ready(Mbim_Token           token,
                                           MbimStatusError      error,
                                           void *               response,
                                           size_t               responseLen,
                                           char *               printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimIPConfigurationInfo_2)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimIPConfigurationInfo_2));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    startResponse(printBuf);

    uint32_t informationBufferSize = 0;
    MbimIPConfigurationInfo *IPConfigurationInfo =
            ip_configuration_response_builder(response, responseLen, &informationBufferSize, printBuf);

    closeResponse(printBuf);
    printResponse(printBuf);

    mbim_command_complete(token, error, IPConfigurationInfo, informationBufferSize);
    free(IPConfigurationInfo);

    return 0;
}

int
basic_connect_ip_configuration_notify(void *      data,
                                      size_t      dataLen,
                                      void **     response,
                                      size_t *    responseLen,
                                      char *      printBuf) {
    if (data == NULL || dataLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (dataLen != sizeof(MbimIPConfigurationInfo_2)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)dataLen, (int)sizeof(MbimIPConfigurationInfo_2));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    startResponse(printBuf);

    uint32_t informationBufferSize = 0;
    MbimIPConfigurationInfo *IPConfigurationInfo =
            ip_configuration_response_builder(data, dataLen, &informationBufferSize, printBuf);

    closeResponse(printBuf);
    printResponse(printBuf);

    *response = IPConfigurationInfo;
    *responseLen = informationBufferSize;

    return 0;
}

MbimIPConfigurationInfo *
ip_configuration_response_builder(void *         response,
                                  size_t         responseLen,
                                  uint32_t *     outSize,
                                  char *         printBuf) {
    MBIM_UNUSED_PARM(responseLen);

    MbimIPConfigurationInfo_2 *resp = (MbimIPConfigurationInfo_2 *)response;

    appendPrintBuf(printBuf, "%ssessionId: %d, IPv4ConfigurationAvailable: %d,"
            " IPv6ConfigurationAvailable: %d, IPv4AddressCount: %d, IPv4Element: ",
            printBuf, resp->sessionId, resp->IPv4ConfigurationAvailable,
            resp->IPv6ConfigurationAvailable, resp->IPv4AddressCount);
    for (uint32_t i = 0; i < resp->IPv4AddressCount; i++) {
        appendPrintBuf(printBuf, "%s%d/%s,", printBuf,
                resp->IPv4Element[i]->onLinkPrefixLength,
                resp->IPv4Element[i]->IPv4Address);
    }
    appendPrintBuf(printBuf, "%sIPv6AddressCount: %d, IPv6Element: ",
            printBuf, resp->IPv6AddressCount);
    for (uint32_t i = 0; i < resp->IPv6AddressCount; i++) {
        appendPrintBuf(printBuf, "%s%d/%s,", printBuf,
                resp->IPv6Element[i]->onLinkPrefixLength,
                resp->IPv6Element[i]->IPv6Address);
    }
    appendPrintBuf(printBuf, "%sIPv4Gateway: %s, IPv6Gateway: %s, "
            "IPv4DnsServerCount: %d, IPv4DnsServer: ", printBuf,
            resp->IPv4Gateway, resp->IPv6Gateway, resp->IPv4DnsServerCount);
    for (uint32_t i = 0; i < resp->IPv4DnsServerCount; i++) {
        appendPrintBuf(printBuf, "%s%s,", printBuf, resp->IPv4DnsServer[i]);
    }
    appendPrintBuf(printBuf, "%sIPv6DnsServerCount: %d, IPv6DnsServer: ",
            printBuf, resp->IPv6DnsServerCount);
    for (uint32_t i = 0; i < resp->IPv6DnsServerCount; i++) {
        appendPrintBuf(printBuf, "%s%s,", printBuf, resp->IPv6DnsServer[i]);
    }
    appendPrintBuf(printBuf, "%sIPv4Mtu: %d, IPv6Mtu: %d", printBuf,
            resp->IPv4Mtu, resp->IPv6Mtu);

    MbimIPConfigurationInfo *IPConfigurationInfo = (MbimIPConfigurationInfo *)
            calloc(1, sizeof(MbimIPConfigurationInfo) +
                      sizeof(MbimIPv4Element) * resp->IPv4AddressCount +
                      sizeof(MbimIPv6Element) * resp->IPv6AddressCount +
                      /* IPv4Gateway, IPv6Gateway */
                      4 + 16 + 4 * resp->IPv4DnsServerCount +
                      16 * resp->IPv6DnsServerCount);

    uint32_t dataBufferOffset = G_STRUCT_OFFSET(MbimIPConfigurationInfo, dataBuffer);

    IPConfigurationInfo->sessionId = resp->sessionId;
    IPConfigurationInfo->IPv4ConfigurationAvailable = resp->IPv4ConfigurationAvailable;
    IPConfigurationInfo->IPv6ConfigurationAvailable = resp->IPv6ConfigurationAvailable;
    IPConfigurationInfo->IPv4AddressCount = resp->IPv4AddressCount;
    IPConfigurationInfo->IPv6AddressCount = resp->IPv6AddressCount;
    IPConfigurationInfo->IPv4DnsServerCount = resp->IPv4DnsServerCount;
    IPConfigurationInfo->IPv6DnsServerCount = resp->IPv6DnsServerCount;
    IPConfigurationInfo->IPv4Mtu = resp->IPv4Mtu;
    IPConfigurationInfo->IPv6Mtu = resp->IPv6Mtu;

    uint32_t offsetValue = dataBufferOffset;
    /* IPv4Address */
    if (resp->IPv4Element != NULL && resp->IPv4AddressCount != 0) {
        IPConfigurationInfo->IPv4AddressOffset = dataBufferOffset;
        offsetValue += sizeof(MbimIPv4Element) * IPConfigurationInfo->IPv4AddressCount;
        MbimIPv4Element IPv4Elements[resp->IPv4AddressCount];
        for (uint32_t i = 0; i < resp->IPv4AddressCount; i++) {
            IPv4Elements[i].onLinkPrefixLength = resp->IPv4Element[i]->onLinkPrefixLength;
            inet_pton(AF_INET, resp->IPv4Element[i]->IPv4Address, IPv4Elements[i].IPv4Address.addr);

            memcpy((uint8_t *)IPConfigurationInfo + IPConfigurationInfo->IPv4AddressOffset +
                    sizeof(MbimIPv4Element) * i, (uint8_t *)(&IPv4Elements[i]), sizeof(MbimIPv4Element));
        }
    } else {
        IPConfigurationInfo->IPv4AddressOffset = 0;
    }

    /* IPv6Address */
    if (resp->IPv6Element != NULL && resp->IPv6AddressCount != 0) {
        IPConfigurationInfo->IPv6AddressOffset = offsetValue;
        offsetValue += sizeof(MbimIPv6Element) * IPConfigurationInfo->IPv6AddressCount;
        MbimIPv6Element IPv6Elements[resp->IPv6AddressCount];
        for (uint32_t i = 0; i < resp->IPv6AddressCount; i++) {
            IPv6Elements[i].onLinkPrefixLength = resp->IPv6Element[i]->onLinkPrefixLength;
            inet_pton(AF_INET6, resp->IPv6Element[i]->IPv6Address, IPv6Elements[i].IPv6Address.addr);

            memcpy((uint8_t *)IPConfigurationInfo + IPConfigurationInfo->IPv6AddressOffset +
                    sizeof(MbimIPv6Element) * i, (uint8_t *)&IPv6Elements[i], sizeof(MbimIPv6Element));
        }
    } else {
        IPConfigurationInfo->IPv6AddressOffset = 0;
    }

    /* IPv4Gateway */
    if (resp->IPv4Gateway != NULL) {
        IPConfigurationInfo->IPv4GatewayOffset = offsetValue;
        offsetValue += 4;
        uint8_t IPv4Gateway[4];
        inet_pton(AF_INET, resp->IPv4Gateway, IPv4Gateway);
        memcpy((uint8_t *)IPConfigurationInfo + IPConfigurationInfo->IPv4GatewayOffset,
                (uint8_t *)IPv4Gateway, 4);
    } else {
        IPConfigurationInfo->IPv4GatewayOffset = 0;
    }

    /* IPv6Gateway */
    if (resp->IPv6Gateway != NULL) {
        IPConfigurationInfo->IPv6GatewayOffset = offsetValue;
        offsetValue += 16;
        uint8_t IPv6Gateway[16];
        inet_pton(AF_INET6, resp->IPv6Gateway, IPv6Gateway);
        memcpy((uint8_t *)IPConfigurationInfo + IPConfigurationInfo->IPv6GatewayOffset,
                (uint8_t *)IPv6Gateway, 16);
    } else {
        IPConfigurationInfo->IPv6GatewayOffset = 0;
    }

    /* IPv4DnsServer */
    if (resp->IPv4DnsServer != NULL && resp->IPv4DnsServerCount != 0) {
        IPConfigurationInfo->IPv4DnsServerOffset = offsetValue;
        offsetValue += 4 * IPConfigurationInfo->IPv4DnsServerCount;
        uint8_t IPv4DnsServers[resp->IPv4DnsServerCount][4];
        for (uint32_t i = 0; i < resp->IPv4DnsServerCount; i++) {
            inet_pton(AF_INET, resp->IPv4DnsServer[i], IPv4DnsServers[i]);
            memcpy((uint8_t *)IPConfigurationInfo + IPConfigurationInfo->IPv4DnsServerOffset +
                    4 * i, (uint8_t *)(IPv4DnsServers[i]), 4);
        }
    } else {
        IPConfigurationInfo->IPv4DnsServerOffset = 0;
    }

    /* IPv6DnsServer */
    if (resp->IPv6DnsServer != NULL && resp->IPv6DnsServerCount != 0) {
        IPConfigurationInfo->IPv6DnsServerOffset = offsetValue;
        offsetValue += 16 * IPConfigurationInfo->IPv6DnsServerCount;
        uint8_t IPv6DnsServers[resp->IPv6DnsServerCount][16];
        for (uint32_t i = 0; i < resp->IPv6DnsServerCount; i++) {
            inet_pton(AF_INET, resp->IPv6DnsServer[i], IPv6DnsServers[i]);
            memcpy((uint8_t *)IPConfigurationInfo + IPConfigurationInfo->IPv6DnsServerOffset +
                    16 * i, (uint8_t *)(IPv6DnsServers[i]), 16);
        }
    } else {
        IPConfigurationInfo->IPv6DnsServerOffset = 0;
    }

    *outSize = offsetValue;
    return IPConfigurationInfo;
}

/******************************************************************************/
/**
 * basic_connect_device_services: query only
 *
 * query:
 *  @command:   the informationBuffer shall be null
 *  @response:  the informationBuffer shall be MbimDeviceServicesInfo
 *
 */
int
mbim_message_parser_basic_connect_device_services(MbimMessageInfo * messageInfo,
                                                  char *            printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            basic_connect_device_services_query_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
basic_connect_device_services_query_operation(MbimMessageInfo * messageInfo,
                                              char *            printBuf) {
    startCommand(printBuf);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_device_services_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, NULL, 0);
}

int
basic_connect_device_services_query_ready(Mbim_Token            token,
                                          MbimStatusError       error,
                                          void *                response,
                                          size_t                responseLen,
                                          char *                printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimDeviceServicesInfo_2)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimDeviceServicesInfo_2));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimDeviceServicesInfo_2 *resp = (MbimDeviceServicesInfo_2 *)response;

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%sdeviceServicesCount: %d, maxDssSessions: %d",
            printBuf, resp->deviceServicesCount, resp->maxDssSessions);
    for (uint32_t i = 0; i < resp->deviceServicesCount; i++) {
        appendPrintBuf(printBuf, "%s\n[deviceServiceId: %s, dssPayload: %d,"
                " maxDssInstances: %d, cidCount: %d, cids:\n", printBuf,
                mbim_service_get_printable(resp->deviceServiceElements[i]->deviceServiceId),
                resp->deviceServiceElements[i]->dssPayload,
                resp->deviceServiceElements[i]->maxDssInstances,
                resp->deviceServiceElements[i]->cidCount);
        for (uint32_t j = 0; j < resp->deviceServiceElements[i]->cidCount; j++) {
            appendPrintBuf(printBuf, "%s    %s\n", printBuf,
                    mbim_cid_get_printable(resp->deviceServiceElements[i]->deviceServiceId,
                            resp->deviceServiceElements[i]->cids[j]));
        }
    }
    closeResponse(printBuf);
    printResponse(printBuf);

    uint32_t informationBufferSize[resp->deviceServicesCount];
    uint32_t informationBufferSizeInTotal = 0;
    MbimDeviceServiceElement **deviceServiceElements = (MbimDeviceServiceElement **)
            calloc(resp->deviceServicesCount, sizeof(MbimDeviceServiceElement *));
    for (uint32_t i = 0; i < resp->deviceServicesCount; i++) {
        deviceServiceElements[i] = device_services_response_builder(resp->deviceServiceElements[i],
                sizeof(MbimDeviceServiceElement_2), &informationBufferSize[i]);
        informationBufferSizeInTotal += informationBufferSize[i];
    }

    informationBufferSizeInTotal += sizeof(MbimDeviceServicesInfo) +
            sizeof(OffsetLengthPairList) * resp->deviceServicesCount;

    MbimDeviceServicesInfo *deviceServicesInfo = (MbimDeviceServicesInfo *)
            calloc(1, informationBufferSizeInTotal);

    uint32_t dataBufferOffset = G_STRUCT_OFFSET(MbimDeviceServicesInfo, deviceServicesRefList) +
            sizeof(OffsetLengthPairList) * resp->deviceServicesCount;
    deviceServicesInfo->deviceServicesCount = resp->deviceServicesCount;
    deviceServicesInfo->maxDssSessions = resp->maxDssSessions;
    uint32_t offset = dataBufferOffset;
    for (uint32_t i = 0; i < resp->deviceServicesCount; i++) {
        deviceServicesInfo->deviceServicesRefList[i].offset = offset;
        deviceServicesInfo->deviceServicesRefList[i].length = informationBufferSize[i];
        offset += informationBufferSize[i];

        memcpy((uint8_t *)deviceServicesInfo + deviceServicesInfo->deviceServicesRefList[i].offset,
                (uint8_t *)(deviceServiceElements[i]), informationBufferSize[i]);
    }

    mbim_command_complete(token, error, deviceServicesInfo, informationBufferSizeInTotal);
    for (uint32_t i = 0; i < resp->deviceServicesCount; i++) {
        free(deviceServiceElements[i]);
    }
    free(deviceServiceElements);
    free(deviceServicesInfo);

    return 0;
}

MbimDeviceServiceElement *
device_services_response_builder(void *         response,
                                 size_t         responseLen,
                                 uint32_t *     outSize) {
    MBIM_UNUSED_PARM(responseLen);

    MbimDeviceServiceElement_2 *resp = (MbimDeviceServiceElement_2 *)response;

    uint32_t informationBufferSize = sizeof(MbimDeviceServiceElement) +
            resp->cidCount * sizeof(uint32_t);

    uint32_t dataBufferOffset = G_STRUCT_OFFSET(MbimDeviceServiceElement, cids);

    MbimDeviceServiceElement *deviceServiceElement =
            (MbimDeviceServiceElement *)calloc(1, informationBufferSize);
    deviceServiceElement->deviceServiceId = *mbim_uuid_from_service(resp->deviceServiceId);
    deviceServiceElement->dssPayload = resp->dssPayload;
    deviceServiceElement->maxDssInstances = resp->maxDssInstances;
    deviceServiceElement->cidCount = resp->cidCount;
    memcpy((uint8_t *)(deviceServiceElement->cids), (uint8_t *)(resp->cids),
            sizeof(uint32_t) * resp->cidCount);

    *outSize = informationBufferSize;
    return deviceServiceElement;
}

/******************************************************************************/
/**
 * basic_connect_device_service_subscribe_list: set only
 *
 * set:
 *  @command:   the informationBuffer shall be MbimDeviceServiceSubscribeList
 *  @response:  the informationBuffer shall be MbimDeviceServiceSubscribeList
 *
 */
int
mbim_message_parser_basic_connect_device_service_subscribe_list(MbimMessageInfo * messageInfo,
                                                                char *            printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            basic_connect_device_service_subscribe_list_set_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
basic_connect_device_service_subscribe_list_set_operation(MbimMessageInfo * messageInfo,
                                                          char *            printBuf) {
    int32_t bufferLen = 0;
    MbimDeviceServiceSubscribeList *data = (MbimDeviceServiceSubscribeList *)
            mbim_message_command_get_raw_information_buffer(messageInfo->message, &bufferLen);

    uint32_t information_buffer_offset = mbim_message_get_information_buffer_offset(messageInfo->message);
    uint32_t elementCount = data->elementCount;

    MbimDeviceServiceSubscribeList_2 *deviceServiceSubscribeList =
            (MbimDeviceServiceSubscribeList_2 *)calloc(1, sizeof(MbimDeviceServiceSubscribeList_2));

    deviceServiceSubscribeList->elementCount = elementCount;
    deviceServiceSubscribeList->eventEntries = (MbimEventEntry_2 **)
            calloc(elementCount, sizeof(MbimEventEntry_2 *));

    startCommand(printBuf);
    appendPrintBuf(printBuf, "%selementCount: %d\n", printBuf, elementCount);

    for (uint32_t i = 0; i < elementCount; i++) {
        deviceServiceSubscribeList->eventEntries[i] =
                (MbimEventEntry_2 *)calloc(1, sizeof(MbimEventEntry_2));

        uint32_t offset = data->deviceServiceSubscirbeRefList[i].offset;

        MbimEventEntry *eventEntry = (MbimEventEntry *)
                G_STRUCT_MEMBER_P(messageInfo->message->data, (information_buffer_offset + offset));

        deviceServiceSubscribeList->eventEntries[i]->deviceServiceId =
                mbim_uuid_to_service(&(eventEntry->deviceServiceId));
        deviceServiceSubscribeList->eventEntries[i]->cidCount = eventEntry->cidCount;
        deviceServiceSubscribeList->eventEntries[i]->cidList = eventEntry->cids;

        appendPrintBuf(printBuf, "%sserviceId: %s, cidCount: %d, cid:\n", printBuf,
                mbim_service_get_printable(deviceServiceSubscribeList->eventEntries[i]->deviceServiceId),
                deviceServiceSubscribeList->eventEntries[i]->cidCount);

        for (uint32_t count = 0; count < eventEntry->cidCount; count++) {
            appendPrintBuf(printBuf, "%s    %s\n", printBuf, mbim_cid_get_printable(
                    deviceServiceSubscribeList->eventEntries[i]->deviceServiceId, eventEntry->cids[count]));
        }
    }

    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_device_service_subscribe_list_set_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, deviceServiceSubscribeList,
            sizeof(MbimDeviceServiceSubscribeList_2));

    for (uint32_t i = 0; i < elementCount; i++) {
        free(deviceServiceSubscribeList->eventEntries[i]);
    }
    free(deviceServiceSubscribeList);
}

int
basic_connect_device_service_subscribe_list_set_ready(Mbim_Token        token,
                                                      MbimStatusError   error,
                                                      void *            response,
                                                      size_t            responseLen,
                                                      char *            printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimDeviceServiceSubscribeList_2)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimDeviceServiceSubscribeList_2));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimDeviceServiceSubscribeList_2 *resp = (MbimDeviceServiceSubscribeList_2 *)response;

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%selementCount: %d, eventEntries:\n",
            printBuf, resp->elementCount);
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        appendPrintBuf(printBuf, "%sdeviceServiceId: %s, cidCount: %d, cids:\n",
                printBuf,
                mbim_service_get_printable(resp->eventEntries[i]->deviceServiceId),
                resp->eventEntries[i]->cidCount);
        for (uint32_t j = 0; j < resp->eventEntries[i]->cidCount; j++) {
            appendPrintBuf(printBuf, "%s    %s\n", printBuf,
                    mbim_cid_get_printable(resp->eventEntries[i]->deviceServiceId,
                            resp->eventEntries[i]->cidList[j]));
        }
    }
    closeResponse(printBuf);
    printResponse(printBuf);

    uint32_t informationBufferSize[resp->elementCount];
    uint32_t informationBufferSizeInTotal = 0;
    MbimEventEntry **eventEntries = (MbimEventEntry **)
            calloc(resp->elementCount, sizeof(MbimEventEntry *));
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        eventEntries[i] = device_service_subscribe_list_response_builder(resp->eventEntries[i],
                sizeof(MbimEventEntry_2), &informationBufferSize[i]);
        informationBufferSizeInTotal += informationBufferSize[i];
    }

    informationBufferSizeInTotal += sizeof(MbimDeviceServiceSubscribeList) +
            sizeof(OffsetLengthPairList) * resp->elementCount;

    MbimDeviceServiceSubscribeList *deviceServiceSubscribeList =
            (MbimDeviceServiceSubscribeList *)calloc(1, informationBufferSizeInTotal);

    uint32_t dataBufferOffset = G_STRUCT_OFFSET(MbimDeviceServiceSubscribeList, deviceServiceSubscirbeRefList) +
            sizeof(OffsetLengthPairList) * resp->elementCount;
    deviceServiceSubscribeList->elementCount = resp->elementCount;
    uint32_t offset = dataBufferOffset;
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        deviceServiceSubscribeList->deviceServiceSubscirbeRefList[i].offset = offset;
        deviceServiceSubscribeList->deviceServiceSubscirbeRefList[i].length = informationBufferSize[i];
        offset += informationBufferSize[i];

        memcpy((uint8_t *)deviceServiceSubscribeList + deviceServiceSubscribeList->deviceServiceSubscirbeRefList[i].offset,
                (uint8_t *)(eventEntries[i]), informationBufferSize[i]);
    }

    mbim_command_complete(token, error, deviceServiceSubscribeList, informationBufferSizeInTotal);
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        free(eventEntries[i]);
    }
    free(eventEntries);
    free(deviceServiceSubscribeList);

    return 0;
}

MbimEventEntry *
device_service_subscribe_list_response_builder(void *       response,
                                               size_t       responseLen,
                                               uint32_t *   outSize) {
    MBIM_UNUSED_PARM(responseLen);

    MbimEventEntry_2 *resp = (MbimEventEntry_2 *)response;

    uint32_t informationBufferSize = sizeof(MbimEventEntry) +
            resp->cidCount * sizeof(uint32_t);

    uint32_t dataBufferOffset = G_STRUCT_OFFSET(MbimEventEntry, cids);

    MbimEventEntry *eventEntry =
            (MbimEventEntry *)calloc(1, informationBufferSize);
    eventEntry->deviceServiceId = *mbim_uuid_from_service(resp->deviceServiceId);
    eventEntry->cidCount = resp->cidCount;
    memcpy((uint8_t *)(eventEntry->cids), (uint8_t *)(resp->cidList),
            sizeof(uint32_t) * resp->cidCount);

    *outSize = informationBufferSize;
    return eventEntry;
}

/******************************************************************************/
/**
 * basic_connect_packet_statistics: query only
 *
 * query:
 *  @command:   the informationBuffer shall be null
 *  @response:  the informationBuffer shall be MbimPacketStatisticsInfo
 *
 */
int
mbim_message_parser_basic_connect_packet_statistics(MbimMessageInfo *   messageInfo,
                                                    char *              printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            basic_connect_packet_statistics_query_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
basic_connect_packet_statistics_query_operation(MbimMessageInfo *   messageInfo,
                                                char *              printBuf) {
    startCommand(printBuf);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_packet_statistics_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, NULL, 0);
}

int
basic_connect_packet_statistics_query_ready(Mbim_Token          token,
                                            MbimStatusError     error,
                                            void *              response,
                                            size_t              responseLen,
                                            char *              printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimPacketStatisticsInfo)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimPacketStatisticsInfo));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimPacketStatisticsInfo *packetStatisticsInfo =
            (MbimPacketStatisticsInfo *)response;

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%sinDiscards: %d, inErrors: %d, inOctets: %lld,"
            " inPackets: %lld, outOctets: %lld, outPackets: %lld, outErrors: %d, "
            "outDiscards: %d", printBuf, packetStatisticsInfo->inDiscards,
            packetStatisticsInfo->inErrors, packetStatisticsInfo->inOctets,
            packetStatisticsInfo->inPackets, packetStatisticsInfo->outOctets,
            packetStatisticsInfo->outPackets, packetStatisticsInfo->outErrors,
            packetStatisticsInfo->outDiscards);
    closeResponse(printBuf);
    printResponse(printBuf);

    mbim_command_complete(token, error, response, responseLen);

    return 0;
}

/******************************************************************************/
/**
 * basic_connect_network_idle_hint: set and query supported
 *
 * query:
 *  @command:   the informationBuffer shall be null
 *  @response:  the informationBuffer shall be MbimNetworkIdleHint
 *
 * set:
 *  @command:   the informationBuffer shall be MbimNetworkIdleHint
 *  @response:  the informationBuffer shall be MbimNetworkIdleHint
 */
int
mbim_message_parser_basic_connect_network_idle_hint(MbimMessageInfo * messageInfo,
                                                    char *            printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            basic_connect_network_idle_hint_query_operation(messageInfo, printBuf);
            break;
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            basic_connect_network_idle_hint_set_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
basic_connect_network_idle_hint_query_operation(MbimMessageInfo * messageInfo,
                                                char *            printBuf) {
    int32_t bufferLen = 0;
    MbimNetworkIdleHint *data = (MbimNetworkIdleHint *)
            mbim_message_command_get_raw_information_buffer(messageInfo->message, &bufferLen);

    startCommand(printBuf);
    appendPrintBuf(printBuf, "%snetworkIdleHintState: %s", printBuf,
            mbim_network_idle_hint_state_get_printable(data->networkIdleHintState));
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_network_idle_hint_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, data, sizeof(MbimNetworkIdleHint));
}

void
basic_connect_network_idle_hint_set_operation(MbimMessageInfo * messageInfo,
                                              char *            printBuf) {
    int32_t bufferLen = 0;
    MbimNetworkIdleHint *data = (MbimNetworkIdleHint *)
            mbim_message_command_get_raw_information_buffer(messageInfo->message, &bufferLen);

    startCommand(printBuf);
    appendPrintBuf(printBuf, "%snetworkIdleHintState: %s", printBuf,
            mbim_network_idle_hint_state_get_printable(data->networkIdleHintState));
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_network_idle_hint_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, data, sizeof(MbimNetworkIdleHint));
}

int
basic_connect_network_idle_hint_set_or_query_ready(Mbim_Token           token,
                                                   MbimStatusError      error,
                                                   void *               response,
                                                   size_t               responseLen,
                                                   char *               printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimNetworkIdleHint)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimNetworkIdleHint));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimNetworkIdleHint *networkIdleHint = (MbimNetworkIdleHint *)response;

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%snetworkIdleHintState: %s", printBuf,
            mbim_network_idle_hint_state_get_printable(networkIdleHint->networkIdleHintState));
    closeResponse(printBuf);
    printResponse(printBuf);

    mbim_command_complete(token, error, response, responseLen);

    return 0;
}
/******************************************************************************/
/**
 * basic_connect_emergency_mode: query and notification supported
 *
 * query:
 *  @command:   the informationBuffer shall be null
 *  @response:  the informationBuffer shall be MbimEmergencyModeInfo
 *
 * notification:
 *  @command:   NA
 *  @response:  the informationBuffer shall be MbimEmergencyModeInfo
 */
int
mbim_message_parser_basic_connect_emergency_mode(MbimMessageInfo *  messageInfo,
                                                 char *             printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            basic_connect_emergency_mode_query_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
basic_connect_emergency_mode_query_operation(MbimMessageInfo *  messageInfo,
                                             char *             printBuf) {
    startCommand(printBuf);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_emergency_mode_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, NULL, 0);
}

int
basic_connect_emergency_mode_query_ready(Mbim_Token         token,
                                         MbimStatusError    error,
                                         void *             response,
                                         size_t             responseLen,
                                         char *             printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimEmergencyModeInfo)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimEmergencyModeInfo));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimEmergencyModeInfo *emergencyModeInfo = (MbimEmergencyModeInfo *)response;

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%semergencyMode: %s", printBuf,
            mbim_emergency_mode_state_get_printable(emergencyModeInfo->emergencyMode));
    closeResponse(printBuf);
    printResponse(printBuf);

    mbim_command_complete(token, error, response, responseLen);

    return 0;
}

int
basic_connect_emergency_mode_notify(void *      data,
                                    size_t      dataLen,
                                    void **     response,
                                    size_t *    responseLen,
                                    char *      printBuf) {
    if (data == NULL || dataLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (dataLen != sizeof(MbimEmergencyModeInfo)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)dataLen, (int)sizeof(MbimEmergencyModeInfo));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimEmergencyModeInfo *emergencyModeInfo =
            (MbimEmergencyModeInfo *)calloc(1, sizeof(MbimEmergencyModeInfo));
    memcpy(emergencyModeInfo, (MbimEmergencyModeInfo *)data, dataLen);

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%semergencyMode: %s", printBuf,
            mbim_emergency_mode_state_get_printable(emergencyModeInfo->emergencyMode));
    closeResponse(printBuf);
    printResponse(printBuf);

    *response = emergencyModeInfo;
    *responseLen = dataLen;

    return 0;
}

/******************************************************************************/
/**
 * basic_connect_ip_packet_filters: query and set supported
 *
 * query:
 *  @command:   the informationBuffer shall be MbimPacketFilters
 *  @response:  the informationBuffer shall be MbimPacketFilters
 *
 * set:
 *  @command:   the informationBuffer shall be MbimPacketFilters
 *  @response:  the informationBuffer shall be MbimPacketFilters
 *
 * Note:
 *  for query, the only relevant field of MbimPacketFilters in command's
 *  informationBuffer is the sessionId.
 */
int
mbim_message_parser_basic_connect_ip_packet_filters(MbimMessageInfo * messageInfo,
                                                    char *            printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            basic_connect_ip_packet_filters_query_operation(messageInfo, printBuf);
            break;
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            basic_connect_ip_packet_filters_set_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
basic_connect_ip_packet_filters_query_operation(MbimMessageInfo *   messageInfo,
                                                char *              printBuf) {
    int32_t bufferLen = 0;
    MbimPacketFilters *data = (MbimPacketFilters *)
            mbim_message_command_get_raw_information_buffer(messageInfo->message, &bufferLen);

    startCommand(printBuf);
    appendPrintBuf(printBuf, "%ssessionId: %d", printBuf, data->sessionId);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_ip_packet_filters_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, &(data->sessionId), sizeof(uint32_t));
}

void
basic_connect_ip_packet_filters_set_operation(MbimMessageInfo * messageInfo,
                                              char *            printBuf) {
    int32_t bufferLen = 0;
    MbimPacketFilters *data = (MbimPacketFilters *)
            mbim_message_command_get_raw_information_buffer(messageInfo->message, &bufferLen);

    uint32_t packetFilterCount = data->packetFilterCount;
    MbimPacketFilters_2 *packetFilters = NULL;

    uint32_t information_buffer_offset = mbim_message_get_information_buffer_offset(messageInfo->message);

    packetFilters = (MbimPacketFilters_2 *)calloc(1, sizeof(MbimPacketFilters_2));
    packetFilters->sessionId = data->sessionId;
    packetFilters->packetFilterCount = packetFilterCount;
    packetFilters->singlePacketFiltes = (MbimSinglePacketFilter_2 **)
            calloc(packetFilterCount, sizeof(MbimSinglePacketFilter_2 *));

    startCommand(printBuf);
    appendPrintBuf(printBuf, "%ssessionId: %d, packetFilterCount: %d", printBuf,
            data->sessionId, packetFilters->packetFilterCount);

    for (uint32_t i = 0; i < packetFilterCount; i++) {
        packetFilters->singlePacketFiltes[i] =
                (MbimSinglePacketFilter_2 *)calloc(1, sizeof(MbimSinglePacketFilter_2));

        uint32_t offset = data->packetFilterRefList[i].offset;

        MbimSinglePacketFilter *singlePacketFilter = (MbimSinglePacketFilter *)
                G_STRUCT_MEMBER_P(messageInfo->message->data, (information_buffer_offset + offset));

        uint8_t *packetFilter = (uint8_t *)calloc(singlePacketFilter->filterSize, sizeof(uint8_t));
        uint8_t *packetMask = (uint8_t *)calloc(singlePacketFilter->filterSize, sizeof(uint8_t));
        memcpy((uint8_t *)packetFilter, (uint8_t *)singlePacketFilter + singlePacketFilter->packetFilterOffset,
                singlePacketFilter->filterSize);
        memcpy((uint8_t *)packetMask, (uint8_t *)singlePacketFilter + singlePacketFilter->packetMaskOffset,
                singlePacketFilter->filterSize);

        packetFilters->singlePacketFiltes[i]->filterSize = singlePacketFilter->filterSize;
        packetFilters->singlePacketFiltes[i]->packetFilter = packetFilter;
        packetFilters->singlePacketFiltes[i]->packetMask = packetMask;
        appendPrintBuf(printBuf, "%s{filterSize: %d, packetFilter: %s, packetMask: %s}",
                printBuf, singlePacketFilter->filterSize, packetFilter, packetMask);
    }

    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_ip_packet_filters_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, packetFilters, sizeof(MbimPacketFilters_2));

    for (uint32_t i = 0; i < packetFilterCount; i++) {
        free(packetFilters->singlePacketFiltes[i]->packetFilter);
        free(packetFilters->singlePacketFiltes[i]->packetMask);
        free(packetFilters->singlePacketFiltes[i]);
    }
    free(packetFilters->singlePacketFiltes);
    free(packetFilters);
}

int
basic_connect_ip_packet_filters_set_or_query_ready(Mbim_Token           token,
                                                   MbimStatusError      error,
                                                   void *               response,
                                                   size_t               responseLen,
                                                   char *               printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimPacketFilters_2)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimPacketFilters_2));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimPacketFilters_2 *resp = (MbimPacketFilters_2 *)response;

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%spacketFilterCount: %d", printBuf,
            resp->packetFilterCount);

    uint32_t informationBufferSize[resp->packetFilterCount];
    uint32_t informationBufferSizeInTotal = 0;
    MbimSinglePacketFilter **singlePacketFilters = (MbimSinglePacketFilter **)
            calloc(resp->packetFilterCount, sizeof(MbimSinglePacketFilter *));
    for (uint32_t i = 0; i < resp->packetFilterCount; i++) {
        singlePacketFilters[i] = ip_packet_filters_response_builder(
                resp->singlePacketFiltes[i], sizeof(MbimSinglePacketFilter_2),
                &informationBufferSize[i], printBuf);
        informationBufferSizeInTotal += informationBufferSize[i];
    }

    informationBufferSizeInTotal += sizeof(MbimPacketFilters) +
            sizeof(OffsetLengthPairList) * resp->packetFilterCount;

    MbimPacketFilters *packetFilters =
            (MbimPacketFilters *)calloc(1, informationBufferSizeInTotal);

    uint32_t dataBufferOffset = G_STRUCT_OFFSET(MbimPacketFilters, packetFilterRefList) +
            sizeof(OffsetLengthPairList) * resp->packetFilterCount;
    packetFilters->packetFilterCount = resp->packetFilterCount;
    uint32_t offset = dataBufferOffset;
    for (uint32_t i = 0; i < resp->packetFilterCount; i++) {
        packetFilters->packetFilterRefList[i].offset = offset;
        packetFilters->packetFilterRefList[i].length = informationBufferSize[i];
        offset += informationBufferSize[i];

        memcpy((uint8_t *)packetFilters + packetFilters->packetFilterRefList[i].offset,
                (uint8_t *)(singlePacketFilters[i]), informationBufferSize[i]);
    }

    closeResponse(printBuf);
    printResponse(printBuf);

    mbim_command_complete(token, error, packetFilters, informationBufferSizeInTotal);
    for (uint32_t i = 0; i < resp->packetFilterCount; i++) {
        free(singlePacketFilters[i]);
    }
    free(singlePacketFilters);
    free(packetFilters);

    return 0;
}

MbimSinglePacketFilter *
ip_packet_filters_response_builder(void *       response,
                                   size_t       responseLen,
                                   uint32_t *   outSize,
                                   char *       printBuf) {
    MBIM_UNUSED_PARM(responseLen);

    MbimSinglePacketFilter_2 *resp = (MbimSinglePacketFilter_2 *)response;

    appendPrintBuf(printBuf, "%s[filterSize: %d, packetFilter: %s, packetMask: %s]",
            printBuf, resp->filterSize, resp->packetFilter, resp->packetMask);

    uint32_t informationBufferSize = sizeof(MbimSinglePacketFilter) +
            resp->filterSize * sizeof(uint8_t) * 2;

    uint32_t dataBufferOffset = G_STRUCT_OFFSET(MbimSinglePacketFilter, dataBuffer);

    MbimSinglePacketFilter *singlePacketFilter =
            (MbimSinglePacketFilter *)calloc(1, informationBufferSize);
    singlePacketFilter->filterSize = resp->filterSize;
    singlePacketFilter->packetFilterOffset = dataBufferOffset;
    singlePacketFilter->packetMaskOffset =
            singlePacketFilter->packetFilterOffset + singlePacketFilter->filterSize;
    memcpy((uint8_t *)singlePacketFilter + singlePacketFilter->packetFilterOffset,
            (uint8_t *)(resp->packetFilter), resp->filterSize);
    memcpy((uint8_t *)singlePacketFilter + singlePacketFilter->packetMaskOffset,
            (uint8_t *)(resp->packetMask), resp->filterSize);

    *outSize = informationBufferSize;
    return singlePacketFilter;
}

/******************************************************************************/
/**
 * basic_connect_multicarrier_providers: query, set and notification supported
 *
 * query:
 *  @command:   the informationBuffer shall be null
 *  @response:  the informationBuffer shall be MbimProviders
 *
 * set:
 *  @command:   the informationBuffer shall be MbimProviders
 *  @response:  the informationBuffer shall be MbimProviders
 *
 * notification:
 *  @command:   NA
 *  @response:  the informationBuffer shall be MbimProviders
 *
 */
int
mbim_message_parser_basic_connect_multicarrier_providers(MbimMessageInfo *  messageInfo,
                                                         char *             printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            basic_connect_multicarrier_providers_query_operation(messageInfo, printBuf);
            break;
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            basic_connect_multicarrier_providers_set_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
basic_connect_multicarrier_providers_query_operation(MbimMessageInfo *  messageInfo,
                                                     char *             printBuf) {
    startCommand(printBuf);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_multicarrier_providers_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, NULL, 0);
}

void
basic_connect_multicarrier_providers_set_operation(MbimMessageInfo *    messageInfo,
                                                   char *               printBuf) {
    int32_t bufferLen = 0;
    MbimProviders *data = (MbimProviders *)
            mbim_message_command_get_raw_information_buffer(messageInfo->message, &bufferLen);

    uint32_t elementCount = data->elementCount;
    MbimProviders_2 *providers = NULL;

    uint32_t information_buffer_offset = mbim_message_get_information_buffer_offset(messageInfo->message);

    providers = (MbimProviders_2 *)calloc(1, sizeof(MbimProviders_2));
    providers->provider = (MbimProvider_2 **)calloc(elementCount, sizeof(MbimProvider_2 *));
    providers->elementCount = elementCount;

    startCommand(printBuf);
    appendPrintBuf(printBuf, "%selementCount: %d", printBuf, elementCount);

    for (uint32_t i = 0; i < elementCount; i++) {
        providers->provider[i] = (MbimProvider_2 *)calloc(1, sizeof(MbimProvider_2));

        uint32_t offset = data->providerRefList[i].offset;

        MbimProvider *providerData = (MbimProvider *)
                G_STRUCT_MEMBER_P(messageInfo->message->data, (information_buffer_offset + offset));

        uint32_t providerIdOffset = G_STRUCT_OFFSET(MbimProvider, providerIdOffset);
        char *providerId = mbim_message_read_string(messageInfo->message, offset, providerIdOffset);
        providers->provider[i]->providerId = providerId;

        uint32_t providerNameOffset = G_STRUCT_OFFSET(MbimProvider, providerNameOffset);
        char *providerName = mbim_message_read_string(messageInfo->message, offset, providerNameOffset);
        providers->provider[i]->providerName = providerName;

        providers->provider[i]->providerState = providerData->providerState;
        providers->provider[i]->celluarClass = providerData->celluarClass;
        providers->provider[i]->rssi = providerData->rssi;
        providers->provider[i]->errorRate = providerData->errorRate;

        appendPrintBuf(printBuf, "%s[providerId: %s, providerState: %s, providerName: %s,"
                " celluarClass: %s, rssi = %d, errorRate = %d]",
                printBuf,
                providers->provider[i]->providerId,
                mbim_provider_state_get_printable(providers->provider[i]->providerState),
                providers->provider[i]->providerName,
                mbim_cellular_class_get_printable(providers->provider[i]->celluarClass),
                providers->provider[i]->rssi,
                providers->provider[i]->errorRate);
    }

    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = basic_connect_preferred_providers_set_or_query_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, providers, sizeof(MbimProviders_2));

    for (uint32_t i = 0; i < elementCount; i++) {
        memsetAndFreeStrings(2, providers->provider[i]->providerId,
                providers->provider[i]->providerName);
        free(providers->provider[i]);
    }
    free(providers->provider);
    free(providers);
}

int
basic_connect_multicarrier_providers_set_or_query_ready(Mbim_Token          token,
                                                        MbimStatusError     error,
                                                        void *              response,
                                                        size_t              responseLen,
                                                        char *              printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (responseLen != sizeof(MbimProviders_2)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)responseLen, (int)sizeof(MbimProviders_2));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimProviders_2 *resp = (MbimProviders_2 *)response;

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%selementCount: %d", printBuf, resp->elementCount);

    uint32_t informationBufferSize[resp->elementCount];
    uint32_t informationBufferSizeInTotal = 0;
    MbimProvider **provider = (MbimProvider **)calloc(resp->elementCount, sizeof(MbimProvider));
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        provider[i] = home_provider_response_builder(resp->provider[i],
                sizeof(MbimProvider_2), &informationBufferSize[i], printBuf);
        informationBufferSizeInTotal += informationBufferSize[i];
    }

    informationBufferSizeInTotal += sizeof(MbimProviders) +
            sizeof(OffsetLengthPairList) * resp->elementCount;

    MbimProviders *providers = (MbimProviders *)calloc(1, informationBufferSizeInTotal);
    uint32_t dataBufferOffset = G_STRUCT_OFFSET(MbimProviders, providerRefList) +
            sizeof(OffsetLengthPairList) * resp->elementCount;
    providers->elementCount = resp->elementCount;
    uint32_t offset = dataBufferOffset;
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        providers->providerRefList[i].offset = offset;
        providers->providerRefList[i].length = informationBufferSize[i];
        offset += informationBufferSize[i];

        memcpy((uint8_t *)providers + providers->providerRefList[i].offset,
                (uint8_t *)(provider[i]), informationBufferSize[i]);
    }

    closeResponse(printBuf);
    printResponse(printBuf);

    mbim_command_complete(token, error, providers, informationBufferSizeInTotal);
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        free(provider[i]);
    }
    free(provider);
    free(providers);

    return 0;
}

int
basic_connect_multicarrier_providers_notify(void *      data,
                                            size_t      dataLen,
                                            void **     response,
                                            size_t *    responseLen,
                                            char *      printBuf) {
    if (data == NULL || dataLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }
    if (dataLen != sizeof(MbimProviders_2)) {
        RLOGE("invalid response length %d expected multiple of %d",
            (int)dataLen, (int)sizeof(MbimProviders_2));
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    MbimProviders_2 *resp = (MbimProviders_2 *)data;

    startResponse(printBuf);
    appendPrintBuf(printBuf, "%selementCount: %d", printBuf, resp->elementCount);

    uint32_t informationBufferSize[resp->elementCount];
    uint32_t informationBufferSizeInTotal = 0;
    MbimProvider **provider = (MbimProvider **)calloc(resp->elementCount, sizeof(MbimProvider));
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        provider[i] = home_provider_response_builder(resp->provider[i],
                sizeof(MbimProvider_2), &informationBufferSize[i], printBuf);
        informationBufferSizeInTotal += informationBufferSize[i];
    }

    informationBufferSizeInTotal += sizeof(MbimProviders) +
            sizeof(OffsetLengthPairList) * resp->elementCount;

    MbimProviders *providers = (MbimProviders *)calloc(1, informationBufferSizeInTotal);
    uint32_t dataBufferOffset = G_STRUCT_OFFSET(MbimProviders, providerRefList) +
            sizeof(OffsetLengthPairList) * resp->elementCount;
    providers->elementCount = resp->elementCount;
    uint32_t offset = dataBufferOffset;
    for (uint32_t i = 0; i < resp->elementCount; i++) {
        providers->providerRefList[i].offset = offset;
        providers->providerRefList[i].length = informationBufferSize[i];
        offset += informationBufferSize[i];

        memcpy((uint8_t *)providers + providers->providerRefList[i].offset,
                (uint8_t *)(provider[i]), informationBufferSize[i]);
    }

    closeResponse(printBuf);
    printResponse(printBuf);

    *response = providers;
    *responseLen = (size_t)informationBufferSize;

    return 0;
}

/******************************************************************************/
