#define LOG_TAG "RILC"

#include <utils/Log.h>
#include <utility>
#include <string>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <tinyxml2.h>
#include "ril_ons.h"

using namespace tinyxml2;

#define CONFIG_FILE_PATH "/vendor/etc/numeric_operator.xml"
#define SHORT_PLMN_LEN 4

#ifdef __cplusplus
extern "C" {
#endif

static void adaptPlmn(char *plmnFmt, char *temp, int size)
{
    char plmnFirst[4] = { 0 };
    char plmnFinal[2] = { 0 };
    if(SHORT_PLMN_LEN == strlen(temp)) {
        strncpy(plmnFirst, temp, 3);
        strncpy(plmnFinal, temp + 3, 1);
        snprintf(plmnFmt, size, "%s0%s", plmnFirst, plmnFinal);
    } else {
        strcpy(plmnFmt, temp);
    }
    return;
}

static int matchPlmn (char *updatePlmn, char *plmn, char *plmnTemp)
{
    char *plmnTmp, *name;
    char plmnFormat[7] = { 0 };
    plmnTmp = strtok_r(plmnTemp, "=", &name);
    if(plmnTmp != NULL) {
        adaptPlmn(plmnFormat, plmnTmp, sizeof(plmnFormat));
        if (!strcmp(plmnFormat, plmn) && !((name[0] == '\\') && (name[1] == 't'))) {
            strncpy(updatePlmn, name, strlen(name) + 1);
            RLOGD("match success, plmn: %s, Operator Name: %s", plmn, updatePlmn);
            return 0;
        }
    }
    return -1;
}

int RIL_getONS(char *updatePlmn, char *plmn)
{
    int ret = -1;
    if (updatePlmn == NULL || plmn == NULL) {
        return ret;
    }
    char plmnElement[64];
    XMLDocument doc;
    ret = doc.LoadFile(CONFIG_FILE_PATH);
    if(ret != 0) {
        RLOGD("Load File error: %d", ret);
        goto EXIT;
    }

    XMLElement *resources;
    resources = doc.RootElement();
    if(NULL == resources) {
        goto EXIT;
    }
    XMLElement *stringArray;
    XMLElement *plmnsElement;
    stringArray = resources->FirstChildElement("string-array");
    plmnsElement = stringArray->FirstChildElement("item");

    while(plmnsElement) {
        const char* plmnTemp = plmnsElement->GetText();
        memset(plmnElement, 0, sizeof(plmnElement));
        strcpy(plmnElement, plmnTemp);
        ret = matchPlmn(updatePlmn, plmn, plmnElement);
        if (!ret) {
            break;
        }
        plmnsElement = plmnsElement->NextSiblingElement();
    }
EXIT:
    return ret;
}

#ifdef __cplusplus
}
#endif
