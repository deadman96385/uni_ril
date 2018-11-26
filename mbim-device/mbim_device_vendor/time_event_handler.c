/**
 * ril_sync_cmd_handler.c --- process the async cmd to sync, the request is
 *                            blocked until the UCR response
 *
 * Copyright (C) 2016 Spreadtrum Communications Inc.
 */

#define LOG_TAG "MBIM-Device"

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>
#include <utils/Log.h>
#include <cutils/sockets.h>

#include "misc.h"
#include "time_event_handler.h"
#include "mbim_message_threads.h"

static int s_fdWakeupRead;
static int s_fdWakeupWrite;
static int s_nfds = 0;
static ril_event s_wakeupEvent;
static fd_set s_readFds;
static ril_event s_cmdTimerList;
static ril_event s_requestTimerList;
static ril_event s_pendingList;
static pthread_mutex_t s_listMutex;

extern sem_t s_timeEventHandlerReadySem;

//TODO
const AsyncCmdInfo s_asyncCmdTable[] = {
//        {AT_CMD_STR("+CLCK:"), dispatchCLCK},
//        {AT_CMD_STR("+SPTESTMODE:"), dispatchSPTESTMODE}
};

/*******************************************************************/
/* ATCI socket */
static struct ril_event s_atciListenEvent;
static struct ril_event s_atciCommandsEvent;

SocketListenParam s_atciSocketParam;

#define MAX_FD_EVENTS 4
static struct ril_event *watch_table[MAX_FD_EVENTS];


static void initList(ril_event *list) {
    if (list != NULL) {
        memset(list, 0, sizeof(ril_event));
        list->next = list;
        list->prev = list;
        list->fd = -1;
    }
}

static void addToList(ril_event *ev, ril_event *list) {
    if (ev != NULL && list != NULL) {
        ev->next = list;
        ev->prev = list->prev;
        ev->prev->next = ev;
        list->prev = ev;
    }
}

static void removeFromList(ril_event *ev) {
    if (ev != NULL) {
        ev->next->prev = ev->prev;
        ev->prev->next = ev->next;
        ev->next = NULL;
        ev->prev = NULL;
    }
}

static void removeWatch(struct ril_event *ev, int index) {
    watch_table[index] = NULL;
    ev->index = -1;

    FD_CLR(ev->fd, &s_readFds);

    if (ev->fd + 1 == s_nfds) {
        int n = 0;

        for (int i = 0; i < MAX_FD_EVENTS; i++) {
            struct ril_event *rev = watch_table[i];

            if ((rev != NULL) && (rev->fd > n)) {
                n = rev->fd;
            }
        }
        s_nfds = n + 1;
    }
}

// Initialize an event
void setRilEvent(ril_event *ev, int fd, bool persist,
        ril_event_cb func, void *param) {
    memset(ev, 0, sizeof(ril_event));
    ev->fd = fd;
    ev->index = -1;
    ev->persist = persist;
    ev->func = func;
    ev->param = param;
    fcntl(fd, F_SETFL, O_NONBLOCK);
}

void getNow(struct timeval *tv) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    tv->tv_sec = ts.tv_sec;
    tv->tv_usec = ts.tv_nsec/1000;
}

// Add event to watch list
void ril_event_add(struct ril_event *ev) {
    MUTEX_ACQUIRE(s_listMutex);
    for (int i = 0; i < MAX_FD_EVENTS; i++) {
        if (watch_table[i] == NULL) {
            watch_table[i] = ev;
            ev->index = i;
            FD_SET(ev->fd, &s_readFds);
            if (ev->fd >= s_nfds) s_nfds = ev->fd+1;
            break;
        }
    }
    MUTEX_RELEASE(s_listMutex);
}

