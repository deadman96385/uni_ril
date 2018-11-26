
#include "mbim_uuid.h"
#include "mbim_enums.h"

/**
 * mbim_xx_xx_get_printable:
 * @Mbimxxxx: common enumeration and flag types
 *
 * Gets a printable string for common enumeration and flag types.
 *
 * Returns: (transfer none): a constant string.
 */

/*****************************************************************************/
/* 'Device Caps' enums */
const char *
mbim_device_type_get_printable(MbimDeviceType deviceType) {
    switch (deviceType) {
        case MBIM_DEVICE_TYPE_UNKNOWN:
            return "unknown";
        case MBIM_DEVICE_TYPE_EMBEDDED:
            return "embedded";
        case MBIM_DEVICE_TYPE_REMOVABLE:
            return "removable";
        case MBIM_DEVICE_TYPE_REMOTE:
            return "remote";
        default:
            return "unknown";
    }
}

const char *
mbim_cellular_class_get_printable(MbimCellularClass cellularClass) {
    switch (cellularClass) {
        case MBIM_CELLULAR_CLASS_GSM:
            return "GSM";
        case MBIM_CELLULAR_CLASS_CDMA:
            return "CDMA";
        default:
            return "unknown";
    }
}

const char *
mbim_voice_class_get_printable(MbimVoiceClass voiceClass) {
    switch (voiceClass) {
        case MBIM_VOICE_CLASS_UNKNOWN:
            return "unknown";
        case MBIM_VOICE_CLASS_NO_VOICE:
            return "no_voice";
        case MBIM_VOICE_CLASS_SEPARATED_VOICE_DATA:
            return "separated_voice_data";
        case MBIM_VOICE_CLASS_SIMULTANEOUS_VOICE_DATA:
            return "simultaneous_voice_data";
        default:
            return "unknown";
    }
}

const char *
mbim_sim_class_get_printable(MbimSimClass simClass) {
    switch (simClass) {
        case MBIM_SIM_CLASS_LOGICAL:
            return "logical";
        case MBIM_SIM_CLASS_REMOVABLE:
            return "removable";
        default:
            return "unknown";
    }
}

const char *
mbim_data_class_get_printable(MbimDataClass dataClass) {
    switch (dataClass) {
        case MBIM_DATA_CLASS_GPRS:
            return "GPRS";
        case MBIM_DATA_CLASS_EDGE:
            return "EDGE";
        case MBIM_DATA_CLASS_UMTS:
            return "UMTS";
        case MBIM_DATA_CLASS_HSDPA:
            return "HSDPA";
        case MBIM_DATA_CLASS_HSUPA:
            return "HSUPA";
        case MBIM_DATA_CLASS_LTE:
            return "LTE";
        case MBIM_DATA_CLASS_1XRTT:
            return "1XRTT";
        case MBIM_DATA_CLASS_1XEVDO:
            return "1XEVDO";
        case MBIM_DATA_CLASS_1XEVDO_REVA:
            return "1XEVDO_REVA";
        case MBIM_DATA_CLASS_1XEVDV:
            return "1XEVDV";
        case MBIM_DATA_CLASS_3XRTT:
            return "3XRTT";
        case MBIM_DATA_CLASS_1XEVDO_REVB:
            return "1XEVDO_REVB";
        case MBIM_DATA_CLASS_UMB:
            return "UMB";
        case MBIM_DATA_CLASS_CUSTOM:
            return "CUSTOM";
        default:
            return "unknown";
    }
}

/*****************************************************************************/
/* 'Subscriber Ready Status' enums */

const char *
mbim_suscriber_ready_status_get_printable(MbimSubscriberReadyState readyState) {
    switch (readyState) {
        case MBIM_SUBSCRIBER_READY_STATE_NOT_INITIALIZED:
            return "not_initialized";
        case MBIM_SUBSCRIBER_READY_STATE_INITIALIZED:
            return "initialized";
        case MBIM_SUBSCRIBER_READY_STATE_SIM_NOT_INSERTED:
            return "sim_not_inserted";
        case MBIM_SUBSCRIBER_READY_STATE_BAD_SIM:
            return "bad_sim";
        case MBIM_SUBSCRIBER_READY_STATE_FAILURE:
            return "failure";
        case MBIM_SUBSCRIBER_READY_STATE_NOT_ACTIVATED:
            return "not_activated";
        case MBIM_SUBSCRIBER_READY_STATE_DEVICE_LOCKED:
            return "device_locked";
        default:
            return "unknown";
    }
}

