#ifndef REQUEST_THREADS_H
#define REQUEST_THREADS_H

#define ARRAY_SIZE          128  // cannot change the value

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
    AT_CMD_TYPE_NETWORK,
    AT_CMD_TYPE_OTHER,
} ATCmdType;

typedef struct {
    ATCmdType cmdType;
    int semLockID;
} WaitArrayParam;

enum ChannelState {
    CHANNEL_IDLE,
    CHANNEL_BUSY,
};

typedef struct ChannelInfo {
    int channelID;
    int fd;
    char name[ARRAY_SIZE];
    enum ChannelState state;
    pthread_mutex_t mutex;
} ChannelInfo;

typedef enum {
    D_AT_URC,
    D_AT_CHANNEL_1,
    D_AT_CHANNEL_2,
    D_AT_CHANNEL_OFFSET,

    D_AT_URC2 = D_AT_CHANNEL_OFFSET,
    D_AT_CHANNEL2_1,
    D_AT_CHANNEL2_2,

    D_MAX_AT_CHANNELS
} D_ATChannelId;

typedef enum {
    S_AT_URC,
    S_AT_CHANNEL_1,
    S_AT_CHANNEL_2,
    S_AT_CHANNEL_3,
    S_AT_CHANNEL_OFFSET,
    S_MAX_AT_CHANNELS = S_AT_CHANNEL_OFFSET
} S_ATChannelId;

typedef struct {
    RIL_SOCKET_ID socket_id;
    int request;
    void *data;
    size_t datalen;
    RIL_Token token;
} RequestInfo;

typedef struct RequestListNode {
    RequestInfo *p_reqInfo;
    struct RequestListNode *next;
    struct RequestListNode *prev;
} RequestListNode;

typedef struct {
    pthread_t slowDispatchTid;
    pthread_t normalDispatchTid;
    pthread_t networkDispatchTid;
    pthread_t otherDispatchTid;

    threadpool_t *p_threadpool;

    RequestListNode *callReqList;
    RequestListNode *simReqList;
    RequestListNode *slowReqList;
    RequestListNode *networkReqList;
    RequestListNode *otherReqList;

    pthread_mutex_t listMutex;
    pthread_mutex_t normalDispatchMutex;
    pthread_mutex_t slowDispatchMutex;
    pthread_mutex_t networkDispatchMutex;
    pthread_mutex_t otherDispatchMutex;
    pthread_cond_t slowDispatchCond;
    pthread_cond_t normalDispatchCond;
    pthread_cond_t networkDispatchCond;
    pthread_cond_t otherDispatchCond;
} RequestThreadInfo;

extern const char *requestToString(int request);

#endif  // REQUEST_THREADS_H