// Add timer event
void ril_timer_add(ril_event *ev, struct timeval *tv, int listFlag) {
    MUTEX_ACQUIRE(s_listMutex);

    ril_event *list = NULL;
    ril_event *head = NULL;
    if (tv != NULL) {
        // add to timer list
        if (listFlag == 0) {
            list = s_cmdTimerList.next;
            head = &s_cmdTimerList;
        } else {
            list = s_requestTimerList.next;
            head = &s_requestTimerList;
        }
        ev->fd = -1;  // make sure fd is invalid

        struct timeval now;
        getNow(&now);
        timeradd(&now, tv, &ev->timeout);

        // keep list sorted
        while (timercmp(&list->timeout, &ev->timeout, <)
                && (list != head)) {
            list = list->next;
        }
        // list now points to the first event older than ev
        addToList(ev, list);
    }

    MUTEX_RELEASE(s_listMutex);
}

static void triggerEvLoop() {
    int ret;

    /* trigger event loop to wakeup. No reason to do this,
     * if we're in the event loop thread */
     do {
        ret = write(s_fdWakeupWrite, " ", 1);
     } while (ret < 0 && errno == EINTR);
}

/**
 * A write on the wakeup fd is done just to pop us out of select()
 * We empty the buffer here and then ril_event will reset the timers on the
 * way back down
 */
static void processWakeupCallback(void *param) {
    MBIM_UNUSED_PARM(param);

    char buff[16];
    int ret;

    /* empty our wakeup socket out */
    do {
        ret = read(s_fdWakeupRead, &buff, sizeof(buff));
    } while (ret > 0 || (ret < 0 && errno == EINTR));
}

static int calcNextTimeout(struct timeval *tv) {
    ril_event *cmdTev = s_cmdTimerList.next;
    ril_event *requestTev = s_requestTimerList.next;

    struct timeval *cmdTv = NULL;
    struct timeval *requestTv = NULL;
    struct timeval now;

    getNow(&now);

    // Sorted list, so calc based on first node
    if (cmdTev == &s_cmdTimerList || requestTev == &s_requestTimerList) {
        // no pending timers
        return -1;
    }

    if (timercmp(&cmdTev->timeout, &now, >)) {
        timersub(&cmdTev->timeout, &now, cmdTv);
    } else {
        // timer already expired.
        cmdTv->tv_sec = cmdTv->tv_usec = 0;
    }

    if (timercmp(&requestTev->timeout, &now, >)) {
        timersub(&requestTev->timeout, &now, requestTv);
    } else {
        // timer already expired.
        requestTv->tv_sec = requestTv->tv_usec = 0;
    }

    if (timercmp(cmdTv, requestTv, >)) {
        tv->tv_sec = requestTv->tv_sec;
        tv->tv_usec  = requestTv->tv_usec;
    } else {
        tv->tv_sec = cmdTv->tv_sec;
        tv->tv_usec  = cmdTv->tv_usec;
    }

    return 0;
}

// add the time out cmd to pending list to process
static void processCmdTimeouts() {
    MUTEX_ACQUIRE(s_listMutex);
    struct timeval now;
    ril_event *tev = s_cmdTimerList.next;
    ril_event *next;

    getNow(&now);
    // walk list, see if now >= ev->timeout for any events
    while ((tev != &s_cmdTimerList) && (timercmp(&now, &tev->timeout, >))) {
        // Timer expired
        next = tev->next;
        removeFromList(tev);
        addToList(tev, &s_pendingList);
        tev = next;
    }
    MUTEX_RELEASE(s_listMutex);
}

// add the time out request to pending list to process
static void processRequestTimeouts() {
    MUTEX_ACQUIRE(s_listMutex);
    struct timeval now;
    ril_event *tev = s_requestTimerList.next;
    ril_event *next;

    getNow(&now);
    // walk list, see if now >= ev->timeout for any events
    while ((tev != &s_requestTimerList) && (timercmp(&now, &tev->timeout, >))) {
        // Timer expired
        next = tev->next;
        removeFromList(tev);
        addToList(tev, &s_pendingList);
        tev = next;
    }
    MUTEX_RELEASE(s_listMutex);
}