const char *
mbim_ready_info_flag_get_printable(MbimReadyInfoFlag readyInfoFlag) {
    switch (readyInfoFlag) {
        case MBIM_READY_INFO_FLAG_NONE:
            return "none";
        case MBIM_READY_INFO_FLAG_PROTECT_UNIQUE_ID:
            return "protect_unique_id";
        default:
            return "unknown";
    }
}

/*****************************************************************************/
/* 'Radio State' enums */

const char *
mbim_radio_switch_state_get_printable(MbimRadioSwitchState radioState) {
    switch (radioState) {
        case MBIM_RADIO_SWITCH_STATE_OFF:
            return "off";
        case MBIM_RADIO_SWITCH_STATE_ON:
            return "on";
        default:
            return "unknown";
    }
}

/*****************************************************************************/
/* 'Pin' enums */

const char *
mbim_pin_type_get_printable(MbimPinType pinType) {
    switch (pinType) {
        case MBIM_PIN_TYPE_UNKNOWN:
            return "unknown";
        case MBIM_PIN_TYPE_CUSTOM:
            return "custom";
        case MBIM_PIN_TYPE_PIN1:
            return "pin1";
        case MBIM_PIN_TYPE_PIN2:
            return "pin2";
        case MBIM_PIN_TYPE_DEVICE_SIM_PIN:
            return "device_sim_pin";
        case MBIM_PIN_TYPE_DEVICE_FIRST_SIM_PIN:
            return "device_first_sim_pin";
        case MBIM_PIN_TYPE_NETWORK_PIN:
            return "network_pin";
        case MBIM_PIN_TYPE_NETWORK_SUBSET_PIN:
            return "network_subset_pin";
        case MBIM_PIN_TYPE_SERVICE_PROVIDER_PIN:
            return "service_provider_pin";
        case MBIM_PIN_TYPE_CORPORATE_PIN:
            return "corporate_pin";
        case MBIM_PIN_TYPE_SUBSIDY_PIN:
            return "subsidy_pin";
        case MBIM_PIN_TYPE_PUK1:
            return "puk1";
        case MBIM_PIN_TYPE_PUK2:
            return "puk2";
        case MBIM_PIN_TYPE_DEVICE_FIRST_SIM_PUK:
            return "device_first_sim_puk";
        case MBIM_PIN_TYPE_NETWORK_PUK:
            return "network_puk";
        case MBIM_PIN_TYPE_NETWORK_SUBSET_PUK:
            return "network_subset_puk";
        case MBIM_PIN_TYPE_SERVICE_PROVIDER_PUK:
            return "service_provider_puk";
        case MBIM_PIN_TYPE_CORPORATE_PUK:
            return "corporate_puk";
        default:
            return "unknown";
    }
}

const char *
mbim_pin_state_get_printable(MbimPinState pinState) {
    switch (pinState) {
        case MBIM_PIN_STATE_UNLOCKED:
            return "unlocked";
        case MBIM_PIN_STATE_LOCKED:
            return "locked";
        default:
            return "unknown";
    }
}

const char *
mbim_pin_operation_get_printable(MbimPinOperation pinOperation) {
    switch (pinOperation) {
        case MBIM_PIN_OPERATION_ENTER:
            return "enter";
        case MBIM_PIN_OPERATION_ENABLE:
            return "enable";
        case MBIM_PIN_OPERATION_DISABLE:
            return "disable";
        case MBIM_PIN_OPERATION_CHANGE:
            return "change";
        default:
            return "unknown";
    }
}
/*****************************************************************************/
/* 'Pin List' enums */

const char *
mbim_pin_mode_get_printable(MbimPinMode pinMode) {
    switch (pinMode) {
        case MBIM_PIN_MODE_NOT_SUPPORTED:
            return "not_supported";
        case MBIM_PIN_MODE_ENABLED:
            return "enabled";
        case MBIM_PIN_MODE_DISABLED:
            return "disabled";
        default:
            return "unknown";
    }
}

