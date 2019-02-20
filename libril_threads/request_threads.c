/**
 * request_threads.c --- ril multi threads process implementation
 *
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 */

#define LOG_TAG "REQ_THDS"

#include <stdio.h>
#include <string.h>
#include <alloca.h>
#include <semaphore.h>
#include <errno.h>
#include <utils/Log.h>
#include <unistd.h>

#include "telephony/ril.h"
#include "telephony/thread_pool.h"
#include "request_threads.h"
#include <cutils/properties.h>

int s_simCount = 1;
int s_threadNumber = 0;
int s_maxChannelNum = 0;
int s_channelOpen[4];

pthread_mutex_t s_waitArrayMutex = PTHREAD_MUTEX_INITIALIZER;
SemLockInfo *s_semLockArray = NULL;
ChannelInfo *s_channelInfo = NULL;
WaitArrayParam *s_dataWaitArray = NULL;
WaitArrayParam **s_slowWaitArray = NULL;
WaitArrayParam **s_normalWaitArray = NULL;

// ChannelInfo s_channelInfo[MAX_AT_CHANNELS];
// WaitArrayParam s_slowWaitArray[SIM_COUNT][THREAD_NUMBER];
// WaitArrayParam s_normalWaitArray[SIM_COUNT][THREAD_NUMBER];
// WaitArrayParam s_dataWaitArray[THREAD_NUMBER];

int s_socketId[4] = {0, 1, 2, 3};
RequestThreadInfo *s_requestThread = NULL;
RequestListNode *s_dataReqList = NULL;
pthread_mutex_t s_dataDispatchMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t s_dataDispatchCond  = PTHREAD_COND_INITIALIZER;

void setChannelInfo(int fd, int channelID);
void setChannelOpened(RIL_SOCKET_ID socket_id);
void putChannel(int channelID);
void enqueueRequest(int request, void *data, size_t datalen, RIL_Token t,
                    RIL_SOCKET_ID socket_id);
void *noopRemoveWarning(void *a);
int getChannel(RIL_SOCKET_ID socket_id);
RIL_SOCKET_ID getSocketIdByChannelID(int channelID);
bool isLLVersion();

const RIL_RequestFunctions *s_requestFunctions = NULL;
#define processRequest(request, data, datalen, t, socket_id) \
        s_requestFunctions->processRequest(request, data, datalen, t, socket_id)
#define RIL_UNUSED_PARM(a)  noopRemoveWarning((void *)&(a));

static const RIL_TheadsFunctions s_threadsFunctions = {
    setChannelInfo,
    setChannelOpened,
    getChannel,
    putChannel,
    getSocketIdByChannelID,
    enqueueRequest
};

void *noopRemoveWarning(void *a) {
    return a;
}

void request_list_init(RequestListNode **node) {
    *node = (RequestListNode *)malloc(sizeof(RequestListNode));
    if (*node == NULL) {
        RLOGE("Failed malloc memory!");
    } else {
        (*node)->next = *node;
        (*node)->prev = *node;
    }
}

void request_list_add_tail(RequestListNode *head, RequestListNode *item,
                           RIL_SOCKET_ID socket_id) {
    pthread_mutex_lock(&s_requestThread[socket_id].listMutex);
    item->next = head;
    item->prev = head->prev;
    head->prev->next = item;
    head->prev = item;
    pthread_mutex_unlock(&s_requestThread[socket_id].listMutex);
}

void request_list_add_head(RequestListNode *head, RequestListNode *item,
                           RIL_SOCKET_ID socket_id) {
    pthread_mutex_lock(&s_requestThread[socket_id].listMutex);
    item->next = head->next;
    item->prev = head;
    head->next->prev = item;
    head->next = item;
    pthread_mutex_unlock(&s_requestThread[socket_id].listMutex);
}

void request_list_remove(RequestListNode *item, RIL_SOCKET_ID socket_id) {
    pthread_mutex_lock(&s_requestThread[socket_id].listMutex);
    item->next->prev = item->prev;
    item->prev->next = item->next;
    pthread_mutex_unlock(&s_requestThread[socket_id].listMutex);
}

