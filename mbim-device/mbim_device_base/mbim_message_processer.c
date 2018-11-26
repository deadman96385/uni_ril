#define LOG_TAG "MBIM-Device"

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <utils/Log.h>
#include <sys/time.h>
#include <time.h>
#include <arpa/inet.h>

#include "mbim_service_basic_connect.h"
#include "mbim_service_sms.h"
#include "mbim_service_oem.h"
#include "mbim_message_processer.h"
#include "mbim_message_threads.h"
#include "mbim_device_vendor.h"
#include "mbim_cid.h"
#include "time_event_handler.h"

#define MAX_TIME_BETWEEN_FRAGMENTS_MS   1250
#define MAX_CONTROL_TRANSFER            4096

typedef struct {
    MbimMessage *           fragments;
    uint32_t                transaction_id;
    uint32_t                totalFragments;
    uint32_t                currentFragment;
    uint32_t                currentDataLength;
    struct timeval          receivedTime;
} FragmentMessageContext;

static uint             s_maxControlTransfer = MAX_CONTROL_TRANSFER;
static MbimDevice *     s_mbimDevice = NULL;
static pthread_mutex_t  s_deviceWriteMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t  s_atciWriteMutex = PTHREAD_MUTEX_INITIALIZER;

static int              s_started = 0;
static pthread_mutex_t  s_startupMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   s_startupCond = PTHREAD_COND_INITIALIZER;

/**
 * As the fragment message should be received in order, cannot intermix fragments
 * from other message, so at one time, only one fragment messages should be
 * considered.
 */
FragmentMessageContext *s_fragmentMessageContext = NULL;

void
mbim_command_message_parser(MbimMessageInfo *messageInfo) {
    bool canQuery = false;
    bool canSet = false;

    /*
     * Checks whether the given command allows querying or setting.
     */
    if (messageInfo->commandType == MBIM_MESSAGE_COMMAND_TYPE_QUERY) {
        canQuery = mbim_cid_can_query(messageInfo->service, messageInfo->cid);
    } else if (messageInfo->commandType == MBIM_MESSAGE_COMMAND_TYPE_SET) {
        canSet = mbim_cid_can_set(messageInfo->service, messageInfo->cid);
    }

    if (canQuery == false && canSet == false) {
        RLOGE("Invaild command type");
        mbim_device_command_done(messageInfo,
                MBIM_STATUS_ERROR_NO_DEVICE_SUPPORT, NULL, 0);
        return;
    }

    switch (messageInfo->service) {
        case MBIM_SERVICE_BASIC_CONNECT:
            mbim_message_parser_basic_connect(messageInfo);
            break;
        case MBIM_SERVICE_SMS:
            mbim_message_parser_sms(messageInfo);
            break;
        case MBIM_SERVICE_OEM:
            mbim_message_parser_oem(messageInfo);
            break;
        default:
            RLOGE("unsupported service type");
            mbim_device_command_done(messageInfo,
                    MBIM_STATUS_ERROR_INVALID_DEVICE_SERVICE_OPERATION, NULL, 0);
            break;
    }
}

int
sendResponse(const void *           data,
             size_t                 dataSize) {
    int ret = 0;
    size_t writeOffset = 0;
    const uint8_t *toWrite;

    pthread_mutex_lock(&s_deviceWriteMutex);

#if 1
    if (s_mbimDevice->fd < 0) {
        ret = -1;
        goto out;
    }
    if (s_mbimDevice->open_status != OPEN_STATUS_OPEN) {
        RLOGE("mbim device is not opened, ingore");
        ret = -1;
        goto out;
    }

    toWrite = (const uint8_t *)(data);

    while (writeOffset < dataSize) {
        ssize_t written;
        do {
            written = write(s_mbimDevice->fd, toWrite + writeOffset,
                    dataSize - writeOffset);
        } while (written < 0 && ((errno == EINTR) || (errno == EAGAIN)));

        if (written >= 0) {
            writeOffset += written;
        } else {   // written < 0
            RLOGE("unexpected error on write errno:%d, %s",
                    errno, strerror(errno));
            ret = -1;
            goto out;
        }
    }
    RLOGD("sendResponse ret = %d", ret);
#endif

out:
    pthread_mutex_unlock(&s_deviceWriteMutex);
    return ret;
}

void
mbim_device_command_done(Mbim_Token         token,
                         MbimStatusError    error,
                         void *             response,
                         size_t             responseLen) {
    int ret = -1;
    char printBuf[PRINTBUF_SIZE] = {0};
    MbimMessageInfo *messageInfo = (MbimMessageInfo *)token;
    mbim_message_info_get_printtable(printBuf, messageInfo, LOG_TYPE_RESPONSE);

    /**
     * If the Status field returned to the host is not equal to MBIM_STATUS_SUCCESS,
     * the device must set the InformationBufferLength to 0, indicating an empty
     * InformationBuffer. However, there are exceptions to this rule.
     * In some cases called out explicitly below, a Status value other than
     * MBIM_STATUS_SUCCESS could be accompanied by further information in the
     * field NwError within the InformationBuffer. In these cases, the function
     * must follow the specification for the CID in question and send back the
     * required information, while zeroing out all irrelevant fields. Such
     * cases are specified for the following CIDs:
     *      MBIM_CID_REGISTER_STATE
     *      MBIM_CID_PACKET_SERVICE
     *      MBIM_CID_CONNECT
     *      MBIM_CID_SERVICE_ACTIVATION
     *      MBIM_CID_PIN
     */
    if (error != MBIM_STATUS_ERROR_NONE && messageInfo->cid != MBIM_CID_BASIC_CONNECT_PIN ) {
        appendPrintBuf(printBuf, "%s fails by %s", printBuf,
                mbim_status_error_get_printable(error));
        printResponse(printBuf);

        mbim_command_complete(token, error, NULL, 0);
    } else {
        if (messageInfo->callback != NULL) {
            ret = messageInfo->callback(token, error, response, responseLen, printBuf);
            if (ret == MBIM_ERROR_INVALID_RESPONSE) {
                mbim_command_complete(token, MBIM_STATUS_ERROR_FAILURE, NULL, 0);
            }
        } else {
            mbim_command_complete(token, MBIM_STATUS_ERROR_FAILURE, NULL, 0);
        }
    }
}

