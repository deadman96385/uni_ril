#define LOG_TAG "SUBSIDYLOCKJNI"

#include <jni.h>
#include <string.h>
#include "utils/Log.h"
#include "subsidy.h"
#include "at_toc.h"

//using namespace android;

#define UNUSED(x) (void*)x

char* ConvertJByteArrayToChars(JNIEnv *env, jbyteArray bytearray)
{
    char *chars = NULL;
    jbyte *bytes;
    int chars_len = 0;
    int h = 0;
    int l = 0;

    bytes = env->GetByteArrayElements(bytearray, 0);
    chars_len = env->GetArrayLength(bytearray);
    chars = new char[2*chars_len + 1];

    for (int i = 0; i < chars_len; i++) {
        h = (int) ((bytes[i] >> 4) & 0x0f);
        l = (int) (bytes[i] & 0x0f);

        if (h > 9) {
            chars[2*i] = (char)('A' + (h - 10));
        } else {
            chars[2*i] = (char) ('0' + h);
        } if (l > 9) {
            chars[2*i + 1] = (char) ('A' + (l - 10));
        } else {
            chars[2*i + 1] = (char) ('0' + l);
        }
    }

    chars[2*chars_len] = 0;
    ALOGD("blob in hex string is: %s", chars);
    env->ReleaseByteArrayElements(bytearray, bytes, 0);

    return chars;
}

/*
 * <result_code >:
 *  0 – If the operation is successful
 *  1 – If the protection algorithm is invalid, the signature is invalid or if there is a timestamp mismatch
 *  2 – If the Subsidy lock configuration data such as MCC/MNC/GID is invalid
 *  3 – If the IMEI received in the blob does not match with that of the device
 *  4 – If a length mismatch occurs
 *  5 – If some other error occurs
 */
int getResultCode(char* string)
{
    // the string format is +SPSLBLOB: <result_code>
    // +SPSLBLOB: 0
    int res = -1;
    ALOGD("getResultCode. str = %s\n", string);
    at_tok_start(&string);

    ALOGD("getResultCode. str2 = %s\n", string);
    at_tok_nextint(&string, &res);
    ALOGD("getResultCode. res = %d\n", res);

    return res;
}

/*
 * AT+SPSLBLOB=<len>,<“blob”>
 * return:
 * +SPSLBLOB: <result_code>
 * OK
 * or:
 *+CME ERROR:
 *
 * params:
 * <len>：the length is the blob data
 * <“blob”>：the data of the blob
 * <result_code >: the result returned
 *
 * @return 0 if blob operation is successful at modem otherwise 1 for operation failure
 */
JNIEXPORT jbyte JNICALL send_Blob_To_Modem_JNI(JNIEnv *env, jobject obj, jbyteArray buffer, jint length)
{
    UNUSED(obj);
    ALOGD("lock/unlock the device. length = %d\n", length);

    char* blob = NULL;
    const char* resString = NULL;
    char* str = NULL;
    char cmd[2048] = {0};
    int resCode = -1;

    blob = ConvertJByteArrayToChars(env, buffer);
    snprintf(cmd, sizeof(cmd), "AT+SPSLBLOB=%d,\"%s\"", strlen(blob) + 1, blob);
    ALOGD("send at cmd: %s\n", cmd);
    resString = sendCommand(0, cmd);
    ALOGD("the result string = %s\n", resString);

    if (!strncmp(resString, "+CME", 4)) {
        //some other error occurs
        delete blob;
        blob = NULL;
        return 0x01;
    }

    str = new char[128];
    strcpy(str, resString);
    resCode = getResultCode(str);

    delete blob;
    delete str;
    blob = NULL;
    str = NULL;

    if (resCode == 0) {
        return 0x00;
    } else {
        return 0x01;
    }
}

//get the status of the device is locked or unlocked
/*
bool getSimlockStatus(const char* string)
{
    //The string format is "+CPIN: < code>"
    bool status = true;
    char* res = NULL;
    char* str = NULL;

    str = new char[128];
    strcpy(str, string);
    ALOGD("getSimlockStatus. str = %s\n", str);
    at_tok_start(&str);
    ALOGD("getSimlockStatus. str2 = %s\n", str);
    at_tok_nextstr(&str, &res);
    ALOGD("getResultCode. res = %s\n", res);

    if ((0 == strcmp(res, "READY")) ||
        (0 == strcmp(res, "SIM PIN")) ||
        (0 == strcmp(res, "SIM PUK")) ||
        (0 == strcmp(res, "PIN1_BLOCK_PUK1_BLOCK")) ||
        (0 == strcmp(res, "PIN1_OK_PUK1_BLOCK"))) {
        status = false;
    }

    return status;
}
*/


//@return return 1 if subsidy device is permanent unlocked
JNIEXPORT jint JNICALL get_Sim_Permanent_Unlock_Status_JNI(JNIEnv *env, jobject obj)
{
    UNUSED(obj);
    ALOGD("get the Subsidy Lock status\n");

    const char* resString = NULL;
    const char* cmd = "AT+SPSLENABLED?";

    ALOGD("send at cmd: AT+SPSLENABLED?\n");
    resString = sendCommand(0, cmd);
    ALOGD("the result string = %s\n", resString);

    char* dumpStr = new char[128];
    strcpy(dumpStr, resString);
    int lockStatus = getResultCode(dumpStr);

    if (0 == lockStatus) {
        //unlocked
        ALOGD("UNLOCKED!\n");
        return (jint)1;
    }

    //resString is "+SPSLENABLED: 1" and simlock is locked
    ALOGD("LOCKED!\n");
    return (jint)0;
}


/*
 * the full class name of the native function is called,
 * such as "com/android/bluetooth/pan/PanService"
 */
//static const char *objectClassPathName = "com/rjio/slc/jni/JniLayer";
static const char *objectClassPathName =
        "com/rjio/slc/jni/JniLayer";

static JNINativeMethod getMethods[] = {
                {"sendBlobToModem", "([BI)B", (void*) send_Blob_To_Modem_JNI},
                {"getSimPermanentUnlockStatus", "()I", (void*) get_Sim_Permanent_Unlock_Status_JNI},
};


static int registerNativeMethods(JNIEnv* env, const char* className,
        JNINativeMethod* gMethods, int numMethods) {
    jclass clazz;
    clazz = env->FindClass(className);

    if (clazz == NULL) {
        ALOGE("Native registration unable to find class '%s'\n", className);
        return JNI_FALSE;
    }

    if (env->RegisterNatives(clazz, gMethods, numMethods) < 0) {
        ALOGE("RegisterNatives failed for '%s'\n", className);
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    UNUSED(reserved);
    JNIEnv* env;
    //use JNI1.4
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_4) != JNI_OK) {
        ALOGE("Error: GetEnv failed in JNI_OnLoad\n");
        return -1;
    }

    if (!registerNativeMethods(env, objectClassPathName, getMethods,
            sizeof(getMethods) / sizeof(getMethods[0]))) {
        ALOGE("Error: could not register native methods for JniLayer\n");
        return -1;
    }

    return JNI_VERSION_1_4;
}
