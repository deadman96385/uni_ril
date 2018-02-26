/**
 * ril_stk_parser.h --- stk TLV parser
 *
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 */


#ifndef RIL_STK_PARSER_H_
#define RIL_STK_PARSER_H_

#include <stdbool.h>
#include "sprd_ril.h"

/*
#define RESULT_OK 0
#define RESULT_FAILED 1
#define RESULT_UNHANDLED RESULT_FAILED
#define RESULT_SIM_ERROR_UNEXPECTED_CMD 2
#define RESULT_SIM_ERROR_BAD_DATA 3
#define RESULT_CMD_TYPE_NOT_UNDERSTOOD 4
#define BEYOND_TERMINAL_CAPABILITY 5
*/
#define RESULT_EXCEPTION -1
#define PROACTIVE_COMMAND_TAG 0xD0

#define MAX_BUFFER_BYTES     (8 * 1024)
/**
 * Enumeration for the return code in TERMINAL RESPONSE.
 * To get the actual return code for each enum value, call {@link #value}
 * method.
 *
 * {@hide}
 */
typedef enum {

    /*
     * Results '0X' and '1X' indicate that the command has been performed.
     */
    /** Command performed successfully */
    OK = 0x00,

    /** Command performed with partial comprehension */
    PRFRMD_WITH_PARTIAL_COMPREHENSION = 0x01,

    /** Command performed, with missing information */
    PRFRMD_WITH_MISSING_INFO = 0x02,

    /** REFRESH performed with additional EFs read */
    PRFRMD_WITH_ADDITIONAL_EFS_READ = 0x03,

    /**
     * Command performed successfully, but requested icon could not be
     * displayed
     */
    PRFRMD_ICON_NOT_DISPLAYED = 0x04,

    /** Command performed, but modified by call control by NAA */
    PRFRMD_MODIFIED_BY_NAA = 0x05,

    /** Command performed successfully, limited service */
    PRFRMD_LIMITED_SERVICE = 0x06,

    /** Command performed with modification */
    PRFRMD_WITH_MODIFICATION = 0x07,

    /** REFRESH performed but indicated NAA was not active */
    PRFRMD_NAA_NOT_ACTIVE = 0x08,

    /** Command performed successfully, tone not played */
    PRFRMD_TONE_NOT_PLAYED = 0x09,

    /** Proactive UICC session terminated by the user */
    UICC_SESSION_TERM_BY_USER = 0x10,

    /** Backward move in the proactive UICC session requested by the user */
    BACKWARD_MOVE_BY_USER = 0x11,

    /** No response from user */
    NO_RESPONSE_FROM_USER = 0x12,

    /** Help information required by the user */
    HELP_INFO_REQUIRED = 0x13,

    /** USSD or SS transaction terminated by the user */
    USSD_SS_SESSION_TERM_BY_USER = 0x14,


    /*
     * Results '2X' indicate to the UICC that it may be worth re-trying the
     * command at a later opportunity.
     */

    /** Terminal currently unable to process command */
    TERMINAL_CRNTLY_UNABLE_TO_PROCESS = 0x20,

    /** Network currently unable to process command */
    NETWORK_CRNTLY_UNABLE_TO_PROCESS = 0x21,

    /** User did not accept the proactive command */
    USER_NOT_ACCEPT = 0x22,

    /** User cleared down call before connection or network release */
    USER_CLEAR_DOWN_CALL = 0x23,

    /** Action in contradiction with the current timer state */
    CONTRADICTION_WITH_TIMER = 0x24,

    /** Interaction with call control by NAA, temporary problem */
    NAA_CALL_CONTROL_TEMPORARY = 0x25,

    /** Launch browser generic error code */
    LAUNCH_BROWSER_ERROR = 0x26,

    /** MMS temporary problem. */
    MMS_TEMPORARY = 0x27,


    /*
     * Results '3X' indicate that it is not worth the UICC re-trying with an
     * identical command, as it will only get the same response. However, the
     * decision to retry lies with the application.
     */

    /** Command beyond terminal's capabilities */
    BEYOND_TERMINAL_CAPABILITY = 0x30,

    /** Command type not understood by terminal */
    CMD_TYPE_NOT_UNDERSTOOD = 0x31,

    /** Command data not understood by terminal */
    CMD_DATA_NOT_UNDERSTOOD = 0x32,

    /** Command number not known by terminal */
    CMD_NUM_NOT_KNOWN = 0x33,

    /** SS Return Error */
    SS_RETURN_ERROR = 0x34,

    /** SMS RP-ERROR */
    SMS_RP_ERROR = 0x35,

    /** Error, required values are missing */
    REQUIRED_VALUES_MISSING = 0x36,

    /** USSD Return Error */
    USSD_RETURN_ERROR = 0x37,

    /** MultipleCard commands error */
    MULTI_CARDS_CMD_ERROR = 0x38,

    /**
     * Interaction with call control by USIM or MO short message control by
     * USIM, permanent problem
     */
    USIM_CALL_CONTROL_PERMANENT = 0x39,

    /** Bearer Independent Protocol error */
    BIP_ERROR = 0x3a,

    /** Access Technology unable to process command */
    ACCESS_TECH_UNABLE_TO_PROCESS = 0x3b,

    /** Frames error */
    FRAMES_ERROR = 0x3c,

    /** MMS Error */
    MMS_ERROR = 0x3d,

} ResultCode;

