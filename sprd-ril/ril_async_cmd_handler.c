/**
 * ril_sync_cmd_handler.c --- process the async cmd to sync, the request is
 *                            blocked until the UCR response
 *
 * Copyright (C) 2016 Spreadtrum Communications Inc.
 */

#define LOG_TAG "RIL"

#include "sprd_ril.h"
#include "ril_sim.h"
#include "ril_network.h"
#include "ril_async_cmd_handler.h"

static int s_fdWakeupRead;
static int s_fdWakeupWrite;
static int s_nfds = 0;
static ril_event s_wakeupEvent;
static fd_set s_readFds;
static ril_event s_timerList;
static ril_event s_pendingList;
static pthread_mutex_t s_listMutex;

const AsyncCmdInfo s_asyncCmdTable[] = {
        {AT_CMD_STR("+CLCK:"), dispatchCLCK},
        {AT_CMD_STR("+SPTESTMODE:"), dispatchSPTESTMODE}
};

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
        if(ev->next != NULL) {
	    RLOGE("removeFromList ev->next");
            ev->next->prev = ev->prev;
	}
	if(ev->prev != NULL) {
	    RLOGE("removeFromList ev->prev");
            ev->prev->next = ev->next;
	}
        ev->next = NULL;
        ev->prev = NULL;
    }
}

