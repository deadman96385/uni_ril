/*
 * THIS IS AN UNPUBLISHED WORK CONTAINING D2 TECHNOLOGIES, INC. CONFIDENTIAL
 * AND PROPRIETARY INFORMATION.  IF PUBLICATION OCCURS, THE FOLLOWING NOTICE
 * APPLIES: "COPYRIGHT 2012 D2 TECHNOLOGIES, INC. ALL RIGHTS RESERVED"
 *
 * $D2Tech$ $Revision: 25157 $ $Date: 2014-03-16 09:59:26 -0700 (Sun, 16 Mar 2014) $
 */
#include <osal.h>
#include "_fsm.h"

static const char _stateName[] = "Codec:encode";
/*
 * ======== _processEvent ========
 * Process the incoming event using the supplied state machine context.
 */
static void _processEvent(
    FSM_Context_Ptr context_ptr,
    VCE_Event       event,
    vint            codecType,
    char           *eventDesc_ptr)
{
    FSM_State_Ptr  nextState_ptr = (FSM_State_Ptr)&_FSM_STATE_ENCODE;

    switch (event) {
        case VCE_EVENT_STOP_ENC:
            nextState_ptr = (FSM_State_Ptr)&_FSM_STATE_IDLE;
            break;
        case VCE_EVENT_START_DEC:
            nextState_ptr = (FSM_State_Ptr)&_FSM_STATE_ENCODEDECODE;
            break;
        case VCE_EVENT_REMOTE_RECV_BW_KBPS:
            context_ptr->codec_ptr->enc.data.maxBitrate = atoi(eventDesc_ptr);
            codecModify(context_ptr->codec_ptr);
            break;
        case VCE_EVENT_SEND_KEY_FRAME:
            context_ptr->codec_ptr->enc.data.sendFIR = OSAL_TRUE;
            break;
        case VCE_EVENT_NONE:
        case VCE_EVENT_INIT_COMPLETE:
        case VCE_EVENT_START_ENC:
        case VCE_EVENT_STOP_DEC: 
        default:
            OSAL_logMsg("%s:%d Invalid event\n", __FUNCTION__, __LINE__);
            break;
    }
    _FSM_setState(context_ptr, nextState_ptr, event, codecType);
}

/*
 * ======== _stateEnter ========
 * Perform actions needed when entering this state.
 *
 * Return Values:
 * none
 */
static void _stateEnter(
    FSM_Context_Ptr context_ptr,
    VCE_Event       event,
    vint            codecType)
{
    switch (event) {
        case VCE_EVENT_START_ENC:
            codecInit(context_ptr->codec_ptr, CODEC_ENCODER, codecType);
            break;
        default:
            break;
    }
}

/*
 * ======== _stateExit ========
 * Perform actions needed when exiting this state.
 *
 * Return Values:
 * none
 */
static void _stateExit(
    FSM_Context_Ptr context_ptr,
    VCE_Event       event)
{
    switch (event) {
        case VCE_EVENT_STOP_ENC:
            codecRelease(context_ptr->codec_ptr, CODEC_ENCODER);
            break;
        default:
            break;
    }
}

/*
 * This constant holds pointers to functions that are used to process events
 * and handle state transitions.
 */
const FSM_State _FSM_STATE_ENCODE = {
    _processEvent,  /* Process events */
    _stateEnter,    /* Prepare to enter state */
    _stateExit,     /* Post process when exiting state */
    _stateName,     /* The name of this state */
};
