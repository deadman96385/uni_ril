/**
 * ril_utils.h --- utils functions declaration
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef RIL_UTILS_H_
#define RIL_UTILS_H_

void convertBinToHex(char *bin_ptr, int length, unsigned char *hex_ptr);
int convertHexToBin(const char *hex_ptr, int length, char *bin_ptr);
void convertUcs2ToUtf8(unsigned char *ucs2, int len, unsigned char *buf);
void convertGsm7ToUtf8(unsigned char *gsm7bits, int len, unsigned char *utf8);

#endif  // RIL_UTILS_H_
