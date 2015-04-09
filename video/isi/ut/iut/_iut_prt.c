/*
 * THIS IS AN UNPUBLISHED WORK CONTAINING D2 TECHNOLOGIES, INC. CONFIDENTIAL
 * AND PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
 * APPLIES: "COPYRIGHT 2006 D2 TECHNOLOGIES, INC. ALL RIGHTS RESERVED"
 *
 * $D2Tech$ $Rev: 28749 $ $Date: 2014-09-09 14:55:32 +0800 (Tue, 09 Sep 2014) $
 */

#include <osal.h>
#include <isi.h>
#include <isip.h>
#include "_iut_app.h"
#include "_iut_prt.h"
#include "_iut.h"

static char  _IUT_prtBuf[IUT_PRT_OUTPUT_BUFFER_MAX_SIZE + 1];

/* Look up table for call ISI event type categories */
static const char *_IUT_prtIdType[ISI_ID_TYPE_INVALID + 1] = {
    "ISI_ID_TYPE_NONE",
    "ISI_ID_TYPE_CALL",
    "ISI_ID_TYPE_TEL_EVENT",
    "ISI_ID_TYPE_SERVICE",
    "ISI_ID_TYPE_PRESENCE",
    "ISI_ID_TYPE_MESSAGE",
    "ISI_ID_TYPE_CHAT",
    "ISI_ID_TYPE_FILE",
    "ISI_ID_TYPE_INVALID",
};