const char *
mbim_pin_format_get_printable(MbimPinFormat pinMode) {
    switch (pinMode) {
        case MBIM_PIN_FORMAT_UNKNOWN:
            return "unknown";
        case MBIM_PIN_FORMAT_NUMERIC:
            return "numeric";
        case MBIM_PIN_FORMAT_ALPHANUMERIC:
            return "alphanumeric";
        default:
            return "unknown";
    }
}
/*****************************************************************************/
/* 'Home Provider' enums */

const char *
mbim_provider_state_get_printable(MbimProviderState providerState) {
    switch (providerState) {
        case MBIM_PROVIDER_STATE_UNKNOWN:
            return "unknown";
        case MBIM_PROVIDER_STATE_HOME:
            return "home";
        case MBIM_PROVIDER_STATE_FORBIDDEN:
            return "forbidden";
        case MBIM_PROVIDER_STATE_PREFERRED:
            return "preferred";
        case MBIM_PROVIDER_STATE_VISIBLE:
            return "visible";
        case MBIM_PROVIDER_STATE_REGISTERED:
            return "registered";
        case MBIM_PROVIDER_STATE_PREFERRED_MULTICARRIER:
            return "preferred_multicarrier";
        default:
            return "unknown";
    }
}
/*****************************************************************************/
/* 'Visible Providers' enums */

const char *
mbim_visible_providers_action_get_printable(MbimVisibleProvidersAction action) {
    switch (action) {
        case MBIM_VISIBLE_PROVIDERS_ACTION_FULL_SCAN:
            return "full_scan";
        case MBIM_VISIBLE_PROVIDERS_ACTION_RESTRICTED_SCAN:
            return "restricted_scan";
        default:
            return "unknown";
    }
}

/*****************************************************************************/
/* 'Register State' enums */

const char *
mbim_network_error_get_printable(MbimNwError nwError) {
    switch (nwError) {
        case MBIM_NW_ERROR_UNKNOWN:
            return "unknown";
        case MBIM_NW_ERROR_IMSI_UNKNOWN_IN_HLR:
            return "imsi_unknown_in_hlr";
        case MBIM_NW_ERROR_ILLEGAL_MS:
            return "illegal_ms";
        case MBIM_NW_ERROR_IMSI_UNKNOWN_IN_VLR:
            return "imsi_unknown_in_vlr";
        case MBIM_NW_ERROR_IMEI_NOT_ACCEPTED:
            return "imei_not_accepted";
        case MBIM_NW_ERROR_ILLEGAL_ME:
            return "illegal_me";
        case MBIM_NW_ERROR_GPRS_NOT_ALLOWED:
            return "gprs_not_allowed";
        case MBIM_NW_ERROR_GPRS_AND_NON_GPRS_NOT_ALLOWED:
            return "gps_and_non_gprs_not_allowed";
        case MBIM_NW_ERROR_MS_IDENTITY_NOT_DERIVED_BY_NETWORK:
            return "ms_identity_not_derived_by_network";
        case MBIM_NW_ERROR_IMPLICITLY_DETACHED:
            return "implicitly_deatched";
        case MBIM_NW_ERROR_PLMN_NOT_ALLOWED:
            return "plmn_not_allowed";
        case MBIM_NW_ERROR_LOCATION_AREA_NOT_ALLOWED:
            return "location_area_not_allowed";
        case MBIM_NW_ERROR_ROAMING_NOT_ALLOWED_IN_LOCATION_AREA:
            return "roaming_not_allowed_in_location_area";
        case MBIM_NW_ERROR_GPRS_NOT_ALLOWED_IN_PLMN:
            return "gprs_not_allowed_in_plmn";
        case MBIM_NW_ERROR_NO_CELLS_IN_LOCATION_AREA:
            return "no_cells_in_location_area";
        case MBIM_NW_ERROR_MSC_TEMPORARILY_NOT_REACHABLE:
            return "msc_temporarily_not_reachable";
        case MBIM_NW_ERROR_NETWORK_FAILURE:
            return "network_failure";
        case MBIM_NW_ERROR_MAC_FAILURE:
            return "mac_failure";
        case MBIM_NW_ERROR_SYNCH_FAILURE:
            return "synch_failure";
        case MBIM_NW_ERROR_CONGESTION:
            return "congestion";
        case MBIM_NW_ERROR_GSM_AUTHENTICATION_UNACCEPTABLE:
            return "gsm_authentication_unacceptable";
        case MBIM_NW_ERROR_NOT_AUTHORIZED_FOR_CSG:
            return "not_authorized_for_csg";
        case MBIM_NW_ERROR_MISSING_OR_UNKNOWN_APN:
            return "missing_or_unknown_apn";
        case MBIM_NW_ERROR_SERVICE_OPTION_NOT_SUPPORTED:
            return "service_option_not_supported";
        case MBIM_NW_ERROR_REQUESTED_SERVICE_OPTION_NOT_SUBSCRIBED:
            return "requested_service_option_not_subscribed";
        case MBIM_NW_ERROR_SERVICE_OPTION_TEMPORARILY_OUT_OF_ORDER:
            return "service_option_temporarily_out_of_order";
        case MBIM_NW_ERROR_NO_PDP_CONTEXT_ACTIVATED:
            return "no_pdp_context_activated";
        case MBIM_NW_ERROR_SEMANTICALLY_INCORRECT_MESSAGE:
            return "semantically_incorrect_message";
        case MBIM_NW_ERROR_INVALID_MANDATORY_INFORMATION:
            return "invalid_mandatory_information";
        case MBIM_NW_ERROR_MESSAGE_TYPE_NON_EXISTENT_OR_NOT_IMPLEMENTED:
            return "message_type_non_existent_or_not_implemented";
        case MBIM_NW_ERROR_MESSAGE_TYPE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE:
            return "message_type_not_compatible_with_protocol_state";
        case MBIM_NW_ERROR_INFORMATION_ELEMENT_NON_EXISTENT_OR_NOT_IMPLEMENTED:
            return "information_element_non_existent_or_not_implemented";
        case MBIM_NW_ERROR_CONDITIONAL_IE_ERROR:
            return "coditional_ie_error";
        case MBIM_NW_ERROR_MESSAGE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE:
            return "message_not_compatible_with_protocol_state";
        case MBIM_NW_ERROR_PROTOCOL_ERROR_UNSPECIFIED:
            return "protocol_error_unspecified";
        default:
            return "unknown";
    }
}

