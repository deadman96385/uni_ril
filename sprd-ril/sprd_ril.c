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
#include "hardware/qemu_pipe.h"

#include "sprd_ril.h"
#include "ril_sim.h"
#include "ril_network.h"
#include "ril_ss.h"
#include "ril_sms.h"
#include "ril_stk.h"
#include "ril_misc.h"
#include "ril_call.h"
#include "ril_data.h"
#include "ril_async_cmd_handler.h"

#define VT_DCI "\"000001B000000001B5090000010000000120008440FA282C2090A21F\""
#define VOLTE_ENABLE_PROP "persist.sys.volte.enable"
/* For special instrument's test */
#define VOLTE_PCSCF_PROP  "persist.sys.volte.pcscf"

enum ChannelState {
    CHANNEL_IDLE,
    CHANNEL_BUSY,
};

struct ChannelInfo {
    int channelID;
    int fd;
    char name[ARRAY_SIZE];
    char ttyName[ARRAY_SIZE];
    enum ChannelState state;
    pthread_mutex_t mutex;
};

struct ChannelInfo s_channelInfo[MAX_AT_CHANNELS];
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

sem_t s_sem;
bool s_isLTE = false;
const char *s_modem = NULL;
const struct RIL_Env *s_rilEnv;
const struct timeval TIMEVAL_CALLSTATEPOLL = {0, 500000};

static int s_port = -1;
static int s_deviceSocket = 0;
static const char *s_devicePath = NULL;
/* trigger change to this with s_radioStateCond */
static int s_closed[SIM_COUNT];
static int s_channelOpen[SIM_COUNT];
static const struct timeval TIMEVAL_SIMPOLL = {1, 0};
static const struct timeval TIMEVAL_0 = {0, 0};

#if defined (ANDROID_MULTI_SIM)
static void onRequest(int request, void *data, size_t datalen,
                       RIL_Token t, RIL_SOCKET_ID socket_id);
RIL_RadioState currentState(RIL_SOCKET_ID socket_id);
#else
static void onRequest(int request, void *data, size_t datalen, RIL_Token t);
RIL_RadioState currentState();
#endif

static int onSupports(int requestCode);
static void onCancel(RIL_Token t);
static const char *getVersion();
static void onSIMReady(int channelID);

/*** Static Variables ***/
static const RIL_RadioFunctions s_callbacks = {
    RIL_VERSION,
    onRequest,
    currentState,
    onSupports,
    onCancel,
    getVersion
};

void *noopRemoveWarning(void *a) {
    return a;
}

