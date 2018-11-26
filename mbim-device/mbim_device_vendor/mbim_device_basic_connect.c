#define LOG_TAG "MBIM-Device"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <utils/Log.h>

#include "mbim_message_processer.h"
#include "mbim_service_basic_connect.h"
#include "mbim_device_basic_connect.h"
#include "mbim_device_vendor.h"
#include "time_event_handler.h"
#include "at_tok.h"
#include "mbim_utils.h"
#include "parse_apn.h"

#define TYPE_FCP                                0x62
#define RESPONSE_EF_SIZE                        15
#define TYPE_FILE_DES_LEN                       5
#define RESPONSE_DATA_FCP_FLAG                  0
#define RESPONSE_DATA_FILE_DES_FLAG             2
#define RESPONSE_DATA_FILE_DES_LEN_FLAG         3
#define RESPONSE_DATA_FILE_RECORD_LEN_1         6
#define RESPONSE_DATA_FILE_RECORD_LEN_2         7
#define EF_TYPE_TRANSPARENT                     0x01
#define EF_TYPE_LINEAR_FIXED                    0x02
#define EF_TYPE_CYCLIC                          0x06
#define USIM_DATA_OFFSET_2                      2
#define USIM_DATA_OFFSET_3                      3
#define USIM_FILE_DES_TAG                       0x82
#define USIM_FILE_SIZE_TAG                      0x80

#define COMMAND_READ_BINARY                     0xb0
#define COMMAND_UPDATE_BINARY                   0xd6
#define COMMAND_READ_RECORD                     0xb2
#define COMMAND_UPDATE_RECORD                   0xdc
#define COMMAND_SEEK                            0xa2
#define COMMAND_GET_RESPONSE                    0xc0
#define READ_RECORD_MODE_ABSOLUTE               4
#define GET_RESPONSE_EF_SIZE_BYTES              15
#define GET_RESPONSE_EF_IMG_SIZE_BYTES          10
#define DF_ADF                                  "3F007FFF"
#define DF_GSM                                  "3F007F20"
#define DF_TELECOM                              "3F007F10"
#define SIGNAL_STRENGTH_INTERVAL                5

static MbimRegisterState   s_regState =             MBIM_REGISTER_STATE_UNKNOWN;
static MbimDataClass       s_availableDataClasses = MBIM_DATA_CLASS_NONE;
static RIL_AppType         s_appType =              RIL_APPTYPE_UNKNOWN;

static char s_subscriberId[16] = {0};
static char s_providerId[7] = {0};

static int      s_activePDN;
static IP_INFO  s_ipInfo[MAX_PDP];
static MbimSubscriberReadyState s_simState = MBIM_SUBSCRIBER_READY_STATE_NOT_INITIALIZED;
static MbimPinType s_pinType = MBIM_PIN_TYPE_UNKNOWN;
static MbimPinState s_pinState = MBIM_PIN_STATE_UNLOCKED;

/* order by sessionId */
static PDPInfo  s_PDP[MAX_PDP] = {
    {-1, -1, -1, false, PDP_IDLE, MBIM_CONTEXT_TYPE_INVALID, PTHREAD_MUTEX_INITIALIZER},
    {-1, -1, -1, false, PDP_IDLE, MBIM_CONTEXT_TYPE_INVALID, PTHREAD_MUTEX_INITIALIZER},
    {-1, -1, -1, false, PDP_IDLE, MBIM_CONTEXT_TYPE_INVALID, PTHREAD_MUTEX_INITIALIZER},
    {-1, -1, -1, false, PDP_IDLE, MBIM_CONTEXT_TYPE_INVALID, PTHREAD_MUTEX_INITIALIZER},
    {-1, -1, -1, false, PDP_IDLE, MBIM_CONTEXT_TYPE_INVALID, PTHREAD_MUTEX_INITIALIZER},
    {-1, -1, -1, false, PDP_IDLE, MBIM_CONTEXT_TYPE_INVALID, PTHREAD_MUTEX_INITIALIZER}
};

static PDNInfo s_PDN[MAX_PDP_CP] = {
    { -1, MBIM_CONTEXT_IP_TYPE_DEFAULT, MBIM_CONTEXT_TYPE_INVALID, ""},
    { -1, MBIM_CONTEXT_IP_TYPE_DEFAULT, MBIM_CONTEXT_TYPE_INVALID, ""},
    { -1, MBIM_CONTEXT_IP_TYPE_DEFAULT, MBIM_CONTEXT_TYPE_INVALID, ""},
    { -1, MBIM_CONTEXT_IP_TYPE_DEFAULT, MBIM_CONTEXT_TYPE_INVALID, ""},
    { -1, MBIM_CONTEXT_IP_TYPE_DEFAULT, MBIM_CONTEXT_TYPE_INVALID, ""},
    { -1, MBIM_CONTEXT_IP_TYPE_DEFAULT, MBIM_CONTEXT_TYPE_INVALID, ""},
    { -1, MBIM_CONTEXT_IP_TYPE_DEFAULT, MBIM_CONTEXT_TYPE_INVALID, ""},
    { -1, MBIM_CONTEXT_IP_TYPE_DEFAULT, MBIM_CONTEXT_TYPE_INVALID, ""},
    { -1, MBIM_CONTEXT_IP_TYPE_DEFAULT, MBIM_CONTEXT_TYPE_INVALID, ""},
    { -1, MBIM_CONTEXT_IP_TYPE_DEFAULT, MBIM_CONTEXT_TYPE_INVALID, ""},
    { -1, MBIM_CONTEXT_IP_TYPE_DEFAULT, MBIM_CONTEXT_TYPE_INVALID, ""},
};

/*****************************************************************************/
void
mbim_device_basic_connect(Mbim_Token                token,
                          uint32_t                  cid,
                          MbimMessageCommandType    commandType,
                          void *                    data,
                          size_t                    dataLen) {
    switch (cid) {
        case MBIM_CID_BASIC_CONNECT_DEVICE_CAPS:
            basic_connect_device_caps(token, commandType, data, dataLen);
            break;
        case MBIM_CID_BASIC_CONNECT_SUBSCRIBER_READY_STATUS:
            basic_connect_subscriber_ready_status(token, commandType, data, dataLen);
            break;
        case MBIM_CID_BASIC_CONNECT_RADIO_STATE:
            basic_connect_radio_state(token, commandType, data, dataLen);
            break;
        case MBIM_CID_BASIC_CONNECT_PIN:
            basic_connect_pin(token, commandType, data, dataLen);
            break;
        case MBIM_CID_BASIC_CONNECT_PIN_LIST:
            basic_connect_pin_list(token, commandType, data, dataLen);
            break;
        case MBIM_CID_BASIC_CONNECT_HOME_PROVIDER:
            basic_connect_home_provider(token, commandType, data, dataLen);
            break;
        case MBIM_CID_BASIC_CONNECT_PREFERRED_PROVIDERS:
            basic_connect_preferred_providers(token, commandType, data, dataLen);
            break;
        case MBIM_CID_BASIC_CONNECT_VISIBLE_PROVIDERS:
            basic_connect_visible_providers(token, commandType, data, dataLen);
            break;
        case MBIM_CID_BASIC_CONNECT_REGISTER_STATE:
            basic_connect_register_state(token, commandType, data, dataLen);
            break;
        case MBIM_CID_BASIC_CONNECT_PACKET_SERVICE:
            basic_connect_packet_service(token, commandType, data, dataLen);
            break;
        case MBIM_CID_BASIC_CONNECT_SIGNAL_STATE:
            basic_signal_state_service(token, commandType, data, dataLen);
            break;
        case MBIM_CID_BASIC_CONNECT_CONNECT:
            basic_connect_connect(token, commandType, data, dataLen);
            break;
        case MBIM_CID_BASIC_CONNECT_PROVISIONED_CONTEXTS:
            basic_connect_provisioned_contexts(token, commandType, data, dataLen);
            break;
//        case MBIM_CID_BASIC_CONNECT_SERVICE_ACTIVATION:
//            basic_connect_service_activiation(token);
//            break;
        case MBIM_CID_BASIC_CONNECT_IP_CONFIGURATION:
            basic_connect_ip_configuration(token, commandType, data, dataLen);
            break;
        case MBIM_CID_BASIC_CONNECT_DEVICE_SERVICES:
            basic_connect_device_services(token, commandType, data, dataLen);
            break;
        case MBIM_CID_BASIC_CONNECT_DEVICE_SERVICE_SUBSCRIBE_LIST:
            basic_connect_device_service_subscribe_list(token, commandType,
                    data, dataLen);
            break;
//        case MBIM_CID_BASIC_CONNECT_PACKET_STATISTICS:
//            basic_connect_packet_statistics(token);
//            break;
        case MBIM_CID_BASIC_CONNECT_NETWORK_IDLE_HINT:
            basic_connect_network_idle_hint(token, commandType, data, dataLen);
            break;
        case MBIM_CID_BASIC_CONNECT_EMERGENCY_MODE:
            basic_connect_emergency_mode(token, commandType, data, dataLen);
            break;
//        case MBIM_CID_BASIC_CONNECT_IP_PACKET_FILTERS:
//            basic_connect_ip_packet_filters(token);
//            break;
//        case MBIM_CID_BASIC_CONNECT_MULTICARRIER_PROVIDERS:
//            basic_connect_multicarrier_providers(token);
//            break;
        default:
            RLOGD("unsupproted basic connect cid");
            mbim_device_command_done(token,
                    MBIM_STATUS_ERROR_NO_DEVICE_SUPPORT, NULL, 0);
            break;
    }
}

RIL_AppType
getSimType(int channelID) {
    int err, skip;
    int cardType;
    char *line = NULL;
    ATResponse *p_response = NULL;
    RIL_AppType ret = RIL_APPTYPE_UNKNOWN;

    if (s_appType != RIL_APPTYPE_UNKNOWN) {
        return s_appType;
    }
    err = at_send_command_singleline(s_ATChannels[channelID], "AT+EUICC?",
                                     "+EUICC:", &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &skip);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &skip);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &cardType);
    if (err < 0) goto error;

    if (cardType == 1) {
        ret = RIL_APPTYPE_USIM;
    } else if (cardType == 0) {
        ret = RIL_APPTYPE_SIM;
    } else {
        ret = RIL_APPTYPE_UNKNOWN;
    }

    at_response_free(p_response);
    s_appType = ret;
    return ret;

error:
    at_response_free(p_response);
    return RIL_APPTYPE_UNKNOWN;
}

int
iccExchangeSimIO    (int        command,
                     int        fileid,
                     char *     path,
                     int        p1,
                     int        p2,
                     int        p3,
                     char *     data,
                     char *     pin2,
                     char *     aid,
                     char **    rsp,
                     int        channelID) {
    MBIM_UNUSED_PARM(pin2);
    MBIM_UNUSED_PARM(aid);

    int err = -1;
    char *cmd = NULL;
    char *line = NULL;
    char pad_data = '0';
    int sw1 = 0, sw2 = 0;
    ATResponse *p_response = NULL;

    if (data == NULL) {  /* read */
        err = asprintf(&cmd, "AT+CRSM=%d,%d,%d,%d,%d,%c,\"%s\"", command,
                fileid, p1, p2, p3, pad_data, path);
    } else {

    }

    if (err < 0) {
        RLOGE("Failed to allocate memory");
        cmd = NULL;
        goto error;
    }
    err = at_send_command_singleline(s_ATChannels[channelID], cmd,  "+CRSM:",
                                     &p_response);
    free(cmd);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0)  goto error;

    err = at_tok_nextint(&line, &sw1);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &sw2);
    if (err < 0) goto error;

    if (at_tok_hasmore(&line)) {
        char *simResponse = NULL;
        err = at_tok_nextstr(&line, &simResponse);
        if (err < 0) goto error;

        *rsp = strdup(simResponse);
    }

    if ((sw1 == 0x90 || sw1 == 0x91 || sw1 == 0x9e || sw1 == 0x9f )
            && sw2 == 0x00 && rsp != NULL) {
        RLOGD("rsp = %s", *rsp);
    } else {
        goto error;
    }

    at_response_free(p_response);
    return 1;

error:
    at_response_free(p_response);
    RLOGE("fail to read EF %d", fileid );
    return -1;
}

int
getFileCount(char *     rsp,
             int *      recordSize,
             int        channelID) {
    unsigned char *byteRsp = NULL;
    int len = strlen(rsp) / 2;

    byteRsp = (unsigned char *)malloc(len + sizeof(char));
    hexStringToBytes(rsp, byteRsp);

    int count = 0;
    RIL_AppType simType = getSimType(channelID);

    if (simType == RIL_APPTYPE_SIM) {

    } else if (simType == RIL_APPTYPE_USIM) {
        if (byteRsp[RESPONSE_DATA_FCP_FLAG] != TYPE_FCP) {
            RLOGE("wrong fcp flag, unable to get FileCount ");
            goto error;
        }
        RLOGD("getUsimFileCount");
        int desIndex = 0;
        int sizeIndex = 0;
        int i = 0;
        for (i = 0; i < len; i++) {
            if (byteRsp[i] == USIM_FILE_DES_TAG) {
                desIndex = i;
                break;
            }
        }
        RLOGD("TYPE_FCP_DES index = %d", desIndex);
        for (i = desIndex; i < len;) {
            if (byteRsp[i] == USIM_FILE_SIZE_TAG) {
                sizeIndex = i;
                break;
            } else {
                i += (byteRsp[i + 1] + 2);
            }
        }
        RLOGD("TYPE_FCP_SIZE index = %d ", sizeIndex);
        if ((byteRsp[desIndex + RESPONSE_DATA_FILE_DES_FLAG] & 0x07)
                == EF_TYPE_TRANSPARENT) {
            RLOGD("EF_TYPE_TRANSPARENT");
            count = (unsigned int) (((byteRsp[sizeIndex + USIM_DATA_OFFSET_2]
                    & 0xff) << 8)
                    | (byteRsp[sizeIndex + USIM_DATA_OFFSET_3] & 0xff));
            RLOGD("count = ...%d", count);
        } else if ((byteRsp[desIndex + RESPONSE_DATA_FILE_DES_FLAG] & 0x07)
                == EF_TYPE_LINEAR_FIXED
                || (byteRsp[desIndex + RESPONSE_DATA_FILE_DES_FLAG] & 0x07)
                        == EF_TYPE_CYCLIC) {
            RLOGD("EF_TYPE_LINEAR_FIXED OR EF_TYPE_CYCLIC");
            if (USIM_FILE_DES_TAG != byteRsp[RESPONSE_DATA_FILE_DES_FLAG]) {
                RLOGE("USIM_FILE_DES_TAG != ...");
                goto error;
            }
            if (TYPE_FILE_DES_LEN != byteRsp[RESPONSE_DATA_FILE_DES_LEN_FLAG]) {
                RLOGE("TYPE_FILE_DES_LEN != ...");
                goto error;
            }
            *recordSize =
                    (unsigned int) (((byteRsp[RESPONSE_DATA_FILE_RECORD_LEN_1]
                            & 0xff) << 8)
                            | (byteRsp[RESPONSE_DATA_FILE_RECORD_LEN_2] & 0xff));
            count = (unsigned int) (((byteRsp[sizeIndex + USIM_DATA_OFFSET_2]
                    & 0xff) << 8)
                    | (byteRsp[sizeIndex + USIM_DATA_OFFSET_3] & 0xff))
                    / *recordSize;
            RLOGD("recordSize = ...%d", *recordSize);
            RLOGD("count = ...%d", count);
        } else {
            RLOGE("Unknown EF type %d ",
                    (byteRsp[desIndex + RESPONSE_DATA_FILE_DES_FLAG] & 0x07));
        }
    }

    if (byteRsp != NULL) {
        free(byteRsp);
        byteRsp = NULL;
    }
    return count;

error:
    RLOGD("getUsimFileCount error, return 0");
    if (byteRsp != NULL) {
        free(byteRsp);
        byteRsp = NULL;
    }
    return -1;
}

/******************************************************************************/
/**
 * basic_connect_device_caps: query only
 *
 * query:
 *  @command:   the data shall be null
 *  @response:  the data shall be MbimDeviceCapsInfo_2
 */

void
basic_connect_device_caps(Mbim_Token                token,
                          MbimMessageCommandType    commandType,
                          void *                    data,
                          size_t                    dataLen) {
    switch (commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            query_device_caps(token, data, dataLen);
            break;
        default:
            RLOGE("unsupported commandType");
            break;
    }
}

void
query_device_caps(Mbim_Token            token,
                  void *                data,
                  size_t                dataLen) {
    MBIM_UNUSED_PARM(data);
    MBIM_UNUSED_PARM(dataLen);

    int err = -1;
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id  = RIL_SOCKET_1;

    int channelID = getChannel(socket_id);

    MbimDeviceCapsInfo_2 *deviceCapsInfo =
            (MbimDeviceCapsInfo_2 *)calloc(1, sizeof(MbimDeviceCapsInfo_2));

    deviceCapsInfo->deviceType = MBIM_DEVICE_TYPE_REMOVABLE;
    deviceCapsInfo->cellularClass = MBIM_CELLULAR_CLASS_GSM;
    deviceCapsInfo->voiceClass = MBIM_VOICE_CLASS_NO_VOICE;
    deviceCapsInfo->simClass = MBIM_SIM_CLASS_REMOVABLE;
    deviceCapsInfo->dataClass = MBIM_DATA_CLASS_GPRS
                              | MBIM_DATA_CLASS_EDGE
                              | MBIM_DATA_CLASS_HSDPA
                              | MBIM_DATA_CLASS_HSUPA
                              | MBIM_DATA_CLASS_LTE;
    deviceCapsInfo->smsCaps = MBIM_SMS_CAPS_PDU_RECEIVE
                            | MBIM_SMS_CAPS_PDU_SEND;
    deviceCapsInfo->controlCaps = MBIM_CTRL_CAPS_REG_MANUAL
                                | MBIM_CTRL_CAPS_HW_RADIO_SWITCH;
    deviceCapsInfo->maxSessions = MAX_PDP;
    deviceCapsInfo->customDataClass = NULL;

    /* get IMEI */
    err = at_send_command_numeric(s_ATChannels[channelID], "AT+CGSN",
                                  &p_response);
    if (err < 0 || p_response->success == 0) {
        deviceCapsInfo->deviceId = NULL;
    } else {
        deviceCapsInfo->deviceId = p_response->p_intermediates->line;
        RLOGD("deviceId: %s", deviceCapsInfo->deviceId);
    }
    AT_RESPONSE_FREE(p_response);

    // TODO
    deviceCapsInfo->firmwareInfo = "sprd";
    deviceCapsInfo->hardwareInfo = "sc9850_modem";

    mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE, deviceCapsInfo,
            sizeof(MbimDeviceCapsInfo_2));
    putChannel(channelID);
    free(deviceCapsInfo);
}

/******************************************************************************/
/**
 * basic_connect_subscriber_ready_status: query and notification supported
 *
 * query:
 *  @command:   the data shall be null
 *  @response:  the data shall be MbimSubscriberReadyInfo_2
 *
 * notification:
 *  @command:   NA
 *  @response:  the data shall be MbimSubscriberReadyInfo_2
 */
void
basic_connect_subscriber_ready_status(Mbim_Token                token,
                                      MbimMessageCommandType    commandType,
                                      void *                    data,
                                      size_t                    dataLen) {
    switch (commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            query_subscriber_ready_status(token, data, dataLen);
            break;
        default:
            RLOGE("unsupported commandType");
            break;
    }
}

void setSimState(MbimSubscriberReadyState newState, int channelID) {
    if (s_simState != newState) {
        s_simState = newState;
        if (s_simState == MBIM_SUBSCRIBER_READY_STATE_INITIALIZED) {
            at_send_command_singleline(s_ATChannels[channelID], "AT+CSMS=1",
                    "+CSMS:", NULL);
            /**
             * Always send SMS messages directly to the TE
             *
             * mode = 1 // discard when link is reserved (link should never be
             *             reserved)
             * mt = 2   // most messages routed to TE
             * bm = 2   // new cell BM's routed to TE
             * ds = 1   // Status reports routed to TE
             * bfr = 1  // flush buffer
             */
            at_send_command(s_ATChannels[channelID], "AT+CNMI=3,2,2,1,1", NULL);
        }
    }
}

