#define LOG_TAG "MBIM-Device"

#include <utils/Log.h>
#include "mbim_cid.h"

/* Note: index of the array is CID - 1 */
static const CidConfig s_cid_basic_connect_config[MBIM_CID_BASIC_CONNECT_LAST] = {
    { NO_SET, QUERY,    NO_NOTIFY }, /* MBIM_CID_BASIC_CONNECT_DEVICE_CAPS */
    { NO_SET, QUERY,    NOTIFY    }, /* MBIM_CID_BASIC_CONNECT_SUBSCRIBER_READY_STATUS */
    { SET,    QUERY,    NOTIFY    }, /* MBIM_CID_BASIC_CONNECT_RADIO_STATE */
    { SET,    QUERY,    NO_NOTIFY }, /* MBIM_CID_BASIC_CONNECT_PIN */
    { NO_SET, QUERY,    NO_NOTIFY }, /* MBIM_CID_BASIC_CONNECT_PIN_LIST */
    { SET,    QUERY,    NO_NOTIFY }, /* MBIM_CID_BASIC_CONNECT_HOME_PROVIDER */
    { SET,    QUERY,    NOTIFY    }, /* MBIM_CID_BASIC_CONNECT_PREFERRED_PROVIDERS */
    { NO_SET, QUERY,    NO_NOTIFY }, /* MBIM_CID_BASIC_CONNECT_VISIBLE_PROVIDERS */
    { SET,    QUERY,    NOTIFY    }, /* MBIM_CID_BASIC_CONNECT_REGISTER_STATE */
    { SET,    QUERY,    NOTIFY    }, /* MBIM_CID_BASIC_CONNECT_PACKET_SERVICE */
    { SET,    QUERY,    NOTIFY    }, /* MBIM_CID_BASIC_CONNECT_SIGNAL_STATE */
    { SET,    QUERY,    NOTIFY    }, /* MBIM_CID_BASIC_CONNECT_CONNECT */
    { SET,    QUERY,    NOTIFY    }, /* MBIM_CID_BASIC_CONNECT_PROVISIONED_CONTEXTS */
    { SET,    NO_QUERY, NO_NOTIFY }, /* MBIM_CID_BASIC_CONNECT_SERVICE_ACTIVATION */
    { NO_SET, QUERY,    NOTIFY    }, /* MBIM_CID_BASIC_CONNECT_IP_CONFIGURATION */
    { NO_SET, QUERY,    NO_NOTIFY }, /* MBIM_CID_BASIC_CONNECT_DEVICE_SERVICES */
    { NO_SET, NO_QUERY, NO_NOTIFY }, /* 17 reserved */
    { NO_SET, NO_QUERY, NO_NOTIFY }, /* 18 reserved */
    { SET,    NO_QUERY, NO_NOTIFY }, /* MBIM_CID_BASIC_CONNECT_DEVICE_SERVICE_SUBSCRIBE_LIST */
    { NO_SET, QUERY,    NO_NOTIFY }, /* MBIM_CID_BASIC_CONNECT_PACKET_STATISTICS */
    { SET,    QUERY,    NO_NOTIFY }, /* MBIM_CID_BASIC_CONNECT_NETWORK_IDLE_HINT */
    { NO_SET, QUERY,    NOTIFY    }, /* MBIM_CID_BASIC_CONNECT_EMERGENCY_MODE */
    { SET,    QUERY,    NO_NOTIFY }, /* MBIM_CID_BASIC_CONNECT_IP_PACKET_FILTERS */
    { SET,    QUERY,    NOTIFY    }, /* MBIM_CID_BASIC_CONNECT_MULTICARRIER_PROVIDERS */
};

/* Note: index of the array is CID-1 */
static const CidConfig s_cid_sms_config[MBIM_CID_SMS_LAST] = {
    { SET,    QUERY,    NOTIFY    }, /* MBIM_CID_SMS_CONFIGURATION */
    { NO_SET, QUERY,    NOTIFY    }, /* MBIM_CID_SMS_READ */
    { SET,    NO_QUERY, NO_NOTIFY }, /* MBIM_CID_SMS_SEND */
    { SET,    NO_QUERY, NO_NOTIFY }, /* MBIM_CID_SMS_DELETE */
    { NO_SET, QUERY,    NOTIFY    }, /* MBIM_CID_SMS_MESSAGE_STORE_STATUS */
};

