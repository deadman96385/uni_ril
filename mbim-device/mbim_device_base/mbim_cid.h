#ifndef MBIM_CID_H_
#define MBIM_CID_H_

#include "mbim_uuid.h"

/**
 * SECTION: mbim-cid
 * @title: Command IDs
 * @short_description: Generic command handling routines.
 *
 * This section defines the interface of the known command IDs.
 */
typedef struct {
    bool set;
    bool query;
    bool notify;
} CidConfig;

#define NO_SET      false
#define NO_QUERY    false
#define NO_NOTIFY   false

#define SET         true
#define QUERY       true
#define NOTIFY      true

/**
 * MbimCidBasicConnect:
 * @MBIM_CID_BASIC_CONNECT_UNKNOWN: Unknown command.
 * @MBIM_CID_BASIC_CONNECT_DEVICE_CAPS: Device capabilities.
 * @MBIM_CID_BASIC_CONNECT_SUBSCRIBER_READY_STATUS: Subscriber ready status.
 * @MBIM_CID_BASIC_CONNECT_RADIO_STATE: Radio state.
 * @MBIM_CID_BASIC_CONNECT_PIN: PIN.
 * @MBIM_CID_BASIC_CONNECT_PIN_LIST: PIN list.
 * @MBIM_CID_BASIC_CONNECT_HOME_PROVIDER: Home provider.
 * @MBIM_CID_BASIC_CONNECT_PREFERRED_PROVIDERS: Preferred providers.
 * @MBIM_CID_BASIC_CONNECT_VISIBLE_PROVIDERS: Visible providers.
 * @MBIM_CID_BASIC_CONNECT_REGISTER_STATE: Register state.
 * @MBIM_CID_BASIC_CONNECT_PACKET_SERVICE: Packet service.
 * @MBIM_CID_BASIC_CONNECT_SIGNAL_STATE: Signal state.
 * @MBIM_CID_BASIC_CONNECT_CONNECT: Connect.
 * @MBIM_CID_BASIC_CONNECT_PROVISIONED_CONTEXTS: Provisioned contexts.
 * @MBIM_CID_BASIC_CONNECT_SERVICE_ACTIVATION: Service activation.
 * @MBIM_CID_BASIC_CONNECT_IP_CONFIGURATION: IP configuration.
 * @MBIM_CID_BASIC_CONNECT_DEVICE_SERVICES: Device services.
 * @MBIM_CID_BASIC_CONNECT_DEVICE_SERVICE_SUBSCRIBE_LIST: Device service subscribe list.
 * @MBIM_CID_BASIC_CONNECT_PACKET_STATISTICS: Packet statistics.
 * @MBIM_CID_BASIC_CONNECT_NETWORK_IDLE_HINT: Network idle hint.
 * @MBIM_CID_BASIC_CONNECT_EMERGENCY_MODE: Emergency mode.
 * @MBIM_CID_BASIC_CONNECT_IP_PACKET_FILTERS: IP packet filters.
 * @MBIM_CID_BASIC_CONNECT_MULTICARRIER_PROVIDERS: Multicarrier providers.
 *
 * MBIM commands in the %MBIM_SERVICE_BASIC_CONNECT service.
 */
typedef enum {
    MBIM_CID_BASIC_CONNECT_UNKNOWN                        = 0,
    MBIM_CID_BASIC_CONNECT_DEVICE_CAPS                    = 1,
    MBIM_CID_BASIC_CONNECT_SUBSCRIBER_READY_STATUS        = 2,
    MBIM_CID_BASIC_CONNECT_RADIO_STATE                    = 3,
    MBIM_CID_BASIC_CONNECT_PIN                            = 4,
    MBIM_CID_BASIC_CONNECT_PIN_LIST                       = 5,
    MBIM_CID_BASIC_CONNECT_HOME_PROVIDER                  = 6,
    MBIM_CID_BASIC_CONNECT_PREFERRED_PROVIDERS            = 7,
    MBIM_CID_BASIC_CONNECT_VISIBLE_PROVIDERS              = 8,
    MBIM_CID_BASIC_CONNECT_REGISTER_STATE                 = 9,
    MBIM_CID_BASIC_CONNECT_PACKET_SERVICE                 = 10,
    MBIM_CID_BASIC_CONNECT_SIGNAL_STATE                   = 11,
    MBIM_CID_BASIC_CONNECT_CONNECT                        = 12,
    MBIM_CID_BASIC_CONNECT_PROVISIONED_CONTEXTS           = 13,
    MBIM_CID_BASIC_CONNECT_SERVICE_ACTIVATION             = 14,
    MBIM_CID_BASIC_CONNECT_IP_CONFIGURATION               = 15,
    MBIM_CID_BASIC_CONNECT_DEVICE_SERVICES                = 16,
    /* 17, 18 reserved */
    MBIM_CID_BASIC_CONNECT_DEVICE_SERVICE_SUBSCRIBE_LIST  = 19,
    MBIM_CID_BASIC_CONNECT_PACKET_STATISTICS              = 20,
    MBIM_CID_BASIC_CONNECT_NETWORK_IDLE_HINT              = 21,
    MBIM_CID_BASIC_CONNECT_EMERGENCY_MODE                 = 22,
    MBIM_CID_BASIC_CONNECT_IP_PACKET_FILTERS              = 23,
    MBIM_CID_BASIC_CONNECT_MULTICARRIER_PROVIDERS         = 24,
    MBIM_CID_BASIC_CONNECT_LAST                           =
            MBIM_CID_BASIC_CONNECT_MULTICARRIER_PROVIDERS,
} MbimCidBasicConnect;