void setChannelInfo(int fd, int channelID) {
    s_channelInfo[channelID].fd = fd;
    s_channelInfo[channelID].state = CHANNEL_IDLE;
    s_channelInfo[channelID].channelID = channelID;
    snprintf(s_channelInfo[channelID].name,
            sizeof(s_channelInfo[channelID].name), "Channel%d", channelID);
}

void setChannelOpened(RIL_SOCKET_ID socket_id) {
    s_channelOpen[socket_id] = 1;
}

/* release Channel */
void putChannel(int channelID) {
    if (channelID < 1 || channelID >= s_maxChannelNum) {
        return;
    }

    ChannelInfo *p_channelInfo = s_channelInfo;

    pthread_mutex_lock(&p_channelInfo[channelID].mutex);
    if (p_channelInfo[channelID].state != CHANNEL_BUSY) {
        goto done;
    }
    p_channelInfo[channelID].state = CHANNEL_IDLE;

done:
    RLOGD("put Channel%d", p_channelInfo[channelID].channelID);
    pthread_mutex_unlock(&p_channelInfo[channelID].mutex);
}

/* Return channel ID */
int getChannel(RIL_SOCKET_ID socket_id) {
    if ((int)socket_id < 0 || (int)socket_id >= s_simCount) {
        RLOGE("getChannel: invalid socket_id %d", socket_id);
        return -1;
    }

    int ret = 0;
    int channelID = -1;
    int firstChannel = -1;
    int lastChannel = -1;
    ChannelInfo *p_channelInfo = s_channelInfo;

    if (s_simCount >= 2) {
        firstChannel = socket_id * D_AT_CHANNEL_OFFSET + D_AT_CHANNEL_1;
        lastChannel = (socket_id + 1) * D_AT_CHANNEL_OFFSET;
    } else {
        firstChannel = S_AT_CHANNEL_1;
        lastChannel = S_MAX_AT_CHANNELS;
    }

    for (;;) {
        if (!s_channelOpen[socket_id]) {
            sleep(1);
            continue;
        }
        for (channelID = firstChannel; channelID < lastChannel; channelID++) {
            pthread_mutex_lock(&p_channelInfo[channelID].mutex);
            if (p_channelInfo[channelID].state == CHANNEL_IDLE) {
                p_channelInfo[channelID].state = CHANNEL_BUSY;
                RLOGD("get Channel%d", p_channelInfo[channelID].channelID);
                pthread_mutex_unlock(&p_channelInfo[channelID].mutex);
                return channelID;
            }
            pthread_mutex_unlock(&p_channelInfo[channelID].mutex);
        }
        usleep(5000);
    }
}

RIL_SOCKET_ID getSocketIdByChannelID(int channelID) {
    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    if (s_simCount >= 2) {
        if (channelID >= D_AT_URC2 && channelID <= D_AT_CHANNEL2_2) {
            socket_id = 1;  // RIL_SOCKET_2
        }
    }
    return socket_id;
}

#if 0
int acquireSemLock() {
    int semLockID = 0;

    for (;;) {
        for (semLockID = 0; semLockID < s_threadNumber; semLockID++) {
            if (pthread_mutex_trylock(&s_semLockArray[semLockID].semLockMutex) == 0) {
                return semLockID;
            }
        }
        usleep(5000);
    }
}

void releaseSemLock(int semLockID) {
    if (semLockID < 0 || semLockID >= s_threadNumber) {
        RLOGE("Invalid semLockID to release");
        return;
    }

    pthread_mutex_unlock(&s_semLockArray[semLockID].semLockMutex);
}

