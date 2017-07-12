/**
 * ril_utils.h --- utils functions declaration
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef RIL_UTILS_H_
#define RIL_UTILS_H_

#include <telephony/ril.h>

void convertBinToHex(char *bin_ptr, int length, unsigned char *hex_ptr);
int convertHexToBin(const char *hex_ptr, int length, char *bin_ptr);
void convertUcs2ToUtf8(unsigned char *ucs2, int len, unsigned char *buf);
void convertGsm7ToUtf8(unsigned char *gsm7bits, int len, unsigned char *utf8);
void convertUcsToUtf8(unsigned char *ucs2, int len, unsigned char *buf);
int utf8Package(unsigned char *utf8, int offset, int v);
int emNStrlen(char *str);
void getProperty(RIL_SOCKET_ID socket_id, const char *property, char *value,
                 const char *defaultVal);
void setProperty(RIL_SOCKET_ID socket_id, const char *property,
                 const char *value);

#endif  // RIL_UTILS_H_
