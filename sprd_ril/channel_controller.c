#define LOG_TAG "RIL"

#include <telephony/sprd_ril.h>
#include <sys/socket.h>
#include <cutils/sockets.h>
#include "channel_controller.h"

#define LOG_TAG "RIL"
#include <utils/Log.h>

int s_ATTableSize = 0;
int s_modemdFd = -1;

const cmd_table s_ATTimeoutTable[] = {
        {AT_CMD_STR("AT+CREG"), 15},
        {AT_CMD_STR("AT+CGREG"), 15},
        {AT_CMD_STR("AT+CEREG"), 15},
        {AT_CMD_STR("AT+CSQ=?"), 5},
        {AT_CMD_STR("AT+CSQ"), 5},
        {AT_CMD_STR("AT+CESQ"), 5},
        {AT_CMD_STR("AT+COPS=0"), 180},
        {AT_CMD_STR("AT+COPS=3"), 10},
        {AT_CMD_STR("AT+COPS?"), 10},
        {AT_CMD_STR("AT+CPIN?"), 4},
        {AT_CMD_STR("AT+CPIN="), 50},
        {AT_CMD_STR("AT+CPWD"), 60},
        {AT_CMD_STR("AT+ECPIN2"), 60},
        {AT_CMD_STR("AT+EUICC"), 60},
        {AT_CMD_STR("AT+CMGS=?"), 5},
        {AT_CMD_STR("AT+CMGS="), 138},
        {AT_CMD_STR("AT+SNVM=1"), 10},
        {AT_CMD_STR("AT+CMGW=?"), 5},
        {AT_CMD_STR("AT+CMGW="), 10},
        {AT_CMD_STR("ATD"), 40},
        {AT_CMD_STR("ATA"), 40},
        {AT_CMD_STR("ATH"), 40},
        {AT_CMD_STR("AT+CLCC"), 5},
        {AT_CMD_STR("AT+SFUN=2"),300},
        {AT_CMD_STR("AT+SFUN=4"), 180},
        {AT_CMD_STR("AT+CTZR"), 5},
        {AT_CMD_STR("AT+CGSN"), 5},
        {AT_CMD_STR("AT+CIMI"), 5},
        {AT_CMD_STR("AT+CGMR"), 5},
        {AT_CMD_STR("AT+CGDCONT?"), 5},
        {AT_CMD_STR("AT+CGDCONT="), 5},
        {AT_CMD_STR("AT+CGDATA="), 120},
        {AT_CMD_STR("AT+CGACT?"), 5},
        {AT_CMD_STR("AT+CGACT=1"), 600},
        {AT_CMD_STR("AT+CGACT=0"), 50},
        {AT_CMD_STR("AT+COPS=?"), 210},
        {AT_CMD_STR("AT+CCWA?"), 30},
        {AT_CMD_STR("AT+CCWA=?"), 5},
        {AT_CMD_STR("AT+CCWA="), 50},
        {AT_CMD_STR("AT+COLP?"), 30},
        {AT_CMD_STR("AT+COLP=?"), 5},
        {AT_CMD_STR("AT+COLP="), 50},
        {AT_CMD_STR("AT+EVTS="), 10},
        {AT_CMD_STR("AT+CRSM"), 60},
        {AT_CMD_STR("AT+CMOD"), 5},
        {AT_CMD_STR("AT+CLIP?"), 30},
        {AT_CMD_STR("AT+CLIP=?"), 5},
        {AT_CMD_STR("AT+CLIP="), 50},
        {AT_CMD_STR("AT+CSSN"), 5},
        {AT_CMD_STR("AT+CUSD?"), 5},
        {AT_CMD_STR("AT+CUSD=?"), 5},
        {AT_CMD_STR("AT+CUSD="), 50},
        {AT_CMD_STR("AT+CMGD"), 5},
        {AT_CMD_STR("AT+CSCA"), 5},
        {AT_CMD_STR("AT+CNMA"),  5},
        {AT_CMD_STR("AT+CNMI"), 5},
        {AT_CMD_STR("AT+CMMS"), 5},
        {AT_CMD_STR("AT+VTS"), 30},
        {AT_CMD_STR("AT+SDTMF"), 30},
        {AT_CMD_STR("AT+VTD"), 5},
        {AT_CMD_STR("AT+CGEQREQ"), 5},
        {AT_CMD_STR("AT+CGEREP"), 5},
        {AT_CMD_STR("AT+COPS=1"), 90},
        {AT_CMD_STR("AT"), 50},  // default 50s timeout
    };

void detectATNoResponse() {
    int retryTimes = 0;
    const char socketName[ARRAY_SIZE] = "modemd";

    s_ATTableSize = NUM_ELEMS(s_ATTimeoutTable);

    s_modemdFd = socket_local_client(socketName,
            ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
    while (s_modemdFd < 0 && retryTimes < 3) {
        retryTimes++;
        sleep(1);
        s_modemdFd = socket_local_client(socketName,
                ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
    }
    if (s_modemdFd <= 0) {
        RILLOGE("Failed to connect to socket %s", socketName);
        return;
    }
    RILLOGD("connect to modemd socket success!");
}

/**
 * Returns 1 if found, 0 otherwise. needle must be null-terminated.
 * strstr might not work because WebBox sends garbage before the first OKread
 */
int findInBuf(char *buf, int len, char *needle) {
    int i;
    int needleMatchedPos = 0;

    if (buf == NULL) {
        return 0;
    }

    if (needle[0] == '\0') {
        return 1;
    }
    for (i = 0; i < len; i++) {
        if (needle[needleMatchedPos] == buf[i]) {
            needleMatchedPos++;
            if (needle[needleMatchedPos] == '\0') {
                // Entire needle was found
                return 1;
            }
        } else {
            needleMatchedPos = 0;
        }
    }
    return 0;
}

int at_tok_flag_start(char **p_cur, char start_flag) {
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

/* add an intermediate response to sp_response */
void reWriteIntermediate(ATResponse *sp_response, char *newLine) {
    ATLine *p_new;

    p_new = (ATLine *)malloc(sizeof(ATLine));

    p_new->line = strdup(newLine);

    /* note: this adds to the head of the list, so the list
       will be in reverse order of lines received. the order is flipped
       again before passing on to the command issuer */
    p_new->p_next = sp_response->p_intermediates;
    sp_response->p_intermediates = p_new;
}

/**
 * The line reader places the intermediate responses in reverse order
 * here we flip them back
 */
void reverseNewIntermediates(ATResponse *sp_response) {
    ATLine *pcur, *pnext;

    pcur = sp_response->p_intermediates;
    sp_response->p_intermediates = NULL;

    while (pcur != NULL) {
        pnext = pcur->p_next;
        pcur->p_next = sp_response->p_intermediates;
        sp_response->p_intermediates = pcur;
        pcur = pnext;
    }
}

int getATResponseType(char *str) {
    int rspType = AT_RSP_TYPE_MID;
    if (strStartsWith(str, "CONNECT")) {
        rspType = AT_RSP_TYPE_CONNECT;
    } else if (isFinalResponseError(str)) {
        rspType = AT_RSP_TYPE_ERROR;
    } else if (isFinalResponseSuccess(str)) {
        rspType = AT_RSP_TYPE_OK;
    }
    return rspType;
}