/**
 * Enumeration for representing the tag value of COMPREHENSION-TLV objects. If
 * you want to get the actual value, call {@link #value() value} method.
 *
 * {@hide}
 */
typedef enum {
    COMMAND_DETAILS = 0x01,
    DEVICE_IDENTITIES = 0x02,
    RESULT = 0x03,
    DURATION = 0x04,
    ALPHA_ID = 0x05,
    ADDRESS = 0x06,
    USSD_STRING = 0x0a,
    SMS_TPDU = 0x0b,
    TEXT_STRING = 0x0d,
    TONE = 0x0e,
    ITEM = 0x0f,
    ITEM_ID = 0x10,
    RESPONSE_LENGTH = 0x11,
    FILE_LIST = 0x12,
    HELP_REQUEST = 0x15,
    DEFAULT_TEXT = 0x17,
    EVENT_LIST = 0x19,
    ICON_ID = 0x1e,
    ITEM_ICON_ID_LIST = 0x1f,
    IMMEDIATE_RESPONSE = 0x2b,
    DTMF = 0x2c, //SPRD: Add DTMF Tag.
    LANGUAGE = 0x2d,
    URL = 0x31,
    BROWSER_TERMINATION_CAUSE = 0x34,
    BEARER_DESCRIPTION = 0x35,
    CHANNEL_DATA = 0x36,
    CHANNEL_DATA_LENGTH = 0x37,
    CHANNEL_STATUS = 0x38,
    BUFFER_SIZE = 0x39,
    TRANSPORT_LEVEL = 0x3c,
    OTHER_ADDRESS = 0x3e,
    NETWORK_ACCESS_NAME = 0x47,
    TEXT_ATTRIBUTE = 0x50,
} ComprehensionTlvTag;

/*
 * Enumeration for representing "Type of Command" of proactive commands.
 * Those are the only commands which are supported by the Telephony. Any app
 * implementation should support those.
 * Refer to ETSI TS 102.223 section 9.4
 */
typedef enum {
    OPEN_CHANNEL = 0x40,
    CLOSE_CHANNEL = 0x41,
    RECEIVE_DATA = 0x42,
    SEND_DATA = 0x43,
    GET_CHANNEL_STATUS = 0x44,
    REFRESH = 0x01,
} CommandType;

/**
 * structure for representing COMPREHENSION-TLV objects.
 *
 * @see "ETSI TS 101 220 subsection 7.1.1"
 *
 */
typedef struct {
    int tag;
    int cr;
    int length;
    char *value;
} TLV;

/**
 * Parses an COMPREHENSION-TLV object from a byte array.
 *
 * @param data A byte array containing data to be parsed
 * @param pTlv A COMPREHENSION-TLV object parsed
 * The result: decode procedure.
 */
typedef struct ComprehensionTlv {
    TLV tTlv;
    struct ComprehensionTlv *next;
} ComprehensionTlv;

typedef struct {
    int tag;
    ComprehensionTlv *compTlvs;
    bool lengthValid;
} BerTlv;

typedef struct {
    int commandNumber;
    int typeOfCommand;
    int commandQualifier;
    bool compRequired;
} CommandDetails;

typedef struct {
    int recordNumber;
    bool selfExplanatory;
} IconId;

typedef struct {
    int sourceId;
    int destinationId;
} DeviceIdentities;

typedef struct {
    int openChannelType;
    unsigned char *text;
    bool iconSelfExplanatory;
    bool isNullAlphaId;
} ChannelData;