int
getSubscriberReadyInfo(MbimSubscriberReadyInfo_2 *subscriberReadyInfo) {
    int err = -1;
    ATResponse *p_response = NULL;
    ATResponse *p_iccIdResponse = NULL;
    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    char *line = NULL;
    char *cpinResult = NULL;
    char *iccId = NULL;
    int skip;
    char * rsp_iccid = NULL;
    int channelID = getChannel(socket_id);

    subscriberReadyInfo->readyInfo = MBIM_READY_INFO_FLAG_NONE;

    /* get sim status */
    err = at_send_command(s_ATChannels[channelID], "AT+SFUN=2",
                                  &p_response);
    if (err < 0|| p_response->success == 0) {
        RLOGE("error for turn on sim");
        goto error;
    }

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+CPIN?",
                                     "+CPIN:", &p_response);
    if (err != 0) {
        subscriberReadyInfo->readyState =
                MBIM_SUBSCRIBER_READY_STATE_NOT_INITIALIZED;
        goto error;
    }

    switch (at_get_cme_error(p_response)) {
        case CME_SUCCESS:
            /**
             * get IccId
             */
            err = iccExchangeSimIO(COMMAND_GET_RESPONSE, 12258, "3F00", 0, 0,
                    GET_RESPONSE_EF_SIZE_BYTES, NULL, NULL, NULL, &rsp_iccid, channelID);
            if (err < 0) {
                RLOGE("Failed to get IccId");
            } else {
                RLOGD("rsp_iccid = %s", rsp_iccid);
                if (rsp_iccid == NULL)
                    break;
                int recordSize = 0;
                int count = getFileCount(rsp_iccid, &recordSize, channelID);
                if (0 == count) {
                    RLOGE("unable getFileCount, return error");
                    break;
                }
                free(rsp_iccid);
                rsp_iccid = NULL;
                // read EF_ICCID content
                err = iccExchangeSimIO(COMMAND_READ_BINARY, 12258, "3F00", 0, 0,
                        count, NULL, NULL, NULL, &rsp_iccid, channelID);
                RLOGD("rsp_iccid = %s", rsp_iccid);
                bchToString(rsp_iccid);
                subscriberReadyInfo->simIccId = strdup(rsp_iccid);
                RLOGD("simIccId is %s", subscriberReadyInfo->simIccId);
                free(rsp_iccid);
                rsp_iccid = NULL;
            }
            break;
        case CME_SIM_NOT_INSERTED:
            subscriberReadyInfo->readyState =
                    MBIM_SUBSCRIBER_READY_STATE_SIM_NOT_INSERTED;
            goto done;
        case CME_SIM_BUSY:
            subscriberReadyInfo->readyState =
                    MBIM_SUBSCRIBER_READY_STATE_NOT_INITIALIZED;
            goto done;
        default:
            subscriberReadyInfo->readyState = MBIM_SUBSCRIBER_READY_STATE_FAILURE;
            goto done;
    }

    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0) {
        subscriberReadyInfo->readyState =
                MBIM_SUBSCRIBER_READY_STATE_NOT_INITIALIZED;
        goto error;
    }

    err = at_tok_nextstr(&line, &cpinResult);
    if (err < 0) {
        subscriberReadyInfo->readyState =
                MBIM_SUBSCRIBER_READY_STATE_NOT_INITIALIZED;
        goto error;
    }

    if (0 == strcmp(cpinResult, "READY")) {

        /* the sim state is ready, need to get the IMSI, IccId and MSISDN of the sim */
        subscriberReadyInfo->readyState =
                MBIM_SUBSCRIBER_READY_STATE_INITIALIZED;

        /* get IMSI */
        AT_RESPONSE_FREE(p_response);
        err = at_send_command_numeric(s_ATChannels[channelID], "AT+CIMI",
                &p_response);
        if (err < 0 || p_response->success == 0) {
            RLOGE("error for get imsi");
            goto error;
        } else {
            subscriberReadyInfo->subscriberId = strdup(
                    p_response->p_intermediates->line);
            RLOGD("subscriberId is %s", subscriberReadyInfo->subscriberId);
            snprintf(s_subscriberId, 16, "%s",
                    subscriberReadyInfo->subscriberId);
        }

        /**
         * FixMe get MSISDN, because it is not mandatoty and not exist in most
         * SIM cards, so not read to read it now
         */
        subscriberReadyInfo->elementCount = 0;
        char* rsp_numbers = NULL;
        int count = 0;
        RIL_AppType simType = getSimType(channelID);
        // get IccType
        if (simType == RIL_APPTYPE_USIM) {
            RLOGD("usim, get from DF_ADF");
            err = iccExchangeSimIO(COMMAND_GET_RESPONSE, 28480, DF_ADF, 0, 0,
                    GET_RESPONSE_EF_SIZE_BYTES, NULL, NULL, NULL, &rsp_numbers, channelID);
        } else if (simType == RIL_APPTYPE_SIM) {
            RLOGD("sim, get from DF_TELECOM");
            err = iccExchangeSimIO(COMMAND_GET_RESPONSE, 28480, DF_TELECOM, 0,
                    0, GET_RESPONSE_EF_SIZE_BYTES, NULL, NULL, NULL, &rsp_numbers, channelID);
        } else {
            RLOGD("unknown type");
            goto done;
        }
        if (err < 0) {
            RLOGE("Failed to get MSISDN");
        } else {
            RLOGD("rsp_numbers = %s", rsp_numbers);
            if (rsp_numbers == NULL) {
                goto done;
            }

            int recordSize = 0;
            count = getFileCount(rsp_numbers, &recordSize, channelID);
            RLOGD("getFileCount = %d", count);
            if (0 == count || recordSize == 0) {
                RLOGE("unable getFileCount, return error");
                goto done;
            }
            free(rsp_numbers);
            rsp_numbers = NULL;

            subscriberReadyInfo->elementCount = 0;
            subscriberReadyInfo->telephoneNumbers = (char **)calloc(count, sizeof(char *));

            for (int i = 1; i <= count; i++) {
                // read EF_MSISDN content
                if (simType == RIL_APPTYPE_USIM) {
                    RLOGD("usim, get from DF_ADF");
                    err = iccExchangeSimIO(COMMAND_READ_RECORD, 28480, DF_ADF,
                            i, READ_RECORD_MODE_ABSOLUTE, recordSize, NULL,
                            NULL, NULL, &rsp_numbers, channelID);
                } else if (simType == RIL_APPTYPE_SIM) {
                    RLOGD("sim, get from DF_TELECOM");
                    err = iccExchangeSimIO(COMMAND_READ_RECORD, 28480,
                            DF_TELECOM, i, READ_RECORD_MODE_ABSOLUTE,
                            recordSize, NULL, NULL, NULL, &rsp_numbers, channelID);
                } else {
                    RLOGD("unknown type");
                    goto done;
                }
                RLOGD("rsp = %s", rsp_numbers);

                unsigned char *data = (unsigned char *)
                        calloc(strlen(rsp_numbers) / 2, sizeof(char));
                hexStringToBytes(rsp_numbers, data);

                SimAdnRecord rec = (SimAdnRecord)
                        calloc(1, sizeof(SimAdnRecordRec));

                int result = parseSimAdnRecord(rec, data, strlen(data));

                RLOGD("number = %s", rec->adn.number);

                if (rec->adn.number[0]) {
                    subscriberReadyInfo->telephoneNumbers[subscriberReadyInfo->elementCount] =
                            strdup(rec->adn.number);
                    subscriberReadyInfo->elementCount++;
                }
                free(rec);
                free(rsp_numbers);
                rsp_numbers = NULL;
            }
        }
    } else {
        subscriberReadyInfo->readyState =
                MBIM_SUBSCRIBER_READY_STATE_DEVICE_LOCKED;
    }

done:
    setSimState(subscriberReadyInfo->readyState, channelID);
    AT_RESPONSE_FREE(p_response);
    AT_RESPONSE_FREE(p_iccIdResponse);
    putChannel(channelID);
    return MBIM_DEVICE_PROCESS_SUCCESS;

error:
    setSimState(subscriberReadyInfo->readyState, channelID);
    AT_RESPONSE_FREE(p_response);
    AT_RESPONSE_FREE(p_iccIdResponse);
    putChannel(channelID);
    return MBIM_DEVICE_PROCESS_FAIL;
}

void
query_subscriber_ready_status(Mbim_Token            token,
                              void *                data,
                              size_t                dataLen) {
    MBIM_UNUSED_PARM(data);
    MBIM_UNUSED_PARM(dataLen);

    int err = -1;
    MbimStatusError statusError = MBIM_STATUS_ERROR_NONE;

    MbimSubscriberReadyInfo_2 *subscriberReadyInfo =
            (MbimSubscriberReadyInfo_2 *)calloc(1, sizeof(MbimSubscriberReadyInfo_2));

    err = getSubscriberReadyInfo(subscriberReadyInfo);
    if (err != MBIM_DEVICE_PROCESS_SUCCESS) {
        statusError = MBIM_STATUS_ERROR_FAILURE;
    }

    mbim_device_command_done(token, statusError,
            subscriberReadyInfo, sizeof(MbimSubscriberReadyInfo_2));

    /* free */
    memsetAndFreeStrings(2, subscriberReadyInfo->subscriberId,
            subscriberReadyInfo->simIccId);
    for (uint32_t i = 0; i < subscriberReadyInfo->elementCount; i++) {
        free(subscriberReadyInfo->telephoneNumbers[i]);
    }
    free(subscriberReadyInfo->telephoneNumbers);
    free(subscriberReadyInfo);
}

/*****************************************************************************/
/**
 * basic_connect_radio_state: query, set and notification supported
 *
 * query:
 *  @command:   the data shall be null
 *  @response:  the data shall be MbimRadioStateInfo
 *
 * set:
 *  @command:   the data shall be MbimSetRadioState
 *  @response:  the data shall be MbimRadioStateInfo
 *
 * notification:
 *  @command:   NA
 *  @response:  the data shall be MbimRadioStateInfo
 *
 * Note:
 *  for set, MbimSetRadioState's radioState is to set the software controlled
 *  radio state. About hwRadioState in MbimRadioStateInfo, it represents
 *  W_DISABLE switch, if the device does not have a W_DISABLE switch,
 *  the function must return MbimRadioOn in the field.
 */

void
basic_connect_radio_state(Mbim_Token                token,
                          MbimMessageCommandType    commandType,
                          void *                    data,
                          size_t                    dataLen) {
    switch (commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            query_radio_state(token, data, dataLen);
            break;
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            set_radio_state(token, data, dataLen);
            break;
        default:
            RLOGE("unsupported commandType");
            break;
    }
}

MbimRadioSwitchState
RadioStateChangeToMbimRadioState(RIL_RadioState radioState) {
    MbimRadioSwitchState swRadioState = MBIM_RADIO_SWITCH_STATE_OFF;

    if (radioState == RADIO_STATE_OFF || radioState == RADIO_STATE_UNAVAILABLE) {
        swRadioState = MBIM_RADIO_SWITCH_STATE_OFF;
    } else if (radioState == RADIO_STATE_ON) {
        swRadioState = MBIM_RADIO_SWITCH_STATE_ON;
    }

    return swRadioState;
}

void
query_radio_state(Mbim_Token            token,
                  void *                data,
                  size_t                dataLen) {
    MBIM_UNUSED_PARM(data);
    MBIM_UNUSED_PARM(dataLen);

    MbimRadioStateInfo *radioStateInfo =
            (MbimRadioStateInfo *)calloc(1, sizeof(MbimRadioStateInfo));

    radioStateInfo->hwRadioState = MBIM_RADIO_SWITCH_STATE_ON;
    radioStateInfo->swRadioState =
            RadioStateChangeToMbimRadioState(s_radioState[RIL_SOCKET_1]);

    mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE, radioStateInfo,
                sizeof(MbimRadioStateInfo));
    free(radioStateInfo);
}

void
buildWorkModeCmd(char *     cmd,
                 size_t     size) {
    int simId;
    char strFormatter[AT_COMMAND_LEN] = {0};

    for (simId = 0; simId < SIM_COUNT; simId++) {
        if (simId == 0) {
            snprintf(cmd, size, "AT+SPTESTMODEM=%d", getWorkMode(simId));
            if (SIM_COUNT == 1) { /*lint !e774 */
                strncpy(strFormatter, cmd, size);
                strncat(strFormatter, ",%d", strlen(",%d"));
                snprintf(cmd, size, strFormatter, GSM_ONLY);
            }
        } else {
            strncpy(strFormatter, cmd, size);
            strncat(strFormatter, ",%d", strlen(",%d"));
            snprintf(cmd, size, strFormatter, getWorkMode(simId));
        }
    }
}

int
set_radio_state_off() {
    int ret = MBIM_DEVICE_PROCESS_FAIL;
    int err = -1;

    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    ATChannelId channelID = getChannel(socket_id);

    /* The system ask to shutdown the radio */
    err = at_send_command(s_ATChannels[channelID], "AT+SFUN=5", &p_response);
    if (err < 0 || p_response->success == 0) {
        ret = MBIM_DEVICE_PROCESS_FAIL;
        goto exit;
    }

    setRadioState(channelID, RADIO_STATE_OFF);
    ret = MBIM_DEVICE_PROCESS_SUCCESS;

exit:
    putChannel(channelID);
    at_response_free(p_response);
    return ret;
}

int
set_radio_state_on() {
    int ret = MBIM_DEVICE_PROCESS_FAIL;
    int err = -1;
    char cmd[AT_COMMAND_LEN] = {0};

    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    ATChannelId channelID = getChannel(socket_id);

    if (s_radioState[socket_id] == RADIO_STATE_OFF) {
        /* AT+SPTESTMODE */
        buildWorkModeCmd(cmd, sizeof(cmd));
        at_send_command(s_ATChannels[channelID], cmd, NULL);

        /* power radio on */
        err = at_send_command(s_ATChannels[channelID], "AT+SFUN=2",
                                      &p_response);
        if (err < 0|| p_response->success == 0) {
            ret = MBIM_DEVICE_PROCESS_FAIL;
            goto exit;
        }
        err = at_send_command(s_ATChannels[channelID], "AT+SFUN=4",
                                      &p_response);
        if (err < 0|| p_response->success == 0) {
            ret = MBIM_DEVICE_PROCESS_FAIL;
            goto exit;
        }
    }

    setRadioState(channelID, RADIO_STATE_ON);
    ret = MBIM_DEVICE_PROCESS_SUCCESS;

exit:
    putChannel(channelID);
    at_response_free(p_response);
    return ret;
}

void
set_radio_state(Mbim_Token              token,
                void *                  data,
                size_t                  dataLen) {
    MbimSetRadioState *setRadioState = (MbimSetRadioState *)data;
    MBIM_UNUSED_PARM(dataLen);

    int ret = -1;;
    char cmd[AT_COMMAND_LEN] = {0};
    ATResponse *p_response = NULL;

    if (setRadioState->radioState == MBIM_RADIO_SWITCH_STATE_OFF) {
        ret = set_radio_state_off();
    } else if (setRadioState->radioState == MBIM_RADIO_SWITCH_STATE_ON) {
        ret = set_radio_state_on();
    } else {
        RLOGE("unknown radio state set operation: %d", setRadioState->radioState);
    }

    if (ret == MBIM_DEVICE_PROCESS_SUCCESS) {  // success
        MbimRadioStateInfo *radioStateInfo =
                (MbimRadioStateInfo *)calloc(1, sizeof(MbimRadioStateInfo));
        radioStateInfo->hwRadioState = MBIM_RADIO_SWITCH_STATE_ON;
        radioStateInfo->swRadioState =
                RadioStateChangeToMbimRadioState(s_radioState[RIL_SOCKET_1]);

        mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE, radioStateInfo,
                    sizeof(MbimRadioStateInfo));
        free(radioStateInfo);
    } else {  // fail
        mbim_device_command_done(token, MBIM_STATUS_ERROR_FAILURE, NULL, 0);
    }
}

/******************************************************************************/
/**
 * basic_connect_subscriber_ready_status: query and notification supported
 *
 * query:
 *  @command:   the data shall be null
 *  @response:  the data shall be MbimRegistrationStateInfo_2
 *
 * notification:
 *  @command:   NA
 *  @response:  the data shall be MbimRegistrationStateInfo_2
 */
void
basic_signal_state_service(Mbim_Token               token,
                           MbimMessageCommandType   commandType,
                           void *                   data,
                           size_t                   dataLen) {
    switch (commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            query_signal_state(token, data, dataLen);
            break;
        default:
            RLOGE("unsupported commandType");
            break;
    }
}

/* for AT+CESQ execute command response process */
void cesq_execute_cmd(char *line, char *newLine) {
    int err;
    char *atInStr;

    atInStr = line;
    int rxlev = 0, ber = 0, rscp = 0, ecno = 0, rsrq = 0, rsrp = 0;
    char respStr[MAX_AT_RESPONSE];

    err = at_tok_flag_start(&atInStr, ':');
    if (err < 0)
        goto error;

    err = at_tok_nextint(&atInStr, &rxlev);
    if (err < 0)
        goto error;

    if (rxlev <= 61) {
        rxlev = (rxlev + 2) / 2;
    } else if (rxlev > 61 && rxlev <= 63) {
        rxlev = 31;
    } else if (rxlev >= 100 && rxlev < 103) {
        rxlev = 0;
    } else if (rxlev >= 103 && rxlev < 165) {
        rxlev = ((rxlev - 103) + 1) / 2; // add 1 for compensation
    } else if (rxlev >= 165 && rxlev <= 191) {
        rxlev = 31;
    }

    err = at_tok_nextint(&atInStr, &ber);
    if (err < 0) goto error;

    err = at_tok_nextint(&atInStr, &rscp);
    if (err < 0) goto error;

    if (rscp >= 100 && rscp < 103) {
        rscp = 0;
    } else if (rscp >= 103 && rscp < 165) {
        rscp = ((rscp - 103) + 1) / 2;
    } else if (rscp >= 165 && rscp <= 191) {
        rscp = 31;
    }

    err = at_tok_nextint(&atInStr, &ecno);
    if (err < 0) goto error;

    err = at_tok_nextint(&atInStr, &rsrq);
    if (err < 0) goto error;

    err = at_tok_nextint(&atInStr, &rsrp);
    if (err < 0) goto error;

    if (rsrp == 255) {
        rsrp = 99;
    } else {
        rsrp -= 141;
        if (rsrp <= -113) {
            rsrp = 0;
        } else if (rsrp >= -51) {
            rsrp = 31;
        } else {
            rsrp = (rsrp + 113) / 2;
        }
    }

    snprintf(newLine, AT_COMMAND_LEN, "+CESQ: %d,%d,%d,%d,%d,%d", rxlev, ber,
            rscp, ecno, rsrq, rsrp);

error:
    return;
}


int parseSignalStateInfo(char *                 line,
                         MbimSignalStateInfo *  signalStateInfo) {
    RLOGD("parseSignalStateInfo, line = %s", line);

    char newLine[AT_COMMAND_LEN] = {0};
    char *tmp;
    int err = -1;
    int skip;
    int response[6] = { -1, -1, -1, -1, -1, -1 };

    cesq_execute_cmd(line, newLine);

    RLOGD("parseSignalStateInfo newLine = %s", newLine);
    tmp = newLine;

    err = at_tok_start(&tmp);
    if (err < 0) goto error;

    err = at_tok_nextint(&tmp, &response[0]);
    if (err < 0) goto error;

    err = at_tok_nextint(&tmp, &response[1]);
    if (err < 0) goto error;

    err = at_tok_nextint(&tmp, &response[2]);
    if (err < 0) goto error;

    err = at_tok_nextint(&tmp, &skip);
    if (err < 0) goto error;

    err = at_tok_nextint(&tmp, &skip);
    if (err < 0) goto error;

    err = at_tok_nextint(&tmp, &response[5]);
    if (err < 0) goto error;


    for (int i=0; i< 5; i++ ) {
        RLOGE("response = %d", response[i]);
    }

    if (s_availableDataClasses == MBIM_DATA_CLASS_GPRS
            || s_availableDataClasses == MBIM_DATA_CLASS_EDGE) {
        signalStateInfo->rssi = response[0];
    } else if (s_availableDataClasses == MBIM_DATA_CLASS_UMTS
            || s_availableDataClasses == MBIM_DATA_CLASS_HSDPA
            || s_availableDataClasses == MBIM_DATA_CLASS_HSUPA) {
        signalStateInfo->rssi = response[2];
    } else if (s_availableDataClasses == MBIM_DATA_CLASS_LTE) {
        signalStateInfo->rssi = response[5];
    }
    signalStateInfo->signalStrengthInterval = SIGNAL_STRENGTH_INTERVAL;
    signalStateInfo->errorRate = 5;
    signalStateInfo->errorRateThreshold = 0xffffffff;
    RLOGE("rssi = %d",signalStateInfo->rssi );
    return 1;

error:
    return -1;
}

int
getSignalStateStateInfo(MbimSignalStateInfo *signalStateInfo) {
    int err = -1;
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    int channelID = getChannel(socket_id);

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+CESQ",
                                     "+CESQ:", &p_response);

    if (err < 0 || p_response->success == 0) {
        goto error;
    }
    err = parseSignalStateInfo(p_response->p_intermediates->line, signalStateInfo);
    if (err < 0 ) goto error;

done:
    AT_RESPONSE_FREE(p_response);
    putChannel(channelID);
    return 1;

error:
    AT_RESPONSE_FREE(p_response);
    putChannel(channelID);
    return -1;

}

void
query_signal_state(Mbim_Token   token,
                   void *       data,
                   size_t       dataLen) {
    MBIM_UNUSED_PARM(data);
    MBIM_UNUSED_PARM(dataLen);

    int err = -1;

    MbimSignalStateInfo *signalState = (MbimSignalStateInfo *)
            calloc(1, sizeof(MbimSignalStateInfo));

    err = getSignalStateStateInfo(signalState);
    if (err < 0) goto error;

done:
    RLOGE("query_signal_state done");
    mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE, signalState,
            sizeof(MbimSignalStateInfo));
    free(signalState);
    return;

error:
    mbim_device_command_done(token, MBIM_STATUS_ERROR_FAILURE, signalState,
            sizeof(MbimSignalStateInfo));
    free(signalState);
    return;
}

/******************************************************************************/
/**
 * basic_connect_pin: query and set supported
 *
 * query:
 *  @command:   the informationBuffer shall be null
 *  @response:  the informationBuffer shall be MbimPinInfo
 *
 * set:
 *  @command:   the informationBuffer shall be MbimSetPin
 *  @response:  the informationBuffer shall be MbimPinInfo
 *
 */
void
basic_connect_pin(Mbim_Token                token,
                  MbimMessageCommandType    commandType,
                  void *                    data,
                  size_t                    dataLen) {
    switch (commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            query_pin(token, data, dataLen);
            break;
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            set_pin(token, data, dataLen);
            break;
        default:
            RLOGE("unsupported commandType");
            break;
    }
}

static int
getPinPukRemainTimes(int            channelID,
                     PinPukType     type) {
    int err, result;
    int remaintime = 3;
    char *line = NULL;
    char cmd[AT_COMMAND_LEN] = {0};

    ATResponse *p_response = NULL;

    if (UNLOCK_PUK == type || UNLOCK_PUK2 == type) {
        remaintime = 10;
    }

    snprintf(cmd, sizeof(cmd), "AT+XX=%d", type);
    err = at_send_command_singleline(s_ATChannels[channelID], cmd, "+XX:",
                                     &p_response);
    if (err < 0 || p_response->success == 0) {
        RLOGE("getSimlockRemainTimes: +XX response error !");
    } else {
        line = p_response->p_intermediates->line;
        err = at_tok_start(&line);
        if (err == 0) {
            err = at_tok_nextint(&line, &result);
            if (err == 0) {
                remaintime = result;
            }
        }
    }
    at_response_free(p_response);
    return remaintime;
}

static int
getSimLockRemainTimes(int   channelID,
                      int   type) {
    int err;
    int ret = -1;
    int fac = type;
    int ck_type = 1;
    int result[2] = {0, 0};
    char *line = NULL;
    char cmd[AT_COMMAND_LEN] = {0};
    ATResponse *p_response = NULL;

    RLOGD("[MBBMS] fac: %d, ck_type: %d", fac, ck_type);
    snprintf(cmd, sizeof(cmd), "AT+SPSMPN=%d,%d", fac, ck_type);
    err = at_send_command_singleline(s_ATChannels[channelID], cmd, "+SPSMPN:",
                                     &p_response);
    if (err < 0 || p_response->success == 0) {
        ret = -1;
    } else {
        line = p_response->p_intermediates->line;

        err = at_tok_start(&line);

        if (err == 0) {
            err = at_tok_nextint(&line, &result[0]);
            if (err == 0) {
                at_tok_nextint(&line, &result[1]);
                err = at_tok_nextint(&line, &result[1]);
            }
        }

        if (err == 0) {
            ret = result[0] - result[1];
        } else {
            ret = -1;
        }
    }
    at_response_free(p_response);
    return ret;
}

