#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <utils/Log.h>
#include <telephony/sprd_ril.h>
#include <sprd_atchannel.h>
#include "sprd_ril_cb.h"
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <alloca.h>
#include "at_tok.h"
#include "misc.h"
#include <getopt.h>
#include <sys/socket.h>
#include <cutils/sockets.h>
#include <termios.h>
#include <sys/system_properties.h>

#include <telephony/sprd_ril.h>
#include "hardware/qemu_pipe.h"

#define LOG_TAG "RIL"
#define MAX_PDP 6

extern void putPDP(int cid);
extern SIM_Status getSIMStatus(int channelID);

static void onRequest (int request, void *data, size_t datalen, RIL_Token t);

static const RIL_RadioFunctions atch_RadioFuncs = {
    RIL_VERSION,
    onRequest,
    currentState,
    onSupports,
    onCancel,
    getVersion
};
int s_vsimClientFd = -1;
int s_vsimServerFd = -1;
bool s_vsimListenLoop = false;
bool s_vsimInitFlag = false;
pthread_mutex_t s_vsimSocketMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t s_vsimSocketCond = PTHREAD_COND_INITIALIZER;
struct timeval s_timevalCloseVsim = {60, 0};

static const struct RIL_Env *atch_rilenv;
#define sOnRequestComplete(t, e, response, responselen) atch_rilenv->OnRequestComplete(t,e, response, responselen)

#define RIL_REQUEST_SEND_CMD 1

const RIL_RadioFunctions *RIL_ATCI_Init(const struct RIL_Env *env, int argc, char **argv) {
    atch_rilenv = env;
    return &atch_RadioFuncs;
}

int vsimQueryVirtual(int socket_id){
    RLOGD("vsimQueryVirtual, phoneId: %d", socket_id);
    int err = -1;
    int channelID = -1;
    int vsimMode = -1;
    char *line = NULL;
    ATResponse *p_response = NULL;
    channelID = getChannel(socket_id);
    err = at_send_command_singleline(ATch_type[channelID],
            "AT+VIRTUALSIMINIT?", "+VIRTUALSIMINIT:", &p_response);
    if (err < 0 || p_response->success == 0) {
        RILLOGD("vsim query virtual card error");
    } else {
        line = p_response->p_intermediates->line;
        RILLOGD("vsim query virtual card resp:%s",line);
        err = at_tok_start(&line);
        err = at_tok_nextint(&line, &vsimMode);
    }
    at_response_free(p_response);
    if (vsimMode > 0) {
        at_send_command(ATch_type[channelID],"AT+RSIMRSP=\"ERRO\",1,", NULL);
    }
    putChannel(channelID);

    return vsimMode;
}
void onSimDisabled(int channelID) {

    int i;
    at_send_command(ATch_type[channelID], "AT+SFUN=5", NULL);
    for(i = 0; i < MAX_PDP; i++) {
        if (getPDPCid(i) > 0) {
            RILLOGD("pdp[%d].state = %d", i, getPDPState(i));
            putPDP(i);
        }
    }
    setRadioState(channelID, RADIO_STATE_OFF);
}

int closeVirtual(int socket_id){
    RLOGD("closeVirtual, phoneId: %d", socket_id);
    int err = -1;
    int channelID = -1;
    channelID = getChannel(socket_id);

    err = at_send_command(ATch_type[channelID],"AT+RSIMRSP=\"VSIM\",0", NULL);
    onSimDisabled(channelID);

    putChannel(channelID);
    return err;
}

static void closeVirtualThread(void *param) {
    RILLOGD("closeVsimCard");
    int socket_id = *((int *)param);
    if ((s_vsimClientFd < 0) && (socket_id == 1)) {
        if (modem == 0) {
            closeVirtual(RIL_SOCKET_1);
        } else if (modem == 1) {
            closeVirtual(RIL_SOCKET_2);
        }
    }
}