const char *
mbim_register_action_get_printable(MbimRegisterAction registerAction) {
    switch (registerAction) {
        case MBIM_REGISTER_ACTION_AUTOMATIC:
            return "automatic";
        case MBIM_REGISTER_ACTION_MANUAL:
            return "manual";
        default :
            return "unknown";
    }
}

const char *
mbim_register_state_get_printable(MbimRegisterState registerState) {
    switch (registerState) {
        case MBIM_REGISTER_STATE_UNKNOWN:
            return "unknown";
        case MBIM_REGISTER_STATE_DEREGISTERED:
            return "deregistered";
        case MBIM_REGISTER_STATE_SEARCHING:
            return "searching";
        case MBIM_REGISTER_STATE_HOME:
            return "home";
        case MBIM_REGISTER_STATE_ROAMING:
            return "roaming";
        case MBIM_REGISTER_STATE_PARTNER:
            return "partner";
        case MBIM_REGISTER_STATE_DENIED:
            return "denied";
        default:
            return "unknown";
    }
}

const char *
mbim_register_mode_get_printable(MbimRegisterMode registerMode) {
    switch (registerMode) {
        case MBIM_REGISTER_MODE_UNKNOWN:
            return "unknown";
        case MBIM_REGISTER_MODE_AUTOMATIC:
            return "automatic";
        case MBIM_REGISTER_MODE_MANUAL:
            return "manual";
        default:
            return "unknown";
    }
}

const char *
mbim_registration_flag_get_printable(MbimRegistrationFlag registrationFlag) {
    switch (registrationFlag) {
        case MBIM_REGISTRATION_FLAG_NONE:
            return "none";
        case MBIM_REGISTRATION_FLAG_MANUAL_SELECTION_NOT_AVAILABLE:
            return "manual_selection_not_available";
        case MBIM_REGISTRATION_FLAG_PACKET_SERVICE_AUTOMATIC_ATTACH:
            return "packet_service_automatic_attach";
        default:
            return "unknown";
    }
}

