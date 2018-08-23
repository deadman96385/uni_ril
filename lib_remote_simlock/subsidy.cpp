#include <utils/Log.h>
#include <dlfcn.h>
#include <subsidy.h>

//using namespace android;

typedef const char* (*ATC_FUNC)(int, const char*);

const char* sendCommand(int phoneId, const char* atCmd)
{
    void *handle;
    const char *error;
    ATC_FUNC atc_func = NULL;

    handle = dlopen("libatci.so", RTLD_NOW);
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
