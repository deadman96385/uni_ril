/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <private/android_filesystem_config.h>

#define LOG_TAG "CnDaemon"

#include <cutils/sockets.h>
#include <cutils/log.h>

/**
 * Native service which communicates with apk via the socket named "iptablesserver".
 * It executes iptables commands received from apk.
 * If some command is failed to be excuted,then print out the detailed cause.
 */

#define MAX_CMD_LENGTH (200)
#define LINE_LENGTH (500)
#define RESPONSE_LENGTH (50)

const char* debugFile = "/data/data/com.sprd.customizedNet/iptablesDebug.txt";
const char* failureOutput = " 2>> /data/data/com.sprd.customizedNet/iptablesDebug.txt";

void appendDetailErrors() {
    FILE *fp;
    char lineBuf[LINE_LENGTH];
    int len;

    if((fp=fopen(debugFile,"r"))==NULL) {
         ALOGD("debugFile not exist");
         return;
    }

    memset(lineBuf, 0 , LINE_LENGTH);
    while(fgets(lineBuf,LINE_LENGTH,fp) != NULL) {

        len = strlen(lineBuf);
        //delete \n
        lineBuf[len-1] = '\0';
        ALOGE("appendDetailErrors: detailed cause is %s", lineBuf);
        memset(lineBuf, 0 , LINE_LENGTH);
    }
    fclose(fp);

    //flush file
    fp = fopen(debugFile, "w+");
    fclose(fp);
}

void print_error(pid_t status, char *sysError) {

   if (-1 != status) {
        if (WIFEXITED(status) && (0 == WEXITSTATUS(status))) {
            ALOGD("run command success!");
            strncpy(sysError,"execute sucess",strlen("execute sucess"));
            return;
        }
    }

    //ALOGD("run command fail!");
    strncpy(sysError,"execute exception",strlen("execute exception"));
    appendDetailErrors();

    return;
}

void excuteRules(char *strCmd, char *result) {
    pid_t ret;

    if (0 == strlen(strCmd)) {
        ALOGD("excuteRules: cmd string is empty! ");
        return;
    }

    ret =  system(strCmd);
    print_error(ret, result);
    strncat(result, "\0", strlen("\0"));

    if (strstr(result, "exception")) {
        ALOGD("The failed cmd is %s",strCmd);
    }

    return;
}

int readx(int s, void *_buf, int count) {
    char *buf =(char *) _buf;
    int n = 0, r;

    if (count < 0) return -1;

    while (n < count) {
        r = read(s, buf + n, count - n);
        if (r < 0) {
            if (errno == EINTR) continue;
            ALOGE("read error: %s\n", strerror(errno));
            return -1;
        }
        if (r == 0) {
            ALOGE("eof\n");
            return -1; /* EOF */
        }
        n += r;
    }
    return 0;
}

int blockingWrite(int fd, const void *buffer, size_t len) {
    size_t writeOffset = 0;
    const uint8_t *toWrite;

    toWrite = (const uint8_t *)buffer;

    while (writeOffset < len) {
        ssize_t written;
        do {
            written = write (fd, toWrite + writeOffset,
                                len - writeOffset);
        } while (written < 0 && ((errno == EINTR) || (errno == EAGAIN)));

        if (written >= 0) {
            writeOffset += written;
        } else {
            ALOGE ("blockingWrite: unexpected error, errno:%d", errno);
            close(fd);
            return -1;
        }
    }

    return 0;
}

int sendResponse (int fd, char *data, size_t dataSize) {
    int ret;
    uint32_t header;

    if (fd < 0) {
        return -1;
    }

    header = htonl(dataSize);

    ret = blockingWrite(fd, (void *)&header, sizeof(header));

    if (ret < 0) {
        ALOGE(" blockingWrite header error");
        return ret;
    }

    ret = blockingWrite(fd, data, dataSize);

    if (ret < 0) {
        ALOGE("blockingWrite data error");
        return ret;
    }

    return 0;
}


int main() {

    char exec_result[RESPONSE_LENGTH];
    int connect_number = 4;
    int fdListen = -1, new_fd = -1;
    int status = 0xff;
    int ret_val = 0;
    struct sockaddr peeraddr;
    socklen_t socklen = sizeof (peeraddr);
    char buff[MAX_CMD_LENGTH];
    fd_set fds;
    FILE *fp;
    int err = -1;
    int rcv_size = 0;
    socklen_t optlen;

    if((fp=fopen(debugFile,"w+")) == NULL) {
        ALOGE("creat or flush debugFile failure");
        goto ERROR;
    }
    fclose(fp);

     //creat server socket
     fdListen = android_get_control_socket("iptablesserver");
     if (fdListen < 0) {
         ALOGE("Failed to get socket dsserver");
         goto ERROR;
     }

     status = listen(fdListen, connect_number);
     if (status < 0) {
         ALOGE(" socket iptablesserver listen failed");
         goto ERROR;
     }

    for (;;) {
         new_fd = accept(fdListen, (sockaddr *) &peeraddr, &socklen);
         if (new_fd < 0 ) {
             ALOGE("Error on accept() ");
             continue;
         }
         ALOGD("new_fd = %d",new_fd);

         //get recv_buff size
         optlen = sizeof(rcv_size);
         err = getsockopt(new_fd, SOL_SOCKET, SO_RCVBUF, &rcv_size, &optlen);
         if(err<0){
             ALOGD("get recvBuff ERROR");
         } else {
             ALOGD("recvBuff is %d ",rcv_size);
         }

         for (;;) {
             FD_ZERO(&fds);
             FD_SET(new_fd,&fds);

             ret_val = select(new_fd+1, &fds, NULL, NULL, NULL);
             if(ret_val > 0 ) {
                 if(FD_ISSET(new_fd, &fds)) {
                     unsigned short count = 0;
                     memset(buff, 0, MAX_CMD_LENGTH);

                     if (readx(new_fd, &count, sizeof(count))) {
                         ALOGE("failed to read size\n");
                         break;
                     }
                     if (!(count > 0 && count <= MAX_CMD_LENGTH)) {
                         ALOGE("bytes number error");
                         break;
                     }
                     if (readx(new_fd, buff, count)) {
                         ALOGE("failed to read command\n");
                         break;
                     }

                     //ALOGD("receive iptables cmd %s",buff);
                     strncat(buff, failureOutput, strlen(failureOutput));

                     memset(exec_result,0,RESPONSE_LENGTH);
                     excuteRules(buff, exec_result);

                   }
             }
             else
             {
                 ALOGE("Select error or timeout, ret_val = %d", ret_val);
                 break;
             }
         }

         close(new_fd);
         new_fd = -1;
         memset(exec_result,0,RESPONSE_LENGTH);
    }

ERROR:
    return 0;
}