WaitArrayParam *getWaitArray(RIL_SOCKET_ID socket_id, ATCmdType cmdType) {
    WaitArrayParam *pWaitArray = NULL;
    switch (cmdType) {
        case AT_CMD_TYPE_SLOW : {
            pWaitArray = s_slowWaitArray[socket_id];
            break;
        }
        case AT_CMD_TYPE_NORMAL_SLOW :
        case AT_CMD_TYPE_NORMAL_FAST : {
            pWaitArray = s_normalWaitArray[socket_id];
            break;
        }
        case AT_CMD_TYPE_DATA : {
            pWaitArray = s_dataWaitArray;
            break;
        }
        default :
            break;
    }
    return pWaitArray;
}

int addWaitArray(int semLockId, ATCmdType cmdType, WaitArrayParam *pWaitArray) {
    int index = -1;

    if (pWaitArray == NULL) {
        RLOGE("Invalid waitArray address");
        return index;
    }

    int i = 0;
    if (cmdType == AT_CMD_TYPE_NORMAL_FAST) {
        for (i = 0; i < s_threadNumber; i++) {
            if (-1 == pWaitArray[i].semLockID ||
               (i != 0 && pWaitArray[i].cmdType == AT_CMD_TYPE_NORMAL_SLOW)) {
                int j = 0;
                for (j = s_threadNumber - 2; j >= i; j--) {
                    if (pWaitArray[j].semLockID != -1) {
                        pWaitArray[j + 1] = pWaitArray[j];
                    }
                }
                pWaitArray[i].semLockID = semLockId;
                pWaitArray[i].cmdType = cmdType;
                index = i;
                break;
            }
        }
    } else {
        for (i = 0; i < s_threadNumber; i++) {
            if (-1 == pWaitArray[i].semLockID) {
                pWaitArray[i].semLockID = semLockId;
                pWaitArray[i].cmdType = cmdType;
                index = i;
                break;
            }
        }
    }

    return index;
}

void removeWaitArray(WaitArrayParam *pWaitArray) {
    int i = 0;
    for (i = 0; i < (s_threadNumber - 1); i++) {
        pWaitArray[i] = pWaitArray[i + 1];
    }
    pWaitArray[s_threadNumber - 1].semLockID = -1;
    pWaitArray[s_threadNumber - 1].cmdType = AT_CMD_TYPE_UNKOWN;
}


const char *typeToString(ATCmdType cmdType) {
    switch (cmdType) {
        case AT_CMD_TYPE_UNKOWN : return "UNKOWN";
        case AT_CMD_TYPE_SLOW : return "SLOW";
        case AT_CMD_TYPE_NORMAL_SLOW: return "NORMAL_SLOW";
        case AT_CMD_TYPE_NORMAL_FAST: return "NORMAL_FAST";
        case AT_CMD_TYPE_DATA: return "DATA";
        case AT_CMD_TYPE_OTHER: return "OTHER";
        default: return "unkown";
    }
}

void printWaitArray(WaitArrayParam *pWaitArray, RIL_SOCKET_ID socket_id) {
    int i = 0;
    for (i = 0; i < s_threadNumber; i++) {
        if (pWaitArray[i].semLockID != -1) {
            if (pWaitArray == s_slowWaitArray[socket_id]
             || pWaitArray == s_normalWaitArray[socket_id]) {
            RLOGD("%sWaitArray[%d][%d].semLockID = %d, type = %s ",
                    pWaitArray == s_slowWaitArray[socket_id] ? "slow": "normal",
                    socket_id, i, pWaitArray[i].semLockID, typeToString(pWaitArray[i].cmdType));
            } else {
                RLOGD("dataWaitArray[%d].semLockID = %d, type = %s ",
                        i, pWaitArray[i].semLockID, typeToString(pWaitArray[i].cmdType));
            }
        }
    }
}