int
getPinInfo(MbimPinInfo *pinState) {
    int err = -1;
    char *line = NULL;
    char *cpinResult = NULL;
    ATResponse *p_response = NULL;

    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    int channelID = getChannel(socket_id);

    // init pin state
    pinState->pinType = MBIM_PIN_TYPE_PIN1;
    pinState->pinState = MBIM_PIN_STATE_UNLOCKED;
    pinState->remainingAttemps = getPinPukRemainTimes(channelID, UNLOCK_PIN);

    //get pin state
    err = at_send_command(s_ATChannels[channelID], "AT+SFUN=2",
                                  &p_response);
    if (err < 0|| p_response->success == 0) {
        RLOGE("error for turn on sim");
        goto error;
    }

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+CPIN?",
                                     "+CPIN:", &p_response);
    if (err != 0) {
        goto error;
    }

    switch (at_get_cme_error(p_response)) {
        case CME_SUCCESS:
            break;
        case CME_SIM_NOT_INSERTED:
        case CME_SIM_BUSY:
            goto done;  // FixMe
        default:
            goto error;
    }

    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextstr(&line, &cpinResult);
    if (err < 0) goto error;

    if (0 == strcmp(cpinResult, "SIM PIN")) {
        pinState->pinType = MBIM_PIN_TYPE_PIN1;
        pinState->pinState = MBIM_PIN_STATE_LOCKED;
        pinState->remainingAttemps = getPinPukRemainTimes(channelID, UNLOCK_PIN);
    } else if (0 == strcmp(cpinResult, "SIM PUK")) {
        pinState->pinType = MBIM_PIN_TYPE_PUK1;
        pinState->pinState = MBIM_PIN_STATE_LOCKED;
        pinState->remainingAttemps = getPinPukRemainTimes(channelID, UNLOCK_PUK);
    } else if (0 == strcmp(cpinResult, "PH-NET PIN")) {
        pinState->pinType = MBIM_PIN_TYPE_NETWORK_PIN;
        pinState->pinState = MBIM_PIN_STATE_LOCKED;
        pinState->remainingAttemps = getSimLockRemainTimes(channelID, 2);
    } else if (0 == strcmp(cpinResult, "PH-NETSUB PIN")) {
        pinState->pinType = MBIM_PIN_TYPE_NETWORK_SUBSET_PIN;
        pinState->pinState = MBIM_PIN_STATE_LOCKED;
        pinState->remainingAttemps = getSimLockRemainTimes(channelID, 3);
    } else if (0 == strcmp(cpinResult, "PH-SP PIN")) {
        pinState->pinType = MBIM_PIN_TYPE_SERVICE_PROVIDER_PIN;
        pinState->pinState = MBIM_PIN_STATE_LOCKED;
        pinState->remainingAttemps = getSimLockRemainTimes(channelID, 4);
    } else if (0 == strcmp(cpinResult, "PH-CORP PIN")) {
        pinState->pinType = MBIM_PIN_TYPE_CORPORATE_PIN;
        pinState->pinState = MBIM_PIN_STATE_LOCKED;
        pinState->remainingAttemps = getSimLockRemainTimes(channelID, 5);
    } else if (0 == strcmp(cpinResult, "PH-SIM PIN")) {
        //FIXME maybe MBIM_PIN_TYPE_DEVICE_FIRST_SIM_PIN
        pinState->pinType = MBIM_PIN_TYPE_DEVICE_SIM_PIN;
        pinState->pinState = MBIM_PIN_STATE_LOCKED;
        pinState->remainingAttemps = getSimLockRemainTimes(channelID, 1);
    } else if (0 == strcmp(cpinResult, "PH-NET PUK")) {
        pinState->pinType = MBIM_PIN_TYPE_NETWORK_PUK;
        pinState->pinState = MBIM_PIN_STATE_LOCKED;
    } else if (0 == strcmp(cpinResult, "PH-NETSUB PUK")) {
        pinState->pinType = MBIM_PIN_TYPE_NETWORK_SUBSET_PUK;
        pinState->pinState = MBIM_PIN_STATE_LOCKED;
    } else if (0 == strcmp(cpinResult, "PH-SP PUK")) {
        pinState->pinType = MBIM_PIN_TYPE_SERVICE_PROVIDER_PUK;
        pinState->pinState = MBIM_PIN_STATE_LOCKED;
    } else if (0 == strcmp(cpinResult, "PH-CORP PUK")) {
        pinState->pinType = MBIM_PIN_TYPE_CORPORATE_PUK;
        pinState->pinState = MBIM_PIN_STATE_LOCKED;
    } else if (0 == strcmp(cpinResult, "PH-SIM PUK")) {
        pinState->pinType = MBIM_PIN_TYPE_DEVICE_FIRST_SIM_PUK;
        pinState->pinState = MBIM_PIN_STATE_LOCKED;
    } else if (0 == strcmp(cpinResult, "PH-INTEGRITY FAIL")) {
        //FIXME INTEGRITY FAIL...
        pinState->pinType = MBIM_PIN_TYPE_UNKNOWN;
        pinState->pinState = MBIM_PIN_STATE_LOCKED;
    }

done:
    AT_RESPONSE_FREE(p_response);
    putChannel(channelID);
    return MBIM_DEVICE_PROCESS_SUCCESS;

error:
    AT_RESPONSE_FREE(p_response);
    putChannel(channelID);
    return MBIM_DEVICE_PROCESS_FAIL;
}

void
query_pin(Mbim_Token        token,
          void *            data,
          size_t            dataLen) {
    MBIM_UNUSED_PARM(data);
    MBIM_UNUSED_PARM(dataLen);

    int err = -1;
    MbimStatusError statusError = MBIM_STATUS_ERROR_NONE;

    MbimPinInfo *pinState = (MbimPinInfo *)calloc(1, sizeof(MbimPinInfo));

    if (pinState == NULL) {
        RLOGE("out of memory");
        statusError = MBIM_STATUS_ERROR_FAILURE;
        goto done;
    }

    err = getPinInfo(pinState);
    if (pinState->pinState == MBIM_PIN_STATE_LOCKED) {
        s_pinType = pinState->pinType;
        s_pinState = MBIM_PIN_STATE_LOCKED;
    }
    if (err != MBIM_DEVICE_PROCESS_SUCCESS) {
        statusError = MBIM_STATUS_ERROR_FAILURE;
    }

done:
    mbim_device_command_done(token, statusError, pinState, sizeof(MbimPinInfo));
    free(pinState);
}

static int
requestEnterSimPin(MbimPinType      pinType,
                   char *           pin,
                   char *           newPin) {
    int err = -1, ret = -1;
    char *cmd = NULL;
    PinPukType rsqtype;
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    ATChannelId channelID = getChannel(socket_id);

    if (pinType == MBIM_PIN_TYPE_PIN1) {
        ret = asprintf(&cmd, "AT+CPIN=%s", pin);
        rsqtype = UNLOCK_PIN;
    } else if (MBIM_PIN_TYPE_PUK1 ) {
        ret = asprintf(&cmd, "AT+CPIN=%s,%s", pin, newPin);
        rsqtype = UNLOCK_PUK;
    }

    if (ret < 0) {
        RLOGE("Failed to allocate memory");
        cmd = NULL;
        goto error;
    }

    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    free(cmd);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    AT_RESPONSE_FREE(p_response);
    putChannel(channelID);
    return MBIM_DEVICE_PROCESS_SUCCESS;

error:
    AT_RESPONSE_FREE(p_response);
    putChannel(channelID);
    return MBIM_DEVICE_PROCESS_FAIL;
}

static int
requestChangeSimPin(MbimPinType     pinType,
                    char *          pin,
                    char *          newPin) {
    int err = -1, ret = -1;
    char *cmd = NULL;
    PinPukType rsqtype;
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    ATChannelId channelID = getChannel(socket_id);

    if (pinType == MBIM_PIN_TYPE_PIN1) {
        ret = asprintf(&cmd, "AT+CPWD=\"SC\",\"%s\",\"%s\"", pin, newPin);
        if (ret < 0) {
            RLOGE("Failed to allocate memory");
            cmd = NULL;
            goto error;
        }

        err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
        free(cmd);
        if (err < 0 || p_response->success == 0) {
            goto error;
        }
    }

    AT_RESPONSE_FREE(p_response);
    putChannel(channelID);
    return MBIM_DEVICE_PROCESS_SUCCESS;

error:
    AT_RESPONSE_FREE(p_response);
    putChannel(channelID);
    return MBIM_DEVICE_PROCESS_FAIL;
}

static int
requestEnableSimPin(MbimPinType     pinType,
                    char *          pin,
                    char *          newPin,
                    int             enable) {
    MBIM_UNUSED_PARM(newPin);

    int err = -1, ret = -1, errNum = -1;;
    char *cmd = NULL;
    char *line = NULL;
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    ATChannelId channelID = getChannel(socket_id);

    if (pinType == MBIM_PIN_TYPE_PIN1) {
        ret = asprintf(&cmd, "AT+CLCK=\"%s\",%d,\"%s\",7", "SC", enable, pin);
        if (ret < 0) {
            RLOGE("Failed to allocate memory");
            cmd = NULL;
            goto error;
        }
        err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
        free(cmd);
        if (err < 0 || p_response->success == 0) {
            goto error;
        }
    }

    AT_RESPONSE_FREE(p_response);
    putChannel(channelID);
    return MBIM_DEVICE_PROCESS_SUCCESS;

error:
    AT_RESPONSE_FREE(p_response);
    putChannel(channelID);
    return MBIM_DEVICE_PROCESS_FAIL;
}

void
set_pin(Mbim_Token          token,
        void *              data,
        size_t              dataLen) {
    MBIM_UNUSED_PARM(dataLen);

    MbimSetPin_2 *setPin = (MbimSetPin_2 *)data;

    int err = -1;;
    int remaintime = 3;
    char cmd[AT_COMMAND_LEN] = {0};

    MbimPinInfo *pinState = (MbimPinInfo *)calloc(1, sizeof(MbimPinInfo));

    if (pinState == NULL) {
        RLOGE("out of memory");
        goto error;
    }
    if (setPin->pinOperation == MBIM_PIN_OPERATION_ENTER) {
        RLOGD("MBIM_PIN_OPERATION_ENTER");
        if (setPin->pinType == MBIM_PIN_TYPE_PIN1 ||
                setPin->pinType == MBIM_PIN_TYPE_PUK1) {
            err = requestEnterSimPin(setPin->pinType, setPin->pin, setPin->newPin);
            if (err != MBIM_DEVICE_PROCESS_SUCCESS) {
                goto error;
            }
        }
    } else if (setPin->pinOperation == MBIM_PIN_OPERATION_ENABLE) {
        RLOGD("MBIM_PIN_OPERATION_ENABLE");
        if (setPin->pinType == MBIM_PIN_TYPE_PIN1) {
            err = requestEnableSimPin(setPin->pinType, setPin->pin, setPin->newPin, true);
            if (err != MBIM_DEVICE_PROCESS_SUCCESS) {
                goto error;
            }
        }
    } else if (setPin->pinOperation == MBIM_PIN_OPERATION_DISABLE) {
        RLOGD("MBIM_PIN_OPERATION_DISABLE");
        if (setPin->pinType == MBIM_PIN_TYPE_PIN1) {
            err = requestEnableSimPin(setPin->pinType, setPin->pin, setPin->newPin, false);
            if (err != MBIM_DEVICE_PROCESS_SUCCESS) {
                goto error;
            }
        }
    } else if (setPin->pinOperation == MBIM_PIN_OPERATION_CHANGE) {
        RLOGD("MBIM_PIN_OPERATION_CHANGE");
        if (setPin->pinType == MBIM_PIN_TYPE_PIN1) {
            err = requestChangeSimPin(setPin->pinType, setPin->pin, setPin->newPin);
            if (err != MBIM_DEVICE_PROCESS_SUCCESS) {
                goto error;
            }
        }
    } else {
        RLOGE("unknown pinOperation : %d", setPin->pinOperation);
        goto error;
    }

    getPinInfo(pinState);
    mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE, pinState,
            sizeof(MbimPinInfo));
    free(pinState);
    return;

error:
    getPinInfo(pinState);
    mbim_device_command_done(token, MBIM_STATUS_ERROR_FAILURE, pinState,
            sizeof(MbimPinInfo));
    free(pinState);
    return;
}

/******************************************************************************/
/**
 * basic_connect_pin_list: query only
 *
 * query:
 *  @command:   the data shall be null
 *  @response:  the data shall be MbimPinListInfo
 *
 */

void
basic_connect_pin_list(Mbim_Token               token,
                       MbimMessageCommandType   commandType,
                       void *                   data,
                       size_t                   dataLen) {
    switch (commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            query_pin_list(token, data, dataLen);
            break;
        default:
            RLOGE("unsupported commandType");
            break;
    }
}

void
query_pin_list(Mbim_Token       token,
               void *           data,
               size_t           dataLen) {
    MBIM_UNUSED_PARM(data);
    MBIM_UNUSED_PARM(dataLen);

    int err = -1;
    int serviceClass = 0, status;
    int response[2] = {0};
    char *line = NULL;
    char cmd[AT_COMMAND_LEN] = {0};
    ATLine *p_cur = NULL;
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id  = RIL_SOCKET_1;
    int channelID = getChannel(socket_id);

    MbimPinListInfo *pinList =
            (MbimPinListInfo *)calloc(1, sizeof(MbimPinListInfo));
    if (s_pinType == MBIM_PIN_TYPE_PIN1 && s_pinState == MBIM_PIN_STATE_LOCKED) {
        pinList->pinDescPin1.pinMode = MBIM_PIN_MODE_ENABLED;
    } else {
        snprintf(cmd, sizeof(cmd), "AT+CLCK=\"%s\",2,\"\",7",  "SC");
        err = at_send_command_multiline(s_ATChannels[channelID], cmd, "+CLCK: ",
                                        &p_response);
        if (err < 0 || p_response->success == 0) {
            goto error;
        }

        for (p_cur = p_response->p_intermediates; p_cur != NULL;
             p_cur = p_cur->p_next) {
            line = p_cur->line;

            err = at_tok_start(&line);
            if (err < 0) goto error;

            err = at_tok_nextint(&line, &status);
            if (err < 0) goto error;

            if (at_tok_hasmore(&line)) {
                err = at_tok_nextint(&line, &serviceClass);
                if (err < 0) goto error;
            }
            response[0] = status;
            response[1] |= serviceClass;
        }

        pinList->pinDescPin1.pinMode =
                response[0] == 0 ? MBIM_PIN_MODE_DISABLED : MBIM_PIN_MODE_ENABLED;
    }
    pinList->pinDescPin1.pinFormat = MBIM_PIN_FORMAT_NUMERIC;
    pinList->pinDescPin1.pinLengthMin = 4;
    pinList->pinDescPin1.pinLengthMax = 8;

done:
    mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE,
            pinList, sizeof(MbimPinListInfo));
    AT_RESPONSE_FREE(p_response);
    putChannel(channelID);
    free(pinList);
    return;

error:
    mbim_device_command_done(token, MBIM_STATUS_ERROR_FAILURE,
            pinList, sizeof(MbimPinListInfo));
    AT_RESPONSE_FREE(p_response);
    putChannel(channelID);
    free(pinList);
    return;
}

/*****************************************************************************/
/**
 * basic_connect_home_provider: query only for non multi-carrier device
 *
 * query:
 *  @command:   the informationBuffer shall be MbimVisibleProvidersReq
 *  @response:  the informationBuffer shall be MbimProviders_2
 *
 */

void
basic_connect_home_provider(Mbim_Token               token,
                       MbimMessageCommandType   commandType,
                       void *                   data,
                       size_t                   dataLen) {
    switch (commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            query_home_provider(token, data, dataLen);
            break;
        default:
            RLOGE("unsupported commandType");
            break;
    }
}

int
getUsimFileCount(unsigned char const *  byteUSIM,
                 int                    len,
                 unsigned char *        hexUSIM,
                 int *                  recordSize) {
    MBIM_UNUSED_PARM(hexUSIM);

    RLOGD("getUsimFileCount");
    int desIndex = 0;
    int sizeIndex = 0;
    int i = 0;
    for (i = 0; i < len; i++) {
        if (byteUSIM[i] == USIM_FILE_DES_TAG) {
            desIndex = i;
            break;
        }
    }
    RLOGD("TYPE_FCP_DES index = %d", desIndex);
    for (i = desIndex; i < len;) {
        if (byteUSIM[i] == USIM_FILE_SIZE_TAG) {
            sizeIndex = i;
            break;
        } else {
            i += (byteUSIM[i + 1] + 2);
        }
    }
    RLOGD("TYPE_FCP_SIZE index = %d ", sizeIndex);
    int count = 0;
    if ((byteUSIM[desIndex + RESPONSE_DATA_FILE_DES_FLAG] & 0x07) ==
        EF_TYPE_TRANSPARENT) {
        RLOGD("EF_TYPE_TRANSPARENT");
        count =  (unsigned int) (((byteUSIM[sizeIndex + USIM_DATA_OFFSET_2] & 0xff) << 8)
                                |(byteUSIM[sizeIndex + USIM_DATA_OFFSET_3] & 0xff));
        RLOGD("count = ...%d", count);
        return count;
    } else if ((byteUSIM[desIndex + RESPONSE_DATA_FILE_DES_FLAG] & 0x07) ==
                EF_TYPE_LINEAR_FIXED || (byteUSIM[desIndex + RESPONSE_DATA_FILE_DES_FLAG] & 0x07) ==
                        EF_TYPE_CYCLIC) {
        RLOGD("EF_TYPE_LINEAR_FIXED OR EF_TYPE_CYCLIC");
        if (USIM_FILE_DES_TAG != byteUSIM[RESPONSE_DATA_FILE_DES_FLAG]) {
            RLOGD("USIM_FILE_DES_TAG != ...");
            goto error;
        }
        if (TYPE_FILE_DES_LEN != byteUSIM[RESPONSE_DATA_FILE_DES_LEN_FLAG]) {
            RLOGD("TYPE_FILE_DES_LEN != ...");
            goto error;
        }
        *recordSize = (unsigned int) (((byteUSIM[RESPONSE_DATA_FILE_RECORD_LEN_1] & 0xff) << 8) |
                (byteUSIM[RESPONSE_DATA_FILE_RECORD_LEN_2] & 0xff));
        count= (unsigned int)(((byteUSIM[sizeIndex + USIM_DATA_OFFSET_2] & 0xff) << 8) |
                (byteUSIM[sizeIndex + USIM_DATA_OFFSET_3] & 0xff))/ *recordSize;
        RLOGD("recordSize = ...%d", *recordSize);
        RLOGD("count = ...%d", count);
        return count;
    } else {
        RLOGE("Unknown EF type %d ", (byteUSIM[desIndex + RESPONSE_DATA_FILE_DES_FLAG] & 0x07));
        return count;
    }

error:
    RLOGE("getUsimFileCount error, return 0");
    return 0;
}

const char *MCCMNC_CODES_HAVING_3DIGITS_MNC[152] = {
        "302370", "302720", "310260", "405025", "405026", "405027", "405028", "405029",
        "405030", "405031", "405032", "405033", "405034", "405035", "405036", "405037",
        "405038", "405039", "405040", "405041", "405042", "405043", "405044", "405045",
        "405046", "405047", "405750", "405751", "405752", "405753", "405754", "405755",
        "405756", "405799", "405800", "405801", "405802", "405803", "405804", "405805",
        "405806", "405807", "405808", "405809", "405810", "405811", "405812", "405813",
        "405814", "405815", "405816", "405817", "405818", "405819", "405820", "405821",
        "405822", "405823", "405824", "405825", "405826", "405827", "405828", "405829",
        "405830", "405831", "405832", "405833", "405834", "405835", "405836", "405837",
        "405838", "405839", "405840", "405841", "405842", "405843", "405844", "405845",
        "405846", "405847", "405848", "405849", "405850", "405851", "405852", "405853",
        "405854", "405855", "405856", "405857", "405858", "405859", "405860", "405861",
        "405862", "405863", "405864", "405865", "405866", "405867", "405868", "405869",
        "405870", "405871", "405872", "405873", "405874", "405875", "405876", "405877",
        "405878", "405879", "405880", "405881", "405882", "405883", "405884", "405885",
        "405886", "405908", "405909", "405910", "405911", "405912", "405913", "405914",
        "405915", "405916", "405917", "405918", "405919", "405920", "405921", "405922",
        "405923", "405924", "405925", "405926", "405927", "405928", "405929", "405930",
        "405931", "405932", "502142", "502143", "502145", "502146", "502147", "502148"
    };

typedef struct MCCTABLE {
    int mcc;
    int mncLength;
} MccTable;

static MccTable MCC_TABLE[] = {
        {722, 3},
        {708, 3},
        {348, 3},
        {346, 3},
        {344, 3},
        {342, 3},
        {340, 3},
        {338, 3},
        {334, 3},
        {316, 3},
        {315, 3},
        {314, 3},
        {313, 3},
        {312, 3},
        {311, 3},
        {310, 3},
        {302, 3},
};

int
getMncLength(int        channelID,
             char *     imsi) {
    int usimLen = 0;
    int err;
    int sw1 = 0, sw2 = 0;
    int mncLength = 2;
    char *cmd = NULL;
    char *line = NULL, *rsp = NULL;

    RIL_AppType simType = getSimType(channelID);
    // get IccType
    if (simType == RIL_APPTYPE_USIM) {
        RLOGD("usim, get from 7FFF");
        err = iccExchangeSimIO(COMMAND_GET_RESPONSE, 28589, DF_ADF, 0, 0,
                GET_RESPONSE_EF_SIZE_BYTES, NULL, NULL, NULL, &rsp, channelID);
    } else if (simType == RIL_APPTYPE_SIM) {
        RLOGD("sim, get from 7F20");
        err = iccExchangeSimIO(COMMAND_GET_RESPONSE, 28589, DF_GSM, 0, 0,
                GET_RESPONSE_EF_SIZE_BYTES, NULL, NULL, NULL, &rsp, channelID);
    } else {
        RLOGE("unknown type");
        goto ERROR_READ_EFAD_INFO;
    }
    if (err < 0) {
        goto ERROR_READ_EFAD_INFO;
    }
    RLOGD("rsp = %s", rsp);
    if (rsp == NULL) {
        goto ERROR_READ_EFAD_INFO;
    }

    int recordSize = 0;
    int count = getFileCount(rsp, &recordSize, channelID);
    if (0 == count) {
        RLOGE("unable getFileCount, return error");
        goto ERROR_READ_EFAD_INFO;
    }

    free(rsp);
    rsp = NULL;

    // read EF_AD content
    if (simType == RIL_APPTYPE_USIM) {
        RLOGD("usim, get from 7FFF");
        err = iccExchangeSimIO(COMMAND_READ_BINARY, 28589, DF_ADF, 0, 0, count,
                NULL, NULL, NULL, &rsp, channelID);
    } else if (simType == RIL_APPTYPE_SIM) {
        RLOGD("sim, get from 7F20");
        err = iccExchangeSimIO(COMMAND_READ_BINARY, 28589, DF_GSM, 0, 0, count,
                NULL, NULL, NULL, &rsp, channelID);
    }
    if (err < 0) {
        goto ERROR_READ_EFAD_INFO;
    } else {
        RLOGD("FILE_AD %s", rsp);
        mncLength = (hexCharToInt(rsp[6]) << 4) | (hexCharToInt(rsp[7]));
        RLOGD("readMccMnc, mccLength = %d", mncLength);
        if (mncLength != 2 && mncLength != 3) {
            goto ERROR_READ_EFAD_INFO;
        }
        goto READ_EF_AD_INFO;
    }

ERROR_READ_EFAD_INFO:
    RLOGE("unable to read EF_AD, get length from the table");
    int len = sizeof(MCCMNC_CODES_HAVING_3DIGITS_MNC) / sizeof(MCCMNC_CODES_HAVING_3DIGITS_MNC[0]);
    int i;
    for (i = 0; i < len; i++) {
        if (strncmp(imsi, MCCMNC_CODES_HAVING_3DIGITS_MNC[i], 6) == 0) {
            break;
        }
    }
    if (i < len) {
        RLOGE("in MCCMNC_CODES_HAVING_3DIGITS_MNC table");
        mncLength = 3;
        goto READ_EF_AD_INFO;
    }
    int mcc = (hexCharToInt(imsi[0]) * 100) + (hexCharToInt(imsi[1]) * 10)
            + hexCharToInt(imsi[2]);
    int mcc_table_len = sizeof(MCC_TABLE) / sizeof(MCC_TABLE[0]);
    for (i = 0; i < mcc_table_len; i++) {
        if (MCC_TABLE[i].mcc == mcc) {
            mncLength = MCC_TABLE[i].mncLength;
            RLOGD(" MCC_TABLE: mcc = %d, mncLength = %d", mcc, mncLength);
            break;
        }
    }
    if (i < mcc_table_len) {
        RLOGE("in MCC_TABLE table");
        mncLength = 3;
    }

READ_EF_AD_INFO:
    free(rsp);
    rsp = NULL;
    return mncLength;
}