/* Look up table for call ISI events */
static const char *_IUT_prtEvent[ISI_EVENT_INVALID + 1] = {
    "ISI_EVENT_NONE",
    "ISI_EVENT_NET_UNAVAILABLE",
    "ISI_EVENT_CONTACT_RECEIVED",
    "ISI_EVENT_CONTACT_SEND_OK",
    "ISI_EVENT_CONTACT_SEND_FAILED",
    "ISI_EVENT_SUB_TO_PRES_RECEIVED",
    "ISI_EVENT_SUB_TO_PRES_SEND_OK",
    "ISI_EVENT_SUB_TO_PRES_SEND_FAILED",
    "ISI_EVENT_SUBSCRIPTION_RESP_RECEIVED",
    "ISI_EVENT_SUBSCRIPTION_RESP_SEND_OK",
    "ISI_EVENT_SUBSCRIPTION_RESP_SEND_FAILED",
    "ISI_EVENT_PRES_RECEIVED",
    "ISI_EVENT_PRES_SEND_OK",
    "ISI_EVENT_PRES_SEND_FAILED",
    "ISI_EVENT_MESSAGE_RECEIVED",
    "ISI_EVENT_MESSAGE_REPORT_RECEIVED",
    "ISI_EVENT_MESSAGE_SEND_OK",
    "ISI_EVENT_MESSAGE_SEND_FAILED",
    "ISI_EVENT_CREDENTIALS_REJECTED",
    "ISI_EVENT_TEL_EVENT_RECEIVED",
    "ISI_EVENT_TEL_EVENT_SEND_OK",
    "ISI_EVENT_TEL_EVENT_SEND_FAILED",
    "ISI_EVENT_SERVICE_ACTIVE",
    "ISI_EVENT_SERVICE_INACTIVE",
    "ISI_EVENT_SERVICE_INIT_OK",
    "ISI_EVENT_SERVICE_INIT_FAILED",
    "ISI_EVENT_SERVICE_HANDOFF",
    "ISI_EVENT_CALL_FAILED",
    "ISI_EVENT_CALL_XFER_PROGRESS",
    "ISI_EVENT_CALL_XFER_REQUEST",
    "ISI_EVENT_CALL_XFER_FAILED",
    "ISI_EVENT_CALL_XFER_COMPLETED",
    "ISI_EVENT_CALL_MODIFY_FAILED",
    "ISI_EVENT_CALL_MODIFY_COMPLETED",
    "ISI_EVENT_CALL_HOLD",
    "ISI_EVENT_CALL_RESUME",
    "ISI_EVENT_CALL_REJECTED",
    "ISI_EVENT_CALL_ACCEPTED",
    "ISI_EVENT_CALL_ACKNOWLEDGED",
    "ISI_EVENT_CALL_DISCONNECTED",
    "ISI_EVENT_CALL_INCOMING",
    "ISI_EVENT_CALL_TRYING",
    "ISI_EVENT_CALL_HANDOFF",
    "ISI_EVENT_PROTOCOL_READY",
    "ISI_EVENT_PROTOCOL_FAILED",

    "ISI_EVENT_CHAT_INCOMING",
    "ISI_EVENT_CHAT_TRYING",
    "ISI_EVENT_CHAT_ACKNOWLEDGED",
    "ISI_EVENT_CHAT_ACCEPTED",
    "ISI_EVENT_CHAT_DISCONNECTED",
    "ISI_EVENT_CHAT_FAILED",
    "ISI_EVENT_GROUP_CHAT_INCOMING",
    "ISI_EVENT_GROUP_CHAT_PRES_RECEIVED",
    "ISI_EVENT_GROUP_CHAT_NOT_AUTHORIZED",
    "ISI_EVENT_DEFERRED_CHAT_INCOMING",

    "ISI_EVENT_CALL_MODIFY",
    "ISI_EVENT_FILE_SEND_PROGRESS",
    "ISI_EVENT_FILE_SEND_PROGRESS_COMPLETED",
    "ISI_EVENT_FILE_SEND_PROGRESS_FAILED",
    "ISI_EVENT_FILE_RECV_PROGRESS",
    "ISI_EVENT_FILE_RECV_PROGRESS_COMPLETED",
    "ISI_EVENT_FILE_RECV_PROGRESS_FAILED",
    "ISI_EVENT_FRIEND_REQ_RECEIVED",
    "ISI_EVENT_FRIEND_REQ_SEND_OK",
    "ISI_EVENT_FRIEND_REQ_SEND_FAILED",
    "ISI_EVENT_FRIEND_RESP_RECEIVED",
    "ISI_EVENT_FRIEND_RESP_SEND_OK",
    "ISI_EVENT_FRIEND_RESP_SEND_FAILED",
    "ISI_EVENT_CALL_FLASH_HOLD",
    "ISI_EVENT_CALL_FLASH_RESUME",
    "ISI_EVENT_PRES_CAPS_RECEIVED",

    "ISI_EVENT_MESSAGE_COMPOSING_ACTIVE",
    "ISI_EVENT_MESSAGE_COMPOSING_IDLE",

    "ISI_EVENT_FILE_REQUEST",
    "ISI_EVENT_FILE_ACCEPTED",
    "ISI_EVENT_FILE_REJECTED",
    "ISI_EVENT_FILE_PROGRESS",
    "ISI_EVENT_FILE_COMPLETED",
    "ISI_EVENT_FILE_FAILED",
    "ISI_EVENT_FILE_CANCELLED",
    "ISI_EVENT_FILE_ACKNOWLEDGED",
    "ISI_EVENT_FILE_TRYING",

    "ISI_EVENT_AKA_AUTH_REQUIRED",
    "ISI_EVENT_IPSEC_SETUP",
    "ISI_EVENT_IPSEC_RELEASE",
    "ISI_EVENT_USSD_REQUEST",
    "ISI_EVENT_USSD_DISCONNECT",
    "ISI_EVENT_USSD_SEND_OK",
    "ISI_EVENT_USSD_SEND_FAILED",
    "ISI_EVENT_CALL_HANDOFF_START",
    "ISI_EVENT_CALL_HANDOFF_SUCCESS",
    "ISI_EVENT_CALL_HANDOFF_FAILED",
    "ISI_EVENT_CALL_BEING_FORWARDED",
    "ISI_EVENT_RCS_PROVISIONING",
    "ISI_EVENT_CALL_ACCEPT_ACK",
    "ISI_EVENT_INVALID"
};

