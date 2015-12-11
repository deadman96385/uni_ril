/**
 * ril_call.c --- Call-related requests process functions implementation
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */
#define LOG_TAG "RIL"

#include "reference-ril.h"
#include "ril_call.h"

static EccList *s_eccList[SIM_COUNT];
ListNode s_DTMFList[SIM_COUNT];

static pthread_mutex_t s_eccListMutex[SIM_COUNT] = {
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

void sendCallStateChanged(void *param) {
    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0,
                              socket_id);
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

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &(p_call->index));
    if (err < 0) goto error;

    err = at_tok_nextbool(&line, &(p_call->isMT));
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &state);
    if (err < 0) goto error;

    err = clccStateToRILState(state, &(p_call->state));
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &mode);
    if (err < 0) goto error;

    p_call->isVoice = (mode == 0);

    err = at_tok_nextbool(&line, &(p_call->isMpty));
    if (err < 0) goto error;

    if (at_tok_hasmore(&line)) {
        err = at_tok_nextstr(&line, &(p_call->number));

        /* tolerate null here */
        if (err < 0) return 0;

        // Some lame implementations return strings
        // like "NOT AVAILABLE" in the CLCC line
        if (p_call->number != NULL &&
            0 == strspn(p_call->number, "+0123456789*#abc")) {
            p_call->number = NULL;
        }

        err = at_tok_nextint(&line, &p_call->toa);
        if (err < 0) goto error;
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

static inline int all_calls(int channelID, int do_mute) {
    ATResponse *p_response;
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
        size_t datalen, RIL_Token t, int bVideoCall) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int i, err;
    int countCalls;
    int countValidCalls;
    int needRepoll = 0;
    ATResponse *p_response;
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
    for (i = 0; i < countCalls ; i++) {
        pp_calls[i] = &(p_calls[i]);
    }
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
        if (bVideoCall == 0) {
            RIL_requestTimedCallback(sendCallStateChanged,
                    (void *)&s_socketId[socket_id], &TIMEVAL_CALLSTATEPOLL);
        }
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

    RIL_Dial *p_dial = NULL;
    p_dial = (RIL_Dial *)data;

    switch (p_dial->clir) {
        case 0: clir = ""; break;   /* subscription default */
        case 1: clir = "I"; break;  /* invocation */
        case 2: clir = "i"; break;  /* suppression */
        default: break;
    }

    ret = asprintf(&cmd, "ATD%s%s;", p_dial->address, clir);
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
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    return;
error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
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
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
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

    err = at_send_command(s_ATChannels[channelID], "AT+CHLD=0", &p_response);
    if (err < 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    }
    at_response_free(p_response);
}