int
getProviderName(int             channelID,
                unsigned char * providerName) {

    char *line = NULL, *rsp = NULL;
    int usimLen = 0;
    unsigned char *byteUSIM = NULL;
    int err;
    int sw1 = 0, sw2 = 0;
    char *cmd = NULL;
    ATResponse *p_response = NULL;

    err = iccExchangeSimIO(COMMAND_GET_RESPONSE, 28486, DF_ADF, 0, 0,
            GET_RESPONSE_EF_SIZE_BYTES, NULL, NULL, NULL, &rsp, channelID);
    RLOGD("rsp = %s", rsp);
    if (rsp == NULL) {
        goto ERROR_READ_EF_SPN;
    }

    int recordSize = 0;
    int count = getFileCount(rsp, &recordSize, channelID);
    if (0 == count) {
        RLOGE("unable getFileCount, return error");
        goto ERROR_READ_EF_SPN;
    }

    free(rsp);
    rsp = NULL;

    // read EF_SPN content
    err = iccExchangeSimIO(COMMAND_READ_BINARY, 28486, DF_ADF, 0, 0, count,
            NULL, NULL, NULL, &rsp, channelID);
    if (err < 0) {
        goto ERROR_READ_EF_SPN;
    } else {
        RLOGD("FILE_SPN %s", rsp);
        unsigned char bytes[32] = { 0 };
        hexStringToBytes(rsp, bytes);
        int result = adnStringFieldToString(bytes + 1, bytes + strlen(rsp) / 2,
                providerName);
        RLOGD("providerName = %s, result = %d", providerName, result);
    }
    free(rsp);
    rsp = NULL;
    at_response_free(p_response);
    return 0;

ERROR_READ_EF_SPN:
    RLOGE("unable to read EF_SPN");
    free(rsp);
    rsp = NULL;
    at_response_free(p_response);
    return -1;
}

int
getHomeProvider(MbimProvider_2 *provider) {
    int err = -1;
    int mncLength;
    char *providerId = NULL;
    char *line = NULL;
    char *imsi;
    ATResponse *p_response = NULL;

    unsigned char *providerName  = (unsigned char *)calloc(20, sizeof(unsigned char));

    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    int channelID = getChannel(socket_id);

    // get providerId
    err = at_send_command_numeric(s_ATChannels[channelID], "AT+CIMI",
            &p_response);
    if (err < 0 || p_response->success == 0) {
        RLOGE("error for get imsi");
        goto error;
    } else {
        imsi = p_response->p_intermediates->line;
        RLOGD("imis is %s", imsi);
        mncLength = getMncLength(channelID, imsi);
        RLOGD("mncLength is %d", mncLength);
        int mccLength = 3;
        providerId = (char *)calloc(mccLength + mncLength + sizeof("\0"), sizeof(char));
        strncpy(providerId, imsi, mccLength + mncLength);

        RLOGD("providerId is %s", providerId);
        provider->providerId = providerId;

        snprintf(s_providerId, sizeof(s_providerId), "%s", providerId);
    }

    // get provider name
    getProviderName(channelID, providerName);
    provider->providerName = providerName;

done:
    AT_RESPONSE_FREE(p_response);
    putChannel(channelID);
    return MBIM_DEVICE_PROCESS_SUCCESS;

error:
    AT_RESPONSE_FREE(p_response);
    putChannel(channelID);
    return MBIM_DEVICE_PROCESS_FAIL;
}

void
query_home_provider(Mbim_Token          token,
                    void *              data,
                    size_t              dataLen) {
    MBIM_UNUSED_PARM(data);
    MBIM_UNUSED_PARM(dataLen);

    int err = -1;
    MbimStatusError statusError = MBIM_STATUS_ERROR_NONE;

    MbimProvider_2 *homeProvider = (MbimProvider_2 *)calloc(1, sizeof(MbimProvider_2));

    homeProvider->celluarClass = 0;
    homeProvider->rssi = 0;
    homeProvider->errorRate = 0;
    homeProvider->providerState = MBIM_PROVIDER_STATE_HOME;

    err = getHomeProvider(homeProvider);
    if (err != MBIM_DEVICE_PROCESS_SUCCESS) {
        statusError = MBIM_STATUS_ERROR_FAILURE;
    }

    mbim_device_command_done(token, statusError, homeProvider, sizeof(MbimProvider_2));
    free(homeProvider->providerId);
    free(homeProvider->providerName);
    free(homeProvider);
}

/*****************************************************************************/
/**
 * basic_connect_preferred_providers: set query and notification
 *
 * query:
 *  @command:   the informationBuffer shall be MbimVisibleProvidersReq
 *  @response:  the informationBuffer shall be MbimProviders_2
 *
 */

void
basic_connect_preferred_providers(Mbim_Token                token,
                                  MbimMessageCommandType    commandType,
                                  void *                    data,
                                  size_t                    dataLen) {
    switch (commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            query_preferred_providers(token, data, dataLen);
            break;
        default:
            RLOGE("unsupported commandType");
            break;
    }
}

int
getPreferredProviders(MbimProviders_2 *providers) {
    int err = -1;
    int mncLength;
    char *line = NULL;
    char *imsi = NULL;
    ATResponse *p_response = NULL;

    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    int channelID = getChannel(socket_id);

    // get providerId
    char *preferredProviders = NULL;
    err = iccExchangeSimIO(COMMAND_GET_RESPONSE, 28512, DF_ADF, 0, 0,
            GET_RESPONSE_EF_SIZE_BYTES, NULL, NULL, NULL, &preferredProviders, channelID);
    if (err < 0) {
        RLOGE("Failed to get preferredProviders");
    } else {
        RLOGD("preferredProviders = %s", preferredProviders);
        if (preferredProviders == NULL) goto error;

        int recordSize = 0;
        int count = getFileCount(preferredProviders, &recordSize, channelID);
        if (0 == count) {
            RLOGE("unable getFileCount, return error");
            goto error;
        }

        free(preferredProviders);
        preferredProviders = NULL;

        // read EF_ICCID content
        err = iccExchangeSimIO(COMMAND_READ_BINARY, 28512, DF_ADF, 0, 0,
                count, NULL, NULL, NULL, &preferredProviders, channelID);
        RLOGD("preferredProviders = %s", preferredProviders);

        int LEN_UNIT = 5;
        count = strlen(preferredProviders) / 2 / LEN_UNIT;

        providers->provider = (MbimProvider_2 **)calloc(count, sizeof(MbimProvider_2 *));

        unsigned char *byteRsp = NULL;
        int len = strlen(preferredProviders) / 2;
        byteRsp = (unsigned char *)malloc(len + sizeof(char));
        hexStringToBytes(preferredProviders, byteRsp);

        for (int i = 0; i < count; i++) {
            if ((byteRsp[i * LEN_UNIT] & 0xff) != 0xff) {
                char *providerId = (char *)calloc(7, sizeof(char));
                spdiPlmnBcdToString(byteRsp, i * LEN_UNIT, providerId);
                RLOGD("preferredProvider = %s", providerId);
                providers->provider[providers->elementCount] =
                        (MbimProvider_2 *)calloc(1, sizeof(MbimProvider_2));
                providers->provider[providers->elementCount]->celluarClass =
                        MBIM_CELLULAR_CLASS_GSM;
                providers->provider[providers->elementCount]->providerState =
                        MBIM_PROVIDER_STATE_PREFERRED;
                providers->provider[providers->elementCount]->providerId =
                        providerId;
                providers->elementCount++;
            }
        }

        free(byteRsp);
    }

done:
    AT_RESPONSE_FREE(p_response);
    putChannel(channelID);
    if (preferredProviders != NULL) {
        free(preferredProviders);
    }
    return MBIM_DEVICE_PROCESS_SUCCESS;

error:
    AT_RESPONSE_FREE(p_response);
    putChannel(channelID);
    if (preferredProviders != NULL) {
        free(preferredProviders);
    }
    return MBIM_DEVICE_PROCESS_FAIL;
}

void
query_preferred_providers(Mbim_Token        token,
                          void *            data,
                          size_t            dataLen) {
    MBIM_UNUSED_PARM(data);
    MBIM_UNUSED_PARM(dataLen);

    int err = -1;
    MbimStatusError statusError = MBIM_STATUS_ERROR_NONE;

    MbimProviders_2 *preferredProviders =
            (MbimProviders_2 *)calloc(1, sizeof(MbimProviders_2));

    err = getPreferredProviders(preferredProviders);
    if (err != MBIM_DEVICE_PROCESS_SUCCESS) {
        statusError = MBIM_STATUS_ERROR_FAILURE;
    }
    mbim_device_command_done(token, statusError, preferredProviders,
            sizeof(MbimProviders_2));
    for (uint32_t i = 0; i < preferredProviders->elementCount; i++) {
        free (preferredProviders->provider[i]);
    }
    free(preferredProviders->provider);
    free(preferredProviders);
}

/*****************************************************************************/
/**
 * basic_connect_visible_providers: query only
 *
 * query:
 *  @command:   the informationBuffer shall be MbimVisibleProvidersReq
 *  @response:  the informationBuffer shall be MbimProviders_2
 *
 */
void
basic_connect_visible_providers(Mbim_Token                  token,
                                MbimMessageCommandType      commandType,
                                void *                      data,
                                size_t                      dataLen) {
    switch (commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            query_visible_providers(token, data, dataLen);
            break;
        default:
            RLOGE("unsupported commandType");
            break;
    }
}

MbimProviderState
mapProviderState(int state) {
    switch (state) {
        case 0:     // unknown;
            return MBIM_PROVIDER_STATE_UNKNOWN;
        case 1:     // available
            return MBIM_PROVIDER_STATE_VISIBLE;
        case 2:     // currently used
            return MBIM_PROVIDER_STATE_REGISTERED;
        case 3:     // forbidden
            return MBIM_PROVIDER_STATE_FORBIDDEN;
        default:    // unknown
            return MBIM_PROVIDER_STATE_UNKNOWN;
    }
}

void
query_visible_providers(Mbim_Token              token,
                        void *                  data,
                        size_t                  dataLen) {
    MBIM_UNUSED_PARM(dataLen);

    // FixMe ignore action
    MbimVisibleProvidersReq *visibleProvidersReq = (MbimVisibleProvidersReq *)data;

    int err, state, act;
    int tok = 0, count = 0, i = 0;
    char *line = NULL, *sskip = NULL;
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    MbimProviders_2 *visibleProviders = NULL;
    MbimStatusError statusError = MBIM_STATUS_ERROR_NONE;

    int channelID = getChannel(socket_id);

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+COPS=?",
                                     "+COPS:", &p_response);
    if (err != 0 || p_response->success == 0) {
        statusError = MBIM_STATUS_ERROR_FAILURE;
        goto out;
    }

    line = p_response->p_intermediates->line;

    while (*line) {
        if (*line == '(')
            tok++;
        if (*line  == ')') {
            if (tok == 1) {
                count++;
                tok--;
            }
        }
        if (*line == '"') {
            do {
                line++;
                if (*line == 0)
                    break;
                else if (*line == '"')
                    break;
            } while (1);
        }
        if (*line != 0)
            line++;
    }
    RLOGD("Searched available network list numbers = %d", count - 2);
    if (count <= 2) {
        statusError = MBIM_STATUS_ERROR_FAILURE;
        goto out;
    }
    count -= 2;

    line = p_response->p_intermediates->line;
    // (,,,,),,(0-4),(0-2)
    if (strstr(line, ",,,,")) {
        RLOGD("No network");
        statusError = MBIM_STATUS_ERROR_FAILURE;
        goto out;
    }

    // (1,"CHINA MOBILE","CMCC","46000",0), (2,"CHINA MOBILE","CMCC","46000",2),
    // (3,"CHN-UNICOM","CUCC","46001",0),,(0-4),(0-2)
    visibleProviders = (MbimProviders_2 *)calloc(1, sizeof(MbimProviders_2));
    visibleProviders->elementCount = count;
    visibleProviders->provider = (MbimProvider_2 **)calloc(count, sizeof(MbimProvider_2 *));
    for (i = 0; i < count; i++) {
        visibleProviders->provider[i] = (MbimProvider_2 *)calloc(1, sizeof(MbimProvider_2));
    }

    i = 0;
    while ((line = strchr(line, '(')) && (i < count)) {
        line++;
        err = at_tok_nextint(&line, &state);
        if (err < 0) continue;

        visibleProviders->provider[i]->providerState = mapProviderState(state);

        err = at_tok_nextstr(&line, &sskip);
        if (err < 0) continue;

        err = at_tok_nextstr(&line, &(visibleProviders->provider[i]->providerName));
        if (err < 0) continue;

        err = at_tok_nextstr(&line, &(visibleProviders->provider[i]->providerId));
        if (err < 0) continue;

        err = at_tok_nextint(&line, &act);
        if (err < 0) continue;

        visibleProviders->provider[i]->celluarClass = MBIM_CELLULAR_CLASS_GSM;
        visibleProviders->provider[i]->errorRate = 0;
        visibleProviders->provider[i]->rssi = 0;

        i++;
    }

out:
    if (statusError == MBIM_STATUS_ERROR_NONE) {
        mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE,
                visibleProviders, sizeof(MbimProviders_2));
    } else {
        mbim_device_command_done(token, MBIM_STATUS_ERROR_FAILURE, NULL, 0);
    }

    at_response_free(p_response);
    if (visibleProviders != NULL) {
        for (i = 0; i < count; i++) {
            free(visibleProviders->provider[i]);
        }
        free(visibleProviders->provider);
        free(visibleProviders);
    }

    putChannel(channelID);
}

/******************************************************************************/
/**
 * basic_connect_register_state: query, set and notification supported
 *
 * query:
 *  @command:   the informationBuffer shall be null
 *  @response:  the informationBuffer shall be MbimRegistrationStateInfo_2
 *
 * set:
 *  @command:   the informationBuffer shall be MbimSetRegistrationState
 *  @response:  the informationBuffer shall be MbimRegistrationStateInfo_2
 *
 * notification:
 *  @command:   NA
 *  @response:  the informationBuffer shall be MbimRegistrationStateInfo_2
 */

void
basic_connect_register_state(Mbim_Token                 token,
                             MbimMessageCommandType     commandType,
                             void *                     data,
                             size_t                     dataLen) {
    switch (commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            query_register_state(token, data, dataLen);
            break;
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            set_register_state(token, data, dataLen);
            break;
        default:
            RLOGE("unsupported commandType");
            break;
    }
}

static MbimRegisterState
mapRegState(int inResponse) {
    MbimRegisterState outResponse = MBIM_REGISTER_STATE_UNKNOWN;

    switch (inResponse) {
        case 0:
            outResponse = MBIM_REGISTER_STATE_UNKNOWN;
            break;
        case 1:
            outResponse = MBIM_REGISTER_STATE_HOME;
            break;
        case 2:
            outResponse = MBIM_REGISTER_STATE_SEARCHING;
            break;
        case 3:
            outResponse = MBIM_REGISTER_STATE_DENIED;
            break;
        case 4:
            outResponse = MBIM_REGISTER_STATE_UNKNOWN;
            break;
        case 5:
            outResponse = MBIM_REGISTER_STATE_ROAMING;
            break;
        default:
            outResponse = MBIM_REGISTER_STATE_UNKNOWN;
            break;
    }
    return outResponse;
}

static MbimDataClass
mapNetType(int in_response) {
    MbimDataClass out_response = MBIM_DATA_CLASS_NONE;

    switch (in_response) {
        case 0:
            out_response = MBIM_DATA_CLASS_GPRS;    /* GPRS */
            break;
        case 3:
            out_response = MBIM_DATA_CLASS_EDGE;    /* EDGE */
            break;
        case 2:
            out_response = MBIM_DATA_CLASS_UMTS;    /* TD, no TD set to  UMTS*/
            break;
        case 4:
            out_response = MBIM_DATA_CLASS_HSDPA;   /* HSDPA */
            break;
        case 5:
            out_response = MBIM_DATA_CLASS_HSUPA;   /* HSUPA */
            break;
        case 6:
            out_response = MBIM_DATA_CLASS_HSDPA;   /* HSPA, no HSPA set to MBIM_DATA_CLASS_HSDPA */
            break;
        case 15:
            out_response = MBIM_DATA_CLASS_HSDPA;   /* HSPA+, no HSPA+ set to MBIM_DATA_CLASS_HSDPA*/
            break;
        case 7:
            out_response = MBIM_DATA_CLASS_LTE;     /* LTE */
            break;
        case 16:
            out_response = MBIM_DATA_CLASS_LTE;     /* LTE_CA, no LTE_CA set to MBIM_DATA_CLASS_HSDPA*/
            break;
        default:
            out_response = MBIM_DATA_CLASS_NONE;    /* UNKNOWN */
            break;
    }
    return out_response;
}

static int mapDataClass(MbimDataClass in_data_class) {
    MbimDataClass out_act = -1;
    switch (in_data_class) {
        case MBIM_DATA_CLASS_GPRS:
            out_act = 0;
            break;
        case MBIM_DATA_CLASS_EDGE:
            out_act = 3;
            break;
        case MBIM_DATA_CLASS_UMTS:
            out_act = 0;
            break;
        case MBIM_DATA_CLASS_HSDPA:
            out_act = 2;
            break;
        case MBIM_DATA_CLASS_HSUPA:
            out_act = 5;
            break;
        case MBIM_DATA_CLASS_LTE:
            out_act = 7;
            break;
        default:
            break;
    }
    return out_act;
}

int
getRegistrationStateInfo(MbimRegistrationStateInfo_2 *registerState, int channelID) {
    int err = -1;
    int commas = 0, skip = 0;
    int regMode, regState, netType;
    char *line = NULL, *p = NULL;
    ATResponse *p_response = NULL;

    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;

    // get register Mode: AUTOMATIC or MANUAL
    err = at_send_command_singleline(s_ATChannels[channelID], "AT+COPS?",
            "+COPS:", &p_response);

    if (err < 0 || p_response->success == 0) {
        RLOGE("err < 0 || p_response->success == 0");
        goto error;
    }

    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &regMode);
    if (err < 0) goto error;

    if (regMode == 0) {
        registerState->registerMode = MBIM_REGISTER_MODE_AUTOMATIC;
    } else if (regMode == 1) {
        registerState->registerMode = MBIM_REGISTER_MODE_MANUAL;
    } else {
        registerState->registerMode = MBIM_REGISTER_MODE_UNKNOWN;
    }

    // get register state
    AT_RESPONSE_FREE(p_response);
    err = at_send_command_singleline(s_ATChannels[channelID], "AT+CEREG?",
            "+CEREG:", &p_response);
    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0)
        goto error;

    for (p = line; *p != '\0'; p++) {
        if (*p == ',')
            commas++;
    }
    if (commas == 4) {
        err = at_tok_nextint(&line, &skip);
        if (err < 0)
            goto error;
        err = at_tok_nextint(&line, &regState);
        if (err < 0)
            goto error;
        err = at_tok_nexthexint(&line, &skip);
        if (err < 0)
            goto error;
        err = at_tok_nexthexint(&line, &skip);
        if (err < 0)
            goto error;
        err = at_tok_nextint(&line, &netType);
        if (err < 0)
            goto error;
        registerState->registerState = mapRegState(regState);
        s_regState = registerState->registerState;
        if (registerState->registerState == MBIM_REGISTER_STATE_HOME
                || registerState->registerState == MBIM_REGISTER_STATE_ROAMING
                || registerState->registerState
                        == MBIM_REGISTER_STATE_PARTNER) {
            registerState->availableDataClasses = mapNetType(netType);
            s_availableDataClasses = registerState->availableDataClasses;
        } else {
            registerState->availableDataClasses = MBIM_DATA_CLASS_NONE;
            s_availableDataClasses = registerState->availableDataClasses;
        }
    } else {
        registerState->registerState = MBIM_REGISTER_STATE_UNKNOWN;
        s_regState = registerState->registerState;
        registerState->availableDataClasses = MBIM_DATA_CLASS_NONE;
    }

    registerState->currentCellularClass = MBIM_CELLULAR_CLASS_GSM;

    // get serviceId and providerName
    if (registerState->registerMode == MBIM_REGISTER_MODE_AUTOMATIC) {
        AT_RESPONSE_FREE(p_response);
        err = at_send_command_multiline(s_ATChannels[channelID],
                "AT+COPS=3,0;+COPS?;+COPS=3,1;+COPS?;+COPS=3,2;+COPS?",
                "+COPS:", &p_response);
        if (err != 0) goto error;

        int i;
        ATLine *p_cur = NULL;
        char *response[3] = {NULL, NULL, NULL};

        for (i = 0, p_cur = p_response->p_intermediates; p_cur != NULL; p_cur =
                p_cur->p_next, i++) {
            char *line = p_cur->line;

            err = at_tok_start(&line);
            if (err < 0)
                goto error;

            err = at_tok_nextint(&line, &skip);
            if (err < 0)
                goto error;

            /* If we're unregistered, we may just get
             * a "+COPS: 0" response
             */
            if (!at_tok_hasmore(&line)) {
                response[i] = NULL;
                continue;
            }

            err = at_tok_nextint(&line, &skip);
            if (err < 0)
                goto error;

            /* a "+COPS: 0, n" response is also possible */
            if (!at_tok_hasmore(&line)) {
                response[i] = NULL;
                continue;
            }

            err = at_tok_nextstr(&line, &(response[i]));
            if (err < 0)
                goto error;
        }

        if (i != 3) {
            /* expect 3 lines exactly */
            goto error;
        }

        // FixMe just get the providerName from the AT response, get the providerName follow 3GPP
        if (response[2] != NULL) {
            registerState->providerId = strdup(response[2]);
        }
        if (response[0] != NULL) {
            registerState->providerName = strdup(response[0]);
        } else if (response[1] != NULL) {
            registerState->providerName = strdup(response[1]);
        }
    } else if (registerState->registerMode == MBIM_REGISTER_MODE_MANUAL) {
        // FixMe return the provider Id that the device is requested to register with
    }

    // FixMe roamingText is optional, so not support it now

    // FixMe set to none, if necessary reset it
    registerState->registrationFlag = MBIM_REGISTRATION_FLAG_NONE;

done:
    AT_RESPONSE_FREE(p_response);
    RLOGE("getRegistrationStateInfo done");
    return 1;

