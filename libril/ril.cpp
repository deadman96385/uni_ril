/*
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#define LOG_TAG "RILC"

#include <hardware_legacy/power.h>
#include <telephony/thread_pool.h>
#include <telephony/ril.h>
#include <telephony/ril_cdma_sms.h>
#include <cutils/sockets.h>
#include <cutils/jstring.h>
#include <telephony/record_stream.h>
#include <utils/Log.h>
#include <utils/SystemClock.h>
#include <pthread.h>
#include <binder/Parcel.h>
#include <cutils/jstring.h>
#include <sys/types.h>
#include <sys/limits.h>
#include <sys/system_properties.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <sys/un.h>
#include <assert.h>
#include <netinet/in.h>
#include <cutils/properties.h>
#include <RilSapSocket.h>

int s_multiModeSim = 0;

extern "C" void RIL_onRequestComplete(RIL_Token t, RIL_Errno e,
                                       void *response, size_t responselen);

extern "C" void
RIL_onRequestAck(RIL_Token t);

namespace android {

#define PHONE_PROCESS "radio"
#define BLUETOOTH_PROCESS "bluetooth"

#define SOCKET_NAME_RIL "rild"
#define SOCKET2_NAME_RIL "rild2"
#define SOCKET3_NAME_RIL "rild3"
#define SOCKET4_NAME_RIL "rild4"

#define SOCKET_NAME_RIL_DEBUG "rild-debug"

#define ANDROID_WAKE_LOCK_NAME "radio-interface"

#define ANDROID_WAKE_LOCK_SECS 0
#define ANDROID_WAKE_LOCK_USECS 200000

#define PROPERTY_RIL_IMPL "gsm.version.ril-impl"

#define PRIMARY_SIM_PROP        "persist.radio.primary.sim"
#define MODEM_WORKMODE_PROP     "persist.radio.modem.workmode"
#define MODEM_CONFIG_PROP       "persist.radio.modem.config"

// match with constant in RIL.java
#define MAX_COMMAND_BYTES (8 * 1024)

// Basically: memset buffers that the client library
// shouldn't be using anymore in an attempt to find
// memory usage issues sooner.
#define MEMSET_FREED 1

#define NUM_ELEMS(a)     (sizeof(a) / sizeof(a)[0])

#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* Constants for response types */
#define RESPONSE_SOLICITED 0
#define RESPONSE_UNSOLICITED 1
#define RESPONSE_SOLICITED_ACK 2
#define RESPONSE_SOLICITED_ACK_EXP 3
#define RESPONSE_UNSOLICITED_ACK_EXP 4

/* Negative values for private RIL errno's */
#define RIL_ERRNO_INVALID_RESPONSE -1
#define RIL_ERRNO_NO_MEMORY -12

// request, response, and unsolicited msg print macro
#define PRINTBUF_SIZE 1024

// Enable verbose logging
#define VDBG 0

// Enable RILC log
#define RILC_LOG 1

#if RILC_LOG
    #define startRequest           snprintf(printBuf, PRINTBUF_SIZE, "(")
    #define closeRequest           snprintf(printBuf, PRINTBUF_SIZE, "%s)", printBuf)
    #define printRequest(token, req)           \
            RLOGD("[%04d]> %s %s", token, requestToString(req), printBuf)

    #define startResponse           snprintf(printBuf, PRINTBUF_SIZE, "%s {", printBuf)
    #define closeResponse           snprintf(printBuf, PRINTBUF_SIZE, "%s}", printBuf)
    #define printResponse           RLOGD("%s", printBuf)

    #define clearPrintBuf           printBuf[0] = 0
//    #define removeLastChar          printBuf[strlen(printBuf)-1] = 0
    #define removeLastChar
    #define appendPrintBuf(x...)    snprintf(printBuf, PRINTBUF_SIZE, x)
#else
    #define startRequest
    #define closeRequest
    #define printRequest(token, req)
    #define startResponse
    #define closeResponse
    #define printResponse
    #define clearPrintBuf
    #define removeLastChar
    #define appendPrintBuf(x...)
#endif

enum WakeType {DONT_WAKE, WAKE_PARTIAL};

typedef struct {
    int requestNumber;
    void (*dispatchFunction)(Parcel &p, struct RequestInfo *pRI);
    int(*responseFunction)(Parcel &p, void *response, size_t responselen);
} CommandInfo;

typedef struct {
    int requestNumber;
    int (*responseFunction)(Parcel &p, void *response, size_t responselen);
    WakeType wakeType;
} UnsolResponseInfo;

typedef struct RequestInfo {
    int32_t token;  // this is not RIL_Token
    CommandInfo *pCI;
    RequestInfo *p_next;
    char cancelled;
    char local;  // responses to local commands donot go back to command process
    RIL_SOCKET_ID socket_id;
    RIL_SOCKET_TYPE socket_type;
    int wasAckSent;    // Indicates whether an ack was sent earlier
} RequestInfo;

typedef struct UserCallbackInfo {
    RIL_TimedCallback p_callback;
    void *userParam;
    struct ril_event event;
    struct UserCallbackInfo *p_next;
} UserCallbackInfo;

extern "C" const char *requestToString(int request);
extern "C" const char *failCauseToString(RIL_Errno);
extern "C" const char *callStateToString(RIL_CallState);
extern "C" const char *radioStateToString(RIL_RadioState);
extern "C" const char *rilSocketIdToString(RIL_SOCKET_ID socket_id);

extern "C" void getProperty(RIL_SOCKET_ID socket_id, const char *property,
                            char *value, const char *defaultVal);
extern "C" void setProperty(RIL_SOCKET_ID socket_id, const char *property,
                            const char *value);
extern "C" bool isPrimaryCardWorkMode(int workMode);

void initPrimarySim();
extern "C" char rild[MAX_SOCKET_NAME_LENGTH] = SOCKET_NAME_RIL;

/*******************************************************************/
RIL_RadioFunctions s_callbacks = {0, NULL, NULL, NULL, NULL, NULL};
static int s_registerCalled = 0;

static pthread_t s_tidDispatch;
static pthread_t s_tidReader;
static int s_started = 0;

static int s_fdDebug = -1;
static int s_fdDebugSocket2 = -1;

static int s_fdWakeupRead;
static int s_fdWakeupWrite;

int s_wakelock_count = 0;

static struct ril_event s_wakeupEvent;
static struct ril_event s_wakeTimeoutEvent;
static struct ril_event s_debugEvent;

static struct ril_event s_commandsEvent[SIM_COUNT];
static struct ril_event s_listenEvent[SIM_COUNT];

static SocketListenParam s_rilSocketParam[SIM_COUNT];
static RequestInfo *s_pendingRequests[SIM_COUNT];

static pthread_mutex_t s_pendingRequestsMutex[SIM_COUNT] = {
        PTHREAD_MUTEX_INITIALIZER
#if (SIM_COUNT >= 2)
        ,PTHREAD_MUTEX_INITIALIZER
#if (SIM_COUNT >= 3)
        ,PTHREAD_MUTEX_INITIALIZER
#if (SIM_COUNT >= 4)
        ,PTHREAD_MUTEX_INITIALIZER
#endif
#endif
#endif
        };

static pthread_mutex_t s_writeMutex[SIM_COUNT] = {
        PTHREAD_MUTEX_INITIALIZER
#if (SIM_COUNT >= 2)
        ,PTHREAD_MUTEX_INITIALIZER
#if (SIM_COUNT >= 3)
        ,PTHREAD_MUTEX_INITIALIZER
#if (SIM_COUNT >= 4)
        ,PTHREAD_MUTEX_INITIALIZER
#endif
#endif
#endif
};

static pthread_mutex_t s_wakeLockCountMutex = PTHREAD_MUTEX_INITIALIZER;

static const struct timeval TIMEVAL_WAKE_TIMEOUT =
{ANDROID_WAKE_LOCK_SECS, ANDROID_WAKE_LOCK_USECS};

static pthread_mutex_t s_startupMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_startupCond = PTHREAD_COND_INITIALIZER;

static UserCallbackInfo *s_lastWakeTimeoutInfo = NULL;

static void *s_lastNITZTimeData = NULL;
static size_t s_lastNITZTimeDataSize;

#if RILC_LOG
static char printBuf[PRINTBUF_SIZE];
#endif

/*******************************************************************/
/* ATCI socket */
#define SOCKET_NAME_ATCI "atci_socket"

static struct ril_event s_atciListenEvent;
static struct ril_event s_atciCommandsEvent;

static SocketListenParam s_atciSocketParam;
static pthread_mutex_t s_atciWriteMutex = PTHREAD_MUTEX_INITIALIZER;

static void listenCallbackEXT(int fd, short flags, void *param);

/*******************************************************************/
/* IMS socket */
#define SOCKET_NAME_IMS "ims_socket1"
#define SOCKET2_NAME_IMS "ims_socket2"
#define SOCKET3_NAME_IMS "ims_socket3"
#define SOCKET4_NAME_IMS "ims_socket4"

static struct ril_event s_imsListenEvent[SIM_COUNT];
static struct ril_event s_imsCommandsEvent[SIM_COUNT];

static SocketListenParam s_imsSocketParam[SIM_COUNT];
// since only primary card connected at one time, one writeMutex is OK
static pthread_mutex_t s_imsWriteMutex[SIM_COUNT] = {
        PTHREAD_MUTEX_INITIALIZER
#if (SIM_COUNT >= 2)
        ,PTHREAD_MUTEX_INITIALIZER
#if (SIM_COUNT >= 3)
        ,PTHREAD_MUTEX_INITIALIZER
#if (SIM_COUNT >= 4)
        ,PTHREAD_MUTEX_INITIALIZER
#endif
#endif
#endif
};
/*******************************************************************/
/* OEMSOCKET @{ */
#define SOCKET_NAME_OEM "oem_socket1"
#define SOCKET2_NAME_OEM "oem_socket2"

static struct ril_event s_oemListenEvent[SIM_COUNT];
static struct ril_event s_oemCommandsEvent[SIM_COUNT];
static SocketListenParam s_oemSocketParam[SIM_COUNT];
static pthread_mutex_t s_oemWriteMutex[SIM_COUNT] = {
        PTHREAD_MUTEX_INITIALIZER
#if (SIM_COUNT >= 2)
        ,PTHREAD_MUTEX_INITIALIZER
#if (SIM_COUNT >= 3)
        ,PTHREAD_MUTEX_INITIALIZER
#if (SIM_COUNT >= 4)
        ,PTHREAD_MUTEX_INITIALIZER
#endif
#endif
#endif
};
/* }@ */
/*******************************************************************/
/* Command thread local data */
typedef struct {
    RecordStream *p_rs;
    char *buffer;
    size_t buflen;
    RIL_SOCKET_TYPE socket_type;
    RIL_SOCKET_ID socket_id;
} RequestThreadData;

typedef struct ListNode {
    RequestThreadData *p_reqData;
    ListNode *next;
    ListNode *prev;
} ListNode;

typedef struct {
    pthread_t slowDispatchTid;
    pthread_t normalDispatchTid;
    pthread_t otherDispatchTid;

    threadpool_t *p_threadpool;

    ListNode *callReqList;
    ListNode *simReqList;
    ListNode *slowReqList;
    ListNode *otherReqList;

    pthread_mutex_t listMutex;
    pthread_mutex_t normalDispatchMutex;
    pthread_mutex_t slowDispatchMutex;
    pthread_mutex_t otherDispatchMutex;
    pthread_cond_t slowDispatchCond;
    pthread_cond_t normalDispatchCond;
    pthread_cond_t otherDispatchCond;
} RequestThreadInfo;

RequestThreadInfo s_requestThread[SIM_COUNT];

#if defined (ANDROID_MULTI_SIM)
ListNode *s_dataReqList = NULL;
pthread_mutex_t s_dataDispatchMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t s_dataDispatchCond  = PTHREAD_COND_INITIALIZER;
#endif

const RIL_SOCKET_ID s_socketId[SIM_COUNT] = {
        RIL_SOCKET_1
#if (SIM_COUNT >= 2)
        ,RIL_SOCKET_2
#if (SIM_COUNT >= 3)
        ,RIL_SOCKET_3
#endif
#if (SIM_COUNT >= 4)
        ,RIL_SOCKET_4
#endif
#endif
};

bool s_isUserdebug = false;

#define MAX_THR             3
#define BUILD_TYPE_PROP     "ro.build.type"

void list_init(ListNode **list);
void list_add_tail(ListNode *list, ListNode *item, RIL_SOCKET_ID socket_id);
void list_add_head(ListNode *list, ListNode *item, RIL_SOCKET_ID socket_id);
void list_remove(ListNode *item, RIL_SOCKET_ID socket_id);

/*******************************************************************/
static int sendResponse(Parcel &p, RIL_SOCKET_ID socket_id,
                         RIL_SOCKET_TYPE socket_type);

static void dispatchVoid(Parcel &p, RequestInfo *pRI);
static void dispatchString(Parcel &p, RequestInfo *pRI);
static void dispatchStrings(Parcel &p, RequestInfo *pRI);
static void dispatchStringsForSafety(Parcel &p, RequestInfo *pRI);
static void dispatchInts(Parcel &p, RequestInfo *pRI);
static void dispatchDial(Parcel &p, RequestInfo *pRI);
static void dispatchSIM_IO(Parcel &p, RequestInfo *pRI);
static void dispatchSIM_APDU(Parcel &p, RequestInfo *pRI);
static void dispatchCallForward(Parcel &p, RequestInfo *pRI);
static void dispatchRaw(Parcel &p, RequestInfo *pRI);
static void dispatchSmsWrite(Parcel &p, RequestInfo *pRI);
static void dispatchDataCall(Parcel &p, RequestInfo *pRI);
static void dispatchVoiceRadioTech(Parcel &p, RequestInfo *pRI);
static void dispatchSetInitialAttachApn(Parcel &p, RequestInfo *pRI);
static void dispatchCdmaSubscriptionSource(Parcel&p, RequestInfo *pRI);
static void dispatchCdmaSms(Parcel &p, RequestInfo *pRI);
static void dispatchImsSms(Parcel &p, RequestInfo *pRI);
static void dispatchImsCdmaSms(Parcel &p, RequestInfo *pRI, uint8_t retry, int32_t messageRef);
static void dispatchImsGsmSms(Parcel &p, RequestInfo *pRI, uint8_t retry, int32_t messageRef);
static void dispatchCdmaSmsAck(Parcel &p, RequestInfo *pRI);
static void dispatchGsmBrSmsCnf(Parcel &p, RequestInfo *pRI);
static void dispatchCdmaBrSmsCnf(Parcel &p, RequestInfo *pRI);
static void dispatchRilCdmaSmsWriteArgs(Parcel &p, RequestInfo *pRI);
static void dispatchNVReadItem(Parcel &p, RequestInfo *pRI);
static void dispatchNVWriteItem(Parcel &p, RequestInfo *pRI);
static void dispatchUiccSubscripton(Parcel &p, RequestInfo *pRI);
static void dispatchSimAuthentication(Parcel &p, RequestInfo *pRI);
static void dispatchDataProfile(Parcel &p, RequestInfo *pRI);
static void dispatchRadioCapability(Parcel &p, RequestInfo *pRI);
static void dispatchNetworkList(Parcel &p, RequestInfo *pRI);
/* IMS request response function @{ */
static void dispatchCallForwardUri(Parcel &p, RequestInfo *pRI);
static void dispatchVideoPhoneDial(Parcel& p, RequestInfo *pRI);
static void dispatchVideoPhoneCodec(Parcel& p, RequestInfo *pRI);
static void dispatchImsNetworkInfo(Parcel& p, RequestInfo *pRI);
/* }@ */

static int responseInts(Parcel &p, void *response, size_t responselen);
static int responseFailCause(Parcel &p, void *response, size_t responselen);
static int responseStrings(Parcel &p, void *response, size_t responselen);
static int responseString(Parcel &p, void *response, size_t responselen);
static int responseVoid(Parcel &p, void *response, size_t responselen);
static int responseCallList(Parcel &p, void *response, size_t responselen);
static int responseSMS(Parcel &p, void *response, size_t responselen);
static int responseSIM_IO(Parcel &p, void *response, size_t responselen);
static int responseCallForwards(Parcel &p, void *response, size_t responselen);
static int responseDataCallList(Parcel &p, void *response, size_t responselen);
static int responseSetupDataCall(Parcel &p, void *response, size_t responselen);
static int responseRaw(Parcel &p, void *response, size_t responselen);
static int responseSsn(Parcel &p, void *response, size_t responselen);
static int responseSimStatus(Parcel &p, void *response, size_t responselen);
static int responseGsmBrSmsCnf(Parcel &p, void *response, size_t responselen);
static int responseCdmaBrSmsCnf(Parcel &p, void *response, size_t responselen);
static int responseCdmaSms(Parcel &p, void *response, size_t responselen);
static int responseCellList(Parcel &p, void *response, size_t responselen);
static int responseCdmaInformationRecords(Parcel &p, void *response, size_t responselen);
static int responseRilSignalStrength(Parcel &p, void *response, size_t responselen);
static int responseCallRing(Parcel &p, void *response, size_t responselen);
static int responseCdmaSignalInfoRecord(Parcel &p, void *response, size_t responselen);
static int responseCdmaCallWaiting(Parcel &p, void *response, size_t responselen);
static int responseSimRefresh(Parcel &p, void *response, size_t responselen);
static int responseCellInfoList(Parcel &p, void *response, size_t responselen);
static int responseHardwareConfig(Parcel &p, void *response, size_t responselen);
static int responseDcRtInfo(Parcel &p, void *response, size_t responselen);
static int responseRadioCapability(Parcel &p, void *response, size_t responselen);
static int responseSSData(Parcel &p, void *response, size_t responselen);
static int responseLceStatus(Parcel &p, void *response, size_t responselen);
static int responseLceData(Parcel &p, void *response, size_t responselen);
static int responseActivityData(Parcel &p, void *response, size_t responselen);
/* IMS request response function @{ */
static int responseCallListIMS(Parcel &p, void *response, size_t responselen);
static int responseCallForwardsUri(Parcel &p, void *response, size_t responselen);
static int responseCMCCSI(Parcel &p, void *response, size_t responselen);
static int responseImsNetworkInfo(Parcel &p, void *response, size_t responselen);
/* }@ */

static int responseDSCI(Parcel &p, void *response, size_t responselen);

extern "C" void stripNumberFromSipAddress(const char *sipAddress, char *number, int len);
static int decodeVoiceRadioTechnology(RIL_RadioState radioState);
static int decodeCdmaSubscriptionSource(RIL_RadioState radioState);
static RIL_RadioState processRadioState(RIL_RadioState newRadioState);
static void grabPartialWakeLock();
static void releaseWakeLock();
static void wakeTimeoutCallback(void *);

static bool isServiceTypeCfQuery(RIL_SsServiceType serType, RIL_SsRequestType reqType);

static bool isDebuggable();

#ifdef RIL_SHLIB
#if defined(ANDROID_MULTI_SIM)
extern "C" void RIL_onUnsolicitedResponse(int unsolResponse, const void *data,
                                size_t datalen, RIL_SOCKET_ID socket_id);
#else
extern "C" void RIL_onUnsolicitedResponse(int unsolResponse, const void *data,
                                size_t datalen);
#endif
#endif

#if defined (ANDROID_MULTI_SIM)
#define RIL_UNSOL_RESPONSE(a, b, c, d) RIL_onUnsolicitedResponse((a), (b), (c), (d))
#define CALL_ONREQUEST(a, b, c, d, e) s_callbacks.onRequest((a), (b), (c), (d), (e))
#define CALL_ONSTATEREQUEST(a) s_callbacks.onStateRequest(a)
#else
#define RIL_UNSOL_RESPONSE(a, b, c, d) RIL_onUnsolicitedResponse((a), (b), (c))
#define CALL_ONREQUEST(a, b, c, d, e) s_callbacks.onRequest((a), (b), (c), (d))
#define CALL_ONSTATEREQUEST(a) s_callbacks.onStateRequest()
#endif

static UserCallbackInfo *internalRequestTimedCallback
    (RIL_TimedCallback callback, void *param, const struct timeval *relativeTime);

/* Index == requestNumber */
static CommandInfo s_commands[] = {
#include "ril_commands.h"
};

static UnsolResponseInfo s_unsolResponses[] = {
#include "ril_unsol_commands.h"
};

/* IMS Request @{ */
static CommandInfo s_ims_commands[] = {
#include "ril_ims_commands.h"
};

static UnsolResponseInfo s_ims_unsolResponses[] = {
#include "ril_ims_unsol_commands.h"
};
/* }@ */

/* OEMSOCKET Request @{ */
static CommandInfo s_oem_commands[] = {
#include "ril_oem_commands.h"
};

static UnsolResponseInfo s_oem_unsolResponses[] = {
#include "ril_oem_unsol_commands.h"
};
/* }@ */

/* For older RILs that do not support new commands RIL_REQUEST_VOICE_RADIO_TECH and
   RIL_UNSOL_VOICE_RADIO_TECH_CHANGED messages, decode the voice radio tech from
   radio state message and store it. Every time there is a change in Radio State
   check to see if voice radio tech changes and notify telephony
 */
int s_voiceRadioTech = -1;

/* For older RILs that do not support new commands RIL_REQUEST_GET_CDMA_SUBSCRIPTION_SOURCE
   and RIL_UNSOL_CDMA_SUBSCRIPTION_SOURCE_CHANGED messages, decode the subscription
   source from radio state and store it. Every time there is a change in Radio State
   check to see if subscription source changed and notify telephony
 */
int s_cdmaSubscriptionSource = -1;

/* For older RILs that do not send RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED, decode the
   SIM/RUIM state from radio state and store it. Every time there is a change in Radio State,
   check to see if SIM/RUIM status changed and notify telephony
 */
int s_simRuimStatus = -1;

static char *RIL_getRilSocketName() {
    return rild;
}

extern "C" void RIL_setRilSocketName(const char *s) {
    strncpy(rild, s, MAX_SOCKET_NAME_LENGTH);
}

static char *strdupReadString(Parcel &p) {
    size_t stringlen;
    const char16_t *s16;

    s16 = p.readString16Inplace(&stringlen);

    return strndup16to8(s16, stringlen);
}

static status_t readStringFromParcelInplace(Parcel &p, char *str, size_t maxLen) {
    size_t s16Len;
    const char16_t *s16;

    s16 = p.readString16Inplace(&s16Len);
    if (s16 == NULL) {
        return NO_MEMORY;
    }
    size_t strLen = strnlen16to8(s16, s16Len);
    if ((strLen + 1) > maxLen) {
        return NO_MEMORY;
    }
    if (strncpy16to8(str, s16, strLen) == NULL) {
        return NO_MEMORY;
    } else {
        return NO_ERROR;
    }
}

static void writeStringToParcel(Parcel &p, const char *s) {
    char16_t *s16;
    size_t s16_len;
    s16 = strdup8to16(s, &s16_len);
    p.writeString16(s16, s16_len);
    free(s16);
}

static void memsetString(char *s) {
    if (s != NULL) {
        memset(s, 0, strlen(s));
    }
}

void nullParcelReleaseFunction(const uint8_t *data __unused,
        size_t dataSize __unused, const size_t *objects __unused,
        size_t objectsSize __unused, void *cookie __unused) {
    // do nothing -- the data reference lives longer than the Parcel object
}

/**
 * To be called from dispatch thread
 * Issue a single local request, ensuring that the response
 * is not sent back up to the command process
 */
static void issueLocalRequest(int request, void *data,
        int len, RIL_SOCKET_ID socket_id) {
    RequestInfo *pRI;
    int ret;
    /* Hook for current context */
    /* pendingRequestsMutextHook refer to &s_pendingRequestsMutex */
    pthread_mutex_t *pendingRequestsMutexHook = &s_pendingRequestsMutex[socket_id];
    /* pendingRequestsHook refer to &s_pendingRequests */
    RequestInfo **pendingRequestsHook = &s_pendingRequests[socket_id];

    pRI = (RequestInfo *)calloc(1, sizeof(RequestInfo));
    if (pRI == NULL) {
        RLOGE("Memory allocation failed for request %s", requestToString(request));
        return;
    }

    pRI->local = 1;
    pRI->token = 0xffffffff;        // token is not used in this context
    pRI->pCI = &(s_commands[request]);
    pRI->socket_id = socket_id;

    ret = pthread_mutex_lock(pendingRequestsMutexHook);
    assert(ret == 0);

    pRI->p_next = *pendingRequestsHook;
    *pendingRequestsHook = pRI;

    ret = pthread_mutex_unlock(pendingRequestsMutexHook);
    assert(ret == 0);

    RLOGD("C[locl]> %s", requestToString(request));

    CALL_ONREQUEST(request, data, len, pRI, pRI->socket_id);
}

static int processCommandBuffer(void *buffer, size_t buflen,
        RIL_SOCKET_ID socket_id, RIL_SOCKET_TYPE socket_type) {
    Parcel p;
    status_t status;
    int32_t request;
    int32_t token;
    int32_t simId;
    RequestInfo *pRI;
    RIL_SOCKET_ID soc_id = socket_id;
    int ret;
    /* Hook for current context */
    /* pendingRequestsMutextHook refer to &s_pendingRequestsMutex */
    pthread_mutex_t *pendingRequestsMutexHook = &s_pendingRequestsMutex[socket_id];
    /* pendingRequestsHook refer to &s_pendingRequests */
    RequestInfo **pendingRequestsHook = &s_pendingRequests[socket_id];

    p.setData((uint8_t *)buffer, buflen);

    // status checked at end
    status = p.readInt32(&request);
    status = p.readInt32(&token);

    if (status != NO_ERROR) {
        RLOGE("invalid request block");
        return 0;
    }

    // Received an Ack for the previous result sent to RIL.java,
    // so release wakelock and exit
    if (request == RIL_RESPONSE_ACKNOWLEDGEMENT) {
        releaseWakeLock();
        return 0;
    }

    if (request == RIL_REQUEST_OEM_HOOK_STRINGS && socket_type == RIL_ATCI_SOCKET) {
        status = p.readInt32(&simId);
        if (status != NO_ERROR || simId < RIL_SOCKET_1
                || simId >= RIL_SOCKET_NUM) {
                RLOGE("invalid simId");
                return 0;
        }
        soc_id = (RIL_SOCKET_ID)simId;
        RLOGD("simId = %d", soc_id);
    } else if (request < 1 ||
               (request > RIL_REQUEST_LAST && request < RIL_IMS_REQUEST_BASE) ||
               (request > RIL_IMS_REQUEST_LAST && request < RIL_EXT_REQUEST_BASE) ||
               (request > RIL_EXT_REQUEST_LAST)) {
        Parcel pErr;
        RLOGE("unsupported request code %d token %d", request, token);
        // FIXME this should perhaps return a response
        pErr.writeInt32(RESPONSE_SOLICITED);
        pErr.writeInt32(token);
        pErr.writeInt32(RIL_E_GENERIC_FAILURE);

        sendResponse(pErr, socket_id, socket_type);
        return 0;
    }

    pRI = (RequestInfo *)calloc(1, sizeof(RequestInfo));
    if (pRI == NULL) {
        RLOGE("Memory allocation failed for request %s", requestToString(request));
        return 0;
    }

    if (request > 0 && request <= RIL_REQUEST_LAST) {
        pRI->pCI = &(s_commands[request]);
    } else if (request > RIL_IMS_REQUEST_BASE && request <= RIL_IMS_REQUEST_LAST) {
        request = request - RIL_IMS_REQUEST_BASE;
        pRI->pCI = &(s_ims_commands[request]);
    } else if (request > RIL_EXT_REQUEST_BASE && request <= RIL_EXT_REQUEST_LAST) {
        request = request - RIL_EXT_REQUEST_BASE;
        pRI->pCI = &(s_oem_commands[request]);
    }

    pRI->token = token;
    pRI->socket_id = soc_id;
    pRI->socket_type = socket_type;

    ret = pthread_mutex_lock(pendingRequestsMutexHook);
    assert(ret == 0);

    pRI->p_next = *pendingRequestsHook;
    *pendingRequestsHook = pRI;

    ret = pthread_mutex_unlock(pendingRequestsMutexHook);
    assert(ret == 0);

//  sLastDispatchedToken = token;

    pRI->pCI->dispatchFunction(p, pRI);

    return 0;
}

static void invalidCommandBlock(RequestInfo *pRI) {
    RLOGE("invalid command block for token %d request %s",
                pRI->token, requestToString(pRI->pCI->requestNumber));
}

/** Callee expects NULL */
static void dispatchVoid(Parcel& p __unused, RequestInfo *pRI) {
    clearPrintBuf;
    printRequest(pRI->token, pRI->pCI->requestNumber);
    CALL_ONREQUEST(pRI->pCI->requestNumber, NULL, 0, pRI, pRI->socket_id);
}

/** Callee expects const char **/
static void dispatchString(Parcel &p, RequestInfo *pRI) {
    status_t status;
    size_t datalen;
    size_t stringlen;
    char *string8 = NULL;

    string8 = strdupReadString(p);

    startRequest;
    appendPrintBuf("%s%s", printBuf, string8);
    closeRequest;
    printRequest(pRI->token, pRI->pCI->requestNumber);

    CALL_ONREQUEST(pRI->pCI->requestNumber, string8, strlen(string8) + 1, pRI,
                   pRI->socket_id);

#ifdef MEMSET_FREED
    memsetString(string8);
#endif

    free(string8);
    return;
}

/** Callee expects const char ** */
static void dispatchStrings(Parcel &p, RequestInfo *pRI) {
    int32_t countStrings;
    status_t status;
    size_t datalen;
    char **pStrings;

    status = p.readInt32(&countStrings);

    if (status != NO_ERROR) {
        goto invalid;
    }

    startRequest;
    if (countStrings == 0) {
        // just some non-null pointer
        pStrings = (char **)calloc(1, sizeof(char *));
        if (pStrings == NULL) {
            RLOGE("Memory allocation failed for request %s",
                    requestToString(pRI->pCI->requestNumber));
            closeRequest;
            return;
        }

        datalen = 0;
    } else if (countStrings < 0) {
        pStrings = NULL;
        datalen = 0;
    } else {
        datalen = sizeof(char *) * countStrings;

        pStrings = (char **)calloc(countStrings, sizeof(char *));
        if (pStrings == NULL) {
            RLOGE("Memory allocation failed for request %s",
                    requestToString(pRI->pCI->requestNumber));
            closeRequest;
            return;
        }

        for (int i = 0; i < countStrings; i++) {
            pStrings[i] = strdupReadString(p);
            appendPrintBuf("%s%s,", printBuf, pStrings[i]);
        }
    }
    removeLastChar;
    closeRequest;
    printRequest(pRI->token, pRI->pCI->requestNumber);

    CALL_ONREQUEST(pRI->pCI->requestNumber, pStrings, datalen, pRI,
                   pRI->socket_id);

    if (pStrings != NULL) {
        for (int i = 0; i < countStrings; i++) {
#ifdef MEMSET_FREED
            memsetString(pStrings[i]);
#endif
            free(pStrings[i]);
        }

#ifdef MEMSET_FREED
        memset(pStrings, 0, datalen);
#endif
        free(pStrings);
    }

    return;
invalid:
    invalidCommandBlock(pRI);
    return;
}

static void dispatchStringsForSafety(Parcel &p, RequestInfo *pRI) {
    int32_t countStrings;
    status_t status;
    size_t datalen;
    char **pStrings;

    status = p.readInt32(&countStrings);

    if (status != NO_ERROR) {
        goto invalid;
    }

    startRequest;
    if (countStrings == 0) {
        // just some non-null pointer
        pStrings = (char **)calloc(1, sizeof(char *));
        if (pStrings == NULL) {
            RLOGE("Memory allocation failed for request %s",
                    requestToString(pRI->pCI->requestNumber));
            closeRequest;
            return;
        }

        datalen = 0;
    } else if (countStrings < 0) {
        pStrings = NULL;
        datalen = 0;
    } else {
        datalen = sizeof(char *) * countStrings;

        pStrings = (char **)calloc(countStrings, sizeof(char *));
        if (pStrings == NULL) {
            RLOGE("Memory allocation failed for request %s",
                    requestToString(pRI->pCI->requestNumber));
            closeRequest;
            return;
        }

        for (int i = 0; i < countStrings; i++) {
            pStrings[i] = strdupReadString(p);
            if (s_isUserdebug) {
                appendPrintBuf("%s%s,", printBuf, pStrings[i]);
            }
        }
    }
    removeLastChar;
    closeRequest;
    printRequest(pRI->token, pRI->pCI->requestNumber);

    CALL_ONREQUEST(pRI->pCI->requestNumber, pStrings, datalen, pRI,
                   pRI->socket_id);

    if (pStrings != NULL) {
        for (int i = 0; i < countStrings; i++) {
#ifdef MEMSET_FREED
            memsetString(pStrings[i]);
#endif
            free(pStrings[i]);
        }

#ifdef MEMSET_FREED
        memset(pStrings, 0, datalen);
#endif
        free(pStrings);
    }

    return;
invalid:
    invalidCommandBlock(pRI);
    return;
}

/** Callee expects const int **/
static void dispatchInts(Parcel &p, RequestInfo *pRI) {
    int32_t count;
    status_t status;
    size_t datalen;
    int *pInts;

    status = p.readInt32(&count);

    if (status != NO_ERROR || count <= 0) {
        goto invalid;
    }

    datalen = sizeof(int) * count;
    pInts = (int *)calloc(count, sizeof(int));
    if (pInts == NULL) {
        RLOGE("Memory allocation failed %s",
                 requestToString(pRI->pCI->requestNumber));
        return;
    }

    startRequest;
    for (int i = 0; i < count; i++) {
        int32_t t;

        status = p.readInt32(&t);
        pInts[i] = (int)t;
        appendPrintBuf("%s%d,", printBuf, t);

        if (status != NO_ERROR) {
            free(pInts);
            goto invalid;
        }
    }
    removeLastChar;
    closeRequest;
    printRequest(pRI->token, pRI->pCI->requestNumber);

    CALL_ONREQUEST(pRI->pCI->requestNumber, const_cast<int *>(pInts),
            datalen, pRI, pRI->socket_id);

#ifdef MEMSET_FREED
    memset(pInts, 0, datalen);
#endif
    free(pInts);
    return;
invalid:
    invalidCommandBlock(pRI);
    return;
}


/**
 * Callee expects const RIL_SMS_WriteArgs *
 * Payload is:
 *   int32_t status
 *   String pdu
 */
static void dispatchSmsWrite(Parcel &p, RequestInfo *pRI) {
    RIL_SMS_WriteArgs args;
    int32_t t;
    status_t status;

    memset (&args, 0, sizeof(args));

    status = p.readInt32(&t);
    args.status = (int)t;

    args.pdu = strdupReadString(p);

    if (status != NO_ERROR || args.pdu == NULL) {
        goto invalid;
    }

    args.smsc = strdupReadString(p);

    startRequest;
    appendPrintBuf("%s%d,%s,smsc=%s", printBuf, args.status,
        (char *)args.pdu,  (char *)args.smsc);
    closeRequest;
    printRequest(pRI->token, pRI->pCI->requestNumber);

    CALL_ONREQUEST(pRI->pCI->requestNumber, &args, sizeof(args), pRI,
                   pRI->socket_id);

#ifdef MEMSET_FREED
    memsetString(args.pdu);
#endif

    free(args.pdu);
    free(args.smsc);

#ifdef MEMSET_FREED
    memset(&args, 0, sizeof(args));
#endif

    return;
invalid:
    invalidCommandBlock(pRI);
    return;
}