/* Look up table for all telephone events (a.k.a. tel events) */
static const char *_IUT_prtTelEvt[ISI_TEL_EVENT_INVALID + 1] = {
    "ISI_TEL_EVENT_DTMF",
    "ISI_TEL_EVENT_DTMF_OOB",
    "ISI_TEL_EVENT_VOICEMAIL",
    "ISI_TEL_EVENT_CALL_FORWARD",
    "ISI_TEL_EVENT_FEATURE",
    "ISI_TEL_EVENT_FLASHHOOK",
    "ISI_TEL_EVENT_RESERVED_1",
    "ISI_TEL_EVENT_RESERVED_2",
    "ISI_TEL_EVENT_RESERVED_3",
    "ISI_TEL_EVENT_RESERVED_4",
    "ISI_TEL_EVENT_RESERVED_5",
    "ISI_TEL_EVENT_INVALID"
};

/* 
 * ======== IUT_prtReturnString() ========
 *
 * This function maps an ISI return value into a string for printing
 * and diagnostic needs.
 *
 * Returns: 
 *   char* : A pointer to a NULL terminated string representing the
 *           value in the "r" parameter.
 *   NULL : "RETURN CODE UNKNOWN" - the value in "r" is unknown.
 */
char* IUT_prtReturnString(
    ISI_Return r)
{
    char *str_ptr = NULL;
    switch (r) {
    case ISI_RETURN_FAILED:
        str_ptr = "ISI_RETURN_FAILED";
        break;
    case ISI_RETURN_TIMEOUT:
        str_ptr = "ISI_RETURN_TIMEOUT";
        break;
    case ISI_RETURN_INVALID_CONF_ID:
        str_ptr = "ISI_RETURN_INVALID_CONF_ID";
        break;
    case ISI_RETURN_INVALID_CALL_ID:
        str_ptr = "ISI_RETURN_INVALID_CALL_ID";
        break;
    case ISI_RETURN_INVALID_TEL_EVENT_ID:
        str_ptr = "ISI_RETURN_INVALID_TEL_EVENT_ID";
        break;
    case ISI_RETURN_INVALID_SERVICE_ID:
        str_ptr = "ISI_RETURN_INVALID_SERVICE_ID";
        break;
    case ISI_RETURN_INVALID_PRESENCE_ID:
        str_ptr = "ISI_RETURN_INVALID_PRESENCE_ID";
        break;
    case ISI_RETURN_INVALID_MESSAGE_ID:
        str_ptr = "ISI_RETURN_INVALID_MESSAGE_ID";
        break;
    case ISI_RETURN_INVALID_COUNTRY:
        str_ptr = "ISI_RETURN_INVALID_COUNTRY";
        break;
    case ISI_RETURN_INVALID_PROTOCOL:
        str_ptr = "ISI_RETURN_INVALID_PROTOCOL";
        break;
    case ISI_RETURN_INVALID_CREDENTIALS:
        str_ptr = "ISI_RETURN_INVALID_CREDENTIALS";
        break;
    case ISI_RETURN_INVALID_SESSION_DIR:
        str_ptr = "ISI_RETURN_INVALID_SESSION_DIR";
        break;
    case ISI_RETURN_INVALID_SERVER_TYPE:
        str_ptr = "ISI_RETURN_INVALID_SERVER_TYPE";
        break;
    case ISI_RETURN_INVALID_ADDRESS:
        str_ptr = "ISI_RETURN_INVALID_ADDRESS";
        break;
    case ISI_RETURN_INVALID_TEL_EVENT:
        str_ptr = "ISI_RETURN_INVALID_TEL_EVENT";
        break;
    case ISI_RETURN_INVALID_CODER:
        str_ptr = "ISI_RETURN_INVALID_CODER";
        break;
    case ISI_RETURN_NOT_SUPPORTED:
        str_ptr = "ISI_RETURN_NOT_SUPPORTED";
        break;
    case ISI_RETURN_DONE:
        str_ptr = "ISI_RETURN_DONE";
        break;
    case ISI_RETURN_SERVICE_ALREADY_ACTIVE:
        str_ptr = "ISI_RETURN_SERVICE_ALREADY_ACTIVE";
        break;
    case ISI_RETURN_SERVICE_NOT_ACTIVE:
        str_ptr = "ISI_RETURN_SERVICE_NOT_ACTIVE";
        break;
    case ISI_RETURN_SERVICE_BUSY:
        str_ptr = "ISI_RETURN_SERVICE_BUSY";
        break;
    case ISI_RETURN_NOT_INIT:
        str_ptr = "ISI_RETURN_NOT_INIT";
        break;
    case ISI_RETURN_MUTEX_ERROR:
        str_ptr = "ISI_RETURN_MUTEX_ERROR";
        break;
    case ISI_RETURN_OK:
        str_ptr = "ISI_RETURN_OK";
        break;
    case ISI_RETURN_INVALID_TONE:
        str_ptr = "ISI_RETURN_INVALID_TONE";
        break;
    case ISI_RETURN_INVALID_CHAT_ID:
        str_ptr = "ISI_RETURN_INVALID_CHAT_ID";
        break;
    case ISI_RETURN_INVALID_FILE_ID:
        str_ptr = "ISI_RETURN_INVALID_FILE_ID";
        break;
    case ISI_RETURN_OK_4G_PLUS:
        str_ptr = "ISI_RETURN_OK_4G_PLUS";
        break;
    default:
        str_ptr = "RETURN CODE UNKNOWN";
        break;
    }
    return str_ptr;
};

