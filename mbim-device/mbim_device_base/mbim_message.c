#define LOG_TAG "MBIM-Device"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <utils/Log.h>

#include "mbim_message.h"
#include "mbim_cid.h"

/*****************************************************************************/
/* Generic message interface */

/**
 * mbim_message_new:
 * @data: contents of the message.
 * @data_length: length of the message.
 *
 * Create a #MbimMessage with the given contents.
 *
 * Returns: (transfer full): a newly created #MbimMessage,
 * which should be freed with mbim_message_free().
 */
MbimMessage *
mbim_message_new(const uint8_t *    data,
                 int32_t            data_length) {
    uint8_t *out = NULL;
    MbimMessage *msg = (MbimMessage *)calloc(1, sizeof(MbimMessage));

    out = (uint8_t *)calloc(data_length, sizeof(uint8_t));
    memcpy(out, data, data_length);

    msg->data = out;
    msg->len = data_length;

    return msg;
}

/**
 * mbim_message_free:
 * @self: a #MbimMessage.
 *
 * free a #MbimMessage @self.
 */
void
mbim_message_free(MbimMessage *self) {
    if (self == NULL) {
        return;
    }

    free(self->data);
    free(self);
}

/**
 * mbim_message_get_transaction_id:
 * @self: a #MbimMessage.
 *
 * Gets the transaction ID of the message.
 *
 * Returns: the transaction ID.
 */
uint32_t
mbim_message_get_transaction_id(const MbimMessage *self) {
    if (self == NULL) {
        RLOGE("Invalid Message");
        return 0;
    }

    return MBIM_MESSAGE_GET_TRANSACTION_ID(self);
}

/**
 * mbim_message_get_message_type:
 * @self: a #MbimMessage.
 *
 * Gets the message type.
 *
 * Returns: a #MbimMessageType.
 */
MbimMessageType
mbim_message_get_message_type(const MbimMessage *self) {
    if (self == NULL) {
        RLOGE("MbimMessage is NULL ");
        return MBIM_MESSAGE_TYPE_INVALID;
    }

    return MBIM_MESSAGE_GET_MESSAGE_TYPE(self);
}

/**
 * mbim_message_get_message_length:
 * @self: a #MbimMessage.
 *
 * Gets the whole message length.
 *
 * Returns: the length of the message.
 */
uint32_t
mbim_message_get_message_length (const MbimMessage *self) {
    if (self == NULL) {
        RLOGE("MbimMessage is NULL ");
        return 0;
    }

    return MBIM_MESSAGE_GET_MESSAGE_LENGTH (self);
}

/**
 * mbim_message_get_information_buffer_offset:
 * @self: a #MbimMessage.
 *
 * Gets the message's informationBuffer offset.
 *
 * Returns: a uint32_t offset value.
 */
uint32_t
mbim_message_get_information_buffer_offset(const MbimMessage *self) {
    switch (MBIM_MESSAGE_GET_MESSAGE_TYPE(self)) {
        case MBIM_MESSAGE_TYPE_COMMAND:
            return (sizeof (struct header) +
                    G_STRUCT_OFFSET(struct command_message, buffer));

        case MBIM_MESSAGE_TYPE_COMMAND_DONE:
            return (sizeof (struct header) +
                    G_STRUCT_OFFSET(struct command_done_message, buffer));

        case MBIM_MESSAGE_TYPE_INDICATE_STATUS:
            return (sizeof (struct header) +
                    G_STRUCT_OFFSET(struct indicate_status_message, buffer));
        default:
            RLOGE("Invalid Message Type");
            return 0;
    }
}

/**
 * Unless otherwise specified, all fields of MBIM control messages and CID
 * related structures are in little-endian format per [USB30] section 7.1.
 * Unless otherwise specified, all strings use UNICODE UTF-16LE encodings
 * limited to characters from the Basic Multilingual Plane.
 */