int getRequestChannel(RIL_SOCKET_ID socket_id, int request) {
    int channelID = -1;

    ATCmdType cmdType = AT_CMD_TYPE_OTHER;

    cmdType = getCmdType(request);
    RLOGD("getRequestChannel-sim%d: request = %s, type = %s",
            socket_id, requestToString(request), typeToString(cmdType));
    if (cmdType == AT_CMD_TYPE_OTHER) {  // no blocking
        channelID = getChannel(socket_id);
    } else {  // blocking
        int index = -1;
        WaitArrayParam *pWaitArray = NULL;
        int semLockID = acquireSemLock();
        RLOGD("getRequestChannel: getSemLock %d", semLockID);

        pthread_mutex_lock(&s_waitArrayMutex);
        pWaitArray = getWaitArray(socket_id, cmdType);
        printWaitArray(pWaitArray, socket_id);
        index = addWaitArray(semLockID, cmdType, pWaitArray);
        RLOGD("after addWaitArray");
        printWaitArray(pWaitArray, socket_id);
        pthread_mutex_unlock(&s_waitArrayMutex);
        if (index == 0) {
            channelID = getChannel(socket_id);
        } else if (index > 0 || index < s_threadNumber) {
            RLOGD("getRequestChannel:before sem_wait semLockId = %d", semLockID);
            sem_wait(&s_semLockArray[semLockID].semLock);
            RLOGD("getRequestChannel:after sem_wait semLockId = %d", semLockID);
            channelID = getChannel(socket_id);
        } else {
            RLOGE("Invalid waitArray index");
        }
    }

    return channelID;
}

void putRequestChannel(RIL_SOCKET_ID socket_id, int request, int channelID) {
    ATCmdType cmdType = AT_CMD_TYPE_OTHER;
    WaitArrayParam *pWaitArray = NULL;
    int semLockID = -1;

    putChannel(channelID);

    cmdType = getCmdType(request);
    RLOGD("putRequestChannel-sim%d: request = %s, type = %s",
            socket_id, requestToString(request), typeToString(cmdType));

    if (cmdType != AT_CMD_TYPE_OTHER) {
        pthread_mutex_lock(&s_waitArrayMutex);
        pWaitArray = getWaitArray(socket_id, cmdType);
        semLockID = pWaitArray[0].semLockID;
        releaseSemLock(semLockID);
        printWaitArray(pWaitArray, socket_id);
        removeWaitArray(pWaitArray);
        RLOGD("after removeWaitArray");
        printWaitArray(pWaitArray, socket_id);

        semLockID = pWaitArray[0].semLockID;
        RLOGD("putRequestChannel, socket_id = %d, semLockID = %d", socket_id, semLockID);
        if (semLockID != -1) {
            sem_post(&s_semLockArray[semLockID].semLock);
        }
        pthread_mutex_unlock(&s_waitArrayMutex);
    }
}
#endif

