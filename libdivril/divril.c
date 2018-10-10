/* //vendor/sprd/proprietories-source/modemd/libdivril/divril.c
**
**
*/


#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/types.h>
#include <pwd.h>
#include <cutils/properties.h>
#include <cutils/sockets.h>
#include <dlfcn.h>
#include <getopt.h>
#include <netutils/ifc.h>
#include "divril.h"
#include <../../ril/sprd-ril/at_tok.h>
#define LOG_TAG "DivRIL"

#include <log/log.h>

int s_fdListen[MAX_SERVICE_NUM];
int s_fdClient[MAX_SERVICE_NUM];
int s_nfds[MAX_SERVICE_NUM];
const char *s_serviceName[MAX_SERVICE_NUM] = {
    "EMBMS",
    "MDT"
};

const char *s_socketName[MAX_SERVICE_NUM] = {
    "embmsd",
    "mdt_socket"
};

const char *s_processName[MAX_SERVICE_NUM] = {
    "u0_a84",
    "mdt"
};

const SERVICE_ID s_funcId[MAX_SERVICE_NUM] = {
    EMBMS,
    MDT
};

fd_set     s_readFds[MAX_SERVICE_NUM];
pthread_t  s_readerThreadTid[MAX_SERVICE_NUM];
pthread_t  s_mainThreadTid[MAX_SERVICE_NUM];

ChannelInfo s_channelInfo[MAX_SERVICE_NUM][MUX_NUM];
ChannelInfo s_socketInfo[MAX_SERVICE_NUM];

static pthread_cond_t   s_channelCond[MAX_SERVICE_NUM];
static pthread_mutex_t  s_channelMutex[MAX_SERVICE_NUM];
static pthread_mutex_t  s_mainwriteMutex[MAX_SERVICE_NUM];

static bool s_needEchoCommand = false;
char s_deviceInfoPtr[PROPERTY_VALUE_MAX]; //mac address

int at_tok_start_flag(char **p_cur, char start_flag) {
    if (*p_cur == NULL) {
        return -1;
    }
    // skip prefix
    // consume "^[^:]:"
    *p_cur = strchr(*p_cur, start_flag);
    if (*p_cur == NULL) {
        return -1;
    }
    (*p_cur)++;
    return 0;
}

static int blockingWrite(int fd, const void *buffer, size_t len) {
    size_t writeOffset = 0;
    const uint8_t *toWrite;
    toWrite = (const uint8_t *)buffer;
    while (writeOffset < len) {
        ssize_t written;
        do {
            written = write(fd, toWrite + writeOffset,
                                len - writeOffset);
            RLOGD("send to:%d %d byte :%s", fd, (int)(len - writeOffset), toWrite + writeOffset);
        } while (written < 0 && ((errno == EINTR) || (errno == EAGAIN)));
        if (written >= 0) {
            writeOffset += written;
        } else {   // written < 0
            RLOGE("Response: unexpected error on write errno:%d, %s",
                   errno, strerror(errno));
            close(fd);
            return -1;
        }
    }
    return 0;
}

static int sendResponse(int serviceId, const void *data, size_t dataSize) {
    int ret;

    int fd = s_fdClient[serviceId];

    if (fd < 0) {
        return -1;
    }
    if (dataSize > MAX_BUFFER_BYTES) {
        RLOGE("packet larger than %u (%u)",
               MAX_BUFFER_BYTES, (unsigned int)dataSize);
        return -1;
    }
    pthread_mutex_lock(&s_mainwriteMutex[serviceId]);
    ret = blockingWrite(fd, data, dataSize);
    if (ret < 0) {
        RLOGE("blockingWrite error");
        pthread_mutex_unlock(&s_mainwriteMutex[serviceId]);
        return ret;
    }

    pthread_mutex_unlock(&s_mainwriteMutex[serviceId]);
    return 0;
}

int getMacAddrInfo(char *ptr)
{
    FILE *stream;
    char buf[MAX_BUFFER_BYTES];
    char *position = NULL;
    char *positionEther = NULL;

    memset(buf, '\0', sizeof(buf));
    stream = popen("ip addr show up", "r");
    fread(buf, sizeof(char), sizeof(buf), stream);
    pclose(stream);
    position = buf;

    if ((positionEther = strstr(position, "ether"))!= NULL) {
        positionEther += 6;
        strncpy(ptr, positionEther, 17);
        RLOGD("matt:wlan0 mac addr is :\n%s\n", ptr);
        return 1;
    }

    return 0;
}