error:
    AT_RESPONSE_FREE(p_response);
    RLOGE("getRegistrationStateInfo error");
    return -1;
}

void
query_register_state(Mbim_Token     token,
                     void *         data,
                     size_t         dataLen) {
    MBIM_UNUSED_PARM(data);
    MBIM_UNUSED_PARM(dataLen);

    int err = -1;
    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    int channelID = getChannel(socket_id);
    MbimRegistrationStateInfo_2 *registerState = (MbimRegistrationStateInfo_2 *)
            calloc(1, sizeof(MbimRegistrationStateInfo_2));

    err = getRegistrationStateInfo(registerState, channelID);
    if (err < 0) goto error;

done:
    mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE,
            registerState, sizeof(MbimRegistrationStateInfo_2));

    memsetAndFreeStrings(3, registerState->providerId,
            registerState->providerName, registerState->roamingText);
    free(registerState);
    putChannel(channelID);
    return;

error:
    mbim_device_command_done(token, MBIM_STATUS_ERROR_FAILURE,
            registerState, sizeof(MbimRegistrationStateInfo_2));

    memsetAndFreeStrings(3, registerState->providerId,
            registerState->providerName, registerState->roamingText);
    free(registerState);
    putChannel(channelID);
    return;
}

void
set_register_state(Mbim_Token           token,
                   void *               data,
                   size_t               dataLen) {
    MBIM_UNUSED_PARM(dataLen);

    MbimSetRegistrationState_2 *setRegistrationState = (MbimSetRegistrationState_2 *)data;

    int err = -1;;
    char cmd[AT_COMMAND_LEN] = {0};
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    int channelID = getChannel(socket_id);

    MbimRegistrationStateInfo_2 *registrationStateInfo =
            (MbimRegistrationStateInfo_2 *)calloc(1, sizeof(MbimRegistrationStateInfo_2));

    // set to register automatic
    if (setRegistrationState->registerAction == MBIM_REGISTER_ACTION_AUTOMATIC) {
        err = at_send_command(s_ATChannels[channelID], "AT+COPS=0", &p_response);
        if (err >= 0 && p_response->success) {
            AT_RESPONSE_FREE(p_response);
            err = at_send_command(s_ATChannels[channelID], "AT+CGAUTO=1",
                                  &p_response);
        }
        if (err < 0 || p_response->success == 0) {
            goto error;
        } else {
            err = getRegistrationStateInfo(registrationStateInfo, channelID);
            if (err < 0) goto error;
        }
    } else if (setRegistrationState->registerAction == MBIM_REGISTER_ACTION_MANUAL) {
        int act = mapDataClass(setRegistrationState->dataClass);
        if (act >= 0) {
            snprintf(cmd, sizeof(cmd), "AT+COPS=1,2,\"%s\",%d", setRegistrationState->providerId, act);
        } else {
            snprintf(cmd, sizeof(cmd), "AT+COPS=1,2,\"%s\"", setRegistrationState->providerId);
        }
        err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
        if (err != 0 || p_response->success == 0) {
            goto error;
        } else {
            err = getRegistrationStateInfo(registrationStateInfo, channelID);
            if (err < 0) goto error;
        }
    }

done:
    mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE, registrationStateInfo,
            sizeof(MbimRegistrationStateInfo_2));
    free(registrationStateInfo);
    putChannel(channelID);
    at_response_free(p_response);
    return;

error:
    mbim_device_command_done(token, MBIM_STATUS_ERROR_FAILURE, NULL, 0);
    at_response_free(p_response);
    putChannel(channelID);
    free(registrationStateInfo);
    return;
}

/****************************************************************************/
/**
 * basic_connect_packet_service: query, set and notification supported
 *
 * query:
 *  @command:   the informationBuffer shall be null
 *  @response:  the informationBuffer shall be MbimPacketServiceInfo
 *
 * set:
 *  @command:   the informationBuffer shall be MbimSetPacketService
 *  @response:  the informationBuffer shall be MbimPacketServiceInfo
 *
 * notification:
 *  @command:   NA
 *  @response:  the informationBuffer shall be MbimPacketServiceInfo
 */
void
basic_connect_packet_service(Mbim_Token                token,
                             MbimMessageCommandType    commandType,
                             void *                    data,
                             size_t                    dataLen) {

    switch (commandType) {
         case MBIM_MESSAGE_COMMAND_TYPE_SET:
             set_packet_service(token, data, dataLen);
             break;
         case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
             query_packet_service(token, data, dataLen);
             break;
         default:
             RLOGE("unsupported commandType");
             break;
     }
}

void
attachGPRS(int channelID) {
    RLOGD("attachGPRS");
    int err;
    ATResponse *p_response = NULL;

    // TODO
    err = at_send_command(s_ATChannels[channelID], "AT+CGATT=1",
                          &p_response);
    if (err < 0 || p_response->success == 0) {
        RLOGE("attachGPRS fail");
    }
    AT_RESPONSE_FREE(p_response);
}

void
detachGPRS(int channelID) {
    RLOGD("detachGPRS");
    int err;
    ATResponse *p_response = NULL;

    // TODO
    err = at_send_command(s_ATChannels[channelID], "AT+CGATT=0",
                          &p_response);
    if (err < 0 || p_response->success == 0) {
        RLOGE("detachGPRS fail");
    }
    AT_RESPONSE_FREE(p_response);
}

/*type: 0  down link speed
 *      1  up link speed
 */
int getLinkSpeed(int type){
    int speed = 0;
    if (type == 0) {
        if (s_availableDataClasses == MBIM_DATA_CLASS_LTE) {
             speed = 300 * 1024* 1024;  // LTE 300Mbps
        } else {
            speed = 21 * 1024* 1024;// W 21Mbps
        }
    } else if (type == 1) {
        if (s_availableDataClasses == MBIM_DATA_CLASS_LTE) {
            speed = 150 * 1024* 1024;  // LTE 150Mbps
        } else {
            speed = 5.76 * 1024* 1024;// W 5.76Mbps
        }
    }
    return speed;
}

void set_packet_service(Mbim_Token                token,
                        void *                    data,
                        size_t                    dataLen) {
    MBIM_UNUSED_PARM(dataLen);

    RIL_SOCKET_ID socket_id  = RIL_SOCKET_1;
    int channelID = getChannel(socket_id);
    MbimSetPacketService *setPacketService = (MbimSetPacketService *)data;

    if (setPacketService->packetServiceAction == MBIM_PACKET_SERVICE_ACTION_ATTACH) {
        attachGPRS(channelID);
    } else if (setPacketService->packetServiceAction == MBIM_PACKET_SERVICE_ACTION_DETACH) {
        detachGPRS(channelID);
    } else {
        RLOGE("unknown packet service set operation: %d", setPacketService->packetServiceAction);
    }

    MbimPacketServiceInfo *packetServiceInfo =
                (MbimPacketServiceInfo *)calloc(1, sizeof(MbimPacketServiceInfo));
    packetServiceInfo->nwError = MBIM_NW_ERROR_UNKNOWN;
    packetServiceInfo->highestAvailableDataClass = MBIM_DATA_CLASS_NONE;
    if (s_regState == MBIM_REGISTER_STATE_HOME ||
            s_regState == MBIM_REGISTER_STATE_ROAMING ||
            s_regState == MBIM_REGISTER_STATE_PARTNER) {
        packetServiceInfo->packetServiceState = MBIM_PACKET_SERVICE_STATE_ATTACHED;
        packetServiceInfo->highestAvailableDataClass = s_availableDataClasses;
        packetServiceInfo->upLinkSpeed = getLinkSpeed(UP_LINK_SPEED);
        packetServiceInfo->downLinkSpeed = getLinkSpeed(DOWN_LINK_SPEED);
    } else if (s_regState == MBIM_REGISTER_STATE_DEREGISTERED ||
            s_regState == MBIM_REGISTER_STATE_SEARCHING ||
            s_regState == MBIM_REGISTER_STATE_DENIED){
        packetServiceInfo->packetServiceState = MBIM_PACKET_SERVICE_STATE_DETACHED;
    }
    mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE, packetServiceInfo,
            sizeof(MbimPacketServiceInfo));
    free(packetServiceInfo);
    putChannel(channelID);
}

void query_packet_service(Mbim_Token                token,
                          void *                    data,
                          size_t                    dataLen) {
    MBIM_UNUSED_PARM(data);
    MBIM_UNUSED_PARM(dataLen);

    RIL_SOCKET_ID socket_id  = RIL_SOCKET_1;
    int channelID = getChannel(socket_id);

    MbimPacketServiceInfo *packetServiceInfo =
                (MbimPacketServiceInfo *)calloc(1, sizeof(MbimPacketServiceInfo));
    packetServiceInfo->nwError = MBIM_NW_ERROR_UNKNOWN;
    packetServiceInfo->highestAvailableDataClass = MBIM_DATA_CLASS_NONE;
    if (s_regState == MBIM_REGISTER_STATE_HOME ||
            s_regState == MBIM_REGISTER_STATE_ROAMING ||
            s_regState == MBIM_REGISTER_STATE_PARTNER) {
        packetServiceInfo->packetServiceState = MBIM_PACKET_SERVICE_STATE_ATTACHED;
        packetServiceInfo->highestAvailableDataClass = s_availableDataClasses;
        //todo query from CP
        packetServiceInfo->upLinkSpeed = getLinkSpeed(UP_LINK_SPEED);
        packetServiceInfo->downLinkSpeed = getLinkSpeed(DOWN_LINK_SPEED);
    } else if (s_regState == MBIM_REGISTER_STATE_DEREGISTERED ||
            s_regState == MBIM_REGISTER_STATE_SEARCHING ||
            s_regState == MBIM_REGISTER_STATE_DENIED){
        packetServiceInfo->packetServiceState = MBIM_PACKET_SERVICE_STATE_DETACHED;
    }

    mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE, packetServiceInfo,
            sizeof(MbimPacketServiceInfo));
    free(packetServiceInfo);
    putChannel(channelID);
}
/*****************************************************************************/

/**
 * basic_connect_connect: query, set and notification supported
 *
 * query:
 *  @command:   the informationBuffer shall be MbimConnectInfo
 *  @response:  the informationBuffer shall be MbimConnectInfo
 *
 * set:
 *  @command:   the informationBuffer shall be MbimSetSignalState
 *  @response:  the informationBuffer shall be MbimConnectInfo
 *
 * notification:
 *  @command:   NA
 *  @response:  the informationBuffer shall be MbimConnectInfo
 *
 * Note:
 *  for query, the only relevant field of MbimConnectInfo in command's
 *  informationBuffer is the sessionId.
 */
void
basic_connect_connect(Mbim_Token                token,
                      MbimMessageCommandType    commandType,
                      void *                    data,
                      size_t                    dataLen) {

       switch (commandType) {
            case MBIM_MESSAGE_COMMAND_TYPE_SET:
                set_connect(token, data, dataLen);
                break;
            case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
                query_connect_info(token, data, dataLen);
                break;
            default:
                RLOGE("unsupported commandType");
                break;
        }

}

int s_setDone = 0;
void
setupDataConnection(Mbim_Token              token,
                    int                     channelID,
                    void *                  data,
                    size_t                  dataLen) {
    MBIM_UNUSED_PARM(dataLen);

    bool error = false;
    int authtype;
    int sessionId, err;
    int cid = -1;
    const char *apn = NULL;
    const char *username = NULL;
    const char *password= NULL;
    const char *iPType= NULL;
    char cmd[AT_COMMAND_LEN] = {0};
    ATResponse *p_response = NULL;
    MbimStatusError errcode = MBIM_STATUS_ERROR_NONE;
    PDPState state = PDP_IDLE;

    MbimConnectInfo *activateConnectionInfo =
            (MbimConnectInfo *)calloc(1, sizeof(MbimConnectInfo));

    MbimSetConnect_2 *activateConnection = (MbimSetConnect_2 *)data;

    sessionId = activateConnection->sessionId;

    if  (s_regState != MBIM_REGISTER_STATE_HOME &&
            s_regState != MBIM_REGISTER_STATE_ROAMING &&
            s_regState != MBIM_REGISTER_STATE_PARTNER) {
        error = true;
        errcode = MBIM_STATUS_ERROR_PACKET_SERVICE_DETACHED;
        goto done;
    }
    if (sessionId >= MAX_PDP) {
        error = true;
        errcode = MBIM_STATUS_ERROR_MAX_ACTIVATED_CONTEXTS;
        goto done;
    }

    pthread_mutex_lock(&s_PDP[sessionId].mutex);
    if (getPDPState(sessionId) == PDP_ACTIVATED) {
        cid = getPDPCidBysessionId(sessionId);
        activateConnectionInfo->sessionId = sessionId;
        activateConnectionInfo->activationState = MBIM_ACTIVATION_STATE_ACTIVATED;
        activateConnectionInfo->IPType = getPDNIPType(cid - 1);
        activateConnectionInfo->contextType =
                *mbim_uuid_from_context_type(activateConnection->contextType);

        pthread_mutex_unlock(&s_PDP[sessionId].mutex);
        goto done;
    }
    pthread_mutex_unlock(&s_PDP[sessionId].mutex);

    apn = activateConnection->accessString;
    username = activateConnection->username;
    password = activateConnection->password;
    authtype = activateConnection->authProtocol;
    switch (activateConnection->IPType) {
        case MBIM_CONTEXT_IP_TYPE_IPV4:
            iPType = "IP";
            break;
        case MBIM_CONTEXT_IP_TYPE_IPV6:
            iPType = "IPV6";
            break;
        case MBIM_CONTEXT_IP_TYPE_IPV4V6:
            iPType = "IPV4V6";
            break;
        case MBIM_CONTEXT_IP_TYPE_IPV4_AND_IPV6:
            iPType = "IPV4+IPV6";
            break;
        default:
            iPType = "IPV4V6";
            activateConnection->IPType = MBIM_CONTEXT_IP_TYPE_IPV4V6;
            break;
    }
    RLOGI("apn = %s; username = %s; password = %s; authtype = %d; iPType = %s",
            apn, username, password, authtype, iPType);
    queryAllActivePDNInfos(channelID);

    if (s_activePDN > 0) {
//        for (i = 0; i < MAX_PDP; i++) {
            cid = getPDNCid(0);
            if (cid == 1) {
                RLOGI("PDPState(0) = %d",getPDPState(0));
                if(isApnEqual((char *)apn, getPDNAPN(0))) {//&& isProtocolEqual(activateConnection->IPType, getPDNIPType(i))

                    snprintf(cmd, sizeof(cmd), "AT+CGDATA=\"M-ETHER\",%d",cid);
                    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
                    if (err < 0 || p_response->success == 0) {
                        error = true;
                        errcode = MBIM_STATUS_ERROR_FAILURE;
                        goto done;
                    }
                    activateConnectionInfo->sessionId = sessionId;
                    activateConnectionInfo->activationState = MBIM_ACTIVATION_STATE_ACTIVATED;
                    activateConnectionInfo->IPType = getPDNIPType(0);
                    activateConnectionInfo->contextType =
                            *mbim_uuid_from_context_type(activateConnection->contextType);
                    goto done;
                } else {
                    snprintf(cmd, sizeof(cmd), "AT+CGDCONT=%d,\"%s\",\"%s\",\"\",0,0",
                              cid, iPType, apn);
                    at_send_command(s_ATChannels[channelID], cmd, &p_response);

                    at_send_command(s_ATChannels[channelID], "AT+SPREATTACH", NULL);
                    error = true;
                    errcode = MBIM_STATUS_ERROR_FAILURE;
                    goto done;
                }
            }
//        }
    }
    cid = getPDP(sessionId);
    if (cid < 1) {
        error = true;
        if(cid == -2){
            errcode = MBIM_STATUS_ERROR_BUSY;
        } else {
            errcode = MBIM_STATUS_ERROR_MAX_ACTIVATED_CONTEXTS;
        }
        goto done;
    }

    setPDPByIndex(sessionId, cid, PDP_ACTIVATING, activateConnection->contextType);

    snprintf(cmd, sizeof(cmd), "AT+CGACT=0,%d", cid);
    at_send_command(s_ATChannels[channelID], cmd, NULL);
//    cgact_deact_cmd_rsp(cid);

    if (!strcmp(iPType, "IPV4+IPV6")) {
         snprintf(cmd, sizeof(cmd), "AT+CGDCONT=%d,\"IP\",\"%s\",\"\",0,0",
                   cid, apn);
     } else {
         snprintf(cmd, sizeof(cmd), "AT+CGDCONT=%d,\"%s\",\"%s\",\"\",0,0",
                   cid, iPType, apn);
     }
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (err < 0 || p_response->success == 0) {
        error = true;
        errcode = MBIM_STATUS_ERROR_FAILURE;
        goto done;
    }

    snprintf(cmd, sizeof(cmd), "AT+CGPCO=0,\"%s\",\"%s\",%d,%d", username,
              password, cid, authtype);
    at_send_command(s_ATChannels[channelID], cmd, NULL);

    /* Set required QoS params to default */
    snprintf(cmd, sizeof(cmd),"AT+CGEQREQ=%d,2,0,0,0,0,2,0,\"1e4\",\"0e0\",3,0,0",cid);
    at_send_command(s_ATChannels[channelID], cmd, NULL);

    AT_RESPONSE_FREE(p_response);
    snprintf(cmd, sizeof(cmd), "AT+CGDATA=\"M-ETHER\",%d", cid);
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (err < 0 || p_response->success == 0) {
        error = true;
        errcode = MBIM_STATUS_ERROR_FAILURE;
        goto done;
    }
    activateConnectionInfo->sessionId = sessionId;
    activateConnectionInfo->activationState = MBIM_ACTIVATION_STATE_ACTIVATED;
    activateConnectionInfo->IPType = getPDNIPType(cid - 1);
    activateConnectionInfo->contextType =
            *mbim_uuid_from_context_type(activateConnection->contextType);

done:
    if (error) {
        cid= -1;
        mbim_device_command_done(token, errcode, NULL, 0);
    } else {
        state = PDP_ACTIVATED;
        mbim_device_command_done(token, errcode, activateConnectionInfo, sizeof(MbimConnectInfo));
    }
    setPDPByIndex(sessionId, cid, state, activateConnection->contextType);
    free(activateConnectionInfo);
    at_response_free(p_response);
}

void
deactivateDataConnection(Mbim_Token         token,
                         int                channelID,
                         void *             data,
                         size_t             dataLen) {
    MBIM_UNUSED_PARM(dataLen);

    char cmd[AT_COMMAND_LEN] = {0};
    int cid;
    bool error = false;
    ATResponse *p_response = NULL;
    MbimStatusError errcode = MBIM_STATUS_ERROR_NONE;

    MbimConnectInfo *deactivateConnectionInfo =
            (MbimConnectInfo *)calloc(1, sizeof(MbimConnectInfo));
    MbimSetConnect_2 *deactivateConnection = (MbimSetConnect_2 *)data;

    if (getPDPState(deactivateConnection->sessionId) != PDP_ACTIVATED ) {
        error = true;
        errcode = MBIM_STATUS_ERROR_CONTEXT_NOT_ACTIVATED;
        goto done;
    }
    cid = getPDPCidBysessionId(deactivateConnection->sessionId);
    setPDPByIndex(deactivateConnection->sessionId, -1, PDP_DEACTIVATING,
            MBIM_CONTEXT_TYPE_NONE);

    snprintf(cmd, sizeof(cmd), "AT+CGACT=0,%d", cid);
    at_send_command(s_ATChannels[channelID], cmd, &p_response);

    deactivateConnectionInfo->sessionId = deactivateConnection->sessionId;
    deactivateConnectionInfo->voiceCallState = MBIM_VOICE_CALL_STATE_NONE;
    deactivateConnectionInfo->activationState = MBIM_ACTIVATION_STATE_DEACTIVATED;
    deactivateConnectionInfo->IPType = MBIM_CONTEXT_IP_TYPE_DEFAULT;
    deactivateConnectionInfo->contextType =
            *mbim_uuid_from_context_type(MBIM_CONTEXT_TYPE_INTERNET);
    deactivateConnectionInfo->nwError = MBIM_NW_ERROR_UNKNOWN;

done:
    if (error) {
        mbim_device_command_done(token, errcode, NULL, 0);
    } else {
        setPDPByIndex(deactivateConnection->sessionId, -1, PDP_IDLE,MBIM_CONTEXT_TYPE_NONE);
        mbim_device_command_done(token, errcode, deactivateConnectionInfo, sizeof(MbimConnectInfo));
    }

    free(deactivateConnectionInfo);
    at_response_free(p_response);
}

void
set_connect(Mbim_Token                token,
            void *                    data,
            size_t                    dataLen) {
    RIL_SOCKET_ID socket_id  = RIL_SOCKET_1;
    int channelID = getChannel(socket_id);
    MbimSetConnect_2 *resp = (MbimSetConnect_2 *)data;

    if (resp->activationCommand == MBIM_ACTIVATION_COMMAND_ACTIVATE) {
        setupDataConnection(token, channelID, data, dataLen);
    } else if (resp->activationCommand == MBIM_ACTIVATION_COMMAND_DEACTIVATE) {
        deactivateDataConnection(token, channelID, data, dataLen);
    }
    putChannel(channelID);
}

/****************************************************************************/
/**
 * basic_connect_provisioned_contexts: query, set and notification supported
 *
 * query:
 *  @command:   the informationBuffer shall be null
 *  @response:  the informationBuffer shall be MbimProvisionedContextsInfo
 *
 * set:
 *  @command:   the informationBuffer shall be MbimSetProvisionedContext
 *  @response:  the informationBuffer shall be MbimProvisionedContextsInfo
 *
 * notification:
 *  @command:   NA
 *  @response:  the informationBuffer shall be MbimProvisionedContextsInfo
 */
void
basic_connect_provisioned_contexts(Mbim_Token                token,
                                   MbimMessageCommandType    commandType,
                                   void *                    data,
                                   size_t                    dataLen) {
    switch (commandType) {
         case MBIM_MESSAGE_COMMAND_TYPE_SET:
//             set_provisioned_contexts(token, data, dataLen);
             break;
         case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
             query_provisioned_contexts(token, data, dataLen);
             break;
         default:
             RLOGE("unsupported commandType");
             break;
     }
}

