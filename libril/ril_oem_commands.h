/* //vendor/sprd/proprietories-source/ril/libril/ril_oem_commands.h
**
** Copyright (C) 2016 Spreadtrum Communications Inc.
*/
    {0, NULL},                   // none
    {RIL_EXT_REQUEST_VIDEOPHONE_DIAL, radio::videoPhoneDialResponse},
    {RIL_EXT_REQUEST_VIDEOPHONE_CODEC, radio::videoPhoneCodecResponse},
    {RIL_EXT_REQUEST_VIDEOPHONE_FALLBACK, radio::videoPhoneFallbackResponse},
    {RIL_EXT_REQUEST_VIDEOPHONE_STRING, radio::videoPhoneStringResponse},
    {RIL_EXT_REQUEST_VIDEOPHONE_LOCAL_MEDIA, radio::videoPhoneLocalMediaResponse},
    {RIL_EXT_REQUEST_VIDEOPHONE_CONTROL_IFRAME, radio::videoPhoneControlIFrameResponse},
    {RIL_EXT_REQUEST_SWITCH_MULTI_CALL, radio::switchMultiCallResponse},
    {RIL_EXT_REQUEST_TRAFFIC_CLASS, radio::setTrafficClassResponse},
    {RIL_EXT_REQUEST_ENABLE_LTE, radio::enableLTEResponse},
    {RIL_EXT_REQUEST_ATTACH_DATA, radio::attachDataResponse},
    {RIL_EXT_REQUEST_STOP_QUERY_NETWORK, radio::stopQueryNetworkResponse},
    {RIL_EXT_REQUEST_FORCE_DETACH, radio::forceDeatchResponse},
    {RIL_EXT_REQUEST_GET_HD_VOICE_STATE, radio::getHDVoiceStateResponse},
    {RIL_EXT_REQUEST_SIMMGR_SIM_POWER, radio::simmgrSimPowerResponse},
    {RIL_EXT_REQUEST_ENABLE_RAU_NOTIFY, radio::enableRauNotifyResponse},
    {RIL_EXT_REQUEST_SET_COLP, radio::setCOLPResponse},
    {RIL_EXT_REQUEST_GET_DEFAULT_NAN, radio::getDefaultNANResponse},
    {RIL_EXT_REQUEST_SIM_GET_ATR, radio::simGetAtrResponse},
    {RIL_EXT_REQUEST_EXPLICIT_CALL_TRANSFER, radio::explicitCallTransferExtResponse},
    {RIL_EXT_REQUEST_GET_SIM_CAPACITY, radio::getSimCapacityResponse},
    {RIL_EXT_REQUEST_STORE_SMS_TO_SIM, radio::storeSmsToSimResponse},
    {RIL_EXT_REQUEST_QUERY_SMS_STORAGE_MODE, radio::querySmsStorageModeResponse},
    {RIL_EXT_REQUEST_GET_SIMLOCK_REMAIN_TIMES, radio::getSimlockRemaintimesResponse},
    {RIL_EXT_REQUEST_SET_FACILITY_LOCK_FOR_USER, radio::setFacilityLockForUserResponse},
    {RIL_EXT_REQUEST_GET_SIMLOCK_STATUS, radio::getSimlockStatusResponse},
    {RIL_EXT_REQUEST_GET_SIMLOCK_DUMMYS, radio::getSimlockDummysResponse},
    {RIL_EXT_REQUEST_GET_SIMLOCK_WHITE_LIST, radio::getSimlockWhitelistResponse},
    {RIL_EXT_REQUEST_UPDATE_ECCLIST, radio::updateEcclistResponse},
    {RIL_EXT_REQUEST_GET_BAND_INFO, radio::getBandInfoResponse},
    {RIL_EXT_REQUEST_SET_BAND_INFO_MODE, radio::setBandInfoModeResponse},
    {RIL_EXT_REQUEST_SET_SINGLE_PDN, radio::setSinglePDNResponse},
    {RIL_EXT_REQUEST_SET_SPECIAL_RATCAP, radio::setSpecialRatcapResponse},
    {RIL_EXT_REQUEST_QUERY_COLP, radio::queryColpResponse},
    {RIL_EXT_REQUEST_QUERY_COLR, radio::queryColrResponse},
    {RIL_EXT_REQUEST_MMI_ENTER_SIM, radio::mmiEnterSimResponse},
    {RIL_EXT_REQUEST_UPDATE_OPERATOR_NAME, radio::updateOperatorNameResponse},
    {RIL_EXT_REQUEST_SIMMGR_GET_SIM_STATUS, radio::simmgrGetSimStatusResponse},
    {RIL_EXT_REQUEST_SET_XCAP_IP_ADDR, radio::setXcapIPAddressResponse},
    {RIL_EXT_REQUEST_SEND_CMD, radio::sendCmdAsyncResponse},
    {RIL_EXT_REQUEST_GET_SIM_STATUS, radio::getIccCardStatusExtResponse},
    {RIL_EXT_REQUEST_REATTACH, radio::reAttachResponse},
    {RIL_EXT_REQUEST_SET_PREFERRED_NETWORK_TYPE, radio::setPreferredNetworkTypeExtResponse},
    {RIL_EXT_REQUEST_SHUTDOWN, radio::requestShutdownExtResponse},
    {RIL_EXT_REQUEST_SET_SMS_BEARER, radio::setSmsBearerResponse},
    {RIL_EXT_REQUEST_SET_VOICE_DOMAIN, radio::setVoiceDomainResponse},
    {RIL_EXT_REQUEST_UPDATE_CLIP, radio::updateCLIPResponse},
    {RIL_EXT_REQUEST_SET_TPMR_STATE, radio::setTPMRStateResponse},
    {RIL_EXT_REQUEST_GET_TPMR_STATE, radio::getTPMRStateResponse},
    {RIL_EXT_REQUEST_SET_VIDEO_RESOLUTION, radio::setVideoResolutionResponse},
    {RIL_EXT_REQUEST_ENABLE_LOCAL_HOLD, radio::enableLocalHoldResponse},
    {RIL_EXT_REQUEST_ENABLE_WIFI_PARAM_REPORT, radio::enableWiFiParamReportResponse},
    {RIL_EXT_REQUEST_CALL_MEDIA_CHANGE_REQUEST_TIMEOUT, radio::callMediaChangeRequestTimeOutResponse},
    {RIL_EXT_REQUEST_SET_DUAL_VOLTE_STATE, radio::setDualVolteStateResponse},
    {RIL_EXT_REQUEST_SET_LOCAL_TONE, radio::setLocalToneResponse},
    {RIL_EXT_REQUEST_UPDATE_PLMN, radio::updatePlmnPriorityResponse},
    {RIL_EXT_REQUEST_QUERY_PLMN, radio::queryPlmnResponse},
    {RIL_EXT_REQUEST_SIM_POWER_REAL, radio::setSimPowerRealResponse},
    {RIL_EXT_REQUEST_GET_RADIO_PREFERENCE, radio::getRadioPreferenceResponse},
    {RIL_EXT_REQUEST_SET_RADIO_PREFERENCE, radio::setRadioPreferenceResponse},
    {RIL_REQUEST_GET_IMS_CURRENT_CALLS, radio::getIMSCurrentCallsResponse},
    {RIL_REQUEST_SET_IMS_VOICE_CALL_AVAILABILITY, radio::setIMSVoiceCallAvailabilityResponse},
    {RIL_REQUEST_GET_IMS_VOICE_CALL_AVAILABILITY, radio::getIMSVoiceCallAvailabilityResponse},
    {RIL_REQUEST_INIT_ISIM, radio::initISIMResponse},
    {RIL_REQUEST_IMS_CALL_REQUEST_MEDIA_CHANGE, radio::requestVolteCallMediaChangeResponse},
    {RIL_REQUEST_IMS_CALL_RESPONSE_MEDIA_CHANGE, radio::responseVolteCallMediaChangeResponse},
    {RIL_REQUEST_SET_IMS_SMSC, radio::setIMSSmscAddressResponse},
    {RIL_REQUEST_IMS_CALL_FALL_BACK_TO_VOICE, radio::volteCallFallBackToVoiceResponse},
    {RIL_REQUEST_SET_IMS_INITIAL_ATTACH_APN, radio::setIMSInitialAttachApnResponse},
    {RIL_REQUEST_QUERY_CALL_FORWARD_STATUS_URI, radio::queryCallForwardStatusResponse},
    {RIL_REQUEST_SET_CALL_FORWARD_URI, radio::setCallForwardUriResponse},
    {RIL_REQUEST_IMS_INITIAL_GROUP_CALL, radio::IMSInitialGroupCallResponse},
    {RIL_REQUEST_IMS_ADD_TO_GROUP_CALL, radio::IMSAddGroupCallResponse},
    {RIL_REQUEST_ENABLE_IMS, radio::enableIMSResponse},
    {RIL_REQUEST_DISABLE_IMS, radio::disableIMSResponse},
    {RIL_REQUEST_GET_IMS_BEARER_STATE, radio::getIMSBearerStateResponse},
    {RIL_REQUEST_SET_SOS_INITIAL_ATTACH_APN, radio::setInitialAttachSOSApnResponse},
    {RIL_REQUEST_IMS_HANDOVER, radio::IMSHandoverResponse},
    {RIL_REQUEST_IMS_HANDOVER_STATUS_UPDATE, radio::notifyIMSHandoverStatusUpdateResponse},
    {RIL_REQUEST_IMS_NETWORK_INFO_CHANGE, radio::notifyIMSNetworkInfoChangedResponse},
    {RIL_REQUEST_IMS_HANDOVER_CALL_END, radio::notifyIMSCallEndResponse},
    {RIL_REQUEST_IMS_WIFI_ENABLE, radio::notifyVoWifiEnableResponse},
    {RIL_REQUEST_IMS_WIFI_CALL_STATE_CHANGE, radio::notifyVoWifiCallStateChangedResponse},
    {RIL_REQUEST_IMS_UPDATE_DATA_ROUTER, radio::notifyDataRouterUpdateResponse},
    {RIL_REQUEST_IMS_HOLD_SINGLE_CALL, radio::IMSHoldSingleCallResponse},
    {RIL_REQUEST_IMS_MUTE_SINGLE_CALL, radio::IMSMuteSingleCallResponse},
    {RIL_REQUEST_IMS_SILENCE_SINGLE_CALL, radio::IMSSilenceSingleCallResponse},
    {RIL_REQUEST_IMS_ENABLE_LOCAL_CONFERENCE, radio::IMSEnableLocalConferenceResponse},
    {RIL_REQUEST_IMS_NOTIFY_HANDOVER_CALL_INFO, radio::notifyHandoverCallInfoResponse},
    {RIL_REQUEST_GET_IMS_SRVCC_CAPBILITY, radio::getSrvccCapbilityResponse},
    {RIL_REQUEST_GET_IMS_PCSCF_ADDR, radio::setIMSPcscfAddressResponse},
    {RIL_REQUEST_SET_IMS_PCSCF_ADDR, radio::getIMSPcscfAddressResponse},
    {RIL_REQUEST_EXT_QUERY_FACILITY_LOCK, radio::getFacilityLockForAppExtResponse},
    {RIL_REQUEST_IMS_REGADDR, radio::getImsRegAddressResponse},
    {RIL_EXT_REQUEST_GET_PREFERRED_NETWORK_TYPE, radio::getPreferredNetworkTypeExtResponse},
    {RIL_EXT_REQUEST_RADIO_POWER_FALLBACK, radio::setRadioPowerFallbackResponse},
    {RIL_EXT_REQUEST_GET_CNAP, radio::getCnapResponse},
    {RIL_EXT_REQUEST_SET_LOCATION_INFO, radio::setLocationInfoResponse},
    {RIL_EXT_REQUEST_GET_SPECIAL_RATCAP, radio::getSpecialRatcapResponse},
    {RIL_EXT_REQUEST_GET_VIDEO_RESOLUTION, radio::getVideoResolutionResponse},
    {RIL_EXT_REQUEST_GET_IMS_PANI_INFO, radio::getImsPaniInfoResponse},
    {RIL_EXT_REQUEST_SET_EMERGENCY_ONLY, radio::setEmergencyOnlyResponse},
    {RIL_EXT_REQUEST_GET_SUBSIDYLOCK_STATUS, radio::getSubsidyLockdyStatusResponse},
    {RIL_EXT_REQUEST_GET_ICCID, radio::getICCIDResponse},
