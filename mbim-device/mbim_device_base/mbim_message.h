#ifndef MBIM_MESSAGE_H_
#define MBIM_MESSAGE_H_

#include "mbim_uuid.h"
#include "mbim_error.h"

/*****************************************************************************/
/* The MbimMessage */

typedef struct _MbimMessage {
  uint8_t *data;
  uint   len;
} MbimMessage;

/*****************************************************************************/
/* Basic message types */

struct header {
  uint32_t type;
  uint32_t length;
  uint32_t transaction_id;
} __attribute__((packed));
typedef struct header MbimMsgHeader;

struct fragment_header {
  uint32_t totalFragments;
  uint32_t currentFragment;
} __attribute__((packed));

struct open_message {
    uint32_t max_control_transfer;
} __attribute__((packed));

struct open_done_message {
    uint32_t status_code;
} __attribute__((packed));

struct close_done_message {
    uint32_t status_code;
} __attribute__((packed));

struct error_message {
    uint32_t error_status_code;
} __attribute__((packed));

struct fragment_message {
    struct fragment_header fragment_header;
    uint8_t                buffer[];
} __attribute__((packed));

struct command_message {
    struct fragment_header fragment_header;
    uint8_t                service_id[16];
    uint32_t               command_id;
    uint32_t               command_type;
    uint32_t               buffer_length;
    uint8_t                buffer[];
} __attribute__((packed));

struct command_done_message {
    struct fragment_header fragment_header;
    uint8_t                service_id[16];
    uint32_t               command_id;
    uint32_t               status_code;
    uint32_t               buffer_length;
    uint8_t                buffer[];
} __attribute__((packed));

struct indicate_status_message {
    struct fragment_header fragment_header;
    uint8_t                service_id[16];
    uint32_t               command_id;
    uint32_t               buffer_length;
    uint8_t                buffer[];
} __attribute__((packed));

struct full_message {
    struct header header;
    union {
        struct open_message            open;
        struct open_done_message       open_done;
        /* nothing needed for close_message */
        struct close_done_message      close_done;
        struct error_message           error;
        struct fragment_message        fragment;
        struct command_message         command;
        struct command_done_message    command_done;
        struct indicate_status_message indicate_status;
    } message;
} __attribute__((packed));

/*****************************************************************************/
/* the OffsetLengthPairList type */

struct _OffsetLengthPairList {
    uint32_t offset;
    uint32_t length;
}__attribute__((packed));
typedef struct _OffsetLengthPairList OffsetLengthPairList;

/*****************************************************************************/
/**
 * MbimMessageType:
 * @MBIM_MESSAGE_TYPE_INVALID: Invalid MBIM message.
 * @MBIM_MESSAGE_TYPE_OPEN: Initialization request.
 * @MBIM_MESSAGE_TYPE_CLOSE: Close request.
 * @MBIM_MESSAGE_TYPE_COMMAND: Command request.
 * @MBIM_MESSAGE_TYPE_HOST_ERROR: Host-reported error in the communication.
 * @MBIM_MESSAGE_TYPE_OPEN_DONE: Response to initialization request.
 * @MBIM_MESSAGE_TYPE_CLOSE_DONE: Response to close request.
 * @MBIM_MESSAGE_TYPE_COMMAND_DONE: Response to command request.
 * @MBIM_MESSAGE_TYPE_FUNCTION_ERROR: Function-reported error in the communication.
 * @MBIM_MESSAGE_TYPE_INDICATE_STATUS: Unsolicited message from the function.
 *
 * Type of MBIM messages.
 */
typedef enum {
    MBIM_MESSAGE_TYPE_INVALID         = 0x00000000,
    /* From Host to Function */
    MBIM_MESSAGE_TYPE_OPEN            = 0x00000001,
    MBIM_MESSAGE_TYPE_CLOSE           = 0x00000002,
    MBIM_MESSAGE_TYPE_COMMAND         = 0x00000003,
    MBIM_MESSAGE_TYPE_HOST_ERROR      = 0x00000004,
    /* From Function to Host */
    MBIM_MESSAGE_TYPE_OPEN_DONE       = 0x80000001,
    MBIM_MESSAGE_TYPE_CLOSE_DONE      = 0x80000002,
    MBIM_MESSAGE_TYPE_COMMAND_DONE    = 0x80000003,
    MBIM_MESSAGE_TYPE_FUNCTION_ERROR  = 0x80000004,
    MBIM_MESSAGE_TYPE_INDICATE_STATUS = 0x80000007
} MbimMessageType;