static int blockingWrite(int fd, const void *buffer, size_t len) {
    size_t writeOffset = 0;
    const uint8_t *toWrite;

    toWrite = (const uint8_t *)buffer;

    while (writeOffset < len) {
        ssize_t written;
        do {
            written = write(fd, toWrite + writeOffset,
                                len - writeOffset);
        } while (written < 0 && ((errno == EINTR) || (errno == EAGAIN)));

        if (written >= 0) {
            writeOffset += written;
        } else {   // written < 0
            RLOGE("RIL Response: unexpected error on write errno:%d, %s",
                    errno, strerror(errno));
            close(fd);
            return -1;
        }
    }
#if VDBG
    RLOGE("RIL Response bytes written:%d", writeOffset);
#endif
    return 0;
}

static int sendResponseToAtciClient(const void *data, size_t dataSize) {
    int ret;
    uint32_t header;
    pthread_mutex_t *writeMutexHook = &s_atciWriteMutex;
    int fd = s_atciSocketParam.fdCommand;

    if (fd < 0) {
        return -1;
    }

    if (dataSize > MAX_COMMAND_BYTES) {
        RLOGE("RIL: packet larger than %u (%u)",
                MAX_COMMAND_BYTES, (unsigned int)dataSize);

        return -1;
    }

    pthread_mutex_lock(writeMutexHook);

    header = htonl(dataSize);

    ret = blockingWrite(fd, (void *)&header, sizeof(header));

    if (ret < 0) {
        pthread_mutex_unlock(writeMutexHook);
        return ret;
    }

    ret = blockingWrite(fd, data, dataSize);

    if (ret < 0) {
        pthread_mutex_unlock(writeMutexHook);
        return ret;
    }

    pthread_mutex_unlock(writeMutexHook);

    return 0;
}

void
mbim_command_complete(Mbim_Token            token,
                      MbimStatusError       status,
                      void *                data,
                      size_t                dataLen) {
    MbimMessage *message = (MbimMessage *)(((MbimMessageInfo *)token)->message);
    MbimCommunicationType communicationType = ((MbimMessageInfo *)token)->communicationType;

    MbimMessage *response = build_command_done_message(message, status, dataLen);
    mbim_message_command_append(response, data, dataLen);

#if MBIM_LOG_DEBUG
    for (uint32_t i = 0; i < response->len; i += 4) {
        RLOGD("response[%d] = %02x, %02x, %02x, %02x", i / 4,
                (response->data)[i], (response->data)[i + 1],
                (response->data)[i + 2], (response->data)[i + 3]);
    }
#endif

    if (communicationType == MBIM_ATCI_COMMUNICATION) {
        sendResponseToAtciClient(response->data, response->len);

        if (s_atciSocketParam.fdCommand != -1) {
            close(s_atciSocketParam.fdCommand);
        }
        s_atciSocketParam.fdCommand = -1;
        if (s_atciSocketParam.p_rs != NULL) {
            record_stream_free(s_atciSocketParam.p_rs);
            s_atciSocketParam.p_rs = NULL;
        }
        rilEventAddWakeup(s_atciSocketParam.listen_event);
    } else {
        /* Single fragment? Send it! */
        if (response->len <= s_maxControlTransfer) {
            sendResponse(response->data, response->len);
        } else {
            struct fragment_info *fragments;
            uint n_fragments;
            uint i;

            fragments = mbim_message_split_fragments(response,
                                                     s_maxControlTransfer,
                                                     &n_fragments);

            for (i = 0; i < n_fragments; i++) {
                uint32_t sendRespLen = fragments[i].data_length +
                        sizeof(struct header) + sizeof(struct fragment_header);
                uint8_t *sendRespData = (uint8_t *)calloc(sendRespLen, sizeof(uint8_t));

                memcpy(sendRespData, (uint8_t *)(&(fragments[i])),
                        sizeof(struct header) + sizeof(struct fragment_header));
                memcpy(sendRespData + sizeof(struct header) + sizeof(struct fragment_header),
                        fragments[i].data, fragments[i].data_length);

#if MBIM_LOG_DEBUG
                for (uint32_t i = 0; i < sendRespLen; i += 4) {
                    RLOGD("sendRespData[%d] = %02x, %02x, %02x, %02x", i / 4,
                            (sendRespData)[i], (sendRespData)[i + 1],
                            (sendRespData)[i + 2], (sendRespData)[i + 3]);
                }
#endif

                sendResponse(sendRespData, sendRespLen);

                free(sendRespData);
            }
            free(fragments);
        }
    }

    mbim_message_free(message);
    mbim_message_free(response);
}

