/**
 * ril_sync_cmd_handler.h --- process the async cmd to sync, the request is
 *                            blocked until the UCR response
 *
 * Copyright (C) 2016 Spreadtrum Communications Inc.
 */

#ifndef TIME_EVENT_HANDLER_H_
#define TIME_EVENT_HANDLER_H_

#include "mbim_device_config.h"
#include "misc.h"
#include <../include/telephony/record_stream.h>

#define MUTEX_ACQUIRE(mutex)    pthread_mutex_lock(&mutex)
#define MUTEX_RELEASE(mutex)    pthread_mutex_unlock(&mutex)
#define MUTEX_INIT(mutex)       pthread_mutex_init(&mutex, NULL)

typedef void * RIL_Token;
typedef void (*ril_event_cb)(void *userdata);
typedef void (*RIL_TimedCallback)(void *param);

typedef struct ril_event {
    struct ril_event *next;
    struct ril_event *prev;

    int fd;
    int index;
    bool persist;
    struct timeval timeout;
    ril_event_cb func;
    void *param;
} ril_event;

typedef struct {
    void *data;
    const char *cmd;
    RIL_Token token;
    RIL_SOCKET_ID socket_id;
    ril_event event;
} CmdCallbackInfo;

typedef struct  {
    RIL_TimedCallback p_callback;
    void *userParam;
    ril_event event;
} UserCallbackInfo;

/* used as parameter by RIL_requestTimedCallback */
typedef struct {
    RIL_SOCKET_ID socket_id;
    void *para;
} CallbackPara;

typedef struct {
    const char *cmd;
    int len;
    void (*func)(RIL_Token t, void *data, void *resp);
} AsyncCmdInfo;

typedef struct SocketListenParam {
    int fdListen;
    int fdCommand;
    struct ril_event *commands_event;
    struct ril_event *listen_event;
    void (*processCommandsCallback)(void *param);
    RecordStream *p_rs;
} SocketListenParam;

extern SocketListenParam s_atciSocketParam;

void listenAtciSocket();

void rilEventAddWakeup          (struct ril_event *     ev);

void *startTimeEventHandlerLoop (void *                 param);

void addAsyncCmdList            (RIL_SOCKET_ID          socket_id,
                                 RIL_Token              t,
                                 const char *           cmd,
                                 void *                 data,
                                 int                    timeout);

void removeAsyncCmdList         (RIL_Token              t,
                                 const char *           cmd);

void checkAndCompleteRequest    (RIL_SOCKET_ID          socket_id,
                                 const char *           cmd,
                                 void *                 resp);

void requestTimedCallback       (RIL_TimedCallback      callback,
                                 void *                 param,
                                 const struct timeval * relativeTime);
#endif  // TIME_EVENT_HANDLER_H_
