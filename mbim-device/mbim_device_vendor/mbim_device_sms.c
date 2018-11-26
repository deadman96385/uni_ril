#define LOG_TAG "MBIM-Device"

#include <utils/Log.h>
#include "misc.h"
#include "mbim_device_sqlite.h"
#include "mbim_device_sms.h"
#include "mbim_service_sms.h"
#include "mbim_message.h"
#include "mbim_cid.h"
#include "mbim_cid.h"
#include "at_channel.h"
#include "mbim_device_vendor.h"

MbimSmsStatusFlag sSmsStatusFlag = MBIM_SMS_STATUS_FLAG_NONE;
int sNewSmsIndex = 0;
static char s_smsc[20] = {0};

void
mbim_device_sms(Mbim_Token                token,
                uint32_t                  cid,
                MbimMessageCommandType    commandType,
                void *                    data,
                size_t dataLen) {
    switch (cid) {
        case MBIM_CID_SMS_CONFIGURATION:
            sms_configuration(token, commandType, data, dataLen);
            break;
        case MBIM_CID_SMS_READ:
            sms_read(token, commandType, data, dataLen);
            break;
        case MBIM_CID_SMS_SEND:
            sms_send(token, commandType, data, dataLen);
            break;
        case MBIM_CID_SMS_DELETE:
            sms_delete(token, commandType, data, dataLen);
            break;
        case MBIM_CID_SMS_MESSAGE_STORE_STATUS:
            sms_message_store_status(token, commandType, data, dataLen);
            break;
        default:
            RLOGD("unsupproted oem cid");
            mbim_device_command_done(token, MBIM_STATUS_ERROR_NO_DEVICE_SUPPORT,
                    NULL, 0);
            break;
    }
}


/******************************************************************************/
/**
 * sms_read: query only
 *
 * query:
 *  @command:   the data shall be null
 *  @response:  the data shall be MbimDeviceCapsInfo_2
 */

void
sms_read(Mbim_Token                token,
          MbimMessageCommandType    commandType,
          void *                    data,
          size_t                    dataLen) {
    switch (commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            query_sms_read(token, data, dataLen);
            break;
        default:
            RLOGE("unsupported commandType");
            break;
    }
}