/* 
 * ======== IUT_prtEvent() ========
 *
 * This function prints events as received by application from ISI_getEvent().
 *
 * Returns: 
 *   Nothing.
 *
 */
void IUT_prtEvent(
    ISI_Id     serviceId,
    ISI_Id     id,
    ISI_IdType idType,
    ISI_Event  event,
    char      *eventDesc_ptr)
{
    char buffer[IUT_STR_SIZE << 1];

    /* First print the id type */
    if (idType > ISI_ID_TYPE_INVALID) {
        idType = ISI_ID_TYPE_INVALID;
    }
    if (event > ISI_EVENT_INVALID) {
        event = ISI_EVENT_INVALID;
    }

    if (NULL == eventDesc_ptr || 0 == *eventDesc_ptr) {
        OSAL_snprintf(buffer, IUT_STR_SIZE << 1,
        "APP EVENT RECV FOR ServiceID:%d ID:%d ID Type:%s\n  The Event %d:%s\n",
        serviceId, id, _IUT_prtIdType[idType], event, _IUT_prtEvent[event]);
    }
    else {
        OSAL_snprintf(buffer, IUT_STR_SIZE << 1,
            "APP EVENT RECV FOR ServiceID:%d ID:%d ID Type:%s\n  The Event %d:%s (%s)\n",
            serviceId, id, _IUT_prtIdType[idType], event, _IUT_prtEvent[event], eventDesc_ptr);
    }
    OSAL_logMsg(buffer, 0);
    return;
}

/* 
 * ======== IUT_prtTelEvt() ========
 *
 * This function prints telephone events (a.k.a. "tel events") 
 * as received by applications from a call to ISI_getEvent().
 *
 * Returns: 
 *   Nothing.
 *
 */
void IUT_prtTelEvt(
    ISI_TelEvent event,
    char        *from_ptr,
    char        *dateTime_ptr,
    uint32       arg0,
    uint32       arg1)
{
    OSAL_logMsg("TELEVT: Evt:%s arg0:%d arg1:%d From:%s Date:%s\n",
            _IUT_prtTelEvt[event], arg0, arg1, from_ptr, dateTime_ptr);
}

/* 
 * ======== IUT_prtRoster() ========
 *
 * This function prints roster entries that reside in a
 * roster (a.k.a. contact) list.
 *
 * Returns: 
 *   Nothing.
 *
 */
