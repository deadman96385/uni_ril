
#define LOG_TAG "MBIM-Device"

#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/system_properties.h>
#include <stdbool.h>
#include <termios.h>
#include <utils/Log.h>
#include <time.h>
#include <semaphore.h>
#include <cutils/properties.h>

#include "misc.h"
#include "at_tok.h"
#include "at_channel.h"
#include "mbim_device_vendor.h"
#include "mbim_device_basic_connect.h"
#include "mbim_device_sms.h"
#include "mbim_device_oem.h"
#include "time_event_handler.h"
#include "mbim_device_sqlite.h"

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

sem_t s_sem[SIM_COUNT];
sem_t s_timeEventHandlerReadySem;
int s_isSimPresent[SIM_COUNT];
/* trigger change to this with s_radioStateCond */
static int s_closed[SIM_COUNT];
static int s_channelOpen[SIM_COUNT];
const struct timeval TIMEVAL_0 = {0, 0};
const struct timeval TIMEVAL_SIMPOLL = {1, 0};
const struct timeval TIMEVAL_CALLSTATEPOLL = {0, 500000};


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

    return -1;/*lint !e527*/
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

/**
 * SIM ready means any commands that access the SIM will work, including:
 *  AT+CPIN, AT+CSMS, AT+CNMI, AT+CRSM
 *  (all SMS-related commands)
 */
static void pollSIMState(void *param) {
    CallbackPara *pollSimStatePara = (CallbackPara *)param;
    RIL_SOCKET_ID socket_id = pollSimStatePara->socket_id;

    // TODO
    if (s_radioState[socket_id] != RADIO_STATE_UNAVAILABLE) {
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

#if 0
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
//            requestTimedCallback(pollSIMState, (void *)cbPara,
//                                     &TIMEVAL_SIMPOLL);  // TODO
            return;
        }
        case SIM_READY: {
            RLOGI("SIM_READY");
            onSIMReady(channelID);
            setRadioState(channelID, RADIO_STATE_SIM_READY);
//            RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED,
//                    NULL, 0, socket_id); TODO
            if (channelIDPassed < 0) {
                putChannel(channelID);
            }
            return;
        }
    }
#endif
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

    /* do these outside of the mutex */
    if (*p_radioState != oldState) {
#if 0
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED,
                                  NULL, 0, socket_id);
        // Sim state can change as result of radio state change
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED,
                                  NULL, 0, socket_id);
#endif

        if (*p_radioState == RADIO_STATE_ON) {
            onRadioPowerOn(channelID);
        }
    }
}

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
    //TODO
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
    at_send_command(s_ATChannels[channelID], "AT+SPDVTTYPE=1", NULL);
    at_send_command(s_ATChannels[channelID], "AT+SPVIDEOTYPE=3", NULL);
    at_send_command(s_ATChannels[channelID], "AT+SPDVTDCI="VT_DCI, NULL);
    at_send_command(s_ATChannels[channelID], "AT+SPDVTTEST=2,650", NULL);
    at_send_command(s_ATChannels[channelID], "AT+CEN=1", NULL);

#if 0
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
            if(strcmp(dsdsMode, "TL_LF_TD_W_G,TL_LF_TD_W_G") == 0) {
                at_send_command(s_ATChannels[channelID], "AT+SPCAPABILITY=49,1,2",
                                NULL);
            } else {
                at_send_command(s_ATChannels[channelID], "AT+SPCAPABILITY=49,1,1",
                                NULL);
            }
        }
    }
#endif
    /* set some auto report AT command on or off */
//    if (isVoLteEnable()) {
//        at_send_command(s_ATChannels[channelID],
//            "AT+SPAURC=\"100110111110000000001000010000111111110001000110\"",
//            NULL);
//    } else {
        at_send_command(s_ATChannels[channelID],
            "AT+SPAURC=\"100110111110000000001000010000111111110001000100\"",
            NULL);
//    }
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
        setRadioState(channelID, RADIO_STATE_ON);
    }

    putChannel(channelID);

//    list_init(&s_DTMFList[socket_id]);
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

void
mbim_device_command(Mbim_Token              token,
                    MbimService             service,
                    uint32_t                cid,
                    MbimMessageCommandType  commandType,
                    void *                  user_data,
                    size_t                  dataLen) {
    switch (service) {
        case MBIM_SERVICE_BASIC_CONNECT:
            mbim_device_basic_connect(token, cid, commandType, user_data, dataLen);
            break;
        case MBIM_SERVICE_SMS:
            mbim_device_sms(token, cid, commandType, user_data, dataLen);
            break;
        case MBIM_SERVICE_OEM:
            mbim_device_oem(token, cid, commandType, user_data, dataLen);
            break;
        default:
            RLOGE("unsupported service");
            break;
    }
}