static int strStartsWith(const char *line, const char *prefix) {
    for ( ; *line != '\0' && *prefix != '\0' ; line++, prefix++) {
        if (*line != *prefix) return 0;
    }
    return *prefix == '\0';
}

static void addIntermediate(ChannelInfo *chInfo) {
    RespLine *p_new = (RespLine *)malloc(sizeof(RespLine));
    p_new->line = strdup(chInfo->line);
    p_new->p_next = chInfo->sp_response->p_intermediates;
    chInfo->sp_response->p_intermediates = p_new;
}

static const char *s_finalResponsesError[] = {"ERROR", "+CMS ERROR:", "+CME ERROR:"};
static int isFinalResponseError(const char *line) {
    size_t i;
    for (i = 0; i < NUM_ELEMS(s_finalResponsesError); i++) {
        if (strStartsWith(line, s_finalResponsesError[i])) {
            return 1;
        }
    }
    return 0;
}

static const char *s_finalResponsesSuccess[] = {"OK", "CONNECT"};
static int isFinalResponseSuccess(const char *line) {
    size_t i;
    for (i = 0; i < NUM_ELEMS(s_finalResponsesSuccess); i++) {
        if (strStartsWith(line, s_finalResponsesSuccess[i])) {
            return 1;
        }
    }
    return 0;
}

static void handleFinalResponse(int serviceId, ChannelInfo *chInfo) {
    chInfo->sp_response->finalResponse = strdup(chInfo->line);
    pthread_cond_signal(&s_channelCond[serviceId]);
}

void processUnsolResp(int serviceId, char *line) {
    int err = -1;
    char *s = NULL;
    char buf[ARRAY_SIZE] = { 0 };

    RLOGD("[%s]Unsol Resp: %s", s_serviceName[serviceId], line);
    s = strdup(line);
    if (serviceId == EMBMS) {
        if (strStartsWith(line, "+MBMSEREG:")) {
            int status = 0;
            char *tmp = NULL;
            tmp = s;

            err = at_tok_start(&tmp);
            if (err < 0)
                goto out;

            err = at_tok_nextint(&tmp, &status);
            if (err < 0)
                goto out;

            snprintf(buf, sizeof(buf), "+CEREG: %d", status);
            strlcat(buf, "\r\n", sizeof(buf));
            sendResponse(serviceId, buf, strlen(buf));
        } else {
            strncpy(buf, s, strlen(s));
            strlcat(buf, "\r\n", sizeof(buf));
            sendResponse(serviceId, buf, strlen(buf));
        }
    } else if (serviceId == MDT) {
        sendResponse(serviceId, line, strlen(line));
    }

out:
    free(s);
}

static void handleUnsolicited(int serviceId, ChannelInfo *chInfo) {
    char *line = chInfo->line;

    processUnsolResp(serviceId, line);
}

static void processLine(int serviceId, ChannelInfo *chInfo) {
    pthread_mutex_lock(&s_channelMutex[serviceId]);
    if (chInfo->sp_response == NULL) {
        handleUnsolicited(serviceId, chInfo);
    } else if (isFinalResponseSuccess(chInfo->line)) {
        chInfo->sp_response->success = 1;
        handleFinalResponse(serviceId, chInfo);
    } else if (isFinalResponseError(chInfo->line)) {
        chInfo->sp_response->success = 0;
        handleFinalResponse(serviceId, chInfo);
    } else {
        addIntermediate(chInfo);
    }
    pthread_mutex_unlock(&s_channelMutex[serviceId]);
}

static char *findNextEOL(char *cur) {
    if (cur[0] == '>' && cur[1] == ' ' && cur[2] == '\0') {
        return cur+2;  /* SMS prompt character...not \r terminated */
    }
    while (*cur != '\0' && *cur != '\r' && *cur != '\n') cur++;

    return *cur == '\0' ? NULL : cur;
}