/**
 * Callee expects const RIL_Dial *
 * Payload is:
 *   String address
 *   int32_t clir
 */
static void dispatchDial(Parcel &p, RequestInfo *pRI) {
    RIL_Dial dial;
    RIL_UUS_Info uusInfo;
    int32_t sizeOfDial;
    int32_t t;
    int32_t uusPresent;
    status_t status;

    memset(&dial, 0, sizeof(dial));

    dial.address = strdupReadString(p);

    status = p.readInt32(&t);
    dial.clir = (int)t;

    if (status != NO_ERROR || dial.address == NULL) {
        goto invalid;
    }

    if (s_callbacks.version < 3) {  // Remove when partners upgrade to version 3
        uusPresent = 0;
        sizeOfDial = sizeof(dial) - sizeof(RIL_UUS_Info *);
    } else {
        status = p.readInt32(&uusPresent);

        if (status != NO_ERROR) {
            goto invalid;
        }

        if (uusPresent == 0) {
            dial.uusInfo = NULL;
        } else {
            int32_t len;

            memset(&uusInfo, 0, sizeof(RIL_UUS_Info));

            status = p.readInt32(&t);
            uusInfo.uusType = (RIL_UUS_Type)t;

            status = p.readInt32(&t);
            uusInfo.uusDcs = (RIL_UUS_DCS)t;

            status = p.readInt32(&len);
            if (status != NO_ERROR) {
                goto invalid;
            }

            // The java code writes -1 for null arrays
            if (((int)len) == -1) {
                uusInfo.uusData = NULL;
                len = 0;
            } else {
                uusInfo.uusData = (char *)p.readInplace(len);
            }

            uusInfo.uusLength = len;
            dial.uusInfo = &uusInfo;
        }
        sizeOfDial = sizeof(dial);
    }

    startRequest;
    if (s_isUserdebug) {
        appendPrintBuf("%snum=%s,clir=%d", printBuf, dial.address, dial.clir);
    } else {
        appendPrintBuf("%snum=****,clir=%d", printBuf, dial.clir);
    }
    if (uusPresent) {
        appendPrintBuf("%s,uusType=%d,uusDcs=%d,uusLen=%d", printBuf,
                dial.uusInfo->uusType, dial.uusInfo->uusDcs,
                dial.uusInfo->uusLength);
    }
    closeRequest;
    printRequest(pRI->token, pRI->pCI->requestNumber);

    CALL_ONREQUEST(pRI->pCI->requestNumber, &dial, sizeOfDial, pRI,
                   pRI->socket_id);

#ifdef MEMSET_FREED
    memsetString(dial.address);
#endif

    free(dial.address);

#ifdef MEMSET_FREED
    memset(&uusInfo, 0, sizeof(RIL_UUS_Info));
    memset(&dial, 0, sizeof(dial));
#endif

    return;
invalid:
    invalidCommandBlock(pRI);
    return;
}

/**
 * Callee expects const RIL_SIM_IO *
 * Payload is:
 *   int32_t command
 *   int32_t fileid
 *   String path
 *   int32_t p1, p2, p3
 *   String data
 *   String pin2
 *   String aidPtr
 */
static void dispatchSIM_IO(Parcel &p, RequestInfo *pRI) {
    union RIL_SIM_IO {
        RIL_SIM_IO_v6 v6;
        RIL_SIM_IO_v5 v5;
    } simIO;

    int32_t t;
    int size;
    status_t status;

#if VDBG
    RLOGD("dispatchSIM_IO");
#endif
    memset(&simIO, 0, sizeof(simIO));

    // note we only check status at the end

    status = p.readInt32(&t);
    simIO.v6.command = (int)t;

    status = p.readInt32(&t);
    simIO.v6.fileid = (int)t;

    simIO.v6.path = strdupReadString(p);

    status = p.readInt32(&t);
    simIO.v6.p1 = (int)t;

    status = p.readInt32(&t);
    simIO.v6.p2 = (int)t;

    status = p.readInt32(&t);
    simIO.v6.p3 = (int)t;

    simIO.v6.data = strdupReadString(p);
    simIO.v6.pin2 = strdupReadString(p);
    simIO.v6.aidPtr = strdupReadString(p);

    startRequest;
    appendPrintBuf("%scmd=0x%X,efid=0x%X,path=%s,%d,%d,%d,%s,pin2=%s,aid=%s",
                   printBuf, simIO.v6.command, simIO.v6.fileid,
                   (char*)simIO.v6.path, simIO.v6.p1, simIO.v6.p2, simIO.v6.p3,
                   (char*)simIO.v6.data, (char*)simIO.v6.pin2, simIO.v6.aidPtr);
    closeRequest;
    printRequest(pRI->token, pRI->pCI->requestNumber);

    if (status != NO_ERROR) {
        goto invalid;
    }

    size = (s_callbacks.version < 6) ? sizeof(simIO.v5) : sizeof(simIO.v6);
    CALL_ONREQUEST(pRI->pCI->requestNumber, &simIO, size, pRI, pRI->socket_id);

#ifdef MEMSET_FREED
    memsetString(simIO.v6.path);
    memsetString(simIO.v6.data);
    memsetString(simIO.v6.pin2);
    memsetString(simIO.v6.aidPtr);
#endif

    free(simIO.v6.path);
    free(simIO.v6.data);
    free(simIO.v6.pin2);
    free(simIO.v6.aidPtr);

#ifdef MEMSET_FREED
    memset(&simIO, 0, sizeof(simIO));
#endif

    return;
invalid:
    invalidCommandBlock(pRI);
    return;
}

/**
 * Callee expects const RIL_SIM_APDU *
 * Payload is:
 *   int32_t sessionid
 *   int32_t cla
 *   int32_t instruction
 *   int32_t p1, p2, p3
 *   String data
 */
static void dispatchSIM_APDU(Parcel &p, RequestInfo *pRI) {
    int32_t t;
    status_t status;
    RIL_SIM_APDU apdu;

#if VDBG
    RLOGD("dispatchSIM_APDU");
#endif

    memset(&apdu, 0, sizeof(RIL_SIM_APDU));

    // Note we only check status at the end. Any single failure leads to
    // subsequent reads filing.
    status = p.readInt32(&t);
    apdu.sessionid = (int)t;

    status = p.readInt32(&t);
    apdu.cla = (int)t;

    status = p.readInt32(&t);
    apdu.instruction = (int)t;

    status = p.readInt32(&t);
    apdu.p1 = (int)t;

    status = p.readInt32(&t);
    apdu.p2 = (int)t;

    status = p.readInt32(&t);
    apdu.p3 = (int)t;

    apdu.data = strdupReadString(p);

    startRequest;
    appendPrintBuf("%ssessionid=%d,cla=%d,ins=%d,p1=%d,p2=%d,p3=%d,data=%s",
        printBuf, apdu.sessionid, apdu.cla, apdu.instruction, apdu.p1, apdu.p2,
        apdu.p3, (char*)apdu.data);
    closeRequest;
    printRequest(pRI->token, pRI->pCI->requestNumber);

    if (status != NO_ERROR) {
        goto invalid;
    }

    CALL_ONREQUEST(pRI->pCI->requestNumber, &apdu,
            sizeof(RIL_SIM_APDU), pRI, pRI->socket_id);

#ifdef MEMSET_FREED
    memsetString(apdu.data);
#endif
    free(apdu.data);

#ifdef MEMSET_FREED
    memset(&apdu, 0, sizeof(RIL_SIM_APDU));
#endif

    return;
invalid:
    invalidCommandBlock(pRI);
    return;
}


/**
 * Callee expects const RIL_CallForwardInfo *
 * Payload is:
 *  int32_t status/action
 *  int32_t reason
 *  int32_t serviceCode
 *  int32_t toa
 *  String number  (0 length -> null)
 *  int32_t timeSeconds
 */
static void dispatchCallForward(Parcel &p, RequestInfo *pRI) {
    RIL_CallForwardInfo cff;
    int32_t t;
    status_t status;

    memset (&cff, 0, sizeof(cff));

    // note we only check status at the end

    status = p.readInt32(&t);
    cff.status = (int)t;

    status = p.readInt32(&t);
    cff.reason = (int)t;

    status = p.readInt32(&t);
    cff.serviceClass = (int)t;

    status = p.readInt32(&t);
    cff.toa = (int)t;

    cff.number = strdupReadString(p);

    status = p.readInt32(&t);
    cff.timeSeconds = (int)t;

    if (status != NO_ERROR) {
        goto invalid;
    }

    // special case: number 0-length fields is null
    if (cff.number != NULL && strlen(cff.number) == 0) {
        cff.number = NULL;
    }

    startRequest;
    appendPrintBuf("%sstat=%d,reason=%d,serv=%d,toa=%d,%s,tout=%d", printBuf,
        cff.status, cff.reason, cff.serviceClass, cff.toa,
        (char*)cff.number, cff.timeSeconds);
    closeRequest;
    printRequest(pRI->token, pRI->pCI->requestNumber);

    CALL_ONREQUEST(pRI->pCI->requestNumber, &cff, sizeof(cff),
                   pRI, pRI->socket_id);

#ifdef MEMSET_FREED
    memsetString(cff.number);
#endif

    free(cff.number);

#ifdef MEMSET_FREED
    memset(&cff, 0, sizeof(cff));
#endif

    return;
invalid:
    invalidCommandBlock(pRI);
    return;
}


static void dispatchRaw(Parcel &p, RequestInfo *pRI) {
    int32_t len;
    status_t status;
    const void *data;

    status = p.readInt32(&len);

    if (status != NO_ERROR) {
        goto invalid;
    }

    // The java code writes -1 for null arrays
    if (((int)len) == -1) {
        data = NULL;
        len = 0;
    }

    data = p.readInplace(len);

    startRequest;
    appendPrintBuf("%sraw_size=%d", printBuf, len);
    closeRequest;
    printRequest(pRI->token, pRI->pCI->requestNumber);

    CALL_ONREQUEST(pRI->pCI->requestNumber, const_cast<void *>(data),
            len, pRI, pRI->socket_id);

    return;
invalid:
    invalidCommandBlock(pRI);
    return;
}


static status_t constructCdmaSms(Parcel &p, RequestInfo *pRI __unused,
        RIL_CDMA_SMS_Message& rcsm) {
    int32_t  t;
    uint8_t ut;
    status_t status;
    int32_t digitCount;
    int digitLimit;

    memset(&rcsm, 0, sizeof(rcsm));

    status = p.readInt32(&t);
    rcsm.uTeleserviceID = (int)t;

    status = p.read(&ut, sizeof(ut));
    rcsm.bIsServicePresent = (uint8_t)ut;

    status = p.readInt32(&t);
    rcsm.uServicecategory = (int)t;

    status = p.readInt32(&t);
    rcsm.sAddress.digit_mode = (RIL_CDMA_SMS_DigitMode)t;

    status = p.readInt32(&t);
    rcsm.sAddress.number_mode = (RIL_CDMA_SMS_NumberMode)t;

    status = p.readInt32(&t);
    rcsm.sAddress.number_type = (RIL_CDMA_SMS_NumberType)t;

    status = p.readInt32(&t);
    rcsm.sAddress.number_plan = (RIL_CDMA_SMS_NumberPlan)t;

    status = p.read(&ut, sizeof(ut));
    rcsm.sAddress.number_of_digits = (uint8_t)ut;

    digitLimit = MIN((rcsm.sAddress.number_of_digits), RIL_CDMA_SMS_ADDRESS_MAX);
    for (digitCount = 0; digitCount < digitLimit; digitCount++) {
        status = p.read(&ut, sizeof(ut));
        rcsm.sAddress.digits[digitCount] = (uint8_t)ut;
    }

    status = p.readInt32(&t);
    rcsm.sSubAddress.subaddressType = (RIL_CDMA_SMS_SubaddressType)t;

    status = p.read(&ut, sizeof(ut));
    rcsm.sSubAddress.odd = (uint8_t)ut;

    status = p.read(&ut, sizeof(ut));
    rcsm.sSubAddress.number_of_digits = (uint8_t)ut;

    digitLimit = MIN((rcsm.sSubAddress.number_of_digits), RIL_CDMA_SMS_SUBADDRESS_MAX);
    for (digitCount = 0; digitCount < digitLimit; digitCount++) {
        status = p.read(&ut, sizeof(ut));
        rcsm.sSubAddress.digits[digitCount] = (uint8_t)ut;
    }

    status = p.readInt32(&t);
    rcsm.uBearerDataLen = (int)t;

    digitLimit = MIN((rcsm.uBearerDataLen), RIL_CDMA_SMS_BEARER_DATA_MAX);
    for (digitCount = 0; digitCount < digitLimit; digitCount++) {
        status = p.read(&ut, sizeof(ut));
        rcsm.aBearerData[digitCount] = (uint8_t)ut;
    }

    if (status != NO_ERROR) {
        return status;
    }

    startRequest;
    appendPrintBuf("%suTeleserviceID = %d, bIsServicePresent = %d, uServicecategory = %d, \
            sAddress.digit_mode = %d, sAddress.Number_mode = %d, sAddress.number_type = %d, ",
            printBuf, rcsm.uTeleserviceID, rcsm.bIsServicePresent, rcsm.uServicecategory,
            rcsm.sAddress.digit_mode, rcsm.sAddress.number_mode, rcsm.sAddress.number_type);
    closeRequest;

    printRequest(pRI->token, pRI->pCI->requestNumber);

    return status;
}

static void dispatchCdmaSms(Parcel &p, RequestInfo *pRI) {
    RIL_CDMA_SMS_Message rcsm;

    RLOGD("dispatchCdmaSms");
    if (NO_ERROR != constructCdmaSms(p, pRI, rcsm)) {
        goto invalid;
    }

    CALL_ONREQUEST(pRI->pCI->requestNumber, &rcsm, sizeof(rcsm), pRI, pRI->socket_id);

#ifdef MEMSET_FREED
    memset(&rcsm, 0, sizeof(rcsm));
#endif

    return;

invalid:
    invalidCommandBlock(pRI);
    return;
}

static void dispatchImsCdmaSms(Parcel &p, RequestInfo *pRI,
        uint8_t retry, int32_t messageRef) {
    RIL_IMS_SMS_Message rism;
    RIL_CDMA_SMS_Message rcsm;

    RLOGD("dispatchImsCdmaSms: retry=%d, messageRef=%d", retry, messageRef);

    if (NO_ERROR != constructCdmaSms(p, pRI, rcsm)) {
        goto invalid;
    }
    memset(&rism, 0, sizeof(rism));
    rism.tech = RADIO_TECH_3GPP2;
    rism.retry = retry;
    rism.messageRef = messageRef;
    rism.message.cdmaMessage = &rcsm;

    CALL_ONREQUEST(pRI->pCI->requestNumber, &rism,
            sizeof(RIL_RadioTechnologyFamily) + sizeof(uint8_t) +
            sizeof(int32_t) + sizeof(rcsm), pRI, pRI->socket_id);

#ifdef MEMSET_FREED
    memset(&rcsm, 0, sizeof(rcsm));
    memset(&rism, 0, sizeof(rism));
#endif

    return;

invalid:
    invalidCommandBlock(pRI);
    return;
}

static void dispatchImsGsmSms(Parcel &p, RequestInfo *pRI,
        uint8_t retry, int32_t messageRef) {
    RIL_IMS_SMS_Message rism;
    int32_t countStrings;
    status_t status;
    size_t datalen;
    char **pStrings;
    RLOGD("dispatchImsGsmSms: retry=%d, messageRef=%d", retry, messageRef);

    status = p.readInt32(&countStrings);

    if (status != NO_ERROR) {
        goto invalid;
    }

    memset(&rism, 0, sizeof(rism));
    rism.tech = RADIO_TECH_3GPP;
    rism.retry = retry;
    rism.messageRef = messageRef;

    startRequest;
    appendPrintBuf("%stech=%d, retry=%d, messageRef=%d, ", printBuf,
                        (int)rism.tech, (int)rism.retry, rism.messageRef);
    if (countStrings == 0) {
        // just some non-null pointer
        pStrings = (char **)calloc(1, sizeof(char *));
        if (pStrings == NULL) {
            RLOGE("Memory allocation failed for request %s",
                    requestToString(pRI->pCI->requestNumber));
            closeRequest;
            return;
        }
        datalen = 0;
    } else if (countStrings < 0) {
        pStrings = NULL;
        datalen = 0;
    } else {
        if (countStrings > (INT_MAX/sizeof(char *))) {
            RLOGE("Invalid value of countStrings: \n");
            closeRequest;
            return;
        }
        datalen = sizeof(char *) * countStrings;

        pStrings = (char **)calloc(countStrings, sizeof(char *));
        if (pStrings == NULL) {
            RLOGE("Memory allocation failed for request %s",
                    requestToString(pRI->pCI->requestNumber));
            closeRequest;
            return;
        }

        for (int i = 0; i < countStrings; i++) {
            pStrings[i] = strdupReadString(p);
            appendPrintBuf("%s%s,", printBuf, pStrings[i]);
        }
    }
    removeLastChar;
    closeRequest;
    printRequest(pRI->token, pRI->pCI->requestNumber);

    rism.message.gsmMessage = pStrings;
    CALL_ONREQUEST(pRI->pCI->requestNumber, &rism,
            sizeof(RIL_RadioTechnologyFamily) + sizeof(uint8_t) +
            sizeof(int32_t) + datalen, pRI, pRI->socket_id);

    if (pStrings != NULL) {
        for (int i = 0; i < countStrings; i++) {
#ifdef MEMSET_FREED
            memsetString(pStrings[i]);
#endif
            free(pStrings[i]);
        }

#ifdef MEMSET_FREED
        memset(pStrings, 0, datalen);
#endif
        free(pStrings);
    }

#ifdef MEMSET_FREED
    memset(&rism, 0, sizeof(rism));
#endif
    return;

invalid:
    ALOGE("dispatchImsGsmSms invalid block");
    invalidCommandBlock(pRI);
    return;
}

static void dispatchImsSms(Parcel &p, RequestInfo *pRI) {
    int32_t  t;
    status_t status = p.readInt32(&t);
    RIL_RadioTechnologyFamily format;
    uint8_t retry;
    int32_t messageRef;

    RLOGD("dispatchImsSms");
    if (status != NO_ERROR) {
        goto invalid;
    }
    format = (RIL_RadioTechnologyFamily)t;

    // read retry field
    status = p.read(&retry, sizeof(retry));
    if (status != NO_ERROR) {
        goto invalid;
    }
    // read messageRef field
    status = p.read(&messageRef, sizeof(messageRef));
    if (status != NO_ERROR) {
        goto invalid;
    }

    if (RADIO_TECH_3GPP == format) {
        dispatchImsGsmSms(p, pRI, retry, messageRef);
    } else if (RADIO_TECH_3GPP2 == format) {
        dispatchImsCdmaSms(p, pRI, retry, messageRef);
    } else {
        ALOGE("requestImsSendSMS invalid format value =%d", format);
    }

    return;

invalid:
    invalidCommandBlock(pRI);
    return;
}

static void dispatchCdmaSmsAck(Parcel &p, RequestInfo *pRI) {
    RIL_CDMA_SMS_Ack rcsa;
    int32_t  t;
    status_t status;
    int32_t digitCount;

    memset(&rcsa, 0, sizeof(rcsa));

    status = p.readInt32(&t);
    rcsa.uErrorClass = (RIL_CDMA_SMS_ErrorClass)t;

    status = p.readInt32(&t);
    rcsa.uSMSCauseCode = (int)t;

    if (status != NO_ERROR) {
        goto invalid;
    }

    startRequest;
    appendPrintBuf("%suErrorClass=%d, uTLStatus=%d, ",
            printBuf, rcsa.uErrorClass, rcsa.uSMSCauseCode);
    closeRequest;

    printRequest(pRI->token, pRI->pCI->requestNumber);

    CALL_ONREQUEST(pRI->pCI->requestNumber, &rcsa, sizeof(rcsa), pRI,
                   pRI->socket_id);

#ifdef MEMSET_FREED
    memset(&rcsa, 0, sizeof(rcsa));
#endif

    return;

invalid:
    invalidCommandBlock(pRI);
    return;
}

static void dispatchGsmBrSmsCnf(Parcel &p, RequestInfo *pRI) {
    int32_t t;
    status_t status;
    int32_t num;

    status = p.readInt32(&num);
    if (status != NO_ERROR) {
        goto invalid;
    }

    {
        RIL_GSM_BroadcastSmsConfigInfo gsmBci[num];
        RIL_GSM_BroadcastSmsConfigInfo *gsmBciPtrs[num];

        startRequest;
        for (int i = 0; i < num; i++) {
            gsmBciPtrs[i] = &gsmBci[i];

            status = p.readInt32(&t);
            gsmBci[i].fromServiceId = (int)t;

            status = p.readInt32(&t);
            gsmBci[i].toServiceId = (int)t;

            status = p.readInt32(&t);
            gsmBci[i].fromCodeScheme = (int)t;

            status = p.readInt32(&t);
            gsmBci[i].toCodeScheme = (int)t;

            status = p.readInt32(&t);
            gsmBci[i].selected = (uint8_t)t;

            appendPrintBuf("%s[%d: fromServiceId = %d, toServiceId = %d, \
                  fromCodeScheme = %d, toCodeScheme = %d, selected = %d]",
                  printBuf, i, gsmBci[i].fromServiceId, gsmBci[i].toServiceId,
                  gsmBci[i].fromCodeScheme, gsmBci[i].toCodeScheme,
                  gsmBci[i].selected);
        }
        closeRequest;

        if (status != NO_ERROR) {
            goto invalid;
        }

        CALL_ONREQUEST(pRI->pCI->requestNumber, gsmBciPtrs,
                       num * sizeof(RIL_GSM_BroadcastSmsConfigInfo *), pRI,
                       pRI->socket_id);

#ifdef MEMSET_FREED
        memset(gsmBci, 0, num * sizeof(RIL_GSM_BroadcastSmsConfigInfo));
        memset(gsmBciPtrs, 0, num * sizeof(RIL_GSM_BroadcastSmsConfigInfo *));
#endif
    }

    return;

invalid:
    invalidCommandBlock(pRI);
    return;
}

static void dispatchCdmaBrSmsCnf(Parcel &p, RequestInfo *pRI) {
    int32_t t;
    status_t status;
    int32_t num;

    status = p.readInt32(&num);
    if (status != NO_ERROR) {
        goto invalid;
    }

    {
        RIL_CDMA_BroadcastSmsConfigInfo cdmaBci[num];
        RIL_CDMA_BroadcastSmsConfigInfo *cdmaBciPtrs[num];

        startRequest;
        for (int i = 0; i < num; i++) {
            cdmaBciPtrs[i] = &cdmaBci[i];

            status = p.readInt32(&t);
            cdmaBci[i].service_category = (int)t;

            status = p.readInt32(&t);
            cdmaBci[i].language = (int)t;

            status = p.readInt32(&t);
            cdmaBci[i].selected = (uint8_t)t;

            appendPrintBuf("%s[%d: service_category = %d, language = %d, \
                           entries.bSelected = %d]", printBuf, i,
                           cdmaBci[i].service_category, cdmaBci[i].language,
                           cdmaBci[i].selected);
        }
        closeRequest;

        if (status != NO_ERROR) {
            goto invalid;
        }

        CALL_ONREQUEST(pRI->pCI->requestNumber, cdmaBciPtrs,
                       num * sizeof(RIL_CDMA_BroadcastSmsConfigInfo *), pRI,
                       pRI->socket_id);

#ifdef MEMSET_FREED
        memset(cdmaBci, 0, num * sizeof(RIL_CDMA_BroadcastSmsConfigInfo));
        memset(cdmaBciPtrs, 0, num * sizeof(RIL_CDMA_BroadcastSmsConfigInfo *));
#endif
    }

    return;

invalid:
    invalidCommandBlock(pRI);
    return;
}

static void dispatchRilCdmaSmsWriteArgs(Parcel &p, RequestInfo *pRI) {
    RIL_CDMA_SMS_WriteArgs rcsw;
    int32_t  t;
    uint32_t ut;
    uint8_t  uct;
    status_t status;
    int32_t  digitCount;
    int32_t  digitLimit;

    memset(&rcsw, 0, sizeof(rcsw));

    status = p.readInt32(&t);
    rcsw.status = t;

    status = p.readInt32(&t);
    rcsw.message.uTeleserviceID = (int)t;

    status = p.read(&uct,sizeof(uct));
    rcsw.message.bIsServicePresent = (uint8_t)uct;

    status = p.readInt32(&t);
    rcsw.message.uServicecategory = (int)t;

    status = p.readInt32(&t);
    rcsw.message.sAddress.digit_mode = (RIL_CDMA_SMS_DigitMode)t;

    status = p.readInt32(&t);
    rcsw.message.sAddress.number_mode = (RIL_CDMA_SMS_NumberMode)t;

    status = p.readInt32(&t);
    rcsw.message.sAddress.number_type = (RIL_CDMA_SMS_NumberType)t;

    status = p.readInt32(&t);
    rcsw.message.sAddress.number_plan = (RIL_CDMA_SMS_NumberPlan)t;

    status = p.read(&uct,sizeof(uct));
    rcsw.message.sAddress.number_of_digits = (uint8_t)uct;

    digitLimit = MIN((rcsw.message.sAddress.number_of_digits), RIL_CDMA_SMS_ADDRESS_MAX);

    for (digitCount = 0 ; digitCount < digitLimit; digitCount ++) {
        status = p.read(&uct,sizeof(uct));
        rcsw.message.sAddress.digits[digitCount] = (uint8_t)uct;
    }

    status = p.readInt32(&t);
    rcsw.message.sSubAddress.subaddressType = (RIL_CDMA_SMS_SubaddressType)t;

    status = p.read(&uct,sizeof(uct));
    rcsw.message.sSubAddress.odd = (uint8_t)uct;

    status = p.read(&uct,sizeof(uct));
    rcsw.message.sSubAddress.number_of_digits = (uint8_t)uct;

    digitLimit = MIN((rcsw.message.sSubAddress.number_of_digits), RIL_CDMA_SMS_SUBADDRESS_MAX);

    for (digitCount = 0 ; digitCount < digitLimit; digitCount ++) {
        status = p.read(&uct,sizeof(uct));
        rcsw.message.sSubAddress.digits[digitCount] = (uint8_t)uct;
    }

    status = p.readInt32(&t);
    rcsw.message.uBearerDataLen = (int)t;

    digitLimit = MIN((rcsw.message.uBearerDataLen), RIL_CDMA_SMS_BEARER_DATA_MAX);

    for (digitCount = 0 ; digitCount < digitLimit; digitCount ++) {
        status = p.read(&uct, sizeof(uct));
        rcsw.message.aBearerData[digitCount] = (uint8_t)uct;
    }

    if (status != NO_ERROR) {
        goto invalid;
    }

    startRequest;
    appendPrintBuf("%sstatus=%d, message.uTeleserviceID=%d, message.bIsServicePresent=%d, \
            message.uServicecategory=%d, message.sAddress.digit_mode=%d, \
            message.sAddress.number_mode=%d, \
            message.sAddress.number_type=%d, ",
            printBuf, rcsw.status, rcsw.message.uTeleserviceID, rcsw.message.bIsServicePresent,
            rcsw.message.uServicecategory, rcsw.message.sAddress.digit_mode,
            rcsw.message.sAddress.number_mode,
            rcsw.message.sAddress.number_type);
    closeRequest;

    printRequest(pRI->token, pRI->pCI->requestNumber);

    CALL_ONREQUEST(pRI->pCI->requestNumber, &rcsw, sizeof(rcsw), pRI, pRI->socket_id);

#ifdef MEMSET_FREED
    memset(&rcsw, 0, sizeof(rcsw));
#endif

    return;

invalid:
    invalidCommandBlock(pRI);
    return;

}

// For backwards compatibility in RIL_REQUEST_SETUP_DATA_CALL.
// Version 4 of the RIL interface adds a new PDP type parameter to support
// IPv6 and dual-stack PDP contexts. When dealing with a previous version of
// RIL, remove the parameter from the request.
static void dispatchDataCall(Parcel &p, RequestInfo *pRI) {
    // In RIL v3, REQUEST_SETUP_DATA_CALL takes 6 parameters.
    const int numParamsRilV3 = 6;

    // The first bytes of the RIL parcel contain the request number and the
    // serial number - see processCommandBuffer(). Copy them over too.
    int pos = p.dataPosition();

    int numParams = p.readInt32();
    if (s_callbacks.version < 4 && numParams > numParamsRilV3) {
        Parcel p2;
        p2.appendFrom(&p, 0, pos);
        p2.writeInt32(numParamsRilV3);
        for (int i = 0; i < numParamsRilV3; i++) {
            p2.writeString16(p.readString16());
        }
        p2.setDataPosition(pos);
        dispatchStrings(p2, pRI);
    } else {
        p.setDataPosition(pos);
        dispatchStrings(p, pRI);
    }
}

