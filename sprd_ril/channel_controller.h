
#ifndef CHANNEL_CONTROLLER_H_
#define CHANNEL_CONTROLLER_H_

#include <telephony/sprd_ril.h>
#include "ril_data.h"

#define BLOCKED_MAX_COUNT       5

#define AT_RESULT_OK            0
#define AT_RESULT_NG            -1

#define AT_RSP_TYPE_OK          0
#define AT_RSP_TYPE_MID         1
#define AT_RSP_TYPE_ERROR       2
#define AT_RSP_TYPE_CONNECT     3

#define NUM_ELEMS(x) (sizeof(x)/sizeof(x[0]))

typedef struct cmd_table {
    const char *cmd;
    int len;
    int timeout;  // timeout for response
} cmd_table;

extern int s_modemdFd;
extern int s_ATTableSize;
extern const cmd_table s_ATTimeoutTable[];

void detectATNoResponse();
void *signal_process();

void reWriteIntermediate(ATResponse *sp_response, char *newLine);
void reverseNewIntermediates(ATResponse *sp_response);
int getATResponseType(char *str);
int at_tok_flag_start(char **p_cur, char start_flag);
int findInBuf(char *buf, int len, char *needle);

#endif  // CHANNEL_CONTROLLER_H_