ATCmdType getCmdType(int request) {
    ATCmdType cmdType = AT_CMD_TYPE_OTHER;
    if (request == RIL_REQUEST_SEND_SMS
        || request == RIL_REQUEST_SEND_SMS_EXPECT_MORE
        || request == RIL_REQUEST_IMS_SEND_SMS
        || request == RIL_REQUEST_QUERY_FACILITY_LOCK
        || request == RIL_REQUEST_SET_FACILITY_LOCK
        || request == RIL_REQUEST_QUERY_CALL_FORWARD_STATUS
        || request == RIL_REQUEST_QUERY_CALL_FORWARD_STATUS_URI
        || request == RIL_REQUEST_SET_CALL_FORWARD
        || request == RIL_REQUEST_SET_CALL_FORWARD_URI
        || request == RIL_REQUEST_GET_CLIR
        || request == RIL_REQUEST_SET_CLIR
        || request == RIL_REQUEST_QUERY_CALL_WAITING
        || request == RIL_REQUEST_SET_CALL_WAITING
        || request == RIL_REQUEST_QUERY_CLIP
        || request == RIL_EXT_REQUEST_GET_CNAP) {
        cmdType = AT_CMD_TYPE_SLOW;
    } else if (request == RIL_REQUEST_RADIO_POWER
            || request == RIL_REQUEST_DIAL
            || request == RIL_REQUEST_DTMF
            || request == RIL_REQUEST_DTMF_START
            || request == RIL_REQUEST_DTMF_STOP
            || request == RIL_REQUEST_HANGUP
            || request == RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND
            || request == RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND
            || request == RIL_REQUEST_ANSWER
            || request == RIL_REQUEST_GET_CURRENT_CALLS
            || request == RIL_REQUEST_CONFERENCE
            || request == RIL_REQUEST_UDUB
            || request == RIL_REQUEST_SEPARATE_CONNECTION
            || request == RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE
            || request == RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE
            || request == RIL_REQUEST_SET_RADIO_CAPABILITY
            || request == RIL_REQUEST_SEND_DEVICE_STATE
            /* IMS @{ */
            || request == RIL_REQUEST_IMS_CALL_RESPONSE_MEDIA_CHANGE
            || request == RIL_REQUEST_IMS_CALL_REQUEST_MEDIA_CHANGE
            || request == RIL_REQUEST_IMS_CALL_FALL_BACK_TO_VOICE
            || request == RIL_REQUEST_IMS_INITIAL_GROUP_CALL
            || request == RIL_REQUEST_IMS_ADD_TO_GROUP_CALL
            || request == RIL_REQUEST_SET_IMS_VOICE_CALL_AVAILABILITY
            || request == RIL_REQUEST_GET_IMS_VOICE_CALL_AVAILABILITY
            || request == RIL_REQUEST_GET_IMS_CURRENT_CALLS
            /* }@ */
            /* OEM SOCKET REQUEST @{ */
            || request == RIL_EXT_REQUEST_VIDEOPHONE_DIAL
            || request == RIL_EXT_REQUEST_SWITCH_MULTI_CALL
            || request == RIL_EXT_REQUEST_GET_HD_VOICE_STATE
            || request == RIL_EXT_REQUEST_SIMMGR_SIM_POWER
            || request == RIL_EXT_REQUEST_GET_RADIO_PREFERENCE
            || request == RIL_EXT_REQUEST_SET_RADIO_PREFERENCE
            /* }@ */
            ) {
        cmdType = AT_CMD_TYPE_NORMAL_FAST;
    } else if (request == RIL_REQUEST_SIM_IO
            || request == RIL_REQUEST_WRITE_SMS_TO_SIM
            || request == RIL_REQUEST_DELETE_SMS_ON_SIM
            || request == RIL_REQUEST_GET_SMSC_ADDRESS) {
        cmdType = AT_CMD_TYPE_NORMAL_SLOW;
    } else if (request == RIL_REQUEST_ALLOW_DATA
            || request == RIL_REQUEST_SETUP_DATA_CALL) {
        if (s_simCount == 1) {
            cmdType = AT_CMD_TYPE_SLOW;
        } else {
            cmdType = AT_CMD_TYPE_DATA;
        }
    } else if (isLLVersion() && s_simCount > 1
            && request == RIL_REQUEST_DEACTIVATE_DATA_CALL) {
        cmdType = AT_CMD_TYPE_DATA;
    }
    return cmdType;
}

static void *slowDispatch(void *param) {
    RequestListNode *cmd_item;
    pid_t tid = gettid();

    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    pthread_mutex_t *slowDispatchMutex =
            &(s_requestThread[socket_id].slowDispatchMutex);
    pthread_cond_t *slowDispatchCond =
            &(s_requestThread[socket_id].slowDispatchCond);
    RequestListNode *slow_list = s_requestThread[socket_id].slowReqList;

    while (1) {
        pthread_mutex_lock(slowDispatchMutex);
        if (slow_list->next == slow_list) {
            pthread_cond_wait(slowDispatchCond, slowDispatchMutex);
        }
        pthread_mutex_unlock(slowDispatchMutex);

        for (cmd_item = slow_list->next; cmd_item != slow_list;
                cmd_item = slow_list->next) {
            RequestInfo *p_reqInfo = cmd_item->p_reqInfo;
            processRequest(p_reqInfo->request, p_reqInfo->data,
                    p_reqInfo->datalen, p_reqInfo->token, p_reqInfo->socket_id);
            RLOGI("-->slowDispatch [%d] free one command", tid);
            request_list_remove(cmd_item, socket_id);  /* remove list node first, then free it */
            free(cmd_item->p_reqInfo);
            free(cmd_item);
        }
    }
}

