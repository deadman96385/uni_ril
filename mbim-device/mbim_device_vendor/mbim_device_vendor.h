#ifndef MBIM_DEVICE_VENDOR_H
#define MBIM_DEVICE_VENDOR_H

#include "mbim_message.h"
#include "mbim_enums.h"
#include "at_channel.h"
#include "misc.h"

#define AT_COMMAND_LEN      128
#define MINIMUM_APN_LEN     19
#define MAX_AT_RESPONSE     0x1000

#define VT_DCI "\"000001B000000001B5090000010000000120008440FA282C2090A21F\""

typedef enum {
    RADIO_STATE_OFF = 0,            /* Radio explictly powered off (eg CFUN=0) */
    RADIO_STATE_UNAVAILABLE = 1,    /* Radio unavailable (eg, resetting or not booted) */
    RADIO_STATE_ON = 2              /* Radio is on */
} RIL_RadioState;

typedef enum {
    SIM_ABSENT = 0,
    SIM_NOT_READY = 1,
    SIM_READY = 2,  /* SIM_READY means radio state is RADIO_STATE_SIM_READY */
    SIM_PIN = 3,
    SIM_PUK = 4,
    SIM_NETWORK_PERSONALIZATION = 5,
    RUIM_ABSENT = 6,
    RUIM_NOT_READY = 7,
    RUIM_READY = 8,
    RUIM_PIN = 9,
    RUIM_PUK = 10,
    RUIM_NETWORK_PERSONALIZATION = 11,
    EXT_SIM_STATUS_BASE = 11,
    SIM_NETWORK_SUBSET_PERSONALIZATION = EXT_SIM_STATUS_BASE + 1,
    SIM_SERVICE_PROVIDER_PERSONALIZATION = EXT_SIM_STATUS_BASE + 2,
    SIM_CORPORATE_PERSONALIZATION = EXT_SIM_STATUS_BASE + 3,
    SIM_SIM_PERSONALIZATION = EXT_SIM_STATUS_BASE + 4,
    SIM_NETWORK_PUK = EXT_SIM_STATUS_BASE + 5,
    SIM_NETWORK_SUBSET_PUK = EXT_SIM_STATUS_BASE + 6,
    SIM_SERVICE_PROVIDER_PUK = EXT_SIM_STATUS_BASE + 7,
    SIM_CORPORATE_PUK = EXT_SIM_STATUS_BASE + 8,
    SIM_SIM_PUK = EXT_SIM_STATUS_BASE + 9,
    SIM_SIMLOCK_FOREVER = EXT_SIM_STATUS_BASE + 10
} SimStatus;

extern struct ATChannels *  s_ATChannels[MAX_AT_CHANNELS];
extern RIL_RadioState       s_radioState[SIM_COUNT];

int     getChannel      (RIL_SOCKET_ID socket_id);
void    putChannel      (int channelID);
void    setRadioState   (int channelID, RIL_RadioState newState);

void
mbim_device_command(Mbim_Token              token,
                    MbimService             service,
                    uint32_t                cid,
                    MbimMessageCommandType  commandType,
                    void *                  user_data,
                    size_t                  dataLen);

bool
notification_subscribe_check(MbimService    serviceId,
                             uint32_t       cid);

#endif  // MBIM_DEVICE_VENDOR_H