void *listenVsimSocketThread() {
    int ret = -1;
    char socket_name[20];

    //sim0: modem = 0; sim1: modem = 1.
    memset(socket_name, 0, sizeof(char) * 20);
    snprintf(socket_name, sizeof(socket_name), "vsim_socket_%c", modem);
    RILLOGD("Start to listen %s", socket_name);

    if (s_vsimServerFd < 0) {
        s_vsimServerFd = socket_local_server(socket_name,
               ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
        if (s_vsimServerFd < 0) {
            RILLOGE("Failed to get socket %s", socket_name);
        }

        ret = listen(s_vsimServerFd, 1);
        if (ret < 0) {
            RILLOGE("Failed to listen on control socket '%d': %s",
                    s_vsimServerFd, strerror(errno));
        }
    }

   s_vsimClientFd = accept(s_vsimServerFd, NULL, NULL);
    pthread_mutex_lock(&s_vsimSocketMutex);
    pthread_cond_signal(&s_vsimSocketCond);
    s_vsimListenLoop = true;
    pthread_mutex_unlock(&s_vsimSocketMutex);
    RILLOGD("vsim connected %d",s_vsimClientFd);
    do {
        char error[128] = {0};
        RILLOGD("vsim read begin");
        if (TEMP_FAILURE_RETRY(read(s_vsimClientFd, &error, sizeof(error)))
                <= 0) {
            RILLOGE("read error from vsim! err = %s",strerror(errno));
            close(s_vsimClientFd);
            s_vsimListenLoop = false;
            s_vsimClientFd = -1;

            int vsimMode = -1;
            if (modem == 0) {
                vsimMode = vsimQueryVirtual(RIL_SOCKET_1);
            } else {
                vsimMode = vsimQueryVirtual(RIL_SOCKET_2);
            }

            RIL_requestTimedCallback(closeVirtualThread,
                        (void *)&vsimMode, &s_timevalCloseVsim);
        }
        RILLOGD("vsim read %s",error);
    } while (s_vsimClientFd > 0);
    return NULL;
}

void vsimInit() {
    int ret;
    pthread_t tid;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&tid, &attr, (void *)listenVsimSocketThread, NULL);
    if (ret < 0) {
        RILLOGE("Failed to create listen_vsim_socket_thread errno: %d", errno);
    }
}

void* sendVsimReqThread(void *cmd) {
    RLOGD("vsim write cmd = %s", cmd);
    if (s_vsimClientFd >= 0) {
        int len = strlen((char *)cmd);
        RILLOGD("vsim write cmd len= %d", len);
        if (TEMP_FAILURE_RETRY(write(s_vsimClientFd, cmd, len)) !=
                                      len) {
            RILLOGE("Failed to write cmd to vsim!error = %s", strerror(errno));
            close(s_vsimClientFd);
            s_vsimClientFd = -1;
            if (modem == 0) {
                vsimQueryVirtual(RIL_SOCKET_1);
            } else {
                vsimQueryVirtual(RIL_SOCKET_2);
            }
        }
        RILLOGD("vsim write OK");
    } else {
        RILLOGE("vsim socket disconnected");
    }
    free(cmd);
    return NULL;
}