static void *normalDispatch(void *param) {
    RequestListNode *cmd_item = NULL;
    pid_t tid = gettid();

    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    pthread_mutex_t *normalDispatchMutex =
            &(s_requestThread[socket_id].normalDispatchMutex);
    pthread_cond_t *normalDispatchCond =
            &(s_requestThread[socket_id].normalDispatchCond);
    RequestListNode *call_list = s_requestThread[socket_id].callReqList;
    RequestListNode *sim_list = s_requestThread[socket_id].simReqList;

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
            RequestInfo *p_reqInfo = cmd_item->p_reqInfo;
            processRequest(p_reqInfo->request, p_reqInfo->data,
                    p_reqInfo->datalen, p_reqInfo->token, p_reqInfo->socket_id);
            RLOGI("-->normalDispatch [%d] free one command", tid);
            request_list_remove(cmd_item, socket_id);
            free(cmd_item->p_reqInfo);
            free(cmd_item);
        }
    }
}

static void *dataDispatch(void *param) {
    RIL_UNUSED_PARM(param);

    RequestListNode *cmd_item = NULL;
    pid_t tid = gettid();

    pthread_mutex_t *dataDispatchMutex = &s_dataDispatchMutex;
    pthread_cond_t *dataDispatchCond = &s_dataDispatchCond;
    RequestListNode *dataList = s_dataReqList;

    while (1) {
        pthread_mutex_lock(dataDispatchMutex);
        if (dataList->next == dataList) {
            pthread_cond_wait(dataDispatchCond, dataDispatchMutex);
        }
        pthread_mutex_unlock(dataDispatchMutex);

        for (cmd_item = dataList->next; cmd_item != dataList;
                cmd_item = dataList->next) {
            RequestInfo *p_reqInfo = cmd_item->p_reqInfo;
            processRequest(p_reqInfo->request, p_reqInfo->data,
                    p_reqInfo->datalen, p_reqInfo->token, p_reqInfo->socket_id);
            RLOGI("-->dataDispatch [%d] free one command", tid);
            request_list_remove(cmd_item, RIL_SOCKET_1);  /* remove list node first, then free it */
            free(cmd_item->p_reqInfo);
            free(cmd_item);
        }
    }
}

static void CommandThread(void *arg, void *socket_id) {
    pid_t tid = gettid();
    RequestInfo *p_reqInfo = (RequestInfo *)arg;
    RIL_SOCKET_ID soc_id = *((RIL_SOCKET_ID *)socket_id);
    processRequest(p_reqInfo->request, p_reqInfo->data,
            p_reqInfo->datalen, p_reqInfo->token, p_reqInfo->socket_id);
    RLOGI("-->CommandThread [%d] free one command", tid);
    free(p_reqInfo);
}

static void *otherDispatch(void *param) {
    int ret = -1;
    pid_t tid = gettid();
    RequestListNode *cmd_item = NULL;

    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    pthread_mutex_t *otherDispatchMutex =
            &(s_requestThread[socket_id].otherDispatchMutex);
    pthread_cond_t *otherDispatchCond =
            &(s_requestThread[socket_id].otherDispatchCond);
    RequestListNode *other_list = s_requestThread[socket_id].otherReqList;
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
                        cmd_item->p_reqInfo, (void *)&s_socketId[socket_id]);
                if (!ret) {
                    RLOGE("dispatch a new thread unsuccess");
                    sleep(1);
                }
            } while (!ret);
            request_list_remove(cmd_item, socket_id);  /* remove listnode first, then free it */
            free(cmd_item);
        }
    }
}