char *
mbim_message_read_string(const MbimMessage *    self,
                         uint32_t               struct_start_offset,
                         uint32_t               relative_offset) {
    uint32_t offset;
    uint32_t size;
    char *str = NULL;
    uint32_t information_buffer_offset;
    unichar2 *utf16d = NULL;
    const unichar2 *utf16 = NULL;

    information_buffer_offset = mbim_message_get_information_buffer_offset (self);

    offset = UINT32_FROM_LE(G_STRUCT_MEMBER(uint32_t, self->data,
            (information_buffer_offset + relative_offset)));

    size   = UINT32_FROM_LE(G_STRUCT_MEMBER(uint32_t, self->data,
            (information_buffer_offset + relative_offset + 4)));

    if (!size) {
        return NULL;
    }

    utf16 = (const unichar2 *)G_STRUCT_MEMBER_P(self->data,
            (information_buffer_offset + struct_start_offset + offset));

//    /* For BE systems, convert from LE to BE */
//    if (G_BYTE_ORDER == G_BIG_ENDIAN) {
//        uint i;
//
//        utf16d = (unichar2 *) g_malloc (size);
//        for (i = 0; i < (size / 2); i++)
//            utf16d[i] = GUINT16_FROM_LE (utf16[i]);
//    }
    str = (char *)calloc(size / 2 + 1, sizeof(char));
    utf16_to_utf8(str, utf16, size / 2, size);
//    if (error) {
//        g_warning ("Error converting string: %s", error->message);
//        g_error_free (error);
//    }
//
//    g_free (utf16d);

    return str;
}

uint32_t
mbim_message_read_guint32(const MbimMessage *  self,
                          uint32_t             relative_offset) {
    uint32_t information_buffer_offset;

    information_buffer_offset = mbim_message_get_information_buffer_offset(self);

    return UINT32_FROM_LE(G_STRUCT_MEMBER(uint32_t, self->data,
            (information_buffer_offset + relative_offset)));
}

uint32_t *
mbim_message_read_guint32_array(const MbimMessage *    self,
                                uint32_t               array_size,
                                uint32_t               relative_offset_array_start) {
    uint i;
    uint32_t *out;
    uint32_t information_buffer_offset;

    if (!array_size) {
        return NULL;
    }

    information_buffer_offset = mbim_message_get_information_buffer_offset (self);

    out = (uint32_t *)calloc(array_size + 1, sizeof(uint32_t));
    for (i = 0; i < array_size; i++) {
        out[i] = UINT32_FROM_LE(G_STRUCT_MEMBER(uint32_t, self->data,
                (information_buffer_offset + relative_offset_array_start + (4 * i))));
    }
    out[array_size] = 0;

    return out;
}

uint64_t
mbim_message_read_guint64(const MbimMessage *  self,
                          uint64_t             relative_offset) {
    uint64_t information_buffer_offset;

    information_buffer_offset = mbim_message_get_information_buffer_offset (self);

    return UINT64_FROM_LE(G_STRUCT_MEMBER(uint64_t, self->data,
            (information_buffer_offset + relative_offset)));
}

MbimMessage *
mbim_message_allocate(MbimMessageType   message_type,
                      uint32_t          transaction_id,
                      uint32_t          additional_size) {
    MbimMessage *self = (MbimMessage *)calloc(1, sizeof(MbimMessage));
    uint32_t len = 0;;

    /* Compute size of the basic empty message and allocate heap for it */
    len = sizeof (struct header) + additional_size;
    RLOGD("mbim_message_allocate message->len = %d", len);
    self->data = (uint8_t *)calloc(len, sizeof(uint8_t));
    self->len = len;

    /* Set MBIM header */
    ((struct header *)(self->data))->type           = UINT32_TO_LE (message_type);
    ((struct header *)(self->data))->length         = UINT32_TO_LE (len);
    ((struct header *)(self->data))->transaction_id = UINT32_TO_LE (transaction_id);

    return self;
}

void
mbim_message_info_get_printtable(char *             printBuf,
                                 MbimMessageInfo *  request,
                                 LogType            flag) {
    const char *commandType = NULL;

    if (request != NULL) {
        if (request->commandType == 0) {
            commandType = "query";
        } else if (request->commandType == 1) {
            commandType = "set";
        } else {
            commandType = "unknown command type";
        }
    }

    if (flag == LOG_TYPE_COMMAND) {  // command
        snprintf(printBuf, PRINTBUF_SIZE, "[%04d]> %s: %s %s",
                request->original_transaction_id,
                mbim_service_get_printable(request->service),
                mbim_cid_get_printable(request->service, request->cid),
                commandType);
    } else if (flag == LOG_TYPE_RESPONSE) {  // response
        snprintf(printBuf, PRINTBUF_SIZE, "[%04d]< %s: %s %s",
                request->original_transaction_id,
                mbim_service_get_printable(request->service),
                mbim_cid_get_printable(request->service, request->cid),
                commandType);
    }
};

/*****************************************************************************/
/* Fragment interface */