void
query_provisioned_contexts(Mbim_Token                token,
                           void *                    data,
                           size_t                    dataLen) {
    MBIM_UNUSED_PARM(data);
    MBIM_UNUSED_PARM(dataLen);

    RLOGD("query_provisioned_contexts");

    RIL_SOCKET_ID socket_id  = RIL_SOCKET_1;
    int channelID = getChannel(socket_id);
    char cmd[AT_COMMAND_LEN] = {0};


    MbimProvisionedContextsInfo_2 *provisionedContextsInfo =
                (MbimProvisionedContextsInfo_2 *)calloc(1, sizeof(MbimProvisionedContextsInfo_2));
    MbimContext_2  *contextInfo =
            (MbimContext_2 *)calloc(1, sizeof(MbimContext_2));

    contextInfo->contextId = 1;
    contextInfo->contextType = MBIM_CONTEXT_TYPE_INTERNET;
    contextInfo->accessString = (char *)calloc(PROPERTY_NAME_MAX, sizeof(char));
    contextInfo->username = (char *)calloc(PROPERTY_NAME_MAX, sizeof(char));
    contextInfo->password = (char *)calloc(PROPERTY_NAME_MAX, sizeof(char));
    contextInfo->authProtocol = MBIM_AUTH_PROTOCOL_NONE;

    queryAllActivePDNInfos(channelID);
    if(getPDNAPN(0) != NULL){
        strncpy(contextInfo->accessString, getPDNAPN(0), checkCmpAnchor(getPDNAPN(0)));
        contextInfo->accessString[strlen(contextInfo->accessString)] = '\0';
    }else{
        getApnUsingPLMN(s_providerId, contextInfo);
        snprintf(cmd, sizeof(cmd), "AT+CGDCONT=%d,\"%s\",\"%s\",\"\",0,0",
                  1, IPV4V6, contextInfo->accessString);
        at_send_command(s_ATChannels[channelID], cmd, NULL);
        RLOGD("contextInfo->accessString = %s",contextInfo->accessString);
    }

    RLOGD("apn = %s",contextInfo->accessString);
    RLOGD("username = %s",contextInfo->username);
    RLOGD("password = %s",contextInfo->password);

    provisionedContextsInfo->elementCount = 1;
    provisionedContextsInfo->context = &contextInfo;

    mbim_device_command_done(token,MBIM_STATUS_ERROR_NONE,provisionedContextsInfo,sizeof(MbimProvisionedContextsInfo_2));
    putChannel(channelID);
    free(contextInfo->accessString);
    free(contextInfo->username);
    free(contextInfo->password);
    free(contextInfo);
    free(provisionedContextsInfo);
}

void
set_provisioned_contexts(Mbim_Token                token,
                         void *                    data,
                         size_t                    dataLen) {
    RLOGD("set_provisioned_contexts");
    //todo length of mnc
    char *mcc;
    char *mnc;
    char *providerId;

    MbimSetProvisionedContext_2  *setProvisionedContext = (MbimSetProvisionedContext_2 *)data;

    MbimProvisionedContextsInfo_2 *provisionedContextsInfo =
                (MbimProvisionedContextsInfo_2 *)calloc(1, sizeof(MbimProvisionedContextsInfo_2));
    MbimContext_2  *contextInfo =
                (MbimContext_2 *)calloc(1, sizeof(MbimContext_2));

//    if(!isStrEmpty(s_provederId) && strcmp(setProvisionedContext->providerId, s_provederId) == 0){
//        RLOGD("s_provederId = %s",s_provederId);
//        strncpy(mcc, s_provederId, 3);
//        mcc[3] = '\0';
//        strncpy(mnc, s_provederId+3, strlen(s_provederId) - 3);
//        mnc[strlen(s_provederId) - 3] = '\0';
//    } else {
//        RLOGD("s_provederId = %s",s_provederId);
//        RLOGD("setProvisionedContext->providerId = %s",setProvisionedContext->providerId);
//    }

    if(!isStrEmpty(s_subscriberId)){
        RLOGD("s_subscriberId = %s",s_subscriberId);
        strncpy(mcc, s_subscriberId, 3);
        mcc[3] = '\0';
        strncpy(mnc, s_subscriberId+3, 2);
        mnc[2] = '\0';
    }
    RLOGD("mcc = %s, mnc = %s",mcc,mnc);

    contextInfo->accessString = strdup(setProvisionedContext->accessString);
    contextInfo->username = strdup(setProvisionedContext->username);
    contextInfo->password = strdup(setProvisionedContext->password);
    contextInfo->authProtocol = setProvisionedContext->authProtocol;
    contextInfo->contextType = setProvisionedContext->contextType;
    contextInfo->contextId = setProvisionedContext->contextId;

    setApnUsingPLMN(mcc, mnc, contextInfo);

    provisionedContextsInfo->elementCount = 1;
    provisionedContextsInfo->context = &contextInfo;

    mbim_device_command_done(token,MBIM_STATUS_ERROR_NONE,provisionedContextsInfo,sizeof(MbimProvisionedContextsInfo_2));
    free(contextInfo->accessString);
    free(contextInfo->username);
    free(contextInfo->password);
    free(contextInfo);
    free(provisionedContextsInfo);

}


/******************************************************************************/
/**
 * basic_connect_ip_configuration: query and notification supported
 *
 * query:
 *  @command:   the informationBuffer shall be MbimIPConfigurationInfo
 *  @response:  the informationBuffer shall be MbimIPConfigurationInfo
 *
 * notification:
 *  @command:   NA
 *  @response:  the informationBuffer shall be MbimIPConfigurationInfo
 *
 * Note:
 *  for query, the only relevant field of MbimIPConfigurationInfo in command's
 *  informationBuffer is the sessionId.
 */

void
basic_connect_ip_configuration(Mbim_Token                token,
                               MbimMessageCommandType    commandType,
                               void *                    data,
                               size_t                    dataLen) {
    MBIM_UNUSED_PARM(commandType);
    MBIM_UNUSED_PARM(dataLen);

    int err,cid;
    RIL_SOCKET_ID socket_id  = RIL_SOCKET_1;
    int channelID = getChannel(socket_id);
    int sessionId = ((int *)data)[0];
    char cmd[AT_COMMAND_LEN];
    char **IPv4DnsServer = NULL;
    char **IPv6DnsServer = NULL;
    ATResponse *p_rdpResponse = NULL;

    MbimIPv4Element_2 **IPv4Element = NULL;
    MbimIPv6Element_2 **IPv6Element = NULL;
    MbimIPConfigurationInfo_2 *iPConfigurationInfo =
            (MbimIPConfigurationInfo_2 *)calloc(1, sizeof(MbimIPConfigurationInfo_2));

    IPv4Element = (MbimIPv4Element_2 **)calloc(1, sizeof(MbimIPv4Element_2 *));
    IPv4Element[0] = (MbimIPv4Element_2 *)calloc(1, sizeof(MbimIPv4Element_2 ));

    IPv6Element = (MbimIPv6Element_2 **)calloc(1, sizeof(MbimIPv6Element_2 *));
    IPv6Element[0] = (MbimIPv6Element_2 *)calloc(1, sizeof(MbimIPv6Element_2 ));

    cid = getPDPCidBysessionId(sessionId);
    snprintf(cmd, sizeof(cmd), "AT+CGCONTRDP=%d", cid);
    err = at_send_command_multiline(s_ATChannels[channelID], cmd,
                                    "+CGCONTRDP:", &p_rdpResponse);

    if (err == AT_ERROR_TIMEOUT) {
        RLOGE("Get IP address timeout");
    } else {
        cgcontrdp_set_cmd_rsp(p_rdpResponse);
    }
    if (strlen(s_ipInfo[cid - 1].ipladdr) != 0) {
        RLOGE("s_ipInfo[%d].ipv4netmask = %s", cid - 1, s_ipInfo[cid - 1].ipv4netmask);
        IPv4Element[0]->onLinkPrefixLength = getOnLinkPrefixLength(s_ipInfo[cid - 1].ipv4netmask);
        IPv4Element[0]->IPv4Address = s_ipInfo[cid - 1].ipladdr;
        iPConfigurationInfo->sessionId = sessionId;
        iPConfigurationInfo->IPv4AddressCount = 1;
        iPConfigurationInfo->IPv4Element = IPv4Element;
        iPConfigurationInfo->IPv4Gateway = s_ipInfo[cid - 1].ipladdr;
        iPConfigurationInfo->IPv4DnsServerCount = 1;
        if (strlen(s_ipInfo[cid - 1].dns2addr) != 0) {
            iPConfigurationInfo->IPv4DnsServerCount = 2;
        }
        IPv4DnsServer = (char **)calloc(iPConfigurationInfo->IPv4DnsServerCount, sizeof(char *));
        IPv4DnsServer[0] = s_ipInfo[cid - 1].dns1addr;
        if (iPConfigurationInfo->IPv4DnsServerCount == 2) {
            IPv4DnsServer[1] = s_ipInfo[cid - 1].dns2addr;
        }
        iPConfigurationInfo->IPv4DnsServer = IPv4DnsServer;
        iPConfigurationInfo->IPv4ConfigurationAvailable = 7;
    }

    if (strlen(s_ipInfo[cid - 1].ipv6laddr) != 0) {
//  TODO      IPv6Element[0]->onLinkPrefixLength = getOnLinkPrefixLength(s_ipInfo[cid - 1].ipv4netmask);
        IPv6Element[0]->IPv6Address = s_ipInfo[cid - 1].ipv6laddr;
        iPConfigurationInfo->sessionId = sessionId;
        iPConfigurationInfo->IPv6AddressCount = 1;
        iPConfigurationInfo->IPv6Element = IPv6Element;
        iPConfigurationInfo->IPv6Gateway = s_ipInfo[cid - 1].ipv6laddr;
        iPConfigurationInfo->IPv6DnsServerCount = 1;
        if (strlen(s_ipInfo[cid - 1].ipv6dns2addr) != 0) {
            iPConfigurationInfo->IPv6DnsServerCount = 2;
        }
        IPv6DnsServer = (char **)calloc(iPConfigurationInfo->IPv6DnsServerCount, sizeof(char *));
        IPv6DnsServer[0] = s_ipInfo[cid - 1].ipv6dns1addr;
        if (iPConfigurationInfo->IPv6DnsServerCount == 2) {
            IPv6DnsServer[1] = s_ipInfo[cid - 1].ipv6dns2addr;
        }
        iPConfigurationInfo->IPv6DnsServer = IPv6DnsServer;
        iPConfigurationInfo->IPv6ConfigurationAvailable = 7;
    }

    mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE, iPConfigurationInfo,
            sizeof(MbimIPConfigurationInfo_2));
    putChannel(channelID);
    at_response_free(p_rdpResponse);

    free(IPv4Element[0]);
    free(IPv4Element);
    free(IPv6Element[0]);
    free(IPv6Element);
    free(IPv4DnsServer);
    free(IPv6DnsServer);
    free(iPConfigurationInfo);
}

/****************************************************************************/
/**
 * basic_connect_device_services: query only
 *
 * query:
 *  @command:   the data shall be null
 *  @response:  the data shall be MbimDeviceServicesInfo_2
 *
 */
void
basic_connect_device_services(Mbim_Token                token,
                              MbimMessageCommandType    commandType,
                              void *                    data,
                              size_t                    dataLen) {
    switch (commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            query_device_services(token, data, dataLen);
            break;
        default:
            RLOGE("unsupported commandType");
            break;
    }
}

void
query_device_services(Mbim_Token            token,
                      void *                data,
                      size_t                dataLen) {
    MBIM_UNUSED_PARM(data);
    MBIM_UNUSED_PARM(dataLen);

    // For now, supported services
    const MbimService services[] = {
            MBIM_SERVICE_BASIC_CONNECT,
            MBIM_SERVICE_SMS,
//            MBIM_SERVICE_USSD,
//            MBIM_SERVICE_PHONEBOOK,
//            MBIM_SERVICE_STK,
//            MBIM_SERVICE_AUTH,
//            MBIM_SERVICE_DSS,
            MBIM_SERVICE_OEM,
    };

    // For now, supported cids
    const MbimCidBasicConnect basicConnectCids[] = {
            MBIM_CID_BASIC_CONNECT_DEVICE_CAPS,
            MBIM_CID_BASIC_CONNECT_SUBSCRIBER_READY_STATUS,
            MBIM_CID_BASIC_CONNECT_RADIO_STATE,
            MBIM_CID_BASIC_CONNECT_PIN,
            MBIM_CID_BASIC_CONNECT_PIN_LIST,
            MBIM_CID_BASIC_CONNECT_HOME_PROVIDER,
            MBIM_CID_BASIC_CONNECT_PREFERRED_PROVIDERS,
            MBIM_CID_BASIC_CONNECT_VISIBLE_PROVIDERS,
            MBIM_CID_BASIC_CONNECT_REGISTER_STATE,
            MBIM_CID_BASIC_CONNECT_PACKET_SERVICE,
            MBIM_CID_BASIC_CONNECT_SIGNAL_STATE,
            MBIM_CID_BASIC_CONNECT_CONNECT,
            MBIM_CID_BASIC_CONNECT_PROVISIONED_CONTEXTS,
//            MBIM_CID_BASIC_CONNECT_SERVICE_ACTIVATION,
            MBIM_CID_BASIC_CONNECT_IP_CONFIGURATION,
            MBIM_CID_BASIC_CONNECT_DEVICE_SERVICES,
            /* 17, 18 reserved */
            MBIM_CID_BASIC_CONNECT_DEVICE_SERVICE_SUBSCRIBE_LIST,
//            MBIM_CID_BASIC_CONNECT_PACKET_STATISTICS,
            MBIM_CID_BASIC_CONNECT_NETWORK_IDLE_HINT,
            MBIM_CID_BASIC_CONNECT_EMERGENCY_MODE,
//            MBIM_CID_BASIC_CONNECT_IP_PACKET_FILTERS,
//            MBIM_CID_BASIC_CONNECT_MULTICARRIER_PROVIDERS,
    };

    const MbimCidSms smsCids[] = {
//            MBIM_CID_SMS_CONFIGURATION,
//            MBIM_CID_SMS_READ,
//            MBIM_CID_SMS_SEND,
//            MBIM_CID_SMS_DELETE,
//            MBIM_CID_SMS_MESSAGE_STORE_STATUS,
    };

    const MbimCidOem oemCids[] = {
            MBIM_CID_OEM_ATCI,
    };

    uint32_t deviceServicesCount = sizeof(services) / sizeof(MbimService);
    uint32_t basicConnectCount =
            sizeof(basicConnectCids) / sizeof(MbimCidBasicConnect);
    uint32_t smsCidCount = sizeof(smsCids) / sizeof(MbimCidSms);
    uint32_t oemCidCount = sizeof(oemCids) / sizeof(MbimCidOem);

    MbimDeviceServicesInfo_2 *deviceServicesInfo =
            (MbimDeviceServicesInfo_2 *)calloc(1, sizeof(MbimDeviceServicesInfo_2));
    deviceServicesInfo->deviceServicesCount = deviceServicesCount;
    deviceServicesInfo->maxDssSessions = 0;
    MbimDeviceServiceElement_2 **deviceServiceElements =
            (MbimDeviceServiceElement_2 **)calloc(1, sizeof(MbimDeviceServiceElement_2));
    for (uint32_t i = 0; i < deviceServicesCount; i++) {
        deviceServiceElements[i] =
                (MbimDeviceServiceElement_2 *)calloc(1, sizeof(MbimDeviceServiceElement_2));
    }

    deviceServiceElements[0]->deviceServiceId = MBIM_SERVICE_BASIC_CONNECT;
    deviceServiceElements[0]->dssPayload = 0;
    deviceServiceElements[0]->maxDssInstances = 0;
    deviceServiceElements[0]->cidCount = basicConnectCount;
    deviceServiceElements[0]->cids = basicConnectCids;

    deviceServiceElements[1]->deviceServiceId = MBIM_SERVICE_OEM;
    deviceServiceElements[1]->dssPayload = 0;
    deviceServiceElements[1]->maxDssInstances = 0;
    deviceServiceElements[1]->cidCount = oemCidCount;
    deviceServiceElements[1]->cids = oemCids;

    deviceServicesInfo->deviceServiceElements = deviceServiceElements;

    mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE, deviceServicesInfo,
            sizeof(MbimDeviceServicesInfo_2));
    for (uint32_t i = 0; i < deviceServicesCount; i++) {
        free(deviceServiceElements[i]);
    }
    free(deviceServiceElements);
    free(deviceServicesInfo);
}

/*****************************************************************************/
/**
 * basic_connect_device_service_subscribe_list: set only
 *
 * set:
 *  @command:   the informationBuffer shall be MbimDeviceServiceSubscribeList
 *  @response:  the informationBuffer shall be MbimDeviceServiceSubscribeList
 *
 */

void
basic_connect_device_service_subscribe_list(Mbim_Token              token,
                                            MbimMessageCommandType  commandType,
                                            void *                  data,
                                            size_t                  dataLen) {
    switch (commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            set_device_service_subscribe_list(token, data, dataLen);
            break;
        default:
            RLOGE("unsupported commandType");
            break;
    }
}

MbimDeviceServiceSubscribeList_2 *s_deviceServiceSubscribeList = NULL;

void
set_device_service_subscribe_list(Mbim_Token            token,
                                  void *                data,
                                  size_t                dataLen) {
    MBIM_UNUSED_PARM(dataLen);

    MbimDeviceServiceSubscribeList_2 *deviceServiceSubscribeList =
            (MbimDeviceServiceSubscribeList_2 *)data;

    if (s_deviceServiceSubscribeList != NULL) {
        for (uint32_t i = 0; i < s_deviceServiceSubscribeList->elementCount; i++) {
            free(s_deviceServiceSubscribeList->eventEntries[i]->cidList);
            s_deviceServiceSubscribeList->eventEntries[i]->cidList = NULL;

            free(s_deviceServiceSubscribeList->eventEntries[i]);
            s_deviceServiceSubscribeList->eventEntries[i] = NULL;
        }
        free(s_deviceServiceSubscribeList);
        s_deviceServiceSubscribeList = NULL;
    }

    s_deviceServiceSubscribeList =
            (MbimDeviceServiceSubscribeList_2 *)calloc(1, sizeof(MbimDeviceServiceSubscribeList_2));
    s_deviceServiceSubscribeList->elementCount = deviceServiceSubscribeList->elementCount;
    MbimEventEntry_2 **eventEntries = (MbimEventEntry_2 **)
            calloc(s_deviceServiceSubscribeList->elementCount, sizeof(MbimEventEntry_2 *));
    for (uint32_t i = 0; i < s_deviceServiceSubscribeList->elementCount; i++) {
        eventEntries[i] = (MbimEventEntry_2 *)calloc(1, sizeof(MbimEventEntry_2));
        eventEntries[i]->deviceServiceId = deviceServiceSubscribeList->eventEntries[i]->deviceServiceId;
        eventEntries[i]->cidCount = deviceServiceSubscribeList->eventEntries[i]->cidCount;
        eventEntries[i]->cidList = (uint32_t *)calloc(eventEntries[i]->cidCount, sizeof(uint32_t));
        memcpy(eventEntries[i]->cidList, deviceServiceSubscribeList->eventEntries[i]->cidList,
                sizeof(uint32_t) * eventEntries[i]->cidCount);
    }

    s_deviceServiceSubscribeList->eventEntries = eventEntries;

    mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE,
            s_deviceServiceSubscribeList, sizeof(MbimDeviceServiceSubscribeList_2));
}

/*****************************************************************************/

/**
 * basic_connect_network_idle_hint: set and query supported
 *
 * query:
 *  @command:   the data shall be null
 *  @response:  the data shall be MbimNetworkIdleHint
 *
 * set:
 *  @command:   the data shall be MbimNetworkIdleHint
 *  @response:  the data shall be MbimNetworkIdleHint
 */

void
basic_connect_network_idle_hint(Mbim_Token                  token,
                                MbimMessageCommandType      commandType,
                                void *                      data,
                                size_t                      dataLen) {
    switch (commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            query_network_idle_hint(token, data, dataLen);
            break;
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            set_network_idle_hint(token, data, dataLen);
            break;
        default:
            RLOGE("unsupported commandType");
            break;
    }
}

MbimNetworkIdleHintState s_networkIdleHintState =
        MBIM_NETWORK_IDLE_HINT_STATE_DISABLED;

void
query_network_idle_hint(Mbim_Token       token,
                        void *                  data,
                        size_t                  dataLen) {
    MBIM_UNUSED_PARM(data);
    MBIM_UNUSED_PARM(dataLen);

    MbimNetworkIdleHint response;
    response.networkIdleHintState = s_networkIdleHintState;

    mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE, &response,
            sizeof(MbimNetworkIdleHint));
}

void
set_network_idle_hint(Mbim_Token         token,
                      void *                    data,
                      size_t                    dataLen) {
    MBIM_UNUSED_PARM(dataLen);

    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    int channelID = getChannel(socket_id);

    MbimNetworkIdleHint *networkIdleHint = (MbimNetworkIdleHint *)data;
    s_networkIdleHintState = networkIdleHint->networkIdleHintState;

    if (s_networkIdleHintState == MBIM_NETWORK_IDLE_HINT_STATE_ENABLED) {
        /* Suspend */
        at_send_command(s_ATChannels[channelID], "AT+CCED=2,8", NULL);
        if (s_isLTE) {
            at_send_command(s_ATChannels[channelID], "AT+CEREG=1", NULL);
        }
        at_send_command(s_ATChannels[channelID], "AT+CREG=1", NULL);
        at_send_command(s_ATChannels[channelID], "AT+CGREG=1", NULL);

//        if (isVoLteEnable()) {
//            at_send_command(s_ATChannels[channelID], "AT+CIREG=1", NULL);
//        }
        at_send_command(s_ATChannels[channelID], "AT*FDY=1,2", NULL);
    } else {
        /* Resume */
        at_send_command(s_ATChannels[channelID], "AT+CCED=1,8", NULL);
        if (s_isLTE) {
            at_send_command(s_ATChannels[channelID], "AT+CEREG=2", NULL);
        }
        at_send_command(s_ATChannels[channelID], "AT+CREG=2", NULL);
        at_send_command(s_ATChannels[channelID], "AT+CGREG=2", NULL);

        at_send_command(s_ATChannels[channelID], "AT*FDY=1,5", NULL);
    }

    MbimNetworkIdleHint response;
    response.networkIdleHintState = s_networkIdleHintState;

    mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE, &response,
            sizeof(MbimNetworkIdleHint));
    putChannel(channelID);
}

/*****************************************************************************/

/**
 * basic_connect_emergency_mode: query and notification supported
 *
 * query:
 *  @command:   the data shall be null
 *  @response:  the data shall be MbimEmergencyModeInfo
 *
 * notification:
 *  @command:   NA
 *  @response:  the data shall be MbimEmergencyModeInfo
 */
void
basic_connect_emergency_mode(Mbim_Token                 token,
                             MbimMessageCommandType     commandType,
                             void *                     data,
                             size_t                     dataLen) {
    switch (commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            query_emergency_mode(token, data, dataLen);
            break;
        default:
            RLOGE("unsupported commandType");
            break;
    }
}

MbimEmergencyModeState s_emergencyModeState = MBIM_EMERGENCY_MODE_STATE_OFF;

MbimEmergencyModeState getEmergencyModeState(int regState) {
    if (regState == 1 ||    /* RIL_REG_STATE_HOME */
        regState == 5 ||    /* RIL_REG_STATE_ROAMING */
        regState == 8 ||    /* RIL_REG_STATE_NOT_REG_EMERGENCY_CALL_ENABLED */
        regState == 10) {   /* RIL_REG_STATE_NOT_REG_EMERGENCY_CALL_ENABLED */
        return MBIM_EMERGENCY_MODE_STATE_ON;
    } else {
        return MBIM_EMERGENCY_MODE_STATE_OFF;
    }
}

