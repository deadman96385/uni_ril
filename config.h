/**
 * config.h --- configuration  implementation for the phoneserver
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#include <errno.h>

#ifdef SIM_COUNT_PHONESERVER_1
#define SIM_COUNT 1
#elif  SIM_COUNT_PHONESERVER_3
#define SIM_COUNT 3
#elif  SIM_COUNT_PHONESERVER_4
#define SIM_COUNT 4
#else
#define SIM_COUNT 2
#endif

#define SERIAL_BUFFSIZE                 (8 * 1024)  // channel buffer size
#define MAX_AT_RESPONSE                 (8 * 1024)

#define MAX_MUX_NUM                     16
#define MAX_PTY_NUM                     15

#if(SIM_COUNT == 1)
#define PTY_NUM                         4
#define INDPTY_NUM                      1
#define MUX_NUM                         4
#elif(SIM_COUNT == 2)
#define PTY_NUM                         6
#define INDPTY_NUM                      2
#define MUX_NUM                         6
#endif

// number of channel buffer
#define CHN_BUF_NUM                     (PTY_NUM + MUX_NUM)

typedef enum mux_type_t {
    /* single sim mode pty type @{ */
    AT,
    IND,
    /* }@ */

    /* multi sim mode pty type @{ */
    IND_SIM1,
    AT_SIM1,

    IND_SIM2,
    AT_SIM2,

    IND_SIM3,
    AT_SIM3,

    IND_SIM4,
    AT_SIM4,
    /* }@ */

    /* single sim mode mux type @{ */
    SINGLE_MUX_BASE = 20,
    INDM = SINGLE_MUX_BASE + 0,
    AT_MUX1 = SINGLE_MUX_BASE + 1,
    AT_MUX2 = SINGLE_MUX_BASE + 2,
    AT_MUX3 = SINGLE_MUX_BASE + 3,
    /* }@ */

    /* multi sim mode mux type @{ */
    MULTI_MUX_BASE = 40,
    INDM_SIM1 = MULTI_MUX_BASE + 0,
    ATM1_SIM1 = MULTI_MUX_BASE + 1,
    ATM2_SIM1 = MULTI_MUX_BASE + 2,

    INDM_SIM2 = MULTI_MUX_BASE + 3,
    ATM1_SIM2 = MULTI_MUX_BASE + 4,
    ATM2_SIM2 = MULTI_MUX_BASE + 5,

    INDM_SIM3 = MULTI_MUX_BASE + 6,
    ATM1_SIM3 = MULTI_MUX_BASE + 7,
    ATM2_SIM3 = MULTI_MUX_BASE + 8,

    INDM_SIM4 = MULTI_MUX_BASE + 9,
    ATM1_SIM4 = MULTI_MUX_BASE + 10,
    ATM2_SIM4 = MULTI_MUX_BASE + 11,
    /* }@ */

    SMSTM,
    VTM_SIM1, VTM_SIM2, VTM_SIM3, VTM_SIM4,
    STMAT, AUDAT, RESERVE,
} mux_type;

typedef struct {
    char *dev_str;  /* attribute dev_str , device node name */
    int index;
    mux_type type;  /* attribute type */
    int prority;    /* channel thread's prority */
} channel_config;

typedef struct {
    channel_config mux[MUX_NUM];
    channel_config pty[PTY_NUM];
} chns_config_t;

#endif  // CONFIG_H_
