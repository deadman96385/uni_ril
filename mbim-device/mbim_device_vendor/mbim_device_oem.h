#ifndef MBIM_DEVICE_OEM_H_
#define MBIM_DEVICE_OEM_H_

#include "mbim_message.h"
#include "mbim_device_config.h"
#include "mbim_cid.h"

void
mbim_device_oem         (Mbim_Token                token,
                         uint32_t                  cid,
                         MbimMessageCommandType    commandType,
                         void *                    data,
                         size_t                    dataLen);

void
basic_connect_oem_atci  (Mbim_Token                 token,
                         MbimMessageCommandType     commandType,
                         void *                     data,
                         size_t                     dataLen);
void
send_at_command         (Mbim_Token                 token,
                         void *                     data,
                         size_t                     dataLen);

#endif  // MBIM_DEVICE_OEM_H_