/* Note: index of the array is CID-1 */
static const CidConfig s_cid_ussd_config[MBIM_CID_USSD_LAST] = {
    { SET, NO_QUERY, NOTIFY }, /* MBIM_CID_USSD */
};

/* Note: index of the array is CID-1 */
static const CidConfig s_cid_phonebook_config[MBIM_CID_PHONEBOOK_LAST] = {
    { NO_SET, QUERY,    NOTIFY    }, /* MBIM_CID_PHONEBOOK_CONFIGURATION */
    { NO_SET, QUERY,    NO_NOTIFY }, /* MBIM_CID_PHONEBOOK_READ */
    { SET,    NO_QUERY, NO_NOTIFY }, /* MBIM_CID_PHONEBOOK_DELETE */
    { SET,    NO_QUERY, NO_NOTIFY }, /* MBIM_CID_PHONEBOOK_WRITE */
};

/* Note: index of the array is CID-1 */
static const CidConfig s_cid_stk_config [MBIM_CID_STK_LAST] = {
    { SET, QUERY,    NOTIFY    }, /* MBIM_CID_STK_PAC */
    { SET, NO_QUERY, NO_NOTIFY }, /* MBIM_CID_STK_TERMINAL_RESPONSE */
    { SET, QUERY,    NO_NOTIFY }, /* MBIM_CID_STK_ENVELOPE */
};

/* Note: index of the array is CID-1 */
static const CidConfig s_cid_auth_config [MBIM_CID_AUTH_LAST] = {
    { NO_SET, QUERY, NO_NOTIFY }, /* MBIM_CID_AUTH_AKA */
    { NO_SET, QUERY, NO_NOTIFY }, /* MBIM_CID_AUTH_AKAP */
    { NO_SET, QUERY, NO_NOTIFY }, /* MBIM_CID_AUTH_SIM */
};

/* Note: index of the array is CID-1 */
static const CidConfig s_cid_dss_config [MBIM_CID_DSS_LAST] = {
    { SET, NO_QUERY, NO_NOTIFY }, /* MBIM_CID_DSS_CONNECT */
};

/* Note: index of the array is CID-1 */
static const CidConfig s_cid_oem_config [MBIM_CID_OEM_LAST] = {
    { SET, QUERY, NOTIFY }, /* MBIM_CID_OEM_ATCI */
};

/**
 * mbim_cid_can_set:
 * @service: a #MbimService.
 * @cid: a command ID.
 *
 * Checks whether the given command allows setting.
 *
 * Returns: %TRUE if the command allows setting, %FALSE otherwise.
 */
bool
mbim_cid_can_set(MbimService    service,
                 uint           cid) {
    if (cid <= 0 || service <= MBIM_SERVICE_INVALID ||
            service > MBIM_SERVICE_LAST) {
        RLOGE("Invalid parameters");
    }

    switch (service) {
        case MBIM_SERVICE_BASIC_CONNECT:
            return s_cid_basic_connect_config[cid - 1].set;
        case MBIM_SERVICE_SMS:
            return s_cid_sms_config[cid - 1].set;
        case MBIM_SERVICE_USSD:
            return s_cid_ussd_config[cid - 1].set;
        case MBIM_SERVICE_PHONEBOOK:
            return s_cid_phonebook_config[cid - 1].set;
        case MBIM_SERVICE_STK:
            return s_cid_stk_config[cid - 1].set;
        case MBIM_SERVICE_AUTH:
            return s_cid_auth_config[cid - 1].set;
        case MBIM_SERVICE_DSS:
            return s_cid_dss_config[cid - 1].set;
        case MBIM_SERVICE_OEM:
            return s_cid_oem_config[cid - 1].set;
        default:
            RLOGE("Invalid MbimService");
            return false;
    }
}

/**
 * mbim_cid_can_query:
 * @service: a #MbimService.
 * @cid: a command ID.
 *
 * Checks whether the given command allows querying.
 *
 * Returns: %TRUE if the command allows querying, %FALSE otherwise.
 */
