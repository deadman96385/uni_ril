
#ifndef PARSEXML_PARSEPLMN_H
#define PARSEXML_PARSEPLMN_H
#include <pthread.h>
#include "mbim_message.h"
#include "mbim_device_basic_connect.h"

#ifdef __cplusplus
extern "C" {
#endif

void getApnUsingPLMN(char* plmn, MbimContext_2  *contextInfo);
int setApnUsingPLMN(char* mcc, char* mnc, MbimContext_2  *contextInfo);
int loadAPNs();

#ifdef __cplusplus
}
#endif

#endif
