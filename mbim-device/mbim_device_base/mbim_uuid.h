
#ifndef MBIM_UUID_H_
#define MBIM_UUID_H_

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

/*****************************************************************************/

/**
 * MbimUuid:
 *
 * A UUID as defined in MBIM.
 */
struct _MbimUuid {
    uint8_t a[4];
    uint8_t b[2];
    uint8_t c[2];
    uint8_t d[2];
    uint8_t e[6];
} __attribute__((packed));
typedef struct _MbimUuid MbimUuid;

bool  mbim_uuid_cmp            (const MbimUuid *    a,
                                const MbimUuid *    b);
char *mbim_uuid_get_printable  (const MbimUuid *    uuid);
bool  mbim_uuid_from_printable (const char *        str,
                                MbimUuid *          uuid);

/*****************************************************************************/

/**
 * MbimService:
 * @MBIM_SERVICE_INVALID: Invalid service.
 * @MBIM_SERVICE_BASIC_CONNECT: Basic connectivity service.
 * @MBIM_SERVICE_SMS: SMS messaging service.
 * @MBIM_SERVICE_USSD: USSD service.
 * @MBIM_SERVICE_PHONEBOOK: Phonebook service.
 * @MBIM_SERVICE_STK: SIM toolkit service.
 * @MBIM_SERVICE_AUTH: Authentication service.
 * @MBIM_SERVICE_DSS: Device Service Stream service.
 *
 * Enumeration of the generic MBIM services.
 */
typedef enum {
    MBIM_SERVICE_INVALID          = 0,
    MBIM_SERVICE_BASIC_CONNECT    = 1,
    MBIM_SERVICE_SMS              = 2,
    MBIM_SERVICE_USSD             = 3,
    MBIM_SERVICE_PHONEBOOK        = 4,
    MBIM_SERVICE_STK              = 5,
    MBIM_SERVICE_AUTH             = 6,
    MBIM_SERVICE_DSS              = 7,
    MBIM_SERVICE_OEM              = 8,
    MBIM_SERVICE_LAST
} MbimService;

/* UUID To/From service */
const MbimUuid *mbim_uuid_from_service  (MbimService     service);
MbimService     mbim_uuid_to_service    (const MbimUuid *uuid);

/**
 * MBIM_UUID_INVALID:
 *
 * Get the UUID of the %MBIM_SERVICE_INVALID service.
 *
 * Returns: (transfer none): a #MbimUuid.
 */
#define MBIM_UUID_INVALID mbim_uuid_from_service (MBIM_SERVICE_INVALID)

/**
 * MBIM_UUID_BASIC_CONNECT:
 *
 * Get the UUID of the %MBIM_SERVICE_BASIC_CONNECT service.
 *
 * Returns: (transfer none): a #MbimUuid.
 */
#define MBIM_UUID_BASIC_CONNECT mbim_uuid_from_service (MBIM_SERVICE_BASIC_CONNECT)

/**
 * MBIM_UUID_SMS:
 *
 * Get the UUID of the %MBIM_SERVICE_SMS service.
 *
 * Returns: (transfer none): a #MbimUuid.
 */
#define MBIM_UUID_SMS mbim_uuid_from_service (MBIM_SERVICE_SMS)

/**
 * MBIM_UUID_USSD:
 *
 * Get the UUID of the %MBIM_SERVICE_USSD service.
 *
 * Returns: (transfer none): a #MbimUuid.
 */
#define MBIM_UUID_USSD mbim_uuid_from_service (MBIM_SERVICE_USSD)

/**
 * MBIM_UUID_PHONEBOOK:
 *
 * Get the UUID of the %MBIM_SERVICE_PHONEBOOK service.
 *
 * Returns: (transfer none): a #MbimUuid.
 */
#define MBIM_UUID_PHONEBOOK mbim_uuid_from_service (MBIM_SERVICE_PHONEBOOK)

/**
 * MBIM_UUID_STK:
 *
 * Get the UUID of the %MBIM_SERVICE_STK service.
 *
 * Returns: (transfer none): a #MbimUuid.
 */
#define MBIM_UUID_STK mbim_uuid_from_service (MBIM_SERVICE_STK)

/**
 * MBIM_UUID_AUTH:
 *
 * Get the UUID of the %MBIM_SERVICE_AUTH service.
 *
 * Returns: (transfer none): a #MbimUuid.
 */
#define MBIM_UUID_AUTH mbim_uuid_from_service (MBIM_SERVICE_AUTH)

/**
 * MBIM_UUID_DSS:
 *
 * Get the UUID of the %MBIM_SERVICE_DSS service.
 *
 * Returns: (transfer none): a #MbimUuid.
 */
#define MBIM_UUID_DSS mbim_uuid_from_service (MBIM_SERVICE_DSS)

/**
 * MBIM_UUID_OEM:
 *
 * Get the UUID of the %MBIM_SERVICE_OEM service.
 *
 * Returns: (transfer none): a #MbimUuid.
 */
#define MBIM_UUID_OEM mbim_uuid_from_service (MBIM_SERVICE_OEM)

const char *
mbim_service_get_printable(MbimService service);

/*****************************************************************************/

/**
 * MbimContextType:
 * @MBIM_CONTEXT_TYPE_INVALID: Invalid context type.
 * @MBIM_CONTEXT_TYPE_NONE: Context not yet provisioned.
 * @MBIM_CONTEXT_TYPE_INTERNET: Connection to the Internet.
 * @MBIM_CONTEXT_TYPE_VPN: Connection to a VPN.
 * @MBIM_CONTEXT_TYPE_VOICE: Connection to a VoIP service.
 * @MBIM_CONTEXT_TYPE_VIDEO_SHARE: Connection to a video sharing service.
 * @MBIM_CONTEXT_TYPE_PURCHASE: Connection to an over-the-air activation site.
 * @MBIM_CONTEXT_TYPE_IMS: Connection to IMS.
 * @MBIM_CONTEXT_TYPE_MMS: Connection to MMS.
 * @MBIM_CONTEXT_TYPE_LOCAL: A local.
 *
 * Enumeration of the generic MBIM context types.
 */
typedef enum {
    MBIM_CONTEXT_TYPE_INVALID     = 0,
    MBIM_CONTEXT_TYPE_NONE        = 1,
    MBIM_CONTEXT_TYPE_INTERNET    = 2,
    MBIM_CONTEXT_TYPE_VPN         = 3,
    MBIM_CONTEXT_TYPE_VOICE       = 4,
    MBIM_CONTEXT_TYPE_VIDEO_SHARE = 5,
    MBIM_CONTEXT_TYPE_PURCHASE    = 6,
    MBIM_CONTEXT_TYPE_IMS         = 7,
    MBIM_CONTEXT_TYPE_MMS         = 8,
    MBIM_CONTEXT_TYPE_LOCAL       = 9,
} MbimContextType;

/* To/From context type */
const MbimUuid *
mbim_uuid_from_context_type (MbimContextType    context_type);

MbimContextType
mbim_uuid_to_context_type   (const MbimUuid *   uuid);

#endif  // MBIM_UUID_H_