#define MBIM_MESSAGE_IS_SUPPORTED_FRAGMENT(self)                                \
    (MBIM_MESSAGE_GET_MESSAGE_TYPE(self) == MBIM_MESSAGE_TYPE_COMMAND ||        \
     MBIM_MESSAGE_GET_MESSAGE_TYPE(self) == MBIM_MESSAGE_TYPE_COMMAND_DONE ||   \
     MBIM_MESSAGE_GET_MESSAGE_TYPE(self) == MBIM_MESSAGE_TYPE_INDICATE_STATUS)

#define MBIM_MESSAGE_FRAGMENT_GET_TOTAL(self)                           \
    UINT32_FROM_LE(((struct full_message *)(self->data))->message.fragment.fragment_header.totalFragments)

#define MBIM_MESSAGE_FRAGMENT_GET_CURRENT(self)                         \
    UINT32_FROM_LE(((struct full_message *)(self->data))->message.fragment.fragment_header.currentFragment)

bool
mbim_message_is_supported_fragment(const MbimMessage *self) {
    return MBIM_MESSAGE_IS_SUPPORTED_FRAGMENT(self);
}

uint32_t
mbim_message_fragment_get_total(const MbimMessage *self) {
    if (!MBIM_MESSAGE_IS_SUPPORTED_FRAGMENT(self)) {
        RLOGE("Invalid message Type, does not support fragment");
        return 0;
    }

    return MBIM_MESSAGE_FRAGMENT_GET_TOTAL(self);
}

uint32_t
mbim_message_fragment_get_current(const MbimMessage *self) {
    if (!MBIM_MESSAGE_IS_SUPPORTED_FRAGMENT(self)) {
        RLOGE("Invalid message Type, does not support fragment");
        return 0;
    }

    return MBIM_MESSAGE_FRAGMENT_GET_CURRENT(self);
}

const uint8_t *
mbim_message_fragment_get_payload(const MbimMessage *self,
                                  uint32_t          *length) {
    if (!MBIM_MESSAGE_IS_SUPPORTED_FRAGMENT(self)) {
        RLOGE("Invalid message Type, does not support fragment");
        *length = 0;
        return NULL;
    }

    *length = (MBIM_MESSAGE_GET_MESSAGE_LENGTH(self) - \
               sizeof(struct header) -                 \
               sizeof(struct fragment_header));
    return ((struct full_message *)(self->data))->message.fragment.buffer;
}

MbimMessage *
mbim_message_fragment_collector_init(const MbimMessage  *fragment,
                                     MbimProtocolError  *error) {
    if (!MBIM_MESSAGE_IS_SUPPORTED_FRAGMENT(fragment)) {
        RLOGE("Invalid message Type, does not support fragment");
        *error = MBIM_PROTOCOL_ERROR_UNKNOWN;
        return NULL;
    }

   /* Collector must start with fragment #0 */
   if (MBIM_MESSAGE_FRAGMENT_GET_CURRENT(fragment) != 0) {
       *error = MBIM_PROTOCOL_ERROR_FRAGMENT_OUT_OF_SEQUENCE;
       RLOGE("Expecting fragment 0/%u, got %u/%u",
               MBIM_MESSAGE_FRAGMENT_GET_TOTAL(fragment),
               MBIM_MESSAGE_FRAGMENT_GET_CURRENT(fragment),
               MBIM_MESSAGE_FRAGMENT_GET_TOTAL(fragment) );
       return NULL;
   }

   /* calculate the all fragments merging into one MbimMessage required datalen */
   /* for now, the function only consider the MBIM_COMMAND_MSG from host */
   int32_t informationBufferLength;
   const uint8_t *data = mbim_message_command_get_raw_information_buffer(
           fragment, &informationBufferLength);

   int32_t total_data_length = sizeof(struct command_message) +
           sizeof(struct header) + informationBufferLength;

   /* Reset total message length */
   ((struct header *)(fragment->data))->length = UINT32_TO_LE(total_data_length);

   MbimMessage *message = mbim_message_new(fragment->data, total_data_length);

   return message;
}