/*****************************************************************************/
/* 'Packet Service' enums */

const char *
mbim_packet_service_action_get_printable(MbimPacketServiceAction packetServiceAction) {
    switch (packetServiceAction) {
        case MBIM_PACKET_SERVICE_ACTION_ATTACH:
            return "attach";
        case MBIM_PACKET_SERVICE_ACTION_DETACH:
            return "detach";
        default:
            return "unknown";
    }
}

const char *
mbim_packet_service_state_get_printable(MbimPacketServiceState packetServiceState) {
    switch (packetServiceState) {
        case MBIM_PACKET_SERVICE_STATE_UNKNOWN:
            return "unknown";
        case MBIM_PACKET_SERVICE_STATE_ATTACHING:
            return "attaching";
        case MBIM_PACKET_SERVICE_STATE_ATTACHED:
            return "attached";
        case MBIM_PACKET_SERVICE_STATE_DETACHING:
            return "detaching";
        case MBIM_PACKET_SERVICE_STATE_DETACHED:
            return "detached";
        default:
            return "";
    }
}

/*****************************************************************************/
/* 'Connect' enums */

const char *
mbim_activation_command_get_printable(MbimActivationCommand activationCommand) {
    switch (activationCommand) {
        case MBIM_ACTIVATION_COMMAND_DEACTIVATE:
            return "deactivate";
        case MBIM_ACTIVATION_COMMAND_ACTIVATE:
            return "activate";
        default:
            return "unknown";
    }
}

const char *
mbim_compression_get_printable(MbimCompression compression) {
    switch (compression) {
        case MBIM_COMPRESSION_NONE:
            return "none";
        case MBIM_COMPRESSION_ENABLE:
            return "enable";
        default:
            return "unknown";
    }
}

const char *
mbim_auth_protocol_get_printable(MbimAuthProtocol authProtocol) {
    switch (authProtocol) {
        case MBIM_AUTH_PROTOCOL_NONE:
            return "none";
        case MBIM_AUTH_PROTOCOL_PAP:
            return "pap";
        case MBIM_AUTH_PROTOCOL_CHAP:
            return "chap";
        case MBIM_AUTH_PROTOCOL_MSCHAPV2:
            return "mschapv";
        default:
            return "unknown";
    }
}

const char *
mbim_context_ip_type_get_printable(MbimContextIpType IPType) {
    switch (IPType) {
        case MBIM_CONTEXT_IP_TYPE_DEFAULT:
            return "default";
        case MBIM_CONTEXT_IP_TYPE_IPV4:
            return "IPv4";
        case MBIM_CONTEXT_IP_TYPE_IPV6:
            return "IPv6";
        case MBIM_CONTEXT_IP_TYPE_IPV4V6:
            return "IPv4v6";
        case MBIM_CONTEXT_IP_TYPE_IPV4_AND_IPV6:
            return "IPv4 and IPv6";
        default:
            return "unknown";
    }
}

const char *
mbim_activation_state_get_printable(MbimActivationState activationState) {
    switch (activationState) {
        case MBIM_ACTIVATION_STATE_UNKNOWN:
            return "unknown";
        case MBIM_ACTIVATION_STATE_ACTIVATED:
            return "activated";
        case MBIM_ACTIVATION_STATE_ACTIVATING:
            return "activating";
        case MBIM_ACTIVATION_STATE_DEACTIVATED:
            return "deactivated";
        case MBIM_ACTIVATION_STATE_DEACTIVATING:
            return "deactivating";
        default:
            return "unknown";
    }
}

const char *
mbim_voice_call_state_get_printable(MbimVoiceCallState voiceCallState) {
    switch (voiceCallState) {
        case MBIM_VOICE_CALL_STATE_NONE:
            return "none";
        case MBIM_VOICE_CALL_STATE_IN_PROGRESS:
            return "in_progress";
        case MBIM_VOICE_CALL_STATE_HANG_UP:
            return "hang_up";
        default:
            return "unknown";
    }
}