static void processReadReadies(fd_set *rfds, int n) {
    MUTEX_ACQUIRE(s_listMutex);

    for (int i = 0; (i < MAX_FD_EVENTS) && (n > 0); i++) {
        struct ril_event *rev = watch_table[i];
        if (rev != NULL && FD_ISSET(rev->fd, rfds)) {
            addToList(rev, &s_pendingList);
            if (rev->persist == false) {
                removeWatch(rev, i);
            }
            n--;
        }
    }

    MUTEX_RELEASE(s_listMutex);
}

// process the event including wake up event and timeout cmd event
static void firePending() {
    struct ril_event *ev = s_pendingList.next;
    while (ev != &s_pendingList) {
        struct ril_event *next = ev->next;
        removeFromList(ev);
        ev->func(ev->param);
        ev = next;
    }
}

void rilEventAddWakeup(struct ril_event *ev) {
    ril_event_add(ev);
    triggerEvLoop();
}

void asyncCmdHandlerLoop() {
    int n;
    fd_set rfds;
    struct timeval tv;
    struct timeval *ptv = NULL;

    sem_post(&s_timeEventHandlerReadySem);

    for (;;) {
        // make local copy of read fd_set
        memcpy(&rfds, &s_readFds, sizeof(fd_set));
        if (-1 == calcNextTimeout(&tv)) {
            // no pending timers; block indefinitely
            ptv = NULL;
        } else {
            ptv = &tv;
        }
        RLOGD("before select");
        n = select(s_nfds, &rfds, NULL, NULL, ptv);
        RLOGD("after select");
        if (n < 0) {
            if (errno == EINTR) continue;
            RLOGE("ril_event: select error (%d)", errno);
            return;
        }

        // Check for timeouts
        processCmdTimeouts();
        processRequestTimeouts();

        // Check for read-ready
        processReadReadies(&rfds, n);

        // Fire away
        firePending();
    }
}

void *startTimeEventHandlerLoop(void *param) {
    MBIM_UNUSED_PARM(param);

    RLOGD("startTimeEventHandlerLoop");

    int ret;
    int filedes[2];

    // Initialize internal data struct
    MUTEX_INIT(s_listMutex);
    FD_ZERO(&s_readFds);
    initList(&s_cmdTimerList);
    initList(&s_requestTimerList);
    initList(&s_pendingList);
    memset(watch_table, 0, sizeof(watch_table));

    ret = pipe(filedes);
    if (ret < 0) {
        RLOGE("Error in pipe() errno:%d", errno);
        return NULL;
    }
    s_fdWakeupRead = filedes[0];
    s_fdWakeupWrite = filedes[1];

    fcntl(s_fdWakeupRead, F_SETFL, O_NONBLOCK);

    setRilEvent(&s_wakeupEvent, s_fdWakeupRead, true, processWakeupCallback, NULL);

    rilEventAddWakeup(&s_wakeupEvent);

    asyncCmdHandlerLoop();

    return NULL;
}

// time out cmd request complete with RIL_E_GENERIC_FAILURE
static void cmdTimerCallback(void *param) {
    CmdCallbackInfo *p_info;

    p_info = (CmdCallbackInfo *)param;

    RLOGE("AT command wait for URC %s timeout", p_info->cmd);

#if (SIM_COUNT == 2)
    if (strcmp(p_info->cmd, "+SPTESTMODE:") == 0) {
        pthread_mutex_unlock(&s_radioPowerMutex[RIL_SOCKET_1]);
        pthread_mutex_unlock(&s_radioPowerMutex[RIL_SOCKET_2]);
    }
#endif
//    RIL_onRequestComplete(p_info->token, RIL_E_GENERIC_FAILURE, NULL, 0);

    free(p_info->data);
    free(p_info);
}

