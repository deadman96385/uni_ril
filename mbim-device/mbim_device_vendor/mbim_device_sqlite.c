#include "sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "mbim_device_sqlite.h"
#include <utils/Log.h>



smscallback = 0;

/*
typedef   int (*sqlite3_callback)(
void*,    Data provided in the 4th argument of sqlite3_exec()
int,      The number of columns in row
char**,   An array of strings representing fields in the row
char**    An array of strings representing column names
);
*/

static int mbim_sms_read_callback(void *param, int argc, char **argv,
        char **cname) {
    int i, finished = 0;
    mbim_sms_sqlresult *sqlresult = (mbim_sms_sqlresult *)param;

    for (i = 0; i < argc; i++) {
        if (strcmp("_id", cname[i]) == 0) {
            RLOGD("%s: id=%s\n", __FUNCTION__, argv[i]);
            finished |= 0x1;
            sqlresult->table[sqlresult->count].id = atoi(argv[i]);
            RLOGD("%s: sqlresult->table[sqlresult->count].id=%d\n", __FUNCTION__, sqlresult->table[sqlresult->count].id);
        }

        if (strcmp("status", cname[i]) == 0) {
            RLOGD("%s: status=%s\n", __FUNCTION__, argv[i]);
            finished |= 0x10;
            sqlresult->table[sqlresult->count].status = atoi(argv[i]);
            RLOGD("%s: sqlresult->table[sqlresult->count].status=%d\n", __FUNCTION__, sqlresult->table[sqlresult->count].status);
        }

        if (strcmp("pdu", cname[i]) == 0) {
            RLOGD("%s: pdu=%s\n", __FUNCTION__, argv[i]);
            finished |= 0x100;
            sqlresult->table[sqlresult->count].pdu = (char *)calloc(strlen(argv[i]) + 1, sizeof(char));
            memcpy(sqlresult->table[sqlresult->count].pdu, argv[i],
                    strlen(argv[i]));

            sqlresult->table[sqlresult->count].pdu_size = strlen(argv[i])/2;
            RLOGD("%s: pdu_size=%d\n", __FUNCTION__, sqlresult->table[sqlresult->count].pdu_size);
            RLOGD("%s: sqlresult->table[sqlresult->count].pdu=%s\n", __FUNCTION__, sqlresult->table[sqlresult->count].pdu);
        }

        if (0x111 == finished) {
            sqlresult->count++;
            smscallback = 1;
        }
    }

    RLOGD("%s: sqlresult->count: %d\n", __FUNCTION__, sqlresult->count);
    return 0;
}

static int mbim_sms_new_callback(void *param, int argc, char **argv,
        char **cname) {
    int *sqlresult = (int *)param;
    return 0;
}

int mbim_sqlite_create(void) {
    sqlite3 *db = NULL;
    char *errmsg = NULL;
    char sql_createtable[MBIM_SQL_LEN];
    int rc, ret = 0;

    // create db
    rc = sqlite3_open(MBIM_DB, &db);

    if (rc != 0) {
        RLOGE("%s: open %s fail [%d:%s]\n", __FUNCTION__, MBIM_DB,
                sqlite3_errcode(db), sqlite3_errmsg(db));
        ret = -1;
        goto out;
    } else {
        RLOGD("%s: open %s success\n", __FUNCTION__, MBIM_DB);
    }

    // create sms table
    memset(sql_createtable, 0, MBIM_SQL_LEN);
    sprintf(sql_createtable,
            "CREATE TABLE %s(_id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "status INTEGER,"
            "pdu TEXT);", MBIM_SMS_TABLE);

    rc = sqlite3_exec(db, sql_createtable, NULL, NULL, &errmsg);
    if (rc == 1) {
        RLOGE("%s: %s already exists\n", __FUNCTION__, MBIM_SMS_TABLE);
    } else if (rc != 0) {
        RLOGE("%s: create table fail, errmsg=%s [%d:%s]\n", __FUNCTION__,
                errmsg, sqlite3_errcode(db), sqlite3_errmsg(db));
        ret = -1;
        goto out;
    } else {
        RLOGE("%s: create table %s success\n", __FUNCTION__, MBIM_SMS_TABLE);
    }

    memset(sql_createtable, 0, sizeof(sql_createtable));
    sprintf(sql_createtable, "chmod 777 %s", MBIM_DB);
    system(sql_createtable);
    sync();
    out: sqlite3_close(db);
    return ret;
}