/**
 * MbimCidSms:
 * @MBIM_CID_SMS_UNKNOWN: Unknown command.
 * @MBIM_CID_SMS_CONFIGURATION: SMS configuration.
 * @MBIM_CID_SMS_READ: Read.
 * @MBIM_CID_SMS_SEND: Send.
 * @MBIM_CID_SMS_DELETE: Delete.
 * @MBIM_CID_SMS_MESSAGE_STORE_STATUS: Store message status.
 *
 * MBIM commands in the %MBIM_SERVICE_SMS service.
 */
typedef enum {
    MBIM_CID_SMS_UNKNOWN              = 0,
    MBIM_CID_SMS_CONFIGURATION        = 1,
    MBIM_CID_SMS_READ                 = 2,
    MBIM_CID_SMS_SEND                 = 3,
    MBIM_CID_SMS_DELETE               = 4,
    MBIM_CID_SMS_MESSAGE_STORE_STATUS = 5,
    MBIM_CID_SMS_LAST                 =
            MBIM_CID_SMS_MESSAGE_STORE_STATUS,
} MbimCidSms;

/**
 * MbimCidUssd:
 * @MBIM_CID_USSD_UNKNOWN: Unknown command.
 * @MBIM_CID_USSD: USSD operation.
 *
 * MBIM commands in the %MBIM_SERVICE_USSD service.
 */
typedef enum {
    MBIM_CID_USSD_UNKNOWN = 0,
    MBIM_CID_USSD         = 1,
    MBIM_CID_USSD_LAST    = MBIM_CID_USSD,
} MbimCidUssd;

/**
 * MbimCidPhonebook:
 * @MBIM_CID_PHONEBOOK_UNKNOWN: Unknown command.
 * @MBIM_CID_PHONEBOOK_CONFIGURATION: Configuration.
 * @MBIM_CID_PHONEBOOK_READ: Read.
 * @MBIM_CID_PHONEBOOK_DELETE: Delete.
 * @MBIM_CID_PHONEBOOK_WRITE: Write.
 *
 * MBIM commands in the %MBIM_SERVICE_PHONEBOOK service.
 */
typedef enum {
    MBIM_CID_PHONEBOOK_UNKNOWN       = 0,
    MBIM_CID_PHONEBOOK_CONFIGURATION = 1,
    MBIM_CID_PHONEBOOK_READ          = 2,
    MBIM_CID_PHONEBOOK_DELETE        = 3,
    MBIM_CID_PHONEBOOK_WRITE         = 4,
    MBIM_CID_PHONEBOOK_LAST          = MBIM_CID_PHONEBOOK_WRITE,
} MbimCidPhonebook;

/**
 * MbimCidStk:
 * @MBIM_CID_STK_UNKNOWN: Unknown command.
 * @MBIM_CID_STK_PAC: PAC.
 * @MBIM_CID_STK_TERMINAL_RESPONSE: Terminal response.
 * @MBIM_CID_STK_ENVELOPE: Envelope.
 *
 * MBIM commands in the %MBIM_SERVICE_STK service.
 */
typedef enum {
    MBIM_CID_STK_UNKNOWN           = 0,
    MBIM_CID_STK_PAC               = 1,
    MBIM_CID_STK_TERMINAL_RESPONSE = 2,
    MBIM_CID_STK_ENVELOPE          = 3,
    MBIM_CID_STK_LAST              = MBIM_CID_STK_ENVELOPE,
} MbimCidStk;

/**
 * MbimCidAuth:
 * @MBIM_CID_AUTH_UNKNOWN: Unknown command
 * @MBIM_CID_AUTH_AKA: AKA.
 * @MBIM_CID_AUTH_AKAP: AKAP.
 * @MBIM_CID_AUTH_SIM: SIM.
 *
 * MBIM commands in the %MBIM_SERVICE_AUTH service.
 */
typedef enum {
    MBIM_CID_AUTH_UNKNOWN = 0,
    MBIM_CID_AUTH_AKA     = 1,
    MBIM_CID_AUTH_AKAP    = 2,
    MBIM_CID_AUTH_SIM     = 3,
    MBIM_CID_AUTH_LAST    = MBIM_CID_AUTH_SIM,
} MbimCidAuth;

/**
 * MbimCidDss:
 * @MBIM_CID_DSS_UNKNOWN: Unknown command.
 * @MBIM_CID_DSS_CONNECT: Connect.
 *
 * MBIM commands in the %MBIM_SERVICE_DSS service.
 */
typedef enum {
    MBIM_CID_DSS_UNKNOWN = 0,
    MBIM_CID_DSS_CONNECT = 1,
    MBIM_CID_DSS_LAST    = MBIM_CID_DSS_CONNECT,
} MbimCidDss;

/**
 * MbimCidOem:
 * @MBIM_CID_OEM_UNKNOWN: Unknown command.
 * @MBIM_CID_OEM_ATCI: AT.
 *
 * MBIM commands in the %MBIM_SERVICE_OEM service.
 */
typedef enum {
    MBIM_CID_OEM_UNKNOWN     = 0,
    MBIM_CID_OEM_ATCI        = 1,
    MBIM_CID_OEM_LAST        = MBIM_CID_OEM_ATCI,
} MbimCidOem;


/* Command helpers */
bool        mbim_cid_can_set       (MbimService service, uint cid);
bool        mbim_cid_can_query     (MbimService service, uint cid);
bool        mbim_cid_can_notify    (MbimService service, uint cid);
const char *mbim_cid_get_printable (MbimService service, uint cid);

#endif  // MBIM_CID_H_
