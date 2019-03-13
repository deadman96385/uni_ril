/**
 * ril_call.c --- Call-related requests process functions implementation
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */
#define LOG_TAG "RIL"

#include "sprd_ril.h"
#include "ril_call.h"
#include "ril_misc.h"
#include "ril_network.h"
#include "ril_sim.h"

const struct timeval TIMEVAL_CSCALLSTATEPOLL = {0, 50000};
const struct timeval TIMEVAL_SRVCC_CALLSTATEPOLL = {0, 500000};
ListNode s_DTMFList[SIM_COUNT];
static SrvccPendingRequest *s_srvccPendingRequest[SIM_COUNT];
char *s_realEccList[SIM_COUNT];

static pthread_mutex_t s_listMutex[SIM_COUNT] = {
        PTHREAD_MUTEX_INITIALIZER
#if (SIM_COUNT >= 2)
        ,PTHREAD_MUTEX_INITIALIZER
#if (SIM_COUNT >= 3)
        ,PTHREAD_MUTEX_INITIALIZER
#if (SIM_COUNT >= 4)
        ,PTHREAD_MUTEX_INITIALIZER
#endif
#endif
#endif
};

pthread_mutex_t s_callMutex[SIM_COUNT] = {
        PTHREAD_MUTEX_INITIALIZER
#if (SIM_COUNT >= 2)
        ,PTHREAD_MUTEX_INITIALIZER
#if (SIM_COUNT >= 3)
        ,PTHREAD_MUTEX_INITIALIZER
#if (SIM_COUNT >= 4)
        ,PTHREAD_MUTEX_INITIALIZER
#endif
#endif
#endif
};

int s_callFailCause[SIM_COUNT] = {
        CALL_FAIL_ERROR_UNSPECIFIED
#if (SIM_COUNT >= 2)
        ,CALL_FAIL_ERROR_UNSPECIFIED
#if (SIM_COUNT >= 3)
        ,CALL_FAIL_ERROR_UNSPECIFIED
#if (SIM_COUNT >= 4)
        ,CALL_FAIL_ERROR_UNSPECIFIED
#endif
#endif
#endif
};

static int s_videoCallId[SIM_COUNT] = {
        -1
#if (SIM_COUNT >= 2)
        ,-1
#if (SIM_COUNT >= 3)
        ,-1
#if (SIM_COUNT >= 4)
        ,-1
#endif
#endif
#endif
};

void list_remove(RIL_SOCKET_ID socket_id, ListNode *item);

void onModemReset_Call() {
    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;

    for (socket_id = RIL_SOCKET_1; socket_id < RIL_SOCKET_NUM; socket_id++) {
        s_callFailCause[socket_id] = CALL_FAIL_ERROR_UNSPECIFIED;
        s_videoCallId[socket_id] = -1;
        s_maybeAddCall = 0;

        ListNode *pList = s_DTMFList[socket_id].next;
        ListNode *next = NULL;
        while (pList != &s_DTMFList[socket_id]) {
            next = pList->next;
            list_remove(socket_id, pList);
            free(pList);
            pList = next;
        }
        list_init(&s_DTMFList[socket_id]);

        if (s_srvccPendingRequest[socket_id] != NULL) {
            SrvccPendingRequest *request = NULL;
            do {
                request = s_srvccPendingRequest[socket_id];
                s_srvccPendingRequest[socket_id] = request->p_next;
                free(request->cmd);
                free(request);
            } while (s_srvccPendingRequest[socket_id] != NULL);
        }
    }
}

void list_init(ListNode *node) {
    node->next = node;
    node->prev = node;
}

void list_add_tail(RIL_SOCKET_ID socket_id, ListNode *head, ListNode *item) {
    pthread_mutex_lock(&s_listMutex[socket_id]);
    item->next = head;
    item->prev = head->prev;
    head->prev->next = item;
    head->prev = item;
    pthread_mutex_unlock(&s_listMutex[socket_id]);
}

void list_remove(RIL_SOCKET_ID socket_id, ListNode *item) {
    pthread_mutex_lock(&s_listMutex[socket_id]);
    item->next->prev = item->prev;
    item->prev->next = item->next;
    pthread_mutex_unlock(&s_listMutex[socket_id]);
}

void reportCallStateChanged(void *param) {
    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    if (s_imsRegistered[socket_id]) {
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_IMS_CALL_STATE_CHANGED,
                                  NULL, 0, socket_id);
    } else {
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL,
                                  0, socket_id);
    }
}

void process_calls(int _calls) {
    char buf[3];
    int incallRecordStatusFd = -1;
    int len = 0;
    static int calls = 0;
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    if (calls && _calls == 0) {
        pthread_mutex_lock(&lock);
        RLOGD("########## < vaudio > This is the Last PhoneCall ##########");
        /**
         * The Last PhoneCall is really terminated,
         * audio codec is freed by Modem side completely [luther.ge]
         */
        incallRecordStatusFd = open("/proc/vaudio/close", O_RDWR);
        if (incallRecordStatusFd >= 0) {
            memset(buf, 0, sizeof buf);
            len = read(incallRecordStatusFd, buf, 3);
            if (len > 0) {
                RLOGD("########## < vaudio > %sincall recording ##########[%s]",
                    buf[0] == '1' ? "" : "no ", buf);
                if (buf[0] == '1') {
                    /* incall recording */
                    len = write(incallRecordStatusFd, buf, 1);
                    RLOGD("write /proc/vaudio/close len = %d", len);
                }
            }
            close(incallRecordStatusFd);
        }
        pthread_mutex_unlock(&lock);
    }

    calls = _calls;
}

static int clccStateToRILState(int state, RIL_CallState *p_state) {
    switch (state) {
        case RIL_CALL_ACTIVE:
            *p_state = RIL_CALL_ACTIVE;
            return 0;
        case RIL_CALL_HOLDING:
            *p_state = RIL_CALL_HOLDING;
            return 0;
        case RIL_CALL_DIALING:
            *p_state = RIL_CALL_DIALING;
            return 0;
        case RIL_CALL_ALERTING:
            *p_state = RIL_CALL_ALERTING;
            return 0;
        case RIL_CALL_INCOMING:
            *p_state = RIL_CALL_INCOMING;
            return 0;
        case RIL_CALL_WAITING:
            *p_state = RIL_CALL_WAITING;
            return 0;
        default:
            return -1;
    }
}

static int voLTEStateToRILState(int state, RIL_CallState *p_state) {
    switch (state) {
        case VOLTE_CALL_IDEL:
        case VOLTE_CALL_RELEASED_MO:
        case VOLTE_CALL_RELEASED_MT:
        case VOLTE_CALL_USER_BUSY:
        case VOLTE_CALL_USER_DETERMINED_BUSY:
            return -1;
        case VOLTE_CALL_CALLING_MO:
            *p_state = RIL_CALL_DIALING;
            return 0;
        case VOLTE_CALL_CONNECTING_MO:
            *p_state = RIL_CALL_DIALING;
            return 0;
        case VOLTE_CALL_ALERTING_MO:
            *p_state = RIL_CALL_ALERTING;
            return 0;
        case VOLTE_CALL_ALERTING_MT:
            *p_state = RIL_CALL_INCOMING;
            return 0;
        case VOLTE_CALL_ACTIVE:
            *p_state = RIL_CALL_ACTIVE;
            return 0;
        case VOLTE_CALL_WAITING_MO:
            *p_state = RIL_CALL_DIALING;
            return 0;
        case VOLTE_CALL_WAITING_MT:
            *p_state = RIL_CALL_WAITING;
            return 0;
        case VOLTE_CALL_HOLD_MO:
            *p_state = RIL_CALL_HOLDING;
            return 0;
        case VOLTE_CALL_HOLD_MT:
            *p_state = RIL_CALL_HOLDING;
            return 0;
        default:
            return -1;
    }
}

/**
 * Note: directly modified line and has *p_call point directly into
 * modified line
 */
int callFromCLCCLine(char *line, RIL_Call *p_call) {
    // +CLCC: 1,0,2,0,0,\"+18005551212\",145
    //     index,isMT,state,mode,isMpty(,number,TOA)?
    int err;
    int state;
    int mode;
    int isMpty;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &(p_call->index));
    if (err < 0) goto error;

    err = at_tok_nextbool(&line, &(p_call->isMT));
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &state);
    if (err < 0) goto error;

    err = clccStateToRILState(state, &(p_call->state));
    if (p_call->state == RIL_CALL_HOLDING ||
        p_call->state == RIL_CALL_WAITING) {
        s_maybeAddCall = 1;
    }
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &mode);
    if (err < 0) goto error;
    p_call->isVoice = (mode == 0);

    err = at_tok_nextint(&line, &isMpty);
    if (err < 0) goto error;
    p_call->isMpty = isMpty;

    if (at_tok_hasmore(&line)) {
        err = at_tok_nextstr(&line, &(p_call->number));

        /* tolerate null here */
        if (err < 0) return 0;

        /* Some lame implementations return strings
        // like "NOT AVAILABLE" in the CLCC line
        if (p_call->number != NULL &&
            0 == strspn(p_call->number, "+0123456789*#abc")) {
            p_call->number = NULL;
        } */

        err = at_tok_nextint(&line, &p_call->toa);
        if (err < 0) goto error;

        if (at_tok_hasmore(&line)) {
            char *p = line;
            for (p = line; *p != '\0'; p++) {
                if (*p == ',') {
                    skipNextComma(&line);
                }
            }
            err = at_tok_nextint(&line, &p_call->numberPresentation);
            if (err < 0) goto error;
        }
    }

    p_call->uusInfo = NULL;
    return 0;

error:
    RLOGE("invalid CLCC line\n");
    return -1;
}

int callFromCLCCLineVoLTE(char *line, RIL_Call_VoLTE *p_call) {
    // +CLCC:index,isMT,state,mode,isMpty(,number,TOA)?

    /**
     * [+CLCCS: <ccid1>,<dir>,<neg_status_present>,<neg_status>,<SDP_md>,
     * <cs_mode>,<ccstatus>,<mpty>,[,<numbertype>,<ton>,<number>
     * [,<priority_present>,<priority>[,<CLI_validity_present>,<CLI_validity>]]]
     */
    int err;
    int state;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &(p_call->index));
    if (err < 0) goto error;

    err = at_tok_nextbool(&line, &(p_call->isMT));
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &(p_call->negStatusPresent));
    if (err < 0) {
        RLOGE("invalid CLCCS line:negStatusPresent\n");
        p_call->negStatusPresent = 0;
    }

    err = at_tok_nextint(&line, &(p_call->negStatus));
    if (err < 0) {
        RLOGE("invalid CLCCS line:negStatus\n");
        p_call->negStatus = 0;
    }

    err = at_tok_nextstr(&line, &(p_call->mediaDescription));
    if (err < 0) {
        RLOGE("invalid CLCCS line:mediaDescription\n");
        p_call->mediaDescription = " ";
    }

    err = at_tok_nextint(&line, &(p_call->csMode));
    if (err < 0) {
        RLOGE("invalid CLCCS line:mode\n");
        p_call->csMode = 0;
    }

    err = at_tok_nextint(&line, &state);
    if (err < 0) goto error;

    err = voLTEStateToRILState(state, &(p_call->state));
    if ((RIL_VoLTE_CallState)p_call->state == RIL_CALL_HOLDING ) {
        s_maybeAddCall = 1;
    }
    if (p_call->state == RIL_CALL_WAITING) {
        s_maybeAddCall = 1;
    }

    if (err < 0) goto error;

    err = at_tok_nextint(&line, &(p_call->mpty));
    if (err < 0) {
        RLOGE("invalid CLCCS line:mpty\n");
        p_call->mpty = 0;
    }

    err = at_tok_nextint(&line, &(p_call->numberType));
    if (err < 0) {
        RLOGE("invalid CLCCS line:numberType\n");
        p_call->numberType = 2;
    }

    err = at_tok_nextint(&line, &(p_call->toa));
    if (err < 0) {
        RLOGE("invalid CLCCS line:toa\n");
        p_call->toa = 128;
    }

    if (at_tok_hasmore(&line)) {
        err = at_tok_nextstr(&line, &(p_call->number));

        /* tolerate null here */
        if (err < 0) return 0;
        if (p_call->number != NULL &&
           (strstr(p_call->number, "anonymous@anonymous.invalid") != NULL)) {
            p_call->number = "";
        }
    }
    err = at_tok_nextint(&line, &(p_call->prioritypresent));
    if (err < 0) {
        RLOGE("invalid CLCCS line:prioritypresent\n");
        p_call->prioritypresent = 0;
    }

    err = at_tok_nextint(&line, &(p_call->priority));
    if (err < 0) {
        RLOGE("invalid CLCCS line: priority\n");
        p_call->priority = 0;
    }

    err = at_tok_nextint(&line, &(p_call->CliValidityPresent));
    if (err < 0) {
        RLOGE("invalid CLCCS line: CliValidityPresent\n");
        p_call->CliValidityPresent = 0;
    }

    err = at_tok_nextint(&line, &(p_call->numberPresentation));
    if (err < 0) {
        RLOGE("invalid CLCCS line: numberPresentation\n");
        p_call->numberPresentation = 0;
    }

    if (at_tok_hasmore(&line)) {
        int localHold = 0;
        err = at_tok_nextint(&line, &localHold);
        if (localHold) {
            p_call->state = RIL_CALL_HOLDING;
        }
        RLOGD("CLCCS->localHold:%d \n",localHold);
    }
    p_call->uusInfo = NULL;
    return 0;

