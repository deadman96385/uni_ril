/**
 * ril_sync_cmd_handler.h --- process the async cmd to sync, the request is
 *                            blocked until the UCR response
 *
 * Copyright (C) 2016 Spreadtrum Communications Inc.
 */

#ifndef RIL_ASYNC_CMD_HANDLER_H_
#define RIL_ASYNC_CMD_HANDLER_H_

#define MUTEX_ACQUIRE(mutex)    pthread_mutex_lock(&mutex)
#define MUTEX_RELEASE(mutex)    pthread_mutex_unlock(&mutex)
#define MUTEX_INIT(mutex)       pthread_mutex_init(&mutex, NULL)

typedef void (*ril_event_cb)(void *userdata);

typedef struct ril_event {
    struct ril_event *next;
    struct ril_event *prev;

    int fd;
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
} UserCallbackInfo;

typedef struct {
    const char *cmd;
    int len;
    void (*func)(RIL_Token t, void *data, void *resp);
} AsyncCmdInfo;

void *startAsyncCmdHandlerLoop(void *param);
void addAsyncCmdList(RIL_SOCKET_ID socket_id, RIL_Token t, const char *cmd,
                        void *data, int timeout);
void removeAsyncCmdList(RIL_Token t, const char *cmd);
void checkAndCompleteRequest(RIL_SOCKET_ID socket_id, const char *cmd,
                                 void *resp);

#endif  // RIL_ASYNC_CMD_HANDLER_H_
