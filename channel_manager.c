/**
 * channel_manager.c --- channel manager implementation for the phoneserver
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
#include <pthread.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include "channel_manager.h"
#include "os_api.h"
#include "pty.h"
#include <time.h>
#include <sys/time.h>
#include <getopt.h>
#include <cutils/sockets.h>
#include <private/android_filesystem_config.h>
#include "cutils/properties.h"
#include <hardware_legacy/power.h>
#include <sys/prctl.h>
#include <sys/capability.h>

#define MODEM_TYPE         "ro.radio.modemtype"
#define PTY_NAME_LENGTH    32

#undef  PHS_LOGD
#define PHS_LOGD(x...)  ALOGD( x )

#define ANDROID_WAKE_LOCK_NAME "phoneserver-init"
#define PROP_BUILD_TYPE "ro.build.type"
#define SIG_POOL_SIZE 10

const char *s_modem = NULL;
int soc_client = -1;
struct channel_manager_t chnmng;
extern int s_isuserdebug;
extern sem sms_lock;

pthread_t s_tid_signal_process;
pthread_attr_t attr;

extern void ps_service_init(void);
extern bool isLTE();
struct chns_config_t single_chns_data = { .pty = {
    /* ind_pty */
    { .dev_str = "/dev/CHNPTY0", .index = 0, .type = IND, .prority = 1 },
    /* at_pty @{ */
    { .dev_str = "/dev/CHNPTY1", .index = 1, .type = AT, .prority = 1 },
    { .dev_str = "/dev/CHNPTY2", .index = 2, .type = AT, .prority = 1 },
    { .dev_str = "/dev/CHNPTY3", .index = 3, .type = AT, .prority = 1 },
    { .dev_str = "/dev/CHNPTY12", .index = 12, .type = AT, .prority = 1 },
    { .dev_str = "/dev/CHNPTY13", .index = 13, .type = AT, .prority = 1 },
    /* }@ */
    },
    .mux = {
    { .dev_str = "", .index = 0, .type = INDM, .prority = 20 },  /*misc_mux */
    { .dev_str = "", .index = 1, .type = SIMM, .prority = 20 },  /*sim_mux */
    { .dev_str = "", .index = 2, .type = SSM, .prority = 20 },  /*ss_mux */
    { .dev_str = "", .index = 3, .type = PBKM, .prority = 20 },  /* pbk_mux */
    { .dev_str = "", .index = 4, .type = STMM, .prority = 20 },  /* stm_mux */
    { .dev_str = "", .index = 5, .type = GSM, .prority = 20 },  /* gsm_mux */
    { .dev_str = "", .index = 6, .type = CSM, .prority = 20 },  /* cs_mux */
    { .dev_str = "", .index = 7, .type = PSM, .prority = 20 },  /* ps_mux */
    { .dev_str = "", .index = 8, .type = NWM, .prority = 20 },  /* nw_mux */
    { .dev_str = "", .index = 9, .type = STKM, .prority = 20 },  /* stk_mux */
    { .dev_str = "", .index = 10, .type = SMSM, .prority = 20 },  /* smsm_mux */
    }, };

struct chns_config_t multi_chns_data = { .pty = {
    /* ind_pty */
    { .dev_str = "/dev/CHNPTY0", .index = 0, .type = IND_SIM1, .prority = 1 },
    /* at_pty */
    { .dev_str = "/dev/CHNPTY1", .index = 1, .type = AT_SIM1, .prority = 1 },
    /* at_pty */
    { .dev_str = "/dev/CHNPTY2", .index = 2, .type = AT_SIM1, .prority = 1 },
    /* ind_pty */
    { .dev_str = "/dev/CHNPTY3", .index = 3, .type = IND_SIM2, .prority = 1 },
    /* at_pty */
    { .dev_str = "/dev/CHNPTY4", .index = 4, .type = AT_SIM2, .prority = 1 },
    /* at_pty */
    { .dev_str = "/dev/CHNPTY5", .index = 5, .type = AT_SIM2, .prority = 1 },
    /* ind_pty */
    { .dev_str = "/dev/CHNPTY6", .index = 6, .type = IND_SIM3, .prority = 1 },
    /* at_pty */
    { .dev_str = "/dev/CHNPTY7", .index = 7, .type = AT_SIM3, .prority = 1 },
    /* at_pty */
    { .dev_str = "/dev/CHNPTY8", .index = 8, .type = AT_SIM3, .prority = 1 },
    /* ind_pty */
    { .dev_str = "/dev/CHNPTY9", .index = 9, .type = IND_SIM4, .prority = 1 },
    /* at_pty */
    { .dev_str = "/dev/CHNPTY10", .index = 10, .type = AT_SIM4, .prority = 1 },
    /* at_pty */
    { .dev_str = "/dev/CHNPTY11", .index = 11, .type = AT_SIM4, .prority = 1 },
    /* at_pty */
    { .dev_str = "/dev/CHNPTY12", .index = 12, .type = AT_SIM1, .prority = 1 },
    /* at_pty */
    { .dev_str = "/dev/CHNPTY13", .index = 13, .type = AT_SIM1, .prority = 1 },
    /* at_pty */
    { .dev_str = "/dev/CHNPTY14", .index = 14, .type = AT_SIM2, .prority = 1 },
    },
    .mux = {
    { .dev_str = "", .index = 0, .type = INDM_SIM1, .prority = 20 },
    { .dev_str = "", .index = 1, .type = ATM1_SIM1, .prority = 20 },
    { .dev_str = "", .index = 2, .type = ATM2_SIM1, .prority = 20 },
    { .dev_str = "", .index = 3, .type = INDM_SIM2, .prority = 20 },
    { .dev_str = "", .index = 4, .type = ATM1_SIM2, .prority = 20 },
    { .dev_str = "", .index = 5, .type = ATM2_SIM2, .prority = 20 },
    { .dev_str = "", .index = 6, .type = INDM_SIM3, .prority = 20 },
    { .dev_str = "", .index = 7, .type = ATM1_SIM3, .prority = 20 },
    { .dev_str = "", .index = 8, .type = ATM2_SIM3, .prority = 20 },
    { .dev_str = "", .index = 9, .type = INDM_SIM4, .prority = 20 },
    { .dev_str = "", .index = 10, .type = ATM1_SIM4, .prority = 20 },
    { .dev_str = "", .index = 11, .type = ATM2_SIM4, .prority = 20 },
    }, };