void
query_sms_read(Mbim_Token            token,
                void *                data,
                size_t              dataLen) {
    MbimSmsReadReq *smsReadReq = (MbimSmsReadReq *) data;
    MBIM_UNUSED_PARM(dataLen);

    int ret = -1;
    MbimSmsFlag flag = smsReadReq->flag;
    MbimSmsFormat fromT = smsReadReq->smsFormat;
    RLOGE("fromT IS %d", fromT);
    uint32_t messageIndex = 0;
    char *smsQuery = NULL;

    MbimSmsReadInfo_2 *smsReadInfo = (MbimSmsReadInfo_2 *) calloc(1,
            sizeof(MbimSmsReadInfo_2));

    switch (flag) {
    case MBIM_SMS_FLAG_ALL:
        ret = asprintf(&smsQuery, "SELECT * FROM %s ", MBIM_SMS_TABLE);
        if (ret < 0) {
            RLOGE("Failed to allocate memory");
            smsQuery = NULL;
            goto error;
        }
        break;
    case MBIM_SMS_FLAG_INDEX:
        messageIndex = smsReadReq->MessageIndex;
        ret = asprintf(&smsQuery, "SELECT * FROM %s WHERE _id = %d",
                MBIM_SMS_TABLE, messageIndex);
        if (ret < 0) {
            RLOGE("Failed to allocate memory");
            smsQuery = NULL;
            goto error;
        }
        break;
    case MBIM_SMS_FLAG_NEW:
        ret = asprintf(&smsQuery, "SELECT * FROM %s WHERE status = %d",
                MBIM_SMS_TABLE, MBIM_SMS_STATUS_NEW);
        if (ret < 0) {
            RLOGE("Failed to allocate memory");
            smsQuery = NULL;
            goto error;
        }
        break;

    case MBIM_SMS_FLAG_OLD:
        ret = asprintf(&smsQuery, "SELECT * FROM %s WHERE status = %d",
                MBIM_SMS_TABLE, MBIM_SMS_STATUS_OLD);
        if (ret < 0) {
            RLOGE("Failed to allocate memory");
            smsQuery = NULL;
            goto error;
        }
        break;
    case MBIM_SMS_FLAG_SENT:
        ret = asprintf(&smsQuery, "SELECT * FROM %s WHERE status = %d",
                MBIM_SMS_TABLE, MBIM_SMS_STATUS_SENT);
        if (ret < 0) {
            RLOGE("Failed to allocate memory");
            smsQuery = NULL;
            goto error;
        }
        break;
    case MBIM_SMS_FLAG_DRAFT:
        ret = asprintf(&smsQuery, "SELECT * FROM %s WHERE status = %d",
                MBIM_SMS_TABLE, MBIM_SMS_STATUS_DRAFT);
        if (ret < 0) {
            RLOGE("Failed to allocate memory");
            smsQuery = NULL;
            goto error;
        }
        break;
    }

    mbim_sms_sqlresult *smsresult = (mbim_sms_sqlresult *) calloc(1,
            sizeof(mbim_sms_sqlresult));

    mbim_sql_exec(SMS_QUERY, smsQuery, smsresult);

    uint32_t elementCount = smsresult->count;
    smsReadInfo->smsPduRecords = (MbimSmsPduRecord_2 **) calloc(elementCount,
            sizeof(MbimSmsPduRecord_2 *));
    smsReadInfo->elementCount = elementCount;
    smsReadInfo->format = MBIM_SMS_FORMAT_PDU;

    unsigned char **pduBytes = (unsigned char **) calloc(elementCount, sizeof(unsigned char *));
    for (int i = 0; i < elementCount; i++) {
        smsReadInfo->smsPduRecords[i] = (MbimSmsPduRecord_2 *) calloc(1,
                sizeof(MbimSmsPduRecord_2));
        smsReadInfo->smsPduRecords[i]->messageIndex = smsresult->table[i].id;
        smsReadInfo->smsPduRecords[i]->messageStatus =
                smsresult->table[i].status;

        pduBytes[i] = (unsigned char *) calloc(smsresult->table[i].pdu_size + 1, sizeof(unsigned char));
        hexStringToBytes(smsresult->table[i].pdu, pduBytes[i]);
        smsReadInfo->smsPduRecords[i]->pduData = pduBytes[i];
        smsReadInfo->smsPduRecords[i]->pduDataSize = smsresult->table[i].pdu_size;
    }

done:
    RLOGE("query_sms_read done");
    mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE, smsReadInfo,
            sizeof(MbimSmsReadInfo_2));
    for (int i = 0; i < elementCount; i++) {
        free(smsReadInfo->smsPduRecords[i]);
        free(smsresult->table[i].pdu);
        free(pduBytes[i]);
    }
    free(smsReadInfo->smsPduRecords);
    free(smsresult->table);
    free(pduBytes);
    free(smsReadInfo);
    return;

error:
    mbim_device_command_done(token, MBIM_STATUS_ERROR_FAILURE,
            smsReadInfo, sizeof(MbimSmsReadInfo_2));
    free(smsReadInfo);
    return;
}


/******************************************************************************/
/**
 * sms_delete: set only
 *
 * set:
 *  @command:   the data shall be null
 *  @response:  the data shall be MbimDeviceCapsInfo_2
 */

void
sms_delete(Mbim_Token                token,
                          MbimMessageCommandType    commandType,
                          void *                    data,
                          size_t                    dataLen) {
    switch (commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            set_sms_delete(token, data, dataLen);
            break;
        default:
            RLOGE("unsupported commandType");
            break;
    }
}