// For backwards compatibility with RILs that dont support RIL_REQUEST_VOICE_RADIO_TECH.
// When all RILs handle this request, this function can be removed and
// the request can be sent directly to the RIL using dispatchVoid.
static void dispatchVoiceRadioTech(Parcel &p, RequestInfo *pRI) {
    RIL_RadioState state = CALL_ONSTATEREQUEST((RIL_SOCKET_ID)pRI->socket_id);

    if ((RADIO_STATE_UNAVAILABLE == state) || (RADIO_STATE_OFF == state)) {
        RIL_onRequestComplete(pRI, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
    }

    // RILs that support RADIO_STATE_ON should support this request.
    if (RADIO_STATE_ON == state) {
        dispatchVoid(p, pRI);
        return;
    }

    // For Older RILs, that do not support RADIO_STATE_ON, assume that they
    // will not support this new request either and decode Voice Radio Technology
    // from Radio State
    s_voiceRadioTech = decodeVoiceRadioTechnology(state);

    if (s_voiceRadioTech < 0) {
        RIL_onRequestComplete(pRI, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(pRI, RIL_E_SUCCESS, &s_voiceRadioTech, sizeof(int));
    }
}

// For backwards compatibility in RIL_REQUEST_CDMA_GET_SUBSCRIPTION_SOURCE:.
// When all RILs handle this request, this function can be removed and
// the request can be sent directly to the RIL using dispatchVoid.
static void dispatchCdmaSubscriptionSource(Parcel &p, RequestInfo *pRI) {
    RIL_RadioState state = CALL_ONSTATEREQUEST((RIL_SOCKET_ID)pRI->socket_id);

    if ((RADIO_STATE_UNAVAILABLE == state) || (RADIO_STATE_OFF == state)) {
        RIL_onRequestComplete(pRI, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
    }

    // RILs that support RADIO_STATE_ON should support this request.
    if (RADIO_STATE_ON == state) {
        dispatchVoid(p, pRI);
        return;
    }

    // For Older RILs, that do not support RADIO_STATE_ON, assume that they
    // will not support this new request either and decode CDMA Subscription Source
    // from Radio State
    s_cdmaSubscriptionSource = decodeCdmaSubscriptionSource(state);

    if (s_cdmaSubscriptionSource < 0) {
        RIL_onRequestComplete(pRI, RIL_E_GENERIC_FAILURE, NULL, 0);
    } else {
        RIL_onRequestComplete(pRI, RIL_E_SUCCESS, &s_cdmaSubscriptionSource, sizeof(int));
    }
}

static void dispatchSetInitialAttachApn(Parcel &p, RequestInfo *pRI) {
    RIL_InitialAttachApn pf;
    int32_t  t;
    status_t status;

    memset(&pf, 0, sizeof(pf));

    pf.apn = strdupReadString(p);
    pf.protocol = strdupReadString(p);

    status = p.readInt32(&t);
    pf.authtype = (int) t;

    pf.username = strdupReadString(p);
    pf.password = strdupReadString(p);

    startRequest;
    appendPrintBuf("%sapn=%s, protocol=%s, authtype=%d, username=%s, password=%s",
            printBuf, pf.apn, pf.protocol, pf.authtype, pf.username, pf.password);
    closeRequest;
    printRequest(pRI->token, pRI->pCI->requestNumber);

    if (status != NO_ERROR) {
        goto invalid;
    }
    CALL_ONREQUEST(pRI->pCI->requestNumber, &pf, sizeof(pf), pRI, pRI->socket_id);

#ifdef MEMSET_FREED
    memsetString(pf.apn);
    memsetString(pf.protocol);
    memsetString(pf.username);
    memsetString(pf.password);
#endif

    free(pf.apn);
    free(pf.protocol);
    free(pf.username);
    free(pf.password);

#ifdef MEMSET_FREED
    memset(&pf, 0, sizeof(pf));
#endif

    return;

invalid:
    invalidCommandBlock(pRI);
    return;
}

static void dispatchNVReadItem(Parcel &p, RequestInfo *pRI) {
    RIL_NV_ReadItem nvri;
    int32_t  t;
    status_t status;

    memset(&nvri, 0, sizeof(nvri));

    status = p.readInt32(&t);
    nvri.itemID = (RIL_NV_Item)t;

    if (status != NO_ERROR) {
        goto invalid;
    }

    startRequest;
    appendPrintBuf("%snvri.itemID=%d, ", printBuf, nvri.itemID);
    closeRequest;

    printRequest(pRI->token, pRI->pCI->requestNumber);

    CALL_ONREQUEST(pRI->pCI->requestNumber, &nvri, sizeof(nvri), pRI,
                   pRI->socket_id);

#ifdef MEMSET_FREED
    memset(&nvri, 0, sizeof(nvri));
#endif

    return;

invalid:
    invalidCommandBlock(pRI);
    return;
}

static void dispatchNVWriteItem(Parcel &p, RequestInfo *pRI) {
    RIL_NV_WriteItem nvwi;
    int32_t  t;
    status_t status;

    memset(&nvwi, 0, sizeof(nvwi));

    status = p.readInt32(&t);
    nvwi.itemID = (RIL_NV_Item)t;

    nvwi.value = strdupReadString(p);

    if (status != NO_ERROR || nvwi.value == NULL) {
        goto invalid;
    }

    startRequest;
    appendPrintBuf("%snvwi.itemID=%d, value=%s, ", printBuf, nvwi.itemID,
            nvwi.value);
    closeRequest;

    printRequest(pRI->token, pRI->pCI->requestNumber);

    CALL_ONREQUEST(pRI->pCI->requestNumber, &nvwi, sizeof(nvwi), pRI,
                   pRI->socket_id);

#ifdef MEMSET_FREED
    memsetString(nvwi.value);
#endif

    free(nvwi.value);

#ifdef MEMSET_FREED
    memset(&nvwi, 0, sizeof(nvwi));
#endif

    return;

invalid:
    invalidCommandBlock(pRI);
    return;
}

static void dispatchUiccSubscripton(Parcel &p, RequestInfo *pRI) {
    RIL_SelectUiccSub uicc_sub;
    status_t status;
    int32_t  t;
    memset(&uicc_sub, 0, sizeof(uicc_sub));

    status = p.readInt32(&t);
    if (status != NO_ERROR) {
        goto invalid;
    }
    uicc_sub.slot = (int) t;

    status = p.readInt32(&t);
    if (status != NO_ERROR) {
        goto invalid;
    }
    uicc_sub.app_index = (int)t;

    status = p.readInt32(&t);
    if (status != NO_ERROR) {
        goto invalid;
    }
    uicc_sub.sub_type = (RIL_SubscriptionType)t;

    status = p.readInt32(&t);
    if (status != NO_ERROR) {
        goto invalid;
    }
    uicc_sub.act_status = (RIL_UiccSubActStatus)t;

    startRequest;
    appendPrintBuf("slot=%d, app_index=%d, act_status = %d",
            uicc_sub.slot, uicc_sub.app_index, uicc_sub.act_status);
    RLOGD("dispatchUiccSubscription, slot=%d, app_index=%d, act_status = %d",
            uicc_sub.slot, uicc_sub.app_index, uicc_sub.act_status);
    closeRequest;
    printRequest(pRI->token, pRI->pCI->requestNumber);

    CALL_ONREQUEST(pRI->pCI->requestNumber, &uicc_sub, sizeof(uicc_sub), pRI,
                   pRI->socket_id);

#ifdef MEMSET_FREED
    memset(&uicc_sub, 0, sizeof(uicc_sub));
#endif
    return;

invalid:
    invalidCommandBlock(pRI);
    return;
}

static void dispatchSimAuthentication(Parcel &p, RequestInfo *pRI) {
    RIL_SimAuthentication pf;
    int32_t  t;
    status_t status;

    memset(&pf, 0, sizeof(pf));

    status = p.readInt32(&t);
    pf.authContext = (int)t;
    pf.authData = strdupReadString(p);
    pf.aid = strdupReadString(p);

    startRequest;
    appendPrintBuf("authContext=%d, authData=%s, aid=%s",
            pf.authContext, pf.authData, pf.aid);
    closeRequest;
    printRequest(pRI->token, pRI->pCI->requestNumber);

    if (status != NO_ERROR) {
        goto invalid;
    }
    CALL_ONREQUEST(pRI->pCI->requestNumber, &pf, sizeof(pf), pRI, pRI->socket_id);

#ifdef MEMSET_FREED
    memsetString(pf.authData);
    memsetString(pf.aid);
#endif

    free(pf.authData);
    free(pf.aid);

#ifdef MEMSET_FREED
    memset(&pf, 0, sizeof(pf));
#endif

    return;
invalid:
    invalidCommandBlock(pRI);
    return;
}

static void dispatchDataProfile(Parcel &p, RequestInfo *pRI) {
    int32_t t;
    status_t status;
    int32_t num;

    status = p.readInt32(&num);
    if (status != NO_ERROR || num < 0) {
        goto invalid;
    }

    {
        RIL_DataProfileInfo *dataProfiles =
                (RIL_DataProfileInfo *)calloc(num, sizeof(RIL_DataProfileInfo));
        if (dataProfiles == NULL) {
            RLOGE("Memory allocation failed for request %s",
                    requestToString(pRI->pCI->requestNumber));
            return;
        }
        RIL_DataProfileInfo **dataProfilePtrs =
                (RIL_DataProfileInfo **)calloc(num, sizeof(RIL_DataProfileInfo *));
        if (dataProfilePtrs == NULL) {
            RLOGE("Memory allocation failed for request %s",
                    requestToString(pRI->pCI->requestNumber));
            free(dataProfiles);
            return;
        }

        startRequest;
        for (int i = 0; i < num; i++) {
            dataProfilePtrs[i] = &dataProfiles[i];

            status = p.readInt32(&t);
            dataProfiles[i].profileId = (int)t;

            dataProfiles[i].apn = strdupReadString(p);
            dataProfiles[i].protocol = strdupReadString(p);
            status = p.readInt32(&t);
            dataProfiles[i].authType = (int)t;

            dataProfiles[i].user = strdupReadString(p);
            dataProfiles[i].password = strdupReadString(p);

            status = p.readInt32(&t);
            dataProfiles[i].type = (int)t;

            status = p.readInt32(&t);
            dataProfiles[i].maxConnsTime = (int)t;
            status = p.readInt32(&t);
            dataProfiles[i].maxConns = (int)t;
            status = p.readInt32(&t);
            dataProfiles[i].waitTime = (int)t;

            status = p.readInt32(&t);
            dataProfiles[i].enabled = (int)t;

            appendPrintBuf("%s[%d: profileId = %d, apn = %s, protocol = %s, authType = %d, \
                  user = %s, password = %s, type = %d, maxConnsTime = %d, maxConns = %d, \
                  waitTime = %d, enabled = %d]",
                  printBuf, i, dataProfiles[i].profileId, dataProfiles[i].apn,
                  dataProfiles[i].protocol, dataProfiles[i].authType,
                  dataProfiles[i].user, dataProfiles[i].password,
                  dataProfiles[i].type, dataProfiles[i].maxConnsTime,
                  dataProfiles[i].maxConns, dataProfiles[i].waitTime,
                  dataProfiles[i].enabled);
        }
        closeRequest;
        printRequest(pRI->token, pRI->pCI->requestNumber);

        if (status != NO_ERROR) {
            for (int i = 0; i < num; i++) {
                free(dataProfiles[i].apn);
                free(dataProfiles[i].protocol);
                free(dataProfiles[i].user);
                free(dataProfiles[i].password);
            }
            free(dataProfiles);
            free(dataProfilePtrs);
            goto invalid;
        }
        CALL_ONREQUEST(pRI->pCI->requestNumber,
                              dataProfilePtrs,
                              num * sizeof(RIL_DataProfileInfo *),
                              pRI, pRI->socket_id);

#ifdef MEMSET_FREED
        memset(dataProfiles, 0, num * sizeof(RIL_DataProfileInfo));
        memset(dataProfilePtrs, 0, num * sizeof(RIL_DataProfileInfo *));
#endif
        for (int i = 0; i < num; i++) {
            free(dataProfiles[i].apn);
            free(dataProfiles[i].protocol);
            free(dataProfiles[i].user);
            free(dataProfiles[i].password);
        }
        free(dataProfiles);
        free(dataProfilePtrs);
    }

    return;

invalid:
    invalidCommandBlock(pRI);
    return;
}

static void dispatchRadioCapability(Parcel &p, RequestInfo *pRI) {
    RIL_RadioCapability rc;
    int32_t t;
    status_t status;

    memset (&rc, 0, sizeof(RIL_RadioCapability));

    status = p.readInt32(&t);
    rc.version = (int)t;
    if (status != NO_ERROR) {
        goto invalid;
    }

    status = p.readInt32(&t);
    rc.session = (int)t;
    if (status != NO_ERROR) {
        goto invalid;
    }

    status = p.readInt32(&t);
    rc.phase = (int)t;
    if (status != NO_ERROR) {
        goto invalid;
    }

    status = p.readInt32(&t);
    rc.rat = (int)t;
    if (status != NO_ERROR) {
        goto invalid;
    }

    status = readStringFromParcelInplace(p,
            rc.logicalModemUuid, sizeof(rc.logicalModemUuid));
    if (status != NO_ERROR) {
        goto invalid;
    }

    status = p.readInt32(&t);
    rc.status = (int)t;

    if (status != NO_ERROR) {
        goto invalid;
    }

    startRequest;
    appendPrintBuf("%s[version:%d, session:%d, phase:%d, rat:%d, \
            logicalModemUuid:%s, status:%d", printBuf, rc.version, rc.session,
            rc.phase, rc.rat, rc.logicalModemUuid, rc.session);

    closeRequest;
    printRequest(pRI->token, pRI->pCI->requestNumber);

    CALL_ONREQUEST(pRI->pCI->requestNumber,
                &rc,
                sizeof(RIL_RadioCapability),
                pRI, pRI->socket_id);
    return;

invalid:
    invalidCommandBlock(pRI);
    return;
}

/**
 * Callee expects const RIL_NetworkList *
 * Payload is:
 *   String operatorNumeric
 *   int32_t act
 */
static void dispatchNetworkList(Parcel &p, RequestInfo *pRI) {
    RIL_NetworkList list;
    int32_t t;
    status_t status;

    memset(&list, 0, sizeof(list));

    /* note we only check status at the end */

    list.operatorNumeric = strdupReadString(p);

    status = p.readInt32(&t);
    list.act = (int)t;

    startRequest;
    appendPrintBuf("%soperatorNumeric=%s, AcT=%d", printBuf,
        (char *)list.operatorNumeric, list.act);
    closeRequest;
    printRequest(pRI->token, pRI->pCI->requestNumber);

    if (status != NO_ERROR) {
        RLOGD("set default AcT = -1");
        list.act = -1;
    }

    CALL_ONREQUEST(pRI->pCI->requestNumber, &list, sizeof(list), pRI,
                   pRI->socket_id);

#ifdef MEMSET_FREED
    memsetString(list.operatorNumeric);
#endif

    free(list.operatorNumeric);

#ifdef MEMSET_FREED
    memset(&list, 0, sizeof(list));
#endif

    return;
}

/* IMS dispatchFunction @{ */
static void dispatchCallForwardUri(Parcel &p, RequestInfo *pRI) {
    RIL_CallForwardInfoUri cff;
    int32_t t;
    status_t status;

    memset (&cff, 0, sizeof(cff));

    // note we only check status at the ends
    status = p.readInt32(&t);
    cff.status = (int)t;

    status = p.readInt32(&t);
    cff.reason = (int)t;

    status = p.readInt32(&t);
    cff.numberType = (int)t;

    status = p.readInt32(&t);
    cff.ton = (int)t;

    cff.number = strdupReadString(p);

    status = p.readInt32(&t);
    cff.serviceClass = (int)t;

    cff.ruleset = strdupReadString(p);

    status = p.readInt32(&t);
    cff.timeSeconds = (int)t;

    if (status != NO_ERROR) {
        goto invalid;
    }

    // special case: number 0-length fields is null
    if (cff.number != NULL && strlen(cff.number) == 0) {
        cff.number = NULL;
    }

    startRequest;
    appendPrintBuf("%sstat=%d,reason=%d,numType=%d,toa=%d,%s,serv=%d,rule=%s,tout=%d",
            printBuf, cff.status, cff.reason, cff.numberType, cff.ton,
            (char*)cff.number, cff.serviceClass, cff.ruleset, cff.timeSeconds);
    closeRequest;
    printRequest(pRI->token, pRI->pCI->requestNumber);

    CALL_ONREQUEST(pRI->pCI->requestNumber, &cff, sizeof(cff), pRI, pRI->socket_id);

#ifdef MEMSET_FREED
    memsetString(cff.number);
#endif

    free(cff.number);
    free(cff.ruleset);

#ifdef MEMSET_FREED
    memset(&cff, 0, sizeof(cff));
#endif

    return;

invalid:
    invalidCommandBlock(pRI);
    return;
}

static void dispatchVideoPhoneDial(Parcel& p, RequestInfo *pRI) {
    RIL_VideoPhone_Dial dial;
    int32_t t;
    status_t status;

    memset(&dial, 0, sizeof(dial));

    dial.address = strdupReadString(p);
    dial.sub_address = strdupReadString(p);

    status = p.readInt32(&t);
    dial.clir = (int)t;

    if (status != NO_ERROR || dial.address == NULL) {
        goto invalid;
    }

    startRequest;
    appendPrintBuf("%saddress=%s,sub_address=%s,clir=%d", printBuf,
                   dial.address, dial.sub_address, dial.clir);
    closeRequest;
    printRequest(pRI->token, pRI->pCI->requestNumber);

    CALL_ONREQUEST(pRI->pCI->requestNumber, &dial, sizeof(dial), pRI, pRI->socket_id);

#ifdef MEMSET_FREED
    memsetString(dial.address);
    memsetString(dial.sub_address);
#endif

    free(dial.address);
    free(dial.sub_address);

#ifdef MEMSET_FREED
    memset(&dial, 0, sizeof(dial));
#endif

    return;
invalid:
    invalidCommandBlock(pRI);
    return;
}

static void dispatchVideoPhoneCodec(Parcel& p, RequestInfo *pRI) {
    RIL_VideoPhone_Codec codec;
    int32_t t;
    status_t status;

    memset(&codec, 0, sizeof(codec));

    status = p.readInt32(&t);
    codec.type = (int)t;

    if (status != NO_ERROR) {
        goto invalid;
    }

    startRequest;
    appendPrintBuf("%stype=%d", printBuf, codec.type);
    closeRequest;
    printRequest(pRI->token, pRI->pCI->requestNumber);

    CALL_ONREQUEST(pRI->pCI->requestNumber, &codec, sizeof(codec), pRI, pRI->socket_id);

    return;
invalid:
    invalidCommandBlock(pRI);
    return;
}

static void dispatchImsNetworkInfo(Parcel& p, RequestInfo *pRI) {
    IMS_NetworkInfo info;
    int32_t t;
    status_t status;

    memset(&info, 0, sizeof(info));

    // note we only check status at the end

    status = p.readInt32(&t);
    info.type = (int)t;

    info.info = strdupReadString(p);

    if (status != NO_ERROR) {
        goto invalid;
    }

    // special case: number 0-length fields is null
    if (info.info != NULL && strlen (info.info) == 0) {
        info.info = NULL;
    }

    startRequest;
    appendPrintBuf("%s,type=%d,info=%s", printBuf, info.type, info.info);
    closeRequest;
    printRequest(pRI->token, pRI->pCI->requestNumber);

    CALL_ONREQUEST(pRI->pCI->requestNumber, &info, sizeof(info), pRI, pRI->socket_id);

#ifdef MEMSET_FREED
    memsetString(info.info);
#endif
    free (info.info);
#ifdef MEMSET_FREED
    memset(&info, 0, sizeof(info));
#endif

    return;

invalid:
    invalidCommandBlock(pRI);
    return;
}
/* }@ */

static int blockingWrite(int fd, const void *buffer, size_t len) {
    size_t writeOffset = 0;
    const uint8_t *toWrite;

    toWrite = (const uint8_t *)buffer;

    while (writeOffset < len) {
        ssize_t written;
        do {
            written = write(fd, toWrite + writeOffset,
                                len - writeOffset);
        } while (written < 0 && ((errno == EINTR) || (errno == EAGAIN)));

        if (written >= 0) {
            writeOffset += written;
        } else {   // written < 0
            RLOGE("RIL Response: unexpected error on write errno:%d, %s",
                    errno, strerror(errno));
            close(fd);
            return -1;
        }
    }
#if VDBG
    RLOGE("RIL Response bytes written:%d", writeOffset);
#endif
    return 0;
}

static int sendResponseRaw(const void *data, size_t dataSize,
        RIL_SOCKET_ID socket_id, RIL_SOCKET_TYPE socket_type) {
    int ret;
    uint32_t header;
    pthread_mutex_t *writeMutexHook = &s_writeMutex[socket_id];
    int fd = s_rilSocketParam[socket_id].fdCommand;

    if (socket_type == RIL_ATCI_SOCKET) {
        fd = s_atciSocketParam.fdCommand;
        writeMutexHook = &s_atciWriteMutex;
    } else if (socket_type == RIL_IMS_SOCKET) {
        fd = s_imsSocketParam[socket_id].fdCommand;
        writeMutexHook = &s_imsWriteMutex[socket_id];
    } else if (socket_type == RIL_OEM_SOCKET) {
        fd = s_oemSocketParam[socket_id].fdCommand;
        writeMutexHook = &s_oemWriteMutex[socket_id];
    }

#if VDBG
    RLOGE("Send Response to %s", rilSocketIdToString(socket_id));
#endif

    if (fd < 0) {
        return -1;
    }

    if (dataSize > MAX_COMMAND_BYTES) {
        RLOGE("RIL: packet larger than %u (%u)",
                MAX_COMMAND_BYTES, (unsigned int)dataSize);

        return -1;
    }

    pthread_mutex_lock(writeMutexHook);

    header = htonl(dataSize);

    ret = blockingWrite(fd, (void *)&header, sizeof(header));

    if (ret < 0) {
        pthread_mutex_unlock(writeMutexHook);
        return ret;
    }

    ret = blockingWrite(fd, data, dataSize);

    if (ret < 0) {
        pthread_mutex_unlock(writeMutexHook);
        return ret;
    }

    pthread_mutex_unlock(writeMutexHook);

    return 0;
}

static int
sendResponse(Parcel &p, RIL_SOCKET_ID socket_id, RIL_SOCKET_TYPE socket_type) {
    printResponse;
    return sendResponseRaw(p.data(), p.dataSize(), socket_id, socket_type);
}

/** response is an int* pointing to an array of ints*/
static int responseInts(Parcel &p, void *response, size_t responselen) {
    int numInts;

    if (response == NULL && responselen != 0) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }
    if (responselen % sizeof(int) != 0) {
        RLOGE("responseInts: invalid response length %d expected multiple of %d\n",
            (int)responselen, (int)sizeof(int));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    int *p_int = (int *)response;

    numInts = responselen / sizeof(int);
    p.writeInt32(numInts);

    /* each int */
    startResponse;
    for (int i = 0; i < numInts; i++) {
        appendPrintBuf("%s%d,", printBuf, p_int[i]);
        p.writeInt32(p_int[i]);
    }
    removeLastChar;
    closeResponse;

    return 0;
}

// Response is an int or RIL_LastCallFailCauseInfo.
// Currently, only Shamu plans to use RIL_LastCallFailCauseInfo.
// TODO(yjl): Let all implementations use RIL_LastCallFailCauseInfo.
static int responseFailCause(Parcel &p, void *response, size_t responselen) {
    if (response == NULL && responselen != 0) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (responselen == sizeof(int)) {
      startResponse;
      int *p_int = (int *) response;
      appendPrintBuf("%s%d,", printBuf, p_int[0]);
      p.writeInt32(p_int[0]);
      removeLastChar;
      closeResponse;
    } else if (responselen == sizeof(RIL_LastCallFailCauseInfo)) {
      startResponse;
      RIL_LastCallFailCauseInfo *p_fail_cause_info =
              (RIL_LastCallFailCauseInfo *)response;
      appendPrintBuf("%s[cause_code=%d,vendor_cause=%s]", printBuf,
              p_fail_cause_info->cause_code, p_fail_cause_info->vendor_cause);
      p.writeInt32(p_fail_cause_info->cause_code);
      writeStringToParcel(p, p_fail_cause_info->vendor_cause);
      removeLastChar;
      closeResponse;
    } else {
      RLOGE("responseFailCause: invalid response length %d expected an int or "
            "RIL_LastCallFailCauseInfo", (int)responselen);
      return RIL_ERRNO_INVALID_RESPONSE;
    }

    return 0;
}

/** response is a char **, pointing to an array of char *'s
    The parcel will begin with the version */
static int responseStringsWithVersion(int version, Parcel &p,
        void *response, size_t responselen) {
    p.writeInt32(version);
    return responseStrings(p, response, responselen);
}

/** response is a char **, pointing to an array of char *'s */
static int responseStrings(Parcel &p, void *response, size_t responselen) {
    int numStrings;

    if (response == NULL && responselen != 0) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }
    if (responselen % sizeof(char *) != 0) {
        RLOGE("responseStrings: invalid response length %d expected multiple of %d\n",
            (int)responselen, (int)sizeof(char *));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (response == NULL) {
        p.writeInt32(0);
    } else {
        char **p_cur = (char **)response;

        numStrings = responselen / sizeof(char *);
        p.writeInt32(numStrings);

        /* each string */
        startResponse;
        for (int i = 0; i < numStrings; i++) {
            appendPrintBuf("%s%s,", printBuf, (char*)p_cur[i]);
            writeStringToParcel(p, p_cur[i]);
        }
        removeLastChar;
        closeResponse;
    }
    return 0;
}


/**
 * NULL strings are accepted
 * FIXME currently ignores responselen
 */
static int responseString(Parcel &p, void *response, size_t responselen __unused) {
    /* one string only */
    startResponse;
    appendPrintBuf("%s%s", printBuf, (char *)response);
    closeResponse;

    writeStringToParcel(p, (const char *)response);

    return 0;
}

static int responseVoid(Parcel &p __unused,
        void *response __unused, size_t responselen __unused) {
    startResponse;
    removeLastChar;
    return 0;
}

static int responseCallList(Parcel &p, void *response, size_t responselen) {
    int num;

    if (response == NULL && responselen != 0) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (responselen % sizeof(RIL_Call *) != 0) {
        RLOGE("responseCallList: invalid response length %d expected multiple of %d\n",
            (int)responselen, (int)sizeof(RIL_Call *));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    startResponse;
    /* number of call info's */
    num = responselen / sizeof(RIL_Call *);
    p.writeInt32(num);

    for (int i = 0; i < num; i++) {
        RIL_Call *p_cur = ((RIL_Call **)response)[i];
        /* each call info */
        p.writeInt32(p_cur->state);
        p.writeInt32(p_cur->index);
        p.writeInt32(p_cur->toa);
        p.writeInt32(p_cur->isMpty);
        p.writeInt32(p_cur->isMT);
        p.writeInt32(p_cur->als);
        p.writeInt32(p_cur->isVoice);
        p.writeInt32(p_cur->isVoicePrivacy);
        if (p_cur->number != NULL) {
            char *numberTmp = strdup(p_cur->number);
            stripNumberFromSipAddress(p_cur->number, numberTmp,
                    strlen(numberTmp) * sizeof(char));
            writeStringToParcel(p, numberTmp);
            free(numberTmp);
        } else {
            writeStringToParcel(p, p_cur->number);
        }
        p.writeInt32(p_cur->numberPresentation);
        writeStringToParcel(p, p_cur->name);
        p.writeInt32(p_cur->namePresentation);
        // Remove when partners upgrade to version 3
        if ((s_callbacks.version < 3) || (p_cur->uusInfo == NULL ||
             p_cur->uusInfo->uusData == NULL)) {
            p.writeInt32(0); /* UUS Information is absent */
        } else {
            RIL_UUS_Info *uusInfo = p_cur->uusInfo;
            p.writeInt32(1); /* UUS Information is present */
            p.writeInt32(uusInfo->uusType);
            p.writeInt32(uusInfo->uusDcs);
            p.writeInt32(uusInfo->uusLength);
            p.write(uusInfo->uusData, uusInfo->uusLength);
        }
        appendPrintBuf("%s[id=%d,%s,toa=%d,",
            printBuf,
            p_cur->index,
            callStateToString(p_cur->state),
            p_cur->toa);
        appendPrintBuf("%s%s,%s,als=%d,%s,%s,",
            printBuf,
            (p_cur->isMpty)?"conf":"norm",
            (p_cur->isMT)?"mt":"mo",
            p_cur->als,
            (p_cur->isVoice)?"voc":"nonvoc",
            (p_cur->isVoicePrivacy)?"evp":"noevp");
        if (s_isUserdebug) {
            appendPrintBuf("%s%s,cli=%d,name='%s',%d]",
                printBuf,
                p_cur->number,
                p_cur->numberPresentation,
                p_cur->name,
                p_cur->namePresentation);
        } else {
            appendPrintBuf("%s****,cli=%d,name='%s',%d]",
                printBuf,
                p_cur->numberPresentation,
                p_cur->name,
                p_cur->namePresentation);
        }
    }
    removeLastChar;
    closeResponse;

    return 0;
}

static int responseSMS(Parcel &p, void *response, size_t responselen) {
    if (response == NULL) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (responselen != sizeof(RIL_SMS_Response)) {
        RLOGE("invalid response length %d expected %d",
                (int)responselen, (int)sizeof(RIL_SMS_Response));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    RIL_SMS_Response *p_cur = (RIL_SMS_Response *)response;

    p.writeInt32(p_cur->messageRef);
    writeStringToParcel(p, p_cur->ackPDU);
    p.writeInt32(p_cur->errorCode);

    startResponse;
    appendPrintBuf("%s%d,%s,%d", printBuf, p_cur->messageRef,
        (char *)p_cur->ackPDU, p_cur->errorCode);
    closeResponse;

    return 0;
}

static int responseDataCallListV4(Parcel &p, void *response, size_t responselen) {
    if (response == NULL && responselen != 0) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (responselen % sizeof(RIL_Data_Call_Response_v4) != 0) {
        RLOGE("responseDataCallListV4: invalid response length %d expected multiple of %d",
                (int)responselen, (int)sizeof(RIL_Data_Call_Response_v4));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    // Write version
    p.writeInt32(4);

    int num = responselen / sizeof(RIL_Data_Call_Response_v4);
    p.writeInt32(num);

    RIL_Data_Call_Response_v4 *p_cur = (RIL_Data_Call_Response_v4 *)response;
    startResponse;
    int i;
    for (i = 0; i < num; i++) {
        p.writeInt32(p_cur[i].cid);
        p.writeInt32(p_cur[i].active);
        writeStringToParcel(p, p_cur[i].type);
        // apn is not used, so don't send.
        writeStringToParcel(p, p_cur[i].address);
        appendPrintBuf("%s[cid=%d,%s,%s,%s],", printBuf,
            p_cur[i].cid,
            (p_cur[i].active == 0)?"down":"up",
            (char *)p_cur[i].type,
            (char *)p_cur[i].address);
    }
    removeLastChar;
    closeResponse;

    return 0;
}

static int responseDataCallListV6(Parcel &p, void *response, size_t responselen) {
    if (response == NULL && responselen != 0) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (responselen % sizeof(RIL_Data_Call_Response_v6) != 0) {
        RLOGE("responseDataCallListV6: invalid response length %d expected multiple of %d",
                (int)responselen, (int)sizeof(RIL_Data_Call_Response_v6));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    // Write version
    p.writeInt32(6);

    int num = responselen / sizeof(RIL_Data_Call_Response_v6);
    p.writeInt32(num);

    RIL_Data_Call_Response_v6 *p_cur = (RIL_Data_Call_Response_v6 *)response;
    startResponse;
    int i;
    for (i = 0; i < num; i++) {
        p.writeInt32((int)p_cur[i].status);
        p.writeInt32(p_cur[i].suggestedRetryTime);
        p.writeInt32(p_cur[i].cid);
        p.writeInt32(p_cur[i].active);
        writeStringToParcel(p, p_cur[i].type);
        writeStringToParcel(p, p_cur[i].ifname);
        writeStringToParcel(p, p_cur[i].addresses);
        writeStringToParcel(p, p_cur[i].dnses);
        writeStringToParcel(p, p_cur[i].gateways);
        appendPrintBuf("%s[status=%d,retry=%d,cid=%d,%s,%s,%s,%s,%s,%s],", printBuf,
            p_cur[i].status,
            p_cur[i].suggestedRetryTime,
            p_cur[i].cid,
            (p_cur[i].active == 0)? "down" : "up",
            (char *)p_cur[i].type,
            (char *)p_cur[i].ifname,
            (char *)p_cur[i].addresses,
            (char *)p_cur[i].dnses,
            (char *)p_cur[i].gateways);
    }
    removeLastChar;
    closeResponse;

    return 0;
}

static int responseDataCallListV9(Parcel &p, void *response, size_t responselen) {
    if (response == NULL && responselen != 0) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (responselen % sizeof(RIL_Data_Call_Response_v9) != 0) {
        RLOGE("responseDataCallListV9: invalid response length %d expected multiple of %d",
                (int)responselen, (int)sizeof(RIL_Data_Call_Response_v9));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    // Write version
    p.writeInt32(10);

    int num = responselen / sizeof(RIL_Data_Call_Response_v9);
    p.writeInt32(num);

    RIL_Data_Call_Response_v9 *p_cur = (RIL_Data_Call_Response_v9 *) response;
    startResponse;
    int i;
    for (i = 0; i < num; i++) {
        p.writeInt32((int)p_cur[i].status);
        p.writeInt32(p_cur[i].suggestedRetryTime);
        p.writeInt32(p_cur[i].cid);
        p.writeInt32(p_cur[i].active);
        writeStringToParcel(p, p_cur[i].type);
        writeStringToParcel(p, p_cur[i].ifname);
        writeStringToParcel(p, p_cur[i].addresses);
        writeStringToParcel(p, p_cur[i].dnses);
        writeStringToParcel(p, p_cur[i].gateways);
        writeStringToParcel(p, p_cur[i].pcscf);
        appendPrintBuf("%s[status=%d,retry=%d,cid=%d,%s,%s,%s,%s,%s,%s,%s],", printBuf,
            p_cur[i].status,
            p_cur[i].suggestedRetryTime,
            p_cur[i].cid,
            (p_cur[i].active == 0)? "down" : "up",
            (char *)p_cur[i].type,
            (char *)p_cur[i].ifname,
            (char *)p_cur[i].addresses,
            (char *)p_cur[i].dnses,
            (char *)p_cur[i].gateways,
            (char *)p_cur[i].pcscf);
    }
    removeLastChar;
    closeResponse;

    return 0;
}

static int responseDataCallListV11(Parcel &p, void *response, size_t responselen) {
    if (response == NULL && responselen != 0) {
                RLOGE("invalid response: NULL");
                return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (responselen % sizeof(RIL_Data_Call_Response_v11) != 0) {
        RLOGE("invalid response length %d expected multiple of %d",
        (int)responselen, (int)sizeof(RIL_Data_Call_Response_v11));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    // Write version
    p.writeInt32(11);

    int num = responselen / sizeof(RIL_Data_Call_Response_v11);
    p.writeInt32(num);

    RIL_Data_Call_Response_v11 *p_cur = (RIL_Data_Call_Response_v11 *) response;
    startResponse;
    int i;
    for (i = 0; i < num; i++) {
        p.writeInt32((int)p_cur[i].status);
        p.writeInt32(p_cur[i].suggestedRetryTime);
        p.writeInt32(p_cur[i].cid);
        p.writeInt32(p_cur[i].active);
        writeStringToParcel(p, p_cur[i].type);
        writeStringToParcel(p, p_cur[i].ifname);
        writeStringToParcel(p, p_cur[i].addresses);
        writeStringToParcel(p, p_cur[i].dnses);
        writeStringToParcel(p, p_cur[i].gateways);
        writeStringToParcel(p, p_cur[i].pcscf);
        p.writeInt32(p_cur[i].mtu);
        appendPrintBuf("%s[status=%d,retry=%d,cid=%d,%s,%s,%s,%s,%s,%s,%s,mtu=%d],", printBuf,
        p_cur[i].status,
        p_cur[i].suggestedRetryTime,
        p_cur[i].cid,
        (p_cur[i].active==0)?"down":"up",
        (char*)p_cur[i].type,
        (char*)p_cur[i].ifname,
        (char*)p_cur[i].addresses,
        (char*)p_cur[i].dnses,
        (char*)p_cur[i].gateways,
        (char*)p_cur[i].pcscf,
        p_cur[i].mtu);
    }
    removeLastChar;
    closeResponse;

    return 0;
}

static int responseDataCallList(Parcel &p, void *response, size_t responselen)
{
    if (s_callbacks.version <= LAST_IMPRECISE_RIL_VERSION) {
        if (s_callbacks.version < 5) {
            RLOGD("responseDataCallList: v4");
            return responseDataCallListV4(p, response, responselen);
        } else if (responselen % sizeof(RIL_Data_Call_Response_v6) == 0) {
            return responseDataCallListV6(p, response, responselen);
        } else if (responselen % sizeof(RIL_Data_Call_Response_v9) == 0) {
            return responseDataCallListV9(p, response, responselen);
        } else {
            return responseDataCallListV11(p, response, responselen);
        }
    } else { // RIL version >= 13
        if (responselen % sizeof(RIL_Data_Call_Response_v11) != 0) {
            RLOGE("Data structure expected is RIL_Data_Call_Response_v11");
            if (!isDebuggable()) {
                return RIL_ERRNO_INVALID_RESPONSE;
            } else {
                assert(0);
            }
        }
        return responseDataCallListV11(p, response, responselen);
    }
}

static int responseSetupDataCall(Parcel &p, void *response, size_t responselen) {
    if (s_callbacks.version < 5) {
        return responseStringsWithVersion(s_callbacks.version, p, response, responselen);
    } else {
        return responseDataCallList(p, response, responselen);
    }
}

static int responseRaw(Parcel &p, void *response, size_t responselen) {
    if (response == NULL && responselen != 0) {
        RLOGE("invalid response: NULL with responselen != 0");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    // The java code reads -1 size as null byte array
    if (response == NULL) {
        p.writeInt32(-1);
    } else {
        p.writeInt32(responselen);
        p.write(response, responselen);
    }

    return 0;
}

static int responseSIM_IO(Parcel &p, void *response, size_t responselen) {
    if (response == NULL) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (responselen != sizeof(RIL_SIM_IO_Response)) {
        RLOGE("invalid response length was %d expected %d",
                (int)responselen, (int)sizeof(RIL_SIM_IO_Response));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    RIL_SIM_IO_Response *p_cur = (RIL_SIM_IO_Response *)response;
    p.writeInt32(p_cur->sw1);
    p.writeInt32(p_cur->sw2);
    writeStringToParcel(p, p_cur->simResponse);

    startResponse;
    appendPrintBuf("%ssw1=0x%X,sw2=0x%X,%s", printBuf, p_cur->sw1, p_cur->sw2,
        (char *)p_cur->simResponse);
    closeResponse;


    return 0;
}

static int responseCallForwards(Parcel &p, void *response, size_t responselen) {
    int num;

    if (response == NULL && responselen != 0) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (responselen % sizeof(RIL_CallForwardInfo *) != 0) {
        RLOGE("responseCallForwards: invalid response length %d expected multiple of %d",
                (int)responselen, (int)sizeof(RIL_CallForwardInfo *));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    /* number of call info's */
    num = responselen / sizeof(RIL_CallForwardInfo *);
    p.writeInt32(num);

    startResponse;
    for (int i = 0; i < num; i++) {
        RIL_CallForwardInfo *p_cur = ((RIL_CallForwardInfo **)response)[i];

        p.writeInt32(p_cur->status);
        p.writeInt32(p_cur->reason);
        p.writeInt32(p_cur->serviceClass);
        p.writeInt32(p_cur->toa);
        writeStringToParcel(p, p_cur->number);
        p.writeInt32(p_cur->timeSeconds);
        appendPrintBuf("%s[%s,reason=%d,cls=%d,toa=%d,%s,tout=%d],", printBuf,
            (p_cur->status == 1)? "enable" : "disable",
            p_cur->reason, p_cur->serviceClass, p_cur->toa,
            (char *)p_cur->number,
            p_cur->timeSeconds);
    }
    removeLastChar;
    closeResponse;

    return 0;
}

static int responseSsn(Parcel &p, void *response, size_t responselen) {
    if (response == NULL) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (responselen != sizeof(RIL_SuppSvcNotification)) {
        RLOGE("invalid response length was %d expected %d",
                (int)responselen, (int)sizeof(RIL_SuppSvcNotification));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    RIL_SuppSvcNotification *p_cur = (RIL_SuppSvcNotification *)response;
    p.writeInt32(p_cur->notificationType);
    p.writeInt32(p_cur->code);
    p.writeInt32(p_cur->index);
    p.writeInt32(p_cur->type);
    writeStringToParcel(p, p_cur->number);

    startResponse;
    appendPrintBuf("%s%s,code=%d,id=%d,type=%d,%s", printBuf,
        (p_cur->notificationType == 0)? "mo" : "mt",
         p_cur->code, p_cur->index, p_cur->type,
        (char *)p_cur->number);
    closeResponse;

    return 0;
}

static int responseCellList(Parcel &p, void *response, size_t responselen) {
    int num;

    if (response == NULL && responselen != 0) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (responselen % sizeof(RIL_NeighboringCell *) != 0) {
        RLOGE("responseCellList: invalid response length %d expected multiple of %d\n",
            (int)responselen, (int)sizeof(RIL_NeighboringCell *));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    startResponse;
    /* number of records */
    num = responselen / sizeof(RIL_NeighboringCell *);
    p.writeInt32(num);

    for (int i = 0; i < num; i++) {
        RIL_NeighboringCell *p_cur = ((RIL_NeighboringCell **)response)[i];

        p.writeInt32(p_cur->rssi);
        writeStringToParcel(p, p_cur->cid);

        appendPrintBuf("%s[cid=%s,rssi=%d],", printBuf,
            p_cur->cid, p_cur->rssi);
    }
    removeLastChar;
    closeResponse;

    return 0;
}

/**
 * Marshall the signalInfoRecord into the parcel if it exists.
 */
static void marshallSignalInfoRecord(Parcel &p,
            RIL_CDMA_SignalInfoRecord &p_signalInfoRecord) {
    p.writeInt32(p_signalInfoRecord.isPresent);
    p.writeInt32(p_signalInfoRecord.signalType);
    p.writeInt32(p_signalInfoRecord.alertPitch);
    p.writeInt32(p_signalInfoRecord.signal);
}

static int responseCdmaInformationRecords(Parcel &p,
            void *response, size_t responselen) {
    int num;
    char *string8 = NULL;
    int buffer_lenght;
    RIL_CDMA_InformationRecord *infoRec;

    if (response == NULL && responselen != 0) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (responselen != sizeof(RIL_CDMA_InformationRecords)) {
        RLOGE("responseCdmaInformationRecords: invalid response length %d expected multiple of %d\n",
            (int)responselen, (int)sizeof(RIL_CDMA_InformationRecords *));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    RIL_CDMA_InformationRecords *p_cur =
                             (RIL_CDMA_InformationRecords *)response;
    num = MIN(p_cur->numberOfInfoRecs, RIL_CDMA_MAX_NUMBER_OF_INFO_RECS);

    startResponse;
    p.writeInt32(num);

    for (int i = 0; i < num; i++) {
        infoRec = &p_cur->infoRec[i];
        p.writeInt32(infoRec->name);
        switch (infoRec->name) {
            case RIL_CDMA_DISPLAY_INFO_REC:
            case RIL_CDMA_EXTENDED_DISPLAY_INFO_REC: {
                if (infoRec->rec.display.alpha_len >
                    CDMA_ALPHA_INFO_BUFFER_LENGTH) {
                    RLOGE("invalid display info response length %d \
                          expected not more than %d\n",
                         (int)infoRec->rec.display.alpha_len,
                         CDMA_ALPHA_INFO_BUFFER_LENGTH);
                    return RIL_ERRNO_INVALID_RESPONSE;
                }
                string8 = (char *)malloc((infoRec->rec.display.alpha_len + 1)
                                                             * sizeof(char));
                for (int i = 0; i < infoRec->rec.display.alpha_len; i++) {
                    string8[i] = infoRec->rec.display.alpha_buf[i];
                }
                string8[(int)infoRec->rec.display.alpha_len] = '\0';
                writeStringToParcel(p, (const char *)string8);
                free(string8);
                string8 = NULL;
                break;
            }
            case RIL_CDMA_CALLED_PARTY_NUMBER_INFO_REC:
            case RIL_CDMA_CALLING_PARTY_NUMBER_INFO_REC:
            case RIL_CDMA_CONNECTED_NUMBER_INFO_REC: {
                if (infoRec->rec.number.len > CDMA_NUMBER_INFO_BUFFER_LENGTH) {
                    RLOGE("invalid display info response length %d \
                          expected not more than %d\n",
                         (int)infoRec->rec.number.len,
                         CDMA_NUMBER_INFO_BUFFER_LENGTH);
                    return RIL_ERRNO_INVALID_RESPONSE;
                }
                string8 = (char *)malloc((infoRec->rec.number.len + 1)
                                                             * sizeof(char));
                for (int i = 0; i < infoRec->rec.number.len; i++) {
                    string8[i] = infoRec->rec.number.buf[i];
                }
                string8[(int)infoRec->rec.number.len] = '\0';
                writeStringToParcel(p, (const char *)string8);
                free(string8);
                string8 = NULL;
                p.writeInt32(infoRec->rec.number.number_type);
                p.writeInt32(infoRec->rec.number.number_plan);
                p.writeInt32(infoRec->rec.number.pi);
                p.writeInt32(infoRec->rec.number.si);
                break;
            }
            case RIL_CDMA_SIGNAL_INFO_REC: {
                p.writeInt32(infoRec->rec.signal.isPresent);
                p.writeInt32(infoRec->rec.signal.signalType);
                p.writeInt32(infoRec->rec.signal.alertPitch);
                p.writeInt32(infoRec->rec.signal.signal);

                appendPrintBuf("%sisPresent = %X, signalType = %X, \
                                alertPitch = %X, signal = %X, ",
                   printBuf, (int)infoRec->rec.signal.isPresent,
                   (int)infoRec->rec.signal.signalType,
                   (int)infoRec->rec.signal.alertPitch,
                   (int)infoRec->rec.signal.signal);
                removeLastChar;
                break;
            }
            case RIL_CDMA_REDIRECTING_NUMBER_INFO_REC: {
                if (infoRec->rec.redir.redirectingNumber.len >
                                              CDMA_NUMBER_INFO_BUFFER_LENGTH) {
                    RLOGE("invalid display info response length %d \
                          expected not more than %d\n",
                         (int)infoRec->rec.redir.redirectingNumber.len,
                         CDMA_NUMBER_INFO_BUFFER_LENGTH);
                    return RIL_ERRNO_INVALID_RESPONSE;
                }
                string8 = (char *)malloc((infoRec->rec.redir.redirectingNumber
                                          .len + 1) * sizeof(char) );
                for (int i = 0; i < infoRec->rec.redir.redirectingNumber.len;
                     i++) {
                    string8[i] = infoRec->rec.redir.redirectingNumber.buf[i];
                }
                string8[(int)infoRec->rec.redir.redirectingNumber.len] = '\0';
                writeStringToParcel(p, (const char *)string8);
                free(string8);
                string8 = NULL;
                p.writeInt32(infoRec->rec.redir.redirectingNumber.number_type);
                p.writeInt32(infoRec->rec.redir.redirectingNumber.number_plan);
                p.writeInt32(infoRec->rec.redir.redirectingNumber.pi);
                p.writeInt32(infoRec->rec.redir.redirectingNumber.si);
                p.writeInt32(infoRec->rec.redir.redirectingReason);
                break;
            }
            case RIL_CDMA_LINE_CONTROL_INFO_REC: {
                p.writeInt32(infoRec->rec.lineCtrl.lineCtrlPolarityIncluded);
                p.writeInt32(infoRec->rec.lineCtrl.lineCtrlToggle);
                p.writeInt32(infoRec->rec.lineCtrl.lineCtrlReverse);
                p.writeInt32(infoRec->rec.lineCtrl.lineCtrlPowerDenial);

                appendPrintBuf("%slineCtrlPolarityIncluded = %d, \
                                lineCtrlToggle = %d, lineCtrlReverse = %d, \
                                lineCtrlPowerDenial = %d, ", printBuf,
                       (int)infoRec->rec.lineCtrl.lineCtrlPolarityIncluded,
                       (int)infoRec->rec.lineCtrl.lineCtrlToggle,
                       (int)infoRec->rec.lineCtrl.lineCtrlReverse,
                       (int)infoRec->rec.lineCtrl.lineCtrlPowerDenial);
                removeLastChar;
                break;
            }
            case RIL_CDMA_T53_CLIR_INFO_REC: {
                p.writeInt32((int)(infoRec->rec.clir.cause));

                appendPrintBuf("%scause%d", printBuf, infoRec->rec.clir.cause);
                removeLastChar;
                break;
            }
            case RIL_CDMA_T53_AUDIO_CONTROL_INFO_REC: {
                p.writeInt32(infoRec->rec.audioCtrl.upLink);
                p.writeInt32(infoRec->rec.audioCtrl.downLink);

                appendPrintBuf("%supLink=%d, downLink=%d, ", printBuf,
                        infoRec->rec.audioCtrl.upLink,
                        infoRec->rec.audioCtrl.downLink);
                removeLastChar;
                break;
            }
            case RIL_CDMA_T53_RELEASE_INFO_REC:
                // TODO(Moto): See David Krause, he has the answer:)
                RLOGE("RIL_CDMA_T53_RELEASE_INFO_REC: return INVALID_RESPONSE");
                return RIL_ERRNO_INVALID_RESPONSE;
            default:
                RLOGE("Incorrect name value");
                return RIL_ERRNO_INVALID_RESPONSE;
        }
    }
    closeResponse;

    return 0;
}

static void responseRilSignalStrengthV5(Parcel &p, RIL_SignalStrength_v10 *p_cur) {
    p.writeInt32(p_cur->GW_SignalStrength.signalStrength);
    p.writeInt32(p_cur->GW_SignalStrength.bitErrorRate);
    p.writeInt32(p_cur->CDMA_SignalStrength.dbm);
    p.writeInt32(p_cur->CDMA_SignalStrength.ecio);
    p.writeInt32(p_cur->EVDO_SignalStrength.dbm);
    p.writeInt32(p_cur->EVDO_SignalStrength.ecio);
    p.writeInt32(p_cur->EVDO_SignalStrength.signalNoiseRatio);
}

static void responseRilSignalStrengthV6Extra(Parcel &p, RIL_SignalStrength_v10 *p_cur) {
    /*
     * Fixup LTE for backwards compatibility
     */
    // signalStrength: -1 -> 99
    if (p_cur->LTE_SignalStrength.signalStrength == -1) {
        p_cur->LTE_SignalStrength.signalStrength = 99;
    }
    // rsrp: -1 -> INT_MAX all other negative value to positive.
    // So remap here
    if (p_cur->LTE_SignalStrength.rsrp == -1) {
        p_cur->LTE_SignalStrength.rsrp = INT_MAX;
    } else if (p_cur->LTE_SignalStrength.rsrp < -1) {
        p_cur->LTE_SignalStrength.rsrp = -p_cur->LTE_SignalStrength.rsrp;
    }
    // rsrq: -1 -> INT_MAX
    if (p_cur->LTE_SignalStrength.rsrq == -1) {
        p_cur->LTE_SignalStrength.rsrq = INT_MAX;
    }
    // Not remapping rssnr is already using INT_MAX

    // cqi: -1 -> INT_MAX
    if (p_cur->LTE_SignalStrength.cqi == -1) {
        p_cur->LTE_SignalStrength.cqi = INT_MAX;
    }

    p.writeInt32(p_cur->LTE_SignalStrength.signalStrength);
    p.writeInt32(p_cur->LTE_SignalStrength.rsrp);
    p.writeInt32(p_cur->LTE_SignalStrength.rsrq);
    p.writeInt32(p_cur->LTE_SignalStrength.rssnr);
    p.writeInt32(p_cur->LTE_SignalStrength.cqi);
}

static void responseRilSignalStrengthV10(Parcel &p, RIL_SignalStrength_v10 *p_cur) {
    responseRilSignalStrengthV5(p, p_cur);
    responseRilSignalStrengthV6Extra(p, p_cur);
    p.writeInt32(p_cur->TD_SCDMA_SignalStrength.rscp);
}

static int responseRilSignalStrength(Parcel &p,
                    void *response, size_t responselen) {
    if (response == NULL && responselen != 0) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }
    RIL_SignalStrength_v10 *p_cur = ((RIL_SignalStrength_v10 *) response);
    if (s_callbacks.version <= LAST_IMPRECISE_RIL_VERSION) {
        if (responselen >= sizeof (RIL_SignalStrength_v5)) {

            responseRilSignalStrengthV5(p, p_cur);

            if (responselen >= sizeof (RIL_SignalStrength_v6)) {
                responseRilSignalStrengthV6Extra(p, p_cur);
                if (responselen >= sizeof (RIL_SignalStrength_v10)) {
                    p.writeInt32(p_cur->TD_SCDMA_SignalStrength.rscp);
                } else {
                    p.writeInt32(INT_MAX);
                }
            } else {
                p.writeInt32(99);
                p.writeInt32(INT_MAX);
                p.writeInt32(INT_MAX);
                p.writeInt32(INT_MAX);
                p.writeInt32(INT_MAX);
                p.writeInt32(INT_MAX);
            }
        } else {
            RLOGE("invalid response length");
            return RIL_ERRNO_INVALID_RESPONSE;
        }
    } else { // RIL version >= 13
        if (responselen % sizeof(RIL_SignalStrength_v10) != 0) {
            RLOGE("Data structure expected is RIL_SignalStrength_v10");
            if (!isDebuggable()) {
                return RIL_ERRNO_INVALID_RESPONSE;
            } else {
                assert(0);
            }
        }
        responseRilSignalStrengthV10(p, p_cur);
    }
    startResponse;
    appendPrintBuf("%ssignalStrength = %d, LTE_SS.rsrp = %d", printBuf,
                    p_cur->GW_SignalStrength.signalStrength,
                    p_cur->LTE_SignalStrength.rsrp);
    closeResponse;
    return 0;
}

static int responseCallRing(Parcel &p, void *response, size_t responselen) {
    if ((response == NULL) || (responselen == 0)) {
        return responseVoid(p, response, responselen);
    } else {
        return responseCdmaSignalInfoRecord(p, response, responselen);
    }
}

static int responseCdmaSignalInfoRecord(Parcel &p, void *response, size_t responselen) {
    if (response == NULL || responselen == 0) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (responselen != sizeof(RIL_CDMA_SignalInfoRecord)) {
        RLOGE("invalid response length %d expected sizeof(RIL_CDMA_SignalInfoRecord) of %d\n",
            (int)responselen, (int)sizeof(RIL_CDMA_SignalInfoRecord));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    startResponse;

    RIL_CDMA_SignalInfoRecord *p_cur = ((RIL_CDMA_SignalInfoRecord *)response);
    marshallSignalInfoRecord(p, *p_cur);

    appendPrintBuf("%s[isPresent=%d, signalType=%d, alertPitch=%d, signal=%d]",
              printBuf,
              p_cur->isPresent,
              p_cur->signalType,
              p_cur->alertPitch,
              p_cur->signal);

    closeResponse;
    return 0;
}

static int responseCdmaCallWaiting(Parcel &p, void *response,
            size_t responselen) {
    if (response == NULL && responselen != 0) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (responselen < sizeof(RIL_CDMA_CallWaiting_v6)) {
        RLOGW("Upgrade to ril version %d\n", RIL_VERSION);
    }

    RIL_CDMA_CallWaiting_v6 *p_cur = ((RIL_CDMA_CallWaiting_v6 *)response);

    writeStringToParcel(p, p_cur->number);
    p.writeInt32(p_cur->numberPresentation);
    writeStringToParcel(p, p_cur->name);
    marshallSignalInfoRecord(p, p_cur->signalInfoRecord);

    if (s_callbacks.version <= LAST_IMPRECISE_RIL_VERSION) {
        if (responselen >= sizeof(RIL_CDMA_CallWaiting_v6)) {
            p.writeInt32(p_cur->number_type);
            p.writeInt32(p_cur->number_plan);
        } else {
            p.writeInt32(0);
            p.writeInt32(0);
        }
    } else { // RIL version >= 13
        if (responselen % sizeof(RIL_CDMA_CallWaiting_v6) != 0) {
            RLOGE("Data structure expected is RIL_CDMA_CallWaiting_v6");
            if (!isDebuggable()) {
                return RIL_ERRNO_INVALID_RESPONSE;
            } else {
                assert(0);
            }
        }
        p.writeInt32(p_cur->number_type);
        p.writeInt32(p_cur->number_plan);
    }

    startResponse;
    appendPrintBuf("%snumber = %s, numberPresentation = %d, name = %s, \
            signalInfoRecord[isPresent = %d, signalType = %d, alertPitch = %d, \
            signal = %d, number_type = %d, number_plan = %d]",
            printBuf,
            p_cur->number,
            p_cur->numberPresentation,
            p_cur->name,
            p_cur->signalInfoRecord.isPresent,
            p_cur->signalInfoRecord.signalType,
            p_cur->signalInfoRecord.alertPitch,
            p_cur->signalInfoRecord.signal,
            p_cur->number_type,
            p_cur->number_plan);
    closeResponse;

    return 0;
}

static void responseSimRefreshV7(Parcel &p, void *response) {
      RIL_SimRefreshResponse_v7 *p_cur = ((RIL_SimRefreshResponse_v7 *) response);
      p.writeInt32(p_cur->result);
      writeStringToParcel(p, p_cur->ef_id);
      if (p_cur->aid != NULL && strcmp(p_cur->aid, "")) {
          writeStringToParcel(p, p_cur->aid);
      } else {
          writeStringToParcel(p, NULL);
      }

      appendPrintBuf("%sresult=%d, ef_id=%s, aid=%s",
            printBuf,
            p_cur->result,
            p_cur->ef_id,
            p_cur->aid);

}

static int responseSimRefresh(Parcel &p, void *response, size_t responselen) {
    if (response == NULL && responselen != 0) {
        RLOGE("responseSimRefresh: invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    startResponse;
    if (s_callbacks.version <= LAST_IMPRECISE_RIL_VERSION) {
        if (s_callbacks.version >= 7) {
            responseSimRefreshV7(p, response);
        } else {
            int *p_cur = ((int *) response);
            p.writeInt32(p_cur[0]);
            p.writeInt32(p_cur[1]);
            writeStringToParcel(p, NULL);

            appendPrintBuf("%sresult=%d, ef_id=%d",
                    printBuf,
                    p_cur[0],
                    p_cur[1]);
        }
    } else { // RIL version >= 13
        if (responselen % sizeof(RIL_SimRefreshResponse_v7) != 0) {
            RLOGE("Data structure expected is RIL_SimRefreshResponse_v7");
            if (!isDebuggable()) {
                return RIL_ERRNO_INVALID_RESPONSE;
            } else {
                assert(0);
            }
        }
        responseSimRefreshV7(p, response);

    }
    closeResponse;

    return 0;
}

static int responseCellInfoListV6(Parcel &p, void *response, size_t responselen) {
    if (response == NULL && responselen != 0) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (responselen % sizeof(RIL_CellInfo) != 0) {
        RLOGE("responseCellInfoList: invalid response length %d expected multiple of %d",
                (int)responselen, (int)sizeof(RIL_CellInfo));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    int num = responselen / sizeof(RIL_CellInfo);
    p.writeInt32(num);

    RIL_CellInfo *p_cur = (RIL_CellInfo *) response;
    startResponse;
    int i;
    for (i = 0; i < num; i++) {
        p.writeInt32((int)p_cur->cellInfoType);
        p.writeInt32(p_cur->registered);
        p.writeInt32(p_cur->timeStampType);
        p.writeInt64(p_cur->timeStamp);
        switch(p_cur->cellInfoType) {
            case RIL_CELL_INFO_TYPE_GSM: {
                p.writeInt32(p_cur->CellInfo.gsm.cellIdentityGsm.mcc);
                p.writeInt32(p_cur->CellInfo.gsm.cellIdentityGsm.mnc);
                p.writeInt32(p_cur->CellInfo.gsm.cellIdentityGsm.lac);
                p.writeInt32(p_cur->CellInfo.gsm.cellIdentityGsm.cid);
                p.writeInt32(p_cur->CellInfo.gsm.signalStrengthGsm.signalStrength);
                p.writeInt32(p_cur->CellInfo.gsm.signalStrengthGsm.bitErrorRate);
                break;
            }
            case RIL_CELL_INFO_TYPE_WCDMA: {
                p.writeInt32(p_cur->CellInfo.wcdma.cellIdentityWcdma.mcc);
                p.writeInt32(p_cur->CellInfo.wcdma.cellIdentityWcdma.mnc);
                p.writeInt32(p_cur->CellInfo.wcdma.cellIdentityWcdma.lac);
                p.writeInt32(p_cur->CellInfo.wcdma.cellIdentityWcdma.cid);
                p.writeInt32(p_cur->CellInfo.wcdma.cellIdentityWcdma.psc);
                p.writeInt32(p_cur->CellInfo.wcdma.signalStrengthWcdma.signalStrength);
                p.writeInt32(p_cur->CellInfo.wcdma.signalStrengthWcdma.bitErrorRate);
                break;
            }
            case RIL_CELL_INFO_TYPE_CDMA: {
                p.writeInt32(p_cur->CellInfo.cdma.cellIdentityCdma.networkId);
                p.writeInt32(p_cur->CellInfo.cdma.cellIdentityCdma.systemId);
                p.writeInt32(p_cur->CellInfo.cdma.cellIdentityCdma.basestationId);
                p.writeInt32(p_cur->CellInfo.cdma.cellIdentityCdma.longitude);
                p.writeInt32(p_cur->CellInfo.cdma.cellIdentityCdma.latitude);

                p.writeInt32(p_cur->CellInfo.cdma.signalStrengthCdma.dbm);
                p.writeInt32(p_cur->CellInfo.cdma.signalStrengthCdma.ecio);
                p.writeInt32(p_cur->CellInfo.cdma.signalStrengthEvdo.dbm);
                p.writeInt32(p_cur->CellInfo.cdma.signalStrengthEvdo.ecio);
                p.writeInt32(p_cur->CellInfo.cdma.signalStrengthEvdo.signalNoiseRatio);
                break;
            }
            case RIL_CELL_INFO_TYPE_LTE: {
                p.writeInt32(p_cur->CellInfo.lte.cellIdentityLte.mcc);
                p.writeInt32(p_cur->CellInfo.lte.cellIdentityLte.mnc);
                p.writeInt32(p_cur->CellInfo.lte.cellIdentityLte.ci);
                p.writeInt32(p_cur->CellInfo.lte.cellIdentityLte.pci);
                p.writeInt32(p_cur->CellInfo.lte.cellIdentityLte.tac);

                p.writeInt32(p_cur->CellInfo.lte.signalStrengthLte.signalStrength);
                p.writeInt32(p_cur->CellInfo.lte.signalStrengthLte.rsrp);
                p.writeInt32(p_cur->CellInfo.lte.signalStrengthLte.rsrq);
                p.writeInt32(p_cur->CellInfo.lte.signalStrengthLte.rssnr);
                p.writeInt32(p_cur->CellInfo.lte.signalStrengthLte.cqi);
                p.writeInt32(p_cur->CellInfo.lte.signalStrengthLte.timingAdvance);
                break;
            }
            case RIL_CELL_INFO_TYPE_TD_SCDMA: {
                p.writeInt32(p_cur->CellInfo.tdscdma.cellIdentityTdscdma.mcc);
                p.writeInt32(p_cur->CellInfo.tdscdma.cellIdentityTdscdma.mnc);
                p.writeInt32(p_cur->CellInfo.tdscdma.cellIdentityTdscdma.lac);
                p.writeInt32(p_cur->CellInfo.tdscdma.cellIdentityTdscdma.cid);
                p.writeInt32(p_cur->CellInfo.tdscdma.cellIdentityTdscdma.cpid);
                p.writeInt32(p_cur->CellInfo.tdscdma.signalStrengthTdscdma.rscp);
                break;
            }
        }
        p_cur += 1;
    }
    removeLastChar;
    closeResponse;

    return 0;
}

static int responseCellInfoListV12(Parcel &p, void *response, size_t responselen) {
    if (response == NULL && responselen != 0) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (responselen % sizeof(RIL_CellInfo_v12) != 0) {
        RLOGE("responseCellInfoList: invalid response length %d expected multiple of %d",
                (int)responselen, (int)sizeof(RIL_CellInfo_v12));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    int num = responselen / sizeof(RIL_CellInfo_v12);
    p.writeInt32(num);

    RIL_CellInfo_v12 *p_cur = (RIL_CellInfo_v12 *) response;
    startResponse;
    int i;
    for (i = 0; i < num; i++) {
        p.writeInt32((int)p_cur->cellInfoType);
        p.writeInt32(p_cur->registered);
        p.writeInt32(p_cur->timeStampType);
        p.writeInt64(p_cur->timeStamp);
        switch(p_cur->cellInfoType) {
            case RIL_CELL_INFO_TYPE_GSM: {
                p.writeInt32(p_cur->CellInfo.gsm.cellIdentityGsm.mcc);
                p.writeInt32(p_cur->CellInfo.gsm.cellIdentityGsm.mnc);
                p.writeInt32(p_cur->CellInfo.gsm.cellIdentityGsm.lac);
                p.writeInt32(p_cur->CellInfo.gsm.cellIdentityGsm.cid);
                p.writeInt32(p_cur->CellInfo.gsm.cellIdentityGsm.arfcn);
                p.writeInt32(p_cur->CellInfo.gsm.cellIdentityGsm.bsic);
                p.writeInt32(p_cur->CellInfo.gsm.signalStrengthGsm.signalStrength);
                p.writeInt32(p_cur->CellInfo.gsm.signalStrengthGsm.bitErrorRate);
                p.writeInt32(p_cur->CellInfo.gsm.signalStrengthGsm.timingAdvance);
                break;
            }
            case RIL_CELL_INFO_TYPE_WCDMA: {
                p.writeInt32(p_cur->CellInfo.wcdma.cellIdentityWcdma.mcc);
                p.writeInt32(p_cur->CellInfo.wcdma.cellIdentityWcdma.mnc);
                p.writeInt32(p_cur->CellInfo.wcdma.cellIdentityWcdma.lac);
                p.writeInt32(p_cur->CellInfo.wcdma.cellIdentityWcdma.cid);
                p.writeInt32(p_cur->CellInfo.wcdma.cellIdentityWcdma.psc);
                p.writeInt32(p_cur->CellInfo.wcdma.cellIdentityWcdma.uarfcn);
                p.writeInt32(p_cur->CellInfo.wcdma.signalStrengthWcdma.signalStrength);
                p.writeInt32(p_cur->CellInfo.wcdma.signalStrengthWcdma.bitErrorRate);
                break;
            }
            case RIL_CELL_INFO_TYPE_CDMA: {
                p.writeInt32(p_cur->CellInfo.cdma.cellIdentityCdma.networkId);
                p.writeInt32(p_cur->CellInfo.cdma.cellIdentityCdma.systemId);
                p.writeInt32(p_cur->CellInfo.cdma.cellIdentityCdma.basestationId);
                p.writeInt32(p_cur->CellInfo.cdma.cellIdentityCdma.longitude);
                p.writeInt32(p_cur->CellInfo.cdma.cellIdentityCdma.latitude);

                p.writeInt32(p_cur->CellInfo.cdma.signalStrengthCdma.dbm);
                p.writeInt32(p_cur->CellInfo.cdma.signalStrengthCdma.ecio);
                p.writeInt32(p_cur->CellInfo.cdma.signalStrengthEvdo.dbm);
                p.writeInt32(p_cur->CellInfo.cdma.signalStrengthEvdo.ecio);
                p.writeInt32(p_cur->CellInfo.cdma.signalStrengthEvdo.signalNoiseRatio);
                break;
            }
            case RIL_CELL_INFO_TYPE_LTE: {
                p.writeInt32(p_cur->CellInfo.lte.cellIdentityLte.mcc);
                p.writeInt32(p_cur->CellInfo.lte.cellIdentityLte.mnc);
                p.writeInt32(p_cur->CellInfo.lte.cellIdentityLte.ci);
                p.writeInt32(p_cur->CellInfo.lte.cellIdentityLte.pci);
                p.writeInt32(p_cur->CellInfo.lte.cellIdentityLte.tac);
                p.writeInt32(p_cur->CellInfo.lte.cellIdentityLte.earfcn);

                p.writeInt32(p_cur->CellInfo.lte.signalStrengthLte.signalStrength);
                p.writeInt32(p_cur->CellInfo.lte.signalStrengthLte.rsrp);
                p.writeInt32(p_cur->CellInfo.lte.signalStrengthLte.rsrq);
                p.writeInt32(p_cur->CellInfo.lte.signalStrengthLte.rssnr);
                p.writeInt32(p_cur->CellInfo.lte.signalStrengthLte.cqi);
                p.writeInt32(p_cur->CellInfo.lte.signalStrengthLte.timingAdvance);
                break;
            }
            case RIL_CELL_INFO_TYPE_TD_SCDMA: {
                p.writeInt32(p_cur->CellInfo.tdscdma.cellIdentityTdscdma.mcc);
                p.writeInt32(p_cur->CellInfo.tdscdma.cellIdentityTdscdma.mnc);
                p.writeInt32(p_cur->CellInfo.tdscdma.cellIdentityTdscdma.lac);
                p.writeInt32(p_cur->CellInfo.tdscdma.cellIdentityTdscdma.cid);
                p.writeInt32(p_cur->CellInfo.tdscdma.cellIdentityTdscdma.cpid);
                p.writeInt32(p_cur->CellInfo.tdscdma.signalStrengthTdscdma.rscp);
                break;
            }
        }
        p_cur += 1;
    }
    removeLastChar;
    closeResponse;
    return 0;
}

static int responseCellInfoList(Parcel &p, void *response, size_t responselen)
{
    if (s_callbacks.version <= LAST_IMPRECISE_RIL_VERSION) {
        if (s_callbacks.version < 12) {
            RLOGD("responseCellInfoList: v6");
            return responseCellInfoListV6(p, response, responselen);
        } else {
            RLOGD("responseCellInfoList: v12");
            return responseCellInfoListV12(p, response, responselen);
        }
    } else { // RIL version >= 13
        if (responselen % sizeof(RIL_CellInfo_v12) != 0) {
            RLOGE("Data structure expected is RIL_CellInfo_v12");
            if (!isDebuggable()) {
                return RIL_ERRNO_INVALID_RESPONSE;
            } else {
                assert(0);
            }
        }
        return responseCellInfoListV12(p, response, responselen);
    }

    return 0;
}

static int responseHardwareConfig(Parcel &p, void *response, size_t responselen) {
    if (response == NULL && responselen != 0) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (responselen % sizeof(RIL_HardwareConfig) != 0) {
        RLOGE("responseHardwareConfig: invalid response length %d expected multiple of %d",
                (int)responselen, (int)sizeof(RIL_HardwareConfig));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    int num = responselen / sizeof(RIL_HardwareConfig);
    int i;
    RIL_HardwareConfig *p_cur = (RIL_HardwareConfig *)response;

    p.writeInt32(num);

    startResponse;
    for (i = 0; i < num; i++) {
        switch (p_cur[i].type) {
            case RIL_HARDWARE_CONFIG_MODEM: {
                writeStringToParcel(p, p_cur[i].uuid);
                p.writeInt32((int)p_cur[i].state);
                p.writeInt32(p_cur[i].cfg.modem.rat);
                p.writeInt32(p_cur[i].cfg.modem.maxVoice);
                p.writeInt32(p_cur[i].cfg.modem.maxData);
                p.writeInt32(p_cur[i].cfg.modem.maxStandby);

                appendPrintBuf("%s modem: uuid=%s,state=%d,rat=%08x,maxV=%d,maxD=%d,maxS=%d",
                    printBuf, p_cur[i].uuid, (int)p_cur[i].state,
                    p_cur[i].cfg.modem.rat, p_cur[i].cfg.modem.maxVoice,
                    p_cur[i].cfg.modem.maxData, p_cur[i].cfg.modem.maxStandby);
                break;
            }
            case RIL_HARDWARE_CONFIG_SIM: {
                writeStringToParcel(p, p_cur[i].uuid);
                p.writeInt32((int)p_cur[i].state);
                writeStringToParcel(p, p_cur[i].cfg.sim.modemUuid);

                appendPrintBuf("%s sim: uuid=%s,state=%d,modem-uuid=%s", printBuf,
                        p_cur[i].uuid, (int)p_cur[i].state, p_cur[i].cfg.sim.modemUuid);
                break;
            }
        }
    }
    removeLastChar;
    closeResponse;
    return 0;
}

static int responseRadioCapability(Parcel &p, void *response, size_t responselen) {
    if (response == NULL) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (responselen != sizeof(RIL_RadioCapability)) {
        RLOGE("invalid response length was %d expected %d",
                (int)responselen, (int)sizeof(RIL_SIM_IO_Response));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    RIL_RadioCapability *p_cur = (RIL_RadioCapability *)response;
    p.writeInt32(p_cur->version);
    p.writeInt32(p_cur->session);
    p.writeInt32(p_cur->phase);
    p.writeInt32(p_cur->rat);
    writeStringToParcel(p, p_cur->logicalModemUuid);
    p.writeInt32(p_cur->status);

    startResponse;
    appendPrintBuf("%s[version = %d, session = %d, phase = %d, \
            rat = %d, logicalModemUuid = %s, status = %d]",
            printBuf,
            p_cur->version,
            p_cur->session,
            p_cur->phase,
            p_cur->rat,
            p_cur->logicalModemUuid,
            p_cur->status);
    closeResponse;
    return 0;
}

static int responseSSData(Parcel &p, void *response, size_t responselen) {
    RLOGD("In responseSSData");
    int num;

    if (response == NULL && responselen != 0) {
        RLOGE("invalid response length was %d expected %d",
                (int)responselen, (int)sizeof(RIL_SIM_IO_Response));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (responselen != sizeof(RIL_StkCcUnsolSsResponse)) {
        RLOGE("invalid response length %d, expected %d",
               (int)responselen, (int)sizeof(RIL_StkCcUnsolSsResponse));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    startResponse;
    RIL_StkCcUnsolSsResponse *p_cur = (RIL_StkCcUnsolSsResponse *)response;
    p.writeInt32(p_cur->serviceType);
    p.writeInt32(p_cur->requestType);
    p.writeInt32(p_cur->teleserviceType);
    p.writeInt32(p_cur->serviceClass);
    p.writeInt32(p_cur->result);

    if (isServiceTypeCfQuery(p_cur->serviceType, p_cur->requestType)) {
        RLOGD("responseSSData CF type, num of Cf elements %d",
                p_cur->cfData.numValidIndexes);
        if (p_cur->cfData.numValidIndexes > NUM_SERVICE_CLASSES) {
            RLOGE("numValidIndexes is greater than max value %d, "
                  "truncating it to max value", NUM_SERVICE_CLASSES);
            p_cur->cfData.numValidIndexes = NUM_SERVICE_CLASSES;
        }
        /* number of call info's */
        p.writeInt32(p_cur->cfData.numValidIndexes);

        for (int i = 0; i < p_cur->cfData.numValidIndexes; i++) {
             RIL_CallForwardInfo cf = p_cur->cfData.cfInfo[i];

             p.writeInt32(cf.status);
             p.writeInt32(cf.reason);
             p.writeInt32(cf.serviceClass);
             p.writeInt32(cf.toa);
             writeStringToParcel(p, cf.number);
             p.writeInt32(cf.timeSeconds);
             appendPrintBuf("%s[%s, reason = %d,cls = %d, toa = %d, %s, tout = %d],",
                     printBuf, (cf.status == 1)? "enable" : "disable",
                     cf.reason, cf.serviceClass, cf.toa,
                     (char*)cf.number, cf.timeSeconds);
             RLOGD("Data: %d, reason = %d, cls = %d, toa = %d, num = %s, tout = %d],",
                     cf.status, cf.reason, cf.serviceClass, cf.toa,
                     (char*)cf.number, cf.timeSeconds);
        }
    } else {
        p.writeInt32(SS_INFO_MAX);

        /* each int*/
        for (int i = 0; i < SS_INFO_MAX; i++) {
             appendPrintBuf("%s%d,", printBuf, p_cur->ssInfo[i]);
             RLOGD("Data: %d", p_cur->ssInfo[i]);
             p.writeInt32(p_cur->ssInfo[i]);
        }
    }
    removeLastChar;
    closeResponse;

    return 0;
}

static bool isServiceTypeCfQuery(RIL_SsServiceType serType, RIL_SsRequestType reqType) {
    if ((reqType == SS_INTERROGATION) &&
        (serType == SS_CFU ||
         serType == SS_CF_BUSY ||
         serType == SS_CF_NO_REPLY ||
         serType == SS_CF_NOT_REACHABLE ||
         serType == SS_CF_ALL ||
         serType == SS_CF_ALL_CONDITIONAL)) {
        return true;
    }
    return false;
}

static void triggerEvLoop() {
    int ret;
    if (!pthread_equal(pthread_self(), s_tidDispatch)) {
        /* trigger event loop to wakeup. No reason to do this,
         * if we're in the event loop thread */
         do {
            ret = write(s_fdWakeupWrite, " ", 1);
         } while (ret < 0 && errno == EINTR);
    }
}

static void rilEventAddWakeup(struct ril_event *ev) {
    ril_event_add(ev);
    triggerEvLoop();
}

static void sendSimStatusAppInfo(Parcel &p, int num_apps, RIL_AppStatus appStatus[]) {
        p.writeInt32(num_apps);
        startResponse;
        for (int i = 0; i < num_apps; i++) {
            p.writeInt32(appStatus[i].app_type);
            p.writeInt32(appStatus[i].app_state);
            p.writeInt32(appStatus[i].perso_substate);
            writeStringToParcel(p, (const char *)(appStatus[i].aid_ptr));
            writeStringToParcel(p, (const char *)
                                          (appStatus[i].app_label_ptr));
            p.writeInt32(appStatus[i].pin1_replaced);
            p.writeInt32(appStatus[i].pin1);
            p.writeInt32(appStatus[i].pin2);
            appendPrintBuf("%s[app_type = %d, app_state = %d, perso_substate = %d, \
                    aid_ptr = %s, app_label_ptr = %s, pin1_replaced = %d, pin1 = %d, pin2 = %d],",
                    printBuf,
                    appStatus[i].app_type,
                    appStatus[i].app_state,
                    appStatus[i].perso_substate,
                    appStatus[i].aid_ptr,
                    appStatus[i].app_label_ptr,
                    appStatus[i].pin1_replaced,
                    appStatus[i].pin1,
                    appStatus[i].pin2);
        }
        closeResponse;
}

static void responseSimStatusV5(Parcel &p, void *response) {
    RIL_CardStatus_v5 *p_cur = ((RIL_CardStatus_v5 *) response);

    p.writeInt32(p_cur->card_state);
    p.writeInt32(p_cur->universal_pin_state);
    p.writeInt32(p_cur->gsm_umts_subscription_app_index);
    p.writeInt32(p_cur->cdma_subscription_app_index);

    sendSimStatusAppInfo(p, p_cur->num_applications, p_cur->applications);
}

static void responseSimStatusV6(Parcel &p, void *response) {
    RIL_CardStatus_v6 *p_cur = ((RIL_CardStatus_v6 *) response);

    p.writeInt32(p_cur->card_state);
    p.writeInt32(p_cur->universal_pin_state);
    p.writeInt32(p_cur->gsm_umts_subscription_app_index);
    p.writeInt32(p_cur->cdma_subscription_app_index);
    p.writeInt32(p_cur->ims_subscription_app_index);

    sendSimStatusAppInfo(p, p_cur->num_applications, p_cur->applications);
}

static int responseSimStatus(Parcel &p, void *response, size_t responselen) {
    int i;

    if (response == NULL && responselen != 0) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (s_callbacks.version <= LAST_IMPRECISE_RIL_VERSION) {
        if (responselen == sizeof (RIL_CardStatus_v6)) {
            responseSimStatusV6(p, response);
        } else if (responselen == sizeof (RIL_CardStatus_v5)) {
            responseSimStatusV5(p, response);
        } else {
            RLOGE("responseSimStatus: A RilCardStatus_v6 or _v5 expected\n");
            return RIL_ERRNO_INVALID_RESPONSE;
        }
    } else { // RIL version >= 13
        if (responselen % sizeof(RIL_CardStatus_v6) != 0) {
            RLOGE("Data structure expected is RIL_CardStatus_v6");
            if (!isDebuggable()) {
                return RIL_ERRNO_INVALID_RESPONSE;
            } else {
                assert(0);
            }
        }
        responseSimStatusV6(p, response);
    }

    return 0;
}

static int responseGsmBrSmsCnf(Parcel &p, void *response, size_t responselen) {
    int num = responselen / sizeof(RIL_GSM_BroadcastSmsConfigInfo *);
    p.writeInt32(num);

    startResponse;
    RIL_GSM_BroadcastSmsConfigInfo **p_cur =
                (RIL_GSM_BroadcastSmsConfigInfo **)response;
    for (int i = 0; i < num; i++) {
        p.writeInt32(p_cur[i]->fromServiceId);
        p.writeInt32(p_cur[i]->toServiceId);
        p.writeInt32(p_cur[i]->fromCodeScheme);
        p.writeInt32(p_cur[i]->toCodeScheme);
        p.writeInt32(p_cur[i]->selected);

        appendPrintBuf("%s [%d: fromServiceId = %d, toServiceId = %d, \
                fromCodeScheme = %d, toCodeScheme = %d, selected = %d]",
                printBuf, i, p_cur[i]->fromServiceId, p_cur[i]->toServiceId,
                p_cur[i]->fromCodeScheme, p_cur[i]->toCodeScheme,
                p_cur[i]->selected);
    }
    closeResponse;

    return 0;
}

static int responseCdmaBrSmsCnf(Parcel &p, void *response, size_t responselen) {
    RIL_CDMA_BroadcastSmsConfigInfo **p_cur =
               (RIL_CDMA_BroadcastSmsConfigInfo **)response;

    int num = responselen / sizeof(RIL_CDMA_BroadcastSmsConfigInfo *);
    p.writeInt32(num);

    startResponse;
    for (int i = 0; i < num; i++) {
        p.writeInt32(p_cur[i]->service_category);
        p.writeInt32(p_cur[i]->language);
        p.writeInt32(p_cur[i]->selected);

        appendPrintBuf("%s [%d: srvice_category = %d, language = %d, \
              selected = %d], ",
              printBuf, i, p_cur[i]->service_category, p_cur[i]->language,
              p_cur[i]->selected);
    }
    closeResponse;

    return 0;
}

static int responseCdmaSms(Parcel &p, void *response, size_t responselen) {
    int num;
    int digitCount;
    int digitLimit;
    uint8_t uct;
    void *dest;

    RLOGD("Inside responseCdmaSms");

    if (response == NULL && responselen != 0) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (responselen != sizeof(RIL_CDMA_SMS_Message)) {
        RLOGE("invalid response length was %d expected %d",
                (int)responselen, (int)sizeof(RIL_CDMA_SMS_Message));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    RIL_CDMA_SMS_Message *p_cur = (RIL_CDMA_SMS_Message *)response;
    p.writeInt32(p_cur->uTeleserviceID);
    p.write(&(p_cur->bIsServicePresent), sizeof(uct));
    p.writeInt32(p_cur->uServicecategory);
    p.writeInt32(p_cur->sAddress.digit_mode);
    p.writeInt32(p_cur->sAddress.number_mode);
    p.writeInt32(p_cur->sAddress.number_type);
    p.writeInt32(p_cur->sAddress.number_plan);
    p.write(&(p_cur->sAddress.number_of_digits), sizeof(uct));
    digitLimit = MIN((p_cur->sAddress.number_of_digits), RIL_CDMA_SMS_ADDRESS_MAX);
    for (digitCount = 0; digitCount < digitLimit; digitCount++) {
        p.write(&(p_cur->sAddress.digits[digitCount]), sizeof(uct));
    }

    p.writeInt32(p_cur->sSubAddress.subaddressType);
    p.write(&(p_cur->sSubAddress.odd), sizeof(uct));
    p.write(&(p_cur->sSubAddress.number_of_digits), sizeof(uct));
    digitLimit = MIN((p_cur->sSubAddress.number_of_digits), RIL_CDMA_SMS_SUBADDRESS_MAX);
    for (digitCount = 0; digitCount < digitLimit; digitCount++) {
        p.write(&(p_cur->sSubAddress.digits[digitCount]), sizeof(uct));
    }

    digitLimit = MIN((p_cur->uBearerDataLen), RIL_CDMA_SMS_BEARER_DATA_MAX);
    p.writeInt32(p_cur->uBearerDataLen);
    for (digitCount = 0; digitCount < digitLimit; digitCount++) {
       p.write(&(p_cur->aBearerData[digitCount]), sizeof(uct));
    }

    startResponse;
    appendPrintBuf("%suTeleserviceID = %d, bIsServicePresent = %d, uServicecategory = %d, \
            sAddress.digit_mode = %d, sAddress.number_mode = %d, sAddress.number_type = %d, ",
            printBuf, p_cur->uTeleserviceID, p_cur->bIsServicePresent, p_cur->uServicecategory,
            p_cur->sAddress.digit_mode, p_cur->sAddress.number_mode, p_cur->sAddress.number_type);
    closeResponse;

    return 0;
}

static int responseDcRtInfo(Parcel &p, void *response, size_t responselen) {
    int num = responselen / sizeof(RIL_DcRtInfo);
    if ((responselen % sizeof(RIL_DcRtInfo) != 0) || (num != 1)) {
        RLOGE("responseDcRtInfo: invalid response length %d expected multiple of %d",
                (int)responselen, (int)sizeof(RIL_DcRtInfo));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    startResponse;
    RIL_DcRtInfo *pDcRtInfo = (RIL_DcRtInfo *)response;
    p.writeInt64(pDcRtInfo->time);
    p.writeInt32(pDcRtInfo->powerState);
//    appendPrintBuf("%s[time=%d,powerState=%d]", printBuf,
//        pDcRtInfo->time,
//        pDcRtInfo->powerState);
    closeResponse;

    return 0;
}

static int responseLceStatus(Parcel &p, void *response, size_t responselen) {
  if (response == NULL || responselen != sizeof(RIL_LceStatusInfo)) {
    if (response == NULL) {
      RLOGE("invalid response: NULL");
    } else {
      RLOGE("responseLceStatus: invalid response length %d expecting len: %d",
            (int)sizeof(RIL_LceStatusInfo), (int)responselen);
    }
    return RIL_ERRNO_INVALID_RESPONSE;
  }

  RIL_LceStatusInfo *p_cur = (RIL_LceStatusInfo *)response;
  p.write((void *)p_cur, 1);  // p_cur->lce_status takes one byte.
  p.writeInt32(p_cur->actual_interval_ms);

  startResponse;
  appendPrintBuf("LCE Status: %d, actual_interval_ms: %d",
                 p_cur->lce_status, p_cur->actual_interval_ms);
  closeResponse;

  return 0;
}

static int responseLceData(Parcel &p, void *response, size_t responselen) {
  if (response == NULL || responselen != sizeof(RIL_LceDataInfo)) {
    if (response == NULL) {
      RLOGE("invalid response: NULL");
    } else {
      RLOGE("responseLceData: invalid response length %d expecting len: %d",
            (int)sizeof(RIL_LceDataInfo), (int)responselen);
    }
    return RIL_ERRNO_INVALID_RESPONSE;
  }

  RIL_LceDataInfo *p_cur = (RIL_LceDataInfo *)response;
  p.writeInt32(p_cur->last_hop_capacity_kbps);

  /* p_cur->confidence_level and p_cur->lce_suspended take 1 byte each.*/
  p.write((void *)&(p_cur->confidence_level), 1);
  p.write((void *)&(p_cur->lce_suspended), 1);

  startResponse;
  appendPrintBuf("LCE info received: capacity %d confidence level %d \
                  and suspended %d",
                  p_cur->last_hop_capacity_kbps, p_cur->confidence_level,
                  p_cur->lce_suspended);
  closeResponse;

  return 0;
}

static int responseActivityData(Parcel &p, void *response, size_t responselen) {
    if (response == NULL || responselen != sizeof(RIL_ActivityStatsInfo)) {
        if (response == NULL) {
          RLOGE("invalid response: NULL");
        } else {
          RLOGE("responseActivityData: invalid response length %d expecting len: %d",
                (int)sizeof(RIL_ActivityStatsInfo), (int)responselen);
        }
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    RIL_ActivityStatsInfo *p_cur = (RIL_ActivityStatsInfo *)response;
    p.writeInt32(p_cur->sleep_mode_time_ms);
    p.writeInt32(p_cur->idle_mode_time_ms);
    for (int i = 0; i < RIL_NUM_TX_POWER_LEVELS; i++) {
        p.writeInt32(p_cur->tx_mode_time_ms[i]);
    }
    p.writeInt32(p_cur->rx_mode_time_ms);

    startResponse;
    appendPrintBuf("Modem activity info received: sleep_mode_time_ms %d idle_mode_time_ms %d \
                    tx_mode_time_ms %d %d %d %d %d and rx_mode_time_ms %d",
                    p_cur->sleep_mode_time_ms, p_cur->idle_mode_time_ms, p_cur->tx_mode_time_ms[0],
                    p_cur->tx_mode_time_ms[1], p_cur->tx_mode_time_ms[2], p_cur->tx_mode_time_ms[3],
                    p_cur->tx_mode_time_ms[4], p_cur->rx_mode_time_ms);
    closeResponse;

    return 0;
}

static int responseCallListIMS(Parcel &p, void *response, size_t responselen) {
    int num;

    if (response == NULL && responselen != 0) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (responselen % sizeof(RIL_Call_VoLTE *) != 0) {
        RLOGE("invalid response length %d expected multiple of %d\n",
            (int)responselen, (int)sizeof(RIL_Call_VoLTE *));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    startResponse;
    /* number of call info's */
    num = responselen / sizeof(RIL_Call_VoLTE *);
    p.writeInt32(num);

    for (int i = 0; i < num; i++) {
        RIL_Call_VoLTE *p_cur = ((RIL_Call_VoLTE **) response)[i];
        /* each call info */
        p.writeInt32(p_cur->index);
        p.writeInt32(p_cur->isMT);
        p.writeInt32(p_cur->negStatusPresent);
        p.writeInt32(p_cur->negStatus);
        writeStringToParcel(p, p_cur->mediaDescription);
        p.writeInt32(p_cur->csMode);
        p.writeInt32(p_cur->state);
        p.writeInt32(p_cur->mpty);
        p.writeInt32(p_cur->numberType);
        p.writeInt32(p_cur->toa);
        if (p_cur->number != NULL) {
            char *numberTmp = strdup(p_cur->number);
            stripNumberFromSipAddress(p_cur->number, numberTmp,
                    strlen(numberTmp) * sizeof(char));
            writeStringToParcel(p, numberTmp);
            free(numberTmp);
        } else {
            writeStringToParcel(p, p_cur->number);
        }
        p.writeInt32(p_cur->prioritypresent);
        p.writeInt32(p_cur->priority);
        p.writeInt32(p_cur->CliValidityPresent);
        p.writeInt32(p_cur->numberPresentation);
        p.writeInt32(p_cur->als);
        p.writeInt32(p_cur->isVoicePrivacy);
        writeStringToParcel(p, p_cur->name);
        p.writeInt32(p_cur->namePresentation);

        // Remove when partners upgrade to version 3
        if ((s_callbacks.version < 3) || (p_cur->uusInfo == NULL
                || p_cur->uusInfo->uusData == NULL)) {
            p.writeInt32(0);  /* UUS Information is absent */
        } else {
            RIL_UUS_Info *uusInfo = p_cur->uusInfo;
            p.writeInt32(1);  /* UUS Information is present */
            p.writeInt32(uusInfo->uusType);
            p.writeInt32(uusInfo->uusDcs);
            p.writeInt32(uusInfo->uusLength);
            p.write(uusInfo->uusData, uusInfo->uusLength);
        }

        appendPrintBuf("%s[id=%d,%s,[neg_Present=%d,",
            printBuf,
            p_cur->index,
            (p_cur->isMT)?"mt":"mo",
            p_cur->negStatusPresent);
        appendPrintBuf("%snegStatus=%d,mediaDes=%s],[csMode=%d,",
            printBuf,
            p_cur->negStatus,
            p_cur->mediaDescription,
            (p_cur->csMode));
        appendPrintBuf("%s,%s,conf=%d,numberType=%d,",
            printBuf,
            callStateToString(p_cur->state),
            (p_cur->mpty),
            p_cur->numberType);

        if (s_isUserdebug) {
            appendPrintBuf("%s,toa=%d,%s],[pri_p=%d,priority=%d,CliValidity=%d,",
                printBuf,
                p_cur->toa,
                p_cur->number,
                p_cur->prioritypresent,
                p_cur->priority,
                p_cur->CliValidityPresent);
        } else {
            appendPrintBuf("%s,toa=%d,****],[pri_p=%d,priority=%d,CliValidity=%d,",
                printBuf,
                p_cur->toa,
                p_cur->prioritypresent,
                p_cur->priority,
                p_cur->CliValidityPresent);
        }

        appendPrintBuf("%s,cli=%d],als='%d',%s,%s,%d]",
            printBuf,
            p_cur->numberPresentation,
            p_cur->als,
            (p_cur->isVoicePrivacy)?"voc":"nonvoc",
            p_cur->name,
            p_cur->namePresentation);
    }
    removeLastChar;
    closeResponse;

    return 0;
}

static int responseCallForwardsUri(Parcel &p, void *response, size_t responselen) {
    int num;

    if (response == NULL && responselen != 0) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (responselen % sizeof(RIL_CallForwardInfoUri *) != 0) {
        RLOGE("invalid response length %d expected multiple of %d",
                (int)responselen, (int)sizeof(RIL_CallForwardInfoUri *));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    /* number of call info's */
    num = responselen / sizeof(RIL_CallForwardInfoUri *);
    p.writeInt32(num);

    startResponse;
    for (int i = 0; i < num; i++) {
        RIL_CallForwardInfoUri *p_cur = ((RIL_CallForwardInfoUri **)response)[i];

        p.writeInt32(p_cur->status);
        p.writeInt32(p_cur->reason);
        p.writeInt32(p_cur->numberType);
        p.writeInt32(p_cur->ton);
        writeStringToParcel(p, p_cur->number);
        p.writeInt32(p_cur->serviceClass);
        writeStringToParcel(p, p_cur->ruleset);
        p.writeInt32(p_cur->timeSeconds);

        if (s_isUserdebug) {
            appendPrintBuf("%s[%s, reason=%d, numType = %d, ton = %d,%s, cls = %d, rule = %s, tout = %d],",
                    printBuf, (p_cur->status == 1)? "enable" : "disable",
                    p_cur->reason, p_cur->numberType, p_cur->ton,
                    (char*)p_cur->number,
                    p_cur->serviceClass,
                    p_cur->ruleset,
                    p_cur->timeSeconds);
        } else {
            appendPrintBuf("%s[%s, reason=%d, numType = %d, ton = %d,****, cls = %d, rule = %s, tout = %d],",
                    printBuf, (p_cur->status == 1)? "enable" : "disable",
                    p_cur->reason, p_cur->numberType, p_cur->ton,
                    p_cur->serviceClass,
                    p_cur->ruleset,
                    p_cur->timeSeconds);
        }
    }
    removeLastChar;
    closeResponse;

    return 0;
}

static int responseCMCCSI(Parcel &p, void *response, size_t responselen) {
    if (response == NULL) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (responselen != sizeof(RIL_IMSPHONE_CMCCSI)) {
        RLOGE("invalid response length was %d expected %d",
                (int)responselen, (int)sizeof(RIL_IMSPHONE_CMCCSI));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    RIL_IMSPHONE_CMCCSI *p_cur = (RIL_IMSPHONE_CMCCSI *)response;
    p.writeInt32(p_cur->id);
    p.writeInt32(p_cur->idr);
    p.writeInt32(p_cur->neg_stat_present);
    p.writeInt32(p_cur->neg_stat);
    writeStringToParcel(p, p_cur->SDP_md);
    p.writeInt32(p_cur->cs_mod);
    p.writeInt32(p_cur->ccs_stat);
    p.writeInt32(p_cur->mpty);
    p.writeInt32(p_cur->num_type);
    p.writeInt32(p_cur->ton);
    writeStringToParcel(p, p_cur->number);
    p.writeInt32(p_cur->exit_type);
    p.writeInt32(p_cur->exit_cause);
    startResponse;
    closeResponse;

    return 0;
}

static int responseDSCI(Parcel &p, void *response, size_t responselen) {
    if (response == NULL) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (responselen != sizeof(RIL_VideoPhone_DSCI)) {
        RLOGE("invalid response length was %d expected %d",
                (int)responselen, (int)sizeof(RIL_VideoPhone_DSCI));
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    RIL_VideoPhone_DSCI *p_cur = (RIL_VideoPhone_DSCI *)response;
    p.writeInt32(p_cur->id);
    p.writeInt32(p_cur->idr);
    p.writeInt32(p_cur->stat);
    p.writeInt32(p_cur->type);
    p.writeInt32(p_cur->mpty);
    writeStringToParcel(p, p_cur->number);
    p.writeInt32(p_cur->num_type);
    p.writeInt32(p_cur->bs_type);
    p.writeInt32(p_cur->cause);
    p.writeInt32(p_cur->location);

    startResponse;
    if (s_isUserdebug) {
        appendPrintBuf("%sstatus = %d, type = %s, number = %s, cause = %d, location = %d",
                       printBuf, p_cur->stat, (p_cur->type == 0)? "voice" : "video",
                       p_cur->number, p_cur->cause, p_cur->location);
    } else {
        appendPrintBuf("%sstatus = %d, type = %s, number = ****, cause = %d, location = %d",
                       printBuf, p_cur->stat, (p_cur->type == 0)? "voice" : "video",
                       p_cur->cause, p_cur->location);
    }
    closeResponse;

    return 0;
}

static int responseImsNetworkInfo(Parcel &p, void *response, size_t responselen) {
    if (response == NULL) {
        RLOGE("invalid response: NULL");
        return RIL_ERRNO_INVALID_RESPONSE;
    }

    if (responselen != sizeof(IMS_NetworkInfo)) {
        RLOGE("invalid IMS_NetworkInfo response length was %d expected %d",
                (int)responselen, (int)sizeof (IMS_NetworkInfo));
        return RIL_ERRNO_INVALID_RESPONSE;
    }
    IMS_NetworkInfo *p_cur = (IMS_NetworkInfo *)response;
    p.writeInt32(p_cur->type);
    writeStringToParcel(p, p_cur->info);
    startResponse;
    appendPrintBuf("responseImsNetworkInfo: type=%d, info=%s", p_cur->type, p_cur->info);
    closeResponse;
    return 0;
}

extern "C" void
stripNumberFromSipAddress(const char *sipAddress, char *number, int len) {
    if (sipAddress == NULL || strlen(sipAddress) == 0
            || number == NULL || len <= 0) {
        return;
    }

    memset(number, 0, len * sizeof(char));

    char delim[] = ":;@";
    char *strDupSipAddr = strdup(sipAddress);
    char *s = strDupSipAddr;
    char *token = strsep(&s, delim);
    if (token != NULL) {
        if (strlen(token) == strlen(sipAddress)) {
            strncpy(number, sipAddress, len);
            goto EXIT;
        }
        token = strsep(&s, delim);
        if (token == NULL) {
            strncpy(number, sipAddress, len);
            goto EXIT;
        } else {
            strncpy(number, token, len);
            goto EXIT;
        }
    }
    strncpy(number, sipAddress, len);

EXIT:
    if (s != NULL) {
        free(strDupSipAddr);
        strDupSipAddr = NULL;
    }
    return;
}

/**
 * A write on the wakeup fd is done just to pop us out of select()
 * We empty the buffer here and then ril_event will reset the timers on the
 * way back down
 */
static void processWakeupCallback(int fd __unused,
        short flags __unused, void *param __unused) {
    char buff[16];
    int ret;

    RLOGV("processWakeupCallback");

    /* empty our wakeup socket out */
    do {
        ret = read(s_fdWakeupRead, &buff, sizeof(buff));
    } while (ret > 0 || (ret < 0 && errno == EINTR));
}

static void onCommandsSocketClosed(RIL_SOCKET_ID socket_id) {
    int ret;
    RequestInfo *p_cur;
    /* Hook for current context
       pendingRequestsMutextHook refer to &s_pendingRequestsMutex */
    pthread_mutex_t *pendingRequestsMutexHook = &s_pendingRequestsMutex[socket_id];
    /* pendingRequestsHook refer to &s_pendingRequests */
    RequestInfo **pendingRequestsHook = &s_pendingRequests[socket_id];

    /* mark pending requests as "cancelled" so we dont report responses */
    ret = pthread_mutex_lock(pendingRequestsMutexHook);
    assert(ret == 0);

    p_cur = *pendingRequestsHook;

    for (p_cur = *pendingRequestsHook; p_cur != NULL; p_cur  = p_cur->p_next) {
        p_cur->cancelled = 1;
    }

    ret = pthread_mutex_unlock(pendingRequestsMutexHook);
    assert(ret == 0);
}

static void *slowDispatch(void *param) {
    ListNode *cmd_item;
    pid_t tid;
    tid = gettid();

    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    pthread_mutex_t *slowDispatchMutex =
            &(s_requestThread[socket_id].slowDispatchMutex);
    pthread_cond_t *slowDispatchCond =
            &(s_requestThread[socket_id].slowDispatchCond);
    ListNode *slow_list = s_requestThread[socket_id].slowReqList;

    while (1) {
        pthread_mutex_lock(slowDispatchMutex);
        if (slow_list->next == slow_list) {
            pthread_cond_wait(slowDispatchCond, slowDispatchMutex);
        }
        pthread_mutex_unlock(slowDispatchMutex);

        for (cmd_item = slow_list->next; cmd_item != slow_list;
                cmd_item = slow_list->next) {
            processCommandBuffer(cmd_item->p_reqData->buffer,
                    cmd_item->p_reqData->buflen, socket_id, cmd_item->p_reqData->socket_type);
            RLOGI("-->slowDispatch [%d] free one command", tid);
            list_remove(cmd_item, socket_id);  /* remove list node first, then free it */
            free(cmd_item->p_reqData->buffer);
            free(cmd_item->p_reqData);
            free(cmd_item);
        }
    }
    return NULL;
}

static void *normalDispatch(void *param) {
    ListNode *cmd_item;
    pid_t tid;
    tid = gettid();

    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    pthread_mutex_t *normalDispatchMutex =
            &(s_requestThread[socket_id].normalDispatchMutex);
    pthread_cond_t *normalDispatchCond =
            &(s_requestThread[socket_id].normalDispatchCond);
    ListNode *call_list = s_requestThread[socket_id].callReqList;
    ListNode *sim_list = s_requestThread[socket_id].simReqList;

    while (1) {
        pthread_mutex_lock(normalDispatchMutex);
        if ((call_list->next == call_list) && (sim_list->next == sim_list)) {
            pthread_cond_wait(normalDispatchCond, normalDispatchMutex);
        }
        pthread_mutex_unlock(normalDispatchMutex);

        while ((call_list->next != call_list) || (sim_list->next != sim_list)) {
            if (call_list->next != call_list) {  // call_list has priority
                cmd_item = call_list->next;
            } else {
                cmd_item = sim_list->next;
            }
            processCommandBuffer(cmd_item->p_reqData->buffer,
                cmd_item->p_reqData->buflen, socket_id, cmd_item->p_reqData->socket_type);
            RLOGI("-->Normal Dispatch [%d] free one command", tid);
            list_remove(cmd_item, socket_id);  /* remove listnode first, then free it */
            free(cmd_item->p_reqData->buffer);
            free(cmd_item->p_reqData);
            free(cmd_item);
        }
    }
    return NULL;
}

#if defined (ANDROID_MULTI_SIM)
static void *dataDispatch(void *param __unused) {
    ListNode *cmd_item;
    pid_t tid;
    tid = gettid();

    pthread_mutex_t *dataDispatchMutex = &s_dataDispatchMutex;
    pthread_cond_t *dataDispatchCond = &s_dataDispatchCond;
    ListNode *dataList = s_dataReqList;

    while (1) {
        pthread_mutex_lock(dataDispatchMutex);
        if (dataList->next == dataList) {
            pthread_cond_wait(dataDispatchCond, dataDispatchMutex);
        }
        pthread_mutex_unlock(dataDispatchMutex);

        for (cmd_item = dataList->next; cmd_item != dataList;
                cmd_item = dataList->next) {
            processCommandBuffer(cmd_item->p_reqData->buffer,
                    cmd_item->p_reqData->buflen, cmd_item->p_reqData->socket_id,
                    cmd_item->p_reqData->socket_type);
            RLOGI("-->dataDispatch [%d] free one command", tid);
            list_remove(cmd_item, RIL_SOCKET_1);  /* remove list node first, then free it */
            free(cmd_item->p_reqData->buffer);
            free(cmd_item->p_reqData);
            free(cmd_item);
        }
    }
    return NULL;
}
#endif

static void CommandThread(void *arg, void *socket_id) {
    pid_t tid;
    tid = gettid();
    RequestThreadData *p_reqData = (RequestThreadData *)arg;
    RIL_SOCKET_ID soc_id = *((RIL_SOCKET_ID *)socket_id);
    processCommandBuffer(p_reqData->buffer, p_reqData->buflen,
            soc_id, p_reqData->socket_type);
    RLOGI("-->CommandThread [%d] free one command", tid);
    free(p_reqData->buffer);
    free(p_reqData);
}

static void *otherDispatch(void *param) {
    ListNode *cmd_item;
    pid_t tid;
    int ret;
    tid = gettid();

    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    pthread_mutex_t *otherDispatchMutex =
            &(s_requestThread[socket_id].otherDispatchMutex);
    pthread_cond_t *otherDispatchCond =
            &(s_requestThread[socket_id].otherDispatchCond);
    ListNode *other_list = s_requestThread[socket_id].otherReqList;
    threadpool_t *thrpool = s_requestThread[socket_id].p_threadpool;

    while (1) {
        pthread_mutex_lock(otherDispatchMutex);
        if (other_list->next == other_list) {
            pthread_cond_wait(otherDispatchCond, otherDispatchMutex);
        }
        pthread_mutex_unlock(otherDispatchMutex);

        for (cmd_item = other_list->next; cmd_item != other_list;
                cmd_item = other_list->next) {
            do {
                ret = thread_pool_dispatch(thrpool, CommandThread,
                        cmd_item->p_reqData, (void *)&s_socketId[socket_id]);
                if (!ret) {
                    RLOGE("dispatch a new thread unsuccess");
                    sleep(1);
                }
            } while (!ret);
            list_remove(cmd_item, socket_id);  /* remove listnode first, then free it */
            free(cmd_item);
        }
    }
    return NULL;
}

static void processCommandsCallback(int fd, short flags __unused, void *param) {
    RecordStream *p_rs;
    void *p_record;
    size_t recordlen;
    int ret;
    int32_t simId = 0;

    SocketListenParam *p_info = (SocketListenParam *)param;

    assert(fd == p_info->fdCommand);

    Parcel p;
    status_t status;
    int32_t request;
    int32_t token;
    CommandInfo *pCI = NULL;
    RequestThreadData *p_reqData = NULL;
    ListNode *cmd_item = NULL;
    RIL_SOCKET_ID socket_id = p_info->socket_id;  /* Not for ATCI socket */
    RequestThreadInfo *cmdList = &s_requestThread[socket_id];

    p_rs = p_info->p_rs;

    for (;;) {
        /* loop until EAGAIN/EINTR, end of stream, or other error */
        ret = record_stream_get_next(p_rs, &p_record, &recordlen);

        if (ret == 0 && p_record == NULL) {
            /* end-of-stream */
            break;
        } else if (ret < 0) {
            break;
        } else if (ret == 0) {  /* && p_record != NULL */
            cmd_item = (ListNode *)malloc(sizeof(ListNode));
            if (cmd_item == NULL) {
                RLOGE("Failed to allocate memory for cmd_item");
                exit(-1);
            }
            p_reqData =(RequestThreadData *)malloc(sizeof(RequestThreadData));
            if (p_reqData == NULL) {
                RLOGE("Failed to allocate memory for p_reqData");
                free(cmd_item);
                exit(-1);
            }
            p_reqData->p_rs = p_rs;
            p_reqData->buffer =(char *)malloc(recordlen);
            if (p_reqData->buffer == NULL) {
                RLOGE("Failed to allocate memory for p_reqData buffer");
                free(cmd_item);
                free(p_reqData);
                exit(-1);
            }
            p_reqData->buflen = recordlen;
            p_reqData->socket_type = p_info->type;
            p_reqData->socket_id = socket_id;
            memcpy(p_reqData->buffer, p_record, recordlen);

            cmd_item->p_reqData = p_reqData;

            p.setData((uint8_t *)(p_reqData->buffer), p_reqData->buflen);
            status = p.readInt32(&request);
            status = p.readInt32(&token);
            if (p_info->type == RIL_ATCI_SOCKET) {
                status = p.readInt32(&simId);
                socket_id = (RIL_SOCKET_ID)simId;
                cmdList = &s_requestThread[socket_id];
            }
            if (status != NO_ERROR) {
                RLOGE("Invalid request");
                free(cmd_item);
                free(p_reqData->buffer);
                free(p_reqData);
                return;
            }

            if (request < 1
                || (request > RIL_REQUEST_LAST && request < RIL_IMS_REQUEST_BASE)
                || (request > RIL_IMS_REQUEST_LAST && request < RIL_EXT_REQUEST_BASE)
                || (request > RIL_EXT_REQUEST_LAST)) {
                RLOGE("Unsupported request code %d token %d", request, token);
                free(cmd_item);
                free(p_reqData->buffer);
                free(p_reqData);
                return;
            }

            if (request > 0 && request <= RIL_REQUEST_LAST) {
                pCI = &(s_commands[request]);
            } else if (request > RIL_IMS_REQUEST_BASE && request <= RIL_IMS_REQUEST_LAST) {
                request = request - RIL_IMS_REQUEST_BASE;
                pCI = &(s_ims_commands[request]);
            } else if (request > RIL_EXT_REQUEST_BASE && request <= RIL_EXT_REQUEST_LAST) {
                request = request - RIL_EXT_REQUEST_BASE;
                pCI = &(s_oem_commands[request]);
            }

            if (pCI->requestNumber == RIL_REQUEST_SEND_SMS
                || pCI->requestNumber == RIL_REQUEST_SEND_SMS_EXPECT_MORE
                || pCI->requestNumber == RIL_REQUEST_IMS_SEND_SMS
                || pCI->requestNumber == RIL_REQUEST_QUERY_CALL_FORWARD_STATUS
                || pCI->requestNumber == RIL_REQUEST_SET_CALL_FORWARD
                || pCI->requestNumber == RIL_REQUEST_GET_CLIR
                || pCI->requestNumber == RIL_REQUEST_SET_CLIR
                || pCI->requestNumber == RIL_REQUEST_QUERY_CALL_WAITING
                || pCI->requestNumber == RIL_REQUEST_SET_CALL_WAITING
                || pCI->requestNumber == RIL_REQUEST_QUERY_CLIP
#if (SIM_COUNT == 1)
                || pCI->requestNumber == RIL_REQUEST_ALLOW_DATA
                || pCI->requestNumber == RIL_REQUEST_SETUP_DATA_CALL
#endif
                ) {
                list_add_tail(cmdList->slowReqList, cmd_item, socket_id);
                pthread_mutex_lock(&(cmdList->slowDispatchMutex));
                pthread_cond_signal(&(cmdList->slowDispatchCond));
                pthread_mutex_unlock(&(cmdList->slowDispatchMutex));
            } else if (pCI->requestNumber == RIL_REQUEST_RADIO_POWER
                    || pCI->requestNumber == RIL_REQUEST_DIAL
                    || pCI->requestNumber == RIL_REQUEST_DTMF
                    || pCI->requestNumber == RIL_REQUEST_DTMF_START
                    || pCI->requestNumber == RIL_REQUEST_DTMF_STOP
                    || pCI->requestNumber == RIL_REQUEST_HANGUP
                    || pCI->requestNumber == RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND
                    || pCI->requestNumber == RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND
                    || pCI->requestNumber == RIL_REQUEST_ANSWER
                    || pCI->requestNumber == RIL_REQUEST_GET_CURRENT_CALLS
                    || pCI->requestNumber == RIL_REQUEST_CONFERENCE
                    || pCI->requestNumber == RIL_REQUEST_UDUB
                    || pCI->requestNumber == RIL_REQUEST_SEPARATE_CONNECTION
                    || pCI->requestNumber == RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE
                    || pCI->requestNumber == RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE
                    || pCI->requestNumber == RIL_REQUEST_SET_RADIO_CAPABILITY
                    /* IMS @{ */
                    || pCI->requestNumber == RIL_REQUEST_IMS_CALL_RESPONSE_MEDIA_CHANGE
                    || pCI->requestNumber == RIL_REQUEST_IMS_CALL_REQUEST_MEDIA_CHANGE
                    || pCI->requestNumber == RIL_REQUEST_IMS_CALL_FALL_BACK_TO_VOICE
                    || pCI->requestNumber == RIL_REQUEST_IMS_INITIAL_GROUP_CALL
                    || pCI->requestNumber == RIL_REQUEST_IMS_ADD_TO_GROUP_CALL
                    || pCI->requestNumber == RIL_REQUEST_VIDEOPHONE_DIAL
                    /* }@ */
                    /* OEM SOCKET REQUEST @{ */
                    || pCI->requestNumber == RIL_EXT_REQUEST_VIDEOPHONE_DIAL
                    || pCI->requestNumber == RIL_EXT_REQUEST_SWITCH_MULTI_CALL
                    || pCI->requestNumber == RIL_EXT_REQUEST_GET_HD_VOICE_STATE
                    || pCI->requestNumber == RIL_EXT_REQUEST_SIMMGR_SIM_POWER
                    /* }@ */
                    ) {
                list_add_tail(cmdList->callReqList, cmd_item, socket_id);
                pthread_mutex_lock(&(cmdList->normalDispatchMutex));
                pthread_cond_signal(&(cmdList->normalDispatchCond));
                pthread_mutex_unlock(&(cmdList->normalDispatchMutex));
            } else if (pCI->requestNumber == RIL_REQUEST_SIM_IO
                        || pCI->requestNumber == RIL_REQUEST_WRITE_SMS_TO_SIM
                        || pCI->requestNumber == RIL_REQUEST_DELETE_SMS_ON_SIM
                        || pCI->requestNumber == RIL_REQUEST_GET_SMSC_ADDRESS
                        || pCI->requestNumber == RIL_REQUEST_QUERY_FACILITY_LOCK
                        || pCI->requestNumber == RIL_REQUEST_SET_FACILITY_LOCK) {
                list_add_tail(cmdList->simReqList, cmd_item, socket_id);
                pthread_mutex_lock(&(cmdList->normalDispatchMutex));
                pthread_cond_signal(&(cmdList->normalDispatchCond));
                pthread_mutex_unlock(&(cmdList->normalDispatchMutex));
#if defined (ANDROID_MULTI_SIM)
            } else if (pCI->requestNumber == RIL_REQUEST_ALLOW_DATA
                    || pCI->requestNumber == RIL_REQUEST_SETUP_DATA_CALL) {
                RLOGD("add sim%d %s to datalist", socket_id, requestToString(request));
                list_add_tail(s_dataReqList, cmd_item, RIL_SOCKET_1);
                pthread_mutex_lock(&s_dataDispatchMutex);
                pthread_cond_signal(&s_dataDispatchCond);
                pthread_mutex_unlock(&s_dataDispatchMutex);
#endif
            } else {
                list_add_tail(cmdList->otherReqList, cmd_item, socket_id);
                pthread_mutex_lock(&(cmdList->otherDispatchMutex));
                pthread_cond_signal(&(cmdList->otherDispatchCond));
                pthread_mutex_unlock(&(cmdList->otherDispatchMutex));
            }
        }
    }

    if (ret == 0 || !(errno == EAGAIN || errno == EINTR)) {
        /* fatal error or end-of-stream */
        if (ret != 0) {
            RLOGE("error on reading command socket errno:%d\n", errno);
        } else {
            RLOGW("EOS.  Closing command socket.");
        }
        RLOGD("Closing socket type = %d", p_info->type);
        if (fd != -1) {
            close(fd);
        }
        p_info->fdCommand = -1;
        if (p_info->type == RIL_ATCI_SOCKET) {
            RLOGE("not receive at cmd, client died");
            s_atciSocketParam.fdCommand = -1;
            if (s_atciSocketParam.p_rs != NULL) {
                record_stream_free(s_atciSocketParam.p_rs);
                s_atciSocketParam.p_rs = NULL;
            }
            rilEventAddWakeup(s_atciSocketParam.listen_event);
        } else {
            ril_event_del(p_info->commands_event);

            record_stream_free(p_rs);

            /* start listening for new connections again */
            rilEventAddWakeup(p_info->listen_event);

            onCommandsSocketClosed(p_info->socket_id);
        }
    }
}

static void onNewCommandConnect(RIL_SOCKET_ID socket_id) {
    // Init Variables
    initPrimarySim();

    // Inform we are connected and the ril version
    int rilVer = s_callbacks.version;
    RIL_UNSOL_RESPONSE(RIL_UNSOL_RIL_CONNECTED,
                                    &rilVer, sizeof(rilVer), socket_id);

    // implicit radio state changed
    RIL_UNSOL_RESPONSE(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED,
                                    NULL, 0, socket_id);

    // unsolicited sim status changed
    RIL_UNSOL_RESPONSE(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED,
                                    NULL, 0, socket_id);

    // Send last NITZ time data, in case it was missed
    if (s_lastNITZTimeData != NULL) {
        sendResponseRaw(s_lastNITZTimeData, s_lastNITZTimeDataSize,
                socket_id, RIL_TELEPHONY_SOCKET);

        free(s_lastNITZTimeData);
        s_lastNITZTimeData = NULL;
    }

    // Get version string
    if (s_callbacks.getVersion != NULL) {
        const char *version;
        version = s_callbacks.getVersion();
        RLOGI("RIL Daemon version: %s\n", version);

        property_set(PROPERTY_RIL_IMPL, version);
    } else {
        RLOGI("RIL Daemon version: unavailable\n");
        property_set(PROPERTY_RIL_IMPL, "unavailable");
    }
}

static void listenCallback(int fd, short flags __unused, void *param) {
    int ret;
    int err;
    int is_phone_socket;
    int fdCommand = -1;
    char *processName;
    RecordStream *p_rs;
    MySocketListenParam *listenParam;
    RilSocket *sapSocket = NULL;
    socketClient *sClient = NULL;

    SocketListenParam *p_info = (SocketListenParam *)param;

    if (RIL_SAP_SOCKET == p_info->type) {
        listenParam = (MySocketListenParam *)param;
        sapSocket = listenParam->socket;
    }

    struct sockaddr_un peeraddr;
    socklen_t socklen = sizeof(peeraddr);

    struct ucred creds;
    socklen_t szCreds = sizeof(creds);

    struct passwd *pwd = NULL;

    if (NULL == sapSocket) {
        assert(*p_info->fdCommand < 0);
        assert(fd == *p_info->fdListen);
        processName = PHONE_PROCESS;
    } else {
        assert(sapSocket->commandFd < 0);
        assert(fd == sapSocket->listenFd);
        processName = BLUETOOTH_PROCESS;
    }


    fdCommand = accept(fd, (sockaddr *)&peeraddr, &socklen);

    if (fdCommand < 0) {
        RLOGE("Error on accept() errno:%d", errno);
        /* start listening for new connections again */
        if (NULL == sapSocket) {
            rilEventAddWakeup(p_info->listen_event);
        } else {
            rilEventAddWakeup(sapSocket->getListenEvent());
        }
        return;
    }

    /* check the credential of the other side and only accept socket from
     * phone process
     */
    errno = 0;
    is_phone_socket = 0;

    err = getsockopt(fdCommand, SOL_SOCKET, SO_PEERCRED, &creds, &szCreds);

    if (err == 0 && szCreds > 0) {
        errno = 0;
        pwd = getpwuid(creds.uid);
        if (pwd != NULL) {
            if (strcmp(pwd->pw_name, processName) == 0) {
                is_phone_socket = 1;
            } else {
                RLOGE("RILD can't accept socket from process %s", pwd->pw_name);
            }
        } else {
            RLOGE("Error on getpwuid() errno: %d", errno);
        }
    } else {
        RLOGD("Error on getsockopt() errno: %d", errno);
    }

    if (!is_phone_socket) {
        RLOGE("RILD must accept socket from %s", processName);

        close(fdCommand);
        fdCommand = -1;

        if (NULL == sapSocket) {
            onCommandsSocketClosed(p_info->socket_id);

            /* start listening for new connections again */
            rilEventAddWakeup(p_info->listen_event);
        } else {
            sapSocket->onCommandsSocketClosed();

            /* start listening for new connections again */
            rilEventAddWakeup(sapSocket->getListenEvent());
        }

        return;
    }

    ret = fcntl(fdCommand, F_SETFL, O_NONBLOCK);

    if (ret < 0) {
        RLOGE("Error setting O_NONBLOCK errno:%d", errno);
    }

    if (NULL == sapSocket) {
        RLOGI("libril: new connection to %s", rilSocketIdToString(p_info->socket_id));

        p_info->fdCommand = fdCommand;
        p_rs = record_stream_new(p_info->fdCommand, MAX_COMMAND_BYTES);
        p_info->p_rs = p_rs;

        ril_event_set(p_info->commands_event, p_info->fdCommand, 1,
        p_info->processCommandsCallback, p_info);
        rilEventAddWakeup(p_info->commands_event);

        onNewCommandConnect(p_info->socket_id);
    } else {
        RLOGI("libril: new connection");

        sapSocket->setCommandFd(fdCommand);
        p_rs = record_stream_new(sapSocket->getCommandFd(), MAX_COMMAND_BYTES);
        sClient = new socketClient(sapSocket, p_rs);
        ril_event_set(sapSocket->getCallbackEvent(), sapSocket->getCommandFd(), 1,
        sapSocket->getCommandCb(), sClient);

        rilEventAddWakeup(sapSocket->getCallbackEvent());
        sapSocket->onNewCommandConnect();
    }
}

static void freeDebugCallbackArgs(int number, char **args) {
    for (int i = 0; i < number; i++) {
        if (args[i] != NULL) {
            free(args[i]);
        }
    }
    free(args);
}

static void debugCallback(int fd, short flags __unused, void *param __unused) {
    int acceptFD, option;
    struct sockaddr_un peeraddr;
    socklen_t socklen = sizeof(peeraddr);
    int data;
    unsigned int qxdm_data[6];
    const char *deactData[1] = {"1"};
    char *actData[1];
    RIL_Dial dialData;
    int hangupData[1] = {1};
    int number;
    char **args;
    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    int sim_id = 0;

    RLOGI("debugCallback for socket %s", rilSocketIdToString(socket_id));

    acceptFD = accept(fd, (sockaddr *) &peeraddr, &socklen);

    if (acceptFD < 0) {
        RLOGE("error accepting on debug port: %d\n", errno);
        return;
    }

    if (recv(acceptFD, &number, sizeof(int), 0) != sizeof(int)) {
        RLOGE("error reading on socket: number of Args: \n");
        close(acceptFD);
        return;
    }

    if (number < 0) {
        RLOGE("Invalid number of arguments: \n");
        close(acceptFD);
        return;
    }

    args = (char **) calloc(number, sizeof(char*));
    if (args == NULL) {
        RLOGE("Memory allocation failed for debug args");
        close(acceptFD);
        return;
    }

    for (int i = 0; i < number; i++) {
        int len;
        if (recv(acceptFD, &len, sizeof(int), 0) != sizeof(int)) {
            RLOGE("error reading on socket: Len of Args: \n");
            freeDebugCallbackArgs(i, args);
            close(acceptFD);
            return;
        }
        if (len == INT_MAX || len < 0) {
            RLOGE("Invalid value of len: \n");
            freeDebugCallbackArgs(i, args);
            close(acceptFD);
            return;
        }

        // +1 for null-term
        args[i] = (char *) calloc(len + 1, sizeof(char));
        if (args[i] == NULL) {
            RLOGE("Memory allocation failed for debug args");
            freeDebugCallbackArgs(i, args);
            close(acceptFD);
            return;
        }
        if (recv(acceptFD, args[i], sizeof(char) * len, 0)
            != (int)sizeof(char) * len) {
            RLOGE("error reading on socket: Args[%d] \n", i);
            freeDebugCallbackArgs(i, args);
            close(acceptFD);
            return;
        }
        char *buf = args[i];
        buf[len] = 0;
        if ((i + 1) == number) {
            /* The last argument should be sim id 0(SIM1)~3(SIM4) */
            sim_id = atoi(args[i]);
            switch (sim_id) {
                case 0:
                    socket_id = RIL_SOCKET_1;
                    break;
            #if (SIM_COUNT >= 2)
                case 1:
                    socket_id = RIL_SOCKET_2;
                    break;
            #endif
            #if (SIM_COUNT >= 3)
                case 2:
                    socket_id = RIL_SOCKET_3;
                    break;
            #endif
            #if (SIM_COUNT >= 4)
                case 3:
                    socket_id = RIL_SOCKET_4;
                    break;
            #endif
                default:
                    socket_id = RIL_SOCKET_1;
                    break;
            }
        }
    }

    switch (atoi(args[0])) {
        case 0:
            RLOGI("Connection on debug port: issuing reset.");
            issueLocalRequest(RIL_REQUEST_RESET_RADIO, NULL, 0, socket_id);
            break;
        case 1:
            RLOGI("Connection on debug port: issuing radio power off.");
            data = 0;
            issueLocalRequest(RIL_REQUEST_RADIO_POWER, &data, sizeof(int), socket_id);
            // Close the socket
            if (s_rilSocketParam[socket_id].fdCommand > 0) {
                close(s_rilSocketParam[socket_id].fdCommand);
                s_rilSocketParam[socket_id].fdCommand = -1;
            }
            break;
        case 2:
            RLOGI("Debug port: issuing unsolicited voice network change.");
            RIL_UNSOL_RESPONSE(RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED,
                    NULL, 0, socket_id);
            break;
        case 3:
            RLOGI("Debug port: QXDM log enable.");
            qxdm_data[0] = 65536;     // head.func_tag
            qxdm_data[1] = 16;        // head.len
            qxdm_data[2] = 1;         // mode: 1 for 'start logging'
            qxdm_data[3] = 32;        // log_file_size: 32megabytes
            qxdm_data[4] = 0;         // log_mask
            qxdm_data[5] = 8;         // log_max_fileindex
            issueLocalRequest(RIL_REQUEST_OEM_HOOK_RAW, qxdm_data,
                              6 * sizeof(int), socket_id);
            break;
        case 4:
            RLOGI("Debug port: QXDM log disable.");
            qxdm_data[0] = 65536;
            qxdm_data[1] = 16;
            qxdm_data[2] = 0;          // mode: 0 for 'stop logging'
            qxdm_data[3] = 32;
            qxdm_data[4] = 0;
            qxdm_data[5] = 8;
            issueLocalRequest(RIL_REQUEST_OEM_HOOK_RAW, qxdm_data,
                              6 * sizeof(int), socket_id);
            break;
        case 5:
            RLOGI("Debug port: Radio On");
            data = 1;
            issueLocalRequest(RIL_REQUEST_RADIO_POWER, &data, sizeof(int), socket_id);
            sleep(2);
            // Set network selection automatic.
            issueLocalRequest(RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC,
                    NULL, 0, socket_id);
            break;
        case 6:
            RLOGI("Debug port: Setup Data Call, Apn :%s\n", args[1]);
            actData[0] = args[1];
            issueLocalRequest(RIL_REQUEST_SETUP_DATA_CALL, &actData,
                              sizeof(actData), socket_id);
            break;
        case 7:
            RLOGI("Debug port: Deactivate Data Call");
            issueLocalRequest(RIL_REQUEST_DEACTIVATE_DATA_CALL, &deactData,
                              sizeof(deactData), socket_id);
            break;
        case 8:
            RLOGI("Debug port: Dial Call");
            dialData.clir = 0;
            dialData.address = args[1];
            issueLocalRequest(RIL_REQUEST_DIAL, &dialData, sizeof(dialData), socket_id);
            break;
        case 9:
            RLOGI("Debug port: Answer Call");
            issueLocalRequest(RIL_REQUEST_ANSWER, NULL, 0, socket_id);
            break;
        case 10:
            RLOGI("Debug port: End Call");
            issueLocalRequest(RIL_REQUEST_HANGUP, &hangupData,
                              sizeof(hangupData), socket_id);
            break;
        default:
            RLOGE("Invalid request");
            break;
    }
    freeDebugCallbackArgs(number, args);
    close(acceptFD);
}


static void userTimerCallback(int fd __unused, short flags __unused, void *param) {
    UserCallbackInfo *p_info;

    p_info = (UserCallbackInfo *)param;

    p_info->p_callback(p_info->userParam);

    // FIXME generalize this...there should be a cancel mechanism
    if (s_lastWakeTimeoutInfo != NULL && s_lastWakeTimeoutInfo == p_info) {
        s_lastWakeTimeoutInfo = NULL;
    }

    free(p_info);
}


static void *eventLoop(void *param __unused) {
    int ret;
    int filedes[2];

    ril_event_init();

    pthread_mutex_lock(&s_startupMutex);

    s_started = 1;
    pthread_cond_broadcast(&s_startupCond);

    pthread_mutex_unlock(&s_startupMutex);

    ret = pipe(filedes);

    if (ret < 0) {
        RLOGE("Error in pipe() errno:%d", errno);
        return NULL;
    }

    s_fdWakeupRead = filedes[0];
    s_fdWakeupWrite = filedes[1];

    fcntl(s_fdWakeupRead, F_SETFL, O_NONBLOCK);

    ril_event_set(&s_wakeupEvent, s_fdWakeupRead, true,
                processWakeupCallback, NULL);

    rilEventAddWakeup(&s_wakeupEvent);

    // Only returns on error
    ril_event_loop();
    RLOGE("error in event_loop_base errno:%d", errno);
    // kill self to restart on error
    kill(0, SIGKILL);

    return NULL;
}

extern "C" void RIL_startEventLoop(void) {
    int ret;
    pthread_attr_t attr;

    signal(SIGPIPE, SIG_IGN);

    /* spin up eventLoop thread and wait for it to get started */
    s_started = 0;
    pthread_mutex_lock(&s_startupMutex);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&s_tidDispatch, &attr, eventLoop, NULL);

    while (s_started == 0) {
        pthread_cond_wait(&s_startupCond, &s_startupMutex);
    }

    pthread_mutex_unlock(&s_startupMutex);

    if (ret < 0) {
        RLOGE("Failed to create dispatch thread errno:%d", errno);
        return;
    }
}

// Used for testing purpose only.
extern "C" void RIL_setcallbacks(const RIL_RadioFunctions *callbacks) {
    memcpy(&s_callbacks, callbacks, sizeof(RIL_RadioFunctions));
}

static void startListen(RIL_SOCKET_ID socket_id, SocketListenParam *socket_listen_p) {
    int fdListen = -1;
    int ret;
    char socket_name[10];

    memset(socket_name, 0, sizeof(char) * 10);

    switch (socket_id) {
        case RIL_SOCKET_1:
            strncpy(socket_name, RIL_getRilSocketName(), 9);
            break;
    #if (SIM_COUNT >= 2)
        case RIL_SOCKET_2:
            strncpy(socket_name, SOCKET2_NAME_RIL, 9);
            break;
    #endif
    #if (SIM_COUNT >= 3)
        case RIL_SOCKET_3:
            strncpy(socket_name, SOCKET3_NAME_RIL, 9);
            break;
    #endif
    #if (SIM_COUNT >= 4)
        case RIL_SOCKET_4:
            strncpy(socket_name, SOCKET4_NAME_RIL, 9);
            break;
    #endif
        default:
            RLOGE("Socket id is wrong!!");
            return;
    }

    RLOGI("Start to listen %s", rilSocketIdToString(socket_id));

    fdListen = android_get_control_socket(socket_name);
    if (fdListen < 0) {
        RLOGE("Failed to get socket %s", socket_name);
        exit(-1);
    }

    ret = listen(fdListen, 4);

    if (ret < 0) {
        RLOGE("Failed to listen on control socket '%d': %s",
             fdListen, strerror(errno));
        exit(-1);
    }
    socket_listen_p->fdListen = fdListen;

    /* note: non-persistent so we can accept only one connection at a time */
    ril_event_set(socket_listen_p->listen_event, fdListen, false,
                listenCallback, socket_listen_p);

    rilEventAddWakeup(socket_listen_p->listen_event);
}

static void startListenEXT(char *socketName, SocketListenParam *socket_listen_p) {
    int fdListen = -1;
    int ret;

    RLOGI("Start to listen %s", socketName);

    fdListen = socket_local_server(socketName,
            ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
    if (fdListen < 0) {
        RLOGE("Failed to get socket %s", socketName);
        exit(-1);
    }

    ret = listen(fdListen, 4);

    if (ret < 0) {
        RLOGE("Failed to listen on control socket '%d': %s",
             fdListen, strerror(errno));
        exit(-1);
    }
    socket_listen_p->fdListen = fdListen;

    /* note: non-persistent so we can accept only one connection at a time */
    ril_event_set(socket_listen_p->listen_event, fdListen, false,
                listenCallbackEXT, socket_listen_p);

    rilEventAddWakeup(socket_listen_p->listen_event);
}

static int creatProcessThread(RIL_SOCKET_ID socket_id) {
    int ret;
    s_requestThread[socket_id] = {
                        0, 0, 0,
                        NULL, NULL, NULL, NULL, NULL,
                        PTHREAD_MUTEX_INITIALIZER,
                        PTHREAD_MUTEX_INITIALIZER,
                        PTHREAD_MUTEX_INITIALIZER,
                        PTHREAD_MUTEX_INITIALIZER,
                        PTHREAD_COND_INITIALIZER,
                        PTHREAD_COND_INITIALIZER,
                        PTHREAD_COND_INITIALIZER
                        };

    list_init(&(s_requestThread[socket_id].slowReqList));
    list_init(&(s_requestThread[socket_id].otherReqList));
    list_init(&(s_requestThread[socket_id].callReqList));
    list_init(&(s_requestThread[socket_id].simReqList));

    s_requestThread[socket_id].p_threadpool = thread_pool_init(MAX_THR, 10000);
    if (s_requestThread[socket_id].p_threadpool->thr_max == MAX_THR) {
        RLOGI("SIM%d: %d CommandThread create",
                socket_id, s_requestThread[socket_id].p_threadpool->thr_max);
    }

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    ret = pthread_create(&(s_requestThread[socket_id].slowDispatchTid),
            &attr, slowDispatch, (void *)&s_socketId[socket_id]);
    if (ret < 0) {
        RLOGE("Failed to create slow dispatch thread errno: %s", strerror(errno));
        return 1;
    }

    ret = pthread_create(&(s_requestThread[socket_id].normalDispatchTid),
            &attr, normalDispatch, (void *)&s_socketId[socket_id]);
    if (ret < 0) {
        RLOGE("Failed to create normal dispatch thread errno: %s", strerror(errno));
        return 1;
    }

    ret = pthread_create(&(s_requestThread[socket_id].otherDispatchTid),
                &attr, otherDispatch, (void *)&s_socketId[socket_id]);
    if (ret < 0) {
        RLOGE("Failed to create other dispatch thread errno: %s", strerror(errno));
        return 1;
    }

#if defined (ANDROID_MULTI_SIM)
    if (socket_id == RIL_SOCKET_1) {
        list_init(&s_dataReqList);
        pthread_t tid;
        ret = pthread_create(&tid, &attr, dataDispatch, NULL);
        if (ret < 0) {
            RLOGE("Failed to create data dispatch thread errno: %s", strerror(errno));
            return 1;
        }
    }
#endif

    return 0;
}

extern "C" void RIL_register(const RIL_RadioFunctions *callbacks) {
    int ret, simId;
    int flags;
    int fdListen;
    char socket_name[20];
    char prop[PROPERTY_VALUE_MAX];

    RLOGI("SIM_COUNT: %d", SIM_COUNT);

    if (callbacks == NULL) {
        RLOGE("RIL_register: RIL_RadioFunctions * null");
        return;
    }
    if (callbacks->version < RIL_VERSION_MIN) {
        RLOGE("RIL_register: version %d is to old, min version is %d",
             callbacks->version, RIL_VERSION_MIN);
        return;
    }
    RLOGE("RIL_register: RIL version %d", callbacks->version);

    if (s_registerCalled > 0) {
        RLOGE("RIL_register has been called more than once. "
                "Subsequent call ignored");
        return;
    }

    property_get(BUILD_TYPE_PROP, prop, "user");
    if (strstr(prop, "userdebug")) {
        s_isUserdebug = true;
    }

    memcpy(&s_callbacks, callbacks, sizeof(RIL_RadioFunctions));

    /* Initialize Rild socket parameters */
    for (simId = 0; simId < SIM_COUNT; simId++) {
        s_rilSocketParam[simId] = {
                (RIL_SOCKET_ID)(RIL_SOCKET_1 + simId),  /* socket_id */
                -1,                                     /* fdListen */
                -1,                                     /* fdCommand */
                PHONE_PROCESS,                          /* processName */
                &s_commandsEvent[simId],                /* commands_event */
                &s_listenEvent[simId],                  /* listen_event */
                processCommandsCallback,                /* processCommandsCallback */
                NULL,                                   /* p_rs */
                RIL_TELEPHONY_SOCKET                    /* RIL_SOCKET_TYPE */
                };
    }

    s_registerCalled = 1;

    RLOGI("s_registerCalled flag set, %d", s_started);

    // Little self-check
    for (int i = 0; i <= RIL_REQUEST_LAST; i++) {
        assert(i == s_commands[i].requestNumber);
    }

    for (int i = 0; i <= RIL_UNSOL_RESPONSE_LAST - RIL_UNSOL_RESPONSE_BASE; i++) {
        assert(i == s_unsolResponses[i].requestNumber - RIL_UNSOL_RESPONSE_BASE);
    }

    /* IMS Request @{ */
    for (int i = 1; i <= RIL_IMS_REQUEST_LAST - RIL_IMS_REQUEST_BASE; i++) {
        assert(i == s_ims_commands[i].requestNumber - RIL_IMS_REQUEST_BASE);
    }
    for (int i = 0; i <= RIL_IMS_UNSOL_RESPONSE_LAST -
                            RIL_IMS_UNSOL_RESPONSE_BASE; i++) {
        assert(i == s_ims_unsolResponses[i].requestNumber -
                    RIL_IMS_UNSOL_RESPONSE_BASE);
    }
    /* }@ */

    /* OEMSOCKET Request @{ */
    for (int i = 1; i <= RIL_EXT_REQUEST_LAST - RIL_EXT_REQUEST_BASE; i++) {
        assert(i == s_oem_commands[i].requestNumber - RIL_EXT_REQUEST_BASE);
    }
    for (int i = 0; i <= RIL_EXT_UNSOL_RESPONSE_LAST -
                            RIL_EXT_UNSOL_RESPONSE_BASE; i++) {
        assert(i == s_oem_unsolResponses[i].requestNumber -
                    RIL_EXT_UNSOL_RESPONSE_BASE);
    }
    /* }@ */

    // creat pthead to process RIL requests
    for (simId = 0; simId < SIM_COUNT; simId++) {
        ret = creatProcessThread((RIL_SOCKET_ID)(RIL_SOCKET_1 + simId));
        if (ret != 0) {
            return;
        }
    }

    // New rild impl calls RIL_startEventLoop() first
    // old standalone impl wants it here.
    if (s_started == 0) {
        RIL_startEventLoop();
    }

    // start listen socket
    for (simId = 0; simId < SIM_COUNT; simId++) {
        startListen((RIL_SOCKET_ID)(RIL_SOCKET_1 + simId), &s_rilSocketParam[simId]);
    }

#if 1
    // start debug interface socket

    char *inst = NULL;
    if (strlen(RIL_getRilSocketName()) >= strlen(SOCKET_NAME_RIL)) {
        inst = RIL_getRilSocketName() + strlen(SOCKET_NAME_RIL);
    }

    char rildebug[MAX_DEBUG_SOCKET_NAME_LENGTH] = SOCKET_NAME_RIL_DEBUG;
    if (inst != NULL) {
        strlcat(rildebug, inst, MAX_DEBUG_SOCKET_NAME_LENGTH);
    }

    s_fdDebug = android_get_control_socket(rildebug);
    if (s_fdDebug < 0) {
        RLOGE("Failed to get socket : %s errno:%d", rildebug, errno);
        exit(-1);
    }

    ret = listen(s_fdDebug, 4);

    if (ret < 0) {
        RLOGE("Failed to listen on ril debug socket '%d': %s",
             s_fdDebug, strerror(errno));
        exit(-1);
    }

    ril_event_set(&s_debugEvent, s_fdDebug, true,
                debugCallback, NULL);

    rilEventAddWakeup(&s_debugEvent);
#endif


    // start ATCI interface socket
    /* Initialize ATCI socket parameters */
    s_atciSocketParam = {
                        RIL_SOCKET_1,               /* socket_id, need modify*/
                        -1,                         /* fdListen */
                        -1,                         /* fdCommand */
                        NULL,                       /* processName */
                        &s_atciCommandsEvent,       /* commands_event */
                        &s_atciListenEvent,         /* listen_event */
                        processCommandsCallback,    /* processCommandsCallback */
                        NULL,                       /* p_rs */
                        RIL_ATCI_SOCKET             /* RIL_SOCKET_TYPE */
                        };

    char atci[MAX_DEBUG_SOCKET_NAME_LENGTH] = SOCKET_NAME_ATCI;
    fdListen = socket_local_server(atci,
                ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
    if (fdListen < 0) {
        RLOGE("Failed to get socket %s", atci);
        exit(-1);
    }

    ret = listen(fdListen, 1);
    if (ret < 0) {
        RLOGE("Failed to listen on control socket '%d': %s",
                fdListen, strerror(errno));
        exit(-1);
    }
    s_atciSocketParam.fdListen = fdListen;

    /* note: we can accept only one atci connection at a time */
    ril_event_set(&s_atciListenEvent, fdListen, false,
            listenCallbackEXT, &s_atciSocketParam);

    rilEventAddWakeup(&s_atciListenEvent);


    // start IMS interface socket
    /* Initialize IMS socket parameters */
    for (simId = 0; simId < SIM_COUNT; simId++) {
        s_imsSocketParam[simId] = {
                                (RIL_SOCKET_ID)(RIL_SOCKET_1 + simId),  /* socket_id */
                                -1,                                     /* fdListen */
                                -1,                                     /* fdCommand */
                                NULL,                                   /* processName */
                                &s_imsCommandsEvent[simId],             /* commands_event */
                                &s_imsListenEvent[simId],               /* listen_event */
                                processCommandsCallback,                /* processCommandsCallback */
                                NULL,                                   /* p_rs */
                                RIL_IMS_SOCKET                          /* RIL_SOCKET_TYPE */
                                };
    }
    char imsSocketName[20];
    for (simId = 0; simId < SIM_COUNT; simId++) {
        snprintf(imsSocketName, sizeof(imsSocketName), "ims_socket%d", (simId + 1));
        startListenEXT(imsSocketName, &s_imsSocketParam[simId]);
    }

    // start OEM interface socket
    /* Initialize OEM socket parameters */
    for (simId = 0; simId < SIM_COUNT; simId++) {
        s_oemSocketParam[simId] = {
                                (RIL_SOCKET_ID)(RIL_SOCKET_1 + simId),  /* socket_id */
                                -1,                                     /* fdListen */
                                -1,                                     /* fdCommand */
                                NULL,                                   /* processName */
                                &s_oemCommandsEvent[simId],             /* commands_event */
                                &s_oemListenEvent[simId],               /* listen_event */
                                processCommandsCallback,                /* processCommandsCallback */
                                NULL,                                   /* p_rs */
                                RIL_OEM_SOCKET                          /* RIL_SOCKET_TYPE */
                                };
    }
    char oemSocketName[20];
    for (simId = 0; simId < SIM_COUNT; simId++) {
        snprintf(oemSocketName, sizeof(oemSocketName), "oem_socket%d", (simId + 1));
        startListenEXT(oemSocketName, &s_oemSocketParam[simId]);
    }
}

extern "C" void
RIL_register_socket(RIL_RadioFunctions *(*Init)(const struct RIL_Env *, int, char **),
        RIL_SOCKET_TYPE socketType, int argc, char **argv) {
    RIL_RadioFunctions *UimFuncs = NULL;

    if (Init) {
        UimFuncs = Init(&RilSapSocket::uimRilEnv, argc, argv);

        switch (socketType) {
            case RIL_SAP_SOCKET:
                RilSapSocket::initSapSocket("sap_uim_socket1", UimFuncs);

#if (SIM_COUNT >= 2)
                RilSapSocket::initSapSocket("sap_uim_socket2", UimFuncs);
#endif

#if (SIM_COUNT >= 3)
                RilSapSocket::initSapSocket("sap_uim_socket3", UimFuncs);
#endif

#if (SIM_COUNT >= 4)
                RilSapSocket::initSapSocket("sap_uim_socket4", UimFuncs);
#endif
            default:
                RLOGD("Wrong socket type!");
        }
    }
}

// Check and remove RequestInfo if its a response and not just ack sent back
static int
checkAndDequeueRequestInfoIfAck(struct RequestInfo *pRI, bool isAck) {
    int ret = 0;

    if (pRI == NULL) {
        return 0;
    }
    /* Hook for current context
       pendingRequestsMutextHook refer to &s_pendingRequestsMutex */
    pthread_mutex_t *pendingRequestsMutexHook = &s_pendingRequestsMutex[pRI->socket_id];
    /* pendingRequestsHook refer to &s_pendingRequests */
    RequestInfo **pendingRequestsHook = &s_pendingRequests[pRI->socket_id];

    pthread_mutex_lock(pendingRequestsMutexHook);

    for (RequestInfo **ppCur = pendingRequestsHook; *ppCur != NULL;
         ppCur = &((*ppCur)->p_next)) {
        if (pRI == *ppCur) {
            ret = 1;
            if (isAck) { // Async ack
                if (pRI->wasAckSent == 1) {
                    RLOGD("Ack was already sent for %s", requestToString(pRI->pCI->requestNumber));
                } else {
                    pRI->wasAckSent = 1;
                }
            } else {
                *ppCur = (*ppCur)->p_next;
            }
            break;
        }
    }

    pthread_mutex_unlock(pendingRequestsMutexHook);

    return ret;
}

static int findFd(int socket_id, RIL_SOCKET_TYPE socket_type) {
    int fd = -1;

    if (socket_type == RIL_TELEPHONY_SOCKET) {
        fd = s_rilSocketParam[socket_id].fdCommand;
    } else if (socket_type == RIL_ATCI_SOCKET) {
        fd = s_atciSocketParam.fdCommand;
    } else if (socket_type == RIL_IMS_SOCKET) {
        fd = s_imsSocketParam[socket_id].fdCommand;
    } else if (socket_type == RIL_OEM_SOCKET) {
        fd = s_oemSocketParam[socket_id].fdCommand;
    }
    return fd;
}

extern "C" void
RIL_onRequestAck(RIL_Token t) {
    RequestInfo *pRI;
    int ret, fd;

    size_t errorOffset;
    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    RIL_SOCKET_TYPE socket_type = RIL_TELEPHONY_SOCKET;

    pRI = (RequestInfo *)t;

    if (!checkAndDequeueRequestInfoIfAck(pRI, true)) {
        RLOGE("RIL_onRequestAck: invalid RIL_Token");
        return;
    }

    socket_id = pRI->socket_id;
    socket_type = pRI->socket_type;
    fd = findFd(socket_id, socket_type);

#if VDBG
    RLOGD("Request Ack, %s", rilSocketIdToString(socket_id));
#endif

    appendPrintBuf("Ack [%04d]< %s", pRI->token,
            requestToString(pRI->pCI->requestNumber));

    if (pRI->cancelled == 0) {
        Parcel p;

        p.writeInt32(RESPONSE_SOLICITED_ACK);
        p.writeInt32(pRI->token);

        if (fd < 0) {
            RLOGE("RIL onRequestComplete: Command channel closed");
        }

        sendResponse(p, socket_id, socket_type);
    }
}

extern "C" void
RIL_onRequestComplete(RIL_Token t, RIL_Errno e, void *response, size_t responselen) {
    RequestInfo *pRI;
    int ret;
    int fd = -1;
    size_t errorOffset;
    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    RIL_SOCKET_TYPE socket_type = RIL_TELEPHONY_SOCKET;

    pRI = (RequestInfo *)t;
    if (!checkAndDequeueRequestInfoIfAck(pRI, false)) {
        RLOGE("RIL_onRequestComplete: invalid RIL_Token");
        return;
    }

    socket_type = pRI->socket_type;
    socket_id = pRI->socket_id;
    fd = findFd(socket_id, socket_type);

#if VDBG
    RLOGD("RequestComplete, %s", rilSocketIdToString(socket_id));
#endif

    if (pRI->local > 0) {
        // Locally issued command...void only!
        // response does not go back up the command socket
        RLOGD("C[locl]< %s", requestToString(pRI->pCI->requestNumber));

        goto done;
    }

    appendPrintBuf("[%04d]< %s",
        pRI->token, requestToString(pRI->pCI->requestNumber));

    if (pRI->cancelled == 0) {
        Parcel p;

        if (s_callbacks.version >= 13 && pRI->wasAckSent == 1) {
            // If ack was already sent, then this call is an asynchronous response. So we need to
            // send id indicating that we expect an ack from RIL.java as we acquire wakelock here.
            p.writeInt32 (RESPONSE_SOLICITED_ACK_EXP);
            grabPartialWakeLock();
        } else {
            p.writeInt32 (RESPONSE_SOLICITED);
        }
        p.writeInt32(pRI->token);
        errorOffset = p.dataPosition();

        p.writeInt32(e);

        if (response != NULL) {
            // there is a response payload, no matter success or not.
            ret = pRI->pCI->responseFunction(p, response, responselen);

            /* if an error occurred, rewind and mark it */
            if (ret != 0) {
                RLOGE("responseFunction error, ret %d", ret);
                p.setDataPosition(errorOffset);
                p.writeInt32(ret);
            }
        }

        if (e != RIL_E_SUCCESS) {
            appendPrintBuf("%s fails by %s", printBuf, failCauseToString(e));
        }

        if (fd < 0) {
            RLOGD("RIL onRequestComplete: Command channel closed");
        }
        sendResponse(p, socket_id, socket_type);
    }

done:
    if (socket_type == RIL_ATCI_SOCKET) {
        if (fd != -1) {
            close(fd);
        }
        s_atciSocketParam.fdCommand = -1;
        if (s_atciSocketParam.p_rs != NULL) {
            record_stream_free(s_atciSocketParam.p_rs);
            s_atciSocketParam.p_rs = NULL;
        }
        RLOGD("start to listen atci socket connect again");
        rilEventAddWakeup(s_atciSocketParam.listen_event);
    }
    free(pRI);
}

static void
grabPartialWakeLock() {
    if (s_callbacks.version >= 13) {
        int ret;
        ret = pthread_mutex_lock(&s_wakeLockCountMutex);
        assert(ret == 0);
        acquire_wake_lock(PARTIAL_WAKE_LOCK, ANDROID_WAKE_LOCK_NAME);

        UserCallbackInfo *p_info =
                internalRequestTimedCallback(wakeTimeoutCallback, NULL, &TIMEVAL_WAKE_TIMEOUT);
        if (p_info == NULL) {
            release_wake_lock(ANDROID_WAKE_LOCK_NAME);
        } else {
            s_wakelock_count++;
            if (s_lastWakeTimeoutInfo != NULL) {
                s_lastWakeTimeoutInfo->userParam = (void *)1;
            }
            s_lastWakeTimeoutInfo = p_info;
        }
        ret = pthread_mutex_unlock(&s_wakeLockCountMutex);
        assert(ret == 0);
    } else {
        acquire_wake_lock(PARTIAL_WAKE_LOCK, ANDROID_WAKE_LOCK_NAME);
    }
}

static void
releaseWakeLock() {
    if (s_callbacks.version >= 13) {
        int ret;
        ret = pthread_mutex_lock(&s_wakeLockCountMutex);
        assert(ret == 0);

        if (s_wakelock_count > 1) {
            s_wakelock_count--;
        } else {
            s_wakelock_count = 0;
            release_wake_lock(ANDROID_WAKE_LOCK_NAME);
            if (s_lastWakeTimeoutInfo != NULL) {
                s_lastWakeTimeoutInfo->userParam = (void *)1;
            }
        }

        ret = pthread_mutex_unlock(&s_wakeLockCountMutex);
        assert(ret == 0);
    } else {
        release_wake_lock(ANDROID_WAKE_LOCK_NAME);
    }
}

/**
 * Timer callback to put us back to sleep before the default timeout
 */
static void
wakeTimeoutCallback (void *param) {
    // We're using "param != NULL" as a cancellation mechanism
    if (s_callbacks.version >= 13) {
        if (param == NULL) {
            int ret;
            ret = pthread_mutex_lock(&s_wakeLockCountMutex);
            assert(ret == 0);
            s_wakelock_count = 0;
            release_wake_lock(ANDROID_WAKE_LOCK_NAME);
            ret = pthread_mutex_unlock(&s_wakeLockCountMutex);
            assert(ret == 0);
        }
    } else {
        if (param == NULL) {
            releaseWakeLock();
        }
    }
}

static int decodeVoiceRadioTechnology(RIL_RadioState radioState) {
    switch (radioState) {
        case RADIO_STATE_SIM_NOT_READY:
        case RADIO_STATE_SIM_LOCKED_OR_ABSENT:
        case RADIO_STATE_SIM_READY:
            return RADIO_TECH_UMTS;

        case RADIO_STATE_RUIM_NOT_READY:
        case RADIO_STATE_RUIM_READY:
        case RADIO_STATE_RUIM_LOCKED_OR_ABSENT:
        case RADIO_STATE_NV_NOT_READY:
        case RADIO_STATE_NV_READY:
            return RADIO_TECH_1xRTT;

        default:
            RLOGD("decodeVoiceRadioTechnology: Invoked with incorrect RadioState");
            return -1;
    }
}

static int decodeCdmaSubscriptionSource(RIL_RadioState radioState) {
    switch (radioState) {
        case RADIO_STATE_SIM_NOT_READY:
        case RADIO_STATE_SIM_LOCKED_OR_ABSENT:
        case RADIO_STATE_SIM_READY:
        case RADIO_STATE_RUIM_NOT_READY:
        case RADIO_STATE_RUIM_READY:
        case RADIO_STATE_RUIM_LOCKED_OR_ABSENT:
            return CDMA_SUBSCRIPTION_SOURCE_RUIM_SIM;

        case RADIO_STATE_NV_NOT_READY:
        case RADIO_STATE_NV_READY:
            return CDMA_SUBSCRIPTION_SOURCE_NV;

        default:
            RLOGD("decodeCdmaSubscriptionSource: Invoked with incorrect RadioState");
            return -1;
    }
}

static int decodeSimStatus(RIL_RadioState radioState) {
    switch (radioState) {
       case RADIO_STATE_SIM_NOT_READY:
       case RADIO_STATE_RUIM_NOT_READY:
       case RADIO_STATE_NV_NOT_READY:
       case RADIO_STATE_NV_READY:
           return -1;
       case RADIO_STATE_SIM_LOCKED_OR_ABSENT:
       case RADIO_STATE_SIM_READY:
       case RADIO_STATE_RUIM_READY:
       case RADIO_STATE_RUIM_LOCKED_OR_ABSENT:
           return radioState;
       default:
           RLOGD("decodeSimStatus: Invoked with incorrect RadioState");
           return -1;
    }
}

static bool is3gpp2(int radioTech) {
    switch (radioTech) {
        case RADIO_TECH_IS95A:
        case RADIO_TECH_IS95B:
        case RADIO_TECH_1xRTT:
        case RADIO_TECH_EVDO_0:
        case RADIO_TECH_EVDO_A:
        case RADIO_TECH_EVDO_B:
        case RADIO_TECH_EHRPD:
            return true;
        default:
            return false;
    }
}

/* If RIL sends SIM states or RUIM states, store the voice radio
 * technology and subscription source information so that they can be
 * returned when telephony framework requests them
 */
static RIL_RadioState
processRadioState(RIL_RadioState newRadioState, RIL_SOCKET_ID socket_id) {
    if ((newRadioState > RADIO_STATE_UNAVAILABLE) && (newRadioState < RADIO_STATE_ON)) {
        int newVoiceRadioTech;
        int newCdmaSubscriptionSource;
        int newSimStatus;

        /* This is old RIL. Decode Subscription source and Voice Radio Technology
           from Radio State and send change notifications if there has been a change */
        newVoiceRadioTech = decodeVoiceRadioTechnology(newRadioState);
        if (newVoiceRadioTech != s_voiceRadioTech) {
            s_voiceRadioTech = newVoiceRadioTech;
            RIL_UNSOL_RESPONSE(RIL_UNSOL_VOICE_RADIO_TECH_CHANGED,
                        &s_voiceRadioTech, sizeof(s_voiceRadioTech), socket_id);
        }
        if (is3gpp2(newVoiceRadioTech)) {
            newCdmaSubscriptionSource = decodeCdmaSubscriptionSource(newRadioState);
            if (newCdmaSubscriptionSource != s_cdmaSubscriptionSource) {
                s_cdmaSubscriptionSource = newCdmaSubscriptionSource;
                RIL_UNSOL_RESPONSE(RIL_UNSOL_CDMA_SUBSCRIPTION_SOURCE_CHANGED,
                        &s_cdmaSubscriptionSource, sizeof(s_cdmaSubscriptionSource), socket_id);
            }
        }
        newSimStatus = decodeSimStatus(newRadioState);
        if (newSimStatus != s_simRuimStatus) {
            s_simRuimStatus = newSimStatus;
            RIL_UNSOL_RESPONSE(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED, NULL, 0, socket_id);
        }

        /* Send RADIO_ON to telephony */
        newRadioState = RADIO_STATE_ON;
    }

    return newRadioState;
}

#if defined(ANDROID_MULTI_SIM)
extern "C" void RIL_onUnsolicitedResponse(int unsolResponse, const void *data,
        size_t datalen, RIL_SOCKET_ID socket_id)
#else
extern "C" void RIL_onUnsolicitedResponse(int unsolResponse, const void *data,
        size_t datalen)
#endif
{
    int unsolResponseIndex;
    int ret = -1;
    int64_t timeReceived = 0;
    bool shouldScheduleTimeout = false;
    RIL_RadioState newState;
    RIL_SOCKET_ID soc_id = RIL_SOCKET_1;
    RIL_SOCKET_TYPE socket_type = RIL_TELEPHONY_SOCKET;
    UnsolResponseInfo *pURI = NULL;

#if defined(ANDROID_MULTI_SIM)
    soc_id = socket_id;
#endif

    if (s_registerCalled == 0) {
        // Ignore RIL_onUnsolicitedResponse before RIL_register
        RLOGW("RIL_onUnsolicitedResponse called before RIL_register");
        return;
    }

    if ((unsolResponse < RIL_UNSOL_RESPONSE_BASE)
        || (unsolResponse > RIL_UNSOL_RESPONSE_LAST
                 && unsolResponse < RIL_IMS_UNSOL_RESPONSE_BASE)
        || (unsolResponse > RIL_IMS_UNSOL_RESPONSE_LAST
                && unsolResponse < RIL_EXT_UNSOL_RESPONSE_BASE)
        || (unsolResponse > RIL_EXT_UNSOL_RESPONSE_LAST)) {
        RLOGE("unsupported unsolicited response code %d", unsolResponse);
        return;
    }

    if (unsolResponse >= RIL_UNSOL_RESPONSE_BASE
            && unsolResponse <= RIL_UNSOL_RESPONSE_LAST) {
        unsolResponseIndex = unsolResponse - RIL_UNSOL_RESPONSE_BASE;
        socket_type = RIL_TELEPHONY_SOCKET;
        pURI = &(s_unsolResponses[unsolResponseIndex]);
    } else if (unsolResponse >= RIL_IMS_UNSOL_RESPONSE_BASE
            && unsolResponse <= RIL_IMS_UNSOL_RESPONSE_LAST) {
        unsolResponseIndex = unsolResponse - RIL_IMS_UNSOL_RESPONSE_BASE;
        socket_type = RIL_IMS_SOCKET;
        pURI = &(s_ims_unsolResponses[unsolResponseIndex]);
    } else if (unsolResponse >= RIL_EXT_UNSOL_RESPONSE_BASE
            && unsolResponse <= RIL_EXT_UNSOL_RESPONSE_LAST) {
        unsolResponseIndex = unsolResponse - RIL_EXT_UNSOL_RESPONSE_BASE;
        socket_type = RIL_OEM_SOCKET;
        pURI = &(s_oem_unsolResponses[unsolResponseIndex]);
    }

    // Grab a wake lock if needed for this reponse,
    // as we exit we'll either release it immediately
    // or set a timer to release it later.
    switch (pURI->wakeType) {
        case WAKE_PARTIAL:
            grabPartialWakeLock();
            shouldScheduleTimeout = true;
        break;

        case DONT_WAKE:
        default:
            // No wake lock is grabed so don't set timeout
            shouldScheduleTimeout = false;
            break;
    }

    // Mark the time this was received, doing this
    // after grabing the wakelock incase getting
    // the elapsedRealTime might cause us to goto
    // sleep.
    if (unsolResponse == RIL_UNSOL_NITZ_TIME_RECEIVED) {
        timeReceived = elapsedRealtime();
    }

    appendPrintBuf("[UNSL]< %s", requestToString(unsolResponse));

    Parcel p;

    if (s_callbacks.version >= 13
                && s_unsolResponses[unsolResponseIndex].wakeType == WAKE_PARTIAL) {
        p.writeInt32 (RESPONSE_UNSOLICITED_ACK_EXP);
    } else {
        p.writeInt32 (RESPONSE_UNSOLICITED);
    }
    p.writeInt32(unsolResponse);

    ret = pURI->responseFunction(p, const_cast<void*>(data), datalen);

    if (ret != 0) {
        // Problem with the response. Don't continue;
        goto error_exit;
    }

    // some things get more payload
    switch (unsolResponse) {
        case RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED:
            newState = processRadioState(CALL_ONSTATEREQUEST(soc_id), soc_id);
            p.writeInt32(newState);
            appendPrintBuf("%s {%s}", printBuf,
                radioStateToString(CALL_ONSTATEREQUEST(soc_id)));
        break;


        case RIL_UNSOL_NITZ_TIME_RECEIVED:
            // Store the time that this was received so the
            // handler of this message can account for
            // the time it takes to arrive and process. In
            // particular the system has been known to sleep
            // before this message can be processed.
            p.writeInt64(timeReceived);
        break;
    }

    if (s_callbacks.version < 13) {
        if (shouldScheduleTimeout) {
            UserCallbackInfo *p_info = internalRequestTimedCallback(wakeTimeoutCallback, NULL,
                    &TIMEVAL_WAKE_TIMEOUT);

            if (p_info == NULL) {
                goto error_exit;
            } else {
                // Cancel the previous request
                if (s_lastWakeTimeoutInfo != NULL) {
                    s_lastWakeTimeoutInfo->userParam = (void *)1;
                }
                s_lastWakeTimeoutInfo = p_info;
            }
        }
    }

#if VDBG
    RLOGI("%s UNSOLICITED: %s length:%d",
            rilSocketIdToString(soc_id), requestToString(unsolResponse), p.dataSize());
#endif
    ret = sendResponse(p, soc_id, socket_type);
    if (ret != 0 && unsolResponse == RIL_UNSOL_NITZ_TIME_RECEIVED) {
        // Unfortunately, NITZ time is not poll/update like everything
        // else in the system. So, if the upstream client isn't connected,
        // keep a copy of the last NITZ response (with receive time noted
        // above) around so we can deliver it when it is connected

        if (s_lastNITZTimeData != NULL) {
            free(s_lastNITZTimeData);
            s_lastNITZTimeData = NULL;
        }

        s_lastNITZTimeData = calloc(p.dataSize(), 1);
        if (s_lastNITZTimeData == NULL) {
             RLOGE("Memory allocation failed in RIL_onUnsolicitedResponse");
             goto error_exit;
        }
        s_lastNITZTimeDataSize = p.dataSize();
        memcpy(s_lastNITZTimeData, p.data(), p.dataSize());
    }

    // Normal exit
    return;

error_exit:
    if (shouldScheduleTimeout) {
        releaseWakeLock();
    }
}

/** FIXME generalize this if you track UserCallbackInfo, clear it
    when the callback occurs
*/
static UserCallbackInfo *internalRequestTimedCallback(RIL_TimedCallback callback,
        void *param, const struct timeval *relativeTime) {
    struct timeval myRelativeTime;
    UserCallbackInfo *p_info;

    p_info = (UserCallbackInfo *) calloc(1, sizeof(UserCallbackInfo));
    if (p_info == NULL) {
        RLOGE("Memory allocation failed in internalRequestTimedCallback");
        return p_info;

    }

    p_info->p_callback = callback;
    p_info->userParam = param;

    if (relativeTime == NULL) {
        /* treat null parameter as a 0 relative time */
        memset(&myRelativeTime, 0, sizeof(myRelativeTime));
    } else {
        /* FIXME I think event_add's tv param is really const anyway */
        memcpy(&myRelativeTime, relativeTime, sizeof(myRelativeTime));
    }

    ril_event_set(&(p_info->event), -1, false, userTimerCallback, p_info);

    ril_timer_add(&(p_info->event), &myRelativeTime);

    triggerEvLoop();
    return p_info;
}


extern "C" void RIL_requestTimedCallback(RIL_TimedCallback callback, void *param,
                                const struct timeval *relativeTime) {
    internalRequestTimedCallback(callback, param, relativeTime);
}


const char *failCauseToString(RIL_Errno e) {
    switch (e) {
        case RIL_E_SUCCESS: return "E_SUCCESS";
        case RIL_E_RADIO_NOT_AVAILABLE: return "E_RADIO_NOT_AVAILABLE";
        case RIL_E_GENERIC_FAILURE: return "E_GENERIC_FAILURE";
        case RIL_E_PASSWORD_INCORRECT: return "E_PASSWORD_INCORRECT";
        case RIL_E_SIM_PIN2: return "E_SIM_PIN2";
        case RIL_E_SIM_PUK2: return "E_SIM_PUK2";
        case RIL_E_REQUEST_NOT_SUPPORTED: return "E_REQUEST_NOT_SUPPORTED";
        case RIL_E_CANCELLED: return "E_CANCELLED";
        case RIL_E_OP_NOT_ALLOWED_DURING_VOICE_CALL: return "E_OP_NOT_ALLOWED_DURING_VOICE_CALL";
        case RIL_E_OP_NOT_ALLOWED_BEFORE_REG_TO_NW: return "E_OP_NOT_ALLOWED_BEFORE_REG_TO_NW";
        case RIL_E_SMS_SEND_FAIL_RETRY: return "E_SMS_SEND_FAIL_RETRY";
        case RIL_E_SIM_ABSENT:return "E_SIM_ABSENT";
        case RIL_E_ILLEGAL_SIM_OR_ME:return "E_ILLEGAL_SIM_OR_ME";
#ifdef FEATURE_MULTIMODE_ANDROID
        case RIL_E_SUBSCRIPTION_NOT_AVAILABLE:return "E_SUBSCRIPTION_NOT_AVAILABLE";
        case RIL_E_MODE_NOT_SUPPORTED:return "E_MODE_NOT_SUPPORTED";
#endif
        case RIL_E_FDN_CHECK_FAILURE: return "E_FDN_CHECK_FAILURE";
        case RIL_E_MISSING_RESOURCE: return "E_MISSING_RESOURCE";
        case RIL_E_NO_SUCH_ELEMENT: return "E_NO_SUCH_ELEMENT";
        case RIL_E_DIAL_MODIFIED_TO_USSD: return "E_DIAL_MODIFIED_TO_USSD";
        case RIL_E_DIAL_MODIFIED_TO_SS: return "E_DIAL_MODIFIED_TO_SS";
        case RIL_E_DIAL_MODIFIED_TO_DIAL: return "E_DIAL_MODIFIED_TO_DIAL";
        case RIL_E_USSD_MODIFIED_TO_DIAL: return "E_USSD_MODIFIED_TO_DIAL";
        case RIL_E_USSD_MODIFIED_TO_SS: return "E_USSD_MODIFIED_TO_SS";
        case RIL_E_USSD_MODIFIED_TO_USSD: return "E_USSD_MODIFIED_TO_USSD";
        case RIL_E_SS_MODIFIED_TO_DIAL: return "E_SS_MODIFIED_TO_DIAL";
        case RIL_E_SS_MODIFIED_TO_USSD: return "E_SS_MODIFIED_TO_USSD";
        case RIL_E_SUBSCRIPTION_NOT_SUPPORTED: return "E_SUBSCRIPTION_NOT_SUPPORTED";
        case RIL_E_SS_MODIFIED_TO_SS: return "E_SS_MODIFIED_TO_SS";
        case RIL_E_LCE_NOT_SUPPORTED: return "E_LCE_NOT_SUPPORTED";
        case RIL_E_NO_MEMORY: return "E_NO_MEMORY";
        case RIL_E_INTERNAL_ERR: return "E_INTERNAL_ERR";
        case RIL_E_SYSTEM_ERR: return "E_SYSTEM_ERR";
        case RIL_E_MODEM_ERR: return "E_MODEM_ERR";
        case RIL_E_INVALID_STATE: return "E_INVALID_STATE";
        case RIL_E_NO_RESOURCES: return "E_NO_RESOURCES";
        case RIL_E_SIM_ERR: return "E_SIM_ERR";
        case RIL_E_INVALID_ARGUMENTS: return "E_INVALID_ARGUMENTS";
        case RIL_E_INVALID_SIM_STATE: return "E_INVALID_SIM_STATE";
        case RIL_E_INVALID_MODEM_STATE: return "E_INVALID_MODEM_STATE";
        case RIL_E_INVALID_CALL_ID: return "E_INVALID_CALL_ID";
        case RIL_E_NO_SMS_TO_ACK: return "E_NO_SMS_TO_ACK";
        case RIL_E_NETWORK_ERR: return "E_NETWORK_ERR";
        case RIL_E_REQUEST_RATE_LIMITED: return "E_REQUEST_RATE_LIMITED";
        case RIL_E_SIM_BUSY: return "E_SIM_BUSY";
        case RIL_E_SIM_FULL: return "E_SIM_FULL";
        case RIL_E_NETWORK_REJECT: return "E_NETWORK_REJECT";
        case RIL_E_OPERATION_NOT_ALLOWED: return "E_OPERATION_NOT_ALLOWED";
        case RIL_E_EMPTY_RECORD: "E_EMPTY_RECORD";
        case RIL_E_INVALID_SMS_FORMAT: return "E_INVALID_SMS_FORMAT";
        case RIL_E_ENCODING_ERR: return "E_ENCODING_ERR";
        case RIL_E_INVALID_SMSC_ADDRESS: return "E_INVALID_SMSC_ADDRESS";
        case RIL_E_NO_SUCH_ENTRY: return "E_NO_SUCH_ENTRY";
        case RIL_E_NETWORK_NOT_READY: return "E_NETWORK_NOT_READY";
        case RIL_E_NOT_PROVISIONED: return "E_NOT_PROVISIONED";
        case RIL_E_NO_SUBSCRIPTION: return "E_NO_SUBSCRIPTION";
        case RIL_E_NO_NETWORK_FOUND: return "E_NO_NETWORK_FOUND";
        case RIL_E_DEVICE_IN_USE: return "E_DEVICE_IN_USE";
        case RIL_E_ABORTED: return "E_ABORTED";
        case RIL_E_OEM_ERROR_1: return "E_OEM_ERROR_1";
        case RIL_E_OEM_ERROR_2: return "E_OEM_ERROR_2";
        case RIL_E_OEM_ERROR_3: return "E_OEM_ERROR_3";
        case RIL_E_OEM_ERROR_4: return "E_OEM_ERROR_4";
        case RIL_E_OEM_ERROR_5: return "E_OEM_ERROR_5";
        case RIL_E_OEM_ERROR_6: return "E_OEM_ERROR_6";
        case RIL_E_OEM_ERROR_7: return "E_OEM_ERROR_7";
        case RIL_E_OEM_ERROR_8: return "E_OEM_ERROR_8";
        case RIL_E_OEM_ERROR_9: return "E_OEM_ERROR_9";
        case RIL_E_OEM_ERROR_10: return "E_OEM_ERROR_10";
        case RIL_E_OEM_ERROR_11: return "E_OEM_ERROR_11";
        case RIL_E_OEM_ERROR_12: return "E_OEM_ERROR_12";
        case RIL_E_OEM_ERROR_13: return "E_OEM_ERROR_13";
        case RIL_E_OEM_ERROR_14: return "E_OEM_ERROR_14";
        case RIL_E_OEM_ERROR_15: return "E_OEM_ERROR_15";
        case RIL_E_OEM_ERROR_16: return "E_OEM_ERROR_16";
        case RIL_E_OEM_ERROR_17: return "E_OEM_ERROR_17";
        case RIL_E_OEM_ERROR_18: return "E_OEM_ERROR_18";
        case RIL_E_OEM_ERROR_19: return "E_OEM_ERROR_19";
        case RIL_E_OEM_ERROR_20: return "E_OEM_ERROR_20";
        case RIL_E_OEM_ERROR_21: return "E_OEM_ERROR_21";
        case RIL_E_OEM_ERROR_22: return "E_OEM_ERROR_22";
        case RIL_E_OEM_ERROR_23: return "E_OEM_ERROR_23";
        case RIL_E_OEM_ERROR_24: return "E_OEM_ERROR_24";
        case RIL_E_OEM_ERROR_25: return "E_OEM_ERROR_25";
        default: return "<unknown error>";
    }
}

const char *radioStateToString(RIL_RadioState s) {
    switch (s) {
        case RADIO_STATE_OFF: return "RADIO_OFF";
        case RADIO_STATE_UNAVAILABLE: return "RADIO_UNAVAILABLE";
        case RADIO_STATE_SIM_NOT_READY: return "RADIO_SIM_NOT_READY";
        case RADIO_STATE_SIM_LOCKED_OR_ABSENT: return "RADIO_SIM_LOCKED_OR_ABSENT";
        case RADIO_STATE_SIM_READY: return "RADIO_SIM_READY";
        case RADIO_STATE_RUIM_NOT_READY:return"RADIO_RUIM_NOT_READY";
        case RADIO_STATE_RUIM_READY:return"RADIO_RUIM_READY";
        case RADIO_STATE_RUIM_LOCKED_OR_ABSENT:return"RADIO_RUIM_LOCKED_OR_ABSENT";
        case RADIO_STATE_NV_NOT_READY:return"RADIO_NV_NOT_READY";
        case RADIO_STATE_NV_READY:return"RADIO_NV_READY";
        case RADIO_STATE_ON:return"RADIO_ON";
        default: return "<unknown state>";
    }
}

const char *callStateToString(RIL_CallState s) {
    switch (s) {
        case RIL_CALL_ACTIVE : return "ACTIVE";
        case RIL_CALL_HOLDING: return "HOLDING";
        case RIL_CALL_DIALING: return "DIALING";
        case RIL_CALL_ALERTING: return "ALERTING";
        case RIL_CALL_INCOMING: return "INCOMING";
        case RIL_CALL_WAITING: return "WAITING";
        default: return "<unknown state>";
    }
}

const char *requestToString(int request) {
/*
 cat libs/telephony/ril_commands.h \
 | egrep "^ *{RIL_" \
 | sed -re 's/\{RIL_([^,]+),[^,]+,([^}]+).+/case RIL_\1: return "\1";/'


 cat libs/telephony/ril_unsol_commands.h \
 | egrep "^ *{RIL_" \
 | sed -re 's/\{RIL_([^,]+),([^}]+).+/case RIL_\1: return "\1";/'
*/
    switch (request) {
        case RIL_REQUEST_GET_SIM_STATUS: return "GET_SIM_STATUS";
        case RIL_REQUEST_ENTER_SIM_PIN: return "ENTER_SIM_PIN";
        case RIL_REQUEST_ENTER_SIM_PUK: return "ENTER_SIM_PUK";
        case RIL_REQUEST_ENTER_SIM_PIN2: return "ENTER_SIM_PIN2";
        case RIL_REQUEST_ENTER_SIM_PUK2: return "ENTER_SIM_PUK2";
        case RIL_REQUEST_CHANGE_SIM_PIN: return "CHANGE_SIM_PIN";
        case RIL_REQUEST_CHANGE_SIM_PIN2: return "CHANGE_SIM_PIN2";
        case RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION: return "ENTER_NETWORK_DEPERSONALIZATION";
        case RIL_REQUEST_GET_CURRENT_CALLS: return "GET_CURRENT_CALLS";
        case RIL_REQUEST_DIAL: return "DIAL";
        case RIL_REQUEST_GET_IMSI: return "GET_IMSI";
        case RIL_REQUEST_HANGUP: return "HANGUP";
        case RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND: return "HANGUP_WAITING_OR_BACKGROUND";
        case RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND: return "HANGUP_FOREGROUND_RESUME_BACKGROUND";
        case RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE: return "SWITCH_WAITING_OR_HOLDING_AND_ACTIVE";
        case RIL_REQUEST_CONFERENCE: return "CONFERENCE";
        case RIL_REQUEST_UDUB: return "UDUB";
        case RIL_REQUEST_LAST_CALL_FAIL_CAUSE: return "LAST_CALL_FAIL_CAUSE";
        case RIL_REQUEST_SIGNAL_STRENGTH: return "SIGNAL_STRENGTH";
        case RIL_REQUEST_VOICE_REGISTRATION_STATE: return "VOICE_REGISTRATION_STATE";
        case RIL_REQUEST_DATA_REGISTRATION_STATE: return "DATA_REGISTRATION_STATE";
        case RIL_REQUEST_OPERATOR: return "OPERATOR";
        case RIL_REQUEST_RADIO_POWER: return "RADIO_POWER";
        case RIL_REQUEST_DTMF: return "DTMF";
        case RIL_REQUEST_SEND_SMS: return "SEND_SMS";
        case RIL_REQUEST_SEND_SMS_EXPECT_MORE: return "SEND_SMS_EXPECT_MORE";
        case RIL_REQUEST_SETUP_DATA_CALL: return "SETUP_DATA_CALL";
        case RIL_REQUEST_SIM_IO: return "SIM_IO";
        case RIL_REQUEST_SEND_USSD: return "SEND_USSD";
        case RIL_REQUEST_CANCEL_USSD: return "CANCEL_USSD";
        case RIL_REQUEST_GET_CLIR: return "GET_CLIR";
        case RIL_REQUEST_SET_CLIR: return "SET_CLIR";
        case RIL_REQUEST_QUERY_CALL_FORWARD_STATUS: return "QUERY_CALL_FORWARD_STATUS";
        case RIL_REQUEST_SET_CALL_FORWARD: return "SET_CALL_FORWARD";
        case RIL_REQUEST_QUERY_CALL_WAITING: return "QUERY_CALL_WAITING";
        case RIL_REQUEST_SET_CALL_WAITING: return "SET_CALL_WAITING";
        case RIL_REQUEST_SMS_ACKNOWLEDGE: return "SMS_ACKNOWLEDGE";
        case RIL_REQUEST_GET_IMEI: return "GET_IMEI";
        case RIL_REQUEST_GET_IMEISV: return "GET_IMEISV";
        case RIL_REQUEST_ANSWER: return "ANSWER";
        case RIL_REQUEST_DEACTIVATE_DATA_CALL: return "DEACTIVATE_DATA_CALL";
        case RIL_REQUEST_QUERY_FACILITY_LOCK: return "QUERY_FACILITY_LOCK";
        case RIL_REQUEST_SET_FACILITY_LOCK: return "SET_FACILITY_LOCK";
        case RIL_REQUEST_CHANGE_BARRING_PASSWORD: return "CHANGE_BARRING_PASSWORD";
        case RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE: return "QUERY_NETWORK_SELECTION_MODE";
        case RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC: return "SET_NETWORK_SELECTION_AUTOMATIC";
        case RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL: return "SET_NETWORK_SELECTION_MANUAL";
        case RIL_REQUEST_QUERY_AVAILABLE_NETWORKS : return "QUERY_AVAILABLE_NETWORKS ";
        case RIL_REQUEST_DTMF_START: return "DTMF_START";
        case RIL_REQUEST_DTMF_STOP: return "DTMF_STOP";
        case RIL_REQUEST_BASEBAND_VERSION: return "BASEBAND_VERSION";
        case RIL_REQUEST_SEPARATE_CONNECTION: return "SEPARATE_CONNECTION";
        case RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE: return "SET_PREFERRED_NETWORK_TYPE";
        case RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE: return "GET_PREFERRED_NETWORK_TYPE";
        case RIL_REQUEST_GET_NEIGHBORING_CELL_IDS: return "GET_NEIGHBORING_CELL_IDS";
        case RIL_REQUEST_SET_MUTE: return "SET_MUTE";
        case RIL_REQUEST_GET_MUTE: return "GET_MUTE";
        case RIL_REQUEST_QUERY_CLIP: return "QUERY_CLIP";
        case RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE: return "LAST_DATA_CALL_FAIL_CAUSE";
        case RIL_REQUEST_DATA_CALL_LIST: return "DATA_CALL_LIST";
        case RIL_REQUEST_RESET_RADIO: return "RESET_RADIO";
        case RIL_REQUEST_OEM_HOOK_RAW: return "OEM_HOOK_RAW";
        case RIL_REQUEST_OEM_HOOK_STRINGS: return "OEM_HOOK_STRINGS";
        case RIL_REQUEST_SET_BAND_MODE: return "SET_BAND_MODE";
        case RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE: return "QUERY_AVAILABLE_BAND_MODE";
        case RIL_REQUEST_STK_GET_PROFILE: return "STK_GET_PROFILE";
        case RIL_REQUEST_STK_SET_PROFILE: return "STK_SET_PROFILE";
        case RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND: return "STK_SEND_ENVELOPE_COMMAND";
        case RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE: return "STK_SEND_TERMINAL_RESPONSE";
        case RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM: return "STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM";
        case RIL_REQUEST_SCREEN_STATE: return "SCREEN_STATE";
        case RIL_REQUEST_EXPLICIT_CALL_TRANSFER: return "EXPLICIT_CALL_TRANSFER";
        case RIL_REQUEST_SET_LOCATION_UPDATES: return "SET_LOCATION_UPDATES";
        case RIL_REQUEST_CDMA_SET_SUBSCRIPTION_SOURCE:return"CDMA_SET_SUBSCRIPTION_SOURCE";
        case RIL_REQUEST_CDMA_SET_ROAMING_PREFERENCE:return"CDMA_SET_ROAMING_PREFERENCE";
        case RIL_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE:return"CDMA_QUERY_ROAMING_PREFERENCE";
        case RIL_REQUEST_SET_TTY_MODE:return"SET_TTY_MODE";
        case RIL_REQUEST_QUERY_TTY_MODE:return"QUERY_TTY_MODE";
        case RIL_REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE:return"CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE";
        case RIL_REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE:return"CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE";
        case RIL_REQUEST_CDMA_FLASH:return"CDMA_FLASH";
        case RIL_REQUEST_CDMA_BURST_DTMF:return"CDMA_BURST_DTMF";
        case RIL_REQUEST_CDMA_SEND_SMS:return"CDMA_SEND_SMS";
        case RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE:return"CDMA_SMS_ACKNOWLEDGE";
        case RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG:return"GSM_GET_BROADCAST_SMS_CONFIG";
        case RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG:return"GSM_SET_BROADCAST_SMS_CONFIG";
        case RIL_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG:return "CDMA_GET_BROADCAST_SMS_CONFIG";
        case RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG:return "CDMA_SET_BROADCAST_SMS_CONFIG";
        case RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION:return "CDMA_SMS_BROADCAST_ACTIVATION";
        case RIL_REQUEST_CDMA_VALIDATE_AND_WRITE_AKEY: return"CDMA_VALIDATE_AND_WRITE_AKEY";
        case RIL_REQUEST_CDMA_SUBSCRIPTION: return"CDMA_SUBSCRIPTION";
        case RIL_REQUEST_CDMA_WRITE_SMS_TO_RUIM: return "CDMA_WRITE_SMS_TO_RUIM";
        case RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM: return "CDMA_DELETE_SMS_ON_RUIM";
        case RIL_REQUEST_DEVICE_IDENTITY: return "DEVICE_IDENTITY";
        case RIL_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE: return "EXIT_EMERGENCY_CALLBACK_MODE";
        case RIL_REQUEST_GET_SMSC_ADDRESS: return "GET_SMSC_ADDRESS";
        case RIL_REQUEST_SET_SMSC_ADDRESS: return "SET_SMSC_ADDRESS";
        case RIL_REQUEST_REPORT_SMS_MEMORY_STATUS: return "REPORT_SMS_MEMORY_STATUS";
        case RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING: return "REPORT_STK_SERVICE_IS_RUNNING";
        case RIL_REQUEST_CDMA_GET_SUBSCRIPTION_SOURCE: return "CDMA_GET_SUBSCRIPTION_SOURCE";
        case RIL_REQUEST_ISIM_AUTHENTICATION: return "ISIM_AUTHENTICATION";
        case RIL_REQUEST_ACKNOWLEDGE_INCOMING_GSM_SMS_WITH_PDU: return "RIL_REQUEST_ACKNOWLEDGE_INCOMING_GSM_SMS_WITH_PDU";
        case RIL_REQUEST_STK_SEND_ENVELOPE_WITH_STATUS: return "RIL_REQUEST_STK_SEND_ENVELOPE_WITH_STATUS";
        case RIL_REQUEST_VOICE_RADIO_TECH: return "VOICE_RADIO_TECH";
        case RIL_REQUEST_WRITE_SMS_TO_SIM: return "WRITE_SMS_TO_SIM";
        case RIL_REQUEST_GET_CELL_INFO_LIST: return"GET_CELL_INFO_LIST";
        case RIL_REQUEST_SET_UNSOL_CELL_INFO_LIST_RATE: return"SET_UNSOL_CELL_INFO_LIST_RATE";
        case RIL_REQUEST_SET_INITIAL_ATTACH_APN: return "RIL_REQUEST_SET_INITIAL_ATTACH_APN";
        case RIL_REQUEST_IMS_REGISTRATION_STATE: return "IMS_REGISTRATION_STATE";
        case RIL_REQUEST_IMS_SEND_SMS: return "IMS_SEND_SMS";
        case RIL_REQUEST_SIM_TRANSMIT_APDU_BASIC: return "SIM_TRANSMIT_APDU_BASIC";
        case RIL_REQUEST_SIM_OPEN_CHANNEL: return "SIM_OPEN_CHANNEL";
        case RIL_REQUEST_SIM_CLOSE_CHANNEL: return "SIM_CLOSE_CHANNEL";
        case RIL_REQUEST_SIM_TRANSMIT_APDU_CHANNEL: return "SIM_TRANSMIT_APDU_CHANNEL";
        case RIL_REQUEST_GET_RADIO_CAPABILITY: return "GET_RADIO_CAPABILITY";
        case RIL_REQUEST_SET_RADIO_CAPABILITY: return "SET_RADIO_CAPABILITY";
        case RIL_REQUEST_START_LCE : return "START_LCE";
        case RIL_REQUEST_STOP_LCE : return "STOP_LCE";
        case RIL_REQUEST_SET_UICC_SUBSCRIPTION: return "SET_UICC_SUBSCRIPTION";
        case RIL_REQUEST_ALLOW_DATA: return "ALLOW_DATA";
        case RIL_REQUEST_GET_HARDWARE_CONFIG: return "GET_HARDWARE_CONFIG";
        case RIL_REQUEST_SIM_AUTHENTICATION: return "SIM_AUTHENTICATION";
        case RIL_REQUEST_GET_DC_RT_INFO: return "GET_DC_RT_INFO";
        case RIL_REQUEST_SET_DC_RT_INFO_RATE: return "SET_DC_RT_INFO_RATE";
        case RIL_REQUEST_SET_DATA_PROFILE: return "SET_DATA_PROFILE";
        case RIL_REQUEST_SHUTDOWN: return "SHUTDOWN";
        /* IMS @{ */
        case RIL_REQUEST_GET_IMS_CURRENT_CALLS: return "GET_IMS_CURRENT_CALLS";
        case RIL_REQUEST_SET_IMS_VOICE_CALL_AVAILABILITY: return "SET_IMS_VOICE_CALL_AVAILABILITY";
        case RIL_REQUEST_GET_IMS_VOICE_CALL_AVAILABILITY: return "GET_IMS_VOICE_CALL_AVAILABILITY";
        case RIL_REQUEST_INIT_ISIM: return "INIT_ISIM";
        case RIL_REQUEST_IMS_CALL_REQUEST_MEDIA_CHANGE: return "IMS_CALL_REQUEST_MEDIA_CHANGE";
        case RIL_REQUEST_IMS_CALL_RESPONSE_MEDIA_CHANGE: return "IMS_CALL_RESPONSE_MEDIA_CHANGE";
        case RIL_REQUEST_SET_IMS_SMSC: return "SET_IMS_SMSC";
        case RIL_REQUEST_IMS_CALL_FALL_BACK_TO_VOICE: return "IMS_CALL_FALL_BACK_TO_VOICE";
        case RIL_REQUEST_SET_IMS_INITIAL_ATTACH_APN: return "SET_IMS_INITIAL_ATTACH_APN";
        case RIL_REQUEST_QUERY_CALL_FORWARD_STATUS_URI: return "QUERY_CALL_FORWARD_STATUS_URI";
        case RIL_REQUEST_SET_CALL_FORWARD_URI: return "SET_CALL_FORWARD_URI";
        case RIL_REQUEST_IMS_INITIAL_GROUP_CALL: return "IMS_INITIAL_GROUP_CALL";
        case RIL_REQUEST_IMS_ADD_TO_GROUP_CALL: return "IMS_ADD_TO_GROUP_CALL";
        case RIL_REQUEST_VIDEOPHONE_DIAL: return "VIDEOPHONE_DIAL";
        case RIL_REQUEST_ENABLE_IMS: return "ENABLE_IMS";
        case RIL_REQUEST_DISABLE_IMS: return "DISABLE_IMS";
        case RIL_REQUEST_GET_IMS_BEARER_STATE: return "GET_IMS_BEARER_STATE";
        case RIL_REQUEST_VIDEOPHONE_CODEC: return "VIDEOPHONE_CODEC";
        case RIL_REQUEST_SET_SOS_INITIAL_ATTACH_APN: return "SET_SOS_INITIAL_ATTACH_APN";
        case RIL_REQUEST_IMS_HANDOVER: return "IMS_HANDOVER";
        case RIL_REQUEST_IMS_HANDOVER_STATUS_UPDATE: return "IMS_HANDOVER_STATUS_UPDATE";
        case RIL_REQUEST_IMS_NETWORK_INFO_CHANGE: return "IMS_NETWORK_INFO_CHANGE";
        case RIL_REQUEST_IMS_HANDOVER_CALL_END: return "IMS_HANDOVER_CALL_END";
        case RIL_REQUEST_IMS_WIFI_ENABLE: return "IMS_WIFI_ENABLE";
        case RIL_REQUEST_IMS_WIFI_CALL_STATE_CHANGE: return "IMS_WIFI_CALL_STATE_CHANGE";
        case RIL_REQUEST_IMS_UPDATE_DATA_ROUTER: return "IMS_UPDATE_DATA_ROUTER";
        case RIL_REQUEST_GET_TPMR_STATE: return "GET_TPMR_STATE";
        case RIL_REQUEST_SET_TPMR_STATE: return "SET_TPMR_STATE";
        case RIL_REQUEST_IMS_HOLD_SINGLE_CALL: return "IMS_HOLD_SINGLE_CALL";
        case RIL_REQUEST_IMS_MUTE_SINGLE_CALL: return "IMS_MUTE_SINGLE_CALL";
        case RIL_REQUEST_IMS_SILENCE_SINGLE_CALL: return "IMS_SILENCE_SINGLE_CALL";
        case RIL_REQUEST_IMS_ENABLE_LOCAL_CONFERENCE: return "ENABLE_LOCAL_CONFERENCE";
        case RIL_REQUEST_IMS_NOTIFY_HANDOVER_CALL_INFO: return "IMS_NOTIFY_HANDOVER_CALL_INFO";
        case RIL_REQUEST_GET_IMS_SRVCC_CAPBILITY: return "GET_IMS_SRVCC_CAPBILITY";
        case RIL_REQUEST_GET_IMS_PCSCF_ADDR: return "GET_IMS_PCSCF_ADDR";
        case RIL_REQUEST_SET_VOWIFI_PCSCF_ADDR: return "SET_VOWIFI_PCSCF_ADDR";
        case RIL_REQUEST_IMS_REGADDR: return "IMS_REGADDR";
        case RIL_REQUEST_IMS_UPDATE_CLIP: return "IMS_UPDATE_CLIP";
        /* }@ */
        /* OEM SOCKET REQUEST @{*/
        /* videophone @{ */
        case RIL_EXT_REQUEST_VIDEOPHONE_DIAL: return "VIDEOPHONE_DIAL";
        case RIL_EXT_REQUEST_VIDEOPHONE_CODEC: return "VIDEOPHONE_CODEC";
        case RIL_EXT_REQUEST_VIDEOPHONE_FALLBACK: return "VIDEOPHONE_FALLBACK";
        case RIL_EXT_REQUEST_VIDEOPHONE_STRING: return "VIDEOPHONE_STRING";
        case RIL_EXT_REQUEST_VIDEOPHONE_LOCAL_MEDIA: return "VIDEOPHONE_LOCAL_MEDIA";
        case RIL_EXT_REQUEST_VIDEOPHONE_CONTROL_IFRAME: return "VIDEOPHONE_CONTROL_IFRAME";
        /* }@ */
        case RIL_EXT_REQUEST_SWITCH_MULTI_CALL: return "SWITCH_MULTI_CALL";
        case RIL_EXT_REQUEST_TRAFFIC_CLASS: return "TRAFFIC_CLASS";
        case RIL_EXT_REQUEST_ENABLE_LTE: return "ENABLE_LTE";
        case RIL_EXT_REQUEST_ATTACH_DATA: return "ATTACH_DATA";
        case RIL_EXT_REQUEST_STOP_QUERY_NETWORK: return "STOP_QUERY_NETWORK";
        case RIL_EXT_REQUEST_FORCE_DETACH: return "FORCE_DETACH";
        case RIL_EXT_REQUEST_GET_HD_VOICE_STATE: return "GET_HD_VOICE_STATE";
        case RIL_EXT_REQUEST_SIMMGR_SIM_POWER: return "SIMMGR_SIM_POWER";
        case RIL_EXT_REQUEST_ENABLE_RAU_NOTIFY: return "ENABLE_RAU_NOTIFY";
        case RIL_EXT_REQUEST_SET_COLP: return "SET_COLP";
        case RIL_EXT_REQUEST_GET_DEFAULT_NAN: return "GET_DEFAULT_NAN";
        case RIL_EXT_REQUEST_SIM_GET_ATR: return "SIM_GET_ATR";
        case RIL_EXT_REQUEST_SIM_OPEN_CHANNEL_WITH_P2: return "OPEN_CHANNEL_WITH_P2";
        case RIL_EXT_REQUEST_GET_SIM_CAPACITY: return "GET_SIM_CAPACITY";
        case RIL_EXT_REQUEST_STORE_SMS_TO_SIM: return "STORE_SMS_TO_SIM";
        case RIL_EXT_REQUEST_QUERY_SMS_STORAGE_MODE: return "QUERY_SMS_STORAGE_MODE";
        case RIL_EXT_REQUEST_GET_SIMLOCK_REMAIN_TIMES: return "GET_SIMLOCK_REMAIN_TIMES";
        case RIL_EXT_REQUEST_SET_FACILITY_LOCK_FOR_USER: return "SET_FACILITY_LOCK_FOR_USER";
        case RIL_EXT_REQUEST_GET_SIMLOCK_STATUS: return "GET_SIMLOCK_STATUS";
        case RIL_EXT_REQUEST_GET_SIMLOCK_DUMMYS: return "GET_SIMLOCK_DUMMYS";
        case RIL_EXT_REQUEST_GET_SIMLOCK_WHITE_LIST: return "GET_SIMLOCK_WHITE_LIST";
        case RIL_EXT_REQUEST_UPDATE_ECCLIST: return "UPDATE_ECCLIST";
        case RIL_EXT_REQUEST_GET_BAND_INFO: return "GET_BAND_INFO";
        case RIL_EXT_REQUEST_SET_BAND_INFO_MODE: return "SET_BAND_INFO_MODE";
        case RIL_EXT_REQUEST_SET_SINGLE_PDN: return "SET_SINGLE_PDN";
        case RIL_EXT_REQUEST_SET_SPECIAL_RATCAP: return "SET_SPECIAL_RATCAP";
        case RIL_EXT_REQUEST_QUERY_COLP: return "QUERY_COLP";
        case RIL_EXT_REQUEST_QUERY_COLR: return "QUERY_COLR";
        case RIL_EXT_REQUEST_MMI_ENTER_SIM: return "MMI_ENTER_SIM";
        case RIL_EXT_REQUEST_UPDATE_OPERATOR_NAME: return "UPDATE_OPERATOR_NAME";
        case RIL_EXT_REQUEST_SIMMGR_GET_SIM_STATUS: return "SIMMGR_GET_SIM_STATUS";
        case RIL_EXT_REQUEST_SET_XCAP_IP_ADDR: return "SET_XCAP_IP_ADDR";
        case RIL_EXT_REQUEST_REATTACH: return "REATTACH";
        case RIL_EXT_REQUEST_SET_VOICE_DOMAIN: return "SET_VOICE_DOMAIN";
        case RIL_EXT_REQUEST_SET_LOCAL_TONE: return "SET_LOCAL_TONE";
        case RIL_EXT_REQUEST_SIM_POWER: return "SIM_POWER";
        /* }@ */

        case RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED: return "UNSOL_RESPONSE_RADIO_STATE_CHANGED";
        case RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED: return "UNSOL_RESPONSE_CALL_STATE_CHANGED";
        case RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED: return "UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED";
        case RIL_UNSOL_RESPONSE_NEW_SMS: return "UNSOL_RESPONSE_NEW_SMS";
        case RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT: return "UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT";
        case RIL_UNSOL_RESPONSE_NEW_SMS_ON_SIM: return "UNSOL_RESPONSE_NEW_SMS_ON_SIM";
        case RIL_UNSOL_ON_USSD: return "UNSOL_ON_USSD";
        case RIL_UNSOL_ON_USSD_REQUEST: return "UNSOL_ON_USSD_REQUEST(obsolete)";
        case RIL_UNSOL_NITZ_TIME_RECEIVED: return "UNSOL_NITZ_TIME_RECEIVED";
        case RIL_UNSOL_SIGNAL_STRENGTH: return "UNSOL_SIGNAL_STRENGTH";
        case RIL_UNSOL_SUPP_SVC_NOTIFICATION: return "UNSOL_SUPP_SVC_NOTIFICATION";
        case RIL_UNSOL_STK_SESSION_END: return "UNSOL_STK_SESSION_END";
        case RIL_UNSOL_STK_PROACTIVE_COMMAND: return "UNSOL_STK_PROACTIVE_COMMAND";
        case RIL_UNSOL_STK_EVENT_NOTIFY: return "UNSOL_STK_EVENT_NOTIFY";
        case RIL_UNSOL_STK_CALL_SETUP: return "UNSOL_STK_CALL_SETUP";
        case RIL_UNSOL_SIM_SMS_STORAGE_FULL: return "UNSOL_SIM_SMS_STORAGE_FUL";
        case RIL_UNSOL_SIM_REFRESH: return "UNSOL_SIM_REFRESH";
        case RIL_UNSOL_DATA_CALL_LIST_CHANGED: return "UNSOL_DATA_CALL_LIST_CHANGED";
        case RIL_UNSOL_CALL_RING: return "UNSOL_CALL_RING";
        case RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED: return "UNSOL_RESPONSE_SIM_STATUS_CHANGED";
        case RIL_UNSOL_RESPONSE_CDMA_NEW_SMS: return "UNSOL_NEW_CDMA_SMS";
        case RIL_UNSOL_RESPONSE_NEW_BROADCAST_SMS: return "UNSOL_NEW_BROADCAST_SMS";
        case RIL_UNSOL_CDMA_RUIM_SMS_STORAGE_FULL: return "UNSOL_CDMA_RUIM_SMS_STORAGE_FULL";
        case RIL_UNSOL_RESTRICTED_STATE_CHANGED: return "UNSOL_RESTRICTED_STATE_CHANGED";
        case RIL_UNSOL_ENTER_EMERGENCY_CALLBACK_MODE: return "UNSOL_ENTER_EMERGENCY_CALLBACK_MODE";
        case RIL_UNSOL_CDMA_CALL_WAITING: return "UNSOL_CDMA_CALL_WAITING";
        case RIL_UNSOL_CDMA_OTA_PROVISION_STATUS: return "UNSOL_CDMA_OTA_PROVISION_STATUS";
        case RIL_UNSOL_CDMA_INFO_REC: return "UNSOL_CDMA_INFO_REC";
        case RIL_UNSOL_OEM_HOOK_RAW: return "UNSOL_OEM_HOOK_RAW";
        case RIL_UNSOL_RINGBACK_TONE: return "UNSOL_RINGBACK_TONE";
        case RIL_UNSOL_RESEND_INCALL_MUTE: return "UNSOL_RESEND_INCALL_MUTE";
        case RIL_UNSOL_CDMA_SUBSCRIPTION_SOURCE_CHANGED: return "UNSOL_CDMA_SUBSCRIPTION_SOURCE_CHANGED";
        case RIL_UNSOL_CDMA_PRL_CHANGED: return "UNSOL_CDMA_PRL_CHANGED";
        case RIL_UNSOL_EXIT_EMERGENCY_CALLBACK_MODE: return "UNSOL_EXIT_EMERGENCY_CALLBACK_MODE";
        case RIL_UNSOL_RIL_CONNECTED: return "UNSOL_RIL_CONNECTED";
        case RIL_UNSOL_VOICE_RADIO_TECH_CHANGED: return "UNSOL_VOICE_RADIO_TECH_CHANGED";
        case RIL_UNSOL_CELL_INFO_LIST: return "UNSOL_CELL_INFO_LIST";
        case RIL_UNSOL_RESPONSE_IMS_NETWORK_STATE_CHANGED: return "RESPONSE_IMS_NETWORK_STATE_CHANGED";
        case RIL_UNSOL_UICC_SUBSCRIPTION_STATUS_CHANGED: return "UNSOL_UICC_SUBSCRIPTION_STATUS_CHANGED";
        case RIL_UNSOL_SRVCC_STATE_NOTIFY: return "UNSOL_SRVCC_STATE_NOTIFY";
        case RIL_UNSOL_HARDWARE_CONFIG_CHANGED: return "HARDWARE_CONFIG_CHANGED";
        case RIL_UNSOL_DC_RT_INFO_CHANGED: return "UNSOL_DC_RT_INFO_CHANGED";
        case RIL_UNSOL_RADIO_CAPABILITY: return "UNSOL_RADIO_CAPABILITY";
        case RIL_RESPONSE_ACKNOWLEDGEMENT: return "RIL_RESPONSE_ACKNOWLEDGEMENT";
        /* IMS unsolicited response @{ */
        case RIL_UNSOL_RESPONSE_IMS_CALL_STATE_CHANGED: return "UNSOL_IMS_CALL_STATE_CHANGED";
        case RIL_UNSOL_RESPONSE_VIDEO_QUALITY: return "UNSOL_VIDEO_QUALITY";
        case RIL_UNSOL_RESPONSE_IMS_BEARER_ESTABLISTED: return "UNSOL_RESPONSE_IMS_BEARER_ESTABLISTED";
        case RIL_UNSOL_IMS_HANDOVER_REQUEST: return "UNSOL_IMS_HANDOVER_REQUEST";
        case RIL_UNSOL_IMS_HANDOVER_STATUS_CHANGE: return "UNSOL_IMS_HANDOVER_STATUS_CHANGE";
        case RIL_UNSOL_IMS_NETWORK_INFO_CHANGE: return "UNSOL_IMS_NETWORK_INFO_CHANGE";
        case RIL_UNSOL_IMS_REGISTER_ADDRESS_CHANGE: return "UNSOL_IMS_REGISTER_ADDRESS_CHANGE";
        case RIL_UNSOL_IMS_WIFI_PARAM: return "UNSOL_IMS_WIFI_PARAM";
        case RIL_UNSOL_IMS_NETWORK_STATE_CHANGED: return "UNSOL_IMS_NETWORK_STATE_CHANGED";
        /* }@ */
        /* videophone @{ */
        case RIL_EXT_UNSOL_VIDEOPHONE_CODEC: return "UNSOL_VIDEOPHONE_CODEC";
        case RIL_EXT_UNSOL_VIDEOPHONE_DSCI: return "UNSOL_VIDEOPHONE_DSCI";
        case RIL_EXT_UNSOL_VIDEOPHONE_STRING: return "UNSOL_VIDEOPHONE_STRING";
        case RIL_EXT_UNSOL_VIDEOPHONE_REMOTE_MEDIA: return "UNSOL_VIDEOPHONE_REMOTE_MEDIA";
        case RIL_EXT_UNSOL_VIDEOPHONE_MM_RING: return "UNSOL_VIDEOPHONE_MM_RING";
        case RIL_EXT_UNSOL_VIDEOPHONE_RELEASING: return "UNSOL_VIDEOPHONE_RELEASING";
        case RIL_EXT_UNSOL_VIDEOPHONE_RECORD_VIDEO: return "UNSOL_VIDEOPHONE_RECORD_VIDEO";
        case RIL_EXT_UNSOL_VIDEOPHONE_MEDIA_START: return "UNSOL_VIDEOPHONE_MEDIA_START";
        /* }@ */
        case RIL_EXT_UNSOL_ECC_NETWORKLIST_CHANGED: return "UNSOL_ECC_NETWORKLIST_CHANGED";
        case RIL_EXT_UNSOL_RAU_SUCCESS: return "UNSOL_RAU_SUCCESS";
        case RIL_EXT_UNSOL_CLEAR_CODE_FALLBACK: return "UNSOL_CLEAR_CODE_FALLBACK";
        case RIL_EXT_UNSOL_RIL_CONNECTED: return "UNSOL_RIL_CONNECTED";
        case RIL_EXT_UNSOL_SIMLOCK_STATUS_CHANGED: return "UNSOL_SIMLOCK_STATUS_CHANGED";
        case RIL_EXT_UNSOL_SIMLOCK_SIM_EXPIRED: return "UNSOL_SIMLOCK_SIM_EXPIRED";
        case RIL_EXT_UNSOL_BAND_INFO: return "UNSOL_BAND_INFO";
        case RIL_EXT_UNSOL_SWITCH_PRIMARY_CARD: return "UNSOL_SWITCH_PRIMARY_CARD";
        case RIL_EXT_UNSOL_SIM_PS_REJECT: return "UNSOL_SIM_PS_REJECT";
        case RIL_EXT_UNSOL_SETUP_DATA_FOR_CP: return "UNSOL_SETUP_DATA_FOR_CP";
        case RIL_EXT_UNSOL_SIMMGR_SIM_STATUS_CHANGED: return "UNSOL_SIMMGR_SIM_STATUS_CHANGED";
        case RIL_EXT_UNSOL_RADIO_CAPABILITY_CHANGED: return "UNSOL_RADIO_CAPABILITY_CHANGED";
        case RIL_EXT_UNSOL_EARLY_MEDIA: return "UNSOL_EARLY_MEDIA";
        default: return "<unknown request>";
    }
}

const char *rilSocketIdToString(RIL_SOCKET_ID socket_id) {
    switch (socket_id) {
        case RIL_SOCKET_1:
            return "RIL_SOCKET_1";
#if (SIM_COUNT >= 2)
        case RIL_SOCKET_2:
            return "RIL_SOCKET_2";
#endif
#if (SIM_COUNT >= 3)
        case RIL_SOCKET_3:
            return "RIL_SOCKET_3";
#endif
#if (SIM_COUNT >= 4)
        case RIL_SOCKET_4:
            return "RIL_SOCKET_4";
#endif
        default:
            return "not a valid RIL";
    }
}

static void listenCallbackEXT(int fd, short flags __unused, void *param) {
    int ret;
    int fdCommand;
    RecordStream *p_rs;
    bool persist = false;

    SocketListenParam *p_info = (SocketListenParam *)param;

    assert(fd == p_info->fdListen);

    struct sockaddr_un peeraddr;
    socklen_t socklen = sizeof(peeraddr);

    fdCommand = accept(fd, (sockaddr *)&peeraddr, &socklen);

    if (fdCommand < 0) {
        RLOGE("[ATCI] Error on accept() errno:%d", errno);
        return;
    }

    ret = fcntl(fdCommand, F_SETFL, O_NONBLOCK);

    if (ret < 0) {
        RLOGE("[ATCI] Error setting O_NONBLOCK errno:%d", errno);
    }

    RLOGI("libril: new connection to %s",
            (p_info->type == RIL_ATCI_SOCKET)? "ATCI" :
                    ((p_info->type == RIL_IMS_SOCKET)? "IMS" : "OEM"));

    p_info->fdCommand = fdCommand;
    p_rs = record_stream_new(p_info->fdCommand, MAX_COMMAND_BYTES);
    p_info->p_rs = p_rs;

    if (p_info->type == RIL_ATCI_SOCKET) {
        /* note: we can only send one AT command at a connection */
        persist = false;
    } else if (p_info->type == RIL_IMS_SOCKET || p_info->type == RIL_OEM_SOCKET) {
        persist = true;
    }
    ril_event_set(p_info->commands_event, p_info->fdCommand, persist,
            p_info->processCommandsCallback, p_info);
    rilEventAddWakeup(p_info->commands_event);

    if (p_info->type == RIL_OEM_SOCKET) {
        // Inform oem socket that modem maybe assert or reset
        RIL_UNSOL_RESPONSE(RIL_EXT_UNSOL_RIL_CONNECTED, NULL, 0,
                           p_info->socket_id);
        RIL_UNSOL_RESPONSE(RIL_EXT_UNSOL_SIMMGR_SIM_STATUS_CHANGED,
                           NULL, 0, p_info->socket_id);
    }
}

void list_init(ListNode **node) {
    *node = (ListNode *)malloc(sizeof(ListNode));
    if (*node == NULL) {
        RLOGE("Failed malloc memory!");
    } else {
        (*node)->next = *node;
        (*node)->prev = *node;
    }
}

void list_add_tail(ListNode *head, ListNode *item, RIL_SOCKET_ID socket_id) {
    pthread_mutex_lock(&s_requestThread[socket_id].listMutex);
    item->next = head;
    item->prev = head->prev;
    head->prev->next = item;
    head->prev = item;
    pthread_mutex_unlock(&s_requestThread[socket_id].listMutex);
}

void list_add_head(ListNode *head, ListNode *item, RIL_SOCKET_ID socket_id) {
    pthread_mutex_lock(&s_requestThread[socket_id].listMutex);
    item->next = head->next;
    item->prev = head;
    head->next->prev = item;
    head->next = item;
    pthread_mutex_unlock(&s_requestThread[socket_id].listMutex);
}

void list_remove(ListNode *item, RIL_SOCKET_ID socket_id) {
    pthread_mutex_lock(&s_requestThread[socket_id].listMutex);
    item->next->prev = item->prev;
    item->prev->next = item->next;
    pthread_mutex_unlock(&s_requestThread[socket_id].listMutex);
}

/*
 * Returns true for a debuggable build.
 */
static bool isDebuggable() {
    char debuggable[PROP_VALUE_MAX];
    property_get("ro.debuggable", debuggable, "0");
    if (strcmp(debuggable, "1") == 0) {
        return true;
    }
    return false;
}

void getProperty(RIL_SOCKET_ID socket_id, const char *property, char *value,
                   const char *defaultVal) {
    int simId = 0;
    char prop[PROPERTY_VALUE_MAX];
    int len = property_get(property, prop, "");
    char *p[RIL_SOCKET_NUM];
    char *buf = prop;
    char *ptr = NULL;
    RLOGD("get sim%d [%s] property: %s", socket_id, property, prop);

    if (value == NULL) {
        RLOGE("The memory to save prop is NULL!");
        return;
    }

    memset(p, 0, RIL_SOCKET_NUM * sizeof(char *));
    if (len > 0) {
        for (simId = 0; simId < RIL_SOCKET_NUM; simId++) {
            ptr = strsep(&buf, ",");
            p[simId] = ptr;
        }

        if (socket_id >= RIL_SOCKET_1 && socket_id < RIL_SOCKET_NUM &&
                (p[socket_id] != NULL) && strcmp(p[socket_id], "")) {
            memcpy(value, p[socket_id], strlen(p[socket_id]) + 1);
            return;
        }
    }

    if (defaultVal != NULL) {
        len = strlen(defaultVal);
        memcpy(value, defaultVal, len);
        value[len] = '\0';
    }
}

void setProperty(RIL_SOCKET_ID socket_id, const char *property,
                   const char *value) {
    char prop[PROPERTY_VALUE_MAX];
    char propVal[PROPERTY_VALUE_MAX];
    int len = property_get(property, prop, "");
    int i, simId = 0;
    char *p[RIL_SOCKET_NUM];
    char *buf = prop;
    char *ptr = NULL;

    if (socket_id < RIL_SOCKET_1 || socket_id >= RIL_SOCKET_NUM) {
        RLOGE("setProperty: invalid socket id = %d, property = %s",
                socket_id, property);
        return;
    }

    RLOGD("set sim%d [%s] property: %s", socket_id, property, value);
    memset(p, 0, RIL_SOCKET_NUM * sizeof(char *));
    if (len > 0) {
        for (simId = 0; simId < RIL_SOCKET_NUM; simId++) {
            ptr = strsep(&buf, ",");
            p[simId] = ptr;
        }
    }

    memset(propVal, 0, sizeof(propVal));
    for (i = 0; i < (int)socket_id; i++) {
        if (p[i] != NULL) {
            strncat(propVal, p[i], strlen(p[i]));
        }
        strncat(propVal, ",", 1);
    }

    if (value != NULL) {
        strncat(propVal, value, strlen(value));
    }

    for (i = socket_id + 1; i < RIL_SOCKET_NUM; i++) {
        strncat(propVal, ",", 1);
        if (p[i] != NULL) {
            strncat(propVal, p[i], strlen(p[i]));
        }
    }

    property_set(property, propVal);
}

// for L+W product
bool isPrimaryCardWorkMode(int workMode) {
    if (workMode == GSM_ONLY || workMode == WCDMA_ONLY ||
        workMode == WCDMA_AND_GSM || workMode == TD_AND_WCDMA ||
        workMode == NONE) {
        return false;
    }
    return true;
}

void initPrimarySim() {
    char prop[PROPERTY_VALUE_MAX];
    char numToStr[128];
    int simId;

    property_get(PRIMARY_SIM_PROP, prop, "0");
    s_multiModeSim = atoi(prop);
    RLOGD("before initPrimarySim: s_multiModeSim = %d", s_multiModeSim);
#if (SIM_COUNT == 2)
    property_get(MODEM_CONFIG_PROP, prop, "");
    if (strcmp(prop, "TL_LF_TD_W_G,W_G") && strcmp(prop, "TL_LF_W_G,W_G") &&
        strcmp(prop, "TL_LF_TD_W_G,TL_LF_TD_W_G") && strcmp(prop, "TL_LF_W_G,TL_LF_W_G")) {
        for (simId = 0; simId < SIM_COUNT; simId++) {
            getProperty((RIL_SOCKET_ID)simId, MODEM_WORKMODE_PROP, prop, "");
            if (strcmp(prop, "10") == 0 && s_multiModeSim == simId) {
                s_multiModeSim = 1 - simId;
                snprintf(numToStr, sizeof(numToStr), "%d", s_multiModeSim);
                property_set(PRIMARY_SIM_PROP, numToStr);
            }
        }
    } else if (!strcmp(prop, "TL_LF_TD_W_G,W_G") || !strcmp(prop, "TL_LF_W_G,W_G")) {
        for (simId = 0; simId < SIM_COUNT; simId++) {
            getProperty((RIL_SOCKET_ID)simId, MODEM_WORKMODE_PROP, prop, "");
            if (!isPrimaryCardWorkMode(atoi(prop)) && s_multiModeSim == simId) {
                s_multiModeSim = 1 - simId;
                snprintf(numToStr, sizeof(numToStr), "%d", s_multiModeSim);
                property_set(PRIMARY_SIM_PROP, numToStr);
            }
        }
    }
#endif
    RLOGD("after initPrimarySim : s_multiModeSim = %d", s_multiModeSim);
}
} /* namespace android */

void rilEventAddWakeup_helper(struct ril_event *ev) {
    android::rilEventAddWakeup(ev);
}

void listenCallback_helper(int fd, short flags, void *param) {
    android::listenCallback(fd, flags, param);
}

int blockingWrite_helper(int fd, void *buffer, size_t len) {
    return android::blockingWrite(fd, buffer, len);
}
