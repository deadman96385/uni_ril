/**
 * request_threads.c --- ril multi threads process implementation
 *
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 */

#define LOG_TAG "MBIM_THDS"

#include <string.h>
#include <errno.h>
#include <utils/Log.h>

#include "mbim_message_processer.h"
#include "mbim_message_threads.h"
#include "mbim_device_config.h"
#include "mbim_cid.h"
#include "misc.h"

static MessageThreadInfo *  s_messageThread = NULL;
extern const RIL_SOCKET_ID  s_socketId[];

static void
message_list_init(MessageListNode **node) {
    *node = (MessageListNode *)malloc(sizeof(MessageListNode));
    if (*node == NULL) {
        RLOGE("Failed malloc memory!");
    } else {
        (*node)->next = *node;
        (*node)->prev = *node;
    }
}

static void
message_list_add_tail(MessageListNode *     head,
                      MessageListNode *     item,
                      RIL_SOCKET_ID         socket_id) {
    pthread_mutex_lock(&s_messageThread[socket_id].listMutex);
    item->next = head;
    item->prev = head->prev;
    head->prev->next = item;
    head->prev = item;
    pthread_mutex_unlock(&s_messageThread[socket_id].listMutex);
}

static void
message_list_add_head(MessageListNode *     head,
                      MessageListNode *     item,
                      RIL_SOCKET_ID         socket_id) {
    pthread_mutex_lock(&s_messageThread[socket_id].listMutex);
    item->next = head->next;
    item->prev = head;
    head->next->prev = item;
    head->next = item;
    pthread_mutex_unlock(&s_messageThread[socket_id].listMutex);
}

static void
message_list_remove(MessageListNode *   item,
                    RIL_SOCKET_ID       socket_id) {
    pthread_mutex_lock(&s_messageThread[socket_id].listMutex);
    item->next->prev = item->prev;
    item->prev->next = item->next;
    pthread_mutex_unlock(&s_messageThread[socket_id].listMutex);
}

static CommandMessageType
getCmdMsgType(MbimService   service,
              uint32_t      cid) {
    MBIM_UNUSED_PARM(service);
    MBIM_UNUSED_PARM(cid);

    // TODO: according the future debug work, to decide the queue
    CommandMessageType cmdMsgType = CMD_MSG_TYPE_NORMAL;

    return cmdMsgType;
}

static MbimMessageInfo *
mbim_message_info_new(MbimMessage *         message,
                      MbimCommunicationType communicationType) {
    int buffer_length = 0;
    const uint8_t *buffer = NULL;

    MbimMessageInfo *mbimMessageInfo =
            (MbimMessageInfo *)calloc(1, sizeof(MbimMessageInfo));

    mbimMessageInfo->message = message;
    mbimMessageInfo->original_transaction_id =
            mbim_message_get_transaction_id(message);
    mbimMessageInfo->service = mbim_message_command_get_service(message);
    mbimMessageInfo->cid = mbim_message_command_get_cid(message);
    mbimMessageInfo->commandType = mbim_message_command_get_command_type(message);
    mbimMessageInfo->communicationType = communicationType;

    RLOGD("trasactionId = %d, service = %s, cid = %s, commandType: %s",
            mbimMessageInfo->original_transaction_id,
            mbim_service_get_printable(mbimMessageInfo->service),
            mbim_cid_get_printable(mbimMessageInfo->service, mbimMessageInfo->cid),
            mbimMessageInfo->commandType == 0 ? "query": "set");

    return mbimMessageInfo;
}