const char *
mbim_context_type_get_printable(MbimContextType contextType) {
    switch (contextType) {
        case MBIM_CONTEXT_TYPE_INVALID:
            return "invalid";
        case MBIM_CONTEXT_TYPE_NONE:
            return "none";
        case MBIM_CONTEXT_TYPE_INTERNET:
            return "internet";
        case MBIM_CONTEXT_TYPE_VPN:
            return "vpn";
        case MBIM_CONTEXT_TYPE_VOICE:
            return "voice";
        case MBIM_CONTEXT_TYPE_VIDEO_SHARE:
            return "video_share";
        case MBIM_CONTEXT_TYPE_PURCHASE:
            return "purchase";
        case MBIM_CONTEXT_TYPE_IMS:
            return "ims";
        case MBIM_CONTEXT_TYPE_MMS:
            return "mms";
        case MBIM_CONTEXT_TYPE_LOCAL:
            return "local";
        default:
            return "unknown";
    }
}

/*****************************************************************************/
/* 'IP Configuration' enums */


/*****************************************************************************/
/* 'Emergency mode' enums */
const char *
mbim_emergency_mode_state_get_printable(MbimEmergencyModeState emergencyMode) {
    switch (emergencyMode) {
        case MBIM_EMERGENCY_MODE_STATE_OFF:
            return "off";
        case MBIM_EMERGENCY_MODE_STATE_ON:
            return "on";
        default:
            return "unknown";
    }
}

/*****************************************************************************/
/* 'Network idle hint' enums */

const char *
mbim_network_idle_hint_state_get_printable(MbimNetworkIdleHintState networkIdleHintState) {
    switch (networkIdleHintState) {
        case MBIM_NETWORK_IDLE_HINT_STATE_DISABLED:
            return "disabled";
        case MBIM_NETWORK_IDLE_HINT_STATE_ENABLED:
            return "enabled";
        default:
            return "unknown";
    }
}

/*****************************************************************************/
/* 'SMS Configuration' enums */

const char *
mbim_sms_format_get_printable(MbimSmsFormat smsFormat) {
    switch (smsFormat) {
        case MBIM_SMS_FORMAT_PDU:
            return "pdu";
        case MBIM_SMS_FORMAT_CDMA:
            return "cdma";
        default:
            return "unknown";
    }
}

const char *
mbim_sms_storage_state_get_printable(MbimSmsStorageState smsStroageState) {
    switch (smsStroageState) {
        case MBIM_SMS_STORAGE_STATE_NOT_INITIALIZED:
            return "not_initialized";
        case MBIM_SMS_STORAGE_STATE_INITIALIZED:
            return "initialized";
        default:
            return "unknown";
    }
}

/*****************************************************************************/
/* 'SMS Read' enums */

const char *
mbim_sms_message_status_get_printable(MbimSmsMessageStatus smsMessageStatus) {
    switch (smsMessageStatus) {
        case MBIM_SMS_STATUS_NEW:
            return "new";
        case MBIM_SMS_STATUS_OLD:
            return "old";
        case MBIM_SMS_STATUS_DRAFT:
            return "draft";
        case MBIM_SMS_STATUS_SENT:
            return "sent";
        default:
            return "unknown";
    }
}

const char *
mbim_sms_flag_get_printable(MbimSmsFlag smsFlag) {
    switch (smsFlag) {
        case MBIM_SMS_FLAG_ALL:
            return "all";
        case MBIM_SMS_FLAG_INDEX:
            return "index";
        case MBIM_SMS_FLAG_NEW:
            return "new";
        case MBIM_SMS_FLAG_OLD:
            return "old";
        case MBIM_SMS_FLAG_SENT:
            return "sent";
        case MBIM_SMS_FLAG_DRAFT:
            return "draft";
        default:
            return "unknown";
    }
}

/*****************************************************************************/
/* 'SMS Message Store Status' enums */

const char *
mbim_sms_status_flag_get_printable(MbimSmsStatusFlag smsStatusFlag) {
    switch (smsStatusFlag) {
        case MBIM_SMS_STATUS_FLAG_NONE:
            return "none";
        case MBIM_SMS_STATUS_FLAG_MESSAGE_STORE_FULL:
            return "store_full";
        case MBIM_SMS_STATUS_FLAG_NEW_MESSAGE:
            return "new_message";
        default:
            return "unknown";
    }
}

/*****************************************************************************/
/* Status of the MBIM request */