struct chns_config_t single_chns_data_l = { .pty = {
    /* ind_pty */
    { .dev_str = "/dev/CHNPTY0", .index = 0, .type = IND, .prority = 1 },
    /* at_pty */
    { .dev_str = "/dev/CHNPTY1", .index = 1, .type = AT, .prority = 1 },
    /* at_pty */
    { .dev_str = "/dev/CHNPTY2", .index = 2, .type = AT, .prority = 1 },
    /* at_pty */
    { .dev_str = "/dev/CHNPTY3", .index = 3, .type = AT, .prority = 1 },
    /* at_pty */
    },
    .mux = {
    { .dev_str = "", .index = 0, .type = INDM, .prority = 20 },  /* misc_mux */
    { .dev_str = "", .index = 1, .type = SIMM, .prority = 20 },  /* sim_mux */
    { .dev_str = "", .index = 2, .type = SSM, .prority = 20 },  /* ss_mux */
    { .dev_str = "", .index = 3, .type = PBKM, .prority = 20 },  /* pbk_mux */
    { .dev_str = "", .index = 4, .type = STMM, .prority = 20 },  /* stm_mux */
    { .dev_str = "", .index = 5, .type = GSM, .prority = 20 },  /* gsm_mux */
    { .dev_str = "", .index = 6, .type = CSM, .prority = 20 },  /* cs_mux */
    { .dev_str = "", .index = 7, .type = PSM, .prority = 20 },  /* ps_mux */
    { .dev_str = "", .index = 8, .type = NWM, .prority = 20 },  /* nw_mux */
    { .dev_str = "", .index = 9, .type = STKM, .prority = 20 },  /* stk_mux */
    { .dev_str = "", .index = 10, .type = SMSM, .prority = 20 },  /* smsm_mux */
    }, };

/**
 * get_pty - get a pty master/slave pair and chown the slave side
 * Assumes slave_name points to >= 16 bytes of space.
 */