typedef struct {
    int alphaIdLength;
    char NetAccessName[ARRAY_SIZE * 2];
    int bufferSize;
    unsigned char BearerType;
    char BearerParam[ARRAY_SIZE];
    char OtherAddressType;
    unsigned char OtherAddress[ARRAY_SIZE];
    unsigned char LoginStr[ARRAY_SIZE / 4];
    unsigned char PwdStr[ARRAY_SIZE / 4];
    unsigned char transportType;
    int portNumber;
    unsigned char DataDstAddressType;
    char DataDstAddress[ARRAY_SIZE];
    ChannelData channelData;
} OpenChannelData;

typedef struct {
    DeviceIdentities deviceIdentities;
    ChannelData channelData;
    int channelId;
    unsigned char *sendDataStr;
} SendChannelData;

typedef struct {
    DeviceIdentities deviceIdentities;
    ChannelData channelData;
    int channelId;
    int channelDataLength;
} ReceiveChannelData;

typedef struct {
    DeviceIdentities deviceIdentities;
    ChannelData channelData;
    int channelId;
} CloseChannelData;

typedef struct {
    char *rawData;
    ResultCode resCode;
} RilMessage;

typedef struct _CatResponseMessageSprd {
    CommandDetails cmdDet;
    ResultCode resCode ;
    int usersMenuSelection;
    char *usersInput;
    bool usersYesNoSelection;
    bool usersConfirm;
    bool includeAdditionalInfo;
    int additionalInfo;
    int eventValue;
    unsigned char *addedInfo;
    /* SPRD: Add here for BIP function @{ */
    unsigned char BearerType;
    char *BearerParam;
    int bufferSize;
    int ChannelId;
    bool LinkStatus;
    int channelDataLen;
    char *channelData;
    int mode;
}CatResponseMessageSprd;

typedef struct _ChannelStatus {
    int channelId;
    int mode_info;
    bool linkStatus;
}ChannelStatus;

typedef struct _StkContext {
    int phone_id;
    int channel_id;

    int tcpSocket;
    int udpSocket;

    int openchannelCid;

    int connectType;

    int sendDataLen;
    int receiveDataLen;
    int receiveDataOffset;

    bool needRuning;
    bool threadInLoop;
    bool cmdInProgress;
    bool channelEstablished;

    char sendDataStr[MAX_BUFFER_BYTES];
    char sendDataStorer[MAX_BUFFER_BYTES];
    char *recevieData;

    pthread_t openChannelTid;
    pthread_t sendDataTid;
    pthread_mutex_t writeStkMutex;
    pthread_mutex_t receiveDataMutex;
    pthread_mutex_t closeChannelMutex;
    pthread_mutex_t bipTcpUdpMutex;

    ChannelStatus channelStatus;
    ChannelStatus lastChannelStatus;
    OpenChannelData *pOCData;
    ChannelStatus *pCStatus;
    SendChannelData *pSCData;
    ReceiveChannelData *pRCData;
    CloseChannelData *pCCData;

    CommandDetails *pCmdDet;
}StkContext;

typedef struct _StkContextList{
    StkContext *pstkcontext;
    struct _StkContextList *prev;
    struct _StkContextList *next;
}StkContextList;

/**
 * The parser for the STK command.
 * The intent of this function is to filter out all of BIP command.
 * The result:
 *       0: handled by RIL BIP Session and no need to pass this unsolicited response to client.
 *       1: need to pass this unsolicited response to client
 *      -1: exception in this function.
 **/
int parseProCommand(const char * const rawData, const int length, BerTlv *berTlv);
int processCommandDetails(ComprehensionTlv *comprehensionTlv, CommandDetails *cmdDet);
void processOpenChannel(ComprehensionTlv *comprehensionTlv, OpenChannelData *openChannelData, RilMessage *rilMessage);
void processSendData(ComprehensionTlv *comprehensionTlv, SendChannelData *sendData, RilMessage *rilMessage);
void processReceiveData(ComprehensionTlv *comprehensionTlv, ReceiveChannelData *receiveData, RilMessage *rilMessage);
void processCloseChannel(ComprehensionTlv *comprehensionTlv, CloseChannelData *closeChannelDatat, RilMessage *rilMessage);
void processGetChannelStatus(RilMessage *rilMessage);
StkContext* getStkContext(int channel_id);

#endif  //RIL_STK_PARSER_H_