void IUT_prtRoster(
    char *contact_ptr,
    char *group_ptr,
    char *name_ptr,
    char *substate_ptr)
{
    OSAL_logMsg("ROSTER: contact:%s group:%s name:%s state:%s\n", 
            contact_ptr, group_ptr, name_ptr, substate_ptr);

}

/* 
 * ======== IUT_prtIm() ========
 *
 * This function prints instant messages (a.k.a. "IM" or "text messages") 
 * that come from ISI.
 *
 * Returns: 
 *   Nothing.
 *
 */
void IUT_prtIm(
    char  *from_ptr,
    ISI_Id chatId,
    char  *subject_ptr,
    char  *message_ptr,
    char  *dateTime_ptr,
    int    report,
    char  *reportId_ptr)
{
    OSAL_logMsg("IM MSG: from:%s chatId:%d subject:%s date:%s\n%s\n",
            from_ptr, chatId, subject_ptr, dateTime_ptr, message_ptr);
    OSAL_logMsg("REQUESTING IM REPORT: report:%x reportId:%s\n",
            report, reportId_ptr);
}

void IUT_prtImReport(
    char  *from_ptr,
    ISI_Id chatId,
    char  *dateTime_ptr,
    int    report,
    char  *reportId_ptr)
{
    OSAL_logMsg("RECV'D IM REPORT: report:%x reportId:%s date:%s from:%s chatId:%d\n",
            report, reportId_ptr, dateTime_ptr, from_ptr, chatId);
}

void IUT_prtImComposing(
    ISI_Id chatId,
    vint   event)
{
    OSAL_logMsg("IM COMPOSING STATE: chatId:%d %s\n",
            chatId, _IUT_prtEvent[event]);
}

void IUT_prtFile(
    char            *from_ptr,
    ISI_Id           chatId,
    char            *filePath_ptr,
    ISI_FileType     fileType,
    vint             progress,
    char            *subject_ptr)
{
    OSAL_logMsg("FILE: from:%s chatId:%d path:%s type:%d progress:%d subject:%s\n",
            from_ptr, chatId, filePath_ptr, fileType, progress, subject_ptr);
}

void IUT_prtFileProgress(
    char            *from_ptr,
    ISI_Id           chatId,
    char            *filePath_ptr,
    ISI_FileType     fileType,
    ISI_FileAttribute fileAttr,
    vint             fileSize,
    vint             progress,
    char            *subject_ptr)
{
    OSAL_logMsg("FILE: from:%s chatId:%d path:%s type:%d attr:%d size:%d progress:%d subject:%s\n",
            from_ptr, chatId, filePath_ptr, fileType, fileAttr, fileSize, progress, subject_ptr);
}

void IUT_prtChatPresence(
    ISI_Id chatId,
    char  *roomName_ptr,
    char  *from_ptr,
    char  *subject_ptr,
    char  *presence_ptr)
{
    OSAL_logMsg("GROUP CHAT PRESENCE: chatId:%d room:%s from:%s subject:%s presence%s\n",
            chatId, roomName_ptr, from_ptr, subject_ptr, presence_ptr);
}

/* 
 * ======== IUT_prtRosterList() ========
 *
 * This function prints an entire roster list 
 * (a.k.a. contact list or buddy list)
 *
 * Returns: 
 *   Nothing.
 *
 */
void IUT_prtRosterList(
    IUT_Roster aRoster[])
{
    vint x;
    for (x = 0 ; x < IUT_MAX_NUM_ROSTERS ; x++) {
        if (aRoster[x].contact[0] != 0) {
            /* print it */
            OSAL_snprintf(_IUT_prtBuf, IUT_PRT_OUTPUT_BUFFER_MAX_SIZE,
                    "Entry Number: %d\n"
                    "Roster Entry: %s\n"
                    "JID: %s\n"
                    "Status: %s\n"
                    "State: %s\n"
                    "Group: %s\n"
                    "Name: %s\n\n\n",
                    x,
                    aRoster[x].contact,
                    aRoster[x].jid,
                    aRoster[x].statusDesc,
                    aRoster[x].state,
                    aRoster[x].group,
                    aRoster[x].name);
            OSAL_logMsg("%s",_IUT_prtBuf);
        }
    }
}