void list_init(ListNode *node) {
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

/* release Channel */
void putChannel(int channelID) {
    if (channelID < 1 || channelID >= MAX_AT_CHANNELS) {
        return;
    }

    struct ChannelInfo *p_channelInfo = s_channelInfo;

    pthread_mutex_lock(&p_channelInfo[channelID].mutex);
    if (p_channelInfo[channelID].state != CHANNEL_BUSY) {
        goto done;
    }
    p_channelInfo[channelID].state = CHANNEL_IDLE;

done:
    RLOGD("put Channel%d", p_channelInfo[channelID].channelID);
    pthread_mutex_unlock(&p_channelInfo[channelID].mutex);
}

/* Return channel ID */
int getChannel(RIL_SOCKET_ID socket_id) {
    int ret = 0;
    int channelID;
    int firstChannel, lastChannel;
    struct ChannelInfo *p_channelInfo = s_channelInfo;

#if defined (ANDROID_MULTI_SIM)
    firstChannel = socket_id * AT_CHANNEL_OFFSET + AT_CHANNEL_1;
    lastChannel = (socket_id + 1) * AT_CHANNEL_OFFSET;
#else
    firstChannel = AT_CHANNEL_1;
    lastChannel = MAX_AT_CHANNELS;
#endif

    for (;;) {
        if (!s_channelOpen[socket_id]) {
            sleep(1);
            continue;
        }
        for (channelID = firstChannel; channelID < lastChannel; channelID++) {
            pthread_mutex_lock(&p_channelInfo[channelID].mutex);
            if (p_channelInfo[channelID].state == CHANNEL_IDLE) {
                p_channelInfo[channelID].state = CHANNEL_BUSY;
                RLOGD("get Channel%d", p_channelInfo[channelID].channelID);
                pthread_mutex_unlock(&p_channelInfo[channelID].mutex);
                return channelID;
            }
            pthread_mutex_unlock(&p_channelInfo[channelID].mutex);
        }
        usleep(5000);
    }

    return -1;  // should never be here
}

RIL_SOCKET_ID getSocketIdByChannelID(int channelID) {
    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
#if (SIM_COUNT >= 2)
    if (channelID >= AT_URC2 && channelID <= AT_CHANNEL2_2) {
        socket_id = RIL_SOCKET_2;
    }
#if (SIM_COUNT >= 3)
    else if (channelID >= AT_URC3 && channelID <= AT_CHANNEL3_2) {
        socket_id = RIL_SOCKET_3;
    }
#if (SIM_COUNT >= 4)
    else if (channelID >= AT_URC4 && channelID <= AT_CHANNEL4_2) {
        socket_id = RIL_SOCKET_4;
    }
#endif
#endif
#endif
    return socket_id;
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
    int err;
    int channelID = -1;
    RIL_SOCKET_ID soc_id = RIL_SOCKET_1;

#if defined(ANDROID_MULTI_SIM)
    soc_id = socket_id;
#endif

    RIL_RadioState radioState = s_radioState[soc_id];

    RLOGD("onRequest: %s radioState=%d", requestToString(request), radioState);

    /**
     * Ignore all requests except !(requests)
     * when RADIO_STATE_UNAVAILABLE.
     */
    if (radioState == RADIO_STATE_UNAVAILABLE &&
        !(request == RIL_REQUEST_GET_SIM_STATUS ||
          request == RIL_REQUEST_GET_IMEI ||
          request == RIL_REQUEST_GET_IMEISV ||
          request == RIL_REQUEST_OEM_HOOK_STRINGS ||
          request == RIL_REQUEST_SIM_CLOSE_CHANNEL ||
          request == RIL_REQUEST_GET_RADIO_CAPABILITY ||
          request == RIL_REQUEST_SET_RADIO_CAPABILITY ||
          request == RIL_REQUEST_SIM_TRANSMIT_APDU_CHANNEL ||
          request == RIL_REQUEST_SHUTDOWN ||
          request == RIL_REQUEST_GET_IMS_BEARER_STATE ||
          request == RIL_EXT_REQUEST_GET_HD_VOICE_STATE)) {
        RIL_onRequestComplete(t, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
        return;
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
                 request == RIL_REQUEST_SCREEN_STATE ||
                 request == RIL_REQUEST_DELETE_SMS_ON_SIM ||
                 request == RIL_REQUEST_GET_IMSI ||
                 request == RIL_REQUEST_QUERY_FACILITY_LOCK ||
                 request == RIL_REQUEST_SET_FACILITY_LOCK ||
                 request == RIL_REQUEST_OEM_HOOK_STRINGS ||
                 request == RIL_REQUEST_SIM_OPEN_CHANNEL ||
                 request == RIL_REQUEST_SET_INITIAL_ATTACH_APN ||
                 request == RIL_REQUEST_SET_INITIAL_ATTACH_APN ||
                 request == RIL_REQUEST_ALLOW_DATA ||
                 request == RIL_REQUEST_GET_RADIO_CAPABILITY ||
                 request == RIL_REQUEST_SET_RADIO_CAPABILITY ||
                 request == RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE ||
                 request == RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE ||
                 request == RIL_REQUEST_SHUTDOWN ||
                 request == RIL_REQUEST_SIM_AUTHENTICATION ||
                 /* IMS Request @{ */
                 request == RIL_REQUEST_GET_IMS_CURRENT_CALLS ||
                 request == RIL_REQUEST_SET_IMS_VOICE_CALL_AVAILABILITY ||
                 request == RIL_REQUEST_GET_IMS_VOICE_CALL_AVAILABILITY ||
                 request == RIL_REQUEST_INIT_ISIM ||
                 request == RIL_REQUEST_SET_IMS_SMSC ||
                 request == RIL_REQUEST_SET_IMS_INITIAL_ATTACH_APN ||
                 request == RIL_REQUEST_GET_IMS_BEARER_STATE ||
              /* }@ */
                 request == RIL_EXT_REQUEST_GET_HD_VOICE_STATE)) {
        RIL_onRequestComplete(t, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
        return;
    }

    channelID = getChannel(soc_id);
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

/**
 * SIM ready means any commands that access the SIM will work, including:
 *  AT+CPIN, AT+CSMS, AT+CNMI, AT+CRSM
 *  (all SMS-related commands)
 */
static void pollSIMState(void *param) {
    CallbackPara *pollSimStatePara = (CallbackPara *)param;
    RIL_SOCKET_ID socket_id = pollSimStatePara->socket_id;

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

    switch (getSIMStatus(channelID)) {
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
            onSIMReady(channelID);
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
    }
    pollSIMState(cbPara);
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
    }

    /* set some auto report AT command on or off */
    if (isVoLteEnable()) {
        at_send_command(s_ATChannels[channelID],
            "AT+SPAURC=\"100110111110000000001000010000111111110001000111\"",
            NULL);
    } else {
        at_send_command(s_ATChannels[channelID],
            "AT+SPAURC=\"100110111110000000001000010000111111110001000100\"",
            NULL);
    }
    /* @} */

    /* for non-CMCC version @{ */
    at_send_command_singleline(s_ATChannels[channelID],
            "at+spcapability=32,1,0", "+SPCAPABILITY:", NULL);
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
    sem_post(&s_sem);
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
    struct ChannelInfo *p_channelInfo = s_channelInfo;
    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);

#if defined (ANDROID_MULTI_SIM)
    firstChannel = socket_id * AT_CHANNEL_OFFSET;
    lastChannel = (socket_id + 1) * AT_CHANNEL_OFFSET;
#else
    firstChannel = AT_URC;
    lastChannel = MAX_AT_CHANNELS;
#endif

    for (;;) {
        fd = -1;
        s_closed[socket_id] = 0;
        init_channels(socket_id);

 again:
        for (channelID = firstChannel; channelID < lastChannel; channelID++) {
            p_channelInfo[channelID].fd = -1;
            p_channelInfo[channelID].state = CHANNEL_IDLE;

            snprintf(p_channelInfo[channelID].ttyName,
                    sizeof(p_channelInfo[channelID].ttyName), "/dev/CHNPTY%d",
                    channelID);

            /* open TTY device, and attach it to channel */
            p_channelInfo[channelID].channelID = channelID;
            snprintf(p_channelInfo[channelID].name,
                    sizeof(p_channelInfo[channelID].name), "Channel%d",
                    channelID);

            fd = open(p_channelInfo[channelID].ttyName, O_RDWR | O_NONBLOCK);

            if (fd >= 0) {
                /* disable echo on serial ports */
                struct termios ios;
                tcgetattr(fd, &ios);
                ios.c_lflag = 0;  /* disable ECHO, ICANON, etc... */
                tcsetattr(fd, TCSANOW, &ios);
                p_channelInfo[channelID].fd = fd;
                RLOGI("AT channel [%d] open successfully, ttyName:%s",
                        channelID, p_channelInfo[channelID].ttyName);
            } else {
                RLOGE("Opening AT interface. retrying...");
                sleep(1);
                goto again;
            }
            s_ATChannels[channelID] = at_open(fd, channelID,
                    p_channelInfo[channelID].name, onUnsolicited);

            if (s_ATChannels[channelID] == NULL) {
                RLOGE("AT error on at_open\n");
                return 0;
            }
            s_ATChannels[channelID]->nolog = 0;
        }

        s_channelOpen[socket_id] = 1;

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

#ifdef RIL_SHLIB

int s_sim_num;
pthread_t s_mainLoopTid[SIM_COUNT];

const RIL_RadioFunctions *RIL_Init(const struct RIL_Env *env,
        int argc, char **argv) {
    int ret;
    int fd = -1;
    int opt;
    pthread_attr_t attr;

    s_rilEnv = env;

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
    s_isLTE = isLte();
    RLOGD("rild connect %s modem, SIM_COUNT: %d\n", s_modem, SIM_COUNT);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    sem_init(&s_sem, 0, 1);

    at_set_on_reader_closed(onATReaderClosed);
    at_set_on_timeout(onATTimeout);

    int simId;
    for (simId = 0; simId < SIM_COUNT; simId++) {
        ret = pthread_create(&s_mainLoopTid[simId], &attr, mainLoop,
                (void *)&s_socketId[simId]);
    }

    pthread_t tid;
    ret = pthread_create(&tid, &attr, startAsyncCmdHandlerLoop, NULL);

    sem_wait(&s_sem);

    return &s_callbacks;
}
#else  /* RIL_SHLIB */
int main(int argc, char **argv) {
    int ret;
    int fd = -1;
    int opt;

    while (-1 != (opt = getopt(argc, argv, "p:d:"))) {
        switch (opt) {
            case 'p':
                s_port = atoi(optarg);
                if (s_port == 0) {
                    usage(argv[0]);
                }
                RLOGI("Opening loopback port %d\n", s_port);
                break;

            case 'd':
                s_devicePath = optarg;
                RLOGI("Opening tty device %s\n", s_devicePath);
                break;

            case 's':
                s_devicePath   = optarg;
                s_deviceSocket = 1;
                RLOGI("Opening socket %s\n", s_devicePath);
                break;

            default:
                usage(argv[0]);
        }
    }

    if (s_port < 0 && s_devicePath == NULL) {
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

void getProperty(RIL_SOCKET_ID socket_id, const char *property, char *value,
                   const char *defaultVal) {
    int simId = 0;
    char prop[PROPERTY_VALUE_MAX];
    int len = property_get(property, prop, "");
    char *p[RIL_SOCKET_NUM];
    char *buf = prop;
    char *ptr = NULL;
    RLOGD("get [%s] property: %s", property, prop);

    if (value == NULL) {
        RLOGE("The memory to save prop is NULL!");
        return;
    }

    memset(p, 0, RIL_SOCKET_NUM * sizeof(char *));
    if (len > 0) {
        for (simId = 0; simId < RIL_SOCKET_NUM; simId++) {
            ptr = strsep(&buf, ",");
            p[simId] = ptr;
        }

        if (socket_id >= RIL_SOCKET_1 && socket_id < RIL_SOCKET_NUM &&
                (p[socket_id] != NULL)) {
            memcpy(value, p[socket_id], strlen(p[socket_id]) + 1);
            return;
        }
    }

    if (defaultVal != NULL) {
        len = strlen(defaultVal);
        memcpy(value, defaultVal, len);
        value[len] = '\0';
    }
}

void setProperty(RIL_SOCKET_ID socket_id, const char *property,
                   const char *value) {
    char prop[PROPERTY_VALUE_MAX];
    char propVal[PROPERTY_VALUE_MAX];
    int len = property_get(property, prop, "");
    int i, simId = 0;
    char *p[RIL_SOCKET_NUM];
    char *buf = prop;
    char *ptr = NULL;

    if (socket_id < RIL_SOCKET_1 || socket_id >= RIL_SOCKET_NUM) {
        RLOGE("setProperty: invalid socket id = %d, property = %s",
                socket_id, property);
        return;
    }

    memset(p, 0, RIL_SOCKET_NUM * sizeof(char *));
    if (len > 0) {
        for (simId = 0; simId < RIL_SOCKET_NUM; simId++) {
            ptr = strsep(&buf, ",");
            p[simId] = ptr;
        }
    }

    memset(propVal, 0, sizeof(propVal));
    for (i = 0; i < (int)socket_id; i++) {
        if (p[i] != NULL) {
            strncat(propVal, p[i], strlen(p[i]));
        }
        strncat(propVal, ",", 1);
    }

    if (value != NULL) {
        strncat(propVal, value, strlen(value));
    }

    for (i = socket_id + 1; i < RIL_SOCKET_NUM; i++) {
        strncat(propVal, ",", 1);
        if (p[i] != NULL) {
            strncat(propVal, p[i], strlen(p[i]));
        }
    }

    property_set(property, propVal);
}
