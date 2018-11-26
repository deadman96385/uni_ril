/**
 * mbim_utils.c --- utils functions implementation
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#include <string.h>
#include <stdbool.h>
#include <utils/Log.h>

#include "mbim_utils.h"

const unsigned short gsm7ToUnicodeTable[128] = {
  '@', 0xa3,  '$', 0xa5, 0xe8, 0xe9, 0xf9, 0xec, 0xf2, 0xc7, '\n', 0xd8, 0xf8, '\r', 0xc5, 0xe5,
0x394,  '_',0x3a6,0x393,0x39b,0x3a9,0x3a0,0x3a8,0x3a3,0x398,0x39e,    0, 0xc6, 0xe6, 0xdf, 0xc9,
  ' ',  '!',  '"',  '#', 0xa4,  '%',  '&', '\'',  '(',  ')',  '*',  '+',  ',',  '-',  '.',  '/',
  '0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  ':',  ';',  '<',  '=',  '>',  '?',
 0xa1,  'A',  'B',  'C',  'D',  'E',  'F',  'G',  'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',
  'P',  'Q',  'R',  'S',  'T',  'U',  'V',  'W',  'X',  'Y',  'Z', 0xc4, 0xd6,0x147, 0xdc, 0xa7,
 0xbf,  'a',  'b',  'c',  'd',  'e',  'f',  'g',  'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
  'p',  'q',  'r',  's',  't',  'u',  'v',  'w',  'x',  'y',  'z', 0xe4, 0xf6, 0xf1, 0xfc, 0xe0,
};

const unsigned short gsm7ExtendToUnicodeTable[128] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,'\f',   0,   0,   0,   0,   0,
    0,   0,   0,   0, '^',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0, '{', '}',   0,   0,   0,   0,   0,'\\',
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, '[', '~', ']',   0,
  '|',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,0x20ac, 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
};

void
convertBinToHex(char *          bin_ptr,
                int             length,
                unsigned char * hex_ptr) {
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

int
convertHexToBin(const char *    hex_ptr,
                int             length,
                char *          bin_ptr) {
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

int
utf8Package(unsigned char * utf8,
            int             offset,
            int             v) {
    int result = 0;

    if (v < 128) {
        result = 1;
        if (utf8) {
            utf8[offset] = (unsigned char)v;
        }
    } else if (v < 0x800) {
        result = 2;
        if (utf8) {
            utf8[offset + 0] = (unsigned char)(0xc0 | (v >> 6));
            utf8[offset + 1] = (unsigned char)(0x80 | (v & 0x3f));
        }
    } else if (v < 0x10000) {
        result = 3;
        if (utf8) {
            utf8[offset + 0] = (unsigned char)(0xe0 | (v >> 12));
            utf8[offset + 1] = (unsigned char)(0x80 | ((v >> 6) & 0x3f));
            utf8[offset + 2] = (unsigned char)(0x80 | (v & 0x3f));
        }
    } else {
        result = 4;
        if (utf8) {
            utf8[offset + 0] = (unsigned char)(0xf0 | ((v >> 18) & 0x7));
            utf8[offset + 1] = (unsigned char)(0x80 | ((v >> 12) & 0x3f));
            utf8[offset + 2] = (unsigned char)(0x80 | ((v >> 6) & 0x3f));
            utf8[offset + 3] = (unsigned char)(0x80 | (v & 0x3f));
        }
    }
    return result;
}

int
convertUcs2ToUtf8(const unsigned char * ucs2,
                  int                   len,
                  unsigned char *       buf) {
    int n;
    int result = 0;

    for (n = 0; n < len; ucs2 += 2, n++) {
        int c = (ucs2[0] << 8) | ucs2[1];
        result += utf8Package(buf, result, c);
    }
    return result;
}

void
convertGsm7ToUtf8(unsigned char *   gsm7bits,
                  int               len,
                  unsigned char *   utf8) {
    int escaped = 0;
    int result = 0;

    for (; len > 0; len--) {
        int c = gsm7bits[0] & 0x7f;
        int v;

        if (escaped) {
            v = gsm7ExtendToUnicodeTable[c];
            escaped = 0;
        } else if (c == 0x1b) { // GSM 7bit escape
            escaped = 1;
            goto Next;
        } else {
            v = gsm7ToUnicodeTable[c];
        }
        result += utf8Package(utf8, result, v);
Next:
        gsm7bits += 1;
    }
}

int
convertGsm8ToUtf8(const unsigned char *     src,
                  int                       count,
                  unsigned char *           utf8) {
    int result = 0;
    int escaped = 0;

    for ( ; count > 0; count--) {
        int  c = *src++;

        if (c == 0xff)
            break;

        if (c == 0x1b) {  // GSM_7BITS_ESCAPE
            if (escaped) { /* two escape characters => one space */
                c = 0x20;
                escaped = 0;
            } else {
                escaped = 1;
                continue;
            }
        } else {
            if (c >= 0x80) {
                c = 0x20;
                escaped = 0;
            } else if (escaped) {
                c = gsm7ExtendToUnicodeTable[c];
            } else {
                c = gsm7ToUnicodeTable[c];
            }
        }

        result += utf8Package(utf8, result, c);
    }
    return  result;
}