bool
mbim_cid_can_query(MbimService      service,
                   uint             cid) {
    if (cid <= 0 || service <= MBIM_SERVICE_INVALID ||
            service > MBIM_SERVICE_LAST) {
        RLOGE("Invalid parameters");
    }

    switch (service) {
        case MBIM_SERVICE_BASIC_CONNECT:
            return s_cid_basic_connect_config[cid - 1].query;
        case MBIM_SERVICE_SMS:
            return s_cid_sms_config[cid - 1].query;
        case MBIM_SERVICE_USSD:
            return s_cid_ussd_config[cid - 1].query;
        case MBIM_SERVICE_PHONEBOOK:
            return s_cid_phonebook_config[cid - 1].query;
        case MBIM_SERVICE_STK:
            return s_cid_stk_config[cid - 1].query;
        case MBIM_SERVICE_AUTH:
            return s_cid_auth_config[cid - 1].query;
        case MBIM_SERVICE_DSS:
            return s_cid_dss_config[cid - 1].query;
        case MBIM_SERVICE_OEM:
            return s_cid_oem_config[cid - 1].query;
        default:
            RLOGE("Invalid MbimService");
            return false;
    }
}

/**
 * mbim_cid_can_notify:
 * @service: a #MbimService.
 * @cid: a command ID.
 *
 * Checks whether the given command allows notifying.
 *
 * Returns: %TRUE if the command allows notifying, %FALSE otherwise.
 */
bool
mbim_cid_can_notify(MbimService     service,
                    uint            cid) {
    if (cid <= 0 || service <= MBIM_SERVICE_INVALID ||
            service > MBIM_SERVICE_LAST) {
        RLOGE("Invalid parameters");
    }

    switch (service) {
        case MBIM_SERVICE_BASIC_CONNECT:
            return s_cid_basic_connect_config[cid - 1].notify;
        case MBIM_SERVICE_SMS:
            return s_cid_sms_config[cid - 1].notify;
        case MBIM_SERVICE_USSD:
            return s_cid_ussd_config[cid - 1].notify;
        case MBIM_SERVICE_PHONEBOOK:
            return s_cid_phonebook_config[cid - 1].notify;
        case MBIM_SERVICE_STK:
            return s_cid_stk_config[cid - 1].notify;
        case MBIM_SERVICE_AUTH:
            return s_cid_auth_config[cid - 1].notify;
        case MBIM_SERVICE_DSS:
            return s_cid_dss_config[cid - 1].notify;
        case MBIM_SERVICE_OEM:
            return s_cid_oem_config[cid - 1].notify;
        default:
            RLOGE("Invalid MbimService");
            return false;
    }
}

/**
 * mbim_cid_basic_connect_get_string:
 * @cid: a basic connect service's command ID.
 *
 * Gets a printable string for the command specified by the @cid.
 *
 * Returns: (transfer none): a constant string.
 */