// Add async cmd to s_timeList, timeout is in second
void addAsyncCmdList(RIL_SOCKET_ID socket_id, RIL_Token t, const char *cmd,
                        void *data, int timeout) {
    struct timeval relativeTime;
    CmdCallbackInfo *p_info;
    p_info = (CmdCallbackInfo *)malloc(sizeof(CmdCallbackInfo));

    p_info->token = t;
    p_info->socket_id = socket_id;
    p_info->data = data;
    p_info->cmd = cmd;

    if (timeout <= 0) {
        /* treat null parameter as a 0 relative time */
        memset(&relativeTime, 0, sizeof(relativeTime));
    } else {
        relativeTime.tv_sec = timeout;
        relativeTime.tv_usec = 0;
    }

    setRilEvent(&(p_info->event), -1, false, cmdTimerCallback, p_info);

    ril_timer_add(&(p_info->event), &relativeTime, 0);

    triggerEvLoop();
}

static void userTimerCallback(void *param) {
    UserCallbackInfo *p_info;

    p_info = (UserCallbackInfo *)param;

    p_info->p_callback(p_info->userParam);

    free(p_info);
}

void requestTimedCallback(RIL_TimedCallback callback,
        void *param, const struct timeval *relativeTime) {
    struct timeval myRelativeTime;
    UserCallbackInfo *p_info;

    p_info = (UserCallbackInfo *)calloc(1, sizeof(UserCallbackInfo));
    if (p_info == NULL) {
        RLOGE("Memory allocation failed in requestTimedCallback");
        return;
    }

    p_info->p_callback = callback;
    p_info->userParam = param;

    if (relativeTime == NULL) {
        /* treat null parameter as a 0 relative time */
        memset(&myRelativeTime, 0, sizeof(myRelativeTime));
    } else {
        /* FIXME I think event_add's tv param is really const anyway */
        memcpy(&myRelativeTime, relativeTime, sizeof(myRelativeTime));
    }

    setRilEvent(&(p_info->event), -1, false, userTimerCallback, p_info);

    ril_timer_add(&(p_info->event), &myRelativeTime, 1);

    triggerEvLoop();
}

void removeAsyncCmdList(RIL_Token t, const char *cmd) {
    MUTEX_ACQUIRE(s_listMutex);

    ril_event *tev = s_cmdTimerList.next;
    ril_event *next;

    while (tev != &s_cmdTimerList) {
        next = tev->next;
        CmdCallbackInfo *p_info = (CmdCallbackInfo *)tev->param;
        if (p_info->token == t && p_info->cmd == cmd) {
            removeFromList(tev);
            free(p_info->data);
            free(p_info);
            break;
        }
        tev = next;
    }

    MUTEX_RELEASE(s_listMutex);
}

void onRequestComplete(ril_event *ev, void *resp) {
    CmdCallbackInfo *p_info;
    p_info = (CmdCallbackInfo *)(ev->param);
    size_t i;
    AsyncCmdInfo *p_cmdInfo = NULL;

    for (i = 0; i < NUM_ELEMS(s_asyncCmdTable); i++) {
        if (!strncasecmp(s_asyncCmdTable[i].cmd, p_info->cmd,
                s_asyncCmdTable[i].len)) {
            p_cmdInfo = (AsyncCmdInfo *)&s_asyncCmdTable[i];
            break;
        }
    }
    if (p_cmdInfo != NULL) {
        p_cmdInfo->func(p_info->token, p_info->data, resp);
    }

    free(p_info->data);
    free(p_info);
}

// walk list, see if URC's corresponding AT command is in s_cmdTimerList
void checkAndCompleteRequest(RIL_SOCKET_ID socket_id, const char *cmd,
                                 void *resp) {
    MUTEX_ACQUIRE(s_listMutex);
    ril_event *tev = s_cmdTimerList.next;
    ril_event *next;
    CmdCallbackInfo *p_info = NULL;

    while (tev != &s_cmdTimerList) {
        next = tev->next;
        p_info = (CmdCallbackInfo *)(tev->param);
        if (p_info->socket_id == socket_id && strcmp(cmd, p_info->cmd) == 0) {
            onRequestComplete(tev, resp);
            removeFromList(tev);
            break;
        }
        tev = next;
    }
    MUTEX_RELEASE(s_listMutex);
}



