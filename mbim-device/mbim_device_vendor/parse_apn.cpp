#define LOG_TAG "MBIM-Device"

#include "parse_apn.h"
#include <map>
#include <utils/Log.h>
#include <utility>
#include <string>
#include "tinyxml2.h"
#include "mbim_uuid.h"
#include "mbim_enums.h"
#include "mbim_message.h"
#include "mbim_device_basic_connect.h"

using namespace tinyxml2;
using namespace std;

#define ARRAY_SIZE               128
#define APN_AUTHTYPE_PAP         1
#define APN_AUTHTYPE_CHAP        2
#define APN_AUTHTYPE_PAP_CHAP    3

//#define APN_XML_PATH             "/data/mbim/telephony/apns-conf.xml"

#define APN_XML_PATH             "/data/local/tmp/apns-conf.xml"

map<string, MbimProvisionedContextsInfo_2*> s_apnMap;


#ifdef __cplusplus
extern "C" {
#endif

XMLElement* queryApnNodeByPlmn(XMLElement* root,int mcc, int mnc)
{

    XMLElement* apnNode=root->FirstChildElement("apn");
    while(apnNode!=NULL)
    {
        if((apnNode->IntAttribute("mcc") == mcc) && (apnNode->IntAttribute("mnc") == mnc)){
            if(NULL != strstr(apnNode->Attribute("type"), "default")){
                break;
            }
        }
        apnNode=apnNode->NextSiblingElement();//next
    }
    return apnNode;
}

void getApnUsingPLMN(char* plmn, MbimContext_2  *contextInfo)
{
    XMLDocument doc;
    doc.LoadFile(APN_XML_PATH);

    XMLElement *resources = doc.RootElement();
    if(NULL == resources) return;
//    RLOGD("mcc=%d,mnc=%d",mcc,mnc);
    int mccTemp, mncTemp, authtype;
    const char * apnType;
    const char * apn;
    const char * userName;
    const char * passWord;

    //MbimContext_2  *contextInfo_temp = (s_apnMap.at(plmn))->context[0];
    if(s_apnMap.find(plmn) != s_apnMap.end()){
        MbimContext_2  *contextInfo_temp;
        contextInfo_temp = (s_apnMap[plmn])->context[0];
        RLOGD("apn=%s", contextInfo_temp->accessString);
        RLOGD("userName=%s", contextInfo_temp->username);
        RLOGD("passWord=%s", contextInfo_temp->password);
        RLOGD("authtype=%d", contextInfo_temp->authProtocol);

        contextInfo->accessString = strdup(contextInfo_temp->accessString);
        if(NULL != contextInfo_temp->username){
            contextInfo->username = strdup(contextInfo_temp->username);
        }
        if(NULL != contextInfo_temp->password){
            contextInfo->password = strdup(contextInfo_temp->password);
        }
        contextInfo->authProtocol = contextInfo_temp->authProtocol;
    }
    return;

//    XMLElement *apnElement = resources->FirstChildElement();
//    while (apnElement != NULL) {
//        /*** get mcc、mnc ***/
//        mccTemp = apnElement->IntAttribute("mcc");
//        mncTemp = apnElement->IntAttribute("mnc");
//        RLOGD("mccTemp=%d,mncTemp=%d",mccTemp,mncTemp);
//        if((mcc != mccTemp) || (mnc != mncTemp)){
//            apnElement = apnElement->NextSiblingElement();
//            continue;
//        }
//        /*** get apn type ***/
//        apnType = apnElement->Attribute("type");
//        RLOGD("apntype=%s",apnType);
//        if(NULL == strstr(apnType, "default")){
//            apnElement = apnElement->NextSiblingElement();
//            continue;
//        }
//        /*** get apn、userName、passWord、authtype ***/
//        apn = apnElement->Attribute("apn");
//        userName = apnElement->Attribute("user");
//        passWord = apnElement->Attribute("password");
//        authtype = apnElement->IntAttribute("authtype");
//        RLOGD("apn=%s, userName=%s, passWord=%s, authtype=%d", apn, userName, passWord, authtype);
//
//        contextInfo->accessString = strdup(apn);
//        RLOGD("log1");
//
//        if(NULL != userName){
//            contextInfo->username = strdup(userName);
//        }
//        RLOGD("log2");
//
//        if(NULL != passWord){
//            contextInfo->password = strdup(passWord);
//        }
//        RLOGD("log3");
//
//        switch(authtype){
//            case APN_AUTHTYPE_PAP:
//                contextInfo->authProtocol = MBIM_AUTH_PROTOCOL_PAP;
//                break;
//            case APN_AUTHTYPE_CHAP:
//                contextInfo->authProtocol = MBIM_AUTH_PROTOCOL_CHAP;
//                break;
//            case APN_AUTHTYPE_PAP_CHAP:
//                contextInfo->authProtocol = MBIM_AUTH_PROTOCOL_MSCHAPV2;
//                break;
//            default:
//                contextInfo->authProtocol = MBIM_AUTH_PROTOCOL_NONE;
//                break;
//        }
//
//        RLOGD("log4");
//        return;
//    }
}

int setApnUsingPLMN(char* mcc, char* mnc, MbimContext_2  *contextInfo)
{

    int mccTemp, mncTemp;
    const char * apnType;
    XMLError error;
    XMLDocument doc;
    doc.LoadFile(APN_XML_PATH);

    XMLElement *resources = doc.RootElement();
    if(NULL == resources) return 0;

    XMLElement *apnElement = queryApnNodeByPlmn(resources, atoi(mcc), atoi(mnc));

    if(apnElement != NULL){
        RLOGD("old-apn=%s",apnElement->Attribute("apn"));
        RLOGD("new-apn=%s",contextInfo->accessString);
        if(strlen(contextInfo->accessString) != 0 && apnElement->Attribute("apn")){  //update
            if (strcmp(apnElement->Attribute("apn"), contextInfo->accessString) != 0 ){
                apnElement->SetAttribute("apn",contextInfo->accessString);
                RLOGD("apn=%s",apnElement->Attribute("apn"));
            }
        }

        RLOGD("old-user=%s",apnElement->Attribute("user"));
        RLOGD("new-user=%s",contextInfo->username);
        if (strlen(contextInfo->username) != 0){
            apnElement->SetAttribute("user",contextInfo->username);
            RLOGD("user=%s",apnElement->Attribute("user"));
        } else {  //delete
            apnElement->DeleteAttribute("user");
        }

        RLOGD("old-password=%s",apnElement->Attribute("password"));
        RLOGD("new-password=%s",contextInfo->password);
        if (strlen(contextInfo->password) != 0){
            apnElement->SetAttribute("password",contextInfo->password);
            RLOGD("password=%s",apnElement->Attribute("password"));
        } else {  //delete
            apnElement->DeleteAttribute("password");
        }

        RLOGD("old-authtype=%d",apnElement->IntAttribute("authtype"));
        RLOGD("new-authtype=%d",contextInfo->authProtocol);
        if (contextInfo->authProtocol == MBIM_AUTH_PROTOCOL_NONE) {
            apnElement->DeleteAttribute("authtype");
        } else {
            apnElement->SetAttribute("authtype",contextInfo->authProtocol);
            RLOGD("authtype=%d",apnElement->IntAttribute("authtype"));
        }
    } else {
        RLOGD("add new apn! apn = %s, username = %s, password = %s, authtype = %d",
                contextInfo->accessString, contextInfo->username, contextInfo->password, contextInfo->authProtocol);
        XMLElement* newApnElement = doc.NewElement("apn");
        newApnElement->SetAttribute("mcc",mcc);
        newApnElement->SetAttribute("mnc",mnc);
        newApnElement->SetAttribute("apn",contextInfo->accessString);
        newApnElement->SetAttribute("type","default");
        if (strlen(contextInfo->username) != 0){
            newApnElement->SetAttribute("user",contextInfo->username);
        }
        if (strlen(contextInfo->password) != 0){
            newApnElement->SetAttribute("password",contextInfo->password);
        }
        if (contextInfo->authProtocol != MBIM_AUTH_PROTOCOL_NONE) {
            newApnElement->SetAttribute("authtype",contextInfo->authProtocol);
        }
        resources->InsertEndChild(newApnElement);
    }

    error = doc.SaveFile(APN_XML_PATH);
    if(error == 0){
        RLOGD("save successful");
        return 1;
    } else {
        RLOGD("save failed error = %d",error);
        return 0;
    }

}

int loadAPNs()
{
    int authtype;
    const char* mcc;
    const char* mnc;
    const char * apnType;
    const char * apn;
    const char * userName;
    const char * passWord;
    char plmn_char[7];
    string plmn;
    XMLDocument doc;
    MbimProvisionedContextsInfo_2 *provisionedContextsInfo;

    doc.LoadFile(APN_XML_PATH);
    XMLElement *resources = doc.RootElement();
    if(NULL == resources) return 0;

    XMLElement *apnElement = resources->FirstChildElement();
        while (apnElement != NULL) {

            /*** get apn type ***/
            apnType = apnElement->Attribute("type");
            RLOGD("apntype=%s",apnType);
            if(NULL == strstr(apnType, "default")){
                apnElement = apnElement->NextSiblingElement();
                continue;
            }

            /*** get mcc、mnc ***/
            mcc = apnElement->Attribute("mcc");
            mnc = apnElement->Attribute("mnc");
            plmn = string(mcc) + string(mnc);
            strcpy(plmn_char,plmn.c_str());
            RLOGD("plmn=%s",plmn_char);

            if(s_apnMap.find(plmn) != s_apnMap.end()){
                //provisionedContextsInfo = s_apnMap.at(plmn);
                provisionedContextsInfo = s_apnMap[plmn];
            } else {
                RLOGD("new provisionedContextsInfo");
                provisionedContextsInfo = (MbimProvisionedContextsInfo_2 *)calloc(1, sizeof(MbimProvisionedContextsInfo_2));
                provisionedContextsInfo->context = (MbimContext_2 **)calloc(1, sizeof(MbimContext_2));
            }

            MbimContext_2  *contextInfo =
                    (MbimContext_2 *)calloc(1, sizeof(MbimContext_2));

            contextInfo->contextType = MBIM_CONTEXT_TYPE_INTERNET;
            contextInfo->compression = MBIM_COMPRESSION_NONE;

            /*** get apn、userName、passWord、authtype ***/
            apn = apnElement->Attribute("apn");
            userName = apnElement->Attribute("user");
            passWord = apnElement->Attribute("password");
            authtype = apnElement->IntAttribute("authtype");
            RLOGD("apn=%s, userName=%s, passWord=%s, authtype=%d", apn, userName, passWord, authtype);

            contextInfo->accessString = strdup(apn);

            if(NULL != userName){
                contextInfo->username = strdup(userName);
            }

            if(NULL != passWord){
                contextInfo->password = strdup(passWord);
            }

            switch(authtype){
                case APN_AUTHTYPE_PAP:
                    contextInfo->authProtocol = MBIM_AUTH_PROTOCOL_PAP;
                    break;
                case APN_AUTHTYPE_CHAP:
                    contextInfo->authProtocol = MBIM_AUTH_PROTOCOL_CHAP;
                    break;
                case APN_AUTHTYPE_PAP_CHAP:
                    contextInfo->authProtocol = MBIM_AUTH_PROTOCOL_MSCHAPV2;
                    break;
                default:
                    contextInfo->authProtocol = MBIM_AUTH_PROTOCOL_NONE;
                    break;
            }

            provisionedContextsInfo->context[provisionedContextsInfo->elementCount] = contextInfo;
            s_apnMap.insert(map<string, MbimProvisionedContextsInfo_2*>::value_type(plmn,provisionedContextsInfo));


            //MbimContext_2  *contextInfo_test = (s_apnMap.at(plmn))->context[provisionedContextsInfo->elementCount];
            MbimContext_2  *contextInfo_test;
            if ( s_apnMap.find(plmn) != s_apnMap.end() ){
                contextInfo_test = (s_apnMap[ plmn ])->context[ provisionedContextsInfo->elementCount];
            }

            provisionedContextsInfo->elementCount++;

            RLOGD("plmn=%s, apnCount=%d", plmn_char, provisionedContextsInfo->elementCount);
//            for(uint32_t i = 0; i < provisionedContextsInfo->elementCount; i++){
//                MbimContext_2  *contextInfo_test = (s_apnMap.at(plmn))->context[i];
//                RLOGD("apn=%s", contextInfo_test->accessString);
//            }
            RLOGD("=====================================================================");
            apnElement = apnElement->NextSiblingElement();
        }

        RLOGD("apn size=%d", s_apnMap.size());

    return 1;
}
#ifdef __cplusplus
}
#endif