static int get_pty(int *master_fdp, int *slave_fdp, char *slave_name) {
    int i, mfd, sfd = -1;
    char pty_name[PTY_NAME_LENGTH] = {0};
    struct termios tios;

#ifdef TIOCGPTN
    /* Try the unix98 way first */
    mfd = open("/dev/ptmx", O_RDWR | O_NONBLOCK);
    if (mfd >= 0) {
        int ptn, rett = 0;

        rett = ioctl(mfd, TIOCGPTN, &ptn);
        PHS_LOGD("CHNMNG:/dev/ptmx opened rett=%x ptn=%d", rett, ptn);
        if (rett >= 0) {
            sprintf(pty_name, "/dev/pts/%d", ptn);
            if (chmod(pty_name, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) < 0) {
                PHS_LOGE(
                        "CHNMNG: Couldn't change pty slave %s's mode", pty_name);
            }
#ifdef TIOCSPTLCK
            ptn = 0;
            if ((rett = ioctl(mfd, TIOCSPTLCK, &ptn)) < 0) {
                PHS_LOGE(
                        "CHNMNG: Couldn't unlock pty slave %s rett=%x", pty_name, rett);
            }
#endif
            if ((sfd = open(pty_name, O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0) {
                PHS_LOGE(
                        "CHNMNG: Couldn't open pty slave %s sfd=%x", pty_name, sfd);
            }
        } else {
            PHS_LOGE("CHNMNG:fail to get pty number");
        }
    }
#endif /* TIOCGPTN */
    if (sfd < 0) {
        return 0;
    }
    strncpy(slave_name, pty_name, PTY_NAME_LENGTH);
    *master_fdp = mfd;
    if (slave_fdp != NULL) {
        *slave_fdp = sfd;
    }
    if (tcgetattr(sfd, &tios) == 0) {
        tios.c_cflag &= ~(CSIZE | CSTOPB | PARENB);
        tios.c_cflag |= CS8 | CREAD | CLOCAL;
        tios.c_iflag = IGNPAR;
        tios.c_oflag = 0;
        tios.c_lflag = 0;
        if (tcsetattr(sfd, TCSAFLUSH, &tios) < 0) {
            PHS_LOGE("CHNMNG couldn't set attributes on pty");
        }
    } else {
        PHS_LOGE("CHNMNG couldn't get attributes on pty");
    }
    PHS_LOGD("CHNMNG get pty OK mpty=%d", mfd);
    return 1;
}

// get master pty
static int create_communication_channel(char *slave_name) {
    int pty_master = -1;
    char pty_name[PTY_NAME_LENGTH] = {0};
    if (!get_pty(&pty_master, NULL, pty_name)) {
        PHS_LOGE("CHNMNG: Couldn't allocate pseudo-tty");
        return -1;
    }
    // create symlink for Application,link pty_name to slave_name
    unlink(slave_name);
    if (symlink(pty_name, slave_name) != 0) {
        PHS_LOGE(
                "CHNMNG: Can't create symbolic link %s -> %s\n", slave_name, pty_name);
    }
    return pty_master;
}

/*cmux0-9:
 cmux0  indicator,misc at; (P)
 cmux1  vt data
 cmux2-5 ppp

 cmux6  cs's AT (P)
 cmux7  ps's AT (P)
 cmux8  general (P)
 cmux9  cmmb
 cmux10  cp log

 AT_CMD_TYPE_CS,
 AT_CMD_TYPE_PS,
 AT_CMD_TYPE_SS,
 AT_CMD_TYPE_SMS,
 AT_CMD_TYPE_NW,
 AT_CMD_TYPE_PBK,
 AT_CMD_TYPE_SIM,
 AT_CMD_TYPE_GEN,
 AT_CMD_TYPE_INVALID
 */
static cmux_t *find_type_cmux(struct channel_manager_t *const me,
        mux_type type) {
    cmux_t *mux = NULL;
    int i;

    sem_lock(&me->get_mux_lock);
    if (isLTE()) {
        for (i = 0; i < LTE_MUX_CHN_NUM; i++) {
            PHS_LOGI("LTE find_type_cmux  i = %d, me->itsCmux[i].type = %d,\
                     me->itsCmux[i].in_use = %d\n", i, me->itsCmux[i].type,
                     me->itsCmux[i].in_use);
            if (me->itsCmux[i].type == (int)type
                    && me->itsCmux[i].in_use == 0) {
                PHS_LOGI("LTE find_type_cmux type = %d\n", type);
                mux = &me->itsCmux[i];
                mux->in_use = 1;
                break;
            }
        }
    } else {
        for (i = 0; i < MUX_NUM; i++) {
            PHS_LOGI("find_type_cmux  i = %d, me->itsCmux[i].type = %d, \
                    me->itsCmux[i].in_use = %d\n", i, me->itsCmux[i].type,
                    me->itsCmux[i].in_use);
            if (me->itsCmux[i].type == (int)type
                    && me->itsCmux[i].in_use == 0) {
                PHS_LOGI(" find_type_cmux type = %d\n", type);
                mux = &me->itsCmux[i];
                mux->in_use = 1;
                break;
            }
        }
    }
    sem_unlock(&me->get_mux_lock);
    return mux;
}

static cmux_t *single_find_cmux(struct channel_manager_t *const me,
        AT_CMD_TYPE_T type) {
    cmux_t *mux = NULL;

    switch (type) {
    case AT_CMD_TYPE_GEN:
        PHS_LOGI("TYPE: GSM\n");
        return find_type_cmux(me, GSM);

    case AT_CMD_TYPE_SS:
        PHS_LOGI("TYPE: SSM\n");
        return find_type_cmux(me, SSM);

    case AT_CMD_TYPE_SMS:
        PHS_LOGI("TYPE: SMSM\n");
        return find_type_cmux(me, SMSM);

    case AT_CMD_TYPE_SMST:
        PHS_LOGI("TYPE: SMSTM\n");
        return find_type_cmux(me, STKM);

    case AT_CMD_TYPE_NW:
        PHS_LOGI("TYPE:NWM\n");
        return find_type_cmux(me, NWM);

    case AT_CMD_TYPE_PBK:
        PHS_LOGI("TYPE:PBKM\n");
        return find_type_cmux(me, PBKM);

    case AT_CMD_TYPE_SIM:
        PHS_LOGI("TYPE:SIMM\n");
        return find_type_cmux(me, SIMM);

    case AT_CMD_TYPE_STK:
        PHS_LOGI("TYPE:STKM\n");
        return find_type_cmux(me, STKM);

    case AT_CMD_TYPE_CS:
        PHS_LOGI("TYPE:CSM\n");
        return find_type_cmux(me, CSM);

    case AT_CMD_TYPE_PS:
        PHS_LOGI("TYPE:PSM\n");
        return find_type_cmux(me, PSM);

    case AT_CMD_TYPE_STM:
        return find_type_cmux(me, STMM);

    default:
        PHS_LOGI(" CHNMNG single_find_cmux invalid cmd type \n");
        break;
    }
    return mux;
}

static cmux_t *multi_find_cmux(struct channel_manager_t *const me,
        AT_CMD_TYPE_T type) {
    cmux_t *mux = NULL;

    switch (type) {
        case AT_CMD_TYPE_SLOW1:
        case AT_CMD_TYPE_NORMAL1: {
            PHS_LOGI("TYPE: AT_CMD_TYPE_SLOW1 or NORMAL1\n");
            mux = find_type_cmux(me, ATM1_SIM1);
            if (mux == NULL)
                mux = find_type_cmux(me, ATM2_SIM1);
            break;
        }
        case AT_CMD_TYPE_SLOW2:
        case AT_CMD_TYPE_NORMAL2: {
            PHS_LOGI("TYPE: AT_CMD_TYPE_SLOW2 or NORMAL2\n");
            mux = find_type_cmux(me, ATM1_SIM2);
            if (mux == NULL)
                mux = find_type_cmux(me, ATM2_SIM2);
            break;
        }
        case AT_CMD_TYPE_SLOW3:
        case AT_CMD_TYPE_NORMAL3: {
            PHS_LOGI("TYPE: AT_CMD_TYPE_SLOW3 or NORMAL3\n");
            mux = find_type_cmux(me, ATM1_SIM3);
            if (mux == NULL)
                mux = find_type_cmux(me, ATM2_SIM3);
            break;
        }
        case AT_CMD_TYPE_SLOW4:
        case AT_CMD_TYPE_NORMAL4: {
            PHS_LOGI("TYPE: AT_CMD_TYPE_SLOW4 or NORMAL4\n");
            mux = find_type_cmux(me, ATM1_SIM4);
            if (mux == NULL)
                mux = find_type_cmux(me, ATM2_SIM4);
            break;
        }
        default:
            PHS_LOGI(" CHNMNG multi_find_cmux invalid cmd type \n");
            break;
    }
    return mux;
}

pty_t *find_pty(struct channel_manager_t *const me, pid_t tid) {
    int i = 0;
    int pty_chn_num;

    if (SIM_COUNT != 1) {
        pty_chn_num = MULTI_PTY_CHN_NUM;
    } else {
        pty_chn_num = SINGLE_PTY_CHN_NUM;
    }

    for (; i < pty_chn_num; i++) {
        if (me->itsSend_thread[i].tid == tid)
            break;
    }
    if (i >= pty_chn_num) {
        return NULL;
    }

    if (SIM_COUNT != 1) {
        return me->itsSend_thread[i].pty;
    } else {
        return &me->itsPty[i];
    }
}

static pty_t **single_get_mux_wait_array(struct channel_manager_t *const me,
        AT_CMD_TYPE_T type, int *array_size) {
    switch (type) {
        case AT_CMD_TYPE_GEN:
            *array_size = GSM_WAIT_NUM;
            return me->gsm_wait_array;
        case AT_CMD_TYPE_SS:
            *array_size = GSM_WAIT_NUM;
            return me->ssm_wait_array;
        case AT_CMD_TYPE_SMS:
            *array_size = GSM_WAIT_NUM;
            return me->smsm_wait_array;
        case AT_CMD_TYPE_SMST:
            *array_size = GSM_WAIT_NUM;
            return me->smstm_wait_array;
        case AT_CMD_TYPE_NW:
            *array_size = GSM_WAIT_NUM;
            return me->nwm_wait_array;
        case AT_CMD_TYPE_PBK:
            *array_size = GSM_WAIT_NUM;
            return me->pbkm_wait_array;
        case AT_CMD_TYPE_SIM:
            *array_size = GSM_WAIT_NUM;
            return me->simm_wait_array;
        case AT_CMD_TYPE_STK:
            *array_size = GSM_WAIT_NUM;
            return me->stkm_wait_array;
        case AT_CMD_TYPE_CS:
            *array_size = CSM_WAIT_NUM;
            return me->csm_wait_array;
        case AT_CMD_TYPE_PS:
            *array_size = PSM_WAIT_NUM;
            return me->psm_wait_array;
        case AT_CMD_TYPE_STM:
            *array_size = STMM_WAIT_NUM;
            return me->stm_wait_array;
        default:
            PHS_LOGI(" CHNMNG single_get_mux_wait_array invalid cmd type \n");
            return NULL;
    }
}

static pty_t **multi_get_mux_wait_array(struct channel_manager_t *const me,
        AT_CMD_TYPE_T type, int *array_size) {
    switch (type) {
        case AT_CMD_TYPE_SLOW1:
            *array_size = SLOW1_WAIT_NUM;
            return me->slow1_wait_array;
        case AT_CMD_TYPE_NORMAL1:
            *array_size = NORMAL1_WAIT_NUM;
            return me->normal1_wait_array;
        case AT_CMD_TYPE_SLOW2:
            *array_size = SLOW2_WAIT_NUM;
            return me->slow2_wait_array;
        case AT_CMD_TYPE_NORMAL2:
            *array_size = NORMAL2_WAIT_NUM;
            return me->normal2_wait_array;
        case AT_CMD_TYPE_SLOW3:
            *array_size = SLOW3_WAIT_NUM;
            return me->slow3_wait_array;
        case AT_CMD_TYPE_NORMAL3:
            *array_size = NORMAL3_WAIT_NUM;
            return me->normal3_wait_array;
        case AT_CMD_TYPE_SLOW4:
            *array_size = SLOW4_WAIT_NUM;
            return me->slow4_wait_array;
        case AT_CMD_TYPE_NORMAL4:
            *array_size = NORMAL4_WAIT_NUM;
            return me->normal4_wait_array;
        default:
            PHS_LOGE(" CHNMNG multi_get_mux_wait_array invalid cmd type ");
            return NULL;
    }
}

static void remove_wait_array(pty_t ** wait_array,
        pty_t __attribute__((unused)) *pty, int array_size) {
    int i;

    for (i = 0; i < (array_size - 1); i++) {
        wait_array[i] = wait_array[i + 1];
    }
    wait_array[array_size - 1] = NULL;
}
static int add_wait_array(pty_t ** wait_array, pty_t * pty, int array_size) {
    int i;
    for (i = 0; i < array_size; i++) {
        if (NULL == wait_array[i]) {
            wait_array[i] = pty;
            break;
        }
    }
    return i;
}

static void print_array(pty_t ** pty, int array_size) {
    int i;
    for (i = 0; i < array_size; i++) {
        if (pty[i] != NULL)
            PHS_LOGI("[%d]:%d ", i, pty[i]->tid);
    }
}

/*## operation getdfcmux(cmd_type) */
static cmux_t *chnmng_get_cmux(void *const chnmng, const AT_CMD_TYPE_T type,
        int block) {
    cmux_t *mux = NULL;
    pid_t tid;
    pty_t *pty = NULL;
    pty_t **wait_array = NULL;
    int array_size = 0;
    int not_found = 0;
    int index = 0;
    tid = gettid();
    struct channel_manager_t *me = (struct channel_manager_t *)chnmng;
    PHS_LOGI(
            "Send thread TID [%d] CHNMNG enter channel_manager_get_cmux ", tid);
    pty = find_pty(me, tid);

    // find free cmux
    PHS_LOGI("Before add wait array pty=%p", pty);
    if (SIM_COUNT != 1) {
        wait_array = multi_get_mux_wait_array(me, type, &array_size);
    } else {
        wait_array = single_get_mux_wait_array(me, type, &array_size);
    }
    if (block) {
        sem_lock(&me->array_lock);
        PHS_LOGI("Before add wait array");
        print_array(wait_array, array_size);
        index = add_wait_array(wait_array, pty, array_size);
        PHS_LOGI("After add wait array");
        print_array(wait_array, array_size);
        sem_unlock(&me->array_lock);
    } else {
        if (SIM_COUNT != 1) {
            mux = multi_find_cmux(me, type);
        } else {
            mux = single_find_cmux(me, type);
        }
    }

    while (mux == NULL && block) {
        if (index == 0) {
            if (SIM_COUNT != 1)
                mux = multi_find_cmux(me, type);
            else
                mux = single_find_cmux(me, type);
        }
        if (mux)
            break;
        PHS_LOGI(
                "Send thread TID [%d] CHNMNG block at channel_manager_get_cmux ", tid);
        sem_lock(&pty->get_mux_lock);
        if (mux == 0) {
            if (SIM_COUNT != 1)
                mux = multi_find_cmux(me, type);
            else
                mux = single_find_cmux(me, type);
        }
        sem_unlock(&pty->get_mux_lock);
    }
    if (mux) {
        mux->pty = pty;
        mux->cmd_type = type;
        PHS_LOGI(
                "Send thread TID [%d] CHNMNG Leave channel_manager_get_cmux:%s ", tid, mux->name);
    }
    return mux;
}

void select_send_thread_run(struct channel_manager_t *const me,
        AT_CMD_TYPE_T type) {
    int array_size;
    pty_t **wait_array = NULL;

    PHS_LOGI("Enter select_send_thread_run");
    if (SIM_COUNT != 1) {
        wait_array = multi_get_mux_wait_array(me, type, &array_size);
    } else {
        wait_array = single_get_mux_wait_array(me, type, &array_size);
    }

    pty_t *pty = wait_array[0];
    if (pty != NULL) {
        PHS_LOGI("select thread tid [%d] run...", wait_array[0]->tid);
        sem_unlock(&pty->get_mux_lock);
    }
    PHS_LOGI("Leave select_send_thread_run");
}

/*## operation free_cmux(cmux_struct) */
static void chnmng_free_cmux(void *const chnmng, struct cmux_t *cmux) {
    PHS_LOGI("CHNMNG enter channel_manager_free_cmux cmux=%s ", cmux->name);
    int array_size = 0;
    int type = cmux->cmd_type;
    struct channel_manager_t *me = (struct channel_manager_t *)chnmng;
    pty_t **wait_array = NULL;
    pty_t *pty = NULL;
    pid_t tid = gettid();

    if (SIM_COUNT != 1)
        wait_array = multi_get_mux_wait_array(me, type, &array_size);
    else
        wait_array = single_get_mux_wait_array(me, type, &array_size);

    sem_lock(&me->array_lock);
    pty = wait_array[0];
    if (pty != NULL) {
        PHS_LOGI("CHNMNG enter channel_manager_free_cmux pty=%p ", pty);
        if (pty == cmux->pty) {
            PHS_LOGI("[%d] Before remove wait array", tid);
            print_array(wait_array, array_size);
            remove_wait_array(wait_array, pty, array_size);
            PHS_LOGI("[%d] After remove wait array", tid);
            print_array(wait_array, array_size);
        } else {
            PHS_LOGI("free cmux error");
        }
    }
    sem_lock(&me->get_mux_lock);
    cmux->ops->cmux_free(cmux);
    select_send_thread_run(me, cmux->cmd_type);
    sem_unlock(&me->get_mux_lock);
    sem_unlock(&me->array_lock);
    PHS_LOGI("CHNMNG Leave channel_manager_free_cmux cmux=%s ", cmux->name);
}

/*## operation free_cmux(cmux_struct) */
void channel_manager_free_cmux(const struct cmux_t *cmux) {
    chnmng_free_cmux(chnmng.me, (struct cmux_t *)cmux);
}

/*## operation get_cmux(cmd_type) */
struct cmux_t *channel_manager_get_cmux(const AT_CMD_TYPE_T type, int block) {
    return chnmng_get_cmux(chnmng.me, type, block);
}

struct pty_t *channel_manager_get_default_ind_pty(void) {
    return &(chnmng.itsPty[0]);
}

struct pty_t *channel_manager_single_get_eng_ind_pty(void) {
    return &(chnmng.itsPty[4]);
}

struct pty_t *channel_manager_get_sim1_ind_pty(void) {
    return &(chnmng.itsPty[0]);
}

struct pty_t *channel_manager_get_sim2_ind_pty(void) {
    return &(chnmng.itsPty[3]);
}

struct pty_t *channel_manager_get_sim3_ind_pty(void) {
    return &(chnmng.itsPty[6]);
}

struct pty_t *channel_manager_get_sim4_ind_pty(void) {
    return &(chnmng.itsPty[9]);
}

struct pty_t *channel_manager_multi_get_eng_ind_pty(void) {
    return &(chnmng.itsPty[12]);
}

struct chnmng_ops chnmng_operaton = {.channel_manager_free_cmux =
        chnmng_free_cmux, .channel_manager_get_cmux = chnmng_get_cmux, };

void chnmng_buffer_Init(struct channel_manager_t *const me) {
    memset(me->itsBuffer, 0, sizeof(me->itsBuffer));
}

char *chnmng_find_buffer(struct channel_manager_t *const me) {
    int chn_num;
    char *ret = NULL;
    int i = 0;

    if (SIM_COUNT != 1) {
        chn_num = MULTI_CHN_NUM;
    } else {
        chn_num = SINGLE_CHN_NUM;
    }

    for (i = 0; i < chn_num; i++) {
        if (me->itsBuffer[i][0] == 0) {
            me->itsBuffer[i][0] = 1;
            ret = &me->itsBuffer[i][4];
            break;
        }
    }
    return ret;
}

/*## operation initialize all cmux objects*/
static void chnmng_cmux_Init(struct channel_manager_t *const me) {
    char prop[PROPERTY_VALUE_MAX] = {0};
    char MUX_SP_DEV[PTY_NAME_LENGTH] = {0};
    thread_sched_param sched;
    int tid = 0;
    int policy = 0;
    int index;
    char muxname[PTY_NAME_LENGTH] = {0};
    int i = 0;
    int fd;
    int phs_mux_num;
    int size;
    int chn_num = MUX_NUM;
    struct chns_config_t chns_data;
    struct termios ser_settings;

    snprintf(MUX_SP_DEV, sizeof(MUX_SP_DEV), "ro.modem.%s.tty", s_modem);
    if (!strcmp(s_modem, "t") || !strcmp(s_modem, "w")) {
        property_get(MUX_SP_DEV, prop, "/dev/ts0710mux");
    } else if (isLTE()) {
        property_get(MUX_SP_DEV, prop, "/dev/sdiomux");
        if (SIM_COUNT == 1) {
            chn_num = LTE_MUX_CHN_NUM;
        }
    }

    PHS_LOGD("cmux_Init: mux device is %s", prop);
    memset(me->itsCmux, 0, sizeof(struct cmux_t) * chn_num);

    for (i = 0; i < chn_num; i++) {
        snprintf(muxname, sizeof(muxname), "%s%d", prop, i);
        me->itsCmux[i].type = RESERVE;
        me->itsCmux[i].ops = cmux_get_operations();
        me->itsCmux[i].ops->cmux_free(&me->itsCmux[i]);
    }

    if (SIM_COUNT != 1) {
        phs_mux_num = MULTI_PHS_MUX_NUM;
        chns_data = multi_chns_data;
    } else {
        if (isLTE()) {
            phs_mux_num = LTE_MUX_CHN_NUM;
            chns_data = single_chns_data_l;
        } else {
            phs_mux_num = SINGLE_PHS_MUX_NUM;
            chns_data = single_chns_data;
        }
    }

    for (i = 0; i < phs_mux_num; i++) {
        index = chns_data.mux[i].index;
        me->itsCmux[i].buffer = chnmng_find_buffer(&chnmng);
        snprintf(muxname, sizeof(muxname), "%s%d", prop, index);
        size = sizeof(me->itsCmux[i].name);
        strncpy(me->itsCmux[i].name, muxname, size);
        me->itsCmux[i].name[size - 1] = '\0';
        me->itsCmux[i].type = chns_data.mux[i].type;
        me->itsCmux[i].muxfd = open(me->itsCmux[i].name, O_RDWR);
        if (me->itsCmux[i].muxfd < 0) {
            PHS_LOGE("Phoneserver exit: open mux:%s failed, errno = %d (%s)",
                    me->itsCmux[i].name, errno, strerror(errno));
            exit(1);
        }
        if (isatty(me->itsCmux[i].muxfd)) {
            tcgetattr(me->itsCmux[i].muxfd, &ser_settings);
            cfmakeraw(&ser_settings);
            tcsetattr(me->itsCmux[i].muxfd, TCSANOW, &ser_settings);
        }
        PHS_LOGD("CHNMNG: open mux:%s fd=%d", me->itsCmux[i].name,
                me->itsCmux[i].muxfd);
        sem_init(&me->itsReceive_thread[i].resp_cmd_lock, 0, 1);
        sem_init(&me->itsCmux[i].cmux_lock, 0, 0);
        cond_init(&me->itsCmux[i].cond_timeout, NULL);
        mutex_init(&me->itsCmux[i].mutex_timeout, NULL);
        me->itsReceive_thread[i].mux = &me->itsCmux[i];
        me->itsReceive_thread[i].ops = receive_thread_get_operations();
    }
    PHS_LOGD("CHNMNG: cmux_Init done");
}

/*## operation initialize all pty objects */
static void chnmng_pty_Init(struct channel_manager_t *const me) {
    int pty_chn_num;
    int i = 0;
    char *buff = 0;
    thread_sched_param sched;
    int tid = 0;
    int policy = 0;
    int index;
    struct chns_config_t chns_data;
    char pre_ptyname[PTY_NAME_LENGTH] = {0};
    char ptyname[PTY_NAME_LENGTH] = {0};
    int size;

    memset(&me->itsPty, 0, sizeof(struct pty_t) * MULTI_PTY_CHN_NUM);
    strcpy(pre_ptyname, "/dev/CHNPTY");
    if (SIM_COUNT != 1) {
        pty_chn_num = MULTI_PTY_CHN_NUM;
        chns_data = multi_chns_data;
    } else {
        if (isLTE()) {
            pty_chn_num = LTE_PTY_CHN_NUM;
            chns_data = single_chns_data_l;
        } else {
            pty_chn_num = SINGLE_PTY_CHN_NUM;
            chns_data = single_chns_data;
        }
    }

    /*set attris to default value */
    for (i = 0; i < pty_chn_num; i++) {
        me->itsPty[i].ops = pty_get_operations();
        sem_init(&me->itsPty[i].write_lock, 0, 1);
        sem_init(&me->itsPty[i].receive_lock, 0, 1);
        sem_init(&me->itsPty[i].get_mux_lock, 0, 0);
        me->itsPty[i].ops->pty_clear_wait_resp_flag(&me->itsPty[i]);
        me->itsPty[i].type = RESERVE;
    }
    for (i = 0; i < pty_chn_num; i++) {
        index = chns_data.pty[i].index;
        snprintf(ptyname, sizeof(ptyname), "%s%d", pre_ptyname, index);
        size = sizeof(me->itsPty[i].name);
        strncpy(me->itsPty[i].name, ptyname, size);
        me->itsPty[i].name[size - 1] = '\0';
        me->itsPty[i].type = chns_data.pty[i].type;
        me->itsPty[i].pty_fd = create_communication_channel(me->itsPty[i].name);
        buff = chnmng_find_buffer(&chnmng);
        if (buff == NULL) {
            PHS_LOGE("ERROR chnmng_pty_Init no buffer");
        }
        me->itsPty[i].buffer = buff;
        sem_init(&me->itsSend_thread[i].req_cmd_lock, 0, 1);
        me->itsSend_thread[i].pty = &me->itsPty[i];
        me->itsSend_thread[i].ops = send_thread_get_operations();
    }
}

static int least_squares(int y[]) {
    int i = 0;
    int x[SIG_POOL_SIZE] = {0};
    int sum_x = 0, sum_y = 0, sum_xy = 0, square = 0;
    float a = 0.0, b = 0.0, value = 0.0;

    for (i = 0; i < SIG_POOL_SIZE; ++i) {
        x[i] = i;
        sum_x += x[i];
        sum_y += y[i];
        sum_xy += x[i] * y[i];
        square += x[i] * x[i];
    }
    a = ((float)(sum_xy * SIG_POOL_SIZE - sum_x * sum_y))
            / (square * SIG_POOL_SIZE - sum_x * sum_x);
    b = ((float)sum_y) / SIG_POOL_SIZE - a * sum_x / SIG_POOL_SIZE;
    value = a * x[SIG_POOL_SIZE - 1] + b;
    return (int)(value) + ((int)(10 * value) % 10 < 5 ? 0 : 1);
}

static void *signal_process() {
    pty_t *ind_pty[SIM_COUNT] = {0};
    pty_t *ind_eng_pty[SIM_COUNT] = {0};
    char ind_str[MAX_AT_CMD_LEN];
    int sim_index = 0;
    int i = 0;
    int sample_rsrp_sim[SIM_COUNT][SIG_POOL_SIZE] = { {0}, {0}},
            sample_rscp_sim[SIM_COUNT][SIG_POOL_SIZE] = { {0}, {0}},
            sample_rxlev_sim[SIM_COUNT][SIG_POOL_SIZE] = { {0}, {0}},
            sample_rssi_sim[SIM_COUNT][SIG_POOL_SIZE] = { {0}, {0}};
    int *rsrp_array = NULL, *rscp_array = NULL, *rxlev_array, newSig;
    int upValue = -1, lowValue = -1;
    int rsrp_value, rscp_value, rxlev_value;
    // 3 means count 2G/3G/4G
    int nosigUpdate[SIM_COUNT], MAXSigCount = 3 * (SIG_POOL_SIZE - 1);
    extern int rxlev[], ber[], rscp[], ecno[], rsrq[], rsrp[];
    extern int rssi[], berr[];
    bool isLte = isLTE();

    if (!isLte) {
        MAXSigCount = SIG_POOL_SIZE - 1;
    }

    for (sim_index = 0; sim_index < SIM_COUNT; sim_index++) {
        if (SIM_COUNT != 1) {
            if (sim_index == 0) {
                ind_pty[sim_index] = adapter_get_ind_pty((mux_type)(INDM_SIM1));
                ind_eng_pty[sim_index] = adapter_multi_get_eng_ind_pty(
                        (mux_type)(INDM_SIM1));
            } else if (sim_index == 1) {
                ind_pty[sim_index] = adapter_get_ind_pty((mux_type)(INDM_SIM2));
                ind_eng_pty[sim_index] = adapter_multi_get_eng_ind_pty(
                        (mux_type)(INDM_SIM2));
            }
        } else {
            ind_pty[sim_index] = adapter_get_default_ind_pty();
            ind_eng_pty[sim_index] = adapter_single_get_eng_ind_pty();
        }
    }

    while (1) {
        for (sim_index = 0; sim_index < SIM_COUNT; sim_index++) {
            // compute the rsrp rscp or rssi
            if (!isLte) {
                rsrp_array = sample_rssi_sim[sim_index];
                rscp_array = NULL;
                rxlev_array = NULL;
                newSig = rssi[sim_index];
                upValue = 31;
                lowValue = 0;
            } else {
                rsrp_array = sample_rsrp_sim[sim_index];
                rscp_array = sample_rscp_sim[sim_index];
                rxlev_array = sample_rxlev_sim[sim_index];
                newSig = rsrp[sim_index];
                upValue = 140;
                lowValue = 44;
            }
            nosigUpdate[sim_index] = 0;
            for (i = 0; i < SIG_POOL_SIZE - 1; ++i) {
                if (rsrp_array[i] == rsrp_array[i + 1]) {
                    if (rsrp_array[i] == newSig) {
                        nosigUpdate[sim_index]++;
                    } else if (rsrp_array[i] == 0 || rsrp_array[i] < lowValue
                            || rsrp_array[i] > upValue) {
                        rsrp_array[i] = newSig;
                    }
                } else
                    rsrp_array[i] = rsrp_array[i + 1];

                if (rscp_array != NULL) {  // w/td mode no rscp
                    if (rscp_array[i] == rscp_array[i + 1]) {
                        if (rscp_array[i] == rscp[sim_index]) {
                            nosigUpdate[sim_index]++;
                        } else if (rscp_array[i] <= 0 || rscp_array[i] > 31) {
                            rscp_array[i] = rscp[sim_index];  // the first unsolicitied
                        }
                    } else
                        rscp_array[i] = rscp_array[i + 1];
                }

                if (rxlev_array != NULL) {  // w/td mode no rxlev
                    if ((rxlev_array[i] == rxlev_array[i + 1])) {
                        if (rxlev_array[i] == rxlev[sim_index]) {
                            nosigUpdate[sim_index]++;
                        } else if (rxlev_array[i] <= 0 || rxlev_array[i] > 31) {
                            rxlev_array[i] = rxlev[sim_index];
                        }
                    } else
                        rxlev_array[i] = rxlev_array[i + 1];
                }
            }
            PHS_LOGD(
                    "sim%d signal no update num=%d", sim_index, nosigUpdate[sim_index]);
            if (nosigUpdate[sim_index] == MAXSigCount) {
                continue;
            }

            rsrp_array[SIG_POOL_SIZE - 1] = newSig;
            if (rsrp_array[SIG_POOL_SIZE - 1]
                    >= rsrp_array[SIG_POOL_SIZE - 2]) {  // signal go up
                rsrp_value = newSig;
            } else {  // signal come down
                if (rsrp_array[SIG_POOL_SIZE - 1]
                        == rsrp_array[SIG_POOL_SIZE - 2] - 1) {
                    rsrp_value = newSig;
                } else {
                    rsrp_value = least_squares(rsrp_array);
                    if (rsrp_value < lowValue || rsrp_value > upValue
                            || rsrp_value < newSig) {  // if invalid, use current value
                        rsrp_value = newSig;
                    }
                    rsrp_array[SIG_POOL_SIZE - 1] = rsrp_value;
                }
            }

            if (rscp_array != NULL) {  // w/td mode no rscp
                rscp_array[SIG_POOL_SIZE - 1] = rscp[sim_index];
                if (rscp_array[SIG_POOL_SIZE - 1]
                        >= rscp_array[SIG_POOL_SIZE - 2]) {  // signal go up
                    rscp_value = rscp[sim_index];
                } else {  // signal come down
                    if (rscp_array[SIG_POOL_SIZE - 1]
                            == rscp_array[SIG_POOL_SIZE - 2] - 1) {
                        rscp_value = rscp[sim_index];
                    } else {
                        rscp_value = least_squares(rscp_array);
                        // if invalid, use current value
                        if (rscp_value < 0 || rscp_value > 31
                                || rscp_value < rscp[sim_index]) {
                            rscp_value = rscp[sim_index];
                        }
                        rscp_array[SIG_POOL_SIZE - 1] = rscp_value;
                    }
                }
            }
            if (rxlev_array != NULL) {  // w/td mode no rxlev
                rxlev_array[SIG_POOL_SIZE - 1] = rxlev[sim_index];
                if (rxlev_array[SIG_POOL_SIZE - 1]
                        >= rxlev_array[SIG_POOL_SIZE - 2]) {  // signal go up
                    rxlev_value = rxlev[sim_index];
                } else {  // signal come down
                    if (rxlev_array[SIG_POOL_SIZE - 1]
                            == rxlev_array[SIG_POOL_SIZE - 2] - 1) {
                        rxlev_value = rxlev[sim_index];
                    } else {
                        rxlev_value = least_squares(rxlev_array);
                        // if invalid, use current value
                        if (rxlev_value < 0 || rxlev_value > 31
                                || rxlev_value < rxlev[sim_index]) {
                            rxlev_value = rxlev[sim_index];
                        }
                        rxlev_array[SIG_POOL_SIZE - 1] = rxlev_value;
                    }
                }
            }

            if (rscp_array != NULL)  // l/tl/lf
                snprintf(ind_str, sizeof(ind_str),
                        "\r\n+CESQ: %d,%d,%d,%d,%d,%d\r\n", rxlev_value,
                        ber[sim_index], rscp_value, ecno[sim_index],
                        rsrq[sim_index], rsrp_value);
            else
                // w/t
                snprintf(ind_str, sizeof(ind_str), "\r\n+CSQ: %d,%d\r\n",
                        rsrp_value, ber[sim_index]);

            if (ind_pty[sim_index] && ind_pty[sim_index]->ops) {
                PHS_LOGD(
                        "rsrp[%d]=%d,rscp[%d]=%d,rxlev[%d]=%d ind_str= %s", sim_index, rsrp_value, sim_index, rscp_value, sim_index, rxlev_value, ind_str);
                ind_pty[sim_index]->ops->pty_write(ind_pty[sim_index], ind_str,
                        strlen(ind_str));
            } else {
                PHS_LOGE("ind string size > %d\n", MAX_AT_CMD_LEN);
            }

            if (ind_eng_pty[sim_index] != NULL) {
                if (ind_eng_pty[sim_index]->ops)
                    ind_eng_pty[sim_index]->ops->pty_write(
                            ind_eng_pty[sim_index], ind_str, strlen(ind_str));
            }
        }
        sleep(1);
    }
    return NULL;
}

void chnmng_start_thread(struct channel_manager_t *const me) {
    int phs_mux_num;
    int pty_chn_num;
    int i = 0;
    int tid = 0;
    int policy = 0;
    int ret = 0;
    thread_sched_param sched;
    struct chns_config_t chns_data;

    if (SIM_COUNT != 1) {
        phs_mux_num = MULTI_PHS_MUX_NUM;
        pty_chn_num = MULTI_PTY_CHN_NUM;
        chns_data = multi_chns_data;
    } else {
        if (isLTE()) {
            pty_chn_num = LTE_PTY_CHN_NUM;
            phs_mux_num = LTE_MUX_CHN_NUM;
            chns_data = single_chns_data_l;
        } else {
            phs_mux_num = SINGLE_PHS_MUX_NUM;
            pty_chn_num = SINGLE_PTY_CHN_NUM;
            chns_data = single_chns_data;
        }
        phs_mux_num = SINGLE_PHS_MUX_NUM;
        pty_chn_num = SINGLE_PTY_CHN_NUM;
        chns_data = single_chns_data;
    }

    for (i = 0; i < phs_mux_num; i++) {  // receive thread
        tid = thread_creat(&me->itsReceive_thread[i].thread, NULL,
                (void *)me->itsReceive_thread[i].ops->receive_data,
                (void *)&(me->itsReceive_thread[i]));
        if (tid < 0) {
            PHS_LOGE("ERROR chnmng_mux pthread_create \n ");
            break;
        }
        thread_getschedparam(me->itsReceive_thread[i].thread, &policy, &sched);
        if (policy != SCHED_OTHER) {
            sched.sched_priority = chns_data.mux[i].prority;
            thread_setschedparam(me->itsReceive_thread[i].thread, policy,
                    &sched);
        }
    }

    for (i = 0; i < pty_chn_num; i++) {  // send thread
        tid = thread_creat(&me->itsSend_thread[i].thread, NULL,
                (void *)me->itsSend_thread[i].ops->send_data,
                (void *)&(me->itsSend_thread[i]));
        if (tid < 0) {
            PHS_LOGE("ERROR chnmng_pty pthread_create");
            break;
        }
        thread_getschedparam(me->itsSend_thread[i].thread, &policy, &sched);
        if (policy != SCHED_OTHER) {
            sched.sched_priority = chns_data.pty[i].prority;
            thread_setschedparam(me->itsSend_thread[i].thread, policy, &sched);
        }
    }

    pthread_attr_init(&attr);
    ret = pthread_create(&s_tid_signal_process, &attr, signal_process, NULL);
    if (ret < 0) {
        PHS_LOGE("ERROR signal_process pthread_create");
    }
}

static void get_partial_wakeLock() {
    acquire_wake_lock(PARTIAL_WAKE_LOCK, ANDROID_WAKE_LOCK_NAME);
}

static void release_wakeLock() {
    release_wake_lock(ANDROID_WAKE_LOCK_NAME);
}

void switchUser(void) {
    struct __user_cap_header_struct header;
    struct __user_cap_data_struct cap;

    prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0);

    setuid(AID_SYSTEM);
    header.version = _LINUX_CAPABILITY_VERSION;
    header.pid = 0;
    cap.effective = cap.permitted = (1 << CAP_NET_ADMIN) | (1 << CAP_NET_RAW);
    cap.inheritable = 0;
    capset(&header, &cap);
    return;
}
/*## operation initialize all channel manager's objects  according to phone server configuration  file*/
static void channel_manager_init(void) {
    chnmng.me = &chnmng;
    chnmng.ops = &chnmng_operaton;
    sem_init(&chnmng.get_mux_lock, 0, 1);

    sem_init(&chnmng.array_lock, 0, 1);
    chnmng.block_count = 0;

    get_partial_wakeLock();
    chnmng_buffer_Init(chnmng.me);
    chnmng_cmux_Init(chnmng.me);
    chnmng_pty_Init(chnmng.me);

    /* switch user to system  */
    switchUser();

    chnmng_start_thread(chnmng.me);
    release_wakeLock();
}

void detect_at_no_response() {
    int retryTimes = 0;
    const char socket_name[64] = "modemd";

    soc_client = socket_local_client(socket_name,
            ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
    while (soc_client < 0 && retryTimes < 3) {
        retryTimes++;
        sleep(1);
        soc_client = socket_local_client(socket_name,
                ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
    }
    if (soc_client <= 0) {
        PHS_LOGE("%s: cannot create socket %s", __func__, socket_name);
        exit(-1);
    }
}

int main(int argc, char *argv[]) {
    char prop[PROPERTY_VALUE_MAX];
    char versionStr[PROPERTY_VALUE_MAX];
    pthread_t tid;
    int ret;

    PHS_LOGD("Phoneserver get modem type %s's value", MODEM_TYPE);
    PHS_LOGD("Phoneserver Compile date:%s, %s", __DATE__, __TIME__);

    if ((argc > 2) && 0 == strcmp(argv[1], "-m")) {
        s_modem = argv[2];
    }
    if (s_modem == NULL) {
        s_modem = (char *)malloc(PROPERTY_VALUE_MAX);
        property_get(MODEM_TYPE, (char*)s_modem, "");
        if (strcmp(s_modem, "") == 0) {
            PHS_LOGD("get modem type failed, exit!");
            free((char*)s_modem);
            exit(-1);
        }
    }

    PHS_LOGD("Current modem is %s, sim num is %d", s_modem, SIM_COUNT);

    sem_init(&sms_lock, 0, 1);

    ps_service_init();
    channel_manager_init();

    detect_at_no_response();

    while (1) {
        pause();
    }

}