static void processCommandsCallback(void *param) {
    RecordStream *p_rs;
    void *p_record;
    size_t recordlen;
    int ret;
    int32_t simId = 0;
    int fd;

    SocketListenParam *p_info = (SocketListenParam *)param;

    fd = p_info->fdCommand;
    p_rs = p_info->p_rs;

    for (;;) {
        /* loop until EAGAIN/EINTR, end of stream, or other error */
        ret = record_stream_get_next(p_rs, &p_record, &recordlen);

        if (ret == 0 && p_record == NULL) {
            /* end-of-stream */
            break;
        } else if (ret < 0) {
            break;
        } else if (ret == 0) {  /* && p_record != NULL */
            MbimMessage *message = NULL;
            message = mbim_message_new(p_record, recordlen);
            enqueueCommandMessage(message, MBIM_ATCI_COMMUNICATION);
        }
    }

    if (ret == 0 || !(errno == EAGAIN || errno == EINTR)) {
        /* fatal error or end-of-stream */
        if (ret != 0) {
            RLOGE("error on reading command socket errno: %d\n", errno);
        } else {
            RLOGW("EOS.  Closing atci socket.");
        }
        if (fd != -1) {
            close(fd);
        }
        p_info->fdCommand = -1;
        s_atciSocketParam.fdCommand = -1;
        if (s_atciSocketParam.p_rs != NULL) {
            record_stream_free(s_atciSocketParam.p_rs);
            s_atciSocketParam.p_rs = NULL;
        }
        rilEventAddWakeup(s_atciSocketParam.listen_event);
    }
}

static void listenCallback(void *param) {
    int ret;
    int fd, fdCommand;
    RecordStream *p_rs;
    bool persist = false;

    SocketListenParam *p_info = (SocketListenParam *)param;

    fd = p_info->fdListen;

    fdCommand = accept(fd, NULL, NULL);
    if (fdCommand < 0) {
        RLOGE("[ATCI] Error on accept() errno:%d", errno);
        return;
    }

    ret = fcntl(fdCommand, F_SETFL, O_NONBLOCK);
    if (ret < 0) {
        RLOGE("[ATCI] Error setting O_NONBLOCK errno:%d", errno);
    }

    RLOGI("new connection to atci socket");

    p_info->fdCommand = fdCommand;
    p_rs = record_stream_new(p_info->fdCommand, 1024);
    p_info->p_rs = p_rs;

    setRilEvent(p_info->commands_event, p_info->fdCommand, false,
            p_info->processCommandsCallback, p_info);

    rilEventAddWakeup(p_info->commands_event);
}

void listenAtciSocket() {
    int ret = -1;
    int fdListen = -1;

    // start ATCI interface socket
    /* Initialize ATCI socket parameters */
    s_atciSocketParam.fdListen = -1;
    s_atciSocketParam.fdCommand = -1;
    s_atciSocketParam.commands_event = &s_atciCommandsEvent;
    s_atciSocketParam.listen_event = &s_atciListenEvent;
    s_atciSocketParam.processCommandsCallback = processCommandsCallback;

    fdListen = socket_local_server("atci_socket",
                ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
    if (fdListen < 0) {
        RLOGE("Failed to get socket atci");
        return;
    }

    ret = listen(fdListen, 1);
    if (ret < 0) {
        RLOGE("Failed to listen on control socket '%d': %s",
                fdListen, strerror(errno));
        return;
    }
    s_atciSocketParam.fdListen = fdListen;

    /* note: we can accept only one atci connection at a time */
    setRilEvent(&s_atciListenEvent, fdListen, false,
            listenCallback, &s_atciSocketParam);

    rilEventAddWakeup(&s_atciListenEvent);
}
