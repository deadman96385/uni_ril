/* //vendor/sprd/proprietories-source/ril/sprd-ril/sprd-ril.c
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

#define LOG_TAG "RIL"

#include <telephony/ril_cdma_sms.h>
#include <unistd.h>
#include <sys/socket.h>
#include <termios.h>
#include <dlfcn.h>
#include <hardware/ril/librilutils/proto/sap-api.pb.h>
#include "pb_decode.h"
#include "pb_encode.h"

#include "sprd_ril.h"
#include "ril_sim.h"
#include "ril_network.h"
#include "ril_ss.h"
#include "ril_sms.h"
#include "ril_stk.h"
#include "ril_misc.h"
#include "ril_call.h"
#include "ril_data.h"
#include "ril_utils.h"
#include "ril_async_cmd_handler.h"
#include "channel_controller.h"
#include "ril_mmgr.h"

#define VT_DCI "\"000001B000000001B5090000010000000120008440FA282C2090A21F\""
#define VOLTE_ENABLE_PROP       "persist.sys.volte.enable"
/* For special instrument's test */
#define VOLTE_PCSCF_PROP        "persist.sys.volte.pcscf"
#define HARDWARE_VERSION_PROP   "sys.hardware.version"
#define BUILD_TYPE_PROP         "ro.build.type"
#define MTBF_ENABLE_PROP        "persist.sys.mtbf.enable"
#define VOLTE_MODE_PROP         "persist.radio.volte.mode"
#define DSDS_MODE_PROP          "persist.radio.modem.config"

struct ATChannels *s_ATChannels[MAX_AT_CHANNELS];

RIL_RadioState s_radioState[SIM_COUNT] = {
        RADIO_STATE_UNAVAILABLE
#if (SIM_COUNT >= 2)
        , RADIO_STATE_UNAVAILABLE
#endif
#if (SIM_COUNT >= 3)
        , RADIO_STATE_UNAVAILABLE
#endif
#if (SIM_COUNT >= 4)
        , RADIO_STATE_UNAVAILABLE
#endif
        };

static pthread_mutex_t s_radioStateMutex[SIM_COUNT] = {
        PTHREAD_MUTEX_INITIALIZER
#if (SIM_COUNT >= 2)
        , PTHREAD_MUTEX_INITIALIZER
#endif
#if (SIM_COUNT >= 3)
        , PTHREAD_MUTEX_INITIALIZER
#endif
#if (SIM_COUNT >= 4)
        , PTHREAD_MUTEX_INITIALIZER
#endif
};

static pthread_cond_t s_radioStateCond[SIM_COUNT] = {
        PTHREAD_COND_INITIALIZER
#if (SIM_COUNT >= 2)
        , PTHREAD_COND_INITIALIZER
#endif
#if (SIM_COUNT >= 3)
        , PTHREAD_COND_INITIALIZER
#endif
#if (SIM_COUNT >= 4)
        , PTHREAD_COND_INITIALIZER
#endif
};

const RIL_SOCKET_ID s_socketId[SIM_COUNT] = {
        RIL_SOCKET_1
#if (SIM_COUNT >= 2)
        , RIL_SOCKET_2
#if (SIM_COUNT >= 3)
        , RIL_SOCKET_3
#endif
#if (SIM_COUNT >= 4)
        , RIL_SOCKET_4
#endif
#endif
};

sem_t s_sem[SIM_COUNT];
bool s_isLTE = false;
bool s_isUserdebug = false;
int s_modemConfig = 0;
int s_roModemConfig = 0;
int s_isSimPresent[SIM_COUNT];
const char *s_modem = NULL;
const struct RIL_Env *s_rilEnv;
const struct timeval TIMEVAL_CALLSTATEPOLL = {0, 500000};
const struct timeval TIMEVAL_SIMPOLL = {1, 0};

/* trigger change to this with s_radioStateCond */
static int s_closed[SIM_COUNT];
static const struct timeval TIMEVAL_0 = {0, 0};

#if defined (ANDROID_MULTI_SIM)
static void onRequest(int request, void *data, size_t datalen,
                      RIL_Token t, RIL_SOCKET_ID socket_id);
static void onSapRequest(int request, void *data, size_t datalen, RIL_Token t,
                         RIL_SOCKET_ID socket_id);
RIL_RadioState currentState(RIL_SOCKET_ID socket_id);
#else
static void onRequest(int request, void *data, size_t datalen, RIL_Token t);
static void onSapRequest(int request, void *data, size_t datalen, RIL_Token t);
RIL_RadioState currentState();
#endif

static int onSupports(int requestCode);
static void onCancel(RIL_Token t);
static const char *getVersion();
static void onSIMReady(int channelID);
static void initVaribales(RIL_SOCKET_ID socket_id);
void processRequest(int request, void *data, size_t datalen, RIL_Token t,
                    RIL_SOCKET_ID socket_id);
static const RIL_RequestFunctions s_requestFunctions = {
        processRequest,
};
const RIL_TheadsFunctions *s_threadsFunctions = NULL;

/*** Static Variables ***/
static const RIL_RadioFunctions s_callbacks = {
    RIL_VERSION,
    onRequest,
    currentState,
    onSupports,
    onCancel,
    getVersion,
    sendCmdSync,
    initVaribales
};

const struct RIL_Env *s_rilSapEnv;
static const RIL_RadioFunctions s_sapCallbacks = {
    RIL_VERSION,
    onSapRequest,
    currentState,
    onSupports,
    onCancel,
    getVersion,
    sendCmdSync,
    initVaribales
};

void *noopRemoveWarning(void *a) {
    return a;
}

void list_init(ListNode *node) {
    node->next = node;
    node->prev = node;
}

void initOperatorInfoList(OperatorInfoList *node) {
    node->next = node;
    node->prev = node;
}

static void usage(char *s) {
#ifdef RIL_SHLIB
    RIL_UNUSED_PARM(s);
    fprintf(stderr,
            "reference-ril requires: -p <tcp port> or -d /dev/tty_device\n");
#else
    fprintf(stderr, "usage: %s [-p <tcp port>] [-d /dev/tty_device]\n", s);
    exit(-1);
#endif
}

/**
 * Callback methods from the RIL library to us
 * Call from RIL to us to make a RIL_REQUEST
 *
 * Must be completed with a call to RIL_onRequestComplete()
 *
 * RIL_onRequestComplete() may be called from any thread, before or after
 * this function returns.
 *
 * Will always be called from the same thread, so returning here implies
 * that the radio is ready to process another command (whether or not
 * the previous command has completed).
 */
#if defined (ANDROID_MULTI_SIM)
static void onRequest(int request, void *data, size_t datalen,
                      RIL_Token t, RIL_SOCKET_ID socket_id)
#else
static void onRequest(int request, void *data, size_t datalen, RIL_Token t)
#endif
{
#if defined(ANDROID_MULTI_SIM)
    enqueueRequest(request, data, datalen, t, socket_id);
#else
    enqueueRequest(request, data, datalen, t, RIL_SOCKET_1);
#endif
}