void sendVsimReq(char *cmd) {
    int ret;
    pthread_t tid;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&tid, &attr, (void *)sendVsimReqThread, (void *)cmd);
    if (ret < 0) {
        RILLOGE("Failed to create sendVsimReqThread errno: %d", errno);
    }
}
static void onRequest (int request, void *data, size_t datalen, RIL_Token t) {
    int i, err;
    int channelID;
    ATResponse *p_response = NULL;
    ATLine *p_cur = NULL;
    char *at_cmd = (char *)data;
    char buf[MAX_AT_RESPONSE] = {0};
    char *cmd;
    char *pdu;
    char *response[1]={NULL};

    if(request == RIL_REQUEST_SEND_CMD) {

        if(at_cmd == NULL) {
            RILLOGE("Invalid AT command");
            return;
        }
        channelID = getChannel();
        if (strStartsWith(at_cmd, "VSIM_CREATE")) {
            //int socket_id = getSocketIdByChannelID(channelID);
            s_vsimInitFlag = true;
            //create socket
            if (!s_vsimListenLoop) {
                vsimInit();
                response[0] = "OK";
                sOnRequestComplete(t, RIL_E_SUCCESS, response, sizeof(char *));
            } else {
                response[0] = "ERROR";
                sOnRequestComplete(t, RIL_E_GENERIC_FAILURE, response, sizeof(char *));
            }
            return;
         } else if (strStartsWith(at_cmd, "VSIM_INIT")) {
             char *cmd = NULL;
             RILLOGD("wait for vsim socket connect");
             pthread_mutex_lock(&s_vsimSocketMutex);
             while (s_vsimClientFd < 0) {
                 pthread_cond_wait(&s_vsimSocketCond, &s_vsimSocketMutex);
             }
             pthread_mutex_unlock(&s_vsimSocketMutex);
             RILLOGD("vsim socket connected");
             //send AT
             cmd = at_cmd;
             at_tok_start(&cmd);
             err = at_send_command(ATch_type[channelID], cmd, &p_response);
         } else if (strStartsWith(at_cmd, "VSIM_EXIT")) {
             char *cmd = NULL;
             //int socket_id = getSocketIdByChannelID(channelID);
             s_vsimInitFlag = false;

             //send AT
             cmd = at_cmd;
             at_tok_start(&cmd);
             int sim_status = getSIMStatus(channelID);
             if (sim_status != SIM_ABSENT) {
             	at_send_command(ATch_type[channelID], "AT+RSIMRSP=\"ERRO\",2,", &p_response);
             }
             err = at_send_command(ATch_type[channelID], cmd, &p_response);
             if (err < 0 || p_response->success == 0) {
                 if (p_response != NULL) {
                     strlcat(buf, p_response->finalResponse, sizeof(buf));
                     strlcat(buf, "\r\n", sizeof(buf));
                     response[0] = buf;
                     sOnRequestComplete(t, RIL_E_GENERIC_FAILURE, response,
                                           sizeof(char *));
                 } else {
                     goto error;
                 }
             } else {
     //#if (SIM_COUNT >= 2)
     //        if ((!s_vsimInitFlag[RIL_SOCKET_1]) && (!s_vsimInitFlag[RIL_SOCKET_2]))
     //#else
             if (!s_vsimInitFlag)
     //#endif
                 {
                     if (s_vsimClientFd != -1) {
                         close(s_vsimClientFd);
                         s_vsimClientFd = -1;
                     }
                     s_vsimListenLoop = false;
                 }
                 onSimDisabled(channelID);
                 strlcat(buf, p_response->finalResponse, sizeof(buf));
                 strlcat(buf, "\r\n", sizeof(buf));
                 response[0] = buf;
                 sOnRequestComplete(t, RIL_E_SUCCESS, response, sizeof(char *));
             }
             at_response_free(p_response);
             return;
        }else if (strStartsWith(at_cmd, "VSIM_TIMEOUT")) {
            int time = -1;
            char *cmd = NULL;
            cmd = at_cmd;
            at_tok_start(&cmd);
            err = at_tok_nextint(&cmd, &time);
            RLOGD("VSIM_TIMEOUT:%d",time);
            if (time > 0) {
                s_timevalCloseVsim.tv_sec = time;
                response[0] = "OK";
                sOnRequestComplete(t, RIL_E_SUCCESS, response, sizeof(char *));
            } else {
                response[0] = "ERROR";
                sOnRequestComplete(t, RIL_E_GENERIC_FAILURE, response, sizeof(char *));
            }
            return;
        } else {
            err = at_send_command_multiline(ATch_type[channelID], at_cmd, "", &p_response);
        }

        if (err < 0 || p_response->success == 0) {
            strlcat(buf, p_response->finalResponse, sizeof(buf));
            strlcat(buf, "\r\n", sizeof(buf));
            response[0] = buf;
            sOnRequestComplete(t, RIL_E_GENERIC_FAILURE, response, sizeof(char*));
        } else {
            p_cur = p_response->p_intermediates;
            for (i=0; p_cur != NULL; p_cur = p_cur->p_next,i++) {
                strlcat(buf, p_cur->line, sizeof(buf));
                strlcat(buf, "\r\n", sizeof(buf));
            }
            strlcat(buf, p_response->finalResponse, sizeof(buf));
            strlcat(buf, "\r\n", sizeof(buf));
            response[0] = buf;
            sOnRequestComplete(t, RIL_E_SUCCESS, response, sizeof(char*));
        }
        at_response_free(p_response);
        putChannel(channelID);
        return;
error:
    memset(buf, 0 ,sizeof(buf));
    strlcat(buf, "ERROR", sizeof(buf));
    strlcat(buf, "\r\n", sizeof(buf));
    response[0] = buf;
    sOnRequestComplete(t, RIL_E_GENERIC_FAILURE, response,
                                  sizeof(char *));
    at_response_free(p_response);
    putChannel(channelID);
    }
}