const char *
mbim_cid_basic_connect_get_string(uint cid) {
    switch(cid) {
        case MBIM_CID_BASIC_CONNECT_UNKNOWN:
            return "CID_UNKNOWN";
        case MBIM_CID_BASIC_CONNECT_DEVICE_CAPS:
            return "CID_DEVICE_CAPS";
        case MBIM_CID_BASIC_CONNECT_SUBSCRIBER_READY_STATUS:
            return "CID_SUBSCRIBER_READY_STATUS";
        case MBIM_CID_BASIC_CONNECT_RADIO_STATE:
            return "CID_RADIO_STATE";
        case MBIM_CID_BASIC_CONNECT_PIN:
            return "CID_PIN";
        case MBIM_CID_BASIC_CONNECT_PIN_LIST:
            return "CID_PIN_LIST";
        case MBIM_CID_BASIC_CONNECT_HOME_PROVIDER:
            return "CID_HOME_PROVIDER";
        case MBIM_CID_BASIC_CONNECT_PREFERRED_PROVIDERS:
            return "CID_PREFERRED_PROVIDERS";
        case MBIM_CID_BASIC_CONNECT_VISIBLE_PROVIDERS:
            return "CID_VISIBLE_PROVIDERS";
        case MBIM_CID_BASIC_CONNECT_REGISTER_STATE:
            return "CID_REGISTER_STATE";
        case MBIM_CID_BASIC_CONNECT_PACKET_SERVICE:
            return "CID_PACKET_SERVICE";
        case MBIM_CID_BASIC_CONNECT_SIGNAL_STATE:
            return "CID_SIGNAL_STATE";
        case MBIM_CID_BASIC_CONNECT_CONNECT:
            return "CID_CONNECT";
        case MBIM_CID_BASIC_CONNECT_PROVISIONED_CONTEXTS:
            return "CID_PROVISIONED_CONTEXTS";
        case MBIM_CID_BASIC_CONNECT_SERVICE_ACTIVATION:
            return "CID_SERVICE_ACTIVATION";
        case MBIM_CID_BASIC_CONNECT_IP_CONFIGURATION:
            return "CID_IP_CONFIGURATION";
        case MBIM_CID_BASIC_CONNECT_DEVICE_SERVICES:
            return "CID_DEVICE_SERVICES";
        case MBIM_CID_BASIC_CONNECT_DEVICE_SERVICE_SUBSCRIBE_LIST:
            return "CID_SERVICE_SUBSCRIBE_LIST";
        case MBIM_CID_BASIC_CONNECT_PACKET_STATISTICS:
            return "CID_PACKET_STATISTICS";
        case MBIM_CID_BASIC_CONNECT_NETWORK_IDLE_HINT:
            return "CID_NETWORK_IDLE_HINT";
        case MBIM_CID_BASIC_CONNECT_EMERGENCY_MODE:
            return "CID_EMERGENCY_MODE";
        case MBIM_CID_BASIC_CONNECT_IP_PACKET_FILTERS:
            return "CID_IP_PACKET_FILTERS";
        case MBIM_CID_BASIC_CONNECT_MULTICARRIER_PROVIDERS:
            return "CID_MULTICARRIER_PROVIDERS";
        default:
            return "CID_UNKNOWN";
    }
}

/**
 * mbim_cid_sms_get_string:
 * @cid: a sms service's command ID.
 *
 * Gets a printable string for the command specified by the @cid.
 *
 * Returns: (transfer none): a constant string.
 */
const char *
mbim_cid_sms_get_string(uint cid) {
    switch(cid) {
        case MBIM_CID_SMS_UNKNOWN:
            return "CID_SMS_UNKNOWN";
        case MBIM_CID_SMS_CONFIGURATION:
            return "CID_SMS_CONFIGURATION";
        case MBIM_CID_SMS_READ:
            return "CID_SMS_READ";
        case MBIM_CID_SMS_SEND:
            return "CID_SMS_SEND";
        case MBIM_CID_SMS_DELETE:
            return "CID_SMS_DELETE";
        case MBIM_CID_SMS_MESSAGE_STORE_STATUS:
            return "CID_SMS_MESSAGE_STORE_STATUS";
        default:
            return "CID_SMS_UNKNOWN";
    }
}

/**
 * mbim_cid_ussd_get_string:
 * @cid: a ussd service's command ID.
 *
 * Gets a printable string for the command specified by the @cid.
 *
 * Returns: (transfer none): a constant string.
 */
const char *
mbim_cid_ussd_get_string(uint cid) {
    switch(cid) {
        case MBIM_CID_USSD_UNKNOWN:
            return "CID_USSD_UNKNOWN";
        case MBIM_CID_USSD:
            return "CID_USSD";
        default:
            return "CID_USSD_UNKNOWN";
    }
}

/**
 * mbim_cid_phonebook_get_string:
 * @cid: a phonebook service's command ID.
 *
 * Gets a printable string for the command specified by the @cid.
 *
 * Returns: (transfer none): a constant string.
 */
const char *
mbim_cid_phonebook_get_string(uint cid) {
    switch(cid) {
        case MBIM_CID_PHONEBOOK_UNKNOWN:
            return "CID_PHONEBOOK_UNKNOWN";
        case MBIM_CID_PHONEBOOK_CONFIGURATION:
            return "CID_PHONEBOOK_CONFIGURATION";
        case MBIM_CID_PHONEBOOK_READ:
            return "CID_PHONEBOOK_READ";
        case MBIM_CID_PHONEBOOK_DELETE:
            return "CID_PHONEBOOK_DELETE";
        case MBIM_CID_PHONEBOOK_WRITE:
            return "CID_PHONEBOOK_WRITE";
        default:
            return "CID_PHONEBOOK_UNKNOWN";
    }
}