void processRequest(int request, void *data, size_t datalen, RIL_Token t,
                    RIL_SOCKET_ID socket_id) {
    int err;
    int channelID = -1;
    MemoryManager *pMC = NULL;

    if ((int)socket_id < 0 || (int)socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        goto done;
    }
    if (request == RIL_REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE ||
        request == RIL_REQUEST_CDMA_SET_ROAMING_PREFERENCE ||
        request == RIL_REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE ||
        request == RIL_REQUEST_GET_ACTIVITY_INFO ||
        request == RIL_REQUEST_SET_CARRIER_RESTRICTIONS||
        request == RIL_REQUEST_GET_CARRIER_RESTRICTIONS ||
        request == RIL_REQUEST_STOP_LCE) {
        RIL_onRequestComplete(t, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
        goto done;
    } else if (request == RIL_REQUEST_PULL_LCEDATA) {
        RIL_onRequestComplete(t, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
        goto done;
    } else if (request == RIL_REQUEST_START_LCE) {
        RIL_onRequestComplete(t, RIL_E_LCE_NOT_SUPPORTED, NULL, 0);
        goto done;
    }

    RIL_RadioState radioState = s_radioState[socket_id];

    RLOGD("onRequest: %s radioState=%d", requestToString(request), radioState);
    /**
     * Ignore all requests except !(requests)
     * when RADIO_STATE_UNAVAILABLE.
     */
    if (radioState == RADIO_STATE_UNAVAILABLE &&
        !(request == RIL_REQUEST_GET_SIM_STATUS ||
          request == RIL_REQUEST_GET_IMEI ||
          request == RIL_REQUEST_GET_IMEISV ||
          request == RIL_REQUEST_DEVICE_IDENTITY ||
          request == RIL_REQUEST_OEM_HOOK_STRINGS ||
          request == RIL_REQUEST_GET_RADIO_CAPABILITY ||
          request == RIL_REQUEST_SET_RADIO_CAPABILITY ||
          request == RIL_REQUEST_SIM_AUTHENTICATION ||
          request == RIL_REQUEST_SIM_CLOSE_CHANNEL ||
          request == RIL_REQUEST_SIM_OPEN_CHANNEL ||
          request == RIL_REQUEST_SIM_TRANSMIT_APDU_CHANNEL ||
          request == RIL_REQUEST_SIM_TRANSMIT_APDU_BASIC ||
          request == RIL_REQUEST_SHUTDOWN ||
          request == RIL_REQUEST_GET_IMS_BEARER_STATE ||
          request == RIL_REQUEST_GET_IMS_SRVCC_CAPBILITY ||
          request == RIL_REQUEST_SET_BAND_MODE ||
          request == RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE ||
          request == RIL_EXT_REQUEST_GET_HD_VOICE_STATE ||
          request == RIL_EXT_REQUEST_GET_SIMLOCK_STATUS ||
          request == RIL_EXT_REQUEST_GET_SIMLOCK_DUMMYS ||
          request == RIL_EXT_REQUEST_GET_SIMLOCK_WHITE_LIST ||
          request == RIL_EXT_REQUEST_SIMMGR_SIM_POWER ||
          request == RIL_EXT_REQUEST_SIMMGR_GET_SIM_STATUS ||
          request == RIL_EXT_REQUEST_SEND_CMD ||
          request == RIL_EXT_REQUEST_GET_SIM_STATUS ||
          request == RIL_EXT_REQUEST_SHUTDOWN)) {
        RIL_onRequestComplete(t, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
        goto done;
    }

    /**
     * Ignore all non-power requests when RADIO_STATE_OFF
     * except !(requests)
     */
    if (radioState == RADIO_STATE_OFF
            && !(request == RIL_REQUEST_RADIO_POWER ||
                 request == RIL_REQUEST_GET_SIM_STATUS ||
                 request == RIL_REQUEST_ENTER_SIM_PIN ||
                 request == RIL_REQUEST_ENTER_SIM_PIN2 ||
                 request == RIL_REQUEST_ENTER_SIM_PUK ||
                 request == RIL_REQUEST_ENTER_SIM_PUK2 ||
                 request == RIL_REQUEST_CHANGE_SIM_PIN ||
                 request == RIL_REQUEST_CHANGE_SIM_PIN2 ||
                 request == RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING ||
                 request == RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE ||
                 request == RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND ||
                 request == RIL_REQUEST_SIM_IO ||
                 request == RIL_REQUEST_SET_SMSC_ADDRESS ||
                 request == RIL_REQUEST_GET_SMSC_ADDRESS ||
                 request == RIL_REQUEST_BASEBAND_VERSION ||
                 request == RIL_REQUEST_GET_IMEI ||
                 request == RIL_REQUEST_GET_IMEISV ||
                 request == RIL_REQUEST_DEVICE_IDENTITY ||
                 request == RIL_REQUEST_SCREEN_STATE ||
                 request == RIL_REQUEST_DELETE_SMS_ON_SIM ||
                 request == RIL_REQUEST_GET_IMSI ||
                 request == RIL_REQUEST_QUERY_FACILITY_LOCK ||
                 request == RIL_REQUEST_SET_FACILITY_LOCK ||
                 request == RIL_REQUEST_OEM_HOOK_STRINGS ||
                 request == RIL_REQUEST_SET_INITIAL_ATTACH_APN ||
                 request == RIL_REQUEST_ALLOW_DATA ||
                 request == RIL_REQUEST_GET_RADIO_CAPABILITY ||
                 request == RIL_REQUEST_SET_RADIO_CAPABILITY ||
                 request == RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE ||
                 request == RIL_REQUEST_SHUTDOWN ||
                 request == RIL_REQUEST_SIM_AUTHENTICATION ||
                 request == RIL_REQUEST_SIM_CLOSE_CHANNEL ||
                 request == RIL_REQUEST_SIM_OPEN_CHANNEL ||
                 request == RIL_REQUEST_SIM_TRANSMIT_APDU_CHANNEL ||
                 request == RIL_REQUEST_SIM_TRANSMIT_APDU_BASIC ||
                 request == RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE ||
                 request == RIL_REQUEST_NV_RESET_CONFIG ||
                 request == RIL_REQUEST_SET_UNSOL_CELL_INFO_LIST_RATE ||
                 request == RIL_REQUEST_SET_BAND_MODE ||
                 request == RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE ||
                 request == RIL_REQUEST_SET_LOCATION_UPDATES ||
                 request == RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION ||
                 request == RIL_REQUEST_SET_TTY_MODE ||
                 request == RIL_REQUEST_QUERY_TTY_MODE ||
                 request == RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE ||
                 request == RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG ||
                 request == RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG ||
                 request == RIL_REQUEST_GSM_SMS_BROADCAST_ACTIVATION ||
                 request == RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE ||
                 request == RIL_REQUEST_VOICE_RADIO_TECH ||
                 request == RIL_REQUEST_GET_NEIGHBORING_CELL_IDS ||
                 request == RIL_REQUEST_GET_MUTE ||
                 request == RIL_REQUEST_GET_CURRENT_CALLS ||
                 request == RIL_REQUEST_LAST_CALL_FAIL_CAUSE ||
                 /* IMS Request @{ */
                 request == RIL_REQUEST_GET_IMS_CURRENT_CALLS ||
                 request == RIL_REQUEST_SET_IMS_VOICE_CALL_AVAILABILITY ||
                 request == RIL_REQUEST_GET_IMS_VOICE_CALL_AVAILABILITY ||
                 request == RIL_REQUEST_INIT_ISIM ||
                 request == RIL_REQUEST_SET_IMS_SMSC ||
                 request == RIL_REQUEST_SET_IMS_INITIAL_ATTACH_APN ||
                 request == RIL_REQUEST_GET_IMS_BEARER_STATE ||
                 request == RIL_REQUEST_SET_SOS_INITIAL_ATTACH_APN ||
                 request == RIL_REQUEST_GET_IMS_SRVCC_CAPBILITY ||
                 request == RIL_EXT_REQUEST_GET_TPMR_STATE ||
                 request == RIL_EXT_REQUEST_SET_TPMR_STATE ||
                 request == RIL_EXT_REQUEST_SET_VIDEO_RESOLUTION ||
                 request == RIL_EXT_REQUEST_ENABLE_WIFI_PARAM_REPORT ||
                 request == RIL_EXT_REQUEST_CALL_MEDIA_CHANGE_REQUEST_TIMEOUT ||
                 request == RIL_EXT_REQUEST_SET_DUAL_VOLTE_STATE ||
                 request == RIL_REQUEST_IMS_HANDOVER ||
                 request == RIL_REQUEST_IMS_HANDOVER_STATUS_UPDATE ||
                 request == RIL_REQUEST_IMS_NETWORK_INFO_CHANGE ||
                 request == RIL_REQUEST_IMS_HANDOVER_CALL_END ||
                 request == RIL_REQUEST_IMS_WIFI_ENABLE ||
                 request == RIL_REQUEST_IMS_WIFI_CALL_STATE_CHANGE ||
                 request == RIL_REQUEST_IMS_UPDATE_DATA_ROUTER ||
                 request == RIL_REQUEST_IMS_NOTIFY_HANDOVER_CALL_INFO ||
                 request == RIL_REQUEST_GET_IMS_PCSCF_ADDR ||
                 request == RIL_REQUEST_SET_IMS_PCSCF_ADDR ||
                 request == RIL_REQUEST_EXT_QUERY_FACILITY_LOCK ||
                 /* }@ */
                 request == RIL_EXT_REQUEST_GET_HD_VOICE_STATE ||
                 request == RIL_EXT_REQUEST_SIMMGR_SIM_POWER ||
                 request == RIL_EXT_REQUEST_ENABLE_RAU_NOTIFY ||
                 request == RIL_EXT_REQUEST_GET_SIMLOCK_REMAIN_TIMES ||
                 request == RIL_EXT_REQUEST_SET_FACILITY_LOCK_FOR_USER ||
                 request == RIL_EXT_REQUEST_GET_SIMLOCK_STATUS ||
                 request == RIL_EXT_REQUEST_GET_SIMLOCK_DUMMYS ||
                 request == RIL_EXT_REQUEST_GET_SIMLOCK_WHITE_LIST ||
                 request == RIL_EXT_REQUEST_SET_COLP ||
                 request == RIL_EXT_REQUEST_SIM_GET_ATR ||
                 request == RIL_EXT_REQUEST_STORE_SMS_TO_SIM ||
                 request == RIL_EXT_REQUEST_QUERY_SMS_STORAGE_MODE ||
                 request == RIL_EXT_REQUEST_UPDATE_ECCLIST ||
                 request == RIL_EXT_REQUEST_GET_BAND_INFO ||
                 request == RIL_EXT_REQUEST_SET_BAND_INFO_MODE ||
                 request == RIL_EXT_REQUEST_SET_SPECIAL_RATCAP ||
                 request == RIL_EXT_REQUEST_SIMMGR_GET_SIM_STATUS ||
                 request == RIL_EXT_REQUEST_SEND_CMD ||
                 request == RIL_EXT_REQUEST_GET_SIM_STATUS ||
                 request == RIL_EXT_REQUEST_SHUTDOWN ||
                 request == RIL_EXT_REQUEST_SET_VOICE_DOMAIN)) {
        RIL_onRequestComplete(t, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
        goto done;
    }

    channelID = getChannel(socket_id);
    if (channelID < 0 || channelID >= MAX_AT_CHANNELS) {
        RLOGE("Invalid request channelID");
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        goto done;
    }

    if (!(processSimRequests(request, data, datalen, t, channelID) ||
          processCallRequest(request, data, datalen, t, channelID) ||
          processNetworkRequests(request, data, datalen, t, channelID) ||
          processDataRequest(request, data, datalen, t, channelID) ||
          processSmsRequests(request, data, datalen, t, channelID) ||
          processMiscRequests(request, data, datalen, t, channelID) ||
          processStkRequests(request, data, datalen, t, channelID) ||
          processSSRequests(request, data, datalen, t, channelID))) {
        RIL_onRequestComplete(t, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
    }
    putChannel(channelID);

done:
    if (request > 0 && request <= RIL_REQUEST_LAST) {
        pMC = &(s_memoryManager[request]);
    } else if (request > RIL_IMS_REQUEST_BASE && request <= RIL_IMS_REQUEST_LAST) {
        request = request - RIL_IMS_REQUEST_BASE;
        pMC = &(s_imsMemoryManager[request]);
    } else if (request > RIL_EXT_REQUEST_BASE && request <= RIL_EXT_REQUEST_LAST) {
        request = request - RIL_EXT_REQUEST_BASE;
        pMC = &(s_oemMemoryManager[request]);
    }
    if (pMC != NULL) {
        pMC->freeFunction(data, datalen);
    } else {
        RLOGE("%s hasn't freeFunction, check!", requestToString(request));
    }
}

/*
 * sap
 */
void OnSapRequestComplete(RIL_Token t, RIL_Errno e, MsgId msgId, void *data) {
    bool success = false;
    int i = 0;
    size_t encoded_size = 0;
    size_t buffer_size = 0;
    ssize_t written_bytes;
    uint32_t written_size = 0;
    pb_ostream_t ostream;
    const pb_field_t *fields = NULL;

    RLOGD("OnSapRequestComplete, msgId (%d)", msgId);

    switch (msgId) {
        case MsgId_RIL_SIM_SAP_CONNECT:
            fields = RIL_SIM_SAP_CONNECT_RSP_fields;
            break;
        case MsgId_RIL_SIM_SAP_DISCONNECT:
            fields = RIL_SIM_SAP_DISCONNECT_RSP_fields;
            break;
        case MsgId_RIL_SIM_SAP_APDU:
            fields = RIL_SIM_SAP_APDU_RSP_fields;
            break;
        case MsgId_RIL_SIM_SAP_TRANSFER_ATR:
            fields = RIL_SIM_SAP_TRANSFER_ATR_RSP_fields;
            break;
        case MsgId_RIL_SIM_SAP_POWER:
            fields = RIL_SIM_SAP_POWER_RSP_fields;
            break;
        case MsgId_RIL_SIM_SAP_RESET_SIM:
            fields = RIL_SIM_SAP_RESET_SIM_RSP_fields;
            break;
        case MsgId_RIL_SIM_SAP_SET_TRANSFER_PROTOCOL:
            fields = RIL_SIM_SAP_SET_TRANSFER_PROTOCOL_RSP_fields;
            break;
        case MsgId_RIL_SIM_SAP_ERROR_RESP:
            fields = RIL_SIM_SAP_ERROR_RSP_fields;
            break;
        default:
            RLOGE("OnSapRequestComplete, MsgId error!");
            return;
    }

    if ((success = pb_get_encoded_size(&encoded_size, fields, data)) &&
        encoded_size <= INT32_MAX) {
        buffer_size = encoded_size;
        uint8_t buffer[buffer_size];
        ostream = pb_ostream_from_buffer(buffer, buffer_size);
        success = pb_encode(&ostream, fields, data);

        if (success) {
            RLOGD("OnSapRequestComplete, Size: %d (0x%x) written size: 0x%x",
                encoded_size, encoded_size, written_size);
            s_rilSapEnv->OnRequestComplete(t, e, buffer, buffer_size);
        } else {
            RLOGE("OnSapRequestComplete, Encode failed!");
        }
    } else {
        RLOGE("Not sending response msgId %d: encoded_size: %u. result: %d",
        msgId, encoded_size, success);
    }
}

#if defined(ANDROID_MULTI_SIM)
static void onSapRequest(int request, void *data, size_t datalen, RIL_Token t,
                         RIL_SOCKET_ID socket_id) {
    RIL_UNUSED_PARM(socket_id);
#else
static void onSapRequest(int request, void *data, size_t datalen, RIL_Token t) {
#endif
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);


    RLOGD("onSapRequest: %d", request);
    if (request < MsgId_RIL_SIM_SAP_CONNECT /* MsgId_UNKNOWN_REQ */ ||
        request > MsgId_RIL_SIM_SAP_SET_TRANSFER_PROTOCOL) {
        RLOGE("invalid msgId");
        RIL_SIM_SAP_ERROR_RSP rsp;
        rsp.dummy_field = 1;
        OnSapRequestComplete(t, (RIL_Errno)Error_RIL_E_REQUEST_NOT_SUPPORTED,
                                MsgId_RIL_SIM_SAP_ERROR_RESP ,&rsp);
        return;
    }

    RIL_SIM_SAP_CONNECT_RSP rsp;
    rsp.response = RIL_SIM_SAP_CONNECT_RSP_Response_RIL_E_SAP_CONNECT_FAILURE;
    rsp.has_max_message_size = false;
    rsp.max_message_size = 0;
    OnSapRequestComplete(t, (RIL_Errno)Error_RIL_E_GENERIC_FAILURE,
                            MsgId_RIL_SIM_SAP_CONNECT, &rsp);
}

/**
 * Synchronous call from the RIL to us to return current radio state.
 * RADIO_STATE_UNAVAILABLE should be the initial state.
 */
#if defined (ANDROID_MULTI_SIM)
RIL_RadioState currentState(RIL_SOCKET_ID socket_id) {
    return s_radioState[socket_id];
}
#else
RIL_RadioState currentState() {
    return s_radioState[RIL_SOCKET_1];
}
#endif

/**
 * Call from RIL to us to find out whether a specific request code
 * is supported by this implementation.
 *
 * Return 1 for "supported" and 0 for "unsupported"
 */
int onSupports(int requestCode) {
    /* @@@ TODO */
    RIL_UNUSED_PARM(requestCode);
    return 1;
}

void onCancel(RIL_Token t) {
    /* @@@ TODO */
    RIL_UNUSED_PARM(t);
}

const char *getVersion(void) {
    return "android reference-ril 1.0";
}

int getModemConfig() {
    char prop[PROPERTY_VALUE_MAX] = {0};
    int modemConfig = 0;

    property_get(MODEM_CONFIG_PROP, prop, "");
    if (strcmp(prop, "TL_LF_TD_W_G,W_G") == 0 || strcmp(prop, "TL_LF_W_G,W_G") == 0) {
        modemConfig = LWG_WG;
    } else if (strcmp(prop, "TL_LF_TD_W_G,TL_LF_TD_W_G") == 0 ||
               strcmp(prop, "TL_LF_W_G,TL_LF_W_G") == 0) {
        modemConfig = LWG_LWG;
    } else if (strcmp(prop, "TL_LF_G,G") == 0) {
        modemConfig = LG_G;
    } else if (strcmp(prop, "W_G,G") == 0) {
        modemConfig = W_G;
    } else if (strcmp(prop, "W_G,W_G") == 0) {
        modemConfig = WG_WG;
    }
    return modemConfig;
}
int getROModemConfig() {
    char prop[PROPERTY_VALUE_MAX] = {0};
    int modemConfig = 0;

    property_get(MODEM_CAPABILITY, prop, "");
    if (strcmp(prop, "TL_LF_TD_W_G,W_G") == 0 || strcmp(prop, "TL_LF_W_G,W_G") == 0) {
        modemConfig = LWG_WG;
    } else if (strcmp(prop, "TL_LF_TD_W_G,TL_LF_TD_W_G") == 0 ||
                strcmp(prop, "TL_LF_W_G,TL_LF_W_G") == 0) {
        modemConfig = LWG_LWG;
    }
    return modemConfig;
}

void initVaribales(RIL_SOCKET_ID socket_id) {
    RIL_UNUSED_PARM(socket_id);

    s_modemConfig = getModemConfig();
    s_roModemConfig = getROModemConfig();
    RLOGD("s_modemConfig = %d", s_modemConfig);

    initPrimarySim();
}

/**
 * SIM ready means any commands that access the SIM will work, including:
 *  AT+CPIN, AT+CSMS, AT+CNMI, AT+CRSM
 *  (all SMS-related commands)
 */
static void pollSIMState(void *param) {
    CallbackPara *pollSimStatePara = (CallbackPara *)param;
    RIL_SOCKET_ID socket_id = pollSimStatePara->socket_id;

    if ((int)socket_id < 0 || (int)socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        free(pollSimStatePara->para);
        free(pollSimStatePara);
        return;
    }

    if (s_radioState[socket_id] != RADIO_STATE_SIM_NOT_READY) {
        free(pollSimStatePara->para);
        free(pollSimStatePara);
        /* no longer valid to poll */
        return;
    }

    int channelID;
    int channelIDPassed = *((int *)(pollSimStatePara->para));
    if (channelIDPassed < 0) {
        channelID = getChannel(socket_id);
    } else {
        channelID = channelIDPassed;
    }
    free(pollSimStatePara->para);
    free(pollSimStatePara);

    switch (getSIMStatus(-1, channelID)) {
        case SIM_ABSENT:
        case SIM_PIN:
        case SIM_PUK:
        case SIM_NETWORK_PERSONALIZATION:
        default: {
            RLOGI("SIM ABSENT or LOCKED");
            setRadioState(channelID, RADIO_STATE_SIM_LOCKED_OR_ABSENT);
            if (channelIDPassed < 0) {
                putChannel(channelID);
            }
            return;
        }
        case SIM_NOT_READY: {
            if (channelIDPassed < 0) {
                putChannel(channelID);
            }
            CallbackPara *cbPara = (CallbackPara *)malloc(sizeof(CallbackPara));
            if (cbPara != NULL) {
                cbPara->para = (int *)malloc(sizeof(int));
                *((int *)(cbPara->para)) = -1;
                cbPara->socket_id = socket_id;
            }
            RIL_requestTimedCallback(pollSIMState, (void *)cbPara,
                                     &TIMEVAL_SIMPOLL);
            return;
        }
        case SIM_READY: {
            RLOGI("SIM_READY");
            setRadioState(channelID, RADIO_STATE_SIM_READY);
            RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED,
                    NULL, 0, socket_id);
            if (channelIDPassed < 0) {
                putChannel(channelID);
            }
            return;
        }
    }
}

/**
 * do post-AT+CFUN=1 initialization
 */
static void onRadioPowerOn(int channelID) {
    at_send_command(s_ATChannels[channelID], "AT+CTZR=1", NULL);

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
    CallbackPara *cbPara = (CallbackPara *)malloc(sizeof(CallbackPara));
    if (cbPara != NULL) {
        cbPara->para = (int *)malloc(sizeof(int));
        *((int *)(cbPara->para)) = channelID;
        cbPara->socket_id = socket_id;
        pollSIMState(cbPara);
    }
}

/**
 * do post- SIM ready initialization
 */
static void onSIMReady(int channelID) {
    at_send_command_singleline(s_ATChannels[channelID], "AT+CSMS=1", "+CSMS:",
                               NULL);
    /**
     * Always send SMS messages directly to the TE
     *
     * mode = 1 // discard when link is reserved (link should never be
     *             reserved)
     * mt = 2   // most messages routed to TE
     * bm = 2   // new cell BM's routed to TE
     * ds = 1   // Status reports routed to TE
     * bfr = 1  // flush buffer
     */
    at_send_command(s_ATChannels[channelID], "AT+CNMI=3,2,2,1,1", NULL);
}

void setRadioState(int channelID, RIL_RadioState newState) {
    RIL_RadioState oldState;

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    RIL_RadioState *p_radioState = &(s_radioState[socket_id]);

    pthread_mutex_lock(&s_radioStateMutex[socket_id]);

    oldState = *p_radioState;

    if (s_closed[socket_id] > 0) {
        /**
         * If we're closed, the only reasonable state is
         * RADIO_STATE_UNAVAILABLE
         * This is here because things on the main thread
         * may attempt to change the radio state after the closed
         * event happened in another thread
         */
        newState = RADIO_STATE_UNAVAILABLE;
    }

    if (*p_radioState != newState || s_closed[socket_id] > 0) {
        *p_radioState = newState;

        pthread_cond_broadcast(&s_radioStateCond[socket_id]);
    }

    pthread_mutex_unlock(&s_radioStateMutex[socket_id]);

    /* Bug 503887 add ISIM for volte. @{ */
    if (newState == RADIO_STATE_OFF || newState == RADIO_STATE_UNAVAILABLE) {
        s_imsBearerEstablished[socket_id] = -1;
    }
    /* }@ */

    /* do these outside of the mutex */
    if (*p_radioState != oldState) {
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED,
                                  NULL, 0, socket_id);
        // Sim state can change as result of radio state change
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED,
                                  NULL, 0, socket_id);
        /**
         * FIXME onSimReady() and onRadioPowerOn() cannot be called
         * from the AT reader thread
         * Currently, this doesn't happen, but if that changes then these
         * will need to be dispatched on the request thread
         */
        if (*p_radioState == RADIO_STATE_SIM_READY) {
            onSIMReady(channelID);
        } else if (*p_radioState == RADIO_STATE_SIM_NOT_READY) {
            onRadioPowerOn(channelID);
        }
    }
}

void initModemProp(int channelID) {
    ATResponse *p_response = NULL;
    char modemCapbility[PROPERTY_VALUE_MAX] = {0};
    char modemConfig[PROPERTY_VALUE_MAX] = {0};
    char workmode[PROPERTY_VALUE_MAX] = {0};
    char overseaProp[PROPERTY_VALUE_MAX] = {0};
    char *line = NULL;
    int err;
    int modemCap = -1; // modem max capability

    property_get(MODEM_CAPABILITY, modemCapbility, "");
    property_get(MODEM_CONFIG_PROP, modemConfig, "");
    property_get(MODEM_WORKMODE_PROP, workmode, "");
    property_get(OVERSEA_VERSION, overseaProp, "");
    RLOGD("modemCapbility=%s, modemConfig=%s, workmode=%s", modemCapbility,
           modemConfig, workmode);

    if (s_isLTE) {
        err = at_send_command_singleline(s_ATChannels[channelID],
                "AT+SPCAPABILITY=51,0", "+SPCAPABILITY:", &p_response);
        if (err < 0 || p_response->success == 0) {
            RLOGE("AT+SPCAPABILITY=51,0 return error");
            goto error;
        }

        line = p_response->p_intermediates->line;

        err = at_tok_start(&line);
        if (err < 0) goto error;

        skipNextComma(&line);
        skipNextComma(&line);
        err = at_tok_nextint(&line, &modemCap);
        if (err < 0) {
            RLOGE("parse modemCap failed");
            goto error;
        }
        RLOGD("modemCap=%d", modemCap);
    }

    if (modemCap == 16) {  // LL-4mod
        if (!strcmp(modemCapbility, "")) {
            property_set(MODEM_CAPABILITY, "TL_LF_W_G,TL_LF_W_G");
        } else if (strcmp(modemCapbility, "TL_LF_W_G,TL_LF_W_G")) {  // for OTA modem update
            property_set(MODEM_CAPABILITY, "TL_LF_W_G,TL_LF_W_G");
            property_set(MODEM_CONFIG_PROP, "TL_LF_W_G,TL_LF_W_G");
            property_set(MODEM_WORKMODE_PROP, "6,6");
        }
        if (!strcmp(modemConfig, "")) {
            property_set(MODEM_CONFIG_PROP, "TL_LF_W_G,TL_LF_W_G");
        }
        if (!strcmp(workmode, "")) {
            property_set(MODEM_WORKMODE_PROP, "6,6");
        }
    } else if (modemCap == 8) {  // LW-4mod
        if (!strcmp(modemCapbility, "")) {
            property_set(MODEM_CAPABILITY, "TL_LF_W_G,W_G");
        } else if (strcmp(modemCapbility, "TL_LF_W_G,W_G")) {  // for OTA modem update
            property_set(MODEM_CAPABILITY, "TL_LF_W_G,W_G");
            property_set(MODEM_CONFIG_PROP, "TL_LF_W_G,W_G");
            property_set(MODEM_WORKMODE_PROP, "6,255");
        }
        if (!strcmp(modemConfig, "")) {
            property_set(MODEM_CONFIG_PROP, "TL_LF_W_G,W_G");
        }
        if (!strcmp(workmode, "")) {
            property_set(MODEM_WORKMODE_PROP, "6,255");
        }
    } else if (modemCap == 4) {  // WW
        if (!strcmp(modemCapbility, "")) {
            property_set(MODEM_CAPABILITY, "W_G,W_G");
        } else if (strcmp(modemCapbility, "W_G,W_G")) {  // for OTA modem update
            property_set(MODEM_CAPABILITY, "W_G,W_G");
            property_set(MODEM_CONFIG_PROP, "W_G,W_G");
            property_set(MODEM_WORKMODE_PROP, "22,14");
        }
        if (!strcmp(modemConfig, "")) {
            property_set(MODEM_CONFIG_PROP, "W_G,W_G");
        }
        if (!strcmp(workmode, "")) {
            property_set(MODEM_WORKMODE_PROP, "22,14");
        }
    } else if (modemCap == 2) {  // LL
        if (!strcmp(modemCapbility, "")) {
            if (strcmp(overseaProp, "")) {
                property_set(MODEM_CAPABILITY, "TL_LF_W_G,TL_LF_W_G");
            } else {
                property_set(MODEM_CAPABILITY, "TL_LF_TD_W_G,TL_LF_TD_W_G");
            }
        } else if (strcmp(modemCapbility, "TL_LF_TD_W_G,TL_LF_TD_W_G") &&
                   strcmp(modemCapbility, "TL_LF_W_G,TL_LF_W_G")) {  // for OTA modem update
            if (strcmp(overseaProp, "")) {
                property_set(MODEM_CAPABILITY, "TL_LF_W_G,TL_LF_W_G");
                property_set(MODEM_CONFIG_PROP, "TL_LF_W_G,TL_LF_W_G");
                property_set(MODEM_WORKMODE_PROP, "6,6");
            } else {
                property_set(MODEM_CAPABILITY, "TL_LF_TD_W_G,TL_LF_TD_W_G");
                property_set(MODEM_CONFIG_PROP, "TL_LF_TD_W_G,TL_LF_TD_W_G");
                property_set(MODEM_WORKMODE_PROP, "9,9");
            }
        }
        if (!strcmp(modemConfig, "")) {
            if (strcmp(overseaProp, "")) {
                property_set(MODEM_CONFIG_PROP, "TL_LF_W_G,TL_LF_W_G");
            } else {
                property_set(MODEM_CONFIG_PROP, "TL_LF_TD_W_G,TL_LF_TD_W_G");
            }
        }
        if (!strcmp(workmode, "")) {
            if (strcmp(overseaProp, "")) {
                property_set(MODEM_WORKMODE_PROP, "6,6");
            } else {
                property_set(MODEM_WORKMODE_PROP, "9,9");
            }
        }
    } else if (modemCap == 1) {  // LW
        if (!strcmp(modemCapbility, "")) {
            if (strcmp(overseaProp, "")) {
                property_set(MODEM_CAPABILITY, "TL_LF_W_G,W_G");
            } else {
                property_set(MODEM_CAPABILITY, "TL_LF_TD_W_G,W_G");
            }

        } else if (strcmp(modemCapbility, "TL_LF_TD_W_G,W_G") &&
                   strcmp(modemCapbility, "TL_LF_W_G,W_G")) {  // for OTA modem update
            if (strcmp(overseaProp, "")) {
                property_set(MODEM_CAPABILITY, "TL_LF_W_G,W_G");
                property_set(MODEM_CONFIG_PROP, "TL_LF_W_G,W_G");
                property_set(MODEM_WORKMODE_PROP, "6,255");
            } else {
                property_set(MODEM_CAPABILITY, "TL_LF_TD_W_G,W_G");
                property_set(MODEM_CONFIG_PROP, "TL_LF_TD_W_G,W_G");
                property_set(MODEM_WORKMODE_PROP, "9,255");
            }
        }
        if (!strcmp(modemConfig, "")) {
            if (strcmp(overseaProp, "")) {
                property_set(MODEM_CONFIG_PROP, "TL_LF_W_G,W_G");
            } else {
                property_set(MODEM_CONFIG_PROP, "TL_LF_TD_W_G,W_G");
            }
        }
        if (!strcmp(workmode, "")) {
            if (strcmp(overseaProp, "")) {
                property_set(MODEM_WORKMODE_PROP, "6,255");
            } else {
                property_set(MODEM_WORKMODE_PROP, "9,255");
            }
        }
    } else if (modemCap == 0) {  // LG
        if (!strcmp(modemConfig, "")) {
            if (strcmp(overseaProp, "")) {
                property_set(MODEM_CONFIG_PROP, "TL_LF_W_G,G");
            } else {
                property_set(MODEM_CONFIG_PROP, "TL_LF_TD_W_G,G");
            }
        } else if (strcmp(modemCapbility, "TL_LF_TD_W_G,G") &&
                   strcmp(modemCapbility, "TL_LF_W_G,G")) {  // for OTA modem update
            if (strcmp(overseaProp, "")) {
                property_set(MODEM_CONFIG_PROP, "TL_LF_W_G,G");
                property_set(MODEM_WORKMODE_PROP, "6,10");
            } else {
                property_set(MODEM_CONFIG_PROP, "TL_LF_TD_W_G,G");
                property_set(MODEM_WORKMODE_PROP, "9,10");
            }
        }
        if (!strcmp(workmode, "")) {
            if (strcmp(overseaProp, "")) {
                property_set(MODEM_WORKMODE_PROP, "6,10");
            } else {
                property_set(MODEM_WORKMODE_PROP, "9,10");
            }
        }
    }

    AT_RESPONSE_FREE(p_response);
    return;

error:
    RLOGE("config default mode wG");
    AT_RESPONSE_FREE(p_response);
    if (!strcmp(modemConfig, "")) {
        property_set(MODEM_CONFIG_PROP, "W_G,G");
    }
    if (!strcmp(workmode, "")) {
        property_set(MODEM_WORKMODE_PROP, "14,10");
    }
}
/**
 * Initialize everything that can be configured while we're still in
 * AT+CFUN=0
 */
static void initializeCallback(void *param) {
    ATResponse *p_response = NULL;
    ATResponse *p_response1 = NULL;
    int err;
    int channelID;

    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    if ((int)socket_id < 0 || (int)socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        return;
    }

    channelID = getChannel(socket_id);
    setRadioState(channelID, RADIO_STATE_OFF);

    /* note: we don't check errors here. Everything important will
       be handled in onATTimeout and onATReaderClosed */

    /* atchannel is tolerant of echo but it must */
    /* have verbose result codes */
    at_send_command(s_ATChannels[channelID], "ATE0Q0V1", NULL);

    /* No auto-answer */
    at_send_command(s_ATChannels[channelID], "ATS0=0", NULL);

    /* Extended errors */
    at_send_command(s_ATChannels[channelID], "AT+CMEE=1", NULL);

    /* Network registration events */
    err = at_send_command(s_ATChannels[channelID], "AT+CREG=2", &p_response);

    /* some handsets -- in tethered mode -- don't support CREG=2 */
    if (err < 0 || p_response->success == 0) {
        at_send_command(s_ATChannels[channelID], "AT+CREG=1", NULL);
    }

    AT_RESPONSE_FREE(p_response);

    if (s_isLTE) {
        /* LTE registration events */
        at_send_command(s_ATChannels[channelID], "AT+CEREG=2", NULL);
    } else {
        /* GPRS registration events */
        at_send_command(s_ATChannels[channelID], "AT+CGREG=2", NULL);
    }

    /**
     * Signal strength unsolicited switch
     *  < +CSQ: rssi,ber> will unsolicited
     */
    at_send_command(s_ATChannels[channelID], "AT+CCED=1,8", NULL);

    /* Call Waiting notifications */
    at_send_command(s_ATChannels[channelID], "AT+CCWA=1", NULL);

    /* Alternating voice/data off */
    at_send_command(s_ATChannels[channelID], "AT+CMOD=0", NULL);

    /* Not muted */
    at_send_command(s_ATChannels[channelID], "AT+CMUT=0", NULL);

    /**
     * +CSSU unsolicited supp service notifications
     * CSSU,CSSI
     */
    at_send_command(s_ATChannels[channelID], "AT+CSSN=1,1", NULL);

    /* no connected line identification */
    at_send_command(s_ATChannels[channelID], "AT+COLP=0", NULL);

    /* HEX character set */
    at_send_command(s_ATChannels[channelID], "AT+CSCS=\"HEX\"", NULL);

    /* USSD unsolicited */
    at_send_command(s_ATChannels[channelID], "AT+CUSD=1", NULL);

    /* Enable +CGEV GPRS event notifications, but don't buffer */
    at_send_command(s_ATChannels[channelID], "AT+CGEREP=1,0", NULL);

    /* SMS PDU mode */
    at_send_command(s_ATChannels[channelID], "AT+CMGF=0", NULL);

    /* set DTMF tone duration to minimum value */
    at_send_command(s_ATChannels[channelID], "AT+VTD=1", NULL);

    /* following is videophone h324 initialization */
    at_send_command(s_ATChannels[channelID], "AT+CRC=1", NULL);

    /* set IPV6 address format */
    if (s_isLTE) {
        at_send_command(s_ATChannels[channelID], "AT+CGPIAF=1", NULL);
    }

    at_send_command(s_ATChannels[channelID], "AT^DSCI=1", NULL);
    at_send_command(s_ATChannels[channelID], "AT"AT_PREFIX"DVTTYPE=1", NULL);
    at_send_command(s_ATChannels[channelID], "AT+SPVIDEOTYPE=3", NULL);
    at_send_command(s_ATChannels[channelID], "AT+SPDVTDCI="VT_DCI, NULL);
    at_send_command(s_ATChannels[channelID], "AT+SPDVTTEST=2,650", NULL);
    at_send_command(s_ATChannels[channelID], "AT+CEN=1", NULL);
    if (isVoLteEnable()) {
        at_send_command(s_ATChannels[channelID], "AT+CIREG=2", NULL);
        at_send_command(s_ATChannels[channelID], "AT+CIREP=1", NULL);
        at_send_command(s_ATChannels[channelID], "AT+CMCCS=2", NULL);
        char address[PROPERTY_VALUE_MAX];
        property_get(VOLTE_PCSCF_PROP, address, "");
        if (strcmp(address, "") != 0) {
            RLOGD("Set PCSCF address = %s", address);
            char cmd[AT_COMMAND_LEN];
            char *p_address = address;
            if (strchr(p_address, '[') != NULL) {
                snprintf(cmd, sizeof(cmd), "AT+PCSCF=2,\"%s\"", address);
            } else {
                snprintf(cmd, sizeof(cmd), "AT+PCSCF=1,\"%s\"", address);
            }
            at_send_command(s_ATChannels[channelID], cmd, NULL);
        }
        char volteMode[PROPERTY_VALUE_MAX];
        char dsdsMode[PROPERTY_VALUE_MAX];
        property_get(VOLTE_MODE_PROP, volteMode, "");
        property_get(DSDS_MODE_PROP, dsdsMode, "");
        if (strcmp(volteMode, "DualVoLTEActive") == 0) {
            // AT+SPCAPABILITY=49,1,X(Status word) to enable/disable DSDA
            // Status word 0 to disable DSDA;
            // Status word 1 for L+W/G modem to enable DSDA;
            // Status word 2 for L+L modem to enable DSDA.
            if(strcmp(dsdsMode, "TL_LF_TD_W_G,TL_LF_TD_W_G") == 0 ||
               strcmp(dsdsMode, "TL_LF_W_G,TL_LF_W_G") == 0) {
                at_send_command(s_ATChannels[channelID], "AT+SPCAPABILITY=49,1,2",
                                NULL);
            } else {
                at_send_command(s_ATChannels[channelID], "AT+SPCAPABILITY=49,1,1",
                                NULL);
            }
        }
    }

    initModemProp(channelID);

    /* set some auto report AT command on or off */
    if (isVoLteEnable()) {
        at_send_command(s_ATChannels[channelID],
            "AT+SPAURC=\"100110111110000000001000010000111111110001000110\"",
            NULL);
    } else {
        at_send_command(s_ATChannels[channelID],
            "AT+SPAURC=\"100110111110000000001000010000111111110001000100\"",
            NULL);
    }
    /* @} */

    /* for CMCC version @{ */
    if (s_isLTE) {
        char prop[PROPERTY_VALUE_MAX] = {0};
        property_get("ro.radio.spice", prop, "0");
        if (!strcmp(prop, "1")) {
            at_send_command_singleline(s_ATChannels[channelID],
                    "AT+SPCAPABILITY=32,1,1", "+SPCAPABILITY:", NULL);
        }
    }
    /* @} */

    if (!s_isLTE) {
        /*power on sim card */
        char prop[PROPERTY_VALUE_MAX];
        getProperty(socket_id, "ril.sim.power", prop, "0");

        if (!strcmp(prop, "0")) {
            at_send_command(s_ATChannels[channelID], "AT+SFUN=2", NULL);
            setProperty(socket_id, "ril.sim.power", "1");
        }
    }

    /* assume radio is off on error */
    if (isRadioOn(channelID) > 0) {
        setRadioState(channelID, RADIO_STATE_SIM_NOT_READY);
    }

    putChannel(channelID);

    list_init(&s_DTMFList[socket_id]);
    sem_post(&(s_sem[socket_id]));
}

static void waitForClose(RIL_SOCKET_ID socket_id) {
    pthread_mutex_lock(&s_radioStateMutex[socket_id]);

    while (s_closed[socket_id] == 0) {
        pthread_cond_wait(&s_radioStateCond[socket_id],
                            &s_radioStateMutex[socket_id]);
    }

    pthread_mutex_unlock(&s_radioStateMutex[socket_id]);
}

/**
 * Called by atchannel when an unsolicited line appears
 * This is called on atchannel's reader thread. AT commands may
 * not be issued here
 */
static void
onUnsolicited(int channelID, const char *s, const char *sms_pdu) {
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
    RIL_RadioState radioState = s_radioState[socket_id];

    /**
     * Ignore unsolicited responses until we're initialized.
     * This is OK because the RIL library will poll for initial state
     */
    if (s_radioState[socket_id] == RADIO_STATE_UNAVAILABLE) {
        RLOGD("[unsl] state=%d  %s", s_radioState[socket_id], s);
        return;
    }

    if (!(processSimUnsolicited(socket_id, s) ||
          processCallUnsolicited(socket_id, s) ||
          processNetworkUnsolicited(socket_id, s) ||
          processDataUnsolicited(socket_id, s) ||
          processSSUnsolicited(socket_id, s) ||
          processSmsUnsolicited(socket_id, s, sms_pdu) ||
          processStkUnsolicited(socket_id, s) ||
          processMiscUnsolicited(socket_id, s))) {
        RLOGE("Unsupported unsolicited response : %s", s);
    }
}

/* Called on command or reader thread */
static void onATReaderClosed(RIL_SOCKET_ID socket_id) {
    if ((int)socket_id < 0 || (int)socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        return;
    }

    int channel = 0;
    int channelID;
    int firstChannel, lastChannel;

    RLOGI("AT channel closed\n");
#if defined (ANDROID_MULTI_SIM)
    firstChannel = socket_id * AT_CHANNEL_OFFSET;
    lastChannel = (socket_id + 1) * AT_CHANNEL_OFFSET;
#else
    firstChannel = AT_URC;
    lastChannel = MAX_AT_CHANNELS;
#endif
    for (channel = firstChannel; channel < lastChannel; channel++) {
        at_close(s_ATChannels[channel]);
    }
    stop_reader(socket_id);
    s_closed[socket_id] = 1;
    channelID = getChannel(socket_id);

    setRadioState(channelID, RADIO_STATE_UNAVAILABLE);
    putChannel(channelID);
}

/* Called on command thread */
static void onATTimeout(RIL_SOCKET_ID socket_id) {
    if ((int)socket_id < 0 || (int)socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        return;
    }

    int channel = 0;
    int channelID;
    int firstChannel, lastChannel;

    RLOGI("AT channel timeout; closing\n");
#if defined (ANDROID_MULTI_SIM)
    firstChannel = socket_id * AT_CHANNEL_OFFSET;
    lastChannel = (socket_id + 1) * AT_CHANNEL_OFFSET;
#else
    firstChannel = AT_URC;
    lastChannel = MAX_AT_CHANNELS;
#endif
    for (channel = firstChannel; channel < lastChannel; channel++) {
        at_close(s_ATChannels[channel]);
    }
    stop_reader(socket_id);

    s_closed[socket_id] = 1;

    /* FIXME cause a radio reset here */
    channelID = getChannel(socket_id);

    setRadioState(channelID, RADIO_STATE_UNAVAILABLE);
    putChannel(channelID);
}

static void *mainLoop(void *param) {
    int fd;
    int channelID = 0;
    int firstChannel, lastChannel;
    char muxDevice[ARRAY_SIZE] = {0};
    char prop[PROPERTY_VALUE_MAX] = {0};
    char ttyName[ARRAY_SIZE] = {0};
    char channelName[ARRAY_SIZE] = {0};
    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);

#if defined (ANDROID_MULTI_SIM)
    firstChannel = socket_id * AT_CHANNEL_OFFSET;
    lastChannel = (socket_id + 1) * AT_CHANNEL_OFFSET;
#else
    firstChannel = AT_URC;
    lastChannel = MAX_AT_CHANNELS;
#endif

    snprintf(muxDevice, sizeof(muxDevice), "ro.modem.%s.tty", s_modem);
    if (!strcmp(s_modem, "t") || !strcmp(s_modem, "w")) {
        property_get(muxDevice, prop, "/dev/ts0710mux");
    } else if (s_isLTE) {
        property_get(muxDevice, prop, "/dev/sdiomux");
    }

    for (;;) {
        fd = -1;
        s_closed[socket_id] = 0;
        init_channels(socket_id);

 again:
        for (channelID = firstChannel; channelID < lastChannel; channelID++) {
            snprintf(ttyName, sizeof(ttyName), "%s%d", prop, channelID);

            fd = open(ttyName, O_RDWR | O_NONBLOCK);

            if (fd >= 0) {
                /* disable echo on serial ports */
                struct termios ios;
                tcgetattr(fd, &ios);
                ios.c_lflag = 0;  /* disable ECHO, ICANON, etc... */
                tcsetattr(fd, TCSANOW, &ios);
                RLOGI("AT channel [%d] open successfully, ttyName:%s",
                        channelID, ttyName);
            } else {
                RLOGE("Opening AT interface. retrying...");
                sleep(1);
                goto again;
            }

            setChannelInfo(fd, channelID);
            snprintf(channelName, sizeof(channelName), "Channel%d", channelID);
            s_ATChannels[channelID] = at_open(fd, channelID, channelName,
                    onUnsolicited);

            if (s_ATChannels[channelID] == NULL) {
                RLOGE("AT error on at_open\n");
                return 0;
            }
            if (s_isUserdebug) {  // only userdebug version print AT logs
                s_ATChannels[channelID]->nolog = 0;
            }
        }

        setChannelOpened(socket_id);

        start_reader(socket_id);
        RIL_requestTimedCallback(initializeCallback,
                (void *)&s_socketId[socket_id], &TIMEVAL_0);

        /* Give initializeCallback a chance to dispatched, since
         * we don't presently have a cancellation mechanism */
        sleep(1);

        waitForClose(socket_id);
        RLOGI("Re-opening after close");
    }
}

void setHwVerPorp() {
    int ret = -1;
    int fd = -1;
    char cmdline[1024];
    char *pKeyWord = "hardware.version=";
    char *pHwVer = NULL;
    char *token = NULL;

    memset(cmdline, 0, 1024);
    fd = open("/proc/cmdline", O_RDONLY);
    if (fd >= 0) {
        ret = read(fd, cmdline, sizeof(cmdline));
        if (ret > 0) {
            pHwVer = strstr(cmdline, pKeyWord);
            if (pHwVer != NULL) {
                pHwVer += strlen(pKeyWord);
                token = strchr(pHwVer, ' ');
                if (token) {
                    *token = '\0';
                }
                RLOGD("Hardware.version = %s", pHwVer);
                property_set(HARDWARE_VERSION_PROP, pHwVer);
            }
        }
        close(fd);
    }
}

#ifdef RIL_SHLIB

int s_sim_num;
pthread_t s_mainLoopTid[SIM_COUNT];

const RIL_RadioFunctions *RIL_Init(const struct RIL_Env *env,
        int argc, char **argv) {
    int ret;
    int fd = -1;
    int opt;
    int simId;
    pthread_attr_t attr;
    pthread_condattr_t condAttr;
    char prop[PROPERTY_VALUE_MAX];
    s_rilEnv = env;
    void *dlHandle = NULL;
    const RIL_TheadsFunctions *(*rilThreadsInit)(const RIL_RequestFunctions *, int, int);

    while (-1 != (opt = getopt(argc, argv, "m:n:"))) {
        switch (opt) {
            case 'm':
                s_modem = optarg;
                break;
            case 'n':
                s_sim_num = atoi(optarg);
                break;
            default:
                usage(argv[0]);
                break;
        }
    }
    if (s_modem == NULL) {
        s_modem = (char *)malloc(PROPERTY_VALUE_MAX);
        property_get("ro.radio.modemtype", (char *)s_modem, "");
        if (strcmp(s_modem, "") == 0) {
            RLOGD("get s_modem failed.");
            free((char *)s_modem);
            usage(argv[0]);
        }
    }

    signal(SIGPIPE, SIG_IGN);

    char mtbfProp[PROPERTY_VALUE_MAX];
    property_get(BUILD_TYPE_PROP, prop, "user");
    property_get(MTBF_ENABLE_PROP, mtbfProp, "0");
    if (strstr(prop, "userdebug") || strcmp(mtbfProp, "1") == 0) {
        s_isUserdebug = true;
    }

    s_isLTE = isLte();

    dlHandle = dlopen("libril_threads.so", RTLD_NOW);
    if (dlHandle == NULL) {
        RLOGE("dlopen failed: %s", dlerror());
        exit(EXIT_FAILURE);
    }
    rilThreadsInit = (const RIL_TheadsFunctions *(*)(const RIL_RequestFunctions *, int, int))
            dlsym(dlHandle, "requestThreadsInit");
    if (rilThreadsInit == NULL) {
        RLOGE("requestThreadsInit not defined or exported in libril_threads.so");
        exit(EXIT_FAILURE);
    }
    s_threadsFunctions = rilThreadsInit(&s_requestFunctions, SIM_COUNT, MAX_THR);

    pthread_condattr_init(&condAttr);
    pthread_condattr_setclock(&condAttr, CLOCK_MONOTONIC);
    for (simId = 0; simId < SIM_COUNT; simId++) {
        s_isSimPresent[simId] = SIM_UNKNOWN;
        pthread_cond_init(&s_simBusy[simId].s_sim_busy_cond, &condAttr);
    }

    RLOGD("rild connect %s modem, SIM_COUNT: %d", s_modem, SIM_COUNT);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    at_set_on_reader_closed(onATReaderClosed);
    //at_set_on_timeout(onATTimeout);

    detectATNoResponse();
    ps_service_init();

    for (simId = 0; simId < SIM_COUNT; simId++) {
        sem_init(&(s_sem[simId]), 0, 1);
        ret = pthread_create(&s_mainLoopTid[simId], &attr, mainLoop,
                (void *)&s_socketId[simId]);
        sem_wait(&(s_sem[simId]));
    }

    pthread_t tid;
    ret = pthread_create(&tid, &attr, startAsyncCmdHandlerLoop, NULL);
    ret = pthread_create(&tid, &attr, signal_process, NULL);
    if (ret < 0) {
        RLOGE("Failed to create signal_process");
    }

    setHwVerPorp();
    initOperatorInfoList(&s_operatorInfoList);
    return &s_callbacks;
}

const RIL_RadioFunctions *RIL_SAP_Init(const struct RIL_Env *env,
                                       int argc, char **argv) {
    RLOGD("RIL_SAP_Init");
    RIL_UNUSED_PARM(argc);
    RIL_UNUSED_PARM(argv);

    s_rilSapEnv = env;
    return &s_sapCallbacks;
}
#else  /* RIL_SHLIB */
int main(int argc, char **argv) {
    int ret;
    int fd = -1;
    int opt;
    int port = -1;
    int deviceSocket = 0;
    const char *devicePath = NULL;
    while (-1 != (opt = getopt(argc, argv, "p:d:"))) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                if (port == 0) {
                    usage(argv[0]);
                }
                RLOGI("Opening loopback port %d\n", port);
                break;

            case 'd':
                devicePath = optarg;
                RLOGI("Opening tty device %s\n", devicePath);
                break;

            case 's':
                devicePath   = optarg;
                deviceSocket = 1;
                RLOGI("Opening socket %s\n", devicePath);
                break;

            default:
                usage(argv[0]);
        }
    }

    if (port < 0 && devicePath == NULL) {
        usage(argv[0]);
    }

    RIL_register(&s_callbacks);

    mainLoop(NULL);

    return 0;
}

#endif  /* RIL_SHLIB */


/** returns 1 if on, 0 if off, and -1 on error */
int isRadioOn(int channelID) {
    ATResponse *p_response = NULL;
    int err;
    char *line;
    int ret;

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+CFUN?",
                                     "+CFUN:", &p_response);

    if (err < 0 || p_response->success == 0) {
        /* assume radio is off */
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &ret);
    if (err < 0) goto error;

    at_response_free(p_response);

    if (ret == 0 || ret == 2) {
        return 0;
    } else if (ret == 1) {
        return 1;
    } else {
        return -1;
    }

error:
    at_response_free(p_response);
    return -1;
}

bool isLte(void) {
    if (s_modem != NULL) {
        if (!strcmp(s_modem, "l") || !strcmp(s_modem, "tl") ||
            !strcmp(s_modem, "lf")) {
            return true;
        }
    }
    return false;
}

bool isVoLteEnable() {
    char prop[PROPERTY_VALUE_MAX];
    property_get(VOLTE_ENABLE_PROP, prop, "0");
    RLOGE("isVoLteEnable = %s", prop);
    if (strcmp(prop, "1") == 0 || strcmp(prop, "true") == 0) {
        return true;
    } else {
        return false;
    }
}
