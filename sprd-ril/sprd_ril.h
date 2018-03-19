/**
 * sprd-ril.h --- functions/struct/variables declaration and definition
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef SPRD_RIL_H_
#define SPRD_RIL_H_

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <alloca.h>
#include <getopt.h>
#include <sys/system_properties.h>
#include <sys/socket.h>
#include <cutils/sockets.h>
#include <unistd.h>
#include <stdbool.h>
#include <telephony/ril.h>

#include <utils/Log.h>
#include <time.h>
#include <signal.h>
#include <ctype.h>
#include <semaphore.h>
#include <cutils/properties.h>

#include "atchannel.h"
#include "at_tok.h"
#include "misc.h"

#define NEW_AT
#ifdef NEW_AT
#define AT_PREFIX "+SP"
#else
#define AT_PREFIX "^"
#endif

#define AT_COMMAND_LEN      128
#define ARRAY_SIZE          128  // cannot change the value
#define MAX_AT_RESPONSE     0x1000

#define NUM_ELEMS(x)        (sizeof(x) / sizeof(x[0]))
#define RIL_UNUSED_PARM(a)  noopRemoveWarning((void *)&(a));
#define MODEM_CONFIG_PROP   "persist.radio.modem.config"
#define PRIMARY_SIM_PROP    "persist.radio.primary.sim"

#define AT_RESPONSE_FREE(rsp)   \
{                                \
    at_response_free(rsp);       \
    rsp = NULL;                  \
}

#define FREEMEMORY(data)    \
{                            \
        free(data);          \
        data = NULL;         \
}

#ifdef RIL_SHLIB
extern const struct RIL_Env *s_rilEnv;
#define RIL_onRequestComplete(t, e, response, responselen) \
            s_rilEnv->OnRequestComplete(t, e, response, responselen)
#define RIL_requestTimedCallback(a, b, c) \
            s_rilEnv->RequestTimedCallback(a, b, c)

#if defined (ANDROID_MULTI_SIM)
#define RIL_onUnsolicitedResponse(a, b, c, d) \
            s_rilEnv->OnUnsolicitedResponse(a, b, c, d)
#else
#define RIL_onUnsolicitedResponse(a, b, c, d) \
            s_rilEnv->OnUnsolicitedResponse(a, b, c)
#endif
#endif

/* used as parameter by RIL_requestTimedCallback */
typedef struct {
    RIL_SOCKET_ID socket_id;
    void *para;
} CallbackPara;

extern bool s_isLTE;
extern int s_modemConfig;
extern int s_multiModeSim;
extern sem_t s_sem[SIM_COUNT];
extern RIL_RadioState s_radioState[SIM_COUNT];
extern const RIL_SOCKET_ID s_socketId[SIM_COUNT];
extern const char *s_modem;
extern const struct timeval TIMEVAL_CALLSTATEPOLL;
extern const struct timeval TIMEVAL_SIMPOLL;
extern struct ATChannels *s_ATChannels[MAX_AT_CHANNELS];

extern const char *requestToString(int request);
void *noopRemoveWarning(void *a);
void putChannel(int channel);
int isRadioOn(int channelID);
bool isVoLteEnable();
bool isLte(void);
bool isCMCC(void);
int getChannel(RIL_SOCKET_ID socket_id);
RIL_SOCKET_ID getSocketIdByChannelID(int channelID);
RIL_RadioState getRadioState(RIL_SOCKET_ID socket_id);
void setRadioState(int channelID, RIL_RadioState newState);
extern void getProperty(RIL_SOCKET_ID socket_id, const char *property,
                            char *value, const char *defaultVal);
extern void setProperty(RIL_SOCKET_ID socket_id, const char *property,
                            const char *value);

#endif  // SPRD_RIL_H_