const char *
mbim_status_error_get_printable(MbimStatusError statusError) {
    switch (statusError) {
        case MBIM_STATUS_ERROR_NONE:
            return "STATUS_ERROR_NONE";
        case MBIM_STATUS_ERROR_BUSY:
            return "STATUS_ERROR_BUSY";
        case MBIM_STATUS_ERROR_FAILURE:
            return "STATUS_ERROR_FAILURE";
        case MBIM_STATUS_ERROR_SIM_NOT_INSERTED:
            return "STATUS_ERROR_SIM_NOT_INSERTED";
        case MBIM_STATUS_ERROR_BAD_SIM:
            return "STATUS_ERROR_BAD_SIM";
        case MBIM_STATUS_ERROR_PIN_REQUIRED:
            return "STATUS_ERROR_PIN_REQUIRED";
        case MBIM_STATUS_ERROR_PIN_DISABLED:
            return "STATUS_ERROR_PIN_DISABLED";
        case MBIM_STATUS_ERROR_NOT_REGISTERED:
            return "STATUS_ERROR_NOT_REGISTERED";
        case MBIM_STATUS_ERROR_PROVIDERS_NOT_FOUND:
            return "STATUS_ERROR_PROVIDERS_NOT_FOUND";
        case MBIM_STATUS_ERROR_NO_DEVICE_SUPPORT:
            return "STATUS_ERROR_NO_DEVICE_SUPPORT";
        case MBIM_STATUS_ERROR_PROVIDER_NOT_VISIBLE:
            return "STATUS_ERROR_PROVIDER_NOT_VISIBLE";
        case MBIM_STATUS_ERROR_DATA_CLASS_NOT_AVAILABLE:
            return "STATUS_ERROR_DATA_CLASS_NOT_AVAILABLE";
        case MBIM_STATUS_ERROR_PACKET_SERVICE_DETACHED:
            return "STATUS_ERROR_PACKET_SERVICE_DETACHED";
        case MBIM_STATUS_ERROR_MAX_ACTIVATED_CONTEXTS:
            return "STATUS_ERROR_MAX_ACTIVATED_CONTEXTS";
        case MBIM_STATUS_ERROR_NOT_INITIALIZED:
            return "STATUS_ERROR_NOT_INITIALIZED";
        case MBIM_STATUS_ERROR_VOICE_CALL_IN_PROGRESS:
            return "STATUS_ERROR_VOICE_CALL_IN_PROGRESS";
        case MBIM_STATUS_ERROR_CONTEXT_NOT_ACTIVATED:
            return "STATUS_ERROR_CONTEXT_NOT_ACTIVATED";
        case MBIM_STATUS_ERROR_SERVICE_NOT_ACTIVATED:
            return "STATUS_ERROR_SERVICE_NOT_ACTIVATED";
        case MBIM_STATUS_ERROR_INVALID_ACCESS_STRING:
            return "STATUS_ERROR_INVALID_ACCESS_STRING";
        case MBIM_STATUS_ERROR_INVALID_USER_NAME_PWD:
            return "STATUS_ERROR_INVALID_USER_NAME_PWD";
        case MBIM_STATUS_ERROR_RADIO_POWER_OFF:
            return "STATUS_ERROR_RADIO_POWER_OFF";
        case MBIM_STATUS_ERROR_INVALID_PARAMETERS:
            return "STATUS_ERROR_INVALID_PARAMETERS";
        case MBIM_STATUS_ERROR_READ_FAILURE:
            return "STATUS_ERROR_READ_FAILURE";
        case MBIM_STATUS_ERROR_WRITE_FAILURE:
            return "STATUS_ERROR_WRITE_FAILURE";
        case MBIM_STATUS_ERROR_NO_PHONEBOOK:
            return "STATUS_ERROR_NO_PHONEBOOK";
        case MBIM_STATUS_ERROR_PARAMETER_TOO_LONG:
            return "STATUS_ERROR_PARAMETER_TOO_LONG";
        case MBIM_STATUS_ERROR_STK_BUSY:
            return "STATUS_ERROR_STK_BUSY";
        case MBIM_STATUS_ERROR_OPERATION_NOT_ALLOWED:
            return "STATUS_ERROR_OPERATION_NOT_ALLOWED";
        case MBIM_STATUS_ERROR_MEMORY_FAILURE:
            return "STATUS_ERROR_MEMORY_FAILURE";
        case MBIM_STATUS_ERROR_INVALID_MEMORY_INDEX:
            return "STATUS_ERROR_INVALID_MEMORY_INDEX";
        case MBIM_STATUS_ERROR_MEMORY_FULL:
            return "STATUS_ERROR_MEMORY_FULL";
        case MBIM_STATUS_ERROR_FILTER_NOT_SUPPORTED:
            return "STATUS_ERROR_FILTER_NOT_SUPPORTED";
        case MBIM_STATUS_ERROR_DSS_INSTANCE_LIMIT:
            return "STATUS_ERROR_DSS_INSTANCE_LIMIT";
        case MBIM_STATUS_ERROR_INVALID_DEVICE_SERVICE_OPERATION:
            return "STATUS_ERROR_INVALID_DEVICE_SERVICE_OPERATION";
        case MBIM_STATUS_ERROR_AUTH_INCORRECT_AUTN:
             return "STATUS_ERROR_AUTH_INCORRECT_AUTN";
        case MBIM_STATUS_ERROR_AUTH_SYNC_FAILURE:
             return "STATUS_ERROR_AUTH_SYNC_FAILURE";
        case MBIM_STATUS_ERROR_AUTH_AMF_NOT_SET:
             return "STATUS_ERROR_AUTH_AMF_NOT_SET";
        case MBIM_STATUS_ERROR_CONTEXT_NOT_SUPPORTED:
             return "STATUS_ERROR_CONTEXT_NOT_SUPPORTED";
        case MBIM_STATUS_ERROR_SMS_UNKNOWN_SMSC_ADDRESS:
            return "STATUS_ERROR_SMS_UNKNOWN_SMSC_ADDRESS";
        case MBIM_STATUS_ERROR_SMS_NETWORK_TIMEOUT:
            return "STATUS_ERROR_SMS_NETWORK_TIMEOUT";
        case MBIM_STATUS_ERROR_SMS_LANG_NOT_SUPPORTED:
            return "STATUS_ERROR_SMS_LANG_NOT_SUPPORTED";
        case MBIM_STATUS_ERROR_SMS_ENCODING_NOT_SUPPORTED:
            return "STATUS_ERROR_SMS_ENCODING_NOT_SUPPORTED";
        case MBIM_STATUS_ERROR_SMS_FORMAT_NOT_SUPPORTED:
            return "STATUS_ERROR_SMS_FORMAT_NOT_SUPPORTED";
        default:
            return "unknown status error";
    }
}