error:
    RLOGE("invalid CLCCS line\n");
    return -1;
}

static inline void speaker_mute(void) {
    RLOGW(
          "\n\nThere will be no call, so mute speaker now to avoid noise pop "
          "sound\n\n");
    /* Remove handsfree pop noise sound [luther.ge] */
    system("alsa_amixer cset -c phone name=\"Speaker Playback Switch\" 0");
}

int all_calls(int channelID, int do_mute) {
    ATResponse *p_response = NULL;
    ATLine *p_cur;
    int countCalls;
    int err;

    err = at_send_command_multiline(s_ATChannels[channelID], "AT+CLCC",
                                    "+CLCC:", &p_response);
    if (err != 0 || p_response->success == 0) {
        at_response_free(p_response);
        return -1;
    }

    /* total the calls */
    for (countCalls = 0, p_cur = p_response->p_intermediates; p_cur != NULL;
         p_cur = p_cur->p_next) {
        countCalls++;
    }
    at_response_free(p_response);

    if (do_mute && countCalls == 1) {
        speaker_mute();
    }

    return countCalls;
}

static void requestGetCurrentCalls(int channelID, void *data,
        size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int i, err;
    int countCalls;
    int countValidCalls;
    int needRepoll = 0;
    ATResponse *p_response = NULL;
    ATLine *p_cur;
    RIL_Call *p_calls;
    RIL_Call **pp_calls;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    err = at_send_command_multiline(s_ATChannels[channelID], "AT+CLCC",
                                    "+CLCC:", &p_response);
    if (err != 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        at_response_free(p_response);
        return;
    }

    /* count the calls */
    for (countCalls = 0, p_cur = p_response->p_intermediates; p_cur != NULL;
         p_cur = p_cur->p_next) {
        countCalls++;
    }
    process_calls(countCalls);

    /* there's an array of pointers and then an array of structures */
    pp_calls = (RIL_Call **)alloca(countCalls * sizeof(RIL_Call *));
    p_calls = (RIL_Call *)alloca(countCalls * sizeof(RIL_Call));
    memset(p_calls, 0, countCalls * sizeof(RIL_Call));

    /* init the pointer array */
    for (i = 0; i < countCalls; i++) {
        pp_calls[i] = &(p_calls[i]);
    }
    s_maybeAddCall = 0;
    for (countValidCalls = 0, p_cur = p_response->p_intermediates;
         p_cur != NULL; p_cur = p_cur->p_next) {
        err = callFromCLCCLine(p_cur->line, p_calls + countValidCalls);
        if (err != 0) {
            continue;
        }
        countValidCalls++;
    }
    RIL_onRequestComplete(t, RIL_E_SUCCESS, pp_calls,
                          countValidCalls * sizeof(RIL_Call *));
    at_response_free(p_response);
#ifdef POLL_CALL_STATE
    if (countValidCalls)
    /* We don't seem to get a "NO CARRIER" message from
     * smd, so we're forced to poll until the call ends.
     */
#else
    if (needRepoll)
#endif
    {
        RIL_requestTimedCallback(reportCallStateChanged,
                (void *)&s_socketId[socket_id], &TIMEVAL_CALLSTATEPOLL);
    }
    return;
}

static void requestDial(int channelID, void *data, size_t datalen,
                           RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err;
    int ret;
    char *cmd = NULL;
    const char *clir = NULL;
    ATResponse *p_response = NULL;

    RIL_Dial *p_dial = NULL;
    p_dial = (RIL_Dial *)data;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
    if (s_isSimPresent[socket_id] != PRESENT) {
        RLOGE("card is absent");
        RIL_onRequestComplete(t, RIL_E_INVALID_STATE, NULL, 0);
        return;
    }
    switch (p_dial->clir) {
        case 0: clir = ""; break;   /* subscription default */
        case 1: clir = "I"; break;  /* invocation */
        case 2: clir = "i"; break;  /* suppression */
        default: break;
    }

    s_callFailCause[socket_id] = CALL_FAIL_ERROR_UNSPECIFIED;
    ret = asprintf(&cmd, "ATD%s%s;", p_dial->address, clir);
    if (ret < 0) {
        RLOGE("Failed to allocate memory");
        cmd = NULL;
        goto error;
    }


    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    free(cmd);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    /* failure is  not ignored by the upper layer here */
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);
    return;
