/* //vendor/sprd/proprietories-source/ril/sprd_libril/sprd_ril_unsol_commands.h
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
    {RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, responseVoid, WAKE_PARTIAL},
    {RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, responseVoid, WAKE_PARTIAL},
    {RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED, responseVoid, WAKE_PARTIAL},
    {RIL_UNSOL_RESPONSE_NEW_SMS, responseString, WAKE_PARTIAL},
    {RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT, responseString, WAKE_PARTIAL},
    {RIL_UNSOL_RESPONSE_NEW_SMS_ON_SIM, responseInts, WAKE_PARTIAL},
    {RIL_UNSOL_ON_USSD, responseStrings, WAKE_PARTIAL},
    {RIL_UNSOL_ON_USSD_REQUEST, responseVoid, DONT_WAKE},
    {RIL_UNSOL_NITZ_TIME_RECEIVED, responseString, WAKE_PARTIAL},
    {RIL_UNSOL_SIGNAL_STRENGTH, responseRilSignalStrength, DONT_WAKE},
    {RIL_UNSOL_DATA_CALL_LIST_CHANGED, responseDataCallList, WAKE_PARTIAL},
    {RIL_UNSOL_SUPP_SVC_NOTIFICATION, responseSsn, WAKE_PARTIAL},
    {RIL_UNSOL_STK_SESSION_END, responseVoid, WAKE_PARTIAL},
    {RIL_UNSOL_STK_PROACTIVE_COMMAND, responseString, WAKE_PARTIAL},
    {RIL_UNSOL_STK_EVENT_NOTIFY, responseString, WAKE_PARTIAL},
    {RIL_UNSOL_STK_CALL_SETUP, responseString, WAKE_PARTIAL},
    {RIL_UNSOL_SIM_SMS_STORAGE_FULL, responseVoid, WAKE_PARTIAL},
    {RIL_UNSOL_SIM_REFRESH, responseSimRefresh, WAKE_PARTIAL},
    {RIL_UNSOL_CALL_RING, responseCallRing, WAKE_PARTIAL},
    {RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED, responseVoid, WAKE_PARTIAL},
    {RIL_UNSOL_RESPONSE_CDMA_NEW_SMS, responseCdmaSms, WAKE_PARTIAL},
    {RIL_UNSOL_RESPONSE_NEW_BROADCAST_SMS, responseBroadcastSms, WAKE_PARTIAL},
    {RIL_UNSOL_CDMA_RUIM_SMS_STORAGE_FULL, responseVoid, WAKE_PARTIAL},
    {RIL_UNSOL_RESTRICTED_STATE_CHANGED, responseInts, WAKE_PARTIAL},
    {RIL_UNSOL_ENTER_EMERGENCY_CALLBACK_MODE, responseVoid, WAKE_PARTIAL},
    {RIL_UNSOL_CDMA_CALL_WAITING, responseCdmaCallWaiting, WAKE_PARTIAL},
    {RIL_UNSOL_CDMA_OTA_PROVISION_STATUS, responseInts, WAKE_PARTIAL},
    {RIL_UNSOL_CDMA_INFO_REC, responseCdmaInformationRecords, WAKE_PARTIAL},
    {RIL_UNSOL_OEM_HOOK_RAW, responseRaw, WAKE_PARTIAL},
    {RIL_UNSOL_RINGBACK_TONE, responseInts, WAKE_PARTIAL},
    {RIL_UNSOL_RESEND_INCALL_MUTE, responseVoid, WAKE_PARTIAL},
    {RIL_UNSOL_CDMA_SUBSCRIPTION_SOURCE_CHANGED, responseInts, WAKE_PARTIAL},
    {RIL_UNSOL_CDMA_PRL_CHANGED, responseInts, WAKE_PARTIAL},
    {RIL_UNSOL_EXIT_EMERGENCY_CALLBACK_MODE, responseVoid, WAKE_PARTIAL},
    {RIL_UNSOL_RIL_CONNECTED, responseInts, WAKE_PARTIAL},
    {RIL_UNSOL_VOICE_RADIO_TECH_CHANGED, responseInts, WAKE_PARTIAL},
    {RIL_UNSOL_CELL_INFO_LIST, responseCellInfoList, WAKE_PARTIAL},
    {RIL_UNSOL_RESPONSE_IMS_NETWORK_STATE_CHANGED, responseInts, WAKE_PARTIAL},
    {RIL_UNSOL_UICC_SUBSCRIPTION_STATUS_CHANGED, responseInts, WAKE_PARTIAL},
    {RIL_UNSOL_SRVCC_STATE_NOTIFY, responseInts, WAKE_PARTIAL},
    {RIL_UNSOL_HARDWARE_CONFIG_CHANGED, responseHardwareConfig, WAKE_PARTIAL},
    {RIL_UNSOL_DC_RT_INFO_CHANGED, responseDcRtInfo, WAKE_PARTIAL},
    {RIL_UNSOL_RADIO_CAPABILITY, responseRadioCapability, WAKE_PARTIAL},
    {RIL_UNSOL_ON_SS, responseSSData, WAKE_PARTIAL},
    /*SPRD: modify for alpha identifier display in stk @{ */
    {RIL_UNSOL_STK_CC_ALPHA_NOTIFY, responseCCresult, WAKE_PARTIAL},
    /* @} */
    {RIL_UNSOL_LCEDATA_RECV, responseLceData, WAKE_PARTIAL}
