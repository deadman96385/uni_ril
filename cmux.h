/**
 * cmux.h: cmux implementation for the phoneserver
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef CMUX_H_

#define CMUX_H_

#include "adapter.h"
#include "pty.h"
#include "os_api.h"

#define MUX_NAME_MAX_LEN 32

struct cmux_ops {
    int (*cmux_close)(struct cmux_t * const cmux);

    int (*cmux_deregist_cmd_callback)(struct cmux_t * const me);

    int (*cmux_free)(struct cmux_t * const cmux);

    int (*cmux_read)(struct cmux_t * const me, char *buf, int len);

    int (*cmux_regist_cmd_callback)(struct cmux_t * const me,
         void *callback_fn, uintptr_t userdata);

    int (*cmux_write)(struct cmux_t * const me, char *buf, int len);
};

struct  cmux_t {
    void *me;
    char *buffer;  // buffer for read tsmux
    uintptr_t userdata;
    int muxfd;
    int type;
    int cmd_type;
    int wait_resp;
    int in_use;
    int (*callback)(AT_CMD_RSP_T * resp_req, uintptr_t usdata);
    char name[MUX_NAME_MAX_LEN];
    struct cmux_ops *ops;
    mutex mutex_timeout;
    cond cond_timeout;
    int cp_blked;
    pty_t *pty;
};

struct cmux_ops *cmux_get_operations(void);

#endif  // CMUX_H_