void
query_emergency_mode(Mbim_Token             token,
                     void *                 data,
                     size_t                 dataLen) {
    MBIM_UNUSED_PARM(data);
    MBIM_UNUSED_PARM(dataLen);

    int err = -1;
    int response = 0;
    int commas = 0, skip = 0;
    char *line = NULL, *p = NULL;

    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    ATChannelId channelID = getChannel(socket_id);

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+CREG?", "+CREG:",
                                     &p_response);
    if (err != 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    /* count number of commas */
    commas = 0;
    for (p = line; *p != '\0'; p++) {
        if (*p == ',') commas++;
    }

    switch (commas) {
        case 0: {  /* +CREG: <stat> */
            err = at_tok_nextint(&line, &response);
            if (err < 0) goto error;
            break;
        }
        default: {  /* +CREG: <n>, <stat>, ... */
            err = at_tok_nextint(&line, &skip);
            if (err < 0) goto error;
            err = at_tok_nextint(&line, &response);
            if (err < 0) goto error;
            break;
        }
    }

    s_emergencyModeState = getEmergencyModeState(response);

    MbimEmergencyModeInfo emergencyModeInfo;
    emergencyModeInfo.emergencyMode = s_emergencyModeState;

    mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE,
            &emergencyModeInfo, sizeof(MbimEmergencyModeInfo));
    AT_RESPONSE_FREE(p_response);
    putChannel(channelID);
    return;

error:
    mbim_device_command_done(token, MBIM_STATUS_ERROR_FAILURE, NULL, 0);
    AT_RESPONSE_FREE(p_response);
    putChannel(channelID);
    return;
}

/*****************************************************************************/

bool isStrEmpty(char *str) {
    if (NULL == str || strcmp(str, "") == 0) {
        return true;
    }
    return false;
}

bool isStrEqual(char *newStr, char *oldStr) {
    bool ret = false;
    if (isStrEmpty(oldStr) && isStrEmpty(newStr)) {
        ret = true;
    } else if (!isStrEmpty(oldStr) && !isStrEmpty(newStr)) {
        if (strcasecmp(oldStr, newStr) == 0) {
            ret = true;
        } else {
            RLOGD("isStrEqual old=%s, new=%s", oldStr, newStr);
        }
    } else {
        RLOGD("isStrEqual old or new is empty!");
    }
    return ret;
}

void
queryAllActivePDNInfos(int channelID) {
    int err = 0;
    int n, skip, active;
    char *line;
    ATLine *pCur;
    PDNInfo *pdns = s_PDN;
    ATResponse *pdnResponse = NULL;

    s_activePDN = 0;
    err = at_send_command_multiline(s_ATChannels[channelID], "AT+SPIPCONTEXT?",
                                    "+SPIPCONTEXT:", &pdnResponse);
    if (err < 0 || pdnResponse->success == 0) goto done;

    for (pCur = pdnResponse->p_intermediates; pCur != NULL;
         pCur = pCur->p_next) {
        int cid;
        int type;
        char *apn;
        line = pCur->line;
        err = at_tok_start(&line);
        if (err < 0) {
            pdns->nCid = -1;
        }
        err = at_tok_nextint(&line, &pdns->nCid);
        if (err < 0) {
            pdns->nCid = -1;
        }
        cid = pdns->nCid;
        if (pdns->nCid > MAX_PDP) {
            continue;
        }
        err = at_tok_nextint(&line, &active);
        if (err < 0 || active == 0) {
            pdns->nCid = -1;
        }
        if (active == 1) {
            s_activePDN++;
        }
        /* apn */
        err = at_tok_nextstr(&line, &s_PDN[cid - 1].strApn);
        if (err < 0) {
            s_PDN[cid - 1].nCid = -1;
        }
        /* type */
        err = at_tok_nextint(&line, &s_PDN[cid - 1].iPType);
        if (err < 0) {
            s_PDN[cid - 1].nCid = -1;
        }
        if (active > 0) {
            RLOGI("active PDN: cid = %d, iptype = %d, apn = %s",
                  s_PDN[cid - 1].nCid, s_PDN[cid - 1].iPType,
                  s_PDN[cid - 1].strApn);
        }
        pdns++;
    }

done:
    at_response_free(pdnResponse);
}

int
getPDNCid(int index) {
    if (index >= MAX_PDP_CP || index < 0) {
        return -1;
    } else {
        return s_PDN[index].nCid;
    }
}

MbimContextIpType
getPDNIPType(int index) {
    if (index >= MAX_PDP_CP || index < 0) {
        return MBIM_CONTEXT_IP_TYPE_DEFAULT;
    } else {
        return s_PDN[index].iPType;
    }
}

char *getPDNAPN(int index) {
    if (index >= MAX_PDP_CP || index < 0) {
        return NULL;
    } else {
        return s_PDN[index].strApn;
    }
}

bool
isProtocolEqual(MbimContextIpType   newIPType,
                MbimContextIpType   oldIPType) {
    bool ret = false;
    RLOGI("newIPType = %d; oldIPType = %d", newIPType, oldIPType);
    if (newIPType == MBIM_CONTEXT_IP_TYPE_IPV4V6 || newIPType == oldIPType) {
        ret = true;
    }
    return ret;
}

int
checkCmpAnchor(char *apn) {
    if (apn == NULL || strlen(apn) == 0) {
        return 0;
    }

    const int len = strlen(apn);
    int i;
    int nDotCount = 0;
    char strApn[ARRAY_SIZE] = {0};
    char tmp[ARRAY_SIZE] = {0};
    static char *str[] = {".GPRS", ".MCC", ".MNC"};

    // if the length of apn is less than "mncxxx.mccxxx.gprs",
    // we would not continue to check.
    if (len <= MINIMUM_APN_LEN) {
        return len;
    }

    snprintf(strApn, sizeof(strApn), "%s", apn);
    RLOGD("getOrgApnlen: apn = %s, strApn = %s, len = %d", apn, strApn, len);

    memset(tmp, 0, sizeof(tmp));
    strncpy(tmp, apn + (len - 5), 5);
    RLOGD("getOrgApnlen: tmp = %s", tmp);
    if (strcasecmp(str[0], tmp)) {
        return len;
    }
    memset(tmp, 0, sizeof(tmp));

    strncpy(tmp, apn + (len - 12), strlen(str[1]));
    RLOGD("getOrgApnlen: tmp = %s", tmp);
    if (strcasecmp(str[1], tmp)) {
        return len;
    }
    memset(tmp, 0, sizeof(tmp));

    strncpy(tmp, apn + (len - MINIMUM_APN_LEN), strlen(str[2]));
    RLOGD("getOrgApnlen: tmp = %s", tmp);
    if (strcasecmp(str[2], tmp)) {
        return len;
    }
    return (len - MINIMUM_APN_LEN);
}

bool
isApnEqual(char *newStr, char *oldStr) {
    char strApnName[ARRAY_SIZE] = {0};
    strncpy(strApnName, oldStr, checkCmpAnchor(oldStr));
    strApnName[strlen(strApnName)] = '\0';
    if (isStrEmpty(newStr) || isStrEqual(newStr, oldStr) ||
        isStrEqual(strApnName, newStr)) {
        return true;
    }
    return false;
}

int
setPDPByIndex(int               index,
              int               cid,
              PDPState          state,
              MbimContextType   apnType) {
    if (index >= 0 && index < MAX_PDP) {  // cid: 1 ~ MAX_PDP
        pthread_mutex_lock(&s_PDP[index].mutex);
        RLOGD("setPDPByIndex: PDP[%d].state = %d, state = %d, cid = %d ", index, s_PDP[index].state, state, cid);
        s_PDP[index].cid = cid;
        s_PDP[index].state = state;
        if (s_PDP[index].state == PDP_ACTIVATED || s_PDP[index].state == PDP_IDLE) {
            s_PDP[index].apnType = apnType;
        }
        pthread_mutex_unlock(&s_PDP[index].mutex);
        return index;
    }
    return -1;
}

int
getPDP(int sessionId){
    int ret = -1;
    int i,cid;
    int busy = 0;
    RLOGD("sessionId = %d",sessionId);
    pthread_mutex_lock(&s_PDP[sessionId].mutex);
    if (s_PDP[sessionId].state != PDP_IDLE) {
    	pthread_mutex_unlock(&s_PDP[sessionId].mutex);
        return -2;
    }
    pthread_mutex_unlock(&s_PDP[sessionId].mutex);
    for (cid = 1; cid <= MAX_PDP; cid++) {
        if (s_activePDN > 0 && s_PDN[cid - 1].nCid == cid) {
            continue;
        }
        for(i = 0; i < MAX_PDP; i++){
            pthread_mutex_lock(&s_PDP[i].mutex);
            if (s_PDP[i].cid == cid) {
                pthread_mutex_unlock(&s_PDP[i].mutex);
                busy = 1;
                break;
            }
            pthread_mutex_unlock(&s_PDP[i].mutex);
        }
        if(busy == 0){
            return cid;
        }
    }
    return ret;

}

PDPState
getPDPState(int index) {
    if (index >= MAX_PDP || index < 0) {
        return PDP_IDLE;
    } else {
        return s_PDP[index].state;
    }
}

int
getPDPCidBysessionId(int sessionId) {
    if (sessionId >= MAX_PDP || sessionId < 0) {
        return -1;
    } else {
        return s_PDP[sessionId].cid;
    }
}

MbimActivationState mapPDPState(PDPState pdpState) {
    MbimActivationState activationState = MBIM_ACTIVATION_STATE_UNKNOWN;
    switch (pdpState) {
    case PDP_IDLE:
        activationState = MBIM_ACTIVATION_STATE_DEACTIVATED;
        break;
    case PDP_ACTIVATING:
        activationState = MBIM_ACTIVATION_STATE_ACTIVATING;
        break;
    case PDP_DEACTIVATING:
        activationState = MBIM_ACTIVATION_STATE_DEACTIVATING;
        break;
    case PDP_ACTIVATED:
        activationState = MBIM_ACTIVATION_STATE_ACTIVATED;
        break;
    default:
        activationState = MBIM_ACTIVATION_STATE_UNKNOWN;
        break;
    }
    return activationState;
}

#if 0
static int errorHandlingForCGDATA(int channelID, ATResponse *p_response,
                                      int err, int cid) {
    int failCause;
    int ret = DATA_ACTIVE_SUCCESS;
    char cmd[AT_COMMAND_LEN] = {0};
    char *line;
    RIL_SOCKET_ID socket_id = getSocketIdByChannelID(channelID);

    if (err < 0 || p_response->success == 0) {
        ret = DATA_ACTIVE_FAILED;
        if (p_response != NULL &&
                strStartsWith(p_response->finalResponse, "+CME ERROR:")) {
            line = p_response->finalResponse;
            err = at_tok_start(&line);
            if (err >= 0) {
                err = at_tok_nextint(&line, &failCause);
                if (err >= 0) {
                    convertFailCause(socket_id, failCause);
                } else {
                    s_lastPDPFailCause[socket_id] = PDP_FAIL_ERROR_UNSPECIFIED;
                }
            }
        } else {
            s_lastPDPFailCause[socket_id] = PDP_FAIL_ERROR_UNSPECIFIED;
        }
        // when cgdata timeout then send deactive to modem
        if (err == AT_ERROR_TIMEOUT || (p_response != NULL &&
                strStartsWith(p_response->finalResponse, "ERROR"))) {
            s_lastPDPFailCause[socket_id] = PDP_FAIL_ERROR_UNSPECIFIED;
            snprintf(cmd, sizeof(cmd), "AT+CGACT=0,%d", cid);
            at_send_command(s_ATChannels[channelID], cmd, NULL);
            cgact_deact_cmd_rsp(cid);
        }
    }
    return ret;
}
#endif

void
query_connect_info(Mbim_Token       token,
                   void *           data,
                   size_t           dataLen) {
    MBIM_UNUSED_PARM(dataLen);

    RIL_SOCKET_ID socket_id  = RIL_SOCKET_1;
    int channelID = getChannel(socket_id);
    int sessionId = ((int *)data)[0];
    MbimConnectInfo *queryConnectInfo =
            (MbimConnectInfo *)calloc(1, sizeof(MbimConnectInfo));

    RLOGI("query_connect_info sessionId = %d; cid = %d state = %d",queryConnectInfo->sessionId,s_PDP[sessionId].cid,s_PDP[sessionId].state);
    if (getPDPState(sessionId) == PDP_IDLE) {
        queryConnectInfo->activationState = MBIM_ACTIVATION_STATE_DEACTIVATED;
        queryConnectInfo->contextType =
                *mbim_uuid_from_context_type(MBIM_CONTEXT_TYPE_NONE);
        mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE,
                queryConnectInfo, sizeof(MbimConnectInfo));
        free(queryConnectInfo);
        putChannel(channelID);
        return;
    }

    queryAllActivePDNInfos(channelID);

    queryConnectInfo->sessionId = sessionId;
    queryConnectInfo->activationState = mapPDPState(getPDPState(sessionId));
    queryConnectInfo->voiceCallState = MBIM_VOICE_CALL_STATE_NONE;
    queryConnectInfo->nwError = MBIM_NW_ERROR_UNKNOWN;

    if(queryConnectInfo->activationState == MBIM_ACTIVATION_STATE_ACTIVATED){
        int cid = getPDPCidBysessionId(queryConnectInfo->sessionId);
        queryConnectInfo->IPType = getPDNIPType(cid - 1);
        queryConnectInfo->contextType =
                *mbim_uuid_from_context_type(MBIM_CONTEXT_TYPE_INTERNET);
    }

    mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE,
            queryConnectInfo, sizeof(MbimConnectInfo));
    putChannel(channelID);
    free(queryConnectInfo);
}


MbimContextIpType
readIPAddr(char *   raw,
           char *   rsp) {
    int comma_count = 0;
    int num = 0, comma4_num = 0, comma16_num = 0;
    int space_num = 0;
    char *buf = raw;
    int len = 0;
    MbimContextIpType ip_type = MBIM_CONTEXT_IP_TYPE_DEFAULT;

    if (raw != NULL) {
        len = strlen(raw);
        for (num = 0; num < len; num++) {
            if (raw[num] == '.') {
                comma_count++;
            }

            if (raw[num] == ' ') {
                space_num = num;
                break;
            }

            if (comma_count == 4 && comma4_num == 0) {
                comma4_num = num;
            }

            if (comma_count > 7 && comma_count == 16) {
                comma16_num = num;
                break;
            }
        }

        if (space_num > 0) {
            buf[space_num] = '\0';
            ip_type = MBIM_CONTEXT_IP_TYPE_IPV6;
            memcpy(rsp, buf, strlen(buf) + 1);
        } else if (comma_count >= 7) {
            if (comma_count == 7) {  // ipv4
                buf[comma4_num] = '\0';
                ip_type = MBIM_CONTEXT_IP_TYPE_IPV4;
            } else {  // ipv6
                buf[comma16_num] = '\0';
                ip_type = MBIM_CONTEXT_IP_TYPE_IPV6;
            }
            memcpy(rsp, buf, strlen(buf) + 1);
        }
    }

    return ip_type;
}

int
getNetMask(char *   p_address,
           char **  p_out) {
    if (p_address == NULL) {
        return -1;
    }
    int dotCount = 0;
    char *p = NULL;
    for (p = p_address; *p != '\0'; p++) {
        if (*p == '.') dotCount++;
        if (dotCount == 4) break;
    }

    p++;
    *p_out = p;

    return 0;
}

int
cgcontrdp_set_cmd_rsp(ATResponse *p_response) {
    int err;
    int cid;
    int count = 0;
    int skip;
    int ip_type_num = 0;
    int maxPDPNum = MAX_PDP;

    char *input;
    char *local_addr_subnet_mask = NULL, *gw_addr = NULL, *netMaskAddr = NULL;;
    char *dns_prim_addr = NULL, *dns_sec_addr = NULL;
    char ip[IP_ADDR_SIZE * 4], dns1[IP_ADDR_SIZE * 4];
    char dns2[IP_ADDR_SIZE * 4], temp_netmask[IP_ADDR_SIZE * 4];
    char cmd[AT_COMMAND_LEN];
    char prop[92];
    char *sskip;
    char *tmp;

    MbimContextIpType ip_type;
    char ETH_SP[32];  // "ro.modem.*.eth"
    ATLine *p_cur = NULL;

    if (p_response == NULL) {
        RLOGE("leave cgcontrdp_set_cmd_rsp:AT_RESULT_NG");
        return AT_RESULT_NG;
    }

    memset(ip, 0, sizeof(ip));
    memset(dns1, 0, sizeof(dns1));
    memset(dns2, 0, sizeof(dns2));
    memset(temp_netmask, 0, sizeof(temp_netmask));

    snprintf(ETH_SP, sizeof(ETH_SP), "ro.modem.l.eth");
    property_get(ETH_SP, prop, "veth");

    for (p_cur = p_response->p_intermediates; p_cur != NULL;
         p_cur = p_cur->p_next) {
        input = p_cur->line;
        if (findInBuf(input, strlen(p_cur->line), "+CGCONTRDP")) {
            do {
                err = at_tok_flag_start(&input, ':');
                if (err < 0) break;

                err = at_tok_nextint(&input, &cid);  // cid
                if (err < 0) break;

                err = at_tok_nextint(&input, &skip);  // bearer_id
                if (err < 0) break;

                err = at_tok_nextstr(&input, &sskip);  // apn
                if (err < 0) break;

                if (at_tok_hasmore(&input)) {
                    // local_addr_and_subnet_mask
                    err = at_tok_nextstr(&input, &local_addr_subnet_mask);
                    if (err < 0) break;
                    memcpy(temp_netmask, local_addr_subnet_mask, sizeof(temp_netmask));
                    if (at_tok_hasmore(&input)) {
                        err = at_tok_nextstr(&input, &sskip);  // gw_addr
                        if (err < 0) break;

                        if (at_tok_hasmore(&input)) {
                            // dns_prim_addr
                            err = at_tok_nextstr(&input, &dns_prim_addr);
                            if (err < 0) break;

                            snprintf(dns1, sizeof(dns1), "%s", dns_prim_addr);

                            if (at_tok_hasmore(&input)) {
                                // dns_sec_addr
                                err = at_tok_nextstr(&input, &dns_sec_addr);
                                if (err < 0) break;

                                snprintf(dns2, sizeof(dns2), "%s", dns_sec_addr);
                            }
                        }
                    }
                }

                if ((cid < maxPDPNum) && (cid >= 1)) {

                    ip_type = readIPAddr(local_addr_subnet_mask, ip);
                    RLOGD("PS:cid = %d,ip_type = %d,ip = %s,dns1 = %s,dns2 = %s",
                             cid, ip_type, ip, dns1, dns2);

                    if (ip_type == MBIM_CONTEXT_IP_TYPE_IPV6) {  // ipv6
                        RLOGD("cgcontrdp_set_cmd_rsp: IPV6");
                        if (!strncasecmp(ip, "0000:0000:0000:0000",
                                strlen("0000:0000:0000:0000"))) {
                            // incomplete address
                            tmp = strchr(ip, ':');
                            if (tmp != NULL) {
                                snprintf(ip, sizeof(ip), "FE80%s", tmp);
                            }
                        }
                        memcpy(s_ipInfo[cid - 1].ipv6laddr, ip,
                                sizeof(s_ipInfo[cid - 1].ipv6laddr));
                        memcpy(s_ipInfo[cid - 1].ipv6dns1addr, dns1,
                                sizeof(s_ipInfo[cid - 1].ipv6dns1addr));

                        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ip_type %d",
                                  prop, cid - 1, MBIM_CONTEXT_IP_TYPE_IPV6);
                        system(cmd);
                        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ipv6_ip %s",
                                  prop, cid - 1, ip);
                        system(cmd);
                        snprintf(cmd, sizeof(cmd),
                                  "setprop net.%s%d.ipv6_dns1 %s", prop, cid - 1,
                                  dns1);
                        system(cmd);
                        if (strlen(dns2) != 0) {
                            if (!strcmp(dns1, dns2)) {
                                RLOGD("Use default DNS2 instead.");
                                snprintf(dns2, sizeof(dns2), "%s",
                                          DEFAULT_PUBLIC_DNS2_IPV6);
                            }
                        } else {
                            RLOGD("DNS2 is empty!!");
                        }
                        memcpy(s_ipInfo[cid - 1].ipv6dns2addr, dns2,
                                sizeof(s_ipInfo[cid - 1].ipv6dns2addr));
                        snprintf(cmd, sizeof(cmd),
                                  "setprop net.%s%d.ipv6_dns2 %s", prop, cid - 1,
                                  dns2);
                        system(cmd);

                        s_ipInfo[cid - 1].ip_type = MBIM_CONTEXT_IP_TYPE_IPV6;
                        ip_type_num++;
                    } else if (ip_type == MBIM_CONTEXT_IP_TYPE_IPV4) {  // ipv4
                        RLOGD("cgcontrdp_set_cmd_rsp: IPV4");
                        RLOGD("temp_netmask = %s",temp_netmask);
                        getNetMask(temp_netmask, &netMaskAddr);
                        snprintf(s_ipInfo[cid - 1].ipv4netmask,
                                sizeof(s_ipInfo[cid - 1].ipv4netmask), "%s", netMaskAddr);
                        RLOGD("s_ipInfo[cid - 1].ipv4netmask = %s", s_ipInfo[cid - 1].ipv4netmask);
                        memcpy(s_ipInfo[cid - 1].ipladdr, ip,
                                sizeof(s_ipInfo[cid - 1].ipladdr));
                        memcpy(s_ipInfo[cid - 1].dns1addr, dns1,
                                sizeof(s_ipInfo[cid - 1].dns1addr));

                        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ip_type %d",
                                  prop, cid - 1, MBIM_CONTEXT_IP_TYPE_IPV4);
                        system(cmd);
                        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ip %s", prop,
                                  cid - 1, ip);
                        system(cmd);
                        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.dns1 %s",
                                  prop, cid - 1, dns1);
                        system(cmd);
                        if (strlen(dns2) != 0) {
                            if (!strcmp(dns1, dns2)) {
                                RLOGD("Two DNS are the same, Use default DNS2 instead.");
                                snprintf(dns2, sizeof(dns2), "%s", DEFAULT_PUBLIC_DNS2);
                            }
                        } else {
                            RLOGD("DNS2 is empty!!");
                        }
                        memcpy(s_ipInfo[cid - 1].dns2addr, dns2,
                                sizeof(s_ipInfo[cid - 1].dns2addr));
                        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.dns2 %s",
                                  prop, cid - 1, dns2);
                        system(cmd);

                        s_ipInfo[cid - 1].ip_type = MBIM_CONTEXT_IP_TYPE_IPV4;
                        ip_type_num++;
                    } else {  // unknown
//                        s_ipInfo[cid - 1].state = PDP_STATE_EST_UP_ERROR;
                        RLOGD("PDP_STATE_EST_UP_ERROR: unknown ip type!");
                    }

                    if (ip_type_num > 1) {
                        RLOGD("cgcontrdp_set_cmd_rsp: IPV4V6");
                        s_ipInfo[cid - 1].ip_type = MBIM_CONTEXT_IP_TYPE_IPV4V6;
                        snprintf(cmd, sizeof(cmd), "setprop net.%s%d.ip_type %d",
                                 prop, cid - 1, MBIM_CONTEXT_IP_TYPE_IPV4V6);
                        system(cmd);
                    }