bool
mbim_message_fragment_collector_add(MbimMessage *       self,
                                    uint32_t *          currentDataLength,
                                    const MbimMessage * fragment,
                                    MbimProtocolError * error) {
    uint32_t buffer_len;
    const uint8_t *buffer;

    if (!MBIM_MESSAGE_IS_SUPPORTED_FRAGMENT(self)) {
        RLOGE("Invalid message Type, does not support fragment");
        *error = MBIM_PROTOCOL_ERROR_UNKNOWN;
        return false;
    }

    if (!MBIM_MESSAGE_IS_SUPPORTED_FRAGMENT(fragment)) {
        RLOGE("Invalid message Type, does not support fragment");
        *error = MBIM_PROTOCOL_ERROR_UNKNOWN;
        return false;
    }

    /* We can only add a fragment if it is the next one we're expecting.
     * Otherwise, we return an error. */
    if (MBIM_MESSAGE_FRAGMENT_GET_CURRENT(self) !=
            (MBIM_MESSAGE_FRAGMENT_GET_CURRENT(fragment) - 1)) {
        *error = MBIM_PROTOCOL_ERROR_FRAGMENT_OUT_OF_SEQUENCE;
        RLOGE("Expecting fragment '%u/%u', got '%u/%u'",
                MBIM_MESSAGE_FRAGMENT_GET_CURRENT (self) + 1,
                MBIM_MESSAGE_FRAGMENT_GET_TOTAL (self),
                MBIM_MESSAGE_FRAGMENT_GET_CURRENT (fragment),
                MBIM_MESSAGE_FRAGMENT_GET_TOTAL (fragment));
        return false;
    }

    buffer = mbim_message_fragment_get_payload(fragment, &buffer_len);
    if (buffer_len) {
        /* Concatenate information buffers */
        memcpy(self->data + *currentDataLength, buffer, buffer_len);

        /* Update the whole message length */
        *currentDataLength += buffer_len;
    }

    /* Update the current fragment info in the main message; skip endian changes */
    ((struct full_message *)(self->data))->message.fragment.fragment_header.currentFragment =
        ((struct full_message *)(fragment->data))->message.fragment.fragment_header.currentFragment;

    return true;
}

bool
mbim_message_fragment_collector_complete(MbimMessage *self) {
    if (!MBIM_MESSAGE_IS_SUPPORTED_FRAGMENT(self)) {
        RLOGE("Invalid message Type, does not support fragment");
        return false;
    }

    if (MBIM_MESSAGE_FRAGMENT_GET_CURRENT(self) !=
            (MBIM_MESSAGE_FRAGMENT_GET_TOTAL(self) - 1)) {
        /* Not complete yet */
        return false;
    }

    /* Reset current & total */
    ((struct full_message *)(self->data))->message.fragment.fragment_header.currentFragment = 0;
    ((struct full_message *)(self->data))->message.fragment.fragment_header.totalFragments = UINT32_TO_LE(1);
    return true;
}

struct fragment_info *
mbim_message_split_fragments(const MbimMessage *    self,
                             uint32_t               max_fragment_size,
                             uint *                 n_fragments) {
    uint32_t total_message_length;
    uint32_t total_payload_length;
    uint32_t fragment_header_length;
    uint32_t fragment_payload_length;
    uint32_t total_fragments;
    uint i;
    const uint8_t *data;
    uint32_t data_length;

    /* A message which is longer than the maximum fragment size needs to be
     * split in different fragments before sending it. */

    total_message_length = mbim_message_get_message_length (self);

    /* If a single fragment is enough, don't try to split */
    if (total_message_length <= max_fragment_size) {
        return NULL;
    }

    /* Total payload length is the total length minus the headers of the
     * input message */
    fragment_header_length = sizeof(struct header) + sizeof(struct fragment_header);
    total_payload_length = total_message_length - fragment_header_length;

    /* Fragment payload length is the maximum amount of data that can fit in a
     * single fragment */
    fragment_payload_length = max_fragment_size - fragment_header_length;

    /* We can now compute the number of fragments that we'll get */
    total_fragments = total_payload_length / fragment_payload_length;
    if (total_payload_length % fragment_payload_length) {
        total_fragments++;
    }

    struct fragment_info *fragments = (struct fragment_info *)
            calloc(total_fragments, sizeof(struct fragment_info));

    /* Initialize data walkers */
    data = ((struct full_message *)(self->data))->message.fragment.buffer;
    data_length = total_payload_length;

    uint32_t offset = 0;
    /* Create fragment infos */
    for (i = 0; i < total_fragments; i++) {
        struct fragment_info info;

        /* Set data info */
        info.data = data;
        info.data_length = (data_length > fragment_payload_length ? fragment_payload_length : data_length);

        /* Set header info */
        info.header.type                     = UINT32_TO_LE (MBIM_MESSAGE_GET_MESSAGE_TYPE(self));
        info.header.length                   = UINT32_TO_LE (fragment_header_length + info.data_length);
        info.header.transaction_id           = UINT32_TO_LE (MBIM_MESSAGE_GET_TRANSACTION_ID(self));
        info.fragment_header.totalFragments  = UINT32_TO_LE (total_fragments);
        info.fragment_header.currentFragment = UINT32_TO_LE (i);

        fragments[i] = info;

        /* Update walkers */
        data = &data[info.data_length];
        data_length -= info.data_length;
    }

    *n_fragments = total_fragments;
    return fragments;
}

