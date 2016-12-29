/* //vendor/sprd/proprietories-source/ril/libril/ril_ims_commands.h
**
** Copyright (C) 2016 Spreadtrum Communications Inc.
*/
    {0, NULL, NULL},
    {RIL_REQUEST_GET_IMS_CURRENT_CALLS, dispatchVoid, responseCallListIMS},
    {RIL_REQUEST_SET_IMS_VOICE_CALL_AVAILABILITY, dispatchInts, responseVoid},
    {RIL_REQUEST_GET_IMS_VOICE_CALL_AVAILABILITY, dispatchVoid, responseInts},
    {RIL_REQUEST_INIT_ISIM, dispatchStrings, responseInts},
    {RIL_REQUEST_IMS_CALL_REQUEST_MEDIA_CHANGE, dispatchInts, responseVoid},
    {RIL_REQUEST_IMS_CALL_RESPONSE_MEDIA_CHANGE, dispatchInts, responseVoid},
    {RIL_REQUEST_SET_IMS_SMSC, dispatchString, responseVoid},
    {RIL_REQUEST_IMS_CALL_FALL_BACK_TO_VOICE, dispatchInts, responseVoid},
    {RIL_REQUEST_SET_IMS_INITIAL_ATTACH_APN, dispatchSetInitialAttachApn, responseVoid},
    {RIL_REQUEST_QUERY_CALL_FORWARD_STATUS_URI, dispatchCallForwardUri, responseCallForwardsUri},
    {RIL_REQUEST_SET_CALL_FORWARD_URI, dispatchCallForwardUri, responseVoid},
    {RIL_REQUEST_IMS_INITIAL_GROUP_CALL, dispatchString, responseVoid},
    {RIL_REQUEST_IMS_ADD_TO_GROUP_CALL, dispatchString, responseVoid},
    {RIL_REQUEST_VIDEOPHONE_DIAL, dispatchVideoPhoneDial, responseVoid},
    {RIL_REQUEST_ENABLE_IMS, dispatchVoid, responseVoid},
    {RIL_REQUEST_DISABLE_IMS, dispatchVoid, responseVoid},
    {RIL_REQUEST_GET_IMS_BEARER_STATE, dispatchVoid, responseInts},
    {RIL_REQUEST_VIDEOPHONE_CODEC, dispatchVideoPhoneCodec, responseVoid},
    {RIL_REQUEST_SET_SOS_INITIAL_ATTACH_APN, dispatchSetInitialAttachApn, responseVoid}