/**
 * MbimMessageCommandType:
 * @MBIM_MESSAGE_COMMAND_TYPE_UNKNOWN: Unknown type.
 * @MBIM_MESSAGE_COMMAND_TYPE_QUERY: Query command.
 * @MBIM_MESSAGE_COMMAND_TYPE_SET: Set command.
 *
 * Type of command message.
 */
typedef enum {
    MBIM_MESSAGE_COMMAND_TYPE_UNKNOWN = -1,
    MBIM_MESSAGE_COMMAND_TYPE_QUERY   = 0,
    MBIM_MESSAGE_COMMAND_TYPE_SET     = 1
} MbimMessageCommandType;


typedef void * Mbim_Token;

typedef int (*AsyncReadyCallback)(Mbim_Token       request,
                                  MbimStatusError  error,
                                  void *           response,
                                  size_t           responseLen,
                                  char *           printBuf);
typedef enum {
    MBIM_DEVICE_COMMUNICATION,
    MBIM_ATCI_COMMUNICATION,
} MbimCommunicationType;

/* MbimMessage info for process */
typedef struct {
    MbimMessage             *message;
    uint32_t                original_transaction_id;
    MbimService             service;
    uint32_t                cid;
    MbimMessageCommandType  commandType;
    AsyncReadyCallback      callback;
    MbimCommunicationType   communicationType;
} MbimMessageInfo;

typedef int (*IndicateStatusCallback)(void *    data,
                                      size_t    dataLen,
                                      void **   response,
                                      size_t *  responseLen,
                                      char *    printBuf);

/* indicate status messages' callback type */
typedef struct {
    uint32_t                cid;
    IndicateStatusCallback  callback;
} IndicateStatusInfo;

/*****************************************************************************/
/* other helpers' define */

typedef enum {
    LOG_TYPE_COMMAND,
    LOG_TYPE_RESPONSE,
    LOG_TYPE_NOTIFICATION
} LogType;

#define PRINTBUF_SIZE                   2048

#define startCommand(printBuf)          snprintf(printBuf, PRINTBUF_SIZE, "%s (", printBuf)
#define closeCommand(printBuf)          snprintf(printBuf, PRINTBUF_SIZE, "%s)", printBuf)
#define printCommand(printBuf)          RLOGD("%s", printBuf)

#define startResponse(printBuf)         snprintf(printBuf, PRINTBUF_SIZE, "%s {", printBuf)
#define closeResponse(printBuf)         snprintf(printBuf, PRINTBUF_SIZE, "%s}", printBuf)
#define printResponse(printBuf)         RLOGD("%s", printBuf)

#define appendPrintBuf(printBuf, x...)  snprintf(printBuf, PRINTBUF_SIZE, x)


typedef uint16_t                        unichar2;
#define MEMSET_FREED                    1
#define PAD_SIZE(s)                     (((s)+3)&~3)

#define le16_to_cpu(x)                  ((__u16)(x))
#define cpu_to_le16(x)                  ((__u16)(x))

#define UINT32_TO_LE(val)               ((uint32_t)(val))
#define UINT32_FROM_LE(val)             ((uint32_t)(val))
#define UINT64_TO_LE(val)               ((uint64_t) (val))
#define UINT64_FROM_LE(val)             (UINT64_TO_LE (val))

#define OFFSET(size, offset)            (size == 0 ? 0 : offset)


#define G_STRUCT_OFFSET(struct_type, member)                    \
    ((long)((uint8_t *) &((struct_type *)0)->member))

#define G_STRUCT_MEMBER_P(struct_p, struct_offset)              \
    ((void *) ((uint8_t*) (struct_p) + (long) (struct_offset)))

#define G_STRUCT_MEMBER(member_type, struct_p, struct_offset)   \
    (*(member_type*) G_STRUCT_MEMBER_P ((struct_p), (struct_offset)))

#define MBIM_MESSAGE_GET_MESSAGE_TYPE(self)                             \
    (MbimMessageType)UINT32_FROM_LE(((MbimMsgHeader *)(self->data))->type)
#define MBIM_MESSAGE_GET_MESSAGE_LENGTH(self)                           \
    UINT32_FROM_LE(((MbimMsgHeader *)(self->data))->length)
#define MBIM_MESSAGE_GET_TRANSACTION_ID(self)                           \
    UINT32_FROM_LE(((MbimMsgHeader *)(self->data))->transaction_id)

/*****************************************************************************/
/* Generic message interface */

MbimMessage *
mbim_message_new                (const uint8_t *    data,
                                 int32_t            data_length);

void
mbim_message_free               (MbimMessage *      self);

MbimMessageType
mbim_message_get_message_type   (const MbimMessage *self);