static const char *readline(ChannelInfo *chInfo) {
    ssize_t count, err_count = 0;
    char *p_read = NULL, *p_eol = NULL;

    if (*chInfo->s_respBufferCur == '\0') {  // empty buffer
        chInfo->s_respBufferCur = chInfo->s_respBuffer;
        *chInfo->s_respBufferCur = '\0';
        p_read = chInfo->s_respBuffer;
    } else {  // there's data in the buffer from the last read
        while (*chInfo->s_respBufferCur == '\r' || *chInfo->s_respBufferCur == '\n') {
            chInfo->s_respBufferCur++;
        }

        p_eol = findNextEOL(chInfo->s_respBufferCur);
        if (p_eol == NULL) {
            size_t len = strlen(chInfo->s_respBufferCur);
            memmove(chInfo->s_respBuffer, chInfo->s_respBufferCur, len + 1);
            p_read = chInfo->s_respBuffer + len;
            chInfo->s_respBufferCur = chInfo->s_respBuffer;
        }
    }

    while (p_eol == NULL) {
        if (0 == MAX_BUFFER_BYTES - (p_read - chInfo->s_respBuffer)) {
            chInfo->s_respBufferCur = chInfo->s_respBuffer;
            *chInfo->s_respBufferCur = '\0';
            p_read = chInfo->s_respBuffer;
        }

        do {
            count = read(chInfo->s_fd, p_read, MAX_BUFFER_BYTES - (p_read - chInfo->s_respBuffer));
        } while (count < 0 && errno == EINTR);

        if (count > 0) {
            p_read[count] = '\0';
            while (*chInfo->s_respBufferCur == '\r' || *chInfo->s_respBufferCur == '\n') {
                chInfo->s_respBufferCur++;
            }
            p_eol = findNextEOL(chInfo->s_respBufferCur);
            p_read += count;
            err_count = 0;
        } else if (count <= 0) {  /* read error encountered or EOF reached */
            if (count == 0) {
                err_count++;
                if (err_count > 10) {
                    sleep(10);
                } else {
                    RLOGE("atchannel: EOF reached. err_count = %zd", err_count);
                }
            }
            return NULL;
        }
    }

    chInfo->line = chInfo->s_respBufferCur;
    *p_eol = '\0';
    chInfo->s_respBufferCur = p_eol + 1;
    if (strstr(chInfo->name, "embmsd") != NULL) {
        RLOGD("embmsd read num = %d, buf = %s", (int)strlen(chInfo->line), chInfo->line);
    } else if (strstr(chInfo->name, "mdt_socket") != NULL) {
        RLOGD("mdt_socket read num = %d, buf = %s", (int)strlen(chInfo->line), chInfo->line);
    } else {
        RLOGD("%s: Recv < %s\n", chInfo->name, chInfo->line);
    }

    return chInfo->line;
}

static int writeline(ChannelInfo *chInfo, const char *s) {
    size_t cur = 0;
    size_t len = strlen(s);
    ssize_t written;

    if (chInfo->s_fd < 0) return ERROR_CHANNEL_CLOSED;

    RLOGD("%s: Send > %s\n", chInfo->name, s);
    while (cur < len) {  /* the main string */
        do {
            written = write(chInfo->s_fd, s + cur, len - cur);
        } while (written < 0 && errno == EINTR);
        if (written < 0) {
            return ERROR_GENERIC;
        }
        cur += written;
    }

    do {
        written = write(chInfo->s_fd, "\r" , 1);  /* the \r */
    } while ((written < 0 && errno == EINTR) || (written == 0));
    if (written < 0) {
        return ERROR_GENERIC;
    }
    return 0;
}

static CmdResponse * at_response_new() {
    return (CmdResponse *)calloc(1, sizeof(CmdResponse));
}

static void at_response_free(CmdResponse *p_response) {
    if (p_response == NULL) return;

    RespLine *p_line = p_response->p_intermediates;
    while (p_line != NULL) {
        RespLine *p_toFree = p_line;
        p_line = p_line->p_next;
        free(p_toFree->line);
        free(p_toFree);
    }

    free(p_response->finalResponse);
    free(p_response);
}

static void reverseIntermediates(ChannelInfo *chInfo) {
    RespLine *pcur, *pnext;
    pcur = chInfo->sp_response->p_intermediates;
    chInfo->sp_response->p_intermediates = NULL;

    while (pcur != NULL) {
        pnext = pcur->p_next;
        pcur->p_next = chInfo->sp_response->p_intermediates;
        chInfo->sp_response->p_intermediates = pcur;
        pcur = pnext;
    }
}