void
mbim_device_indicate_status(MbimService     serviceId,
                            uint32_t        cid,
                            void *          data,
                            size_t          dataLen) {
    bool canNotify = false;
    bool isSubscribed = false;

    /* check if the cid can notify */
    canNotify = mbim_cid_can_notify(serviceId, cid);
    if (!canNotify) {
        RLOGE("service: %s, cid: %s not allows notify",
                mbim_service_get_printable(serviceId),
                mbim_cid_get_printable(serviceId, cid));
        return;
    }

    /* check if the host subscribe the cid of the serviceId */
    isSubscribed = notification_subscribe_check(serviceId, cid);
    if (!(serviceId == MBIM_SERVICE_SMS && cid == MBIM_CID_SMS_MESSAGE_STORE_STATUS) &&  !isSubscribed) {
        RLOGE("not MBIM_CID_SMS_MESSAGE_STORE_STATUS and host done not subscribe service: %s, cid: %s",
                mbim_service_get_printable(serviceId),
                mbim_cid_get_printable(serviceId, cid));
        return;
    }

    int ret = -1;
    IndicateStatusCallback callback = NULL;

    switch (serviceId) {
        case MBIM_SERVICE_BASIC_CONNECT:
            callback = s_basic_connect_indicate_status_info[cid - 1].callback;
            break;
        case MBIM_SERVICE_SMS:
            callback = s_sms_indicate_status_info[cid - 1].callback;
            break;

        default:
            RLOGE("indicate_status: not supported service: %s",
                    mbim_service_get_printable(serviceId));
            break;
    }

    if (callback != NULL) {
        char printBuf[PRINTBUF_SIZE] = {0};
        snprintf(printBuf, PRINTBUF_SIZE, "[UNSOL]< %s: %s",
                mbim_service_get_printable(serviceId),
                mbim_cid_get_printable(serviceId, cid));
        void *response = NULL;
        size_t responseLen = 0;

        ret = callback(data, dataLen, &response, &responseLen, printBuf);
        if (ret == 0) {
            MbimMessage *message = build_indicate_status_message(serviceId,
                    cid, responseLen);

            mbim_message_command_append(message, (uint8_t *)response, responseLen);

#if MBIM_LOG_DEBUG
            for (uint32_t i = 0; i < message->len; i += 4) {
                RLOGD("response[%d] = %02x, %02x, %02x, %02x", i / 4,
                        (message->data)[i], (message->data)[i + 1],
                        (message->data)[i + 2], (message->data)[i + 3]);
            }
#endif

            /* Single fragment? Send it! */
            if (message->len <= s_maxControlTransfer) {
                sendResponse(message->data, message->len);
            } else {
                struct fragment_info *fragments;
                uint n_fragments;
                uint i;

                fragments = mbim_message_split_fragments(message,
                                                         s_maxControlTransfer,
                                                         &n_fragments);

                for (i = 0; i < n_fragments; i++) {
                    uint32_t sendRespLen = fragments[i].data_length +
                            sizeof(struct header) + sizeof(struct fragment_header);
                    uint8_t *sendRespData = (uint8_t *)calloc(sendRespLen, sizeof(uint8_t));

                    memcpy(sendRespData, (uint8_t *)(&(fragments[i])),
                            sizeof(struct header) + sizeof(struct fragment_header));
                    memcpy(sendRespData + sizeof(struct header) + sizeof(struct fragment_header),
                            fragments[i].data, fragments[i].data_length);

#if MBIM_LOG_DEBUG
                    for (uint32_t i = 0; i < sendRespLen; i += 4) {
                        RLOGD("sendRespData[%d] = %02x, %02x, %02x, %02x", i / 4,
                                (sendRespData)[i], (sendRespData)[i + 1],
                                (sendRespData)[i + 2], (sendRespData)[i + 3]);
                    }
#endif

                    sendResponse(sendRespData, sendRespLen);

                    free(sendRespData);
                }
                free(fragments);
            }

            mbim_message_free(message);
            free((void *)response);
        } else {
            RLOGE("indicate_status: pack response failed");
        }
    } else {
        RLOGE("unsupported service: %s, cid: %s",
                mbim_service_get_printable(serviceId),
                mbim_cid_get_printable(serviceId, cid));
    }
}

void
processOpenMessage(MbimMessage *message) {
    uint32_t transaction_id = mbim_message_get_transaction_id(message);
    uint32_t max_control_transfer =
            mbim_message_open_get_max_control_transfer(message);

    RLOGD("transaction_id = %d", transaction_id);
    RLOGD("max_control_transfer = %d", max_control_transfer);

    s_maxControlTransfer = max_control_transfer;

    MbimMessage *response = NULL;
    MbimStatusError status = MBIM_STATUS_ERROR_NONE;

    response = mbim_message_open_done_new(transaction_id, status);
    s_mbimDevice->open_status = OPEN_STATUS_OPEN;

#if MBIM_LOG_DEBUG
    for (uint32_t i = 0; i < response->len; i += 4) {
        RLOGD("response[%d] = %02x, %02x, %02x, %02x", i / 4,
                (response->data)[i], (response->data)[i + 1],
                (response->data)[i + 2], (response->data)[i + 3]);
    }
#endif

    sendResponse(response->data, response->len);

    mbim_message_free(message);
    mbim_message_free(response);
}

void
processCloseMessage(MbimMessage *message) {
    uint32_t transaction_id = mbim_message_get_transaction_id(message);
    RLOGD("transaction_id = %d", transaction_id);

    MbimMessage *response = NULL;
    MbimStatusError status = MBIM_STATUS_ERROR_NONE;

    response = mbim_message_close_done_new(transaction_id, status);

#if MBIM_LOG_DEBUG
    for (uint32_t i = 0; i < response->len; i += 4) {
        RLOGD("response[%d] = %02x, %02x, %02x, %02x", i / 4,
                (response->data)[i], (response->data)[i + 1],
                (response->data)[i + 2], (response->data)[i + 3]);
    }
#endif

    sendResponse(response->data, response->len);

    s_mbimDevice->open_status = OPEN_STATUS_CLOSED;

    mbim_message_free(message);
    mbim_message_free(response);
}

void
processCommandMessage(MbimMessage *message) {
    enqueueCommandMessage(message, MBIM_DEVICE_COMMUNICATION);
}

void
indicateHostErrorMessage(MbimMessage *message, MbimProtocolError protocolError) {
    uint32_t transaction_id = mbim_message_get_transaction_id(message);
    RLOGE("indicateHostErrorMessage, transaction_id: %d, error: %s",
            transaction_id, mbim_protocol_error_get_printable(protocolError));

    MbimMessage *response = NULL;
    MbimProtocolError status = protocolError;

    response = mbim_message_function_error_new(transaction_id, status);

#if MBIM_LOG_DEBUG
    for (uint32_t i = 0; i < response->len; i += 4) {
        RLOGD("response[%d] = %02x, %02x, %02x, %02x", i / 4,
                (response->data)[i], (response->data)[i + 1],
                (response->data)[i + 2], (response->data)[i + 3]);
    }
#endif

    sendResponse(response->data, response->len);

    mbim_message_free(message);
    mbim_message_free(response);
}

extern void getNow(struct timeval *tv);