/**
 * mbim_cid_stk_get_string:
 * @cid: a stk service's command ID.
 *
 * Gets a printable string for the command specified by the @cid.
 *
 * Returns: (transfer none): a constant string.
 */
const char *
mbim_cid_stk_get_string(uint cid) {
    switch(cid) {
        case MBIM_CID_STK_UNKNOWN:
            return "CID_STK_UNKNOWN";
        case MBIM_CID_STK_PAC:
            return "CID_STK_PAC";
        case MBIM_CID_STK_TERMINAL_RESPONSE:
            return "CID_STK_TERMINAL_RESPONSE";
        case MBIM_CID_STK_ENVELOPE:
            return "CID_STK_ENVELOPE";
        default:
            return "CID_STK_UNKNOWN";
    }
}

/**
 * mbim_cid_auth_get_string:
 * @cid: a auth service's command ID.
 *
 * Gets a printable string for the command specified by the @cid.
 *
 * Returns: (transfer none): a constant string.
 */
const char *
mbim_cid_auth_get_string(uint cid) {
    switch(cid) {
        case MBIM_CID_AUTH_UNKNOWN:
            return "CID_AUTH_UNKNOWN";
        case MBIM_CID_AUTH_AKA:
            return "CID_AUTH_AKA";
        case MBIM_CID_AUTH_AKAP:
            return "CID_AUTH_AKAP";
        case MBIM_CID_AUTH_SIM:
            return "CID_AUTH_SIM";
        default:
            return "CID_AUTH_UNKNOWN";
    }
}

/**
 * mbim_cid_dss_get_string:
 * @cid: a dss service's command ID.
 *
 * Gets a printable string for the command specified by the @cid.
 *
 * Returns: (transfer none): a constant string.
 */
const char *
mbim_cid_dss_get_string(uint cid) {
    switch(cid) {
        case MBIM_CID_DSS_UNKNOWN:
            return "CID_DSS_UNKNOWN";
        case MBIM_CID_AUTH_AKA:
            return "CID_DSS_CONNECT";
        default:
            return "CID_DSS_UNKNOWN";
    }
}

/**
 * mbim_cid_oem_get_string:
 * @cid: a dss service's command ID.
 *
 * Gets a printable string for the command specified by the @cid.
 *
 * Returns: (transfer none): a constant string.
 */
const char *
mbim_cid_oem_get_string(uint cid) {
    switch(cid) {
        case MBIM_CID_OEM_UNKNOWN:
            return "CID_OEM_UNKNOWN";
        case MBIM_CID_OEM_ATCI:
            return "CID_OEM_ATCI";
        default:
            return "CID_OEM_UNKNOWN";
    }
}

/**
 * mbim_cid_get_printable:
 * @service: a #MbimService.
 * @cid: a command ID.
 *
 * Gets a printable string for the command specified by the @service and the
 * @cid.
 *
 * Returns: (transfer none): a constant string.
 */
const char *
mbim_cid_get_printable(MbimService      service,
                       uint             cid) {
    if (cid <= 0 || service <= MBIM_SERVICE_INVALID ||
            service > MBIM_SERVICE_LAST) {
        RLOGE("Invalid parameters");
        return NULL;
    }

    switch (service) {
        case MBIM_SERVICE_INVALID:
            return "invalid";
        case MBIM_SERVICE_BASIC_CONNECT:
            return mbim_cid_basic_connect_get_string (cid);
        case MBIM_SERVICE_SMS:
            return mbim_cid_sms_get_string (cid);
        case MBIM_SERVICE_USSD:
            return mbim_cid_ussd_get_string (cid);
        case MBIM_SERVICE_PHONEBOOK:
            return mbim_cid_phonebook_get_string (cid);
        case MBIM_SERVICE_STK:
            return mbim_cid_stk_get_string (cid);
        case MBIM_SERVICE_AUTH:
            return mbim_cid_auth_get_string (cid);
        case MBIM_SERVICE_DSS:
            return mbim_cid_dss_get_string (cid);
        case MBIM_SERVICE_OEM:
            return mbim_cid_oem_get_string (cid);
        default:
            RLOGE("Invalid MbimService");
            return NULL;
    }
}
