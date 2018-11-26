#ifndef MBIM_MESSAGE_THREADS_H
#define MBIM_MESSAGE_THREADS_H

#include "mbim_message.h"
#include "thread_pool.h"

typedef enum {
    CMD_MSG_TYPE_UNKOWN = -1,
    CMD_MSG_TYPE_SLOW,
    CMD_MSG_TYPE_NORMAL,
    CMD_MSG_TYPE_OTHER,
} CommandMessageType;

typedef struct MessageListNode {
    MbimMessageInfo *p_reqInfo;
    struct MessageListNode *next;
    struct MessageListNode *prev;
} MessageListNode;

typedef struct {
    pthread_t slowDispatchTid;
    pthread_t normalDispatchTid;
    pthread_t otherDispatchTid;

    threadpool_t *p_threadpool;

    MessageListNode *slowReqList;
    MessageListNode *normalReqList;
    MessageListNode *otherReqList;

    pthread_mutex_t listMutex;
    pthread_mutex_t slowDispatchMutex;
    pthread_mutex_t normalDispatchMutex;
    pthread_mutex_t otherDispatchMutex;
    pthread_cond_t slowDispatchCond;
    pthread_cond_t normalDispatchCond;
    pthread_cond_t otherDispatchCond;
} MessageThreadInfo;

/*****************************************************************************/

void
enqueueCommandMessage(MbimMessage *         message,
                      MbimCommunicationType communicationType);

int messageThreadsInit      (int                simCount,
                             int                threadNumber);

#endif  // MBIM_MESSAGE_THREADS_H