/******************************************************************************/
/* 'Open' message interface */

/**
 * mbim_message_open_get_max_control_transfer:
 * @self: a #MbimMessage.
 *
 * Get the maximum control transfer set to be used in the #MbimMessage of type
 * %MBIM_MESSAGE_TYPE_OPEN.
 *
 * Returns: the maximum control transfer.
 */
uint32_t
mbim_message_open_get_max_control_transfer(const MbimMessage *self) {
    if (self == NULL ||
        MBIM_MESSAGE_GET_MESSAGE_TYPE (self) != MBIM_MESSAGE_TYPE_OPEN) {
        RLOGE("Invalid parameters");
        return 0;
    }
    return UINT32_FROM_LE(
            ((struct full_message *)(self->data))->message.open.max_control_transfer);
}

/*****************************************************************************/
/* 'Open Done' message interface */

MbimMessage *
mbim_message_open_done_new(uint32_t        transaction_id,
                           MbimStatusError error_status_code) {
    MbimMessage *self = NULL;

    self = mbim_message_allocate(MBIM_MESSAGE_TYPE_OPEN_DONE,
                                 transaction_id,
                                 sizeof(struct open_done_message));

    /* Open header */
    ((struct full_message *)(self->data))->message.open_done.status_code =
            UINT32_TO_LE(error_status_code);

    return (MbimMessage *)self;
}


/*****************************************************************************/
/* 'Close Done' message interface */

/**
 * mbim_message_close_done_new:
 * @transaction_id: transaction ID.
 * @error_status_code: a #MbimStatusError.
 *
 * Create a new #MbimMessage of type %MBIM_MESSAGE_TYPE_CLOSE_DONE with the specified
 * parameters.
 *
 * Returns: (transfer full): a newly created #MbimMessage,
 * which should be freed with mbim_message_free().
 */
MbimMessage *
mbim_message_close_done_new(uint32_t         transaction_id,
                            MbimStatusError  error_status_code) {
    MbimMessage *self = NULL;

    self = mbim_message_allocate(MBIM_MESSAGE_TYPE_CLOSE_DONE,
                                 transaction_id,
                                 sizeof (struct close_done_message));

    /* Open header */
    ((struct full_message *)(self->data))->message.close_done.status_code =
            UINT32_TO_LE (error_status_code);

    return (MbimMessage *)self;
}

/*****************************************************************************/
/* 'Command' message interface */

/**
 * mbim_message_command_get_service:
 * @self: a #MbimMessage.
 *
 * Get the service of a %MBIM_MESSAGE_TYPE_COMMAND message.
 *
 * Returns: a #MbimService.
 */
MbimService
mbim_message_command_get_service(const MbimMessage *self) {
    if (self == NULL ||
        MBIM_MESSAGE_GET_MESSAGE_TYPE (self) != MBIM_MESSAGE_TYPE_COMMAND) {
        RLOGE("Invalid parameters");
        return MBIM_SERVICE_INVALID;
    }

    return mbim_uuid_to_service((const MbimUuid *)&
            (((struct full_message *)(self->data))->message.command.service_id));
}

/**
 * mbim_message_command_get_service_id:
 * @self: a #MbimMessage.
 *
 * Get the service UUID of a %MBIM_MESSAGE_TYPE_COMMAND message.
 *
 * Returns: a #MbimUuid.
 */
const MbimUuid *
mbim_message_command_get_service_id(const MbimMessage *self) {
    if (self == NULL ||
        MBIM_MESSAGE_GET_MESSAGE_TYPE (self) != MBIM_MESSAGE_TYPE_COMMAND) {
        RLOGE("Invalid parameters");
        return NULL;
    }

    return (const MbimUuid *)&
            (((struct full_message *)(self->data))->message.command.service_id);
}

/**
 * mbim_message_command_get_cid:
 * @self: a #MbimMessage.
 *
 * Get the command id of a %MBIM_MESSAGE_TYPE_COMMAND message.
 *
 * Returns: a CID.
 */
uint32_t
mbim_message_command_get_cid(const MbimMessage *self) {
    if (self == NULL ||
        MBIM_MESSAGE_GET_MESSAGE_TYPE (self) != MBIM_MESSAGE_TYPE_COMMAND) {
        RLOGE("Invalid parameters");
        return 0;
    }

    return UINT32_FROM_LE(
            ((struct full_message *)(self->data))->message.command.command_id);
}