void
processMessage(void *       data,
               size_t       dataLen) {
    MbimMessage *message = NULL;
    message = mbim_message_new(data, dataLen);

    bool is_partial_fragment = false;

    /**
     * check if the message type support fragment or not,
     * and if supported, check if the message is fragmented.
     */
    is_partial_fragment = (mbim_message_is_supported_fragment(message) &&
                           mbim_message_fragment_get_total(message) > 1);
    if (is_partial_fragment) {
        /* More than one fragment expected; is this the first one? */
        if (mbim_message_fragment_get_current(message) == 0) {
            /**
             * if s_fragmentMessageContext != NULL, it represents that function is
             * waiting for fragment message which currentFragment should not be 0,
             *
             * if the function receives a first fragment of a new command with a
             * new transactionId, the function shall discard the previous command
             * and start handling the new command.
             */
            if (s_fragmentMessageContext != NULL) {
                if (mbim_message_get_transaction_id(message) ==
                        s_fragmentMessageContext->transaction_id) {
                    RLOGE("Discard the waiting fragment message and the received"
                            " message, because of having the same transactionID, "
                            "transactionID: %d, expecting fragment %u/%u, got 0/%u",
                            s_fragmentMessageContext->transaction_id,
                            s_fragmentMessageContext->currentFragment + 1,
                            s_fragmentMessageContext->totalFragments,
                            s_fragmentMessageContext->totalFragments);
                    /* discard commands */
                    /* note: message would be freed in indicateHostErrorMessage */
                    indicateHostErrorMessage(message,
                            MBIM_PROTOCOL_ERROR_FRAGMENT_OUT_OF_SEQUENCE);

                    mbim_message_free(s_fragmentMessageContext->fragments);
                    free(s_fragmentMessageContext);
                    s_fragmentMessageContext = NULL;
                    return;
                } else {
                    RLOGE("Discard the waiting fragment message,"
                            " transactionID: %d, expecting fragment %u/%u, got 0/%u",
                            s_fragmentMessageContext->transaction_id,
                            s_fragmentMessageContext->currentFragment + 1,
                            s_fragmentMessageContext->totalFragments,
                            s_fragmentMessageContext->totalFragments);

                    /* a new command, discard the previous command */
                    indicateHostErrorMessage(s_fragmentMessageContext->fragments,
                            MBIM_PROTOCOL_ERROR_FRAGMENT_OUT_OF_SEQUENCE);
                    free(s_fragmentMessageContext);
                    s_fragmentMessageContext = NULL;
                }
            }

            RLOGD("handling the new fragment command, transactionId: %d",
                    mbim_message_get_transaction_id(message));

            /* a new command, fragment collector init */
            MbimProtocolError error = MBIM_PROTOCOL_ERROR_INVALID;  // as initialize error here
            MbimMessage *fragment = mbim_message_fragment_collector_init(message, &error);
            if (error != MBIM_PROTOCOL_ERROR_INVALID) {
                indicateHostErrorMessage(message, error);
            } else {
                s_fragmentMessageContext =
                        (FragmentMessageContext *)calloc(1, sizeof(FragmentMessageContext));
                s_fragmentMessageContext->fragments = fragment;
                s_fragmentMessageContext->totalFragments =
                        mbim_message_fragment_get_total(message);
                s_fragmentMessageContext->currentFragment = 0;
                s_fragmentMessageContext->transaction_id =
                        mbim_message_get_transaction_id(message);
                s_fragmentMessageContext->currentDataLength =
                        message->len;
                getNow(&(s_fragmentMessageContext->receivedTime));
            }
            return;
        }

        /* not the first fragment */
        if (s_fragmentMessageContext == NULL) {
            RLOGE("Discard the new fragment message,"
                    " transactionID: %d, expecting fragment 0/%u, got %u/%u",
                    mbim_message_get_transaction_id(message),
                    mbim_message_fragment_get_total(message),
                    mbim_message_fragment_get_current(message),
                    mbim_message_fragment_get_total(message));
            indicateHostErrorMessage(message,
                    MBIM_PROTOCOL_ERROR_FRAGMENT_OUT_OF_SEQUENCE);
            return;
        }
       /**
        * if the function receives a fragment that is not the first fragment
        * of a new command with a new transactionId, both commands shall be
        * discard. one MBIM_FUNCTION_ERROR_MSG message per TransactionId
        * shall be sent.
        */
        if (s_fragmentMessageContext->transaction_id !=
                mbim_message_get_transaction_id(message)) {
            RLOGE("Discard the new fragment message and previous fragment message,"
                    "transactionId: %d and %d",
                    mbim_message_get_transaction_id(message),
                    s_fragmentMessageContext->transaction_id);

            indicateHostErrorMessage(message,
                    MBIM_PROTOCOL_ERROR_FRAGMENT_OUT_OF_SEQUENCE);
            indicateHostErrorMessage(s_fragmentMessageContext->fragments,
                    MBIM_PROTOCOL_ERROR_FRAGMENT_OUT_OF_SEQUENCE);
            free(s_fragmentMessageContext);
            s_fragmentMessageContext = NULL;
            return;
        }

        /**
         * A function that receives fragmented messages shall send an
         * MBIM_X_ERROR_MSG if the time between the fragments exceeds 1250ms
         */
        struct timeval now;
        getNow(&now);

        if (((now.tv_sec - s_fragmentMessageContext->receivedTime.tv_sec) * 1000 +
            (now.tv_usec - s_fragmentMessageContext->receivedTime.tv_usec) / 1000)
            > MAX_TIME_BETWEEN_FRAGMENTS_MS) {
            indicateHostErrorMessage(message, MBIM_PROTOCOL_ERROR_TIMEOUT_FRAGMENT);

            mbim_message_free(s_fragmentMessageContext->fragments);
            free(s_fragmentMessageContext);
            s_fragmentMessageContext = NULL;
            return;
        }
        s_fragmentMessageContext->receivedTime = now;

        /* fragment collector */
        MbimProtocolError error = MBIM_PROTOCOL_ERROR_INVALID;  // as initialize error here
        mbim_message_fragment_collector_add(
                s_fragmentMessageContext->fragments,
                &(s_fragmentMessageContext->currentDataLength),
                message, &error);
        if (error != MBIM_PROTOCOL_ERROR_INVALID) {
            indicateHostErrorMessage(message, error);

            mbim_message_free(s_fragmentMessageContext->fragments);
            free(s_fragmentMessageContext);
            s_fragmentMessageContext = NULL;
        } else {
            s_fragmentMessageContext->currentFragment =
                    mbim_message_fragment_get_current(message);
            /* Did we get all needed fragments? */
            if (mbim_message_fragment_collector_complete(
                    s_fragmentMessageContext->fragments)) {
                /* Now, translate the whole message */
                RLOGD("Get the whole message, start process message");

                mbim_message_free(message);
                message = s_fragmentMessageContext->fragments;

                free(s_fragmentMessageContext);
                s_fragmentMessageContext = NULL;

#if MBIM_LOG_DEBUG
                for (uint32_t i = 0; i < message->len; i += 4) {
                    RLOGD("message[%d] = %02x, %02x, %02x, %02x", i / 4,
                            message->data[i], message->data[i + 1],
                            message->data[i + 2], message->data[i + 3]);
                }
#endif

                goto process;
            }
            RLOGD("fragment message, transcationID: %d, current fragment: %d/%d,"
                    " expecting the next fragment: %d",
                    s_fragmentMessageContext->transaction_id,
                    s_fragmentMessageContext->currentFragment,
                    s_fragmentMessageContext->totalFragments,
                    s_fragmentMessageContext->currentFragment + 1);
        }

        return;
    }

    /**
     * the message is not fragment message
     * check if there is a fragment message waiting for the next fragment message
     */
    if (s_fragmentMessageContext != NULL) {
        RLOGE("Discard the waiting fragment message,"
                " transactionID: %d, expecting fragment %u/%u,"
                " got another not fragmented message",
                s_fragmentMessageContext->transaction_id,
                s_fragmentMessageContext->currentFragment + 1,
                s_fragmentMessageContext->totalFragments);
        indicateHostErrorMessage(s_fragmentMessageContext->fragments,
                MBIM_PROTOCOL_ERROR_FRAGMENT_OUT_OF_SEQUENCE);

        free(s_fragmentMessageContext);
        s_fragmentMessageContext = NULL;
    }

process:
    switch (mbim_message_get_message_type(message)) {
        case MBIM_MESSAGE_TYPE_OPEN:
            RLOGD("MBIM_MESSAGE_TYPE_OPEN");
            processOpenMessage(message);
            break;
        case MBIM_MESSAGE_TYPE_CLOSE:
            RLOGD("MBIM_MESSAGE_TYPE_CLOSE");
            processCloseMessage(message);
            break;
        case MBIM_MESSAGE_TYPE_COMMAND:
            RLOGD("MBIM_MESSAGE_TYPE_COMMAND");
            processCommandMessage(message);
            break;
        default:
            RLOGE("Invalid control message from host");
            indicateHostErrorMessage(message, MBIM_PROTOCOL_ERROR_INVALID);
            break;
    }
}