void
enqueueCommandMessage(MbimMessage *         message,
                      MbimCommunicationType communicationType) {
    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    MessageListNode *p_messageListNode = NULL;
    MessageThreadInfo *p_messageThread = &s_messageThread[socket_id];

    p_messageListNode = (MessageListNode *)calloc(1, sizeof(MessageListNode));
    if (p_messageListNode == NULL) {
        RLOGE("Failed to allocate memory for cmd_item");
        return;
    }

    MbimMessageInfo *p_messageInfo = mbim_message_info_new(message, communicationType);

    p_messageListNode->p_reqInfo = p_messageInfo;

    CommandMessageType cmdType = getCmdMsgType(p_messageInfo->service, p_messageInfo->cid);
    if (cmdType == CMD_MSG_TYPE_SLOW) {
        message_list_add_tail(p_messageThread->slowReqList, p_messageListNode, socket_id);
        pthread_mutex_lock(&(p_messageThread->slowDispatchMutex));
        pthread_cond_signal(&(p_messageThread->slowDispatchCond));
        pthread_mutex_unlock(&(p_messageThread->slowDispatchMutex));
    } else if (cmdType == CMD_MSG_TYPE_NORMAL) {
        message_list_add_tail(p_messageThread->normalReqList, p_messageListNode, socket_id);
        pthread_mutex_lock(&(p_messageThread->normalDispatchMutex));
        pthread_cond_signal(&(p_messageThread->normalDispatchCond));
        pthread_mutex_unlock(&(p_messageThread->normalDispatchMutex));
    } else {
        message_list_add_tail(p_messageThread->otherReqList, p_messageListNode, socket_id);
        pthread_mutex_lock(&(p_messageThread->otherDispatchMutex));
        pthread_cond_signal(&(p_messageThread->otherDispatchCond));
        pthread_mutex_unlock(&(p_messageThread->otherDispatchMutex));
    }
}

static void
dequeueCommandMessage(MbimMessageInfo *messageInfo) {
    mbim_command_message_parser(messageInfo);
}


static void *
slowDispatch(void *param) {
    MessageListNode *cmd_item;
    pid_t tid = gettid();

    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    pthread_mutex_t *slowDispatchMutex =
            &(s_messageThread[socket_id].slowDispatchMutex);
    pthread_cond_t *slowDispatchCond =
            &(s_messageThread[socket_id].slowDispatchCond);
    MessageListNode *slow_list = s_messageThread[socket_id].slowReqList;

    while (1) {
        pthread_mutex_lock(slowDispatchMutex);
        if (slow_list->next == slow_list) {
            pthread_cond_wait(slowDispatchCond, slowDispatchMutex);
        }
        pthread_mutex_unlock(slowDispatchMutex);

        for (cmd_item = slow_list->next; cmd_item != slow_list;
                cmd_item = slow_list->next) {
            MbimMessageInfo *p_reqInfo = cmd_item->p_reqInfo;
            dequeueCommandMessage(p_reqInfo);
            RLOGI("-->slowDispatch [%d] free one command", tid);
            /* remove list node first, then free it */
            message_list_remove(cmd_item, socket_id);
            free(cmd_item->p_reqInfo);
            free(cmd_item);
        }
    }
    return NULL; /*lint !e527 */
}

static void *
normalDispatch(void *param) {
    MessageListNode *cmd_item = NULL;
    pid_t tid = gettid();

    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    pthread_mutex_t *normalDispatchMutex =
            &(s_messageThread[socket_id].normalDispatchMutex);
    pthread_cond_t *normalDispatchCond =
            &(s_messageThread[socket_id].normalDispatchCond);
    MessageListNode *normal_list = s_messageThread[socket_id].normalReqList;

    while (1) {
        pthread_mutex_lock(normalDispatchMutex);
        if (normal_list->next == normal_list) {
            pthread_cond_wait(normalDispatchCond, normalDispatchMutex);
        }
        pthread_mutex_unlock(normalDispatchMutex);

        for (cmd_item = normal_list->next; cmd_item != normal_list;
                cmd_item = normal_list->next) {
            MbimMessageInfo *p_reqInfo = cmd_item->p_reqInfo;
            dequeueCommandMessage(p_reqInfo);
            RLOGI("-->normalDispatch [%d] free one command", tid);
            message_list_remove(cmd_item, socket_id);
            free(cmd_item->p_reqInfo);
            free(cmd_item);
        }
    }
    return NULL; /*lint !e527 */
}