static void clearPendingCommand(struct ChannelInfo *chInfo) {
    CmdResponse *sp_response = chInfo->sp_response;

    if (sp_response != NULL) {
        at_response_free(sp_response);
    }

    chInfo->sp_response = NULL;
    chInfo->s_responsePrefix = NULL;
}

static int send_command_full_nolock(int serviceId, const char *command,
                                    const char *responsePrefix, int timeoutSec,
                                    CmdResponse **pp_outResponse) {
    int err = 0;
    struct timespec ts;
    struct timespec tv;
    ChannelInfo *chInfo = NULL;
    chInfo = &s_channelInfo[serviceId][0];

    if (chInfo->sp_response != NULL) {
        err = ERROR_COMMAND_PENDING;
        goto done;
    }
    chInfo->s_responsePrefix = (char *)responsePrefix;
    chInfo->sp_response = at_response_new();
    if (chInfo->sp_response == NULL) {
        err = ERROR_GENERIC;
        goto done;
    }

    err = writeline(chInfo, command);
    if (err < 0) goto done;

    clock_gettime(CLOCK_MONOTONIC, &tv);

    if (timeoutSec != 0) {
        ts.tv_sec = tv.tv_sec + timeoutSec;
        ts.tv_nsec = 0;
    }
    while (chInfo->sp_response->finalResponse == NULL) {
        if (timeoutSec != 0) {
            err = pthread_cond_timedwait(&s_channelCond[serviceId], &s_channelMutex[serviceId], &ts);
        } else {
            err = pthread_cond_wait(&s_channelCond[serviceId], &s_channelMutex[serviceId]);
        }
        if (err == ETIMEDOUT) {
            err = ERROR_TIMEOUT;
            goto done;
        }
    }

    if (pp_outResponse == NULL) {
        at_response_free(chInfo->sp_response);
    } else {  /* line reader stores intermediate responses in reverse order */
        reverseIntermediates(chInfo);
        *pp_outResponse = chInfo->sp_response;
    }

    chInfo->sp_response = NULL;
    err = 0;

done:
    clearPendingCommand(chInfo);
    return err;
}

static int sendCommand(int serviceId, const char *command, int tag) {
    int timeoutSec = 60;
    int err, i;
    const char *responsePrefix = NULL;
    char buf[ARRAY_SIZE * 8] = {0};
    char *response = NULL;
    RespLine *p_cur = NULL;
    CmdResponse *p_outResponse = NULL;

    pthread_mutex_lock(&s_channelMutex[serviceId]);
    err = send_command_full_nolock(serviceId, command, responsePrefix, timeoutSec,
                                   &p_outResponse);
    pthread_mutex_unlock(&s_channelMutex[serviceId]);

    if (s_needEchoCommand && (serviceId == EMBMS)) {
        if (tag == 1) {
            int status = 0;
            char *line = NULL;
            char cmdBuf[ARRAY_SIZE * 8] = {0};
            snprintf(cmdBuf, sizeof(cmdBuf), "%s", command);
            line = cmdBuf;
            at_tok_start_flag(&line, '=');
            at_tok_nextint(&line, &status);
            snprintf(buf, sizeof(buf), "AT+CEREG=%d", status);
            strlcat(buf, "\r\n", sizeof(buf));
        }else {
            strlcat(buf, command, sizeof(buf));
            strlcat(buf, "\r\n", sizeof(buf));
        }
    }

    if (err < 0) {
        strlcat(buf, "ERROR", sizeof(buf));
    } else if (p_outResponse != NULL) {
        if (p_outResponse->success == 0) {
            strlcat(buf, p_outResponse->finalResponse, sizeof(buf));
        } else {
            p_cur = p_outResponse->p_intermediates;
            for (i = 0; p_cur != NULL; p_cur = p_cur->p_next, i++) {
                strlcat(buf, p_cur->line, sizeof(buf));
                strlcat(buf, "\r\n", sizeof(buf));
            }
            strlcat(buf, p_outResponse->finalResponse, sizeof(buf));
        }
    }
    strlcat(buf, "\r\n", sizeof(buf));
    response = buf;
    sendResponse(serviceId, response, strlen(response));

    at_response_free(p_outResponse);
    return err;
}