void
set_sms_delete(Mbim_Token            token,
                  void *                data,
                  size_t dataLen) {
    MbimSetSmsDelete *smsReadReq = (MbimSetSmsDelete *) data;
    MBIM_UNUSED_PARM(dataLen);

    int ret = -1;
    MbimSmsFlag flag = smsReadReq->flags;
    uint32_t messageIndex = 0;
    char *smsDelete = NULL;

    switch (flag) {
    case MBIM_SMS_FLAG_ALL:
        ret = asprintf(&smsDelete, "DELETE FROM %s ", MBIM_SMS_TABLE);
        if (ret < 0) {
            RLOGE("Failed to allocate memory");
            smsDelete = NULL;
            goto error;
        }
        break;
    case MBIM_SMS_FLAG_INDEX:
        messageIndex = smsReadReq->messageIndex;
        ret = asprintf(&smsDelete, "DELETE FROM %s WHERE _id = %d",
                MBIM_SMS_TABLE, messageIndex);
        if (ret < 0) {
            RLOGE("Failed to allocate memory");
            smsDelete = NULL;
            goto error;
        }
        break;
    case MBIM_SMS_FLAG_NEW:
        ret = asprintf(&smsDelete, "DELETE FROM %s WHERE status = %d",
                MBIM_SMS_TABLE, MBIM_SMS_STATUS_NEW);
        if (ret < 0) {
            RLOGE("Failed to allocate memory");
            smsDelete = NULL;
            goto error;
        }
        break;

    case MBIM_SMS_FLAG_OLD:
        ret = asprintf(&smsDelete, "DELETE FROM %s WHERE status = %d",
                MBIM_SMS_TABLE, MBIM_SMS_STATUS_OLD);
        if (ret < 0) {
            RLOGE("Failed to allocate memory");
            smsDelete = NULL;
            goto error;
        }
        break;
    case MBIM_SMS_FLAG_SENT:
        ret = asprintf(&smsDelete, "DELETE FROM %s WHERE status = %d",
                MBIM_SMS_TABLE, MBIM_SMS_STATUS_SENT);
        if (ret < 0) {
            RLOGE("Failed to allocate memory");
            smsDelete = NULL;
            goto error;
        }
        break;
    case MBIM_SMS_FLAG_DRAFT:
        ret = asprintf(&smsDelete, "DELETE FROM %s WHERE status = %d",
                MBIM_SMS_TABLE, MBIM_SMS_STATUS_DRAFT);
        if (ret < 0) {
            RLOGE("Failed to allocate memory");
            smsDelete = NULL;
            goto error;
        }
        break;
    }
    mbim_sql_exec(SMS_DELETE, smsDelete, NULL);
done:
    RLOGE("query_sms_delete done");
    mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE, NULL,
            sizeof(MbimSmsReadInfo_2));
    return;

error:
    mbim_device_command_done(token, MBIM_STATUS_ERROR_FAILURE,
            NULL, sizeof(MbimSmsReadInfo_2));
    return;
}

/******************************************************************************/
/**
 * sms_send: set only
 *
 * set:
 *  @command:   MBIM_SET_SMS_SEND
 *  @response:  MBIM_SMS_SEND_INFO
 */

void
sms_send(Mbim_Token                token,
          MbimMessageCommandType    commandType,
          void *                    data,
          size_t                    dataLen) {
    switch (commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            set_sms_send(token, data, dataLen);
            break;
        default:
            RLOGE("unsupported commandType");
            break;
    }
}


void
set_sms_send(Mbim_Token            token,
              void *                data,
              size_t                dataLen) {
    MbimSetSmsSend_2 *smsReadReq = (MbimSetSmsSend_2 *) data;
    MBIM_UNUSED_PARM(dataLen);

    int ret, err;
    MbimSmsFormat fromat = smsReadReq->smsFormat;
    char *pduData = smsReadReq->pduData;

    char *cmd1, *cmd2;
    int smsc_length = (hexCharToInt(smsReadReq->pduData[0]) << 4) | (hexCharToInt(smsReadReq->pduData[1]));
    int tpLayerLength = strlen(pduData) / 2 - smsc_length -1;

    MbimSmsSendInfo *smsSendInfo = (MbimSmsSendInfo *) calloc(1,
            sizeof(MbimSmsSendInfo));

    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    int channelID = getChannel(socket_id);

    ret = asprintf(&cmd1, "AT+CMGS=%d", tpLayerLength);
    if (ret < 0) {
        RLOGE("Failed to allocate memory");
        cmd1 = NULL;
        goto error1;
    }
    ret = asprintf(&cmd2, "%s%s", "", pduData);
    if (ret < 0) {
        RLOGE("Failed to allocate memory");
        free(cmd1);
        cmd2 = NULL;
        goto error1;
    }
    RIL_SMS_Response response;
    ATResponse *p_response = NULL;

    char *line = NULL;

    err = at_send_command_sms(s_ATChannels[channelID], cmd1, cmd2, "+CMGS:",
            &p_response);
    free(cmd1);
    free(cmd2);
    if (err != 0 || p_response->success == 0) {
        goto error;
    }

    /* FIXME fill in messageRef and ackPDU */
    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0)
        goto error1;

    err = at_tok_nextint(&line, &smsSendInfo->messageReference);
    if (err < 0)
        goto error1;

    char *smsInsert = NULL;
    ret = asprintf(&smsInsert, "INSERT INTO %s(status,pdu) VALUES(%d, '%s')", MBIM_SMS_TABLE, MBIM_SMS_STATUS_SENT, pduData);
    mbim_sql_exec(SMS_INSERT, smsInsert, NULL);
    free(smsInsert);
    RLOGD("set_sms_send done");
    mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE, smsSendInfo,
            sizeof(MbimSmsSendInfo));
    at_response_free(p_response);
    free(smsSendInfo);
    putChannel(channelID);
    return;