/* 
 * ======== IUT_prtCallerID() ========
 *
 * This function prints the caller ID info of a call.
 *
 * Returns: 
 *   Nothing.
 *
 */
void IUT_prtCallerId(
    char *from_ptr,
    char *subject_ptr)
{
    OSAL_logMsg("CALLER ID: from:%s subject:%s\n", 
            from_ptr, subject_ptr);

}

/* 
 * ======== IUT_prtCallModifyResult() ========
 *
 * This function prints the session type for call modify.
 *
 * Returns: 
 *   Nothing.
 *
 */
void IUT_prtCallModifyResult(
    IUT_HandSetObj *hs_ptr,
    ISI_Event       event, 
    vint            id)
{
    uint16               type;
    ISI_Return           r;

    r = ISI_getCallSessionType(id, &type);

    if (r != ISI_RETURN_OK) {
        OSAL_logMsg("Could not get session type ERROR:%s\n",
            IUT_prtReturnString(r));
        return;
    }

    OSAL_logMsg("APP EVENT RECV FOR CallID:%d The Event:%s, type:%d\n",
            id, _IUT_prtEvent[event], type);
}

/* 
 * ======== IUT_prtCallModify() ========
 *
 * This function prints the session type for call modify.
 *
 * Returns: 
 *   Nothing.
 *
 */
void IUT_prtCallModify(
    IUT_HandSetObj *hs_ptr,
    vint            id)
{
    ISI_SessionDirection audioDir;
    ISI_SessionDirection videoDir;
    uint16               type;
    ISI_Return           r;

    r = ISI_getCallSessionType(id, &type);

    if (r != ISI_RETURN_OK) {
        OSAL_logMsg("Could not get session type ERROR:%s\n",
            IUT_prtReturnString(r));
        return;
    }

    r = ISI_getCallSessionDirection(id, ISI_SESSION_TYPE_AUDIO, &audioDir);

    if (r != ISI_RETURN_OK) {
        OSAL_logMsg("Could not get session dir ERROR:%s\n",
            IUT_prtReturnString(r));
        return;
    }

    if (type & ISI_SESSION_TYPE_VIDEO) {
        r = ISI_getCallSessionDirection(id, ISI_SESSION_TYPE_VIDEO, &videoDir);

        if (r != ISI_RETURN_OK) {
            OSAL_logMsg("Could not get session dir ERROR:%s\n",
                IUT_prtReturnString(r));
            return;
        }
        OSAL_logMsg("Call is trying to modify to %s, audio dir:%d video dir:%d\n",
                "audio + video", audioDir, videoDir);
        return;
    }

    OSAL_logMsg("Call is trying to modify to %s, audio dir:%d\n",
            "audio only", audioDir);
}

void IUT_prtGroupChat(
    ISI_Id  chatId,
    char   *subject_ptr,
    char   *from_ptr)
{
    OSAL_logMsg("CHAT: chatId:%d subject:%s from:%s\n",
            chatId, subject_ptr, from_ptr);
}

void IUT_prtFeature(
    vint features)
{
    if (features & ISI_FEATURE_TYPE_VOLTE_CALL) {
        OSAL_logMsg("ISI_FEATURE_TYPE_VOLTE_CALL set.\n");
    }
    if (features & ISI_FEATURE_TYPE_VOLTE_SMS) {
        OSAL_logMsg("ISI_FEATURE_TYPE_VOLTE_SMS set.\n");
    }
    if (features & ISI_FEATURE_TYPE_RCS) {
        OSAL_logMsg("ISI_FEATURE_TYPE_VOLTE_RCS set.\n");
    }
    if (0 == features) {
        OSAL_logMsg("None feature set.\n");
    }
}