/**
 * mbim_message_command_get_command_type:
 * @self: a #MbimMessage.
 *
 * Get the command type of a %MBIM_MESSAGE_TYPE_COMMAND message.
 *
 * Returns: a #MbimMessageCommandType.
 */
MbimMessageCommandType
mbim_message_command_get_command_type(const MbimMessage *self) {
    if (self == NULL ||
        MBIM_MESSAGE_GET_MESSAGE_TYPE (self) != MBIM_MESSAGE_TYPE_COMMAND) {
        RLOGE("Invalid parameters");
        return MBIM_MESSAGE_COMMAND_TYPE_UNKNOWN;
    }

    return (MbimMessageCommandType)UINT32_FROM_LE(
            ((struct full_message *)(self->data))->message.command.command_type);
}

/**
 * mbim_message_command_get_raw_information_buffer:
 * @self: a #MbimMessage.
 * @length: (out): return location for the size of the output buffer.
 *
 * Gets the information buffer of the %MBIM_MESSAGE_TYPE_COMMAND message.
 *
 * Returns: (transfer none): The raw data buffer, or #NULL if empty.
 */
const uint8_t *
mbim_message_command_get_raw_information_buffer(const MbimMessage * self,
                                                int32_t *           length) {
    if (self == NULL || length == NULL ||
        MBIM_MESSAGE_GET_MESSAGE_TYPE(self) != MBIM_MESSAGE_TYPE_COMMAND) {
        RLOGE("Invalid parameters");
        return NULL;
    }

    *length = UINT32_FROM_LE(
            ((struct full_message *)(self->data))->message.command.buffer_length);

    return (*length > 0 ?
            ((struct full_message *)(self->data))->message.command.buffer :
            NULL);
}

/**
 * mbim_message_command_append:
 * @self: a #MbimMessage.
 * @buffer: raw buffer to append to the message.
 * @buffer_size: length of the data in @buffer.
 *
 * Appends the contents of @buffer to @self.
 */
void
mbim_message_command_append(MbimMessage *      response,
                            const uint8_t *    buffer,
                            uint32_t           buffer_size) {
    uint32_t offset = mbim_message_get_information_buffer_offset(response);

    memcpy((response->data) + offset, buffer, buffer_size);
}

/******************************************************************************/
/* 'Command Done' message interface */

MbimMessage *
build_command_done_message(MbimMessage     *message,
                           MbimStatusError  status,
                           uint32_t         variable_buffer_len) {
    MbimMessage *response;
    struct command_done_message *command_done;
    int32_t raw_len;
    const uint8_t *raw_data;

    RLOGD("variable_buffer_len = %d, command_done_message len = %d",
            variable_buffer_len, sizeof (struct command_done_message));

    response = (MbimMessage *)mbim_message_allocate(
            MBIM_MESSAGE_TYPE_COMMAND_DONE,
            mbim_message_get_transaction_id(message),
            sizeof (struct command_done_message) + variable_buffer_len);
    command_done = &(((struct full_message *)(response->data))->message.command_done);
    command_done->fragment_header.totalFragments   = UINT32_TO_LE (1);
    command_done->fragment_header.currentFragment = 0;
    const MbimUuid *uuid = NULL;
    uuid = mbim_message_command_get_service_id(message);
    if (uuid == NULL) {
        uuid = MBIM_UUID_INVALID;
    }
    memcpy(command_done->service_id, uuid, sizeof(MbimUuid));
    command_done->command_id  = UINT32_TO_LE (mbim_message_command_get_cid(message));
    command_done->status_code = UINT32_TO_LE (status);
    command_done->buffer_length = variable_buffer_len;

    return response;
}


/******************************************************************************/
/* 'Indicate Status' message interface */

MbimMessage *
build_indicate_status_message(MbimService       serviceId,
                              uint32_t          cid,
                              uint32_t          variable_buffer_len) {
    MbimMessage *response;
    struct indicate_status_message *indicate_status;
    int32_t raw_len;
    const uint8_t *raw_data;

    response = (MbimMessage *)mbim_message_allocate(
            MBIM_MESSAGE_TYPE_INDICATE_STATUS, 0,
            sizeof(struct indicate_status_message) + variable_buffer_len);

    indicate_status = &(((struct full_message *)(response->data))->message.indicate_status);
    indicate_status->fragment_header.totalFragments = UINT32_TO_LE (1);
    indicate_status->fragment_header.currentFragment = 0;
    memcpy(indicate_status->service_id, mbim_uuid_from_service(serviceId), sizeof(MbimUuid));
    indicate_status->command_id  = UINT32_TO_LE(cid);
    indicate_status->buffer_length = variable_buffer_len;

    return response;
}

