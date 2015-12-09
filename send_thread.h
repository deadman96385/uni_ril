/**
 * send_thread.h --- channel  implementation for the phoneserver
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef SEND_THREAD_H_

#define SEND_THREAD_H_

#include "os_api.h"
#include "pty.h"

struct send_thread_t {
    void *me;
    int prority;
    struct pty_t *pty;
    thread_t thread;
    sem_t req_cmd_lock;
    char *s_ATBufferCur;
    struct send_thread_ops *ops;
    char end_char;
    pid_t tid;
};

struct send_thread_ops {
    void (*send_thread_deliver_cmd_req)(struct send_thread_t * const me,
            char *cmd_str, int len);
    void *(*send_data)(struct send_thread_t * const me);
};
struct send_thread_ops *send_thread_get_operations(void);

#endif  // SEND_THREAD_H_
