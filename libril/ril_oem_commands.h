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
