/**
 * pty.h --- pty implementation for the phoneserver
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef PTY_H_
#define PTY_H_

#include "os_api.h"
#include "cmux.h"

struct pty_ops {
    int (*pty_clear_wait_resp_flag)(void * const pty);

    int (*pty_enter_edit_mode)(void * const pty, void *callback,
            uintptr_t userdata);

    int (*pty_read)(void * const pty, char *buf, int len);

    int (*pty_set_wait_resp_flag)(void * const pty);

    int (*pty_write)(void * const pty, char *buf, int len);
};

struct pty_t {
    void *me;
    char *buffer;                       /*for reading from channel pty */
    int (*edit_callback)(struct pty_t * pty, char *str, int len,
            uintptr_t userdata);        /* attribute edit_callback */
    int edit_mode;                      /* sms text edit_mode */
    uintptr_t user_data;
    int pty_fd;                         /* pty_fd for channel pty */
    pid_t tid;
    int used;
    int type;
    char name[30];
    int wait_resp;                      /* flag for wait_resp */
    sem_t write_lock;                   /* write_lock to avoid multi access */
    struct pty_ops *ops;
    cmux_t *mux;
    int cmgs_cmgw_set_result;
};

struct pty_ops *pty_get_operations(void);

#endif  // PTY_H_
