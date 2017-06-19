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
#include <utils/Log.h>

#include "telephony/ril.h"
#include "request_threads.h"

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
                for (j = i; j < s_threadNumber - 1, pWaitArray[j].semLockID != -1; j++) {
                    pWaitArray[j + 1] = pWaitArray[j];
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

ATCmdType getCmdType(int request) {
    ATCmdType cmdType = AT_CMD_TYPE_OTHER;
    if (request == RIL_REQUEST_SEND_SMS
        || request == RIL_REQUEST_SEND_SMS_EXPECT_MORE
        || request == RIL_REQUEST_IMS_SEND_SMS
        || request == RIL_REQUEST_QUERY_FACILITY_LOCK
        || request == RIL_REQUEST_SET_FACILITY_LOCK
        || request == RIL_REQUEST_QUERY_CALL_FORWARD_STATUS
        || request == RIL_REQUEST_SET_CALL_FORWARD
        || request == RIL_REQUEST_GET_CLIR
        || request == RIL_REQUEST_SET_CLIR
        || request == RIL_REQUEST_QUERY_CALL_WAITING
        || request == RIL_REQUEST_SET_CALL_WAITING
        || request == RIL_REQUEST_QUERY_CLIP
        || request == RIL_REQUEST_DEACTIVATE_DATA_CALL) {
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
            /* IMS @{ */
            || request == RIL_REQUEST_IMS_CALL_RESPONSE_MEDIA_CHANGE
            || request == RIL_REQUEST_IMS_CALL_REQUEST_MEDIA_CHANGE
            || request == RIL_REQUEST_IMS_CALL_FALL_BACK_TO_VOICE
            || request == RIL_REQUEST_IMS_INITIAL_GROUP_CALL
            || request == RIL_REQUEST_IMS_ADD_TO_GROUP_CALL
            /* }@ */
            /* OEM SOCKET REQUEST @{ */
            || request == RIL_EXT_REQUEST_VIDEOPHONE_DIAL
            || request == RIL_EXT_REQUEST_SWITCH_MULTI_CALL
            || request == RIL_EXT_REQUEST_GET_HD_VOICE_STATE
            || request == RIL_EXT_REQUEST_SIMMGR_SIM_POWER
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
    }
    return cmdType;
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
        RLOGD("getRequestChannel: index in waitArray = %d", index);
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
            RLOGD("putRequestChannel, sem_post semLockId = %d", semLockID);
        }
        pthread_mutex_unlock(&s_waitArrayMutex);
    }
}

void requestThreadsInit(int simCount, int threadNumber) {
    int i = 0;

    s_simCount = simCount;
    s_threadNumber = threadNumber;
    s_maxChannelNum = (simCount == 1 ? S_MAX_AT_CHANNELS : D_MAX_AT_CHANNELS);

    RLOGD("requestThreadsInit: simCount = %d, threadNumber = %d, maxChannelNum = %d",
            s_simCount, s_threadNumber, s_maxChannelNum);

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

    s_channelInfo = (ChannelInfo *)calloc(s_maxChannelNum, sizeof(ChannelInfo));
    for (i = 0; i < s_maxChannelNum; i++) {
        s_channelInfo[i].fd = -1;
    }
}
