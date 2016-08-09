/**
 * cmux.c --- channel mux implementation for the phoneserver
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#include <sys/types.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include "os_api.h"
#include "cmux.h"

/* operation close() */
static int cmux_close(cmux_t *cmux) {
    if (cmux->muxfd > 0) {
        close(cmux->muxfd);
    }
    return 0;
}

/* operation deregist_cmd_callback() */
static int cmux_deregist_cmd_callback(cmux_t *const me) {
    PHS_LOGD("PS_CMUX cmux_deregist_cmd_callback cmux:%s\n", me->name);
    if (me->callback) {
        me->wait_resp = 0;
        me->callback = NULL;
        me->userdata = 0;
    } else {
        PHS_LOGE("PS_CMUX error enter cmux_deregist_cmd_callback cmux:%s\n",
                me->name);
    }
    return 0;
}

/* operation free() */
static int cmux_free(cmux_t *cmux) {
    cmux->in_use = 0;
    cmux->wait_resp = 0;
    return 0;
}

/*## operation read() */
static int cmux_read(cmux_t *const me, char *buf, int len) {
    int ret = 0;
    if (me->muxfd > 0) {
        while (len) {
            ret = read(me->muxfd, buf, len);
            if (ret > 0) {
                len -= ret;
                buf += ret;
            } else if (ret < 0) {
                PHS_LOGE("PS_PTY  ERROR read error:%s\n", me->name);
                return ret;
            }
        }
    }
    return ret;
}

/* operation regist_cmd_callback() */
static int cmux_regist_cmd_callback(cmux_t *const me,
        void *callback_fn, uintptr_t userdata) {
    PHS_LOGD("PS_CMUX cmux_regist_cmd_callback cmux:%s\n", me->name);
    if (me->callback) {
        PHS_LOGD("PS_CMUX   multi enter  cmux_regist_cmd_callback cmux:%s\n",
                me->name);
    } else if (callback_fn) {
        me->callback = callback_fn;
        me->userdata = userdata;
        me->wait_resp = 1;
    }
    return 0;
}

/* operation write() */
int cmux_write(cmux_t *const me, char *buf, int len) {
    int ret = 0;

    PHS_LOGD("PS_CMUX :%s cmux_write:%s:len=%d\n", me->name, buf, len);
    if (me->muxfd > 0) {
        while (len) {
            ret = write(me->muxfd, buf, len);
            if (ret > 0) {
                len -= ret;
                buf += ret;
            } else if (ret < 0) {
                PHS_LOGE("PS_PTY  ERROR write error:%s\n", me->name);
                return ret;
            }
        }
    }
    return ret;
}

struct cmux_ops mux_ops = {
.cmux_close = cmux_close,
.cmux_deregist_cmd_callback = cmux_deregist_cmd_callback,
.cmux_free = cmux_free,
.cmux_read = cmux_read,
.cmux_regist_cmd_callback = cmux_regist_cmd_callback,
.cmux_write = cmux_write,
};

struct cmux_ops *cmux_get_operations(void) {
    return &mux_ops;
}