/* //vendor/sprd/proprietories-source/ril/libril/ril_ims_unsol_commands.h
**
** Copyright (C) 2016 Spreadtrum Communications Inc.
*/
    {RIL_UNSOL_RESPONSE_IMS_CALL_STATE_CHANGED, radio::IMSCallStateChangedInd, WAKE_PARTIAL},
    {RIL_UNSOL_RESPONSE_VIDEO_QUALITY, radio::videoQualityInd, WAKE_PARTIAL},
    {RIL_UNSOL_RESPONSE_IMS_BEARER_ESTABLISTED, radio::IMSBearerEstablished, WAKE_PARTIAL},
    {RIL_UNSOL_IMS_HANDOVER_REQUEST, radio::IMSHandoverRequestInd, WAKE_PARTIAL},
    {RIL_UNSOL_IMS_HANDOVER_STATUS_CHANGE, radio::IMSHandoverStatusChangedInd, WAKE_PARTIAL},
    {RIL_UNSOL_IMS_NETWORK_INFO_CHANGE, radio::IMSNetworkInfoChangedInd, WAKE_PARTIAL},
    {RIL_UNSOL_IMS_REGISTER_ADDRESS_CHANGE, radio::IMSRegisterAddressChangedInd, WAKE_PARTIAL},
    {RIL_UNSOL_IMS_WIFI_PARAM, radio::IMSWifiParamInd, WAKE_PARTIAL},
    {RIL_UNSOL_IMS_NETWORK_STATE_CHANGED, radio::IMSNetworkStateChangedInd, WAKE_PARTIAL}