/******************************************************************************/
/* 'Host Error' message interface */

/**
 * mbim_message_function_error_new:
 * @transaction_id: transaction ID.
 * @error_status_code: a #MbimProtocolError.
 *
 * Create a new #MbimMessage of type %MBIM_MESSAGE_TYPE_FUNCTION_ERROR with the specified
 * parameters.
 *
 * Returns: (transfer full): a newly created #MbimMessage. The returned value
 * should be freed with mbim_message_unref().
 */
MbimMessage *
mbim_message_function_error_new(uint32_t          transaction_id,
                                MbimProtocolError error_status_code) {
    MbimMessage *self = NULL;

    self = mbim_message_allocate(MBIM_MESSAGE_TYPE_FUNCTION_ERROR,
                                 transaction_id,
                                 sizeof (struct error_message));

    /* Open header */
    ((struct full_message *)(self->data))->message.error.error_status_code =
            UINT32_TO_LE(error_status_code);

    return (MbimMessage *)self;
}

/*****************************************************************************/
/* Other helpers */

void
memsetAndFreeStrings(int numPointers, ...) {
    va_list ap;

    va_start(ap, numPointers);
    for (int i = 0; i < numPointers; i++) {
        char *ptr = va_arg(ap, char *);
        if (ptr) {
#ifdef MEMSET_FREED
#define MAX_STRING_LENGTH 4096
            memset(ptr, 0, strnlen(ptr, MAX_STRING_LENGTH));
#endif
            free(ptr);
        }
    }
    va_end(ap);
}

void
memsetAndFreeUnichar2Arrays(int numPointers, ...) {
    va_list ap;

    va_start(ap, numPointers);
    for (int i = 0; i < numPointers; i++) {
        unichar2 *ptr = va_arg(ap, unichar2 *);
        if (ptr) {
            free(ptr);
        }
    }
    va_end(ap);
}

/*
 * UTF conversion codes are Copied from exfat tools.
 */
static const char *
utf8_to_wchar(const char *  input,
              wchar_t *     wc,
              size_t        insize) {
    if ((input[0] & 0x80) == 0 && insize >= 1) {
        *wc = (wchar_t) input[0];
        return input + 1;
    }
    if ((input[0] & 0xe0) == 0xc0 && insize >= 2) {
        *wc = (((wchar_t) input[0] & 0x1f) << 6) |
               ((wchar_t) input[1] & 0x3f);
        return input + 2;
    }
    if ((input[0] & 0xf0) == 0xe0 && insize >= 3) {
        *wc = (((wchar_t) input[0] & 0x0f) << 12) |
              (((wchar_t) input[1] & 0x3f) << 6) |
               ((wchar_t) input[2] & 0x3f);
        return input + 3;
    }
    if ((input[0] & 0xf8) == 0xf0 && insize >= 4) {
        *wc = (((wchar_t) input[0] & 0x07) << 18) |
              (((wchar_t) input[1] & 0x3f) << 12) |
              (((wchar_t) input[2] & 0x3f) << 6) |
               ((wchar_t) input[3] & 0x3f);
        return input + 4;
    }
    if ((input[0] & 0xfc) == 0xf8 && insize >= 5) {
        *wc = (((wchar_t) input[0] & 0x03) << 24) |
              (((wchar_t) input[1] & 0x3f) << 18) |
              (((wchar_t) input[2] & 0x3f) << 12) |
              (((wchar_t) input[3] & 0x3f) << 6) |
               ((wchar_t) input[4] & 0x3f);
        return input + 5;
    }
    if ((input[0] & 0xfe) == 0xfc && insize >= 6) {
        *wc = (((wchar_t) input[0] & 0x01) << 30) |
              (((wchar_t) input[1] & 0x3f) << 24) |
              (((wchar_t) input[2] & 0x3f) << 18) |
              (((wchar_t) input[3] & 0x3f) << 12) |
              (((wchar_t) input[4] & 0x3f) << 6) |
               ((wchar_t) input[5] & 0x3f);
        return input + 6;
    }
    return NULL;
}