static void
CommandThread(void *    arg,
              void *    socket_id) {
    pid_t tid = gettid();
    MbimMessageInfo *p_reqInfo = (MbimMessageInfo *)arg;
    RIL_SOCKET_ID soc_id = *((RIL_SOCKET_ID *)socket_id);
    dequeueCommandMessage(p_reqInfo);
    RLOGI("-->CommandThread [%d] free one command", tid);
    free(p_reqInfo);
}

static void *
otherDispatch(void *param) {
    int ret = -1;
    pid_t tid = gettid();
    MessageListNode *cmd_item = NULL;

    RIL_SOCKET_ID socket_id = *((RIL_SOCKET_ID *)param);
    pthread_mutex_t *otherDispatchMutex =
            &(s_messageThread[socket_id].otherDispatchMutex);
    pthread_cond_t *otherDispatchCond =
            &(s_messageThread[socket_id].otherDispatchCond);
    MessageListNode *other_list = s_messageThread[socket_id].otherReqList;
    threadpool_t *thrpool = s_messageThread[socket_id].p_threadpool;

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
            /* remove listnode first, then free it */
            message_list_remove(cmd_item, socket_id);
            free(cmd_item);
        }
    }
    return NULL; /*lint !e527 */
}

int
messageThreadsInit(int  simCount,
                   int  threadNumber) {
    int ret = -1;

    RLOGD("messageThreadsInit: simCount = %d, threadNumber = %d",
            simCount, threadNumber);

    s_messageThread = (MessageThreadInfo *)calloc(simCount, sizeof(MessageThreadInfo));

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    for (int simId = 0; simId < simCount; simId++) {
        MessageThreadInfo *p_messageThread = &s_messageThread[simId];

        pthread_mutex_init(&p_messageThread->listMutex, NULL);
        pthread_mutex_init(&p_messageThread->normalDispatchMutex, NULL);
        pthread_mutex_init(&p_messageThread->slowDispatchMutex, NULL);
        pthread_mutex_init(&p_messageThread->otherDispatchMutex, NULL);
        pthread_cond_init(&p_messageThread->slowDispatchCond, NULL);
        pthread_cond_init(&p_messageThread->normalDispatchCond, NULL);
        pthread_cond_init(&p_messageThread->otherDispatchCond, NULL);

        message_list_init(&(s_messageThread[simId].slowReqList));
        message_list_init(&(s_messageThread[simId].normalReqList));
        message_list_init(&(s_messageThread[simId].otherReqList));

        s_messageThread[simId].p_threadpool = thread_pool_init(threadNumber, 10000);
        if (s_messageThread[simId].p_threadpool->thr_max == threadNumber) {
            RLOGI("SIM%d: %d CommandThread create",
                    simId, s_messageThread[simId].p_threadpool->thr_max);
        }

        ret = pthread_create(&(s_messageThread[simId].slowDispatchTid),
                &attr, slowDispatch, (void *)&s_socketId[simId]);
        if (ret < 0) {
            RLOGE("Failed to create slow dispatch thread errno: %s", strerror(errno));
            return -1;
        }

        ret = pthread_create(&(s_messageThread[simId].normalDispatchTid),
                &attr, normalDispatch, (void *)&s_socketId[simId]);
        if (ret < 0) {
            RLOGE("Failed to create normal dispatch thread errno: %s", strerror(errno));
            return -1;
        }

        ret = pthread_create(&(s_messageThread[simId].otherDispatchTid),
                &attr, otherDispatch, (void *)&s_socketId[simId]);
        if (ret < 0) {
            RLOGE("Failed to create other dispatch thread errno: %s", strerror(errno));
            return -1;
        }
    }

    return 0;
}