int mbim_sql_insert(char* table, char* values) {
    char **result;
    int rownum;
    int colnum;
    sqlite3 *db = NULL;
    char *errmsg = NULL;
    char sqlbuf[MBIM_SQL_LEN];
    int rc, ret = 0;

    // open db
    rc = sqlite3_open(MBIM_DB, &db);
    if (rc != 0) {
        RLOGE("%s: open %s fail [%d:%s]\n", __FUNCTION__, MBIM_DB,
                sqlite3_errcode(db), sqlite3_errmsg(db));
        ret = -1;
        goto out;
    } else {
        RLOGD("%s: open %s success\n", __FUNCTION__, MBIM_DB);
    }

    // insert item
    memset(sqlbuf, 0, MBIM_SQL_LEN);
    sprintf(sqlbuf, "INSERT INTO %sVALUES(%s);", table, values);
    RLOGE("%s: sqlbuf=%s\n", __FUNCTION__, sqlbuf);

    rc = sqlite3_exec(db, sqlbuf, NULL, NULL, &errmsg);
    if (rc != 0) {
        RLOGE("%s: insert table fail, errmsg=%s [%d:%s]\n", __FUNCTION__,
                errmsg, sqlite3_errcode(db), sqlite3_errmsg(db));
        ret = -1;
        goto out;
    } else {
        RLOGD("%s: insert table %s success\n", __FUNCTION__, MBIM_SMS_TABLE);
    }

out:
    sqlite3_close(db);
    return ret;
}

int mbim_sql_exec(SqlQueryType type, char* sqlbuf, void * data) {
    sqlite3 *db = NULL;
    char *errmsg = NULL;
    int rc, ret = 0;
    rc = sqlite3_open(MBIM_DB, &db);
    if (rc != 0) {
        RLOGE("%s: open %s fail [%d:%s]\n", __FUNCTION__, MBIM_DB,
                sqlite3_errcode(db), sqlite3_errmsg(db));
        ret = -1;
        goto out;
    } else {
        RLOGD("%s: open %s success\n", __FUNCTION__, MBIM_DB);
    }

    RLOGD("%s: sqlbuf=%s\n", __FUNCTION__, sqlbuf);

    smscallback = 0;
    if (type == SMS_INSERT) {
        rc = sqlite3_exec(db, sqlbuf, NULL, NULL, &errmsg);
    } else if (type == SMS_QUERY) {
        mbim_sms_sqlresult *smsresult = (mbim_sms_sqlresult *)data;

        memset(smsresult, 0, sizeof(mbim_sms_sqlresult));
        smsresult->table = (mbim_read_sms *)calloc(1024, sizeof(mbim_read_sms));
        rc = sqlite3_exec(db, sqlbuf, &mbim_sms_read_callback, smsresult, &errmsg);
    } else if (type == SMS_DELETE){
        rc = sqlite3_exec(db, sqlbuf, NULL, NULL, &errmsg);
    } else if (type == SMS_LAST_INSERT_ROWID) {
        int *newIndex = (int *)data;
        rc = sqlite3_exec(db, sqlbuf, &mbim_sms_new_callback, newIndex, &errmsg);
    }

    if (rc != 0) {
        RLOGE("%s: sql exec fail, errmsg=%s [%d:%s]\n", __FUNCTION__,
                errmsg, sqlite3_errcode(db), sqlite3_errmsg(db));
        ret = MBIM_SQL_ERR;
        goto out;
    }
    if (smscallback == 1)
        ret = 1;
    else
        ret = MBIM_SQL_ERR;

    RLOGD("%s: sql exec %s success, ret=0x%x\n", __FUNCTION__,
            MBIM_SMS_TABLE, ret);
out:
    sqlite3_close(db);
    return ret;
}