//                    s_ipInfo[cid - 1].state = PDP_STATE_ACTIVE;
                    RLOGD("PDP_STATE_ACTIVE");
                }
            } while (0);
        }
    }
    return AT_RESULT_OK;
}

int
getOnLinkPrefixLength(char *address) {
    if (address == NULL) {
        RLOGE("Invaild address");
        return -1;
    }
    RLOGD("address = %s", address);

    uint8_t addr[16];
    memset(addr, 0, sizeof(addr));


    inet_pton(AF_INET, address, addr);


    int bitCount = 0;
    for (uint32_t i = 0; i < 16; i++) {
        RLOGD("addr[%d] = %0x", i, addr[i]);

        while (addr[i])  {
            bitCount++ ;
            addr[i] &= (addr[i] - 1) ;
        }
    }

    RLOGD("bitCount = %d", bitCount);
    return bitCount;
}

/*****************************************************************************/

void
connect_state_notify(const char *s) {
    RLOGD("connect_state_notify");
    int commas,err;
    int cid =-1;
    int endStatus;
    int ccCause;
    int notify = 0;
    char *p;
    char *tmp;
    char *line = NULL;

    line = strdup(s);
    tmp = line;
    at_tok_start(&tmp);

    MbimConnectInfo *connectInfo =
            (MbimConnectInfo *)calloc(1, sizeof(MbimConnectInfo));
    commas = 0;
    for (p = tmp; *p != '\0'; p++) {
        if (*p == ',') commas++;
    }
    err = at_tok_nextint(&tmp, &cid);
    if (err < 0) goto out;

    skipNextComma(&tmp);

    err = at_tok_nextint(&tmp, &endStatus);
    if (err < 0) goto out;

    err = at_tok_nextint(&tmp, &ccCause);
    if (err < 0) goto out;

    if (cid > 0 && cid <= MAX_PDP)
    RLOGD("s_PDP[%d].state = %d", cid - 1, s_PDP[cid - 1].state);

    if (commas == 4) {  /* GPRS reply 5 parameters */
        /* as endStatus 21 means: PDP reject by network,
         * so we not do connect_state_notify */
        if (endStatus != 29 && endStatus != 21) {
            if (endStatus == 104) {
                if (cid > 0 && cid <= MAX_PDP &&
                    s_PDP[cid - 1].state == PDP_ACTIVATED) {
                    notify = 1;
                }
            } else {
                notify = 1;
            }
        }
    }
    RLOGD("notify = %d",notify);
    if (notify == 1) {
        connectInfo->sessionId = cid-1;
        connectInfo->activationState = MBIM_ACTIVATION_STATE_DEACTIVATED;
        connectInfo->contextType = *mbim_uuid_from_context_type(MBIM_CONTEXT_TYPE_NONE);
        mbim_device_indicate_status(MBIM_SERVICE_BASIC_CONNECT,
                MBIM_CID_BASIC_CONNECT_CONNECT, connectInfo,
                sizeof(MbimConnectInfo));
    }

out:
    free(connectInfo);
}

void
packet_service_notify() {
    RLOGD("packet_service_notify");
    MbimPacketServiceInfo *packetServiceInfo =
                (MbimPacketServiceInfo *)calloc(1, sizeof(MbimPacketServiceInfo));
    packetServiceInfo->nwError = MBIM_NW_ERROR_UNKNOWN;
    packetServiceInfo->highestAvailableDataClass = MBIM_DATA_CLASS_NONE;
    if (s_regState == MBIM_REGISTER_STATE_HOME ||
            s_regState == MBIM_REGISTER_STATE_ROAMING ||
            s_regState == MBIM_REGISTER_STATE_PARTNER) {
        packetServiceInfo->packetServiceState = MBIM_PACKET_SERVICE_STATE_ATTACHED;
        packetServiceInfo->highestAvailableDataClass = s_availableDataClasses;
        // todo query from CP
        packetServiceInfo->upLinkSpeed = 52428800;
        packetServiceInfo->downLinkSpeed = 157286400;
    } else if (s_regState == MBIM_REGISTER_STATE_DEREGISTERED ||
            s_regState == MBIM_REGISTER_STATE_SEARCHING ||
            s_regState == MBIM_REGISTER_STATE_DENIED) {
        packetServiceInfo->packetServiceState = MBIM_PACKET_SERVICE_STATE_DETACHED;
    }
    mbim_device_indicate_status(MBIM_SERVICE_BASIC_CONNECT,
            MBIM_CID_BASIC_CONNECT_PACKET_SERVICE, packetServiceInfo,
            sizeof(MbimPacketServiceInfo));
    free(packetServiceInfo);
}


void
register_state_notify() {
    RLOGD("register_state_notify");
    MbimRegistrationStateInfo_2 *registerState =
            (MbimRegistrationStateInfo_2 *)calloc(1,
                    sizeof(MbimRegistrationStateInfo_2));
    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    int channelID = getChannel(socket_id);
    int ret = getRegistrationStateInfo(registerState, channelID);
    putChannel(channelID);
    mbim_device_indicate_status(MBIM_SERVICE_BASIC_CONNECT,
            MBIM_CID_BASIC_CONNECT_REGISTER_STATE, registerState,
            sizeof(MbimRegistrationStateInfo_2));

    packet_service_notify();

    memsetAndFreeStrings(3, registerState->providerId,
            registerState->providerName, registerState->roamingText);
    free(registerState);
}

void
subscriber_ready_status_notify() {
    RLOGD("subscriber_ready_status_notify");
    MbimSubscriberReadyInfo_2 *subscriberReadyInfo =
            (MbimSubscriberReadyInfo_2 *)calloc(1, sizeof(MbimSubscriberReadyInfo_2));

    requestTimedCallback(getSubscriberReadyInfo, subscriberReadyInfo, NULL);

    mbim_device_indicate_status(MBIM_SERVICE_BASIC_CONNECT,
            MBIM_CID_BASIC_CONNECT_SUBSCRIBER_READY_STATUS, subscriberReadyInfo,
            sizeof(MbimRegistrationStateInfo_2));

    memsetAndFreeStrings(2, subscriberReadyInfo->subscriberId,
            subscriberReadyInfo->simIccId);
    for (uint32_t i = 0; i < subscriberReadyInfo->elementCount; i++) {
        free(subscriberReadyInfo->telephoneNumbers[i]);
    }
    free(subscriberReadyInfo->telephoneNumbers);
    free(subscriberReadyInfo);
}

void
onSimStatusChanged(const char *s) {
    int err;
    int type;
    int value = 0, cause = -1;
    char *line = NULL;
    char *tmp;

    line = strdup(s);
    tmp = line;
    at_tok_start(&tmp);

    err = at_tok_nextint(&tmp, &type);
    if (err < 0) goto out;

    if (type == 3) {
        if (at_tok_hasmore(&tmp)) {
            err = at_tok_nextint(&tmp, &value);
            if (err < 0) goto out;
            if (value == 1) {
                if (at_tok_hasmore(&tmp)) {
                    err = at_tok_nextint(&tmp, &cause);
                    if (err < 0) goto out;

                    if (cause == 2 || cause == 34 || cause == 1 || cause == 7) {
                        subscriber_ready_status_notify();
                    }
                }
            } else if (value == 100 || value == 4 || value == 0 || value == 2) {
                subscriber_ready_status_notify();
            }
        }
    }

out:
    free(line);
}

void
signal_state_notify(const char *s) {
    RLOGD("register_state_notify");
    MbimSignalStateInfo *signalState =
            (MbimSignalStateInfo *)calloc(1, sizeof(MbimSignalStateInfo));

    int ret = parseSignalStateInfo(s, signalState);

    mbim_device_indicate_status(MBIM_SERVICE_BASIC_CONNECT,
            MBIM_CID_BASIC_CONNECT_SIGNAL_STATE, signalState,
            sizeof(MbimSignalStateInfo));
    free(signalState);
}

int
processBasicConnectUnsolicited(const char *s) {
    char *line = NULL;
    int err;

    if (strStartsWith(s, "+CREG:")) {
        int regState;
        char *tmp = NULL;
        MbimEmergencyModeInfo emergencyModeInfo;

        line = strdup(s);
        tmp = line;
        at_tok_start(&tmp);

        err = at_tok_nextint(&tmp, &regState);

        emergencyModeInfo.emergencyMode = getEmergencyModeState(regState);
        if (emergencyModeInfo.emergencyMode != s_emergencyModeState) {
            s_emergencyModeState = emergencyModeInfo.emergencyMode;

            mbim_device_indicate_status(
                    MBIM_SERVICE_BASIC_CONNECT,
                    MBIM_CID_BASIC_CONNECT_EMERGENCY_MODE,
                    &emergencyModeInfo,
                    sizeof(MbimEmergencyModeInfo));
        }
    } else if (strStartsWith(s, "+CEREG:")) {
        requestTimedCallback(register_state_notify, NULL, NULL);
    } else if (strStartsWith(s, "+ECIND:")) {
//        requestTimedCallback(onSimStatusChanged, s, NULL);
    } else if (strStartsWith(s, "+CESQ:")) {
        cesq_unsol_rsp(s, RIL_SOCKET_1, NULL);
//        requestTimedCallback(signal_state_notify, s, NULL);
    } else {
        return 0;
    }

out:
    free(line);
    return 1;
}

int getNextAddr(char *p_address, char **p_out) {
    if (p_address == NULL) {
        return -1;
    }

    int dotCount = 0;
    char *p = NULL;
    for (p = p_address; *p != '\0'; p++) {
        if (*p == '.') dotCount++;

        if (dotCount == 4) break;
    }

    p++;
    *p_out = p;

    return 0;
}

/*****************************************************************************/
int s_rxlev[SIM_COUNT], s_ber[SIM_COUNT], s_rscp[SIM_COUNT];
int s_ecno[SIM_COUNT], s_rsrq[SIM_COUNT], s_rsrp[SIM_COUNT];
int s_rssi[SIM_COUNT], s_berr[SIM_COUNT];

#define SIG_POOL_SIZE           10
static int
least_squares(int y[]) {
    int i = 0;
    int x[SIG_POOL_SIZE] = {0};
    int sum_x = 0, sum_y = 0, sum_xy = 0, square = 0;
    float a = 0.0, b = 0.0, value = 0.0;

    for (i = 0; i < SIG_POOL_SIZE; ++i) {
        x[i] = i;
        sum_x += x[i];
        sum_y += y[i];
        sum_xy += x[i] * y[i];
        square += x[i] * x[i];
    }
    a = ((float)(sum_xy * SIG_POOL_SIZE - sum_x * sum_y))
            / (square * SIG_POOL_SIZE - sum_x * sum_x);
    b = ((float)sum_y) / SIG_POOL_SIZE - a * sum_x / SIG_POOL_SIZE;
    value = a * x[SIG_POOL_SIZE - 1] + b;
    return (int)(value) + ((int)(10 * value) % 10 < 5 ? 0 : 1);
}

void *
signal_process() {
    int sim_index = 0;
    int i = 0;
    int sample_rsrp_sim[SIM_COUNT][SIG_POOL_SIZE];
    int sample_rscp_sim[SIM_COUNT][SIG_POOL_SIZE];
    int sample_rxlev_sim[SIM_COUNT][SIG_POOL_SIZE];
    int sample_rssi_sim[SIM_COUNT][SIG_POOL_SIZE];
    int *rsrp_array = NULL, *rscp_array = NULL, *rxlev_array = NULL, newSig;
    int upValue = -1, lowValue = -1;
    int rsrp_value = 0, rscp_value = 0, rxlev_value = 0;
    // 3 means count 2G/3G/4G
    int nosigUpdate[SIM_COUNT], MAXSigCount = 3 * (SIG_POOL_SIZE - 1);
    int noSigChange = 0;  // numbers of SIM cards with constant signal value

    memset(sample_rsrp_sim, 0, sizeof(int) * SIM_COUNT * SIG_POOL_SIZE);
    memset(sample_rscp_sim, 0, sizeof(int) * SIM_COUNT * SIG_POOL_SIZE);
    memset(sample_rxlev_sim, 0, sizeof(int) * SIM_COUNT * SIG_POOL_SIZE);
    memset(sample_rssi_sim, 0, sizeof(int) * SIM_COUNT * SIG_POOL_SIZE);

    if (!s_isLTE) {
        MAXSigCount = SIG_POOL_SIZE - 1;
    }

    while (1) {
        noSigChange = 0;
        for (sim_index = 0; sim_index < SIM_COUNT; sim_index++) {
            // compute the rsrp(4G) s_rscp(3G) s_rxlev(2G) or rssi(CSQ)
            if (!s_isLTE) {
                rsrp_array = NULL;
                rscp_array = sample_rssi_sim[sim_index];
                rxlev_array = NULL;
                newSig = s_rssi[sim_index];
                upValue = 31;
                lowValue = 0;
            } else {
                rsrp_array = sample_rsrp_sim[sim_index];
                rscp_array = sample_rscp_sim[sim_index];
                rxlev_array = sample_rxlev_sim[sim_index];
                newSig = s_rscp[sim_index];
                upValue = 140;
                lowValue = 44;
            }
            nosigUpdate[sim_index] = 0;

            for (i = 0; i < SIG_POOL_SIZE - 1; ++i) {
                if (rsrp_array != NULL) {  // w/td mode no rsrp
                    if (rsrp_array[i] == rsrp_array[i + 1]) {
                        if (rsrp_array[i] == s_rsrp[sim_index]) {
                            nosigUpdate[sim_index]++;
                        } else if (rsrp_array[i] == 0 ||
                                   rsrp_array[i] < lowValue ||
                                   rsrp_array[i] > upValue) {
                            rsrp_array[i] = s_rsrp[sim_index];
                        }
                    } else {
                        rsrp_array[i] = rsrp_array[i + 1];
                    }
                }

                if (rscp_array != NULL) {
                    if (rscp_array[i] == rscp_array[i + 1]) {
                        if (rscp_array[i] == newSig) {
                            nosigUpdate[sim_index]++;
                        } else if (rscp_array[i] <= 0 || rscp_array[i] > 31) {
                            // the first unsolicitied
                            rscp_array[i] = newSig;
                        }
                    } else {
                        rscp_array[i] = rscp_array[i + 1];
                    }
                }

                if (rxlev_array != NULL) {  // w/td mode no rxlev
                    if (rxlev_array[i] == rxlev_array[i + 1]) {
                        if (rxlev_array[i] == s_rxlev[sim_index]) {
                            nosigUpdate[sim_index]++;
                        } else if (rxlev_array[i] <= 0 ||
                                    rxlev_array[i] > 31) {
                            rxlev_array[i] = s_rxlev[sim_index];
                        }
                    } else {
                        rxlev_array[i] = rxlev_array[i + 1];
                    }
                }
            }

            if (nosigUpdate[sim_index] == MAXSigCount) {
                noSigChange++;
                continue;
            }

            if (rsrp_array != NULL) {  // w/td mode no rsrp
                rsrp_array[SIG_POOL_SIZE - 1] = s_rsrp[sim_index];
                if (rsrp_array[SIG_POOL_SIZE - 1] <=
                    rsrp_array[SIG_POOL_SIZE - 2]) {  // signal go up
                    rsrp_value = s_rsrp[sim_index];
                } else {  // signal come down
                    if (rsrp_array[SIG_POOL_SIZE - 1] ==
                        rsrp_array[SIG_POOL_SIZE - 2] + 1) {
                        rsrp_value = s_rsrp[sim_index];
                    } else {
                        rsrp_value = least_squares(rsrp_array);
                        // if invalid, use current value
                        if (rsrp_value < lowValue || rsrp_value > upValue  ||
                            rsrp_value > s_rsrp[sim_index]) {
                            rsrp_value = s_rsrp[sim_index];
                        }
                        rsrp_array[SIG_POOL_SIZE - 1] = rsrp_value;
                    }
                }
            }

            if (rscp_array != NULL) {
                rscp_array[SIG_POOL_SIZE - 1] = newSig;
                if (rscp_array[SIG_POOL_SIZE - 1]
                        >= rscp_array[SIG_POOL_SIZE - 2]) {  // signal go up
                    rscp_value = newSig;
                } else {  // signal come down
                    if (rscp_array[SIG_POOL_SIZE - 1]
                            == rscp_array[SIG_POOL_SIZE - 2] - 1) {
                        rscp_value = newSig;
                    } else {
                        rscp_value = least_squares(rscp_array);
                        // if invalid, use current value
                        if (rscp_value < 0 || rscp_value > 31
                                || rscp_value < newSig) {
                            rscp_value = newSig;
                        }
                        rscp_array[SIG_POOL_SIZE - 1] = rscp_value;
                    }
                }
            }
            if (rxlev_array != NULL) {  // w/td mode no rxlev
                rxlev_array[SIG_POOL_SIZE - 1] = s_rxlev[sim_index];
                if (rxlev_array[SIG_POOL_SIZE - 1]
                        >= rxlev_array[SIG_POOL_SIZE - 2]) {  // signal go up
                    rxlev_value = s_rxlev[sim_index];
                } else {  // signal come down
                    if (rxlev_array[SIG_POOL_SIZE - 1]
                            == rxlev_array[SIG_POOL_SIZE - 2] - 1) {
                        rxlev_value = s_rxlev[sim_index];
                    } else {
                        rxlev_value = least_squares(rxlev_array);
                        // if invalid, use current value
                        if (rxlev_value < 0 || rxlev_value > 31
                                || rxlev_value < s_rxlev[sim_index]) {
                            rxlev_value = s_rxlev[sim_index];
                        }
                        rxlev_array[SIG_POOL_SIZE - 1] = rxlev_value;
                    }
                }
            }

            if (s_isLTE) {  // l/tl/lf
                //RIL_SignalStrength_v6 response_v6;
                int response[6] = {-1, -1, -1, -1, -1, -1};
                //RIL_SIGNALSTRENGTH_INIT(response_v6);

                response[0] = rxlev_value;
                response[1] = s_ber[sim_index];
                response[2] = rscp_value;
                response[5] = rsrp_value;

                char newLine[AT_COMMAND_LEN] = {0};

                snprintf(newLine, AT_COMMAND_LEN, "+CESQ: %d,%d,%d,%d,%d,%d",
                         s_rxlev[sim_index], s_ber[sim_index], s_rscp[sim_index],
                         s_ecno[sim_index], s_rsrq[sim_index], s_rsrp[sim_index]);

                signal_state_notify(newLine);

//                if (response[0] != -1 && response[0] != 99) {
//                    response_v6.GW_SignalStrength.signalStrength = response[0];
//                }
//                if (response[2] != -1 && response[2] != 255) {
//                    response_v6.GW_SignalStrength.signalStrength = response[2];
//                }
//                if (response[5] != -1 && response[5] != 255 && response[5] != -255) {
//                    response_v6.LTE_SignalStrength.rsrp = response[5];
//                }
                RLOGD("rxlev[%d]=%d, rscp[%d]=%d, rsrp[%d]=%d", sim_index,
                     rxlev_value, sim_index, rscp_value, sim_index, rsrp_value);
//                RIL_onUnsolicitedResponse(RIL_UNSOL_SIGNAL_STRENGTH,
//                        &response_v6, sizeof(RIL_SignalStrength_v6), sim_index);
            } else {  // w/t
//                RIL_SignalStrength_v6 responseV6;
//                RIL_SIGNALSTRENGTH_INIT(responseV6);
//                responseV6.GW_SignalStrength.signalStrength = rscp_value;
//                responseV6.GW_SignalStrength.bitErrorRate = s_ber[sim_index];
                RLOGD("rssi[%d]=%d", sim_index, rscp_value);
//                RIL_onUnsolicitedResponse(RIL_UNSOL_SIGNAL_STRENGTH,
//                        &responseV6, sizeof(RIL_SignalStrength_v6), sim_index);
            }
        }
        sleep(5);
//        if ((noSigChange == SIM_COUNT) || (s_screenState == 0)) {
//            pthread_mutex_lock(&s_signalProcessMutex);
//            pthread_cond_wait(&s_signalProcessCond, &s_signalProcessMutex);
//            pthread_mutex_unlock(&s_signalProcessMutex);
//        }
    }
    return NULL; /*lint !e527 */
}

/* for +CESQ: unsol response process */
void
cesq_unsol_rsp(char *           line,
               RIL_SOCKET_ID    socket_id,
               char *           newLine) {
    MBIM_UNUSED_PARM(newLine);

    int err;
    char *atInStr;

    atInStr = line;
    err = at_tok_flag_start(&atInStr, ':');
    if (err < 0) goto error;

    err = at_tok_nextint(&atInStr, &s_rxlev[socket_id]);
    if (err < 0) goto error;

    if (s_rxlev[socket_id] <= 61) {
        s_rxlev[socket_id] = (s_rxlev[socket_id] + 2) / 2;
    } else if (s_rxlev[socket_id] > 61 && s_rxlev[socket_id] <= 63) {
        s_rxlev[socket_id] = 31;
    } else if (s_rxlev[socket_id] >= 100 && s_rxlev[socket_id] < 103) {
        s_rxlev[socket_id] = 0;
    } else if (s_rxlev[socket_id] >= 103 && s_rxlev[socket_id] < 165) {
        s_rxlev[socket_id] = ((s_rxlev[socket_id] - 103) + 1) / 2;
    } else if (s_rxlev[socket_id] >= 165 && s_rxlev[socket_id] <= 191) {
        s_rxlev[socket_id] = 31;
    }

    err = at_tok_nextint(&atInStr, &s_ber[socket_id]);
    if (err < 0) goto error;

    err = at_tok_nextint(&atInStr, &s_rscp[socket_id]);
    if (err < 0) goto error;

    if (s_rscp[socket_id] >= 100 && s_rscp[socket_id] < 103) {
        s_rscp[socket_id] = 0;
    } else if (s_rscp[socket_id] >= 103 && s_rscp[socket_id] < 165) {
        s_rscp[socket_id] = ((s_rscp[socket_id] - 103) + 1) / 2;
    } else if (s_rscp[socket_id] >= 165 && s_rscp[socket_id] <= 191) {
        s_rscp[socket_id] = 31;
    }

    err = at_tok_nextint(&atInStr, &s_ecno[socket_id]);
    if (err < 0) goto error;

    err = at_tok_nextint(&atInStr, &s_rsrq[socket_id]);
    if (err < 0) goto error;

    err = at_tok_nextint(&atInStr, &s_rsrp[socket_id]);
    if (err < 0) goto error;

    if (s_rsrp[socket_id] == 255) {
        s_rsrp[socket_id] = -255;
    } else {
        s_rsrp[socket_id] = 141 - s_rsrp[socket_id];
    }

//    if (s_psOpened[socket_id] == 1) {
//        if (s_isLTE) {
//            s_psOpened[socket_id] = 0;
//            snprintf(newLine, AT_COMMAND_LEN, "+CESQ: %d,%d,%d,%d,%d,%d",
//                     rxlev[socket_id], ber[socket_id], rscp[socket_id],
//                     ecno[socket_id], rsrq[socket_id], rsrp[socket_id]);
//            return AT_RESULT_OK;
//        }
//    }

error:
    return;
//    return AT_RESULT_NG;
}

void *
init_apn_process() {

    RLOGD("enter init_apn_process");
    loadAPNs();
    return NULL;
}