static void requestHangupForegrountResumeBackground(int channelID,
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
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
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
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
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
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
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
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
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
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
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
    err = at_send_command(s_ATChannels[channelID], cmd, NULL);
    if (err < 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
    snprintf(cmd, sizeof(cmd), "AT+EVTS=1,%c", (int)character);
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (err < 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
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
        err = at_send_command(s_ATChannels[channelID], "AT+SDTMF=0", NULL);
        if (err < 0) {
            RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        }
        snprintf(cmd, sizeof(cmd), "AT+EVTS=0,%c", (int)character);
        err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
        if (err < 0 || p_response->success == 0) {
            RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        } else {
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        }
        at_response_free(p_response);
        list_remove(socket_id, cmd_item);
        free(cmd_item);
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
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
            RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
        } else {
            RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        }
        at_response_free(p_response);
    } else {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
}

static void requestSTKCallRequestFromSIM(int channelID, void *data,
                                              size_t datalen, RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int err;
    int value = ((int *)data)[0];
    ATResponse *p_response = NULL;

    if (value == 0) {
        RLOGD("cancel STK call");
        err = at_send_command(s_ATChannels[channelID], "AT+SPUSATCALLSETUP=0",
                              &p_response);
    } else {
        RLOGD("confirm STK call");
        err = at_send_command(s_ATChannels[channelID], "AT+SPUSATCALLSETUP=1",
                              &p_response);
    }
    if (err < 0 || p_response->success == 0) {
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
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
        RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    }
    at_response_free(p_response);
}

static int getEccRecordCategory(int simID, char *number) {
    if (number == NULL || s_eccList[simID] == NULL) {
        return -1;
    }

    int category = -1;
    EccList *p_eccList = s_eccList[simID];
    while (p_eccList != NULL && p_eccList->number != NULL) {
        if (strcmp(number, p_eccList->number) == 0) {
            category = p_eccList->category;
            break;
        }
        p_eccList = p_eccList->prev;
    }
    RLOGD("getEccRecordCategory->number: %s category: %d", number, category);
    return category;
}

static void requestEccDial(int channelID, void *data, size_t datalen,
                              RIL_Token t) {
    RIL_UNUSED_PARM(datalen);

    int category = -1;
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

    token = strchr(p_dial->address, '/');
    if (token) {
        *token = '\0';
    }
    category = getEccRecordCategory(socket_id, p_dial->address);

    if (category != -1) {
        ret = asprintf(&cmd, "ATD%s@%d,#%s;", p_dial->address, category, clir);
    } else {
        if (token) {
            *token = '@';
            ret = asprintf(&cmd, "ATD%s,#%s;", p_dial->address, clir);
        } else {
            ret = asprintf(&cmd, "ATD%s@,#%s;", p_dial->address, clir);
        }
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
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    return;

error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

void requestLastCallFailCause(int channelID, void *data, size_t datalen,
                                  RIL_Token t) {
    RIL_UNUSED_PARM(data);
    RIL_UNUSED_PARM(datalen);

    int response = CALL_FAIL_ERROR_UNSPECIFIED;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    pthread_mutex_lock(&s_callMutex[socket_id]);
    switch (s_callFailCause[socket_id]) {
        case 1:
        case 22:
        case 28:
            response = CALL_FAIL_UNOBTAINABLE_NUMBER;
            break;
        case 0:
        case 3:
        case 16:
        case 301:
            response = CALL_FAIL_NORMAL;
            break;
        case 302:
            response = CALL_FAIL_IMEI_NOT_ACCEPTED;
            break;
        case 17:
        case 21:
            response = CALL_FAIL_BUSY;
            break;
        case 34:
        case 38:
        case 41:
        case 42:
        case 44:
        case 47:
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
        default:
            response = CALL_FAIL_ERROR_UNSPECIFIED;
            break;
    }
    pthread_mutex_unlock(&s_callMutex[socket_id]);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(int));
}

static void requestGetCurrentCallsVoLTE(int channelID, void *data,
        size_t datalen, RIL_Token t, int bVideoCall) {
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

    int groupCallIndex = 8;
    for (countValidCalls = 0, p_cur = p_response->p_intermediates;
         p_cur != NULL; p_cur = p_cur->p_next) {
        err = callFromCLCCLineVoLTE(p_cur->line, p_calls + countValidCalls);
        p_t_calls = p_calls + countValidCalls;
        if (p_t_calls->mpty == 2) {
            if (groupCallIndex != 8) {
                p_t_calls->index = groupCallIndex;
            }
            groupCallIndex--;
        }

        if (err != 0) {
            continue;
        }

        countValidCalls++;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, pp_calls,
                          countValidCalls * sizeof(RIL_Call_VoLTE *));

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
        if (bVideoCall == 0) {
            RIL_requestTimedCallback(sendCallStateChanged, NULL,
                                     &TIMEVAL_CALLSTATEPOLL);
        } else {
            // TODO: VoLTE
            // RIL_requestTimedCallback(sendVideoCallStateChanged, NULL,
            //                          &TIMEVAL_CALLSTATEPOLL);
            // RIL_UNSOL_RESPONSE_VIDEOCALL_STATE_CHANGED
        }
    }
    return;
}

int processCallRequest(int request, void *data, size_t datalen,
        RIL_Token t, int channelID) {
    int ret = 1;
    int err;
    ATResponse *p_response = NULL;

    switch (request) {
        case RIL_REQUEST_GET_CURRENT_CALLS:
            requestGetCurrentCalls(channelID, data, datalen, t, 0);
            break;
        case RIL_REQUEST_DIAL:
            requestDial(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_HANGUP:
            requestHangup(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND:
            requestHangupWaitingOrBackground(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND:
            requestHangupForegrountResumeBackground(channelID, data, datalen, t);
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
        case RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM:
            requestSTKCallRequestFromSIM(channelID, data, datalen, t);
            break;
        case RIL_REQUEST_EXPLICIT_CALL_TRANSFER:
            requestExplicitCallTransfer(channelID, data, datalen, t);
            break;
        // TODO: for now, not realized
        // case RIL_REQUEST_SET_TTY_MODE:
        // case RIL_REQUEST_QUERY_TTY_MODE:
        //     break;
        case RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE:
            RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            break;
        /* IMS request @{ */
        case RIL_REQUEST_GET_IMS_CURRENT_CALLS:
            requestGetCurrentCallsVoLTE(channelID, data, datalen, t, 0);
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
                }
            } else {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            }
            at_response_free(p_response);
            break;
        }
        case RIL_REQUEST_IMS_CALL_REQUEST_MEDIA_CHANGE: {
            char cmd[AT_COMMAND_LEN] = {0};
            int isVideo = ((int *)data)[0];
            if (isVideo) {
                snprintf(cmd, sizeof(cmd), "AT+CCMMD=1,2,\"m=audio\"");
            } else {
                snprintf(cmd, sizeof(cmd), "AT+CCMMD=1,2,\"m=video\"");
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
        case RIL_REQUEST_IMS_CALL_RESPONSE_MEDIA_CHANGE: {
            char cmd[AT_COMMAND_LEN] = {0};
            int isAccept = ((int *)data)[0];

            if (isAccept) {
                snprintf(cmd, sizeof(cmd), "AT+CCMMD=1,3");
            } else {
                snprintf(cmd, sizeof(cmd), "AT+CCMMD=1,4");
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
        case RIL_REQUEST_IMS_CALL_FALL_BACK_TO_VOICE: {
            char cmd[AT_COMMAND_LEN] = {0};
            p_response = NULL;
            snprintf(cmd, sizeof(cmd), "AT+CCMMD=1,1,\"m=audio\"");
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

            snprintf(cmd, sizeof(cmd), "AT+CGU=1,\"%s\"", (char *)data);
            err = at_send_command(s_ATChannels[channelID], cmd, NULL);
            if (err >= 0) {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            }
            break;
        }
        case RIL_REQUEST_IMS_ADD_TO_GROUP_CALL: {
            char cmd[AT_COMMAND_LEN] = {0};

            snprintf(cmd, sizeof(cmd), "AT+CGU=4,\"%s\"", (char *)data);
            err = at_send_command(s_ATChannels[channelID], cmd, NULL);
            if (err >= 0) {
                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
            } else {
                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
            }
            break;
        }
        /* }@ */
        default:
            ret = 0;
            break;
    }
    return ret;
}

/* add for VoLTE to handle emergency call*/
static void addEccRecord(RIL_SOCKET_ID socket_id, EccList *record) {
    int simId = socket_id;
    if (record == NULL) {
        return;
    }
    RLOGD("addEccRecord->number: %s category: %d", record->number,
          record->category);
    pthread_mutex_lock(&s_eccListMutex[simId]);
    if (s_eccList[simId] == NULL) {
        s_eccList[simId] = record;
        s_eccList[simId]->prev = NULL;
        s_eccList[simId]->next = NULL;
    } else {
        s_eccList[simId]->next = record;
        record->prev = s_eccList[simId];
        record->next = NULL;
        s_eccList[simId] = record;
    }
    pthread_mutex_unlock(&s_eccListMutex[simId]);
}

static void updateEccRecord(RIL_SOCKET_ID socket_id, EccList *record) {
    int simId = socket_id;
    if (s_eccList[simId] == NULL || record == NULL) {
        return;
    }
    RLOGD("updateEccRecord->number: %s category: %d", record->number,
          record->category);
    pthread_mutex_lock(&s_eccListMutex[simId]);
    EccList *tem_record = s_eccList[simId];
    if (record->number != NULL) {
        while (tem_record != NULL && tem_record->number != NULL) {
            if (strcmp(record->number, tem_record->number) == 0) {
                tem_record->category = record->category;
                break;
            }
            tem_record = tem_record->prev;
        }
    }
    pthread_mutex_unlock(&s_eccListMutex[simId]);
}

static void addEmergencyNumbertoEccList(RIL_SOCKET_ID socket_id,
                                             EccList *record) {
    char eccList[PROPERTY_VALUE_MAX] = {0};
    char propName[PROPERTY_VALUE_MAX] = {0};
    char tmpList[PROPERTY_VALUE_MAX] = {0};
    char *p_eccList;
    int numberExist = 0;
    strcpy(propName, ECC_LIST_PROP);

#if defined (ANDROID_MULTI_SIM)
    sprintf(propName, "%s%d", propName, socket_id);
#endif
    property_get(propName, eccList, "");

    if (strlen(eccList) == 0) {
        numberExist = 1;
    } else {
        strncpy(tmpList, eccList, strlen(eccList));

        p_eccList = strtok(tmpList, ",");
        if (p_eccList != NULL && strcmp(p_eccList, record->number) == 0) {
            numberExist = 1;
        }
        while (p_eccList != NULL && !numberExist) {
            if (strcmp(p_eccList, record->number) == 0) {
                numberExist = 1;
            }
            p_eccList = strtok(NULL, ",");
        }
    }

    if (!numberExist) {
        sprintf(eccList, "%s,%s,", eccList, record->number);
        property_set(propName, eccList);
        addEccRecord(socket_id, record);
    } else {
        updateEccRecord(socket_id, record);
        free(record->number);
        free(record);
    }
    RLOGD("ecclist = %s number_exist: %d", eccList, numberExist);
}

static void dialEmergencyWhileCallFailed(void *param) {
    RLOGD("dialEmergencyWhileCallFailed->address =%s", (char *)param);

    if (param != NULL) {
        CallbackPara *cbPara = (CallbackPara *)param;
        char *number = cbPara->para;
        RIL_Dial *p_dial = (RIL_Dial *)calloc(1, sizeof(RIL_Dial));
        EccList *record = (EccList *)calloc(1, sizeof(EccList));
        record->number = (char *)strdup(number);
        addEmergencyNumbertoEccList(cbPara->socket_id, record);

        p_dial->address = number;
        p_dial->clir = 0;
        p_dial->uusInfo = NULL;
        int channelID = getChannel(cbPara->socket_id);
        requestEccDial(channelID, p_dial, sizeof(*p_dial), NULL);
        putChannel(channelID);

        free(p_dial->address);
        free(p_dial);
        free(cbPara);
    }
}

static void redialWhileCallFailed(void *param) {
    RLOGD("redialWhileCallFailed->address =%s", (char *)param);

    if (param != NULL) {
        CallbackPara *cbPara = (CallbackPara *)param;
        char *number = cbPara->para;
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

int processCallUnsolicited(RIL_SOCKET_ID socket_id, const char *s) {
    int err;
    int ret = 1;
    extern int s_isuserdebug;
    char *line = NULL;

    if (strStartsWith(s, "+CRING:") ||
        strStartsWith(s, "RING") ||
        strStartsWith(s, "NO CARRIER") ||
        strStartsWith(s, "+CCWA")) {
        RIL_onUnsolicitedResponse(
                RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0, socket_id);
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
            RLOGD("get id fail");
            goto out;
        }
        err = at_tok_nextint(&tmp, &response->idr);
        if (err < 0) {
            RLOGD("get idr fail");
            goto out;
        }
        err = at_tok_nextint(&tmp, &response->stat);
        if (err < 0) {
            RLOGD("get stat fail");
            goto out;
        }
        err = at_tok_nextint(&tmp, &response->type);
        if (err < 0) {
            RLOGD("get type fail");
            goto out;
        }
        err = at_tok_nextint(&tmp, &response->mpty);
        if (err < 0) {
            RLOGD("get mpty fail");
            goto out;
        }
        err = at_tok_nextstr(&tmp, &response->number);
        if (err < 0) {
            RLOGD("get number fail");
            goto out;
        }
        if (isVoLteEnable()) {
            err = at_tok_nextint(&tmp, &response->num_type);
            if (err < 0) {
                RLOGD("get num_type fail");
                goto out;
            }
            if (at_tok_hasmore(&tmp)) {
                err = at_tok_nextint(&tmp, &response->bs_type);
                if (err < 0) {
                    RLOGD("get bs_type fail");
                }
                err = at_tok_nextint(&tmp, &response->cause);
                if (err < 0) {
                    RLOGD("get cause fail");
                }
                /* add for VoLTE to handle call retry */
                if (response->cause == 380 && response->number != NULL) {
                    cbPara = (CallbackPara *) malloc(sizeof(CallbackPara));
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
                    RIL_onUnsolicitedResponse(
                            RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0,
                            socket_id);
                }

            } else {
                RIL_onUnsolicitedResponse(
                        RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0,
                        socket_id);
            }
        } else {
            if (response->type == 0) {
                RIL_onUnsolicitedResponse(
                        RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0,
                        socket_id);
                goto out;
            } else if (response->type == 1) {
                // TODO: for videoPhone
            }
        }
    } else if (strStartsWith(s, "+CMCCSI")) {
        RIL_onUnsolicitedResponse(
                RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED,
                NULL, 0, socket_id);
    } else {
        ret = 0;
    }
    /* unused unsolicited response
    RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED
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
