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

void requestThreadsInit(int simCount, int threadNumber);
void setChannelInfo(int fd, int channelID);
void setChannelOpened(RIL_SOCKET_ID socket_id);
RIL_SOCKET_ID getSocketIdByChannelID(int channelID);
int getChannel(RIL_SOCKET_ID socket_id);
void putChannel(int channelID);
int getRequestChannel(RIL_SOCKET_ID socket_id, int request);
void putRequestChannel(RIL_SOCKET_ID socket_id, int request, int channelID);

extern const char *requestToString(int request);

#endif  // REQUEST_THREADS_H
