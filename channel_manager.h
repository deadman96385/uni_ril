/**
 * channel_manager.h --- channel manager implementation for the phoneserver
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef CHANNEL_MANAGER_H_
#define CHANNEL_MANAGER_H_

#include "adapter.h"
#include "cmux.h"
#include "config.h"
#include "pty.h"
#include "receive_thread.h"
#include "send_thread.h"
#include "os_api.h"

#define PHONESERVER_UNUSED_PARM(a)  noopRemoveWarning((void *)&(a));

typedef struct {
    void (*channel_manager_free_cmux)(void *const chnmng,
                struct cmux_t *cmux);
    struct cmux_t *(*channel_manager_get_cmux)(void *const chnmng,
                    const AT_CMD_TYPE_T type, int block);
} chnmng_ops;

typedef struct {
    void *me;
    char itsBuffer[CHN_BUF_NUM][4 + SERIAL_BUFFSIZE];
    cmux_t itsCmux[MUX_NUM];
    struct chns_config_t *itschns_config;
    pty_t itsPty[PTY_NUM];
    struct receive_thread_t itsReceive_thread[MUX_NUM];
    struct send_thread_t itsSend_thread[PTY_NUM];
    sem get_mux_lock[SIM_COUNT];
    chnmng_ops *ops;
} channel_manager_t;

#define RIL_UNUSED_PARM(a)  noopRemoveWarning((void *)&(a));

pty_t *channel_manager_get_ind_pty(mux_type_t type);

void channel_manager_free_cmux(const cmux_t *cmux);

cmux_t *channel_manager_get_cmux(const AT_CMD_TYPE_T type, int block);

#endif  // CHANNEL_MANAGER_H_