static int sendAtCmd(int serviceId, char *command) {
    int err;
    int tag = 0;
    char buf[ARRAY_SIZE * 8] = {0};
    char *response = NULL;
    if (serviceId == EMBMS) {
        if (0 != pthread_equal(s_readerThreadTid[serviceId], pthread_self())) {
            return ERROR_INVALID_THREAD;  /* cannot be called from reader thread */
        }
        if (strstr(command, "DEVICE INFO") != NULL) {
            err = getMacAddrInfo(s_deviceInfoPtr);
            if (s_needEchoCommand) {
                strlcat(buf, command, sizeof(buf));
                strlcat(buf, "\r\n", sizeof(buf));
            }
            if (err > 0) {
                RLOGD("[EMBMS]DEVICE INFO s_deviceInfoPtr is %s",s_deviceInfoPtr);
                strlcat(buf, "%MBMSCMD:", sizeof(buf));
                strlcat(buf, s_deviceInfoPtr, sizeof(buf));
                strlcat(buf, "\r\n", sizeof(buf));
                strlcat(buf, "OK", sizeof(buf));
            } else {
                strlcat(buf, "ERROR", sizeof(buf));
            }
            strlcat(buf, "\r\n", sizeof(buf));
            response = buf;
            sendResponse(serviceId, response, strlen(response));
            return 0;
        }
        if (strstr(command, "ENABLE_EMBMS") != NULL) {
            char *line = command;
            int num = -1;
            char ifname[ARRAY_SIZE] = {0};
            snprintf(ifname, sizeof(ifname), "seth_lte7");
            if (s_needEchoCommand) {
                strlcat(buf, command, sizeof(buf));
                strlcat(buf, "\r\n", sizeof(buf));
            }
            skipNextComma(&line);
            if (at_tok_hasmore(&line)) {
                err = at_tok_nextint(&line, &num);
                if (num == 0) {
                    RLOGD("disable seth_lte");
                    ifc_disable(ifname);
                    strlcat(buf, "OK", sizeof(buf));
                } else if (num == 1) {
                    RLOGD("start ifname = %s", ifname);
                    int error = ifc_enable(ifname);
                    RLOGD("end start seth_lte error = %d", error);
                    if (error) {
                        goto error;
                    }
                    RLOGD("[EMBMS]ENABLE_EMBMS");
                    strlcat(buf, "%MBMSCMD:", sizeof(buf));
                    strlcat(buf, ifname, sizeof(buf));
                    strlcat(buf, "\r\n", sizeof(buf));
                    strlcat(buf, "OK", sizeof(buf));
                } else {
                    goto error;
                }
            } else {
                goto error;
            }

            strlcat(buf, "\r\n", sizeof(buf));
            response = buf;
            sendResponse(serviceId, response, strlen(response));
            return 0;

error:
            strlcat(buf, "ERROR", sizeof(buf));
            strlcat(buf, "\r\n", sizeof(buf));
            response = buf;
            sendResponse(serviceId, response, strlen(response));
            return -1;
        }
        if ((strstr(command, "AT+CEREG=") != NULL) &&
            (strstr(command, "AT+CEREG=?") == NULL)) {
            int status = 0;
            tag = 1;
            err = at_tok_start_flag(&command, '=');
            if (err < 0)
                goto out;

            err = at_tok_nextint(&command, &status);
            if (err < 0)
                goto out;

            snprintf(buf, sizeof(buf), "AT+MBMSEREG=%d", status);
            command = buf;
        }

        if (strcmp(command, "ATE1") == 0) {
            RLOGD("[EMBMS]receive ATE1 form EMBMS APP");
            s_needEchoCommand = true;
            strlcat(buf, "OK", sizeof(buf));
            strlcat(buf, "\r\n", sizeof(buf));
            response = buf;
            sendResponse(serviceId, response, strlen(response));
            return 0;
        } else if (strcmp(command, "ATE0") == 0) {
            RLOGD("[EMBMS]receive ATE0 form EMBMS APP");
            s_needEchoCommand = false;
            strlcat(buf, "OK", sizeof(buf));
            strlcat(buf, "\r\n", sizeof(buf));
            response = buf;
            sendResponse(serviceId, response, strlen(response));
            return 0;
        }
    } else if (serviceId == MDT) {
        if (0 != pthread_equal(s_readerThreadTid[serviceId], pthread_self())) {
            return ERROR_INVALID_THREAD;  /* cannot be called from reader thread */
        }
    }

    err = sendCommand(serviceId, command, tag);

out:
    return err;
}

