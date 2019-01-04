    {0, NULL},                   // none
    {RIL_EXT_REQUEST_VIDEOPHONE_DIAL, freeVideoPhoneDial},  /****/
    {RIL_EXT_REQUEST_VIDEOPHONE_CODEC, freeVideoPhoneCodec},  /****/
    {RIL_EXT_REQUEST_VIDEOPHONE_FALLBACK, freeVoid},
    {RIL_EXT_REQUEST_VIDEOPHONE_STRING, freeString},
    {RIL_EXT_REQUEST_VIDEOPHONE_LOCAL_MEDIA, freeInts},
    {RIL_EXT_REQUEST_VIDEOPHONE_CONTROL_IFRAME, freeInts},
    {RIL_EXT_REQUEST_SWITCH_MULTI_CALL, freeInts},
    {RIL_EXT_REQUEST_TRAFFIC_CLASS, freeInts},
    {RIL_EXT_REQUEST_ENABLE_LTE, freeInts},
    {RIL_EXT_REQUEST_ATTACH_DATA, freeInts},
    {RIL_EXT_REQUEST_STOP_QUERY_NETWORK, freeVoid},
    {RIL_EXT_REQUEST_FORCE_DETACH, freeVoid},
    {RIL_EXT_REQUEST_GET_HD_VOICE_STATE, freeVoid},
    {RIL_EXT_REQUEST_SIMMGR_SIM_POWER, freeInts},
    {RIL_EXT_REQUEST_ENABLE_RAU_NOTIFY, freeVoid},
    {RIL_EXT_REQUEST_SET_COLP, freeInts},
    {RIL_EXT_REQUEST_GET_DEFAULT_NAN, freeVoid},
    {RIL_EXT_REQUEST_SIM_GET_ATR, freeVoid},
    {RIL_EXT_REQUEST_EXPLICIT_CALL_TRANSFER, freeVoid},
    {RIL_EXT_REQUEST_GET_SIM_CAPACITY, freeVoid},
    {RIL_EXT_REQUEST_STORE_SMS_TO_SIM, freeInts},
    {RIL_EXT_REQUEST_QUERY_SMS_STORAGE_MODE, freeVoid},
    {RIL_EXT_REQUEST_GET_SIMLOCK_REMAIN_TIMES, freeVoid},
    {RIL_EXT_REQUEST_SET_FACILITY_LOCK_FOR_USER, freeStrings},
    {RIL_EXT_REQUEST_GET_SIMLOCK_STATUS, freeInts},
    {RIL_EXT_REQUEST_GET_SIMLOCK_DUMMYS, freeVoid},
    {RIL_EXT_REQUEST_GET_SIMLOCK_WHITE_LIST, freeInts},
    {RIL_EXT_REQUEST_UPDATE_ECCLIST, freeString},
    {RIL_EXT_REQUEST_GET_BAND_INFO, freeVoid},
    {RIL_EXT_REQUEST_SET_BAND_INFO_MODE, freeInts},
    {RIL_EXT_REQUEST_SET_SINGLE_PDN, freeInts},
    {RIL_EXT_REQUEST_SET_SPECIAL_RATCAP, freeInts},
    {RIL_EXT_REQUEST_QUERY_COLP, freeVoid},
    {RIL_EXT_REQUEST_QUERY_COLR, freeVoid},
    {RIL_EXT_REQUEST_MMI_ENTER_SIM, freeString},
    {RIL_EXT_REQUEST_UPDATE_OPERATOR_NAME, freeString},
    {RIL_EXT_REQUEST_SIMMGR_GET_SIM_STATUS, freeVoid},
    {RIL_EXT_REQUEST_SET_XCAP_IP_ADDR, freeStrings},
    {RIL_EXT_REQUEST_SEND_CMD, freeString},
    {RIL_EXT_REQUEST_GET_SIM_STATUS, freeVoid},
    {RIL_EXT_REQUEST_REATTACH, freeVoid},
    {RIL_EXT_REQUEST_SET_PREFERRED_NETWORK_TYPE, freeInts},
    {RIL_EXT_REQUEST_SHUTDOWN, freeVoid},
    {RIL_EXT_REQUEST_SET_SMS_BEARER, freeInts},
    {RIL_EXT_REQUEST_SET_VOICE_DOMAIN, freeInts},
    {RIL_EXT_REQUEST_UPDATE_CLIP, freeInts},
    {RIL_EXT_REQUEST_SET_TPMR_STATE, freeInts},
    {RIL_EXT_REQUEST_GET_TPMR_STATE, freeVoid},
    {RIL_EXT_REQUEST_SET_VIDEO_RESOLUTION, freeInts},
    {RIL_EXT_REQUEST_ENABLE_LOCAL_HOLD, freeInts},
    {RIL_EXT_REQUEST_ENABLE_WIFI_PARAM_REPORT, freeInts},
    {RIL_EXT_REQUEST_CALL_MEDIA_CHANGE_REQUEST_TIMEOUT, freeInts},
    {RIL_EXT_REQUEST_SET_DUAL_VOLTE_STATE, freeInts},
    {RIL_EXT_REQUEST_SET_LOCAL_TONE, freeInts},
    {RIL_EXT_REQUEST_UPDATE_PLMN, freeInts},
    {RIL_EXT_REQUEST_QUERY_PLMN, freeInts},
    {RIL_EXT_REQUEST_SIM_POWER_REAL, freeInts},
    {RIL_EXT_REQUEST_GET_RADIO_PREFERENCE, freeString},
    {RIL_EXT_REQUEST_SET_RADIO_PREFERENCE, freeStrings},
    {RIL_REQUEST_GET_IMS_CURRENT_CALLS, freeVoid},
    {RIL_REQUEST_SET_IMS_VOICE_CALL_AVAILABILITY, freeInts},
    {RIL_REQUEST_GET_IMS_VOICE_CALL_AVAILABILITY, freeVoid},
    {RIL_REQUEST_INIT_ISIM, freeStrings},
    {RIL_REQUEST_IMS_CALL_REQUEST_MEDIA_CHANGE, freeInts},
    {RIL_REQUEST_IMS_CALL_RESPONSE_MEDIA_CHANGE, freeInts},
    {RIL_REQUEST_SET_IMS_SMSC, freeString},
    {RIL_REQUEST_IMS_CALL_FALL_BACK_TO_VOICE, freeInts},
    {RIL_REQUEST_SET_IMS_INITIAL_ATTACH_APN, freeSetInitialAttachApn},  /****/
    {RIL_REQUEST_QUERY_CALL_FORWARD_STATUS_URI, freeCallForwardUri},  /****/
    {RIL_REQUEST_SET_CALL_FORWARD_URI, freeCallForwardUri},  /****/
    {RIL_REQUEST_IMS_INITIAL_GROUP_CALL, freeString},
    {RIL_REQUEST_IMS_ADD_TO_GROUP_CALL, freeString},
    {RIL_REQUEST_ENABLE_IMS, freeVoid},
    {RIL_REQUEST_DISABLE_IMS, freeVoid},
    {RIL_REQUEST_GET_IMS_BEARER_STATE, freeVoid},
    {RIL_REQUEST_SET_SOS_INITIAL_ATTACH_APN, freeSetInitialAttachApn},  /****/
    {RIL_REQUEST_IMS_HANDOVER, freeInts},
    {RIL_REQUEST_IMS_HANDOVER_STATUS_UPDATE, freeInts},
    {RIL_REQUEST_IMS_NETWORK_INFO_CHANGE, freeNotifyIMSNetworkInfoChanged},  /****/
    {RIL_REQUEST_IMS_HANDOVER_CALL_END, freeInts},
    {RIL_REQUEST_IMS_WIFI_ENABLE, freeInts},
    {RIL_REQUEST_IMS_WIFI_CALL_STATE_CHANGE, freeInts},
    {RIL_REQUEST_IMS_UPDATE_DATA_ROUTER, freeVoid},
    {RIL_REQUEST_IMS_HOLD_SINGLE_CALL, freeInts},
    {RIL_REQUEST_IMS_MUTE_SINGLE_CALL, freeInts},
    {RIL_REQUEST_IMS_SILENCE_SINGLE_CALL, freeInts},
    {RIL_REQUEST_IMS_ENABLE_LOCAL_CONFERENCE, freeInts},
    {RIL_REQUEST_IMS_NOTIFY_HANDOVER_CALL_INFO,freeString},
    {RIL_REQUEST_GET_IMS_SRVCC_CAPBILITY, freeVoid},
    {RIL_REQUEST_GET_IMS_PCSCF_ADDR, freeVoid},
    {RIL_REQUEST_SET_IMS_PCSCF_ADDR, freeString},
    {RIL_REQUEST_EXT_QUERY_FACILITY_LOCK, freeStrings},
    {RIL_REQUEST_IMS_REGADDR, freeVoid},
    {RIL_EXT_REQUEST_GET_PREFERRED_NETWORK_TYPE, freeVoid},
    {RIL_EXT_REQUEST_RADIO_POWER_FALLBACK, freeInts},