error:
    if (p_response == NULL) {
        goto error1;
    }
    line = p_response->finalResponse;
    err = at_tok_start(&line);
    if (err < 0)
        goto error1;

    err = at_tok_nextint(&line, &response.errorCode);
    if (err < 0)
        goto error1;
    mbim_device_command_done(token, MBIM_STATUS_ERROR_FAILURE, smsSendInfo,
            sizeof(MbimSmsSendInfo));
    at_response_free(p_response);
    free(smsSendInfo);
    putChannel(channelID);
    return;

error1:
    mbim_device_command_done(token, MBIM_STATUS_ERROR_FAILURE,
            smsSendInfo, sizeof(MbimSmsSendInfo));
    at_response_free(p_response);
    free(smsSendInfo);
    putChannel(channelID);
    return;
}

/******************************************************************************/
/**
 * sms_send: query only
 *
 * set:
 *  @command:   empty
 *  @response:  MBIM_SMS_STATUS_INFO
 */

void
sms_message_store_status(Mbim_Token                token,
                          MbimMessageCommandType    commandType,
                          void *                    data,
                          size_t                    dataLen) {
    switch (commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            query_sms_message_store_status(token, data, dataLen);
            break;
        default:
            RLOGE("unsupported commandType");
            break;
    }
}

void
query_sms_message_store_status(Mbim_Token            token,
                               void *                data,
                               size_t                dataLen) {

    MbimSmsStatusInfo *smsStatusInfo = (MbimSmsStatusInfo *) calloc(1,
            sizeof(MbimSmsStatusInfo));
    smsStatusInfo->flag = sSmsStatusFlag;
    smsStatusInfo->messageIndex = sNewSmsIndex;
    mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE, smsStatusInfo,
            sizeof(MbimSmsSendInfo));
    free(smsStatusInfo);
}


/******************************************************************************/
/**
 * sms_configuration: query set notification
 *
 * set:
 *  @command:   empty
 *  @response:  MBIM_SMS_CONFIGURATION_INFO
 */

void
sms_configuration(Mbim_Token                token,
                  MbimMessageCommandType    commandType,
                  void *                    data,
                  size_t                    dataLen) {
    switch (commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_QUERY:
            query_sms_configuration(token, data, dataLen);
            break;
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            set_sms_configuration(token, data, dataLen);
            break;
        default:
            RLOGE("unsupported commandType");
            break;
    }
}