static void *readerThread(void *param) {
    int serviceId = -1;
    int ret, index = 0;
    fd_set rfds;

    if (param) {
        serviceId = *((int*)param);
    }

    if (serviceId < 0 || serviceId >= MAX_SERVICE_NUM) {
        RLOGE("serviceId=%d should be positive and less than %d, return",
               serviceId, MAX_SERVICE_NUM);
        return NULL;
    }

    for (;;) {
        do {
            rfds = s_readFds[serviceId];
            ret = select(s_nfds[serviceId], &rfds, NULL, NULL, NULL);
        } while (ret == -1 && errno == EINTR);
        if (ret > 0) {
            for (index = 0; index < MUX_NUM; index++) {
                if (s_channelInfo[serviceId][index].s_fd != -1 &&
                        FD_ISSET(s_channelInfo[serviceId][index].s_fd, &rfds)) {
                    ChannelInfo *chInfo = &s_channelInfo[serviceId][index];
                    while (1) {
                        if (!readline(chInfo)) break;
                        if (chInfo->line == NULL) break;
                        processLine(serviceId, chInfo);
                    }
                }
            }
        }
    }
    return NULL;
}

static void *mainLoop(void *param) {
    int ret;
    int serviceId = -1;

    if (param) {
        serviceId= *((int*)param);
    }

    if (serviceId < 0 || serviceId >= MAX_SERVICE_NUM) {
        RLOGE("serviceId=%d should be positive and less than %d, return",
               serviceId, MAX_SERVICE_NUM);
        return NULL;
    }

    s_fdListen[serviceId] = -1;

    //s_fdListen[serviceId] = socket_local_server(socket_name,
    //            ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);

    if (serviceId == EMBMS) {
        s_fdListen[serviceId] = android_get_control_socket(s_socketName[serviceId]);
    } else if (serviceId == MDT) {
        s_fdListen[serviceId] = socket_local_server(s_socketName[serviceId],
                    ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
    }

    if (s_fdListen[serviceId] < 0) {
        RLOGE("Failed to get socket %s", s_socketName[serviceId]);
        return NULL;
    }

    ret = listen(s_fdListen[serviceId], 4);
    if (ret < 0) {
        RLOGE("Failed to listen on control socket '%d': %s",
                s_fdListen[serviceId], strerror(errno));
        return NULL;
    }

    for (;;) {
        RLOGD("[%s]mainLoop waiting for client..", s_serviceName[serviceId]);
        if ((s_fdClient[serviceId] = accept(s_fdListen[serviceId], NULL, NULL)) == -1) {
            sleep(1);
            continue;
        }

#if 1
        /* check the credential of the other side and only accept socket from
         * specified process
         */
        struct ucred creds;
        socklen_t szCreds = sizeof(creds);
        struct passwd *pwd = NULL;

        errno = 0;
        int err;

        RLOGD("[%s]mainLoop processName:%s", s_serviceName[serviceId], s_processName[serviceId]);
        err = getsockopt(s_fdClient[serviceId], SOL_SOCKET, SO_PEERCRED, &creds, &szCreds);

        if (err == 0 && szCreds > 0) {
            errno = 0;
            pwd = getpwuid(creds.uid);
            if (pwd != NULL) {
                RLOGD("[%s]pwd->pw_name: [%s]", s_serviceName[serviceId], pwd->pw_name);
//                if (strcmp(pwd->pw_name, processName) == 0) {
//                    is_phone_socket = 1;
//                } else {
//                    RLOGE("EMBMS can't accept socket from process %s", pwd->pw_name);
//                }
            } else {
                RLOGE("Error on getpwuid() errno: %d", errno);
            }
        } else {
            RLOGD("Error on getsockopt() errno: %d", errno);
        }

//        if (!is_phone_socket) {
//            RLOGE("EMBMS must accept socket from %s", processName);
//
//            close(s_fdClient);
//            s_fdClient = -1;
//
//            return NULL;
//        }

#endif

        RLOGD("[%s]mainLoop accept client:%d", s_serviceName[serviceId], s_fdClient[serviceId]);

        s_socketInfo[serviceId].s_fd = s_fdClient[serviceId];
        snprintf(s_socketInfo[serviceId].name, MAX_NAME_LENGTH, "%s", s_socketName[serviceId]);
        memset(s_socketInfo[serviceId].s_respBuffer, 0, MAX_BUFFER_BYTES + 1);
        s_socketInfo[serviceId].s_respBufferCur = s_socketInfo[serviceId].s_respBuffer;
        ChannelInfo *soInfo = &s_socketInfo[serviceId];

        while (1) {
            if (!readline(soInfo)) {
                break;
            }
            if (soInfo->line == NULL) {
                continue;
            }
            sendAtCmd(serviceId, soInfo->line);
        }
        close(s_fdClient[serviceId]);
        s_fdClient[serviceId] = -1;
    }

    return NULL;
}

int service_init(const char *client_Name) {
    int ret = -1, fd = -1, index = 0, retryTimes = 0;
    int serviceId = -1;
    int serviceMuxIndex = -1;
    int serviceMuxNum = -1;
    char propValue[PROPERTY_VALUE_MAX] = {0};
    char muxname[MAX_NAME_LENGTH];
    char *clientName = (char *)client_Name;
    pthread_attr_t attr;
    pthread_condattr_t condattr;

    RLOGD("clientName:%s",clientName);
    if (!clientName || (strlen(clientName) == 0)) {
        RLOGE("clientName:%s shouldn't be NULL or strlen < 0", clientName);
        return -1;
    }

    property_get("ro.vendor.modem.tty", propValue, "/dev/sdiomux");

    if (strcmp(clientName, "embms") == 0) {
        serviceId = EMBMS;
        serviceMuxIndex = MUX_EMBMS_INDEX;
        serviceMuxNum = MUX_EMBMS_NUM;
    } else if (strcmp(clientName, "mdt") == 0) {
        serviceId = MDT;
        serviceMuxIndex = MUX_MDT_INDEX;
        serviceMuxNum = MUX_MDT_NUM;
    }

    if (serviceId < 0 || serviceId >= MAX_SERVICE_NUM) {
        RLOGE("serviceId=%d should be positive and less than %d, return",
               serviceId, MAX_SERVICE_NUM);
        return -1;
    }
retry:
    for (index = 0; index < serviceMuxNum; index++) {
        snprintf(muxname, sizeof(muxname), "%s%d", propValue, index + serviceMuxIndex);
        fd = open(muxname, O_RDWR | O_NONBLOCK);
        if (fd >= 0) {
            RLOGD("[%s]open mux %s successfully", s_serviceName[serviceId], muxname);
        } else if (retryTimes < 5) {
            sleep(1);
            retryTimes++;
            goto retry;
        } else {
            return -1;
        }

        s_channelInfo[serviceId][index].s_fd = fd;
        memset(s_channelInfo[serviceId][index].s_respBuffer, 0, MAX_BUFFER_BYTES + 1);
        s_channelInfo[serviceId][index].s_respBufferCur = s_channelInfo[serviceId][index].s_respBuffer;
        snprintf(s_channelInfo[serviceId][index].name, MAX_NAME_LENGTH, "CMUX%d", index + serviceMuxIndex);
        FD_SET(fd, &s_readFds[serviceId]);
        if (fd >= s_nfds[serviceId]) s_nfds[serviceId] = fd + 1;
    }

    pthread_condattr_init(&condattr);
    pthread_condattr_setclock(&condattr, CLOCK_MONOTONIC);
    pthread_cond_init(&s_channelCond[serviceId], &condattr);
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    RLOGD("[%s]create readerThread", s_serviceName[serviceId]);
    ret = pthread_create(&s_readerThreadTid[serviceId], &attr, readerThread, (void *)&s_funcId[serviceId]);
    RLOGD("[%s]create mainLoop", s_serviceName[serviceId]);
    ret = pthread_create(&s_mainThreadTid[serviceId], &attr, mainLoop, (void *)&s_funcId[serviceId]);

    return 0;
}