#if defined (GLOBALCONFIG_RIL_SAMSUNG_LIBRIL_INTF_EXTENSION)
    ,{RIL_UNSOL_RESPONSE_IMS_NETWORK_STATE_CHANGED, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_RESPONSE_TETHERED_MODE_STATE_CHANGED, responseInts, WAKE_PARTIAL}
    ,{RIL_UNSOL_RESPONSE_DATA_NETWORK_STATE_CHANGED, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_UICC_SUBSCRIPTION_STATUS_CHANGED, responseInts, WAKE_PARTIAL}
    ,{RIL_UNSOL_QOS_STATE_CHANGED_IND, responseInts, WAKE_PARTIAL}
#endif
#if defined (RIL_SPRD_EXTENSION)
    ,{RIL_UNSOL_VIDEOPHONE_DATA, responseString, WAKE_PARTIAL}
    ,{RIL_UNSOL_VIDEOPHONE_CODEC, responseInts, WAKE_PARTIAL}
   , {RIL_UNSOL_VIDEOPHONE_DCPI, responseInts, WAKE_PARTIAL}
    ,{RIL_UNSOL_VIDEOPHONE_DSCI, responseDSCI, WAKE_PARTIAL}
    ,{RIL_UNSOL_VIDEOPHONE_STRING, responseString, WAKE_PARTIAL}
    ,{RIL_UNSOL_VIDEOPHONE_REMOTE_MEDIA, responseInts, WAKE_PARTIAL}
    ,{RIL_UNSOL_VIDEOPHONE_MM_RING, responseInts, WAKE_PARTIAL}
    ,{RIL_UNSOL_VIDEOPHONE_RELEASING, responseString, WAKE_PARTIAL}
    ,{RIL_UNSOL_VIDEOPHONE_RECORD_VIDEO, responseInts, WAKE_PARTIAL}
    ,{RIL_UNSOL_VIDEOPHONE_MEDIA_START, responseInts, WAKE_PARTIAL}
    ,{RIL_UNSOL_RESPONSE_VIDEOCALL_STATE_CHANGED, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_ON_STIN, responseInts, WAKE_PARTIAL}
    ,{RIL_UNSOL_SIM_SMS_READY, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_SIM_DROP, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_SIM_PS_REJECT, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_LTE_READY, responseInts, WAKE_PARTIAL}
    ,{RIL_UNSOL_SVLTE_USIM_READY, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_FDN_ENABLE, responseInts, WAKE_PARTIAL}
    ,{RIL_UNSOL_CALL_CSFALLBACK, responseCallCsFallBack, WAKE_PARTIAL}//SPRD:add for LTE-CSFB to handle CS fall back of MT call
    //SPRD: For WIFI get BandInfo report from modem,* BRCM4343+9620, Zhanlei Feng added. 2014.06.20 START
    ,{RIL_UNSOL_BAND_INFO, responseString, WAKE_PARTIAL}
    //SPRD: For WIFI get BandInfo report from modem,* BRCM4343+9620, Zhanlei Feng added. 2014.06.20 END
    ,{RIL_UNSOL_CALL_CSFALLBACK_FINISH, responseCallCsFallBack, WAKE_PARTIAL}//SPRD:add for LTE-CSFB to handle CS fall back of MT call
    /* SPRD: add AGPS feature for bug 436461 @{ */
    ,{RIL_UNSOL_PHY_CELL_ID, responseInts, WAKE_PARTIAL}
    /* @} */
    ,{RIL_UNSOL_RESPONSE_NEW_BROADCAST_SMS_LTE, responseBroadcastSmsLte, WAKE_PARTIAL}
    ,{RIL_UNSOL_RESPONSE_IMS_CALL_STATE_CHANGED, responseCMCCSI, WAKE_PARTIAL}
    /* SPRD: add RAU SUCCESS Report */
    ,{RIL_UNSOL_GPRS_RAU, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_RESPONS_IMS_CONN_ENABLE, responseInts, WAKE_PARTIAL}
    ,{RIL_UNSOL_CLEAR_CODE_FALLBACK, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_RESPONSE_VIDEO_QUALITY, responseInts, WAKE_PARTIAL}
    ,{RIL_UNSOL_VT_CAPABILITY, responseMDCAPU, WAKE_PARTIAL}
    /* SPRD: add for VoWifi @{ */
    ,{RIL_UNSOL_IMS_HANDOVER_REQUEST, responseInts, WAKE_PARTIAL}
    ,{RIL_UNSOL_IMS_HANDOVER_STATUS_CHANGE, responseInts, WAKE_PARTIAL}
    ,{RIL_UNSOL_IMS_NETWORK_INFO_CHANGE, responseImsNetworkInfo, WAKE_PARTIAL}
    ,{RIL_UNSOL_IMS_REGISTER_ADDRESS_CHANGE, responseStrings, WAKE_PARTIAL}
    ,{RIL_UNSOL_IMS_WIFI_PARAM, responseInts, WAKE_PARTIAL}
    /* @} */
