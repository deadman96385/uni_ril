#define LOG_TAG "MBIM-Device"


#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <utils/Log.h>

#include "mbim_message_processer.h"
#include "mbim_device_vendor.h"
#include "mbim_service_oem.h"

void
mbim_message_parser_oem(MbimMessageInfo *messageInfo) {
    int ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
    char printBuf[PRINTBUF_SIZE] = {0};
    mbim_message_info_get_printtable(printBuf, messageInfo, LOG_TYPE_COMMAND);

    switch (messageInfo->cid) {
        case MBIM_CID_OEM_ATCI:
            ret = mbim_message_parser_oem_atci(messageInfo, printBuf);
            break;
        default:
            RLOGE("Unsupported oem cid");
            break;
    }

    if (ret == MBIM_ERROR_INVALID_COMMAND_TYPE) {
        RLOGE("unsupported oem cid or command type");
        mbim_device_command_done(messageInfo,
                MBIM_STATUS_ERROR_NO_DEVICE_SUPPORT, NULL, 0);
    }
}

/******************************************************************************/
/**
 * oem_atci: query, set and notification supported
 *
 * query:
 *  @command:   the informationBuffer shall be
 *  @response:  the informationBuffer shall be
 *
 * set:
 *  @command:   the informationBuffer shall be
 *  @response:  the informationBuffer shall be
 *
 * notification:
 *  @command:   the informationBuffer shall be
 *  @response:  the informationBuffer shall be
 */
int
mbim_message_parser_oem_atci(MbimMessageInfo *  messageInfo,
                             char *             printBuf) {
    int ret = 0;

    switch (messageInfo->commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            oem_atci_set_operation(messageInfo, printBuf);
            break;
        default:
            ret = MBIM_ERROR_INVALID_COMMAND_TYPE;
            RLOGE("unsupported commandType");
            break;
    }

    return ret;
}

void
oem_atci_set_operation(MbimMessageInfo *    messageInfo,
                       char *               printBuf) {
    int32_t bufferLen = 0;
    MbimOemATCommand *data = (MbimOemATCommand *)
            mbim_message_command_get_raw_information_buffer(messageInfo->message, &bufferLen);

    uint32_t ATCommandOffset =
            G_STRUCT_OFFSET(MbimOemATCommand, ATCommandOffset);
    char *atCommand = mbim_message_read_string(messageInfo->message, 0, ATCommandOffset);

    startCommand(printBuf);
    appendPrintBuf(printBuf, "%sATCommand: %s", printBuf, atCommand);
    closeCommand(printBuf);
    printCommand(printBuf);

    messageInfo->callback = oem_atci_set_ready;
    mbim_device_command(messageInfo, messageInfo->service, messageInfo->cid,
            messageInfo->commandType, atCommand, atCommand == NULL ? 0 : strlen(atCommand));

    memsetAndFreeStrings(1, atCommand);
}

int
oem_atci_set_ready(Mbim_Token          token,
                   MbimStatusError     error,
                   void *              response,
                   size_t              responseLen,
                   char *              printBuf) {
    if (response == NULL || responseLen == 0) {
        RLOGE("invalid response: NULL");
        return MBIM_ERROR_INVALID_RESPONSE;
    }

    char *atResp = (char *)response;
    startResponse(printBuf);
    appendPrintBuf(printBuf, "%sATResp: %s", printBuf, atResp);
    closeResponse(printBuf);
    printResponse(printBuf);

    /* AT Response */
    uint32_t ATRespSize = 0;
    uint32_t ATRespPadSize = 0;
    unichar2 *ATRespInUtf16 = NULL;
    if (atResp == NULL || strlen(atResp) == 0) {
        ATRespSize = 0;
    } else {
        ATRespSize = strlen(atResp) * 2;
        ATRespPadSize = PAD_SIZE(ATRespSize);
        ATRespInUtf16 = (unichar2 *)calloc(ATRespPadSize / 2 + 1, sizeof(unichar2));
        utf8_to_utf16(ATRespInUtf16, atResp, ATRespSize / 2, ATRespSize / 2);
    }

    uint32_t informationBufferSize = sizeof(MbimOemATResp) + ATRespPadSize;
    MbimOemATResp *oemATResp =
            (MbimOemATResp *)calloc(1, informationBufferSize);
    uint32_t dataBufferOffset = G_STRUCT_OFFSET(MbimOemATResp, dataBuffer);

    oemATResp->ATRespOffset = dataBufferOffset;
    oemATResp->ATRespSize = ATRespPadSize;
    if (ATRespPadSize != 0) {
        memcpy((uint8_t *)oemATResp + oemATResp->ATRespOffset,
                (uint8_t *)ATRespInUtf16, ATRespPadSize);
    }

    mbim_command_complete(token, error, oemATResp, informationBufferSize);
    memsetAndFreeUnichar2Arrays(1, ATRespInUtf16);
    free(oemATResp);

    return 0;
}
