
#ifndef MBIM_MESSAGE_PROCESSER_H_
#define MBIM_MESSAGE_PROCESSER_H_

#include "mbim_message.h"
#include "mbim_enums.h"

#define MAX_COMMAND_BYTES (4 * 1024)

#define MBIM_LOG_DEBUG 0

typedef enum {
    OPEN_STATUS_CLOSED  = 0,
    OPEN_STATUS_OPENING = 1,
    OPEN_STATUS_OPEN    = 2
} OpenStatus;

typedef struct {
    /* File */
    int fd;

    char name[32];

    OpenStatus open_status;

    /* message size */
    uint16_t max_control_transfer;
} MbimDevice;

/*****************************************************************************/
void
mbim_command_message_parser (MbimMessageInfo *  messageInfo);

void
mbim_device_command_done    (Mbim_Token         request,
                             MbimStatusError    error,
                             void *             response,
                             size_t             responseLen);
void
mbim_command_complete       (Mbim_Token         token,
                             MbimStatusError    status,
                             void *             data,
                             size_t             dataLen);
void
mbim_device_indicate_status (MbimService        serviceId,
                             uint32_t           cid,
                             void *             data,
                             size_t             dataLen);

#endif  // MBIM_MESSAGE_PROCESSER_H_
