/**
 * receive_thread.h: channel mux implementation for the phoneserver
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef RECEIVE_THREAD_H_

#define RECEIVE_THREAD_H_
#include "os_api.h"
#include "cmux.h"

struct receive_thread_t {

    /***    User explicit entries    ***/
    void *me;
    int prority;
    char *s_ATBufferCur;
    char *buffer;
    sem_t resp_cmd_lock; /*## write_lock  to avoid multi access */
    struct cmux_t *mux; /*## attribute cmux */
    char end_char;
    thread_t thread;
    pid_t tid;
    struct receive_thread_ops *ops;
};

struct receive_thread_ops {

    /* Operations */

    /*## operation get_at_cmd() */
    void (*receive_thread_deliver_cmd_resp)(void * const me, char *cmd_str,
            int len);
    void *(*receive_data)(struct receive_thread_t * const me);
};
struct receive_thread_ops *receive_thread_get_operations(void);

#endif  // RECEIVE_THREAD_H_