void enqueueRequest(int request, void *data, size_t datalen, RIL_Token t,
                    RIL_SOCKET_ID socket_id) {
    RequestInfo *p_reqInfo = NULL;
    RequestListNode *p_reqNode = NULL;
    RequestThreadInfo *cmdList = &s_requestThread[socket_id];

    p_reqNode = (RequestListNode *)calloc(1, sizeof(RequestListNode));
    if (p_reqNode == NULL) {
        RLOGE("Failed to allocate memory for cmd_item");
        exit(-1);
    }
    p_reqInfo =(RequestInfo *)calloc(1, sizeof(RequestInfo));
    if (p_reqInfo == NULL) {
        RLOGE("Failed to allocate memory for p_reqData");
        free(p_reqNode);
        exit(-1);
    }

    p_reqInfo->socket_id = socket_id;
    p_reqInfo->request = request;
    p_reqInfo->data = data;
    p_reqInfo->datalen = datalen;
    p_reqInfo->token = t;

    p_reqNode->p_reqInfo = p_reqInfo;

    ATCmdType cmdType = getCmdType(request);
    if (cmdType == AT_CMD_TYPE_SLOW) {
        request_list_add_tail(cmdList->slowReqList, p_reqNode, socket_id);
        pthread_mutex_lock(&(cmdList->slowDispatchMutex));
        pthread_cond_signal(&(cmdList->slowDispatchCond));
        pthread_mutex_unlock(&(cmdList->slowDispatchMutex));
    } else if (cmdType == AT_CMD_TYPE_NORMAL_SLOW ||
               cmdType == AT_CMD_TYPE_NORMAL_FAST) {
        if (cmdType == AT_CMD_TYPE_NORMAL_FAST) {
            request_list_add_tail(cmdList->callReqList, p_reqNode, socket_id);
        } else {
            request_list_add_tail(cmdList->simReqList, p_reqNode, socket_id);
        }
        pthread_mutex_lock(&(cmdList->normalDispatchMutex));
        pthread_cond_signal(&(cmdList->normalDispatchCond));
        pthread_mutex_unlock(&(cmdList->normalDispatchMutex));
    } else if (cmdType == AT_CMD_TYPE_DATA) {
        request_list_add_tail(s_dataReqList, p_reqNode, RIL_SOCKET_1);
        pthread_mutex_lock(&s_dataDispatchMutex);
        pthread_cond_signal(&s_dataDispatchCond);
        pthread_mutex_unlock(&s_dataDispatchMutex);
    } else {
        request_list_add_tail(cmdList->otherReqList, p_reqNode, socket_id);
        pthread_mutex_lock(&(cmdList->otherDispatchMutex));
        pthread_cond_signal(&(cmdList->otherDispatchCond));
        pthread_mutex_unlock(&(cmdList->otherDispatchMutex));
    }
}

