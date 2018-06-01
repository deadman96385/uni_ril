#include <utils/Log.h>
#include <dlfcn.h>
#include <subsidy.h>

//using namespace android;

#ifdef SUBSIDYLOCK_JNI_ENABLED_64
#define LIB_CACULATE_PATH "/vendor/lib64/libatci.so"
#else
#define LIB_CACULATE_PATH "/vendor/lib/libatci.so"
#endif

typedef const char* (*ATC_FUNC)(int, const char*);

const char* sendCommand(int phoneId, const char* atCmd)
{
    void *handle;
    const char *error;
    ATC_FUNC atc_func = NULL;

    handle = dlopen(LIB_CACULATE_PATH, RTLD_NOW);
    if (!handle) {
        ALOGE ("dlopen error :%s\n", dlerror());
        return NULL;
    }

    dlerror();
 
    atc_func = (ATC_FUNC)dlsym(handle, "sendCmd");
    if ((error = dlerror()) != NULL)  {
        ALOGE ("dlsym error :%s\n", error);
        return NULL;
    }

    const char* str = (*atc_func)(phoneId, atCmd);

    dlclose(handle);
    return str;
}
