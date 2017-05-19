
#define LOG_TAG "REQ_THREADS"

#include "sprd_ril.h"

typedef struct {
    sem_t semLock;
    pthread_mutex_t semLockMutex;
} SemLockInfo;

typedef enum {
    AT_CMD_TYPE_UNKOWN = -1,
    AT_CMD_TYPE_SLOW,
    AT_CMD_TYPE_NORMAL_SLOW,
    AT_CMD_TYPE_NORMAL_FAST,
    AT_CMD_TYPE_DATA,
    AT_CMD_TYPE_OTHER,
} ATCmdType;

typedef struct {
    ATCmdType cmdType;
    int semLockID;
} WaitArrayParam;

WaitArrayParam s_slowWaitArray[SIM_COUNT][THREAD_NUMBER];
WaitArrayParam s_normalWaitArray[SIM_COUNT][THREAD_NUMBER];
WaitArrayParam s_dataWaitArray[THREAD_NUMBER];

pthread_mutex_t s_waitArrayMutex;
SemLockInfo s_semLockArray[THREAD_NUMBER];

int acquireSemLock() {
    int semLockID = 0;

    for (;;) {
        for (semLockID = 0; semLockID < THREAD_NUMBER; semLockID++) {
            if (pthread_mutex_trylock(&s_semLockArray[semLockID].semLockMutex) == 0) {
                return semLockID;
            }
        }
        usleep(5000);
    }

    return semLockID;
}

void releaseSemLock(int semLockID) {
    if (semLockID < 0 || semLockID >= THREAD_NUMBER) {
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
        for (i = 0; i < THREAD_NUMBER; i++) {
            if (-1 == pWaitArray[i].semLockID ||
               (i != 0 && pWaitArray[i].cmdType == AT_CMD_TYPE_NORMAL_SLOW)) {
                int j = 0;
                for (j = i; j < THREAD_NUMBER - 1, pWaitArray[j].semLockID != -1; j++) {
                    pWaitArray[j + 1] = pWaitArray[j];
                }
                pWaitArray[i].semLockID = semLockId;
                pWaitArray[i].cmdType = cmdType;
                index = i;
                break;
            }
        }
    } else {
        for (i = 0; i < THREAD_NUMBER; i++) {
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
    int i;
    for (i = 0; i < (THREAD_NUMBER - 1); i++) {
        pWaitArray[i] = pWaitArray[i + 1];
    }
    pWaitArray[THREAD_NUMBER - 1].semLockID = -1;
    pWaitArray[THREAD_NUMBER - 1].cmdType = AT_CMD_TYPE_UNKOWN;
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
#if (SIM_COUNT == 1)
        || request == RIL_REQUEST_ALLOW_DATA
        || request == RIL_REQUEST_SETUP_DATA_CALL
#endif
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
    }
#if defined (ANDROID_MULTI_SIM)
    else if (request == RIL_REQUEST_ALLOW_DATA
          || request == RIL_REQUEST_SETUP_DATA_CALL) {
        cmdType = AT_CMD_TYPE_DATA;
    }
#endif
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
    int i;
    for (i = 0; i < THREAD_NUMBER; i++) {
        if (pWaitArray[i].semLockID != -1) {
            if (pWaitArray == s_slowWaitArray[socket_id] || pWaitArray == s_normalWaitArray[socket_id]) {
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
    RLOGD("getRequestChannel: socket_id = %d, request = %s, type = %s", socket_id, requestToString(request), typeToString(cmdType));
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
        } else if (index > 0 || index < THREAD_NUMBER) {
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
    RLOGD("putRequestChannel: socket_id = %d, request = %s, type = %s", socket_id, requestToString(request), typeToString(cmdType));

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

void requestThreadsInit() {
    int i = 0;

    memset(s_slowWaitArray, -1, sizeof(WaitArrayParam) * SIM_COUNT * THREAD_NUMBER);
    memset(s_normalWaitArray, -1, sizeof(WaitArrayParam) * SIM_COUNT * THREAD_NUMBER);
    memset(s_dataWaitArray, -1, sizeof(WaitArrayParam) * THREAD_NUMBER);

    for (i = 0; i< THREAD_NUMBER; i++) {
        sem_init(&s_semLockArray[i].semLock, 0, 0);
    }
}


