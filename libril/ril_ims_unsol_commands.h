/* //vendor/sprd/proprietories-source/ril/libril/ril_ims_unsol_commands.h
**
** Copyright (C) 2016 Spreadtrum Communications Inc.
*/
    {RIL_UNSOL_RESPONSE_IMS_CALL_STATE_CHANGED, responseVoid, WAKE_PARTIAL},
    {RIL_UNSOL_RESPONSE_VIDEO_QUALITY, responseInts, WAKE_PARTIAL},
    {RIL_UNSOL_RESPONSE_IMS_BEARER_ESTABLISTED, responseInts, WAKE_PARTIAL},
    {RIL_UNSOL_IMS_HANDOVER_REQUEST, responseInts, WAKE_PARTIAL},
    {RIL_UNSOL_IMS_HANDOVER_STATUS_CHANGE, responseInts, WAKE_PARTIAL},
    {RIL_UNSOL_IMS_NETWORK_INFO_CHANGE, responseImsNetworkInfo, WAKE_PARTIAL},
    {RIL_UNSOL_IMS_REGISTER_ADDRESS_CHANGE, responseStrings, WAKE_PARTIAL},
    {RIL_UNSOL_IMS_WIFI_PARAM, responseInts, WAKE_PARTIAL},
    {RIL_UNSOL_IMS_NETWORK_STATE_CHANGED, responseInts, WAKE_PARTIAL}