uint32_t
mbim_message_get_message_length (const MbimMessage *self);

uint32_t
mbim_message_get_transaction_id (const MbimMessage *self);

uint32_t
mbim_message_get_information_buffer_offset(const MbimMessage *self);

char *
mbim_message_read_string        (const MbimMessage *    self,
                                 uint32_t               struct_start_offset,
                                 uint32_t               relative_offset);

uint32_t
mbim_message_read_guint32       (const MbimMessage *    self,
                                 uint32_t               relative_offset);

uint32_t *
mbim_message_read_guint32_array (const MbimMessage *    self,
                                 uint32_t               array_size,
                                 uint32_t               relative_offset_array_start);

uint64_t
mbim_message_read_guint64       (const MbimMessage *    self,
                                 uint64_t               relative_offset);

MbimMessage *
mbim_message_allocate           (MbimMessageType        message_type,
                                 uint32_t               transaction_id,
                                 uint32_t               additional_size);

void
mbim_message_info_get_printtable(char *                 printBuf,
                                 MbimMessageInfo *      request,
                                 LogType                flag);

/*****************************************************************************/
/* Fragment interface */

bool
mbim_message_is_supported_fragment      (const MbimMessage *    self);

uint32_t
mbim_message_fragment_get_total         (const MbimMessage *    self);

uint32_t
mbim_message_fragment_get_current       (const MbimMessage *    self);

const uint8_t *
mbim_message_fragment_get_payload       (const MbimMessage *    self,
                                         uint32_t *             length);

/* Merge fragments into a message... */

MbimMessage *
mbim_message_fragment_collector_init    (const MbimMessage  *   fragment,
                                         MbimProtocolError *    error);

bool
mbim_message_fragment_collector_add     (MbimMessage *          self,
                                         uint32_t *             currentDataLength,
                                         const MbimMessage *    fragment,
                                         MbimProtocolError *    error);

bool
mbim_message_fragment_collector_complete (MbimMessage *         self);

/* Split message into fragments... */

struct fragment_info {
    struct header           header;
    struct fragment_header  fragment_header;
    uint32_t                data_length;
    const uint8_t *         data;
} __attribute__((packed));

struct fragment_info *
mbim_message_split_fragments (const MbimMessage *   self,
                              uint32_t              max_fragment_size,
                              uint *                n_fragments);

/*****************************************************************************/
/* 'Open' message interface */

uint32_t
mbim_message_open_get_max_control_transfer(const MbimMessage *self);

/*****************************************************************************/
/* 'Open Done' message interface */

MbimMessage *
mbim_message_open_done_new (uint32_t        transaction_id,
                            MbimStatusError error_status_code);

/*****************************************************************************/
/* 'Close Done' message interface */

MbimMessage *
mbim_message_close_done_new (uint32_t         transaction_id,
                             MbimStatusError  error_status_code);

/*****************************************************************************/
/* 'Command' message interface */

MbimService
mbim_message_command_get_service        (const MbimMessage *self);

const MbimUuid *
mbim_message_command_get_service_id     (const MbimMessage *self);

uint32_t
mbim_message_command_get_cid            (const MbimMessage *self);

MbimMessageCommandType
mbim_message_command_get_command_type   (const MbimMessage *self);

const uint8_t *
mbim_message_command_get_raw_information_buffer(const MbimMessage * self,
                                                int32_t *           length);

void
mbim_message_command_append             (MbimMessage  *     response,
                                         const uint8_t *    buffer,
                                         uint32_t           buffer_size);

/*****************************************************************************/
/* 'Command Done' message interface */

MbimMessage *
build_command_done_message(MbimMessage     *message,
                           MbimStatusError  status,
                           uint32_t         variable_buffer_len);

/*****************************************************************************/
/* 'Indicate Status' message interface */

MbimMessage *
build_indicate_status_message(MbimService       serviceId,
                              uint32_t          cid,
                              uint32_t          variable_buffer_len);

/******************************************************************************/
/* 'Host Error' message interface */

MbimMessage *
mbim_message_function_error_new(uint32_t          transaction_id,
                                MbimProtocolError error_status_code);

/*****************************************************************************/
/* Other helpers */

void
memsetAndFreeStrings        (int            numPointers, ...);

void
memsetAndFreeUnichar2Arrays (int            numPointers, ...);

int
utf8_to_utf16               (u_int16_t *    output,
                             const char *   input,
                             size_t         outsize,
                             size_t         insize);

int
utf16_to_utf8               (char *         output,
                             const u_int16_t *input,
                             size_t         outsize,
                             size_t         insize);


#endif  // MBIM_MESSAGE_H_
