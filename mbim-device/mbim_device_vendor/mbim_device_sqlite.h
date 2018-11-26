#ifndef MBIM_DEVICE_SQLITE_H
#define MBIM_DEVICE_SQLITE_H

#include "mbim_enums.h"

#define MBIM_SQL_LEN 1024

#define MBIM_SMS_TABLE "sms"

#define MBIM_SQL_ERR 0x7FFFFFFF

#define MBIM_DB "/productinfo/mbim.db"


int mbim_sqlite_create(void);
int mbim_sql_insert();


typedef enum {
    SMS_INSERT,
    SMS_QUERY,
    SMS_DELETE,
    SMS_LAST_INSERT_ROWID
} SqlQueryType;


typedef struct mbim_sms_t {
   int id;
   MbimSmsMessageStatus status;
   char *pdu;
   int pdu_size;
} mbim_read_sms;

typedef struct mbim_sms_sqlresult_t {
   int count;
   mbim_read_sms *table;
} mbim_sms_sqlresult;

#endif  // MBIM_DEVICE_SQLITE_H