int
hexCharToInt(char c) {
    if (c >= '0' && c <= '9') return (c - '0');
    if (c >= 'A' && c <= 'F') return (c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return (c - 'a' + 10);

    RLOGE("invalid hex char %c", c);

    return -1;
}

void
hexStringToBytes(char *             str,
                 unsigned char *    dst) {

    if (str == NULL || dst == NULL) return;

    int len = strlen(str);

    for (int i = 0 ; i < len; i += 2) {
        dst[i / 2] = ((hexCharToInt(str[i]) << 4) | hexCharToInt(str[i + 1]));
    }
}

/**
 * Converts a byte array into a String of hexadecimal characters.
 *
 * @param bytes an array of bytes
 *
 * @return hex string representation of bytes array
 */
void
bytesToHexString(unsigned char *bytes, int bytesLength, char *dst) {
    if (bytes == NULL || dst == NULL) {
        RLOGE("Invalid params");
        return;
    }

    const char *str = "0123456789abcdef";
    for (int i = 0; i < bytesLength; i++) {
        int b;
        RLOGD("bytes[%d] = %02x", i, bytes[i]);

        b = 0x0f & (bytes[i] >> 4);
        dst[i * 2] = str[b];
        RLOGD("b = %d,  dst[%d] = %c", b, i * 2, dst[i * 2]);

        b = 0x0f & bytes[i];
        dst[i * 2 + 1] = str[b];
        RLOGD("b = %d,  dst[%d] = %c", b, i * 2 + 1, dst[i * 2 + 1]);
    }
}

/*
 * Decodes a string field that's formatted like the EF[ADN] alpha
 * identifier
 *
 * From TS 51.011 10.5.1:
 *   Coding:
 *       this alpha tagging shall use either
 *      -    the SMS default 7 bit coded alphabet as defined in
 *          TS 23.038 [12] with bit 8 set to 0. The alpha identifier
 *          shall be left justified. Unused bytes shall be set to 'FF'; or
 *      -    one of the UCS2 coded options as defined in annex B.
 *
 * Annex B from TS 11.11 V8.13.0:
 *      1)  If the first octet in the alpha string is '80', then the
 *          remaining octets are 16 bit UCS2 characters ...
 *      2)  if the first octet in the alpha string is '81', then the
 *          second octet contains a value indicating the number of
 *          characters in the string, and the third octet contains an
 *          8 bit number which defines bits 15 to 8 of a 16 bit
 *          base pointer, where bit 16 is set to zero and bits 7 to 1
 *          are also set to zero.  These sixteen bits constitute a
 *          base pointer to a "half page" in the UCS2 code space, to be
 *          used with some or all of the remaining octets in the string.
 *          The fourth and subsequent octets contain codings as follows:
 *          If bit 8 of the octet is set to zero, the remaining 7 bits
 *          of the octet contain a GSM Default Alphabet character,
 *          whereas if bit 8 of the octet is set to one, then the
 *          remaining seven bits are an offset value added to the
 *          16 bit base pointer defined earlier...
 *      3)  If the first octet of the alpha string is set to '82', then
 *          the second octet contains a value indicating the number of
 *          characters in the string, and the third and fourth octets
 *          contain a 16 bit number which defines the complete 16 bit
 *          base pointer to a "half page" in the UCS2 code space...
 */
/* see 10.5.1 of 3GPP 51.011 */

int
adnStringFieldToString(const unsigned char *    data,
                       const unsigned char *    end,
                       unsigned char *          dst) {
    int  result = 0;

    /* ignore trailing 0xff */
    while (data < end && end[-1] == 0xff)
        end--;

    if (data >= end)
        return 0;

    if (data[0] == 0x80) {  /* UCS/2 source encoding */
        data += 1;
        result = convertUcs2ToUtf8(data, (end - data) / 2, dst);
    } else {
        int isucs2 = 0;
        int len = 0, base = 0;

        if (data + 3 <= end && data[0] == 0x81) {
            isucs2 = 1;
            len = data[1];
            base = data[2] << 7;
            data += 3;
            if (len > end - data)
                len = end - data;
        } else if (data + 4 <= end && data[0] == 0x82) {
            isucs2 = 1;
            len = data[1];
            base  = (data[2] << 8) | data[3];
            data += 4;
            if (len > end - data)
                len = end - data;
        }

        if (isucs2) {
            end = data + len;
            while (data < end) {
                int c = data[0];
                if (c >= 0x80) {
                    result += utf8Package(dst, result, base + (c & 0x7f));
                    data  += 1;
                } else {
                    /* GSM character set */
                    int count;
                    for (count = 0; data + count < end && data[count] < 128; count++)
                        ;

                    result += convertGsm8ToUtf8(data, count, (dst ? dst + result : NULL));
                    data  += count;
                }
            }
        }
        else {
            result = convertGsm8ToUtf8(data, end - data, dst);
        }
    }
    return result;
}

int
gsm_bcdnum_to_ascii(const unsigned char *   bcd,
                    int                     count,
                    unsigned char *         dst ) {
    int result = 0;
    int shift = 0;

    while (count > 0) {
        int  c = (bcd[0] >> shift) & 0xf;

       if (c == 0xf && count == 1)  /* ignore trailing 0xf */
            break;

        if (c >= 14)
            c = 0;

        if (dst) dst[result] = "0123456789*#,N"[c];
        result += 1;

        count--;
        shift += 4;
        if (shift == 8) {
            shift = 0;
            bcd += 1;
        }
    }
    return  result;
}

int
parseSimAdnRecord(SimAdnRecord          rec,
                  const unsigned char * data,
                  int                   len) {
    const unsigned char *end    = data + len;
    const unsigned char *footer = end - ADN_FOOTER_SIZE;
    int num_len;

    rec->adn.alpha[0]  = 0;
    rec->adn.number[0] = 0;
    rec->ext_record    = 0xff;

    if (len < ADN_FOOTER_SIZE)
        return -1;

    /* alpha is optional */
    if (len > ADN_FOOTER_SIZE) {
        const unsigned char *  dataend = data + len - ADN_FOOTER_SIZE;
        int       count   = adnStringFieldToString(data, dataend, NULL);
        int alphaLen = sizeof(rec->adn.alpha)-1;
        if (count > alphaLen)  /* too long */
            return -1;

        adnStringFieldToString(data, dataend, rec->adn.alpha);
        rec->adn.alpha[count] = 0;
    }

    num_len = footer[ADN_OFFSET_NUMBER_LENGTH];
    if (num_len > 11)
        return -1;

    /* decode TON and number to ASCII, NOTE: this is lossy !! */
    {
        int      ton    = footer[ADN_OFFSET_TON_NPI];
        unsigned char *  number = (unsigned char *) rec->adn.number;
        int      len    = sizeof(rec->adn.number)-1;
        int      count;

        if (ton != 0x81 && ton != 0x91)
            return -1;

        if (ton == 0x91) {
            *number++ = '+';
            len      -= 1;
        }

        count = gsm_bcdnum_to_ascii(footer + ADN_OFFSET_NUMBER_START,
                                    (num_len - 1) * 2, number);
        number[count] = 0;
    }
    return 0;
}

void
spdiPlmnBcdToString(uint8_t *data, int offset, char *str) {
    int v;

    v = data[offset] & 0xf;
    if (v > 9) return;
    str[0] = (char) ('0' + v);

    v = (data[offset] >> 4) & 0xf;
    if (v > 9) return;
    str[1] = (char) ('0' + v);

    v = data[offset + 1] & 0xf;
    if (v > 9) return;
    str[2] = (char) ('0' + v);

    v = data[offset + 2] & 0xf;
    if (v > 9) return;
    str[3] = (char) ('0' + v);

    v = (data[offset + 2] >> 4) & 0xf;
    if (v > 9) return;
    str[4] = (char) ('0' + v);

    v = (data[offset + 1] >> 4) & 0xf;
    if (v > 9) return;
    str[5] = (char) ('0' + v);
}

