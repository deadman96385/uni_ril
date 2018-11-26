#ifndef MBIM_SERVICE_OEM_H_
#define MBIM_SERVICE_OEM_H_

#include "mbim_message.h"
#include "mbim_uuid.h"
#include "mbim_cid.h"
#include "mbim_enums.h"
#include "mbim_error.h"

/*****************************************************************************/
/* 'OEM' struct */

/**
 * MBIM_CID_OEM:
 *                   Set                 Query          Notification
 * Command    MbimOemAtCommand                               NA
 * Response    MbimOemATResp                            MbimOemATResp
 *
 */
struct _MbimOemATCommand {
    uint32_t                ATCommandOffset;
    uint32_t                ATCommandSize;
    uint8_t                 dataBuffer[];   /* AT Command */
} __attribute__((packed));
typedef struct _MbimOemATCommand MbimOemATCommand;

struct _MbimOemATResp {
    uint32_t                ATRespOffset;
    uint32_t                ATRespSize;
    uint8_t                 dataBuffer[];   /* AT Response */
} __attribute__((packed));
typedef struct _MbimOemATResp MbimOemATResp;

/*****************************************************************************/

void
mbim_message_parser_oem(MbimMessageInfo *messageInfo);

/*****************************************************************************/
int
mbim_message_parser_oem_atci    (MbimMessageInfo *  messageInfo,
                                 char *             printBuf);
void
oem_atci_set_operation          (MbimMessageInfo *  messageInfo,
                                 char *             printBuf);
int
oem_atci_set_ready              (Mbim_Token         token,
                                 MbimStatusError    error,
                                 void *             response,
                                 size_t             responseLen,
                                 char *             printBuf);

#endif  // MBIM_SERVICE_OEM_H_