const RIL_TheadsFunctions *requestThreadsInit(RIL_RequestFunctions *requestFunctions,
                                              int simCount, int threadNumber) {
    int i = 0;
    int ret = -1;

    s_requestFunctions = requestFunctions;
    s_simCount = simCount;
    s_threadNumber = threadNumber;
    s_maxChannelNum = (simCount == 1 ? S_MAX_AT_CHANNELS : D_MAX_AT_CHANNELS);

    RLOGD("requestThreadsInit: simCount = %d, threadNumber = %d, maxChannelNum = %d",
            s_simCount, s_threadNumber, s_maxChannelNum);

    s_channelInfo = (ChannelInfo *)calloc(s_maxChannelNum, sizeof(ChannelInfo));
    for (i = 0; i < s_maxChannelNum; i++) {
        s_channelInfo[i].fd = -1;
    }

    s_requestThread = (RequestThreadInfo *)calloc(simCount, sizeof(RequestThreadInfo));

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    for (int simId = 0; simId < s_simCount; simId++) {
        RequestThreadInfo *p_requestThread = &s_requestThread[simId];

        pthread_mutex_init(&p_requestThread->listMutex, NULL);
        pthread_mutex_init(&p_requestThread->normalDispatchMutex, NULL);
        pthread_mutex_init(&p_requestThread->slowDispatchMutex, NULL);
        pthread_mutex_init(&p_requestThread->otherDispatchMutex, NULL);
        pthread_cond_init(&p_requestThread->slowDispatchCond, NULL);
        pthread_cond_init(&p_requestThread->normalDispatchCond, NULL);
        pthread_cond_init(&p_requestThread->otherDispatchCond, NULL);

        request_list_init(&(s_requestThread[simId].slowReqList));
        request_list_init(&(s_requestThread[simId].otherReqList));
        request_list_init(&(s_requestThread[simId].callReqList));
        request_list_init(&(s_requestThread[simId].simReqList));

        s_requestThread[simId].p_threadpool = thread_pool_init(threadNumber, 10000);
        if (s_requestThread[simId].p_threadpool->thr_max == threadNumber) {
            RLOGI("SIM%d: %d CommandThread create",
                    simId, s_requestThread[simId].p_threadpool->thr_max);
        }

        ret = pthread_create(&(s_requestThread[simId].slowDispatchTid),
        &attr, slowDispatch, (void *)&s_socketId[simId]);
        if (ret < 0) {
            RLOGE("Failed to create slow dispatch thread errno: %s", strerror(errno));
            return NULL;
        }

        ret = pthread_create(&(s_requestThread[simId].normalDispatchTid),
        &attr, normalDispatch, (void *)&s_socketId[simId]);
        if (ret < 0) {
            RLOGE("Failed to create normal dispatch thread errno: %s", strerror(errno));
            return NULL;
        }

        ret = pthread_create(&(s_requestThread[simId].otherDispatchTid),
         &attr, otherDispatch, (void *)&s_socketId[simId]);
        if (ret < 0) {
            RLOGE("Failed to create other dispatch thread errno: %s", strerror(errno));
            return NULL;
        }
    }

    if (simCount >= 2) {
        request_list_init(&s_dataReqList);
        pthread_t tid;
        ret = pthread_create(&tid, &attr, dataDispatch, NULL);
        if (ret < 0) {
            RLOGE("Failed to create data dispatch thread errno: %s", strerror(errno));
            return NULL;
        }
    }

#if 0
    s_slowWaitArray = (WaitArrayParam **)calloc(simCount, sizeof(WaitArrayParam *));
    for (i = 0; i < simCount; i++) {
        s_slowWaitArray[i] = (WaitArrayParam *)calloc(s_threadNumber, sizeof(WaitArrayParam));
        memset(s_slowWaitArray[i], -1, sizeof(WaitArrayParam) *  s_threadNumber);
    }

    s_normalWaitArray = (WaitArrayParam **)calloc(simCount, sizeof(WaitArrayParam *));
    for (i = 0; i < simCount; i++) {
        s_normalWaitArray[i] = (WaitArrayParam *)calloc(s_threadNumber, sizeof(WaitArrayParam));
        memset(s_normalWaitArray[i], -1, sizeof(WaitArrayParam) *  s_threadNumber);
    }

    s_dataWaitArray = (WaitArrayParam *)calloc(s_threadNumber, sizeof(WaitArrayParam));
    memset(s_dataWaitArray, -1, sizeof(WaitArrayParam) * s_threadNumber);

    s_semLockArray = (SemLockInfo *)calloc(s_threadNumber, sizeof(SemLockInfo));
    for (i = 0; i< s_threadNumber; i++) {
        sem_init(&s_semLockArray[i].semLock, 0, 0);
    }
#endif

    return &s_threadsFunctions;
}

bool isLLVersion() {
    char prop[PROPERTY_VALUE_MAX];
    property_get("persist.vendor.radio.modem.config", prop, "");
    if (strcmp(prop, "TL_LF_TD_W_G,TL_LF_TD_W_G") == 0 ||
        strcmp(prop, "TL_LF_W_G,TL_LF_W_G") == 0) {
        return true;
    } else {
        return false;
    }
}