// Initialize an event
void setRilEvent(ril_event *ev, int fd, ril_event_cb func, void *param) {
    memset(ev, 0, sizeof(ril_event));
    ev->fd = fd;
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

// Add timer event
void addRilEvent(ril_event *ev, struct timeval *tv) {
    MUTEX_ACQUIRE(s_listMutex);

    ril_event *list;
    if (tv != NULL) {
        // add to timer list
        list = s_timerList.next;
        ev->fd = -1;  // make sure fd is invalid

        struct timeval now;
        getNow(&now);
        timeradd(&now, tv, &ev->timeout);

        // keep list sorted
        while (timercmp(&list->timeout, &ev->timeout, <)
                && (list != &s_timerList)) {
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
    RIL_UNUSED_PARM(param);

    char buff[16];
    int ret;

    /* empty our wakeup socket out */
    do {
        ret = read(s_fdWakeupRead, &buff, sizeof(buff));
    } while (ret > 0 || (ret < 0 && errno == EINTR));
}

static int calcNextTimeout(struct timeval *tv) {
    ril_event *tev = s_timerList.next;
    struct timeval now;

    getNow(&now);

    // Sorted list, so calc based on first node
    if (tev == &s_timerList) {
        // no pending timers
        return -1;
    }

    if (timercmp(&tev->timeout, &now, >)) {
        timersub(&tev->timeout, &now, tv);
    } else {
        // timer already expired.
        tv->tv_sec = tv->tv_usec = 0;
    }
    return 0;
}

// add the time out cmd to pending list to process
static void processTimeouts() {
    MUTEX_ACQUIRE(s_listMutex);
    struct timeval now;
    ril_event *tev = s_timerList.next;
    ril_event *next;

    getNow(&now);
    // walk list, see if now >= ev->timeout for any events
    while ((tev != &s_timerList) && (timercmp(&now, &tev->timeout, >))) {
        // Timer expired
        next = tev->next;
        removeFromList(tev);
        addToList(tev, &s_pendingList);
        tev = next;
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

void asyncCmdHandlerLoop() {
    int n;
    fd_set rfds;
    struct timeval tv;
    struct timeval *ptv;

    for (;;) {
        // make local copy of read fd_set
        memcpy(&rfds, &s_readFds, sizeof(fd_set));
        if (-1 == calcNextTimeout(&tv)) {
            // no pending timers; block indefinitely
            ptv = NULL;
        } else {
            ptv = &tv;
        }
        n = select(s_nfds, &rfds, NULL, NULL, ptv);
        if (n < 0) {
            if (errno == EINTR) continue;
            RLOGE("ril_event: select error (%d)", errno);
            return;
        }

        // Check for timeouts
        processTimeouts();
        // Check for read-ready
        if (FD_ISSET(s_wakeupEvent.fd, &rfds)) {
            addToList(&s_wakeupEvent, &s_pendingList);
        }
        // Fire away
        firePending();
    }
}

void *startAsyncCmdHandlerLoop(void *param) {
    RIL_UNUSED_PARM(param);

    int ret;
    int filedes[2];

    // Initialize internal data struct
    MUTEX_INIT(s_listMutex);
    FD_ZERO(&s_readFds);
    initList(&s_timerList);
    initList(&s_pendingList);

    ret = pipe(filedes);
    if (ret < 0) {
        RLOGE("Error in pipe() errno:%d", errno);
        return NULL;
    }
    s_fdWakeupRead = filedes[0];
    s_fdWakeupWrite = filedes[1];

    fcntl(s_fdWakeupRead, F_SETFL, O_NONBLOCK);

    setRilEvent(&s_wakeupEvent, s_fdWakeupRead, processWakeupCallback, NULL);
    FD_SET(s_fdWakeupRead, &s_readFds);
    if (s_fdWakeupRead >= s_nfds) {
        s_nfds = s_fdWakeupRead + 1;
    }

    triggerEvLoop();

    asyncCmdHandlerLoop();

    return NULL;
}

// time out cmd request complete with RIL_E_GENERIC_FAILURE
static void userTimerCallback(void *param) {
    UserCallbackInfo *p_info;

    p_info = (UserCallbackInfo *)param;

    RLOGE("AT command wait for URC %s timeout", p_info->cmd);

#if (SIM_COUNT == 2)
    if (strcmp(p_info->cmd, "+SPTESTMODE:") == 0) {
        pthread_mutex_unlock(&s_radioPowerMutex[RIL_SOCKET_1]);
        pthread_mutex_unlock(&s_radioPowerMutex[RIL_SOCKET_2]);
    }
#endif
    RIL_onRequestComplete(p_info->token, RIL_E_GENERIC_FAILURE, NULL, 0);

    free(p_info->data);
    free(p_info);
}

// Add async cmd to s_timeList, timeout is in second
void addAsyncCmdList(RIL_SOCKET_ID socket_id, RIL_Token t, const char *cmd,
                        void *data, int timeout) {
    struct timeval relativeTime;
    UserCallbackInfo *p_info;
    p_info = (UserCallbackInfo *)malloc(sizeof(UserCallbackInfo));

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

    setRilEvent(&(p_info->event), -1, userTimerCallback, p_info);

    addRilEvent(&(p_info->event), &relativeTime);

    triggerEvLoop();
}

void removeAsyncCmdList(RIL_Token t, const char *cmd) {
    MUTEX_ACQUIRE(s_listMutex);

    ril_event *tev = s_timerList.next;
    ril_event *next;

    while (tev != &s_timerList) {
        next = tev->next;
        UserCallbackInfo *p_info = (UserCallbackInfo *)tev->param;
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
    UserCallbackInfo *p_info;
    p_info = (UserCallbackInfo *)(ev->param);
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

// walk list, see if URC's corresponding AT command is in s_timerList
void checkAndCompleteRequest(RIL_SOCKET_ID socket_id, const char *cmd,
                                 void *resp) {
    MUTEX_ACQUIRE(s_listMutex);
    ril_event *tev = s_timerList.next;
    ril_event *next;
    UserCallbackInfo *p_info = NULL;

    while (tev != &s_timerList) {
        next = tev->next;
        p_info = (UserCallbackInfo *)(tev->param);
        if (p_info->socket_id == socket_id && strcmp(cmd, p_info->cmd) == 0) {
            onRequestComplete(tev, resp);
            removeFromList(tev);
            break;
        }
        tev = next;
    }
    MUTEX_RELEASE(s_listMutex);
}