void *mbim_device_loop() {
    ssize_t countRead = 0;
    uint8_t buffer[MAX_COMMAND_BYTES] = {0};
    int simCount = 1;
    int threadNum = 2;

    messageThreadsInit(simCount, threadNum);

    pthread_mutex_lock(&s_startupMutex);
    s_started = 1;
    pthread_cond_broadcast(&s_startupCond);
    pthread_mutex_unlock(&s_startupMutex);

#if 1
    for (;;) {
        countRead = 0;
        memset(buffer, 0, sizeof(buffer));

        do {
            RLOGD("before read");
            countRead = read(s_mbimDevice->fd, buffer, sizeof(buffer));
        } while (countRead < 0 && (errno == EINTR));
        RLOGD("countRead = %d", countRead);
        if (countRead < 0) {
            RLOGE("mbim_device: read failed (%s)", strerror(errno));
            break;
        }

#if MBIM_LOG_DEBUG
        for (ssize_t i = 0; i < countRead; i += 4) {
            RLOGD("data[%d] = %02x, %02x, %02x, %02x", i / 4,
                    buffer[i], buffer[i + 1], buffer[i + 2], buffer[i + 3]);
        }
#endif

        if (countRead < (ssize_t)sizeof(MbimMsgHeader)) {
            RLOGE("Invalid data length %d expected at least %d, ignore",
                    countRead, sizeof(MbimMsgHeader));
        } else {
            processMessage(buffer, countRead);
        }
    }

    // kill self to restart on error
    RLOGE("mbim device kill self to restart on error");
    kill(0, SIGKILL);
#endif

#if 0   // for test
        uint8_t buffer0[] = {0x01, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
                            0x46, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00};
        RLOGD("*******************01***************************");
        for (size_t i = 0; i < sizeof(buffer0); i += 4) {
            RLOGD("data[%d] = %02x, %02x, %02x, %02x", i / 4,
                    buffer0[i], buffer0[i + 1], buffer0[i + 2], buffer0[i + 3]);
        }
        processMessage(buffer0, sizeof(buffer0));

        RLOGD("*******************02**************************");
        uint8_t buffer1[] = {0x02, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00,
                            0x47, 0x00, 0x00, 0x00};
        for (size_t i = 0; i < sizeof(buffer1); i += 4) {
            RLOGD("data[%d] = %02x, %02x, %02x, %02x", i / 4,
                    buffer1[i], buffer1[i + 1], buffer1[i + 2], buffer1[i + 3]);
        }
        processMessage(buffer1, sizeof(buffer1));

        RLOGD("*******************03**************************");
        uint8_t buffer2[] = {0x03, 0x00, 0x00, 0x00,  0x30, 0x00, 0x00, 0x00,
                0x02, 0x00, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0xa2, 0x89, 0xcc, 0x33,
                0xbc, 0xbb, 0x8b, 0x4f,  0xb6, 0xb0, 0x13, 0x3e,
                0xc2, 0xaa, 0xe6, 0xdf,  0x01, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00};
        for (size_t i = 0; i < sizeof(buffer2); i += 4) {
            RLOGD("data[%d] = %02x, %02x, %02x, %02x", i / 4,
                    buffer2[i], buffer2[i + 1], buffer2[i + 2], buffer2[i + 3]);
        }
        processMessage(buffer2, sizeof(buffer2));
        RLOGD("********************04*************************");

        uint8_t buffer3[] = {0x03, 0x00, 0x00, 0x00,  0x30, 0x00, 0x00, 0x00,
                0x03, 0x00, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0xa2, 0x89, 0xcc, 0x33,
                0xbc, 0xbb, 0x8b, 0x4f,  0xb6, 0xb0, 0x13, 0x3e,
                0xc2, 0xaa, 0xe6, 0xdf,  0x10, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00};
        for (size_t i = 0; i < sizeof(buffer3); i += 4) {
            RLOGD("data[%d] = %02x, %02x, %02x, %02x", i / 4,
                    buffer3[i], buffer3[i + 1], buffer3[i + 2], buffer3[i + 3]);
        }
        processMessage(buffer3, sizeof(buffer3));
        RLOGD("********************05*************************");

        uint8_t buffer41[] = {
             /*  48  */
             0x03, 0x00, 0x00, 0x00,  0x80, 0x00, 0x00, 0x00,
             0x04, 0x00, 0x00, 0x00,
             0x03, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  // fragment
             0xa2, 0x89, 0xcc, 0x33,  0xbc, 0xbb, 0x8b, 0x4f,
             0xb6, 0xb0, 0x13, 0x3e,  0xc2, 0xaa, 0xe6, 0xdf,
             0x13, 0x00, 0x00, 0x00,
             0x01, 0x00, 0x00, 0x00,
             0x00, 0x01, 0x00, 0x00,  //informationBufferLength

             0x05, 0x00, 0x00, 0x00,  0x2c, 0x00, 0x00, 0x00,
             0x60, 0x00, 0x00, 0x00,  0x8c, 0x00, 0x00, 0x00,
             0x28, 0x00, 0x00, 0x00,  0xb4, 0x00, 0x00, 0x00,
             0x18, 0x00, 0x00, 0x00,  0xcc, 0x00, 0x00, 0x00,
             0x1c, 0x00, 0x00, 0x00,  0xe8, 0x00, 0x00, 0x00,
             0x18, 0x00, 0x00, 0x00,  0xa2, 0x89, 0xcc, 0x33,
             0xbc, 0xbb, 0x8b, 0x4f,  0xb6, 0xb0, 0x13, 0x3e,
             0xc2, 0xaa, 0xe6, 0xdf,  0x13, 0x00, 0x00, 0x00,
             0x01, 0x00, 0x00, 0x00,  0x02, 0x00, 0x00, 0x00,
             0x03, 0x00, 0x00, 0x00,  0x04, 0x00, 0x00, 0x00};
        processMessage(buffer41, sizeof(buffer41));
        uint8_t buffer42[] = {
                /*  20  */
                0x03, 0x00, 0x00, 0x00,  0x80, 0x00, 0x00, 0x00,
                0x04, 0x00, 0x00, 0x00,
                0x03, 0x00, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00,

                0x05, 0x00, 0x00, 0x00,  0x09, 0x00, 0x00, 0x00,
                0x06, 0x00, 0x00, 0x00,  0x0b, 0x00, 0x00, 0x00,
                0x08, 0x00, 0x00, 0x00,  0x07, 0x00, 0x00, 0x00,
                0x15, 0x00, 0x00, 0x00,  0x16, 0x00, 0x00, 0x00,
                0x0a, 0x00, 0x00, 0x00,  0x0f, 0x00, 0x00, 0x00,
                0x0c, 0x00, 0x00, 0x00,  0x10, 0x00, 0x00, 0x00,
                0x13, 0x00, 0x00, 0x00,  0x17, 0x00, 0x00, 0x00,
                0x0d, 0x00, 0x00, 0x00,  0x53, 0x3f, 0xbe, 0xeb,
                0x14, 0xfe, 0x44, 0x67,  0x9f, 0x90, 0x33, 0xa2,
                0x23, 0xe5, 0x6c, 0x3f,  0x05, 0x00, 0x00, 0x00,
                0x01, 0x00, 0x00, 0x00,  0x02, 0x00, 0x00, 0x00,

                0x03, 0x00, 0x00, 0x00,  0x04, 0x00, 0x00, 0x00,
                0x05, 0x00, 0x00, 0x00,  0xe5, 0x50, 0xa0, 0xc8,
                0x5e, 0x82, 0x47, 0x9e};
        processMessage(buffer42, sizeof(buffer42));
        uint8_t buffer43[] = {
                /*  20  */
                0x03, 0x00, 0x00, 0x00,  0x58, 0x00, 0x00, 0x00,
                0x04, 0x00, 0x00, 0x00,
                0x03, 0x00, 0x00, 0x00,  0x02, 0x00, 0x00, 0x00,

                0x82, 0xf7, 0x10, 0xab,
                0xf4, 0xc3, 0x35, 0x1f,  0x01, 0x00, 0x00, 0x00,
                0x01, 0x00, 0x00, 0x00,  0x1d, 0x2b, 0x5f, 0xf7,
                0x0a, 0xa1, 0x48, 0xb2,  0xaa, 0x52, 0x50, 0xf1,
                0x57, 0x67, 0x17, 0x4e,  0x02, 0x00, 0x00, 0x00,
                0x01, 0x00, 0x00, 0x00,  0x03, 0x00, 0x00, 0x00,
                0xc0, 0x8a, 0x26, 0xdd,  0x77, 0x18, 0x43, 0x82,
                0x84, 0x82, 0x6e, 0x0d,  0x58, 0x3c, 0x4d, 0x0e,
                0x01, 0x00, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00};
        processMessage(buffer43, sizeof(buffer43));
        RLOGD("********************06*************************");

        uint8_t buffer5[] = {
                0x03, 0x00, 0x00, 0x00,  0x34, 0x00, 0x00, 0x00,
                0x05, 0x00, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0xa2, 0x89, 0xcc, 0x33,
                0xbc, 0xbb, 0x8b, 0x4f,  0xb6, 0xb0, 0x13, 0x3e,
                0xc2, 0xaa, 0xe6, 0xdf,  0x03, 0x00, 0x00, 0x00,
                0x01, 0x00, 0x00, 0x00,  0x04, 0x00, 0x00, 0x00,
                0x01, 0x00, 0x00, 0x00};
        processMessage(buffer5, sizeof(buffer5));
        RLOGD("********************07*************************");

        uint8_t buffer6[] = {
                0x03, 0x00, 0x00, 0x00,  0x30, 0x00, 0x00, 0x00,
                0x06, 0x00, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0xa2, 0x89, 0xcc, 0x33,
                0xbc, 0xbb, 0x8b, 0x4f,  0xb6, 0xb0, 0x13, 0x3e,
                0xc2, 0xaa, 0xe6, 0xdf,  0x01, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00};
        processMessage(buffer6, sizeof(buffer6));
        RLOGD("********************08*************************");

        uint8_t buffer7[] = {
                0x03, 0x00, 0x00, 0x00,  0x54, 0x00, 0x00, 0x00,
                0x07, 0x00, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0xa2, 0x89, 0xcc, 0x33,
                0xbc, 0xbb, 0x8b, 0x4f,  0xb6, 0xb0, 0x13, 0x3e,
                0xc2, 0xaa, 0xe6, 0xdf,  0x0c, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x24, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00};
        processMessage(buffer7, sizeof(buffer7));
        RLOGD("*******************09**************************");

        uint8_t buffer8[] = {
                0x03, 0x00, 0x00, 0x00,  0x30, 0x00, 0x00, 0x00,
                0x08, 0x00, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0xa2, 0x89, 0xcc, 0x33,
                0xbc, 0xbb, 0x8b, 0x4f,  0xb6, 0xb0, 0x13, 0x3e,
                0xc2, 0xaa, 0xe6, 0xdf,  0x02, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00};
        processMessage(buffer8, sizeof(buffer8));
        RLOGD("********************10*************************");

        uint8_t buffer9[] = {
                0x03, 0x00, 0x00, 0x00,  0x30, 0x00, 0x00, 0x00,
                0x0a, 0x00, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0xa2, 0x89, 0xcc, 0x33,
                0xbc, 0xbb, 0x8b, 0x4f,  0xb6, 0xb0, 0x13, 0x3e,
                0xc2, 0xaa, 0xe6, 0xdf,  0x0b, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00};
        processMessage(buffer9, sizeof(buffer9));
        RLOGD("********************11*************************");

        uint8_t buffer10[] = {
                0x03, 0x00, 0x00, 0x00,  0x30, 0x00, 0x00, 0x00,
                0x0b, 0x00, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0xa2, 0x89, 0xcc, 0x33,
                0xbc, 0xbb, 0x8b, 0x4f,  0xb6, 0xb0, 0x13, 0x3e,
                0xc2, 0xaa, 0xe6, 0xdf,  0x09, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00};
        processMessage(buffer10, sizeof(buffer10));
        RLOGD("********************12*************************");

        uint8_t buffer11[] = {
                0x03, 0x00, 0x00, 0x00,  0x30, 0x00, 0x00, 0x00,
                0x0c, 0x00, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0xa2, 0x89, 0xcc, 0x33,
                0xbc, 0xbb, 0x8b, 0x4f,  0xb6, 0xb0, 0x13, 0x3e,
                0xc2, 0xaa, 0xe6, 0xdf,  0x03, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00};
        processMessage(buffer11, sizeof(buffer11));
        RLOGD("********************13*************************");

        uint8_t buffer12[] = {
                0x03, 0x00, 0x00, 0x00,  0x30, 0x00, 0x00, 0x00,
                0x0e, 0x00, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0xa2, 0x89, 0xcc, 0x33,
                0xbc, 0xbb, 0x8b, 0x4f,  0xb6, 0xb0, 0x13, 0x3e,
                0xc2, 0xaa, 0xe6, 0xdf,  0x0a, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00};
        processMessage(buffer12, sizeof(buffer12));
        RLOGD("********************14*************************");

        uint8_t buffer13[] = {
                0x03, 0x00, 0x00, 0x00,  0x30, 0x00, 0x00, 0x00,
                0x0f, 0x00, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0xa2, 0x89, 0xcc, 0x33,
                0xbc, 0xbb, 0x8b, 0x4f,  0xb6, 0xb0, 0x13, 0x3e,
                0xc2, 0xaa, 0xe6, 0xdf,  0x16, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00};
        processMessage(buffer13, sizeof(buffer13));
        RLOGD("********************15*************************");

        uint8_t buffer14[] = {
                0x03, 0x00, 0x00, 0x00,  0x30, 0x00, 0x00, 0x00,
                0x11, 0x00, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0xa2, 0x89, 0xcc, 0x33,
                0xbc, 0xbb, 0x8b, 0x4f,  0xb6, 0xb0, 0x13, 0x3e,
                0xc2, 0xaa, 0xe6, 0xdf,  0x04, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00};
        processMessage(buffer14, sizeof(buffer14));
        RLOGD("********************16*************************");

        uint8_t buffer15[] = {
                0x03, 0x00, 0x00, 0x00,  0x30, 0x00, 0x00, 0x00,
                0x12, 0x00, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0xa2, 0x89, 0xcc, 0x33,
                0xbc, 0xbb, 0x8b, 0x4f,  0xb6, 0xb0, 0x13, 0x3e,
                0xc2, 0xaa, 0xe6, 0xdf,  0x05, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00};
        processMessage(buffer15, sizeof(buffer15));

        RLOGD("********************17*************************");
        uint8_t buffer16[] = {
                0x03, 0x00, 0x00, 0x00,  0x30, 0x00, 0x00, 0x00,
                0x13, 0x00, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0xa2, 0x89, 0xcc, 0x33,
                0xbc, 0xbb, 0x8b, 0x4f,  0xb6, 0xb0, 0x13, 0x3e,
                0xc2, 0xaa, 0xe6, 0xdf,  0x06, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00};
        processMessage(buffer16, sizeof(buffer16));

        RLOGD("********************18*************************");
        uint8_t buffer17[] = {
                0x03, 0x00, 0x00, 0x00,  0x30, 0x00, 0x00, 0x00,
                0x14, 0x00, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0xa2, 0x89, 0xcc, 0x33,
                0xbc, 0xbb, 0x8b, 0x4f,  0xb6, 0xb0, 0x13, 0x3e,
                0xc2, 0xaa, 0xe6, 0xdf,  0x07, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00};
        processMessage(buffer17, sizeof(buffer17));

        RLOGD("********************19*************************");
        uint8_t buffer18[] = {
                0x03, 0x00, 0x00, 0x00,  0x30, 0x00, 0x00, 0x00,
                0x15, 0x00, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0xa2, 0x89, 0xcc, 0x33,
                0xbc, 0xbb, 0x8b, 0x4f,  0xb6, 0xb0, 0x13, 0x3e,
                0xc2, 0xaa, 0xe6, 0xdf,  0x18, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00};
        processMessage(buffer18, sizeof(buffer18));

        RLOGD("********************20*************************");
        uint8_t buffer19[] = {
                0x03, 0x00, 0x00, 0x00,  0x30, 0x00, 0x00, 0x00,
                0x16, 0x00, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0xa2, 0x89, 0xcc, 0x33,
                0xbc, 0xbb, 0x8b, 0x4f,  0xb6, 0xb0, 0x13, 0x3e,
                0xc2, 0xaa, 0xe6, 0xdf,  0x02, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00};
        processMessage(buffer19, sizeof(buffer19));

        RLOGD("********************21*************************");
        uint8_t buffer20[] = {
                0x03, 0x00, 0x00, 0x00,  0x9c, 0x00, 0x00, 0x00,
                0x46, 0x00, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0xa2, 0x89, 0xcc, 0x33,
                0xbc, 0xbb, 0x8b, 0x4f,  0xb6, 0xb0, 0x13, 0x3e,
                0xc2, 0xaa, 0xe6, 0xdf,  0x0c, 0x00, 0x00, 0x00,
                0x01, 0x00, 0x00, 0x00,  0x6c, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00,
                0x3c, 0x00, 0x00, 0x00,  0x30, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x7e, 0x5e, 0x2a, 0x7e,
                0x4e, 0x6f, 0x72, 0x72,  0x73, 0x6b, 0x65, 0x6e,
                0x7e, 0x5e, 0x2a, 0x7e,  0x62, 0x00, 0x6a, 0x00,
                0x6c, 0x00, 0x65, 0x00,  0x6e, 0x00, 0x6f, 0x00,
                0x76, 0x00, 0x6f, 0x00,  0x30, 0x00, 0x32, 0x00,
                0x2e, 0x00, 0x78, 0x00,  0x66, 0x00, 0x64, 0x00,
                0x7a, 0x00, 0x2e, 0x00,  0x6e, 0x00, 0x6a, 0x00,
                0x6d, 0x00, 0x32, 0x00,  0x6d, 0x00, 0x61, 0x00,
                0x70, 0x00, 0x6e, 0x00};
        processMessage(buffer20, sizeof(buffer20));

        RLOGD("********************22*************************");
        uint8_t buffer21[] = {
                0x03, 0x00, 0x00, 0x00,  0x6c, 0x00, 0x00, 0x00,
                0x47, 0x00, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0xa2, 0x89, 0xcc, 0x33,
                0xbc, 0xbb, 0x8b, 0x4f,  0xb6, 0xb0, 0x13, 0x3e,
                0xc2, 0xaa, 0xe6, 0xdf,  0x0f, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x3c, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00};
        processMessage(buffer21, sizeof(buffer21));
        RLOGD("********************23*************************");

        uint8_t buffer22[] = {
                0x03, 0x00, 0x00, 0x00,  0x3c, 0x00, 0x00, 0x00,
                0x1a, 0x00, 0x00, 0x00,  0x01, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x53, 0x3f, 0xbe, 0xeb,
                0x14, 0xfe, 0x44, 0x67,  0x9f, 0x90, 0x33, 0xa2,
                0x23, 0xe5, 0x6c, 0x3f,  0x02, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x0c, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,  0x02, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00};
        processMessage(buffer22, sizeof(buffer22));
        RLOGD("********************24*************************");

        uint8_t buffer23[] = {
                0x03, 0x00, 0x00, 0x00,  0x3c, 0x00, 0x00, 0x00,
                0x1a, 0x00, 0x00, 0x00,
                0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x63, 0x46, 0x18, 0xd3, 0xa5, 0xb1, 0x4f, 0xab,
                0x92, 0x2f, 0xa7, 0xac, 0x9a, 0x69, 0x14, 0x8c,
                0x01, 0x00, 0x00, 0x00,
                0x01, 0x00, 0x00, 0x00,
                0x28, 0x00, 0x00, 0x00,
                0x08, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
                0x41, 0x00, 0x54, 0x00, 0x2b, 0x00, 0x53, 0x00,
                0x50, 0x00, 0x53, 0x00, 0x45, 0x00, 0x52, 0x00,
                0x49, 0x00, 0x41, 0x00, 0x4c, 0x00, 0x4f, 0x00,
                0x50, 0x00, 0x54, 0x00, 0x3d, 0x00, 0x30, 0x00};
        processMessage(buffer23, sizeof(buffer23));
        RLOGD("********************24*************************");

        uint8_t buffer26[] = {
                0x03, 0x00, 0x00, 0x00,  0x3c, 0x00, 0x00, 0x00,
                0x1a, 0x00, 0x00, 0x00,
                0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                //UUID
                0x53, 0x3f, 0xbe, 0xeb,  0x14, 0xfe, 0x44, 0x67,
                0x9f, 0x90, 0x33, 0xa2,  0x23, 0xe5, 0x6c, 0x3f,
                //cid + commandType(0 for query and 1 for set) + buffer length
                0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

                0x28, 0x00, 0x00, 0x00,//12

                //pdu format
                0x00, 0x00, 0x00, 0x00,

                // offset
                0x0c, 0x00, 0x00, 0x00,

                //size
                0x1c, 0x00, 0x00, 0x00,

                //pdu
                0x08, 0x91, 0x68, 0x31, 0x08, 0x20, 0x02, 0x05,
                0xf0, 0x11, 0x00, 0x0b, 0x81, 0x51, 0x26, 0x90,
                0x28, 0x17, 0xf2, 0x00, 0x00, 0xff, 0x05, 0xd4,
                0xf2, 0x9c, 0x3e, 0x07
                };
        processMessage(buffer26, sizeof(buffer26));
        RLOGD("********************25*************************");
#endif

    return NULL;
}


