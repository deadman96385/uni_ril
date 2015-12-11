/**
 * ril_utils.c --- utils functions implementation
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#include "ril_utils.h"
#include <string.h>

void convertBinToHex(char *bin_ptr, int length, unsigned char *hex_ptr) {
    int i;
    unsigned char tmp;

    if (bin_ptr == NULL || hex_ptr == NULL) {
        return;
    }
    for (i = 0; i < length; i++) {
        tmp = (unsigned char)((bin_ptr[i] & 0xf0) >> 4);
        if (tmp <= 9) {
            *hex_ptr = (unsigned char)(tmp + '0');
        } else {
            *hex_ptr = (unsigned char)(tmp + 'A' - 10);
        }
        hex_ptr++;
        tmp = (unsigned char)(bin_ptr[i] & 0x0f);
        if (tmp <= 9) {
            *hex_ptr = (unsigned char)(tmp + '0');
        } else {
            *hex_ptr = (unsigned char)(tmp + 'A' - 10);
        }
        hex_ptr++;
    }
}

int convertHexToBin(const char *hex_ptr, int length, char *bin_ptr) {
    char *dest_ptr = bin_ptr;
    int i;
    char ch;

    if (hex_ptr == NULL || bin_ptr == NULL) {
        return -1;
    }

    for (i = 0; i < length; i += 2) {
        ch = hex_ptr[i];
        if (ch >= '0' && ch <= '9') {
            *dest_ptr = (char)((ch - '0') << 4);
        } else if (ch >= 'a' && ch <= 'f') {
            *dest_ptr = (char)((ch - 'a' + 10) << 4);
        } else if (ch >= 'A' && ch <= 'F') {
            *dest_ptr = (char)((ch - 'A' + 10) << 4);
        } else {
            return -1;
        }

        ch = hex_ptr[i + 1];
        if (ch >= '0' && ch <= '9') {
            *dest_ptr |= (char)(ch - '0');
        } else if (ch >= 'a' && ch <= 'f') {
            *dest_ptr |= (char)(ch - 'a' + 10);
        } else if (ch >= 'A' && ch <= 'F') {
            *dest_ptr |= (char)(ch - 'A' + 10);
        } else {
            return -1;
        }

        dest_ptr++;
    }
    return 0;
}