error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static void requestHangup(int channelID, void *data, size_t datalen,
                             RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int *p_line;
    int ret;
    char cmd[AT_COMMAND_LEN] = {0};
    ATResponse *p_response = NULL;

    p_line = (int *)data;

    /* 3GPP 22.030 6.5.5
     * "Releases a specific active call X"
     */
    snprintf(cmd, sizeof(cmd), "AT+CHLD=7%d", p_line[0]);
    all_calls(channelID, 1);
    ret = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (ret < 0 || p_response->success == 0) {
        goto error;
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        at_response_free(p_response);
        return;
    }
error:
    if (p_response != NULL &&
            !strcmp(p_response->finalResponse, "+CME ERROR: 21")) {
        RIL_onRequestComplete(t, RIL_E_INVALID_STATE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
    at_response_free(p_response);
}

static void requestHangupWaitingOrBackground(int channelID,
        void *data, size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    /* 3GPP 22.030 6.5.5
     * "Releases all held calls or sets User Determined User Busy
     *  (UDUB) for a waiting call."
   */
    int err;
    ATResponse *p_response = NULL;

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
    if (s_isSimPresent[socket_id] != PRESENT) {
        RLOGE("card is absent");
        RIL_onRequestComplete(t, RIL_E_INVALID_STATE, NULL, 0);
        return;
    }

    err = at_send_command(s_ATChannels[channelID], "AT+CHLD=0", &p_response);
    if (err < 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    }
    at_response_free(p_response);
}

static void requestHangupForeResumeBack(int channelID,
        void *data, size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    /* 3GPP 22.030 6.5.5
     * "Releases all active calls (if any exist) and accepts
     *  the other (held or waiting) call."
     */
    int err;
    ATResponse *p_response = NULL;

    err = at_send_command(s_ATChannels[channelID], "AT+CHLD=1", &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        at_response_free(p_response);
        return;
    }
error:
    if (p_response != NULL &&
            !strcmp(p_response->finalResponse, "+CME ERROR: 3")) {
        RIL_onRequestComplete(t, RIL_E_INVALID_STATE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
    at_response_free(p_response);
}

static void requestSwitchWaitOrHoldAndActive(int channelID,
        void *data, size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    /* 3GPP 22.030 6.5.5
     * "Places all active calls (if any exist) on hold and accepts
     *  the other (held or waiting) call."
     */
    int err;
    ATResponse *p_response = NULL;

    err = at_send_command(s_ATChannels[channelID], "AT+CHLD=2", &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        at_response_free(p_response);
        return;
    }
error:
    if (p_response != NULL &&
            !strcmp(p_response->finalResponse, "+CME ERROR: 3")) {
        RIL_onRequestComplete(t, RIL_E_INVALID_STATE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
    at_response_free(p_response);
}

static void requestConference(int channelID, void *data, size_t datalen,
                                  RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    /* 3GPP 22.030 6.5.5
     * "Adds a held call to the conversation"
     */
    int err;
    ATResponse *p_response = NULL;

    err = at_send_command(s_ATChannels[channelID], "AT+CHLD=3", &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        at_response_free(p_response);
        return;

    }
error:
    if (p_response != NULL &&
            !strcmp(p_response->finalResponse, "+CME ERROR: 3")) {
        RIL_onRequestComplete(t, RIL_E_INVALID_STATE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
    at_response_free(p_response);
    /* success or failure is ignored by the upper layer here.
       it will call GET_CURRENT_CALLS and determine success that way */
}

static void requestUDUB(int channelID, void *data, size_t datalen,
                           RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    /* user determined user busy */
    /* sometimes used: ATH */
    int err;
    ATResponse *p_response = NULL;

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
    if (s_isSimPresent[socket_id] != PRESENT) {
        RLOGE("card is absent");
        RIL_onRequestComplete(t, RIL_E_INVALID_STATE, NULL, 0);
        return;
    }

    err = at_send_command(s_ATChannels[channelID], "AT+CHLD=0", &p_response);
    if (err < 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    }
    at_response_free(p_response);
    /* success or failure is ignored by the upper layer here.
       it will call GET_CURRENT_CALLS and determine success that way */
}

static void requestDTMF(int channelID, void *data, size_t datalen,
                           RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err;
    char character = ((char *)data)[0];
    char cmd[AT_COMMAND_LEN] = {0};
    ATResponse *p_response = NULL;

    snprintf(cmd, sizeof(cmd), "AT+VTS=%c", (int)character);
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        at_response_free(p_response);
        return;
    }
error:
    if (p_response != NULL &&
            !strcmp(p_response->finalResponse, "+CME ERROR: 22")) {
        RIL_onRequestComplete(t, RIL_E_INVALID_MODEM_STATE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
    at_response_free(p_response);
}

static void requestAnswer(int channelID, void *data, size_t datalen,
                             RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int err;
    ATResponse *p_response = NULL;

    err = at_send_command(s_ATChannels[channelID], "ATA", &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        at_response_free(p_response);
        return;
    }
error:
    if (p_response != NULL &&
            !strcmp(p_response->finalResponse, "+CME ERROR: 22")) {
        RIL_onRequestComplete(t, RIL_E_INVALID_STATE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
    at_response_free(p_response);
}

static void requestDTMFStart(int channelID, void *data, size_t datalen,
                                RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err;
    char character = ((char *)data)[0];
    char cmd[AT_COMMAND_LEN] = {0};
    ListNode *cmd_item = NULL;
    ATResponse *p_response = NULL;

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    cmd_item = (ListNode *)malloc(sizeof(ListNode));
    if (cmd_item == NULL) {
        RLOGE("Allocate dtmf cmd_item failed");
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        return;
    }
    cmd_item->data = ((char *)data)[0];
    list_add_tail(socket_id, &s_DTMFList[socket_id], cmd_item);

    snprintf(cmd, sizeof(cmd), "AT+SDTMF=1,\"%c\",0", (int)character);
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }
    AT_RESPONSE_FREE(p_response);
    snprintf(cmd, sizeof(cmd), "AT+EVTS=1,%c", (int)character);
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        at_response_free(p_response);
        return;
    }
error:
    if (p_response != NULL &&
            !strcmp(p_response->finalResponse, "+CME ERROR: 22")) {
        RIL_onRequestComplete(t, RIL_E_INVALID_MODEM_STATE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
    at_response_free(p_response);
}

static void requestDTMFStop(int channelID, void *data, size_t datalen,
                               RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int err;
    char cmd[AT_COMMAND_LEN] = {0};
    char character;
    ListNode *cmd_item = NULL;
    ATResponse *p_response = NULL;

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    cmd_item = (&s_DTMFList[socket_id])->next;
    if (cmd_item != (&s_DTMFList[socket_id])) {
        character = cmd_item->data;
        err = at_send_command(s_ATChannels[channelID], "AT+SDTMF=0", &p_response);
        if (err < 0 || p_response->success == 0) {
            goto error;
        }
        AT_RESPONSE_FREE(p_response);
        snprintf(cmd, sizeof(cmd), "AT+EVTS=0,%c", (int)character);
        err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
        if (err < 0 || p_response->success == 0) {
            goto error;
        } else {
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        }
        at_response_free(p_response);
        list_remove(socket_id, cmd_item);
        free(cmd_item);
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
    return;
error:
    if (p_response != NULL &&
            !strcmp(p_response->finalResponse, "+CME ERROR: 22")) {
        RIL_onRequestComplete(t, RIL_E_INVALID_MODEM_STATE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
    at_response_free(p_response);
    list_remove(socket_id, cmd_item);
    free(cmd_item);
}

static void requestSeparateConnection(int channelID, void *data,
                                           size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err;
    int party = ((int*)data)[0];
    char cmd[AT_COMMAND_LEN] = {0};
    ATResponse *p_response = NULL;

    /* Make sure that party is in a valid range.
     * (Note: The Telephony middle layer imposes a range of 1 to 7.
     * It's sufficient for us to just make sure it's single digit.)
     */
    if (party > 0 && party < 10) {
        snprintf(cmd, sizeof(cmd), "AT+CHLD=2%d", party);
        err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
        if (err < 0 || p_response->success == 0) {
            goto error;
        } else {
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        }
        at_response_free(p_response);
        return;
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        return;
    }
error:
    if (p_response != NULL &&
            !strcmp(p_response->finalResponse, "+CME ERROR: 3")) {
        RIL_onRequestComplete(t, RIL_E_INVALID_STATE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
    at_response_free(p_response);
}

static void requestExplicitCallTransfer(int channelID, void *data,
                                             size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int err;
    ATResponse *p_response = NULL;

    err = at_send_command(s_ATChannels[channelID], "AT+CHLD=4", &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        at_response_free(p_response);
        return;
    }
error:
    if (p_response != NULL &&
            !strcmp(p_response->finalResponse, "+CME ERROR: 3")) {
        RIL_onRequestComplete(t, RIL_E_INVALID_STATE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
    at_response_free(p_response);
}

int isEccNumber(RIL_SOCKET_ID socket_id, char *dialNumber, int *catgry) {
    char eccNumberList[PROPERTY_VALUE_MAX] = {0};
    char *realEccList = NULL;
    char *tmpList = NULL;
    char *tmpNumber = NULL;
    char *outer_ptr = NULL;
    char *inner_ptr = NULL;
    char ecc3GPP_NoSIM[] = "112,911,000,08,110,118,119,999";
    char ecc3GPP_SIM[] = "112,911";
    int numberExist = 0;
    int propValueSize = 0;

    if (s_realEccList[socket_id] != NULL) {
        propValueSize = strlen(s_realEccList[socket_id]) + 1;
        realEccList = (char *)calloc(propValueSize, sizeof(char));
        snprintf(realEccList, propValueSize, "%s", s_realEccList[socket_id]);
        tmpList = realEccList;
        while ((tmpNumber = strtok_r(tmpList, ",", &outer_ptr)) != NULL) {
            tmpList = tmpNumber;
            if ((tmpNumber = strtok_r(tmpList, "@", &inner_ptr)) != NULL) {
                if (strcmp(tmpNumber, dialNumber) == 0) {
                    numberExist = 1;
                    if (inner_ptr != NULL) {
                        *catgry = atoi(inner_ptr);
                    }
                    break;
                }
            }
            tmpList = NULL;
        }
        if (realEccList != NULL) {
            free(realEccList);
        }
        return numberExist;
    }

    //property_get("ro.ril.ecclist", eccNumberList, "");
    //tmpList = eccNumberList;
    //if (strcmp(eccNumberList, "") == 0) {
    if (isSimPresent(socket_id) == 1) {
        tmpList = ecc3GPP_SIM;
    } else {
        tmpList = ecc3GPP_NoSIM;
    }
    //}
    while ((tmpNumber = strtok_r(tmpList, ",", &outer_ptr)) != NULL) {
        if (strcmp(tmpNumber, dialNumber) == 0) {
            numberExist = 1;
            break;
        }
        tmpList = NULL;
    }
    return numberExist;
}

static void requestEccDial(int channelID, void *data, size_t datalen,
                              RIL_Token t, int catgry) {
    RIL_UNUSED_PARM(datalen);

    int ret, err;
    char *cmd = NULL;
    const char *clir = NULL;
    char *token = NULL;
    char *categoryFromJava = NULL;
    RIL_Dial *p_dial = (RIL_Dial *)data;

    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    switch (p_dial->clir) {
        case 0:  /* subscription default */
            clir = "";
            break;
        case 1:  /* invocation */
            clir = "I";
            break;
        case 2:  /* suppression */
            clir = "i";
            break;
        default:
            break;
    }

    s_callFailCause[socket_id] = CALL_FAIL_ERROR_UNSPECIFIED;
    if (catgry != -1) {
        ret = asprintf(&cmd, "ATD%s@%d,#%s;", p_dial->address, catgry, clir);
    } else {
        ret = asprintf(&cmd, "ATD%s@,#%s;", p_dial->address, clir);
    }

    if (ret < 0) {
        RLOGE("Failed to allocate memory");
        cmd = NULL;
        goto error;
    }

    err = at_send_command(s_ATChannels[channelID], cmd, NULL);
    free(cmd);
    if (err != 0) goto error;

    /* success or failure is ignored by the upper layer here.
       it will call GET_CURRENT_CALLS and determine success that way */
    if (t != NULL) {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    }
    return;

error:
    if (t != NULL) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
}

void requestLastCallFailCause(int channelID, void *data, size_t datalen,
                                  RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int response = CALL_FAIL_ERROR_UNSPECIFIED;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
    char vendorCause[32];
    RIL_LastCallFailCauseInfo *failCause = (RIL_LastCallFailCauseInfo *)
            calloc(1, sizeof(RIL_LastCallFailCauseInfo));

    pthread_mutex_lock(&s_callMutex[socket_id]);
    switch (s_callFailCause[socket_id]) {
        case 1:
        case 28:
            response = CALL_FAIL_UNOBTAINABLE_NUMBER;
            break;
        case 0:
        case 3:
        case 16:
        case 301:
        case 999:
            response = CALL_FAIL_NORMAL;
            break;
        case 302:
            response = CALL_FAIL_IMEI_NOT_ACCEPTED;
            break;
        case 17:
        case 21:
        case 1486:
            response = CALL_FAIL_BUSY;
            break;
        case 34:
        case 38:
        case 41:
        case 42:
        case 44:
        case 47:
        case 1301:
        case 1400:
        case 1401:
        case 1402:
        case 1403:
        case 1404:
        case 1405:
        case 1406:
        case 1407:
        case 1408:
        case 1409:
        case 1410:
        case 1411:
        case 1412:
        case 1413:
        case 1414:
        case 1415:
        case 1416:
        case 1420:
        case 1421:
        case 1423:
        case 1480:
        case 1481:
        case 1482:
        case 1483:
        case 1484:
        case 1485:
        case 1487:
        case 1488:
        case 1500:
        case 1501:
        case 1502:
        case 1503:
        case 1504:
        case 1505:
        case 1513:
        case 1600:
        case 1603:
        case 1604:
        case 1606:
            response = CALL_FAIL_CONGESTION;
            break;
        case 68:
            response = CALL_FAIL_ACM_LIMIT_EXCEEDED;
            break;
        case 8:
            response = CALL_FAIL_CALL_BARRED;
            break;
        case 241:
            response = CALL_FAIL_FDN_BLOCKED;
            break;
        case 31: // NORMAL_UNSPECIFIED
        case 6: // CHANNEL_UNACCEPTABLE
        case 18: // CALL_FAIL_NO_USER_RESPONDING
        case 19: // CALL_FAIL_NO_ANSWER_FROM_USER
        case 22: // NUMBER_CHANGED
        case 25: // PREEMPTION
        case 27: // CALL_FAIL_DESTINATION_OUT_OF_ORDER
        case 29: // FACILITY_REJECTED
        case 30: // STATUS_ENQUIRY
        case 43: // ACCESS_INFORMATION_DISCARDED
        case 49: // QOS_NOT_AVAIL
        case 50: // REQUESTED_FACILITY_NOT_SUBSCRIBED
        case 55: // INCOMING_CALLS_BARRED_WITHIN_CUG
        case 57: // BEARER_CAPABILITY_NOT_AUTHORIZED
        case 58: // BEARER_NOT_AVAIL
        case 63: // SERVICE_OPTION_NOT_AVAILABLE
        case 65: // BEARER_SERVICE_NOT_IMPLEMENTED
        case 69: // REQUESTED_FACILITY_NOT_IMPLEMENTED
        case 70: // ONLY_DIGITAL_INFORMATION_BEARER_AVAILABLE
        case 79: // SERVICE_OR_OPTION_NOT_IMPLEMENTED
        case 81: // INVALID_TRANSACTION_IDENTIFIER
        case 87: // USER_NOT_MEMBER_OF_CUG
        case 88: // INCOMPATIBLE_DESTINATION
        case 91: // INVALID_TRANSIT_NW_SELECTION
        case 95: // SEMANTICALLY_INCORRECT_MESSAGE
        case 96: // INVALID_MANDATORY_INFORMATION
        case 97: // MESSAGE_TYPE_NON_IMPLEMENTED
        case 98: // MESSAGE_TYPE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE
        case 99: // INFORMATION_ELEMENT_NON_EXISTENT
        case 100: // CONDITIONAL_IE_ERROR
        case 101: // MESSAGE_NOT_COMPATIBLE_WITH_PROTOCOL_STATE
        case 102: // RECOVERY_ON_TIMER_EXPIRED
        case 111: // PROTOCOL_ERROR_UNSPECIFIED
        case 127: // INTERWORKING_UNSPECIFIED
            response = CALL_FAIL_NORMAL_UNSPECIFIED;
            break;
            /* Multi terminal,one answer the call, the other terminal does not display the missed call. */
        case 1024:
        case 13:
            response = CALL_FAIL_OEM_CAUSE_1;
            break;
            /* When it is registing vowifi,it dial call fial and redial call by vowifi */
        case 501:
            response = CALL_FAIL_OEM_CAUSE_2;
            break;
        default:
            response = CALL_FAIL_ERROR_UNSPECIFIED;
            break;
    }
    failCause->cause_code = response;
    snprintf(vendorCause, sizeof(vendorCause), "%d",
            s_callFailCause[socket_id]);
    failCause->vendor_cause = vendorCause;
    pthread_mutex_unlock(&s_callMutex[socket_id]);

    RIL_onRequestComplete(t, RIL_E_SUCCESS, failCause,
            sizeof(RIL_LastCallFailCauseInfo));
    free(failCause);
}

static void requestGetCurrentCallsVoLTE(int channelID, void *data,
        size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int err;
    int i;
    int needRepoll = 0;
    int countCalls;
    int countValidCalls;
    RIL_Call_VoLTE *p_calls;
    RIL_Call_VoLTE **pp_calls;
    ATResponse *p_response = NULL;
    ATLine *p_cur;

    err = at_send_command_multiline(s_ATChannels[channelID], "AT+CLCCS",
                                    "+CLCCS:", &p_response);
    if (err != 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        at_response_free(p_response);
        return;
    }

    /* count the calls */
    for (countCalls = 0, p_cur = p_response->p_intermediates; p_cur != NULL;
         p_cur = p_cur->p_next) {
        countCalls++;
    }

    process_calls(countCalls);

    /* yes, there's an array of pointers and then an array of structures */
    pp_calls = (RIL_Call_VoLTE **)alloca(countCalls * sizeof(RIL_Call_VoLTE *));
    p_calls = (RIL_Call_VoLTE *)alloca(countCalls * sizeof(RIL_Call_VoLTE));
    RIL_Call_VoLTE *p_t_calls =
            (RIL_Call_VoLTE *)alloca(countCalls * sizeof(RIL_Call_VoLTE));
    memset(p_calls, 0, countCalls * sizeof(RIL_Call_VoLTE));

    /* init the pointer array */
    for (i = 0; i < countCalls; i++) {
        pp_calls[i] = &(p_calls[i]);
    }

    s_maybeAddCall = 0;
    for (countValidCalls = 0, p_cur = p_response->p_intermediates;
         p_cur != NULL; p_cur = p_cur->p_next) {
        err = callFromCLCCLineVoLTE(p_cur->line, p_calls + countValidCalls);
        p_t_calls = p_calls + countValidCalls;

        if (err != 0) {
            continue;
        }

        countValidCalls++;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, pp_calls,
                          countValidCalls * sizeof(RIL_Call_VoLTE *));

    at_response_free(p_response);
#ifdef POLL_CALL_STATE
    if (countValidCalls) {
    /* We don't seem to get a "NO CARRIER" message from
     * smd, so we're forced to poll until the call ends.
     */
#else
    if (needRepoll) {
#endif
        RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);
        RIL_requestTimedCallback(reportCallStateChanged,
                (void *)&s_socketId[socket_id], &TIMEVAL_CALLSTATEPOLL);
    }
    return;
}

static void requestVideoPhoneDial(int channelID, void *data,
                                      size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    RIL_VideoPhone_Dial *p_dial;
    int err;
    char *cmd;
    int ret;

    p_dial = (RIL_VideoPhone_Dial *)data;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    s_callFailCause[socket_id] = CALL_FAIL_ERROR_UNSPECIFIED;
#ifdef NEW_AT
    ret = asprintf(&cmd, "ATD=%s", p_dial->address);
#else
    ret = asprintf(&cmd, "AT^DVTDIAL=\"%s\"", p_dial->address);
#endif
    if (ret < 0) {
        RLOGE("Failed to allocate memory");
        cmd = NULL;
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        return;
    }

    err = at_send_command(s_ATChannels[channelID], cmd, NULL);
    free(cmd);
    if (err != 0) goto error;
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    return;
error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

static void requestSwitchMultiCall(int channelID, void *data,
        size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err = -1;
    int countCalls;
    int countValidCalls;
    char cmd[AT_COMMAND_LEN] = {0};
    ATResponse *p_response = NULL;
    ATResponse *p_resp = NULL;
    ATLine *p_cur;
    RIL_Call *p_calls;

    int mode = ((int *)data)[0];
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    if (mode == 0) {
        goto process;
    }

    err = at_send_command_multiline(s_ATChannels[channelID], "AT+CLCC",
                                    "+CLCC:", &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    /* count the calls */
    for (countCalls = 0, p_cur = p_response->p_intermediates; p_cur != NULL;
         p_cur = p_cur->p_next) {
        countCalls++;
    }

    p_calls = (RIL_Call *)alloca(countCalls * sizeof(RIL_Call));
    memset(p_calls, 0, countCalls * sizeof(RIL_Call));

    /*
     * if mode is 1, hung up all active call;
     * if mode is 2, hang up all hold call
     * if mode is 3, hang up all active and hold call
     */
    for (countValidCalls = 0, p_cur = p_response->p_intermediates;
            p_cur != NULL; p_cur = p_cur->p_next) {
        err = callFromCLCCLine(p_cur->line, p_calls + countValidCalls);
        if (err != 0) {
            continue;
        }
        if ((p_calls + countValidCalls)->state == RIL_CALL_ACTIVE) {
            if (mode == 1 || mode == 3) {
                snprintf(cmd, sizeof(cmd), "AT+CHLD=7%d",
                        (p_calls + countValidCalls)->index);
                err = at_send_command(s_ATChannels[channelID], cmd, &p_resp);
                if (err < 0 || p_response->success == 0) {
                    goto error;
                }
                AT_RESPONSE_FREE(p_resp);
            }
        } else if ((p_calls + countValidCalls)->state == RIL_CALL_HOLDING) {
            if (mode == 2 || mode == 3) {
                snprintf(cmd, sizeof(cmd), "AT+CHLD=7%d",
                        (p_calls + countValidCalls)->index);
                err = at_send_command(s_ATChannels[channelID], cmd, &p_resp);
                if (err < 0 || p_response->success == 0) {
                    goto error;
                }
                AT_RESPONSE_FREE(p_resp);
            }
        }
        countValidCalls++;
    }

process:
    AT_RESPONSE_FREE(p_response);
    err = at_send_command(s_ATChannels[channelID], "AT+CHLD=2", &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    AT_RESPONSE_FREE(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    AT_RESPONSE_FREE(p_response);
    AT_RESPONSE_FREE(p_resp);
    return;
}

static void requestUpdateImsNetworkInfo(int channelID, void *data,
                                        size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    IMS_NetworkInfo *p_info;
    ATResponse   *p_response = NULL;
    int err;
    char *cmd;
    int ret;

    p_info = (IMS_NetworkInfo *)data;

    ret = asprintf(&cmd, "AT+IMSHOWFINF=%d,\"%s\"", p_info->type, p_info->info);
    if (ret < 0) {
        RLOGE("Failed to allocate memory");
        cmd = NULL;
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        return;
    }

    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    free(cmd);
    if (err < 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    }
    at_response_free(p_response);
}

static void requestImsCallRequestMediaChange(int channelID, void *data, size_t datalen,
        RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err = -1;
    ATResponse *p_response = NULL;
    char cmd[AT_COMMAND_LEN] = {0};
    int callId = ((int *)data)[0];
    int mediaRequest = ((int *)data)[1];

    switch (mediaRequest) {
        case MEDIA_REQUEST_AUDIO_UPGRADE_VIDEO_TX:
            snprintf(cmd, sizeof(cmd), "AT+CCMMD=%d,2,\"m=video/a=sendonly\"", callId);
            break;
        case MEDIA_REQUEST_AUDIO_UPGRADE_VIDEO_RX:
            snprintf(cmd, sizeof(cmd), "AT+CCMMD=%d,2,\"m=video/a=recvonly\"", callId);
            break;
        case MEDIA_REQUEST_AUDIO_UPGRADE_VIDEO_BIDIRECTIONAL:
        case MEDIA_REQUEST_VIDEO_TX_UPGRADE_VIDEO_BIDIRECTIONAL:
        case MEDIA_REQUEST_VIDEO_RX_UPGRADE_VIDEO_BIDIRECTIONAL:
            snprintf(cmd, sizeof(cmd), "AT+CCMMD=%d,2,\"m=video\"", callId);
            break;

        case MEDIA_REQUEST_VIDEO_BIDIRECTIONAL_DOWNGRADE_VIDEO_TX:
            snprintf(cmd, sizeof(cmd), "AT+CCMMD=%d,1,\"m=video/a=sendonly\"", callId);
            break;
        case MEDIA_REQUEST_VIDEO_BIDIRECTIONAL_DOWNGRADE_VIDEO_RX:
            snprintf(cmd, sizeof(cmd), "AT+CCMMD=%d,1,\"m=video/a=recvonly\"", callId);
            break;
        case MEDIA_REQUEST_VIDEO_TX_DOWNGRADE_AUDIO:
        case MEDIA_REQUEST_VIDEO_RX_DOWNGRADE_AUDIO:
        case MEDIA_REQUEST_VIDEO_BIDIRECTIONAL_DOWNGRADE_AUDIO:
            snprintf(cmd, sizeof(cmd), "AT+CCMMD=%d,1,\"m=audio\"", callId);
            break;
        default:
            goto error;
            break;
    }

    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static void requestImsCallResponseMediaChange(int channelID, void *data, size_t datalen,
        RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err = -1;
    ATResponse *p_response = NULL;
    char cmd[AT_COMMAND_LEN] = {0};
    int callId = ((int *)data)[0];
    int isAccept = ((int *)data)[1];
    int videoCallMediaDirection = ((int *)data)[2];
    if (isAccept) {
        switch (videoCallMediaDirection) {
            case VIDEO_CALL_MEDIA_DESCRIPTION_SENDRECV:
                snprintf(cmd, sizeof(cmd), "AT+CCMMD=%d,3,\"m=video\"", callId);
                break;
            case VIDEO_CALL_MEDIA_DESCRIPTION_SENDONLY:
                snprintf(cmd, sizeof(cmd), "AT+CCMMD=%d,3,\"m=video/a=sendonly\"", callId);
                break;
            case VIDEO_CALL_MEDIA_DESCRIPTION_RECVONLY:
                snprintf(cmd, sizeof(cmd), "AT+CCMMD=%d,3,\"m=video/a=recvonly\"", callId);
                break;
            default:
                goto error;
                break;
        }
    } else {
        switch (videoCallMediaDirection) {
            case VIDEO_CALL_MEDIA_DESCRIPTION_SENDRECV:
                snprintf(cmd, sizeof(cmd), "AT+CCMMD=%d,4,\"m=video\"", callId);
                break;
            case VIDEO_CALL_MEDIA_DESCRIPTION_SENDONLY:
                snprintf(cmd, sizeof(cmd), "AT+CCMMD=%d,4,\"m=video/a=sendonly\"", callId);
                break;
            case VIDEO_CALL_MEDIA_DESCRIPTION_RECVONLY:
                snprintf(cmd, sizeof(cmd), "AT+CCMMD=%d,4,\"m=video/a=recvonly\"", callId);
                break;
            default:
                goto error;
                break;
        }
    }

    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

static void requestGetImsPaniInfo(int channelID, void *data,
                                  size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int err = -1;
    char *line = NULL;
    ATResponse *p_response = NULL;
    IMS_NetworkInfo *pResp =
            (IMS_NetworkInfo *)calloc(1, sizeof(IMS_NetworkInfo));

    err = at_send_command_singleline(s_ATChannels[channelID],
            "AT+IMSHONWINF?", "+IMSHONWINF:", &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &pResp->type);
    if (err < 0) goto error;

    err = at_tok_nextstr(&line, &pResp->info);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &pResp->age);
    if (err < 0) goto error;

    RIL_onRequestComplete(t, RIL_E_SUCCESS, pResp, sizeof(IMS_NetworkInfo));
    at_response_free(p_response);
    free(pResp);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
    free(pResp);
}

int processCallRequest(int request, void *data, size_t datalen, RIL_Token t,
                          int channelID) {
    int ret = 1;
    int err;
    ATResponse *p_response = NULL;

    switch (request) {
        case RIL_REQUEST_GET_CURRENT_CALLS:
            requestGetCurrentCalls(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_DIAL: {
            RIL_Dial *p_dial = (RIL_Dial *)data;
            int ret = 0, category = -1;

            RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

            ret = isEccNumber(socket_id, p_dial->address, &category);
            RLOGD("requestDial isEccNumber = %d", ret);
            if (ret == 1) {
                requestEccDial(channelID, data, datalen, t, category);
            } else {
                requestDial(channelID, data, datalen, t);
            }
            break;
        }
        case RIL_REQUEST_HANGUP:
            requestHangup(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND:
            requestHangupWaitingOrBackground(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND:
            requestHangupForeResumeBack(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE:
            requestSwitchWaitOrHoldAndActive(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_CONFERENCE:
            requestConference(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_UDUB:
            requestUDUB(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_LAST_CALL_FAIL_CAUSE:
            requestLastCallFailCause(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_DTMF:
            requestDTMF(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_ANSWER:
            requestAnswer(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_DTMF_START:
            requestDTMFStart(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_DTMF_STOP:
            requestDTMFStop(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_SEPARATE_CONNECTION:
            requestSeparateConnection(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_EXPLICIT_CALL_TRANSFER:
        case RIL_EXT_REQUEST_EXPLICIT_CALL_TRANSFER:
            requestExplicitCallTransfer(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_SET_TTY_MODE:
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;
           /* "response" is int *
            * ((int *)response)[0] is == 0 for TTY off
            * ((int *)response)[0] is == 1 for TTY Full
            * ((int *)response)[0] is == 2 for TTY HCO (hearing carryover)
            * ((int *)response)[0] is == 3 for TTY VCO (voice carryover)
            */
        case RIL_REQUEST_QUERY_TTY_MODE: {
            int response = 0;
            RIL_onRequestComplete(t, RIL_E_SUCCESS, &response,
                                  sizeof(response));
            break;
        }
        case RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE:
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;
        /* IMS request @{ */
        case RIL_REQUEST_GET_IMS_CURRENT_CALLS:
            requestGetCurrentCallsVoLTE(channelID, data, datalen, t);
            break;
        /*
         * add for VoLTE to handle Voice call Availability
         * AT+CAVIMS=<state>
         * state: integer type.The UEs IMS voice call availability status
         * 0, Voice calls with the IMS are not available.
         * 1, Voice calls with the IMS are available.
         */
        case RIL_REQUEST_SET_IMS_VOICE_CALL_AVAILABILITY: {
            char cmd[AT_COMMAND_LEN] = {0};
            int state = ((int *)data)[0];

            /* add for Bug 558197 @{ */
            int lastState = -1;
            err = at_send_command_singleline(s_ATChannels[channelID],
                    "AT+CAVIMS?", "+CAVIMS:", &p_response);
            if (err >= 0 && p_response->success) {
                char *line = p_response->p_intermediates->line;
                err = at_tok_start(&line);
                if (err >= 0) {
                    err = at_tok_nextint(&line, &lastState);
                }
            }
            AT_RESPONSE_FREE(p_response);
            if (lastState == state) {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
                break;
            }
            /* }@ */

            snprintf(cmd, sizeof(cmd), "AT+CAVIMS=%d", state);
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_REQUEST_GET_IMS_VOICE_CALL_AVAILABILITY: {
            p_response = NULL;
            int state = 0;

            err = at_send_command_singleline(s_ATChannels[channelID],
                    "AT+CAVIMS?", "+CAVIMS:", &p_response);
            if (err >= 0 && p_response->success) {
                char *line = p_response->p_intermediates->line;
                err = at_tok_start(&line);
                if (err >= 0) {
                    err = at_tok_nextint(&line, &state);
                    RIL_onRequestComplete(t, RIL_E_SUCCESS, &state,
                            sizeof(state));
                } else {
                    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
                }
            } else {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_REQUEST_IMS_CALL_REQUEST_MEDIA_CHANGE: {
            requestImsCallRequestMediaChange(channelID, data, datalen, t);
            break;
        }
        case RIL_REQUEST_IMS_CALL_RESPONSE_MEDIA_CHANGE: {
            requestImsCallResponseMediaChange(channelID, data, datalen, t);
            break;
        }
        case RIL_REQUEST_IMS_CALL_FALL_BACK_TO_VOICE: {
            char cmd[AT_COMMAND_LEN] = {0};
            int callId = ((int *)data)[0];
            p_response = NULL;
            snprintf(cmd, sizeof(cmd), "AT+CCMMD=%d,1,\"m=audio\"", callId);
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_REQUEST_IMS_INITIAL_GROUP_CALL: {
            char cmd[AT_COMMAND_LEN] = {0};
            p_response = NULL;
            snprintf(cmd, sizeof(cmd), "AT+CGU=1,\"%s\"", (char *)data);
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_REQUEST_IMS_ADD_TO_GROUP_CALL: {
            char cmd[AT_COMMAND_LEN] = {0};
            p_response = NULL;
            snprintf(cmd, sizeof(cmd), "AT+CGU=4,\"%s\"", (char *)data);
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_EXT_REQUEST_VIDEOPHONE_DIAL:
            requestVideoPhoneDial(channelID, data, datalen, t);
            break;
        case RIL_EXT_REQUEST_VIDEOPHONE_CODEC: {
            p_response = NULL;
            char cmd[AT_COMMAND_LEN] = {0};

            RIL_VideoPhone_Codec* p_codec = (RIL_VideoPhone_Codec *)data;
            snprintf(cmd, sizeof(cmd), "AT+SPDVTCODEC=%d", p_codec->type);

            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        /* }@ */
        /* videophone @{ */
        case RIL_EXT_REQUEST_VIDEOPHONE_FALLBACK: {
            p_response = NULL;
            err = at_send_command(s_ATChannels[channelID],
                                  "AT"AT_PREFIX"DVTHUP", &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_EXT_REQUEST_VIDEOPHONE_STRING: {
            char *cmd;
            int ret;
            p_response = NULL;
            ret = asprintf(&cmd, "AT"AT_PREFIX"DVTSTRS=\"%s\"", (char *)(data));
            if (ret < 0) {
                RLOGE("Failed to allocate memory");
                cmd = NULL;
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
                break;
            }
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            free(cmd);
            break;
        }
        case RIL_EXT_REQUEST_VIDEOPHONE_LOCAL_MEDIA: {
            p_response = NULL;
            char cmd[AT_COMMAND_LEN] = {0};
            int datatype = ((int *)data)[0];
            int sw = ((int *)data)[1];

            if ((datalen / sizeof(int)) > 2) {
                int indication = ((int *)data)[2];
                snprintf(cmd, sizeof(cmd), "AT"AT_PREFIX"DVTSEND=%d,%d,%d",
                          datatype, sw, indication);
            } else {
                snprintf(cmd, sizeof(cmd), "AT"AT_PREFIX"DVTSEND=%d,%d",
                          datatype, sw);
            }
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_EXT_REQUEST_VIDEOPHONE_CONTROL_IFRAME: {
            p_response = NULL;
            char cmd[AT_COMMAND_LEN];
            snprintf(cmd, sizeof(cmd), "AT"AT_PREFIX"DVTLFRAME=%d,%d",
                      ((int *)data)[0], ((int *)data)[1]);
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        /* }@ */
        case RIL_EXT_REQUEST_SWITCH_MULTI_CALL:
            /* Multi call supplementary function decoupling */
            requestSwitchMultiCall(channelID, data, datalen, t);
            break;
        case RIL_EXT_REQUEST_GET_HD_VOICE_STATE: {
            p_response = NULL;
            int response = 0;

            err = at_send_command_singleline(s_ATChannels[channelID],
               "AT+SPCAPABILITY=10,0", "+SPCAPABILITY", &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                 char *line = p_response->p_intermediates->line;
                 skipNextComma(&line);
                 skipNextComma(&line);
                 err = at_tok_nextint(&line, &response);
                 if (err >= 0) {
                     RIL_onRequestComplete(t, RIL_E_SUCCESS, &response,
                                           sizeof(response));
                 } else {
                     RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
                 }
            }
            at_response_free(p_response);
            break;
         }
        case RIL_EXT_REQUEST_UPDATE_ECCLIST: {
            /* add for bug608793 */
            RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

            if (s_realEccList[socket_id] != NULL) {
                free(s_realEccList[socket_id]);
            }

            s_realEccList[socket_id] = (char *)calloc(datalen, sizeof(char));
            snprintf(s_realEccList[socket_id], datalen, "%s", (char *)(data));
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            break;
        }
        case RIL_EXT_REQUEST_SET_VOICE_DOMAIN: {
            int type = ((int *)data)[0];
            char cmd[AT_COMMAND_LEN] = {0};
            p_response = NULL;

            snprintf(cmd, sizeof(cmd), "AT+CEVDP=%d", type);
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        /* SPRD: add for VoWifi @{ */
        case RIL_REQUEST_IMS_HANDOVER: {
            int type = ((int *)data)[0];
            char cmd[AT_COMMAND_LEN] = {0};
            p_response = NULL;

            snprintf(cmd, sizeof(cmd), "AT+IMSHO=%d", type);
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_REQUEST_IMS_HANDOVER_STATUS_UPDATE: {
            int type = ((int *)data)[0];
            char cmd[AT_COMMAND_LEN] = {0};

            if (type == IMS_HANDOVER_REGISTER_FAIL ||
                type == IMS_HANDOVER_SUCCESS) {
                snprintf(cmd, sizeof(cmd), "AT+VOWFREG=%d", type);
            } else if (type == IMS_HANDOVER_ATTACH_FAIL ||
                       type == IMS_HANDOVER_ATTACH_SUCCESS) {
                if (type == IMS_HANDOVER_ATTACH_FAIL) {
                    type = 0;  // fail
                } else {
                    type = 1;  // success
                }
                snprintf(cmd, sizeof(cmd), "AT+IMSWFATT=%d", type);
            } else {
                snprintf(cmd, sizeof(cmd), "AT+IMSHORST=%d", type);
            }
            err = at_send_command(s_ATChannels[channelID], cmd, NULL);
            if (err < 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            break;
        }
        case RIL_REQUEST_IMS_NETWORK_INFO_CHANGE: {
            requestUpdateImsNetworkInfo(channelID, data, datalen, t);
            break;
        }
        case RIL_REQUEST_IMS_HANDOVER_CALL_END: {
            int type = ((int *)data)[0];
            char cmd[AT_COMMAND_LEN] = {0};

            snprintf(cmd, sizeof(cmd), "AT+IMSHOCALLEND=%d", type);
            err = at_send_command(s_ATChannels[channelID], cmd, NULL);
            if (err < 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            break;
        }
        case RIL_REQUEST_IMS_UPDATE_DATA_ROUTER: {
            err = at_send_command(s_ATChannels[channelID],
                                  "AT+IMSHODATAROUTER=1", NULL);
            if (err < 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            break;
        }
        case RIL_REQUEST_IMS_WIFI_ENABLE: {
            int type = ((int *)data)[0];
            char cmd[AT_COMMAND_LEN] = {0};

            snprintf(cmd, sizeof(cmd), "AT+VOWIFIEN=%d", type);
            err = at_send_command(s_ATChannels[channelID], cmd, NULL);
            if (err < 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            break;
        }
        case RIL_REQUEST_IMS_WIFI_CALL_STATE_CHANGE: {
            int state = ((int *)data)[0];
            char cmd[AT_COMMAND_LEN] = {0};

            snprintf(cmd, sizeof(cmd), "AT+SPCPFS=1,%d", state);
            err = at_send_command(s_ATChannels[channelID], cmd, NULL);
            if (err < 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            break;
        }
        case RIL_EXT_REQUEST_GET_TPMR_STATE: {
            p_response = NULL;
            int response = 0;

            err = at_send_command_singleline(s_ATChannels[channelID],
                    "AT+SPTPMR?", "+SPTPMR:", &p_response);
            if (err >= 0 && p_response->success) {
                char *line = p_response->p_intermediates->line;
                err = at_tok_start(&line);
                if (err >= 0) {
                    err = at_tok_nextint(&line, &response);
                    if (err >= 0) {
                        RIL_onRequestComplete(t, RIL_E_SUCCESS, &response,
                                sizeof(response));
                        at_response_free(p_response);
                        break;
                    }
                }
            }

            RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            at_response_free(p_response);
            break;
        }
        case RIL_EXT_REQUEST_SET_TPMR_STATE: {
            char cmd[AT_COMMAND_LEN];

            snprintf(cmd, sizeof(cmd), "AT+SPTPMR=%d", ((int *)data)[0]);
            err = at_send_command(s_ATChannels[channelID], cmd, NULL);
            if (err < 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            break;
        }
        case RIL_EXT_REQUEST_SET_VIDEO_RESOLUTION:{
            char cmd[AT_COMMAND_LEN] = {0};
            p_response = NULL;

            snprintf(cmd, sizeof(cmd), "AT+CDEFMP=1,\"%d\"", ((int *)data)[0]);
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_EXT_REQUEST_ENABLE_LOCAL_HOLD: {
            int enable = ((int *)data)[0];
            p_response = NULL;

            if (enable) {
                err = at_send_command(s_ATChannels[channelID], "AT+SPLOCALHOLD=1;", &p_response);
            } else {
                err = at_send_command(s_ATChannels[channelID], "AT+SPLOCALHOLD=0;", &p_response);
            }
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_EXT_REQUEST_ENABLE_WIFI_PARAM_REPORT :{
            int enable = ((int *)data)[0];
            p_response = NULL;

            if (enable) {
                err = at_send_command(s_ATChannels[channelID], "AT+WIFIPARAM=1,0,0,0,5", &p_response);
            } else {
                err = at_send_command(s_ATChannels[channelID], "AT+WIFIPARAM=0,0,0,0,0", &p_response);
            }
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_EXT_REQUEST_CALL_MEDIA_CHANGE_REQUEST_TIMEOUT: {
            char cmd[AT_COMMAND_LEN] = {0};
            p_response = NULL;

            snprintf(cmd, sizeof(cmd), "AT+CCMMD=%d,5", ((int *)data)[0]);
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_EXT_REQUEST_SET_DUAL_VOLTE_STATE: {
            char cmd[AT_COMMAND_LEN] = {0};
            p_response = NULL;

            snprintf(cmd, sizeof(cmd), "AT+SPCAPABILITY=49,1,%d", ((int *)data)[0]);
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_EXT_REQUEST_SET_LOCAL_TONE: {
            p_response = NULL;
            int mode = ((int*)data)[0];
            char cmd[AT_COMMAND_LEN] = {0};

            snprintf(cmd, sizeof(cmd), "AT+SPCLSTONE=%d", mode);
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_REQUEST_IMS_NOTIFY_HANDOVER_CALL_INFO: {
            int err;
            char cmd[AT_COMMAND_LEN * 4] = {0};
            const char *strings = (const char *)data;
            ATResponse *p_response = NULL;

            if (datalen > 0 && strings != NULL && strlen(strings) > 0) {
                snprintf(cmd, sizeof(cmd), "AT+VOWIFCALLINF=%s", strings);
                err = at_send_command(s_ATChannels[channelID], cmd , NULL);
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            }
            break;
        }
        case RIL_REQUEST_GET_IMS_SRVCC_CAPBILITY:{
            p_response = NULL;
            int response = 0;

            err = at_send_command_singleline(s_ATChannels[channelID],
                    "AT+CISRVCC?", "+CISRVCC:", &p_response);
            if (err >= 0 && p_response->success) {
                char *line = p_response->p_intermediates->line;
                err = at_tok_start(&line);
                if (err == 0) {
                    err = at_tok_nextint(&line, &response);
                }
                if (err >= 0) {
                    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response,
                            sizeof(response));
                } else {
                    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
                }
            } else {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        /* @} */
        case RIL_REQUEST_IMS_HOLD_SINGLE_CALL: {
            p_response = NULL;
            int id = ((int *)data)[0];
            int state = ((int *)data)[1];
            char cmd[AT_COMMAND_LEN] = {0};

            if (state) {
                state = 1;  // hold
            } else {
                state = 2;  // resume
            }
            snprintf(cmd, sizeof(cmd), "AT+SPCHLD=%d,%d", state, id);
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);

            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }

            at_response_free(p_response);
            break;
        }
        case RIL_REQUEST_IMS_MUTE_SINGLE_CALL: {
            p_response = NULL;
            int id = ((int *)data)[0];
            int state = ((int *)data)[1];
            char cmd[AT_COMMAND_LEN] = {0};

            if (state) {
                state = 3;  // mute
            } else {
                state = 4;  // not mute
            }
            snprintf(cmd, sizeof(cmd), "AT+SPCHLD=%d,%d", state, id);
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);

            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }

            at_response_free(p_response);
            break;
        }
        case RIL_REQUEST_IMS_SILENCE_SINGLE_CALL: {
            p_response = NULL;
            int id = ((int *)data)[0];
            int state = ((int *)data)[1];
            char cmd[AT_COMMAND_LEN] = {0};

            if (state) {
                state = 5;  // silence
            } else {
                state = 6;  // not silence
            }
            snprintf(cmd, sizeof(cmd), "AT+SPCHLD=%d,%d", state, id);
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);

            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }

            at_response_free(p_response);
            break;
        }
        case RIL_REQUEST_IMS_ENABLE_LOCAL_CONFERENCE: {
            p_response = NULL;
            int enable = ((int *)data)[0];
            char cmd[AT_COMMAND_LEN] = {0};

            snprintf(cmd, sizeof(cmd), "AT+MIXVOICE=%d", enable);
            err = at_send_command(s_ATChannels[channelID], cmd, &p_response);

            if (err < 0 || p_response->success == 0) {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_EXT_REQUEST_GET_IMS_PANI_INFO:
            requestGetImsPaniInfo(channelID, data, datalen, t);
            break;
        default:
            ret = 0;
            break;
    }
    return ret;
}

static void dialEmergencyWhileCallFailed(void *param) {
    if (param != NULL) {
        char eccNumber[ARRAY_SIZE] = {0};
        char *line = NULL, *mcc = NULL;
        int channelID, err, category = -1;
        ATResponse *p_response = NULL;
        CallbackPara *cbPara = (CallbackPara *)param;
        if ((int)cbPara->socket_id < 0 || (int)cbPara->socket_id >= SIM_COUNT) {
            RLOGE("Invalid socket_id %d", cbPara->socket_id);
            FREEMEMORY(cbPara->para);
            FREEMEMORY(cbPara);
            return;
        }

        RIL_Dial *p_dial = (RIL_Dial *)calloc(1, sizeof(RIL_Dial));

        p_dial->address = cbPara->para;
        p_dial->clir = 0;
        p_dial->uusInfo = NULL;
        channelID = getChannel(cbPara->socket_id);

        isEccNumber(cbPara->socket_id, p_dial->address, &category);
        RLOGD("dialEmergencyWhileCallFailed->address = %s, category = %d",
              (char *)cbPara->para, category);
        err = at_send_command_multiline(s_ATChannels[channelID], "AT+CEN?",
                                       "+CEN", &p_response);
        /* AT+CEN? Return:
         * +CEN1:<reporting >,<mcc>,<mnc>
         * ...
         */
        if (err < 0 || p_response->success == 0) {
            RLOGE("query  network eccList fail!");
            goto done;
        }

        line = p_response->p_intermediates->line;
        err = at_tok_start(&line);
        if (err < 0) goto done;
        skipNextComma(&line);
        err = at_tok_nextstr(&line, &mcc);
        if (err < 0) {
            RLOGE("%s get mcc fail", p_response->p_intermediates->line);
            goto done;
        }
        snprintf(eccNumber, sizeof(eccNumber), "%s,%s@%d", mcc, p_dial->address, category);
        RIL_onUnsolicitedResponse(RIL_EXT_UNSOL_ECC_NETWORKLIST_CHANGED,
                eccNumber, strlen(eccNumber) + 1, cbPara->socket_id);

        requestEccDial(channelID, p_dial, sizeof(*p_dial), NULL, category);
done:
        putChannel(channelID);
        at_response_free(p_response);
        free(p_dial->address);
        free(p_dial);
        free(cbPara);
    }
}

static void redialWhileCallFailed(void *param) {
    RLOGD("redialWhileCallFailed");

    if (param != NULL) {
        CallbackPara *cbPara = (CallbackPara *)param;
        char *number = cbPara->para;
        if ((int)cbPara->socket_id < 0 || (int)cbPara->socket_id >= SIM_COUNT) {
            RLOGE("Invalid socket_id %d", cbPara->socket_id);
            FREEMEMORY(cbPara->para);
            FREEMEMORY(cbPara);
            return;
        }

        RIL_Dial *p_dial = (RIL_Dial *)calloc(1, sizeof(RIL_Dial));

        p_dial->address = number;
        p_dial->clir = 0;
        p_dial->uusInfo = NULL;

        int channelID = getChannel(cbPara->socket_id);
        requestDial(channelID, p_dial, sizeof(*p_dial), NULL);
        putChannel(channelID);


        free(p_dial->address);
        free(p_dial);
        free(cbPara);
    }
}

static void excuteSrvccPendingOperate(void *param) {
    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    if ((int)socket_id < 0 || (int)socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        return;
    }
    if (s_srvccPendingRequest[socket_id] != NULL) {
        SrvccPendingRequest *request;
        ATResponse *p_response = NULL;
        int err;
        int channelID;
        do {
            request = s_srvccPendingRequest[socket_id];
            channelID = getChannel(socket_id);
            RLOGD("excuteSrvccPendingOperate cmd = %s", request->cmd);
            err = at_send_command(s_ATChannels[channelID], request->cmd,
                                  &p_response);
            if (err < 0 || p_response->success == 0) {
                RLOGD("excuteSrvccPendingOperate fail!");
            }
            AT_RESPONSE_FREE(p_response);
            putChannel(channelID);
            s_srvccPendingRequest[socket_id] = request->p_next;

            free(request->cmd);
            free(request);
        } while (s_srvccPendingRequest[socket_id] != NULL);
    }
}

void sendCallStateChanged(void *param) {
    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    if (s_imsRegistered[socket_id]) {
        RIL_onUnsolicitedResponse(
            RIL_UNSOL_RESPONSE_IMS_CALL_STATE_CHANGED,
            NULL, 0, socket_id);
    } else {
        RIL_onUnsolicitedResponse(
            RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED,
            NULL, 0, socket_id);
    }
}

void sendCSCallStateChanged(void *param) {
    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0,
                              socket_id);
}

void sendIMSCallStateChanged(void *param) {
    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_IMS_CALL_STATE_CHANGED, NULL,
                              0, socket_id);
}

void queryEccNetworkList(void *param) {
    char *mcc = NULL, *number = NULL, *line = NULL;
    char eccNetList[2 * ARRAY_SIZE] = {0};
    int category;
    int channelID;
    int err;
    ATResponse *p_response = NULL;
    ATLine *p_cur = NULL;

    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    if ((int)socket_id < 0 || (int)socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        return;
    }

    channelID = getChannel(socket_id);
    err = at_send_command_multiline(s_ATChannels[channelID], "AT+CEN?", "+CEN",
                                    &p_response);
    /* AT+CEN? Return:
     * +CEN1:<reporting >,<mcc>,<mnc>
     * +CEN2:<cat>,<number>
     * +CEN2:<cat>,<number>
     * ...
     */
    if (err < 0 || p_response->success == 0) {
        RLOGE("queryEccNetworkList fail!");
        goto done;
    }

    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0) goto done;
    skipNextComma(&line);
    err = at_tok_nextstr(&line, &mcc);
    if (err < 0) {
        RLOGE("%s get mcc fail", p_response->p_intermediates->line);
        goto done;
    }
    snprintf(eccNetList, sizeof(eccNetList), "%s", mcc);

    for (p_cur = p_response->p_intermediates->p_next; p_cur != NULL;
         p_cur = p_cur->p_next) {
        line = p_cur->line;

        err = at_tok_start(&line);
        if (err < 0) goto done;

        err = at_tok_nextint(&line, &category);
        if (err < 0) {
            RLOGE("%s get cat fail", p_cur->line);
            goto done;
        }

        err = at_tok_nextstr(&line, &number);
        if (err < 0) {
            RLOGE("%s get number fail", p_cur->line);
            goto done;
        }

        snprintf(eccNetList, sizeof(eccNetList), "%s,%s@%d", eccNetList,
                 number, category);
        RLOGD("queryEccNetworkList category:%d, number:%s, eccNetList:%s",
               category, number, eccNetList);
    }
    if (strlen(eccNetList) > 0) {
        RIL_onUnsolicitedResponse(RIL_EXT_UNSOL_ECC_NETWORKLIST_CHANGED,
                eccNetList, strlen(eccNetList) + 1, socket_id);
    } else {
        RIL_onUnsolicitedResponse(RIL_EXT_UNSOL_ECC_NETWORKLIST_CHANGED,
                                  NULL, 0, socket_id);
    }

done:
    at_response_free(p_response);
    putChannel(channelID);
}

static void onDowngradeToVoice(void *param) {
    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    if ((int)socket_id < 0 || (int)socket_id >= SIM_COUNT) {
        RLOGE("Invalid socket_id %d", socket_id);
        return;
    }
    if (s_videoCallId[socket_id] == -1) {
        RLOGE("onDowngradeToVoice cancel id: %d", s_videoCallId[socket_id]);
        return;
    }

    int err = 0;
    char cmd[AT_COMMAND_LEN] = {0};
    int channelID = getChannel(socket_id);
    ATResponse *p_response = NULL;

    snprintf(cmd, sizeof(cmd), "AT+CCMMD=%d,1,\"m=audio\"",
              s_videoCallId[socket_id]);
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (err < 0 || p_response->success == 0) {
        RLOGE("onDowngradeToVoice failure!");
    } else {
        RLOGE("onDowngradeToVoice->id: %d", s_videoCallId[socket_id]);
    }

    at_response_free(p_response);
    putChannel(channelID);
}

void onCallCSFallBackAccept(void *param) {
    int channelID;
    int index;
    char cmd[AT_COMMAND_LEN] = {0};
    CallbackPara *cbPara = (CallbackPara *)param;

    if ((int)cbPara->socket_id < 0 || (int)cbPara->socket_id >= SIM_COUNT) {
        RLOGE("onCallCSFallBackAccept,Invalid socket_id %d", cbPara->socket_id);
        FREEMEMORY(cbPara->para);
        FREEMEMORY(cbPara);
        return;
    }

    index = *((int *)(cbPara->para));
    channelID = getChannel(cbPara->socket_id);
    FREEMEMORY(cbPara->para);
    FREEMEMORY(cbPara);

    snprintf(cmd, sizeof(cmd), "AT+SCSFB=%d,1",index);
    at_send_command(s_ATChannels[channelID], cmd, NULL);
    putChannel(channelID);
}

void onSSCallCSFallBackAccept(void *param) {
    int channelID = -1;
    int index = 0;
    char cmd[AT_COMMAND_LEN] = {0};
    CallbackPara *cbPara = (CallbackPara *)param;

    if ((int)cbPara->socket_id < 0 || (int)cbPara->socket_id >= SIM_COUNT) {
        RLOGE("onSSCallCSFallBackAccept, Invalid socket_id %d", cbPara->socket_id);
        FREEMEMORY(cbPara->para);
        FREEMEMORY(cbPara);
        return;
    }

    index = *((int *)(cbPara->para));
    channelID = getChannel(cbPara->socket_id);
    FREEMEMORY(cbPara->para);
    FREEMEMORY(cbPara);

    snprintf(cmd, sizeof(cmd), "AT+CSSSFB=%d,1", index);
    at_send_command(s_ATChannels[channelID], cmd, NULL);
    putChannel(channelID);
}

int processCallUnsolicited(RIL_SOCKET_ID socket_id, const char *s) {
    int err;
    int ret = 1;
    char *line = NULL;

    if (strStartsWith(s, "+CRING:") ||
        strStartsWith(s, "RING") ||
        strStartsWith(s, "NO CARRIER") ||
        strStartsWith(s, "+CCWA")) {
        if (!isVoLteEnable()) {
            RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED,
                                      NULL, 0, socket_id);
        }
    } else if (strStartsWith(s, "^DSCI:")) {
        RIL_VideoPhone_DSCI *response = NULL;
        response = (RIL_VideoPhone_DSCI *)alloca(sizeof(RIL_VideoPhone_DSCI));
        char *tmp;
        CallbackPara *cbPara;
        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);
        err = at_tok_nextint(&tmp, &response->id);
        if (err < 0) {
            RLOGE("get id fail");
            goto out;
        }
        err = at_tok_nextint(&tmp, &response->idr);
        if (err < 0) {
            RLOGE("get idr fail");
            goto out;
        }
        err = at_tok_nextint(&tmp, &response->stat);
        if (response->stat == RIL_CALL_HOLDING && s_maybeAddCall == 0) {
            s_maybeAddCall = 1;
        }
        if (err < 0) {
            RLOGE("get stat fail");
            goto out;
        }
        err = at_tok_nextint(&tmp, &response->type);
        if (err < 0) {
            RLOGE("get type fail");
            goto out;
        }

        // state = 6: disconnected, type = 0: video
        if (response->stat == 6 && s_videoCallId[socket_id] == response->id) {
            s_videoCallId[socket_id] = -1;
        } else if (response->type > 0 && response->stat == RIL_CALL_ACTIVE) {
            s_videoCallId[socket_id] = response->id;
        }

        err = at_tok_nextint(&tmp, &response->mpty);
        if (err < 0) {
            RLOGE("get mpty fail");
            goto out;
        }
        err = at_tok_nextstr(&tmp, &response->number);
        if (err < 0) {
            RLOGE("get number fail");
            goto out;
        }
        if (isVoLteEnable()) {
            err = at_tok_nextint(&tmp, &response->num_type);
            if (err < 0) {
                RLOGE("get num_type fail");
                goto out;
            }
            if (at_tok_hasmore(&tmp)) {
                err = at_tok_nextint(&tmp, &response->bs_type);
                if (err < 0) {
                    RLOGE("get bs_type fail");
                }
                err = at_tok_nextint(&tmp, &response->cause);
                if (err < 0) {
                    RLOGE("get cause fail");
                }
                /* add for VoLTE to handle call retry */
                if (response->cause == 380 && response->number != NULL) {
                    cbPara = (CallbackPara *)malloc(sizeof(CallbackPara));
                    if (cbPara != NULL) {
                        cbPara->para = strdup(response->number);
                        cbPara->socket_id = socket_id;
                    }
                    RIL_requestTimedCallback(dialEmergencyWhileCallFailed,
                            (CallbackPara *)cbPara, NULL);

                } else if ((response->cause == 400 || response->cause == 381)
                             && response->number != NULL) {
                    cbPara = (CallbackPara *)malloc(sizeof(CallbackPara));
                    if (cbPara != NULL) {
                        cbPara->para = strdup(response->number);
                        cbPara->socket_id = socket_id;
                    }
                    RIL_requestTimedCallback(redialWhileCallFailed,
                                             (CallbackPara *)cbPara, NULL);
                } else {
                    if (response->type == 1 || response->type == 3) {
                        RIL_onUnsolicitedResponse(
                                RIL_UNSOL_RESPONSE_IMS_CALL_STATE_CHANGED,
                                NULL, 0, socket_id);
                    } else {
                        RIL_onUnsolicitedResponse(
                                RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED,
                                NULL, 0, socket_id);
                    }
                }
                if (response->type == 1 || response->type == 3) {
                    if (at_tok_hasmore(&tmp)) {
                        err = at_tok_nextint(&tmp, &response->location);
                        if (err < 0) {
                            RLOGE("get location fail");
                            response->location = 0;
                        }
                    } else {
                        response->location = 0;
                    }
                    RIL_onUnsolicitedResponse(RIL_EXT_UNSOL_VIDEOPHONE_DSCI,
                            response, sizeof(RIL_VideoPhone_DSCI), socket_id);
                }
            } else {
                if (response->type == 1 || response->type == 3) {
                    RIL_onUnsolicitedResponse(
                            RIL_UNSOL_RESPONSE_IMS_CALL_STATE_CHANGED,
                            NULL, 0, socket_id);
                } else {
                    RIL_onUnsolicitedResponse(
                            RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED,
                            NULL, 0, socket_id);
                }
            }
        } else {
            RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED,
                    NULL, 0, socket_id);

            err = at_tok_nextint(&tmp, &response->num_type);
            if (err < 0) {
                RLOGE("get num_type fail");
                goto out;
            }
            err = at_tok_nextint(&tmp, &response->bs_type);
            if (err < 0) {
                RLOGE("get bs_type fail");
                goto out;
            }

            if (at_tok_hasmore(&tmp)) {
                err = at_tok_nextint(&tmp, &response->cause);
                if (err < 0) {
                    RLOGE("get cause fail");
                    goto out;
                }
                if (at_tok_hasmore(&tmp)) {
                    err = at_tok_nextint(&tmp, &response->location);
                    if (err < 0) {
                        RLOGE("get location fail");
                        goto out;
                    }
                } else {
                    response->location = 0;
                }
                RIL_onUnsolicitedResponse(RIL_EXT_UNSOL_VIDEOPHONE_DSCI,
                        response, sizeof(RIL_VideoPhone_DSCI), socket_id);
            }
        }
    } else if (strStartsWith(s, "+CMCCSI:")) {
        RIL_IMSPHONE_CMCCSI *response = NULL;
        response = (RIL_IMSPHONE_CMCCSI *)alloca(sizeof(RIL_IMSPHONE_CMCCSI));
        char *tmp = NULL;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextint(&tmp, &response->id);
        if (err < 0) {
            RLOGE("get id fail");
            goto out;
        }
        err = at_tok_nextint(&tmp, &response->idr);
        if (err < 0) {
            RLOGD("get idr fail");
            goto out;
        }
        err = at_tok_nextint(&tmp, &response->neg_stat_present);
        if (err < 0) {
            RLOGE("get neg_stat_present fail");
            goto out;
        }
        err = at_tok_nextint(&tmp, &response->neg_stat);
        if (err < 0) {
            RLOGE("get neg_stat fail");
            goto out;
        }
        err = at_tok_nextstr(&tmp, &response->SDP_md);
        if (err < 0) {
            RLOGE("get SDP_md fail");
            goto out;
        }
        err = at_tok_nextint(&tmp, &response->cs_mod);
        if (err < 0) {
            RLOGE("get cs_mod fail");
            goto out;
        }
        err = at_tok_nextint(&tmp, &response->ccs_stat);
        if (err < 0) {
            RLOGE("get ccs_stat fail");
            goto out;
        }
        err = at_tok_nextint(&tmp, &response->mpty);
        if (err < 0) {
            RLOGE("get mpty fail");
            goto out;
        }
        err = at_tok_nextint(&tmp, &response->num_type);
        if (err < 0) {
            RLOGD("get num_type fail");
            goto out;
        }
        err = at_tok_nextint(&tmp, &response->ton);
        if (err < 0) {
            RLOGE("get ton fail");
            goto out;
        }
        err = at_tok_nextstr(&tmp, &response->number);
        if (err < 0) {
            RLOGE("get number fail");
            goto out;
        }
        err = at_tok_nextint(&tmp, &response->exit_type);
        if (err < 0) {
            RLOGE("get exit_type fail");
            goto out;
        }
        err = at_tok_nextint(&tmp, &response->exit_cause);
        if (err < 0) {
            RLOGE("get exit_cause fail");
            goto out;
        }
        if (isVoLteEnable()) {
            if (response->cs_mod == 0) {
                RIL_onUnsolicitedResponse(
                        RIL_UNSOL_RESPONSE_IMS_CALL_STATE_CHANGED, NULL,
                        0, socket_id);
            }
        }
    } else if (strStartsWith(s, "+CMCCSS")) {
        /* CMCCSS1, CMCCSS2, ... CMCCSS7, just report IMS state change */
        if (s_imsRegistered[socket_id]) {
            RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_IMS_CALL_STATE_CHANGED,
                                      NULL, 0, socket_id);
        }
    } else if (strStartsWith(s, "+CEN1")) {
        int ret;
        pthread_t tid;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        ret = pthread_create(&tid, &attr, (void *)queryEccNetworkList,
                               (void *)&s_socketId[socket_id]);
        if (ret < 0) {
            RLOGE("Failed to create slow dispatch thread errno: %s",
                   strerror(errno));
        }
    } else if (strStartsWith(s, "+CIREPH")) {
        int status;
        int index = 0;
        char *tmp;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextint(&tmp, &status);
        if (err < 0) {
            RLOGE("%s fail", s);
            goto out;
        }

        if (!(status == SRVCC_PS_TO_CS_START || status == VSRVCC_PS_TO_CS_START
                || status == SRVCC_CS_TO_PS_START)) {
            RIL_requestTimedCallback(excuteSrvccPendingOperate,
                                     (void *)&s_socketId[socket_id], NULL);
        }
        RIL_onUnsolicitedResponse(RIL_UNSOL_SRVCC_STATE_NOTIFY, &status,
                                  sizeof(status), socket_id);


        if (status == SRVCC_PS_TO_CS_SUCCESS ||
                status == VSRVCC_PS_TO_CS_SUCCESS) {
            RIL_requestTimedCallback(sendCSCallStateChanged,
                    (void *)&s_socketId[socket_id], &TIMEVAL_SRVCC_CALLSTATEPOLL);
        } else if(status == SRVCC_CS_TO_PS_SUCCESS){
            RIL_requestTimedCallback(sendIMSCallStateChanged,
                    (void *)&s_socketId[socket_id], &TIMEVAL_SRVCC_CALLSTATEPOLL);
        }
    } else if (strStartsWith(s, AT_PREFIX"DVTCODECRI:")) {
        int response[4];
        int index = 0;
        int iLen = 1;
        char *tmp;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextint(&tmp, &response[0]);
        if (err < 0) {
            RLOGE("%s fail", s);
            goto out;
        }

        if (3 == response[0]) {
            for (index = 1; index <= 3; index++) {
                err = at_tok_nextint(&tmp, &response[index]);
                if (err < 0) {
                    RLOGD("%s fail", s);
                    goto out;
                }
            }
            iLen = 4;
        }
        RIL_onUnsolicitedResponse(RIL_EXT_UNSOL_VIDEOPHONE_CODEC, &response,
                                  iLen * sizeof(response[0]), socket_id);
    } else if (strStartsWith(s, AT_PREFIX"DVTSTRRI:")) {
        char *response = NULL;
        char *tmp;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextstr(&tmp, &response);
        if (err < 0) {
            RLOGE("%s fail", s);
            goto out;
        }
        RIL_onUnsolicitedResponse(RIL_EXT_UNSOL_VIDEOPHONE_STRING, response,
                                  strlen(response) + 1, socket_id);
    } else if (strStartsWith(s, AT_PREFIX"DVTSENDRI")) {
        int response[3];
        char *tmp;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextint(&tmp, &(response[0]));
        if (err < 0) {
            RLOGE("%s fail", s);
            goto out;
        }
        err = at_tok_nextint(&tmp, &(response[1]));
        if (err < 0) {
            RLOGE("%s fail", s);
            goto out;
        }
        err = at_tok_nextint(&tmp, &(response[2]));
        if (err < 0) {
            RIL_onUnsolicitedResponse(RIL_EXT_UNSOL_VIDEOPHONE_REMOTE_MEDIA,
                    &response, sizeof(response[0]) * 2, socket_id);
        } else {
            RIL_onUnsolicitedResponse(RIL_EXT_UNSOL_VIDEOPHONE_REMOTE_MEDIA,
                    &response, sizeof(response), socket_id);
        }
    } else if (strStartsWith(s, AT_PREFIX"DVTMMTI")) {
        int response;
        char *tmp;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextint(&tmp, &response);
        if (err < 0) {
            RLOGE("%s fail", s);
            goto out;
        }
        RIL_onUnsolicitedResponse(RIL_EXT_UNSOL_VIDEOPHONE_MM_RING, &response,
                sizeof(response), socket_id);
    } else if (strStartsWith(s, AT_PREFIX"DVTRELEASING")) {
        char *response = NULL;
        char *tmp;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextstr(&tmp, &response);
        if (err < 0) {
            RLOGE("%s fail", s);
            goto out;
        }
        RIL_onUnsolicitedResponse(RIL_EXT_UNSOL_VIDEOPHONE_RELEASING, response,
                strlen(response) + 1, socket_id);
    } else if (strStartsWith(s, AT_PREFIX"DVTRECARI")) {
        int response;
        char *tmp;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextint(&tmp, &response);
        if (err < 0) {
            RLOGE("%s fail", s);
            goto out;
        }
        RIL_onUnsolicitedResponse(RIL_EXT_UNSOL_VIDEOPHONE_RECORD_VIDEO,
                &response, sizeof(response), socket_id);
    } else if (strStartsWith(s, AT_PREFIX"VTMDSTRT")) {
        int response;
        char *tmp;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextint(&tmp, &response);
        if (err < 0) {
            RLOGE("%s fail", s);
            goto out;
        }

        RIL_onUnsolicitedResponse(RIL_EXT_UNSOL_VIDEOPHONE_MEDIA_START,
                &response, sizeof(response), socket_id);
    } else if (strStartsWith(s, AT_PREFIX"DVTRING:")
                || strStartsWith(s, AT_PREFIX"DVTCLOSED")) {
        if (!isVoLteEnable()) {
            RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED,
                    NULL, 0, socket_id);
        }
    } else if (strStartsWith(s, "+SPIMSPDPINFO")) {
        /* add for VoLTE to handle video call bearing lost */
        char *tmp;
        int cid;
        int state;
        int qci;
        int isVideoCall = 1;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextint(&tmp, &cid);
        if (err < 0) {
            RLOGE("get cid fail");
        }
        err = at_tok_nextint(&tmp, &state);
        if (err < 0) {
            RLOGE("get state fail");
            goto out;
        }
        err = at_tok_nextint(&tmp, &qci);
        if (err < 0) {
            RLOGE("get qci fail");
            goto out;
        }

        if (at_tok_hasmore(&tmp)) {
            err = at_tok_nextint(&tmp, &isVideoCall);
            if (err < 0) {
                RLOGE("get isVideoCall fail");
                goto out;
            }
        }

        // state = 0: deactive, qci = 2: video
        if (state == 0 && qci == 2 && isVideoCall == 1) {
            RIL_requestTimedCallback(onDowngradeToVoice,
                    (void *)&s_socketId[socket_id], NULL);
        }
    } else if (strStartsWith(s, "+SCSFB")) {
        int index;
        char *tmp;
        CallbackPara *cbPara;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextint(&tmp, &index);
        if (err < 0) {
            RLOGE("%s fail", s);
            goto out;
        }
        cbPara = (CallbackPara *)malloc(sizeof(CallbackPara));
        if (cbPara != NULL) {
            cbPara->para = (int *)malloc(sizeof(int));
            *((int *)(cbPara->para)) = index;
            cbPara->socket_id = socket_id;
            RIL_requestTimedCallback(onCallCSFallBackAccept,
                    (void *)cbPara, NULL);
        }
    } else if (strStartsWith(s, "+CSSSFB")) {
        int index = 0;
        char *tmp = NULL;
        CallbackPara *cbPara = NULL;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextint(&tmp, &index);
        if (err < 0) {
            RLOGE("%s fail", s);
            goto out;
        }
        cbPara = (CallbackPara *)calloc(1, sizeof(CallbackPara));
        if (cbPara != NULL) {
            cbPara->para = (int *)calloc(1, sizeof(int));
            *((int *)(cbPara->para)) = index;
            cbPara->socket_id = socket_id;
            RIL_requestTimedCallback(onSSCallCSFallBackAccept, (void *)cbPara,
                                     NULL);
        }
    } else if (strStartsWith(s, "+IMSHOU")) {
        int status;
        char *tmp;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextint(&tmp, &status);
        if (err < 0) {
            RLOGE("%s fail", s);
            goto out;
        }

        RIL_onUnsolicitedResponse(RIL_UNSOL_IMS_HANDOVER_REQUEST, &status,
                                  sizeof(status), socket_id);
    } else if (strStartsWith(s, "+IMSHORSTU")) {
        int status;
        char *tmp;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextint(&tmp, &status);
        if (err < 0) {
            RLOGE("%s fail", s);
            goto out;
        }

        RIL_onUnsolicitedResponse(RIL_UNSOL_IMS_HANDOVER_STATUS_CHANGE, &status,
                                  sizeof(status), socket_id);
    } else if (strStartsWith(s, "+IMSHOLTEINFU")) {
        int status;
        char *tmp;
        char *info;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        IMS_NetworkInfo *response =
                (IMS_NetworkInfo *)alloca(sizeof(IMS_NetworkInfo));
        err = at_tok_nextint(&tmp, &response->type);
        if (err < 0) {
            RLOGE("get neg_stat fail");
            goto out;
        }
        err = at_tok_nextstr(&tmp, &response->info);
        if (err < 0) {
            RLOGE("get SDP_md fail");
            goto out;
        }

        RIL_onUnsolicitedResponse(RIL_UNSOL_IMS_NETWORK_INFO_CHANGE, response,
                                  sizeof(IMS_NetworkInfo), socket_id);
    } else if (strStartsWith(s, "+IMSREGADDR:")) {
        char *response[2] = {NULL, NULL};
        char *tmp;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextstr(&tmp, &(response[0]));
        if (err < 0) {
            RLOGE("%s fail", s);
            goto out;
        }

        if (at_tok_hasmore(&tmp)) {
            err = at_tok_nextstr(&tmp, &(response[1]));
            if (err < 0) {
               RLOGE("%s fail", s);
               goto out;
            }
            RIL_onUnsolicitedResponse(RIL_UNSOL_IMS_REGISTER_ADDRESS_CHANGE,
                                     response, 2 * sizeof(char *), socket_id);
        } else {
            RIL_onUnsolicitedResponse(RIL_UNSOL_IMS_REGISTER_ADDRESS_CHANGE,
                                     response, 1 * sizeof(char *), socket_id);
        }
    } else if (strStartsWith(s, "+WIFIPARAM:")) {
        char *tmp = NULL;
        int response[4] = {0};

        /* +WIFIPARAM:<latency>,<loss>,<jitter>,<rtpTimeout> */
        line = strdup(s);
        tmp = line;

        err = at_tok_start(&tmp);
        if (err < 0) goto out;

        err = at_tok_nextint(&tmp, &response[0]);
        if (err < 0) goto out;

        err = at_tok_nextint(&tmp, &response[1]);
        if (err < 0) goto out;

        err = at_tok_nextint(&tmp, &response[2]);
        if (err < 0) goto out;

        err = at_tok_nextint(&tmp, &response[3]);
        if (err < 0) goto out;

        RIL_onUnsolicitedResponse(RIL_UNSOL_IMS_WIFI_PARAM, response,
                                  sizeof(response), socket_id);
    } else if (strStartsWith(s, "+EARLYMEDIA:")) {
        char *tmp = NULL;
        int response = 0;
        RLOGD("UNSOL EARLY MEDIA is : %s", s);

        /* +EARLYMEDIA:<value> */
        line = strdup(s);
        tmp = line;

        err = at_tok_start(&tmp);
        if (err < 0) goto out;

        err = at_tok_nextint(&tmp, &response);
        if (err < 0) goto out;

        RIL_onUnsolicitedResponse(RIL_EXT_UNSOL_EARLY_MEDIA,
                &response, sizeof(response), socket_id);
    } else if (strStartsWith(s, "+SPCAPABILITY:")) {
        char *tmp;
        int response = 0;

        line = strdup(s);
        tmp = line;

        err = at_tok_start(&tmp);
        if (err < 0) goto out;

        skipNextComma(&tmp);
        skipNextComma(&tmp);
        err = at_tok_nextint(&tmp, &response);
        if (err < 0) goto out;

        RIL_onUnsolicitedResponse(RIL_EXT_UNSOL_UPDATE_HD_VOICE_STATE, &response,
                                  sizeof(response), socket_id);
    }else {
        ret = 0;
    }
    /* unused unsolicited response
    RIL_UNSOL_CALL_RING
    RIL_UNSOL_ENTER_EMERGENCY_CALLBACK_MODE
    RIL_UNSOL_RINGBACK_TONE
    RIL_UNSOL_RESEND_INCALL_MUTE
    RIL_UNSOL_EXIT_EMERGENCY_CALLBACK_MODE
    */
out:
    free(line);
    return ret;
}