MbimDevice *
mbim_device_open(const char *fileName) {
    if (fileName == NULL) {
        RLOGE("Invalid file name");
        return NULL;
    }

    int retryTimes = 0;
    MbimDevice *mbimDevice = (MbimDevice *)calloc(1, sizeof(MbimDevice));

again:
    mbimDevice->fd = open(fileName, O_RDWR);
    if (mbimDevice->fd < 0) {
        RLOGE("mbim_device: cannot open %s (%s)", fileName, strerror(errno));

        if (retryTimes < 100) {
            retryTimes++;
            sleep(1);
            goto again;
        } else {
            goto fail_open;
        }
    }

    snprintf(mbimDevice->name, sizeof(mbimDevice->name), "%s", fileName);
    mbimDevice->max_control_transfer = MAX_COMMAND_BYTES;
    mbimDevice->open_status = OPEN_STATUS_CLOSED;

    return mbimDevice;

fail_open:
    free(mbimDevice);
    return NULL;
}


void
mbim_device_start_main_loop(int         argc,
                            char**      argv) {
    MBIM_UNUSED_PARM(argc);
    MBIM_UNUSED_PARM(argv);

    char *fileName = "/dev/sprd_mbim";

#if 1
    s_mbimDevice = mbim_device_open(fileName);
    if (!s_mbimDevice) {
        RLOGE("failed to open mbim device driver: %s", fileName);
        exit(-1);
    }
    RLOGD("open /dev/sprd_mbim success, fd = %d", s_mbimDevice->fd);
    s_mbimDevice->open_status = OPEN_STATUS_CLOSED;
#endif

    pthread_t tid;
    pthread_attr_t attr;

    /* spin up mbim_device_loop thread and wait for it to get started */
    s_started = 0;
    pthread_mutex_lock(&s_startupMutex);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);


    int ret = pthread_create(&tid, &attr, mbim_device_loop, NULL);

    while (s_started == 0) {
        pthread_cond_wait(&s_startupCond, &s_startupMutex);
    }

    pthread_mutex_unlock(&s_startupMutex);
}
