/* //vendor/sprd/proprietories-source/ril/libril/ril_oem_commands.h
**
** Copyright (C) 2016 Spreadtrum Communications Inc.
*/
    {0, NULL, NULL},                   // none
    {RIL_EXT_REQUEST_VIDEOPHONE_DIAL, dispatchVideoPhoneDial, responseVoid},
    {RIL_EXT_REQUEST_VIDEOPHONE_CODEC, dispatchVideoPhoneCodec, responseVoid},
    {RIL_EXT_REQUEST_VIDEOPHONE_FALLBACK, dispatchVoid, responseVoid},
    {RIL_EXT_REQUEST_VIDEOPHONE_STRING, dispatchString, responseVoid},
    {RIL_EXT_REQUEST_VIDEOPHONE_LOCAL_MEDIA, dispatchInts, responseVoid},
    {RIL_EXT_REQUEST_VIDEOPHONE_CONTROL_IFRAME, dispatchInts, responseVoid},
    {RIL_EXT_REQUEST_SWITCH_MULTI_CALL, dispatchInts, responseVoid},
    {RIL_EXT_REQUEST_TRAFFIC_CLASS, dispatchInts, responseVoid},
    {RIL_EXT_REQUEST_ENABLE_LTE, dispatchInts, responseVoid},
    {RIL_EXT_REQUEST_ATTACH_DATA, dispatchInts, responseVoid},
    {RIL_EXT_REQUEST_STOP_QUERY_NETWORK, dispatchVoid, responseVoid},
    {RIL_EXT_REQUEST_FORCE_DETACH, dispatchVoid, responseVoid},
    {RIL_EXT_REQUEST_GET_HD_VOICE_STATE, dispatchVoid, responseInts},
    {RIL_EXT_REQUEST_SIM_POWER, dispatchInts, responseVoid},
    {RIL_EXT_REQUEST_ENABLE_RAU_NOTIFY, dispatchVoid, responseVoid},
    {RIL_EXT_REQUEST_SET_COLP, dispatchInts, responseVoid},
    {RIL_EXT_REQUEST_GET_DEFAULT_NAN, dispatchVoid, responseString},
    {RIL_EXT_REQUEST_SIM_GET_ATR, dispatchVoid, responseString},
    {RIL_EXT_REQUEST_SIM_OPEN_CHANNEL_WITH_P2, dispatchStrings, responseInts},
    {RIL_EXT_REQUEST_GET_SIM_CAPACITY, dispatchVoid, responseStrings},
    {RIL_EXT_REQUEST_STORE_SMS_TO_SIM, dispatchInts, responseVoid},
    {RIL_EXT_REQUEST_QUERY_SMS_STORAGE_MODE, dispatchVoid, responseString},
    {RIL_EXT_REQUEST_GET_SIMLOCK_REMAIN_TIMES, dispatchInts, responseInts},
    {RIL_EXT_REQUEST_SET_FACILITY_LOCK_FOR_USER, dispatchStrings, responseVoid},
    {RIL_EXT_REQUEST_GET_SIMLOCK_STATUS, dispatchInts, responseInts},
    {RIL_EXT_REQUEST_GET_SIMLOCK_DUMMYS, dispatchVoid, responseInts},
    {RIL_EXT_REQUEST_GET_SIMLOCK_WHITE_LIST, dispatchInts, responseStrings},
    {RIL_EXT_REQUEST_UPDATE_ECCLIST, dispatchString, responseVoid},
    {RIL_EXT_REQUEST_GET_BAND_INFO, dispatchVoid, responseString},
    {RIL_EXT_REQUEST_SET_BAND_INFO_MODE, dispatchInts, responseVoid},
    {RIL_EXT_REQUEST_SET_SINGLE_PDN, dispatchInts, responseVoid},
    {RIL_EXT_REQUEST_SET_SPECIAL_RATCAP, dispatchInts, responseVoid},
    {RIL_EXT_REQUEST_QUERY_COLP, dispatchVoid, responseInts},
    {RIL_EXT_REQUEST_QUERY_COLR, dispatchVoid, responseInts},
    {RIL_EXT_REQUEST_MMI_ENTER_SIM, dispatchString, responseVoid},
    {RIL_EXT_REQUEST_UPDATE_OPERATOR_NAME, dispatchString, responseVoid},
