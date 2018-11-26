/**
 * mbim_utils.h --- utils functions declaration
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 */

#ifndef MBIM_UTILS_H_
#define MBIM_UTILS_H_


/** ADN: Abbreviated Dialing Numbers
 **/
#define SIM_ADN_MAX_ALPHA           20  /* maximum number of characters in ADN alpha tag */
#define SIM_ADN_MAX_NUMBER          20  /* maximum digits in ADN number */

#define ADN_FOOTER_SIZE             14
#define ADN_OFFSET_NUMBER_LENGTH    0
#define ADN_OFFSET_TON_NPI          1
#define ADN_OFFSET_NUMBER_START     2
#define ADN_OFFSET_NUMBER_END       11
#define ADN_OFFSET_CAPABILITY_ID    12
#define ADN_OFFSET_EXTENSION_ID     13

typedef struct {
    unsigned char   alpha[SIM_ADN_MAX_ALPHA * 3 + 1];   /* alpha tag in zero-terminated utf-8      */
    char            number[SIM_ADN_MAX_NUMBER + 1];     /* dialing number in zero-terminated ASCII */
}
SimAdnRec, *SimAdn;

typedef struct {
    SimAdnRec       adn;
    unsigned char   ext_record;  /* 0 or 0xFF means no extension */
}
SimAdnRecordRec, *SimAdnRecord;


int
convertHexToBin         (const char *           hex_ptr,
                         int                    length,
                         char *                 bin_ptr);

void
convertBinToHex         (char *                 bin_ptr,
                         int                    length,
                         unsigned char *        hex_ptr);

int
convertUcs2ToUtf8       (const unsigned char *  ucs2,
                         int                    len,
                         unsigned char *        buf);

void
convertGsm7ToUtf8       (unsigned char *        gsm7bits,
                         int                    len,
                         unsigned char *        utf8);

int
adnStringFieldToString  (const unsigned char *  data,
                         const unsigned char *  end,
                         unsigned char *        dst);

int
hexCharToInt            (char                   c);

void
hexStringToBytes        (char *                 str,
                         unsigned char *        dst);

int
parseSimAdnRecord       (SimAdnRecord           rec,
                         const unsigned char *  data,
                         int                    len);

void
spdiPlmnBcdToString     (uint8_t *              data,
                         int                    offset,
                         char *                 str);

#endif  // MBIM_UTILS_H_
