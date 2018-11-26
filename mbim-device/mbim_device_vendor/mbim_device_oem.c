#define LOG_TAG "MBIM-Device"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <utils/Log.h>
#include <cutils/properties.h>

#include "mbim_device_oem.h"
#include "mbim_device_vendor.h"
#include "mbim_message_processer.h"
#include "at_channel.h"
#include "at_tok.h"

void
mbim_device_oem(Mbim_Token                token,
                uint32_t                  cid,
                MbimMessageCommandType    commandType,
                void *                    data,
                size_t                    dataLen) {
    switch (cid) {
        case MBIM_CID_OEM_ATCI:
            basic_connect_oem_atci(token, commandType, data, dataLen);
            break;
        default:
            RLOGD("unsupproted oem cid");
            mbim_device_command_done(token,
                    MBIM_STATUS_ERROR_NO_DEVICE_SUPPORT, NULL, 0);
            break;
    }
}
/*****************************************************************************/
/**
 * basic_connect_oem_atci:
 *
 * query:
 *  @command:   the data shall be null
 *  @response:  the data shall be
 */

void
basic_connect_oem_atci(Mbim_Token                token,
                       MbimMessageCommandType    commandType,
                       void *                    data,
                       size_t                    dataLen) {
    switch (commandType) {
        case MBIM_MESSAGE_COMMAND_TYPE_SET:
            send_at_command(token, data, dataLen);
            break;
        default:
            RLOGE("unsupported commandType");
            break;
    }
}

void
send_at_command(Mbim_Token            token,
                void *                data,
                size_t                dataLen) {
    char buf[MAX_AT_RESPONSE] = {0};
    char prop[PROPERTY_VALUE_MAX] = {0};
    char *ATCmd = (char *)data;
    if (ATCmd == NULL || dataLen == 0) {
        RLOGE("Invalid AT command");
        goto error;
    }

    if (strcmp(ATCmd, "AT+SPSERIALOPT=0") == 0) {
        RLOGD("setprop sys.usb.config mbim,gser");
        RLOGD("setprop sys.usb.config.ready 0");

        strlcat(buf, "OK", sizeof(buf));
        strlcat(buf, "\r\n", sizeof(buf));
        mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE, buf, strlen(buf));

        sleep(1);
        property_set("sys.usb.config", "mbim,gser");
        property_set("sys.usb.config.ready", "0");
    } if (strcmp(ATCmd, "AT+SPSERIALOPT=1") == 0) {
        RLOGD("setprop sys.usb.config mbim,gser,adb");
        RLOGD("setprop sys.usb.config.ready 0");

        strlcat(buf, "OK", sizeof(buf));
        strlcat(buf, "\r\n", sizeof(buf));
        mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE, buf, strlen(buf));

        sleep(1);
        property_set("sys.usb.config", "mbim,gser,adb");
        property_set("sys.usb.config.ready", "0");
    } if (strcmp(ATCmd, "AT+GNSS?") == 0) {
	char gps_state[PROPERTY_VALUE_MAX] = {0};
	property_get("persist.gps.state", gps_state, "off");
	if (!strncmp(gps_state, "on", 2)){
		strlcat(buf, "OK", sizeof(buf));
		strlcat(buf, "\r\n", sizeof(buf));
	}
	else{
		strlcat(buf, "+CME ERROR:not ready", sizeof(buf));
		strlcat(buf, "\r\n", sizeof(buf));
	}
	strlcat(buf, gps_state, sizeof(buf));
	strlcat(buf, "\r\n", sizeof(buf));
	mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE, buf, strlen(buf));
    } if (strcmp(ATCmd, "AT+GE2ASSERT=1") == 0) {
	property_set("persist.gps.ge2assert",  "true");
	strlcat(buf, "OK", sizeof(buf));
	strlcat(buf, "\r\n", sizeof(buf));
	mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE, buf, strlen(buf));
 } if (strcmp(ATCmd, "AT+DEBUGENABLE=1") == 0) {
	char state[PROPERTY_VALUE_MAX] = {0};
	
	property_get("persist.ylog.enabled", state, "0");
	if (!strncmp(state, "0", 1)){
		property_set("persist.ylog.enabled",  "1");
	}
	strlcat(buf, "OK", sizeof(buf));

	property_get("persist.sys.engpc.disable", state, "0");
	if (!strncmp(state, "1", 1)){
		property_set("persist.sys.engpc.disable",  "0");
	}
	strlcat(buf, "OK", sizeof(buf));

	
	strlcat(buf, "\r\n", sizeof(buf));
	mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE, buf, strlen(buf));
    } if (strcmp(ATCmd, "AT+DEBUGENABLE=0") == 0) {
	char state[PROPERTY_VALUE_MAX] = {0};
	
	property_get("persist.ylog.enabled", state, "0");
	if (!strncmp(state, "1", 1)){
		property_set("persist.ylog.enabled",  "0");
	}
	strlcat(buf, "OK", sizeof(buf));

	property_get("persist.sys.engpc.disable", state, "0");
	if (!strncmp(state, "0", 1)){
		property_set("persist.sys.engpc.disable",  "1");
	}
	strlcat(buf, "OK", sizeof(buf));
	
	strlcat(buf, "\r\n", sizeof(buf));
	mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE, buf, strlen(buf));
    } if (strcmp(ATCmd, "AT+DEBUGENABLE?") == 0) {
	char state[PROPERTY_VALUE_MAX] = {0};
	
	property_get("persist.ylog.enabled", state, "0");
	strlcat(buf, "ylog=", sizeof(buf));
	strlcat(buf, state, sizeof(buf));
	strlcat(buf, "\r\n", sizeof(buf));
	
	property_get("persist.sys.engpc.disable", state, "0");
	strlcat(buf, "engpc=", sizeof(buf));
	strlcat(buf, state, sizeof(buf));
	strlcat(buf, "\r\n", sizeof(buf));
	
	mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE, buf, strlen(buf));
    } else {
        RLOGD("enter here");
        int i, err;
        ATLine *p_cur = NULL;
        ATResponse *p_response = NULL;
        int channleID = getChannel(RIL_SOCKET_1);

        err = at_send_command_multiline(s_ATChannels[channleID], ATCmd, "",
                                        &p_response);
        if (err < 0 || p_response->success == 0) {
            if (p_response != NULL) {
                strlcat(buf, p_response->finalResponse, sizeof(buf));
                strlcat(buf, "\r\n", sizeof(buf));
                mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE, buf, strlen(buf));
            } else {
                mbim_device_command_done(token, MBIM_STATUS_ERROR_FAILURE, NULL, 0);
            }
        } else {
            p_cur = p_response->p_intermediates;
            for (i = 0; p_cur != NULL; p_cur = p_cur->p_next, i++) {
                strlcat(buf, p_cur->line, sizeof(buf));
                strlcat(buf, "\r\n", sizeof(buf));
            }
            strlcat(buf, p_response->finalResponse, sizeof(buf));
            strlcat(buf, "\r\n", sizeof(buf));

            mbim_device_command_done(token, MBIM_STATUS_ERROR_NONE, buf, strlen(buf));
        }

        at_response_free(p_response);
        putChannel(channleID);
    }

    return;

error:
    mbim_device_command_done(token, MBIM_STATUS_ERROR_FAILURE, NULL, 0);
}