/**
 * The host uses MBIM_CID_DEVICE_SERVICE_SUBSCRIBE_LIST CID to inform the
 * function of the CIDs for which the host wishes to receive unsolicited
 * events via MBIM_INDICATE_STATUS_MESSAGE. As a result, the function must
 * only indicate notifications for CIDs which have been enabled via this CID.
 *
 * If CidCount is 0, the function must enable unsolicited events for all CIDs
 * if defined for this device service.
 */
bool notification_subscribe_check(MbimService serviceId, uint32_t cid) {
    bool ret = false;

    if (s_deviceServiceSubscribeList == NULL) {
        ret = true;
        return ret;
    }

    for (uint32_t i = 0; i < s_deviceServiceSubscribeList->elementCount; i++) {
        MbimEventEntry_2 *eventEntries =
                s_deviceServiceSubscribeList->eventEntries[i];
        if (serviceId == eventEntries->deviceServiceId) {
            if (eventEntries->cidCount == 0) {
                ret = true;
                break;
            } else {
                for (uint32_t j = 0; j < eventEntries->cidCount; j++) {
                    if (cid == eventEntries->cidList[j]) {
                        ret = true;
                        break;
                    }
                }
            }
        }
    }

    return ret;
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

    if (!(processBasicConnectUnsolicited(s) || processSmsUnsolicited(s, sms_pdu))) {
        RLOGE("ignore unsupported unsolicited response : %s, ", s);
    }

}

static void *
mainLoop(void *param) {
    RLOGD("enter mainLoop");

    int fd;
    int channelID = 0;
    int firstChannel, lastChannel;
    char muxDevice[ARRAY_SIZE] = {0};
    char prop[PROPERTY_VALUE_MAX] = {0};
    struct ChannelInfo *p_channelInfo = s_channelInfo;
    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);

#if defined (ANDROID_MULTI_SIM)
    firstChannel = socket_id * AT_CHANNEL_OFFSET;
    lastChannel = (socket_id + 1) * AT_CHANNEL_OFFSET;
#else
    firstChannel = AT_URC;
    lastChannel = MAX_AT_CHANNELS;
#endif

    property_get(MODEM_TYPE_PROP, prop, "");
    snprintf(muxDevice, sizeof(muxDevice), "ro.modem.%s.tty", prop);
    if (s_modemType == MODEM_TYPE_T || s_modemType == MODEM_TYPE_W) {
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
            p_channelInfo[channelID].fd = -1;
            p_channelInfo[channelID].state = CHANNEL_IDLE;

            snprintf(p_channelInfo[channelID].ttyName,
                    sizeof(p_channelInfo[channelID].ttyName), "%s%d",
                    prop, channelID);

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
        requestTimedCallback(initializeCallback,
                (void *)&s_socketId[socket_id], &TIMEVAL_0);

        /* Give initializeCallback a chance to dispatched, since
         * we don't presently have a cancellation mechanism */
        sleep(1);

        waitForClose(socket_id);
        RLOGI("Re-opening after close");
    }

    return NULL; /*lint !e527 */
}


void initVaribales() {
    int simId = 0;

    s_modemType = getModemType();
    s_isLTE = isLte();
    s_modemConfig = getModemConfig();

    for (simId = 0; simId < SIM_COUNT; simId++) {
        s_workMode[simId] = getWorkMode((RIL_SOCKET_ID)simId);
    }
}

void
mbim_device_vendor_init(int        argc,
                        char**     argv) {
    MBIM_UNUSED_PARM(argc);
    MBIM_UNUSED_PARM(argv);

    int ret;
    int fd = -1;
    int opt;
    int simId;
    pthread_t tid;
    pthread_attr_t attr;

    RLOGD("mbim device vendor started, SIM_COUNT: %d", SIM_COUNT);

    mbim_sqlite_create();

    initVaribales();

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    sem_init(&s_timeEventHandlerReadySem, 0, 0);
    ret = pthread_create(&tid, &attr, startTimeEventHandlerLoop, NULL);
    sem_wait(&s_timeEventHandlerReadySem);

    at_set_on_reader_closed(onATReaderClosed);
    at_set_on_timeout(onATTimeout);

    detectATNoResponse();
//    ps_service_init();

    for (simId = 0; simId < SIM_COUNT; simId++) {
        sem_init(&(s_sem[simId]), 0, 1);
        ret = pthread_create(&tid, &attr, mainLoop, (void *)&s_socketId[simId]);
        sem_wait(&(s_sem[simId]));
    }

    listenAtciSocket();

    ret = pthread_create(&tid, &attr, signal_process, NULL);
    if (ret < 0) {
        RLOGE("Failed to create signal_process");
    }

//    setHwVerPorp();
}

void
mbim_device_apn_init(int        argc,
                     char**     argv) {
    MBIM_UNUSED_PARM(argc);
    MBIM_UNUSED_PARM(argv);

    int ret;
    pthread_t tid;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    RLOGD("start init_apn_process");
    ret = pthread_create(&tid, &attr, init_apn_process, NULL);
    if (ret < 0) {
        RLOGE("Failed to create init_apn_process");
    }
}