#if defined (RIL_SUPPORTED_OEMSOCKET)
    ,{RIL_EXT_UNSOL_RIL_CONNECTED, responseVoid, WAKE_PARTIAL}
    ,{RIL_EXT_UNSOL_BAND_INFO, responseString, WAKE_PARTIAL}
    ,{RIL_EXT_UNSOL_ECC_NETWORKLIST_CHANGED, responseString, WAKE_PARTIAL}
    ,{RIL_EXT_UNSOL_EARLY_MEDIA, responseInts, WAKE_PARTIAL}
#endif
#endif
#if defined (GLOBALCONFIG_RIL_SAMSUNG_LIBRIL_INTF_EXTENSION)
    ,{RIL_UNSOL_RESPONSE_NEW_CB_MSG, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_RELEASE_COMPLETE_MESSAGE, responsemsg, WAKE_PARTIAL}
    ,{RIL_UNSOL_STK_SEND_SMS_RESULT, responseInts, WAKE_PARTIAL}
    ,{RIL_UNSOL_STK_CALL_CONTROL_RESULT, responseCCresult, WAKE_PARTIAL}
    ,{RIL_UNSOL_DUN_CALL_STATUS, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_RESPONSE_LINE_SMS_COUNT, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_RESPONSE_LINE_SMS_READ, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_O2_HOME_ZONE_INFO, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_DEVICE_READY_NOTI, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_GPS_NOTI, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_AM, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_DUN_PIN_CONTROL_SIGNAL, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_DATA_SUSPEND_RESUME, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_SAP, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_RESPONSE_NO_NETWORK_RESPONSE, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_SIM_SMS_STORAGE_AVAILALE, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_HSDPA_STATE_CHANGED, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_WB_AMR_STATE, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_TWO_MIC_STATE, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_DHA_STATE, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_UART, responseVoid, WAKE_PARTIAL}
    ,{RIL_UNSOL_SIM_PB_READY, responseVoid, WAKE_PARTIAL}
#endif