void
query_sms_configuration(Mbim_Token            token,
                        void *                data,
                        size_t dataLen) {

    MbimSmsConfigurationInfo_2 *smsConfigurationInfo =
            (MbimSmsConfigurationInfo_2 *) calloc(1,
                    sizeof(MbimSmsConfigurationInfo_2));
    int err;
    char *sc_line = NULL;
    char *decidata = NULL;
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    int channelID = getChannel(socket_id);
    smsConfigurationInfo->smsStorageState = MBIM_SMS_STORAGE_STATE_INITIALIZED;
    smsConfigurationInfo->format = MBIM_SMS_FORMAT_PDU;
    smsConfigurationInfo->maxMessages = 1024;

    err = at_send_command_singleline(s_ATChannels[channelID], "AT+CSCA?",
            "+CSCA:", &p_response);
    if (err >= 0 && p_response->success) {
        char *line = p_response->p_intermediates->line;
        err = at_tok_start(&line);
        if (err >= 0) {
            err = at_tok_nextstr(&line, &sc_line);
            decidata = (char *) calloc((strlen(sc_line) / 2 + 1), sizeof(char));
            convertHexToBin(sc_line, strlen(sc_line), decidata);
            smsConfigurationInfo->scAddress = decidata;
        }
    }
    snprintf(s_smsc, sizeof(s_smsc), "%s", smsConfigurationInfo->scAddress);
    mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE,
            smsConfigurationInfo, sizeof(MbimSmsConfigurationInfo_2));
    if (decidata != NULL) {
        free(decidata);
    }
    free(smsConfigurationInfo);
    at_response_free(p_response);
    putChannel(channelID);
}

void set_sms_configuration(Mbim_Token            token,
        void *                data,
        size_t dataLen){
    char *cmd;
    int ret, err;
    MbimSmsConfigurationInfo_2 *smsConfigurationInfo =
            (MbimSmsConfigurationInfo_2 *) calloc(1,
                    sizeof(MbimSmsConfigurationInfo_2));
    MbimSmsSetConfiguration_2 *smsSetConfiguration = (MbimSmsSetConfiguration_2 *) data;
    smsConfigurationInfo->scAddress = s_smsc;
    char *smsc = smsSetConfiguration->scAddress;
    ATResponse *p_response = NULL;
    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    int channelID = getChannel(socket_id);
    int hexLen = strlen(smsc) * 2 + 1;
    unsigned char *hexData = (unsigned char *)calloc(hexLen, sizeof(unsigned char));
    convertBinToHex(data, strlen(data), hexData);
    ret = asprintf(&cmd, "AT+CSCA=\"%s\"", hexData);
    if (ret < 0) {
        RLOGE("Failed to allocate memory");
        free(hexData);
        cmd = NULL;
        goto done;
    }
    err = at_send_command(s_ATChannels[channelID], cmd, &p_response);
    if (!(err < 0 || p_response->success == 0)) {
        smsConfigurationInfo->scAddress = smsc;
    }
    free(cmd);
    free(hexData);
    at_response_free(p_response);

done:
    smsConfigurationInfo->format = smsSetConfiguration->format;
    smsConfigurationInfo->maxMessages = 1024;
    smsConfigurationInfo->smsStorageState = MBIM_SMS_STORAGE_STATE_INITIALIZED;
    mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE,
            smsConfigurationInfo, sizeof(MbimSmsConfigurationInfo_2));
    free(smsConfigurationInfo);
    putChannel(channelID);
}

int processSmsUnsolicited(const char *s, const char *sms_pdu) {
    char *line = NULL;
    int ret;

    if (strStartsWith(s, "+CMT:")) {
        MbimSmsStatusInfo *smsStatusInfo = (MbimSmsStatusInfo *) calloc(1,
                sizeof(MbimSmsStatusInfo));
        char *smsInsert = NULL;
        ret = asprintf(&smsInsert, "INSERT INTO %s(status,pdu) VALUES(%d, '%s')", MBIM_SMS_TABLE, MBIM_SMS_STATUS_NEW, sms_pdu);
        mbim_sql_exec(SMS_INSERT, smsInsert, NULL);

//        char *smsLastId = NULL;
//        ret = asprintf(&smsLastId, "select last_insert_rowid() from %s", MBIM_SMS_TABLE);
//        int *index;
//        mbim_sql_exec(SMS_LAST_INSERT_ROWID, smsLastId, index);
        smsStatusInfo->messageIndex = 0;
        smsStatusInfo->flag =MBIM_SMS_STATUS_NEW;
        mbim_device_indicate_status(
                MBIM_SERVICE_SMS,
                MBIM_CID_SMS_MESSAGE_STORE_STATUS,
                &smsStatusInfo,
                sizeof(MbimSmsStatusInfo));
        free(smsInsert);
    } else {
        return 0;
    }
out:
    return 1;
}
