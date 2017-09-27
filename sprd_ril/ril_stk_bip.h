/**
 * ril_stk_bip.h --- stk bip session
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef RIL_STK_BIP_H_
#define RIL_STK_BIP_H_

// BIPSession is used to manage more than more BIP Connections.

#include "ril_stk_parser.h"
#include <telephony/ril_cdma_sms.h>
#include <telephony/librilutils.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <alloca.h>
#include "sprd_atchannel.h"
#include "at_tok.h"
#include "misc.h"
#include <getopt.h>
#include <sys/socket.h>
#include <cutils/sockets.h>
#include <termios.h>
#include <sys/system_properties.h>

#include <telephony/sprd_ril.h>
#include "hardware/qemu_pipe.h"

#define NAME_SIZE                           64

#define MAX_BIP_CHANNELS                    7

#define BEARER_TYPE_CSD                     1
#define BEARER_TYPE_GPRS                    2
#define BEARER_TYPE_DEFAULT                 3
#define BEARER_TYPE_UTRAN_PACKET_SERVICE    11

#define TRANSPORT_TYPE_UDP                  1
#define TRANSPORT_TYPE_TCP                  2

#define ADDRESS_TYPE_IPV4                   0X21
#define ADDRESS_TYPE_IPV6                   0X57
#define TYPE_PACKAGE_DATA_PROTOCOL_IP       "02"

#define BER_UNKNOWN_TAG                     0x00
#define BER_PROACTIVE_COMMAND_TAG           0xd0
#define BER_MENU_SELECTION_TAG              0xd3
#define BER_EVENT_DOWNLOAD_TAG              0xd6

#define CHANNEL_MODE_LINK_DROPPED           5

#define SEND_DATA_STORE                     0
#define SEND_DATA_IMMEDIATELY               1
#define DEFAULT_BUFFER_SIZE                 0x05DC
#define RECEIVE_DATA_MAX_TR_LEN             0xED

#define CHANNEL_MODE_NO_FURTHER_INFO        0
#define DEFAULT_CHANNELID                   0x01
#define DATA_AVAILABLE_EVENT                0x09
#define CHANNEL_STATUS_EVENT                0x0A
#define DEV_ID_UICC                         0x81
#define DEV_ID_TERMINAL                     0x82

#define MAX_BUFFER_BYTES                    (8 * 1024)

#define TYPE_MOBILE                         0
#define TYPE_MOBILE_SUPL                    3

#define OTSRD_SPRD                          0
#define OCRD_SPRD                           1
#define CSRD_SPRD                           2
#define SDRD_SPRD                           3
#define RDRD_SPRD                           4
#define CCRD_SPRD                           5

typedef enum {
    NO_SPECIFIC_CAUSE = 0x00,  // No specific cause can be given;
    SCREEN_BUSY = 0x01,        // Screen is busy;
    BUSY_ON_CALL = 0x02,       // ME currently busy on call;
    BUSY_ON_SS = 0x03,         // ME currently busy on SS transaction;
    NO_SERVICE = 0x04,         // No service;
    ACCESS_BAR = 0x05,         // Access control class bar;
    RADIO_NOT_GRANTED = 0x06,  // Radio resource not granted;
    NOT_IN_SPEECH_CALL = 0x07, // Not in speech call;
    BUSY_ON_USSD = 0x08,       // ME currently busy on USSD transaction;
    BUSY_ON_DTMF = 0x09,       // ME currently busy on SEND DTMF command.
} AddinfoMeProblem;

typedef enum {
    //NO_SPECIFIC_CAUSE = 0x00,            // No specific cause can be given;
    NO_CHANNEL_AVAILABLE = 0x01,           // No channel available;
    CHANNEL_CLOSED = 0x02,                 // Channel closed;
    CHANNEL_ID_INVALID = 0x03,             // Channel identifier not valid;
    BUFFER_SIZE_NOT_AVAILABLE = 0x04,      // Requested buffer size not available;
    SECURITY_ERROR = 0x05,                 // Security error (unsuccessful authentication);
    TRANSPORT_LEVEL_NOT_AVAILABLE = 0x06,  // Requested SIM/ME interface transport level not available;
} AddinfoBIPProblem;

typedef struct {
    unsigned char bearerType;
    char *bearerParam;
    int bufferSize;
    int channelId;
    bool linkStatus;
    int channelLen;
    int dataLen;
    char *dataStr;
} ResponseDataSprd;

enum States{
    OPEN,
    REUSE,
    CLOSE,
};

struct OpenchannelInfo {
    int cid;
    enum States state;//bip use state
    bool pdpState; //pdp setup state
    int count;
};

typedef struct DListNode {
    void *data;
    struct DListNode *prev;
    struct DListNode *next;
} DListNode;

enum BipChannelState {
    BIP_CHANNEL_IDLE,
    BIP_CHANNEL_BUSY,
};

typedef struct _BipClient{
    char *response;
    RIL_SOCKET_ID socket_id;
} BipClient;

typedef struct{
    int result;
    RIL_SOCKET_ID socket_id;
} RefreshPara;

int lunchOpenChannel(int channel_id);
int lunchGetChannelStatus(int channel_id);
int lunchSendData(int channel_id);
int lunchReceiveData(int channel_id);
int lunchCloseChannel(int channel_id);

int SendChannelResponse(StkContext *pstkContext, int resCode, int socket_id);
void sendEventChannelStatus(StkContext *pstkContext, int socket_id);
void sendTerminalResponse(CommandDetails *pCmdDet, CatResponseMessageSprd *pResMsg,
        int socket_id, int respId, ResponseDataSprd *pResp);
void *sendEvenLoopThread(void *param);
void convertBinToHex(char *bin_ptr, int length, unsigned char *hex_ptr);
void convertUcsToUtf8(unsigned char *ucs2, int len, unsigned char *buf);
int convertHexToBin(const char *hex_ptr, int length, char *bin_ptr);
int utf8Package(unsigned char *utf8, int offset, int v);

#endif  // RIL_STK_BIP_H_