static u_int16_t *
wchar_to_utf16(u_int16_t *  output,
               wchar_t      wc,
               size_t       outsize) {
    if (wc <= 0xffff) {
        if (outsize == 0)
            return NULL;
        output[0] = cpu_to_le16(wc);
        return output + 1;
    }
    if (outsize < 2)
        return NULL;
    wc -= 0x10000;
    output[0] = cpu_to_le16(0xd800 | ((wc >> 10) & 0x3ff));
    output[1] = cpu_to_le16(0xdc00 | (wc & 0x3ff));
    return output + 2;
}

int
utf8_to_utf16(u_int16_t *   output,
              const char *  input,
              size_t        outsize,
              size_t        insize) {
    const char *inp = input;
    u_int16_t *outp = output;
    wchar_t wc;

    while ((size_t)(inp - input) < insize && *inp) {
        inp = utf8_to_wchar(inp, &wc, insize - (inp - input));
        if (inp == NULL) {
            RLOGE("illegal UTF-8 sequence\n");
            return -EILSEQ;
        }
        outp = wchar_to_utf16(outp, wc, outsize - (outp - output));
        if (outp == NULL) {
            RLOGE("name is too long\n");
            return -ENAMETOOLONG;
        }
    }
    *outp = cpu_to_le16(0);
    return 0;
}

static const u_int16_t *
utf16_to_wchar(const u_int16_t *    input,
               wchar_t *            wc,
               size_t               insize) {
    if ((le16_to_cpu(input[0]) & 0xfc00) == 0xd800) {
        if (insize < 2 || (le16_to_cpu(input[1]) & 0xfc00) != 0xdc00)
            return NULL;
        *wc = ((wchar_t) (le16_to_cpu(input[0]) & 0x3ff) << 10);
        *wc |= (le16_to_cpu(input[1]) & 0x3ff);
        *wc += 0x10000;
        return input + 2;
    } else {
        *wc = le16_to_cpu(*input);
        return input + 1;
    }
}

static char *
wchar_to_utf8(char *    output,
              wchar_t   wc,
              size_t    outsize) {
    if (wc <= 0x7f) {
        if (outsize < 1)
            return NULL;
        *output++ = (char) wc;
    } else if (wc <= 0x7ff) {
        if (outsize < 2)
            return NULL;
        *output++ = 0xc0 | (wc >> 6);
        *output++ = 0x80 | (wc & 0x3f);
    } else if (wc <= 0xffff) {
        if (outsize < 3)
            return NULL;
        *output++ = 0xe0 | (wc >> 12);
        *output++ = 0x80 | ((wc >> 6) & 0x3f);
        *output++ = 0x80 | (wc & 0x3f);
    } else if (wc <= 0x1fffff) {
        if (outsize < 4)
            return NULL;
        *output++ = 0xf0 | (wc >> 18);
        *output++ = 0x80 | ((wc >> 12) & 0x3f);
        *output++ = 0x80 | ((wc >> 6) & 0x3f);
        *output++ = 0x80 | (wc & 0x3f);
    } else if (wc <= 0x3ffffff) {
        if (outsize < 5)
            return NULL;
        *output++ = 0xf8 | (wc >> 24);
        *output++ = 0x80 | ((wc >> 18) & 0x3f);
        *output++ = 0x80 | ((wc >> 12) & 0x3f);
        *output++ = 0x80 | ((wc >> 6) & 0x3f);
        *output++ = 0x80 | (wc & 0x3f);
    } else if (wc <= 0x7fffffff) {
        if (outsize < 6)
            return NULL;
        *output++ = 0xfc | (wc >> 30);
        *output++ = 0x80 | ((wc >> 24) & 0x3f);
        *output++ = 0x80 | ((wc >> 18) & 0x3f);
        *output++ = 0x80 | ((wc >> 12) & 0x3f);
        *output++ = 0x80 | ((wc >> 6) & 0x3f);
        *output++ = 0x80 | (wc & 0x3f);
    } else
        return NULL;

    return output;
}

int
utf16_to_utf8(char *            output,
              const u_int16_t * input,
              size_t            outsize,
              size_t            insize) {
    const u_int16_t *inp = input;
    char *outp = output;
    wchar_t wc;

    while ((size_t)(inp - input) < insize && le16_to_cpu(*inp)) {
        inp = utf16_to_wchar(inp, &wc, insize - (inp - input));
        if (inp == NULL) {
            RLOGE("illegal UTF-16 sequence\n");
            return -EILSEQ;
        }
        outp = wchar_to_utf8(outp, wc, outsize - (outp - output));
        if (outp == NULL) {
            RLOGE("name is too long\n");
            return -ENAMETOOLONG;
        }
    }
    *outp = '\0';
    return 0;
}