const char *
mbim_protocol_error_get_printable(MbimProtocolError protocol) {
    switch (protocol) {
        case MBIM_PROTOCOL_ERROR_INVALID:
            return "PROTOCOL_ERROR_INVALID";
        case MBIM_PROTOCOL_ERROR_TIMEOUT_FRAGMENT:
            return "PROTOCOL_ERROR_TIMEOUT_FRAGMENT";
        case MBIM_PROTOCOL_ERROR_FRAGMENT_OUT_OF_SEQUENCE:
            return "PROTOCOL_ERROR_FRAGMENT_OUT_OF_SEQUENCE";
        case MBIM_PROTOCOL_ERROR_LENGTH_MISMATCH:
            return "PROTOCOL_ERROR_LENGTH_MISMATCH";
        case MBIM_PROTOCOL_ERROR_DUPLICATED_TID:
            return "PROTOCOL_ERROR_DUPLICATED_TID";
        case MBIM_PROTOCOL_ERROR_NOT_OPENED:
            return "PROTOCOL_ERROR_NOT_OPENED";
        case MBIM_PROTOCOL_ERROR_UNKNOWN:
            return "PROTOCOL_ERROR_UNKNOWN";
        case MBIM_PROTOCOL_ERROR_CANCEL:
            return "PROTOCOL_ERROR_CANCEL";
        case MBIM_PROTOCOL_ERROR_MAX_TRANSFER:
            return "PROTOCOL_ERROR_MAX_TRANSFER";
        default:
            return "PROTOCOL_ERROR_UNKNOWN";
    }
}
