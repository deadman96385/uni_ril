/* //device/system/rild/rild.c
**
** Copyright 2006 The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <telephony/ril.h>
#define LOG_TAG "RILD"
#include <log/log.h>
#include <cutils/properties.h>
#include <cutils/sockets.h>
#include <sys/capability.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libril/ril_ex.h>

#define MAX_LIB_ARGS        16
#if defined(PRODUCT_COMPATIBLE_PROPERTY)
#define LIB_PATH_PROPERTY   "vendor.rild.libpath"
#define LIB_ARGS_PROPERTY   "vendor.rild.libargs"
#else
#define LIB_PATH_PROPERTY   "rild.libpath"
#define LIB_ARGS_PROPERTY   "rild.libargs"
#endif

#define PHONE_COUNT_PROPERTY    "persist.vendor.radio.phone_count"

static void usage(const char *argv0) {
    fprintf(stderr, "Usage: %s -l <ril impl library> [-- <args for impl library>]\n", argv0);
    exit(EXIT_FAILURE);
}

static int make_argv(char * args, char ** argv) {
    // Note: reserve argv[0]
    int count = 1;
    char * tok;
    char * s = args;

    while ((tok = strtok(s, " \0"))) {
        argv[count] = tok;
        s = NULL;
        count++;
    }
    return count;
}

int main(int argc, char **argv) {
    // vendor ril lib path either passed in as -l parameter, or read from rild.libpath property
    const char *rilLibPath = NULL;
    // ril arguments either passed in as -- parameter, or read from rild.libargs property
    char **rilArgv;
    // handle for vendor ril lib
    void *dlHandle;
    // Pointer to ril init function in vendor ril
    const RIL_RadioFunctions *(*rilInit)(const struct RIL_Env *, int, char **);
    // Pointer to sap init function in vendor ril
    const RIL_RadioFunctions *(*rilUimInit)(const struct RIL_Env *, int, char **);
    const char *err_str = NULL;

    // functions returned by ril init function in vendor ril
    const RIL_RadioFunctions *funcs;
    // flat to indicate if -- parameters are present
    unsigned char hasLibArgs = 0;

    // handle for ril lib
    void *rilDlHandle = NULL;
    const struct RIL_Env *(*rilStartEventLoop)();
    const struct RIL_Env *rilEnv = NULL;
    void (*rilRegister)(const RIL_RadioFunctions *);
    void (*rilcThreadPool)();
    void (*rilRegisterSocket)(const RIL_RadioFunctions *(*)(const struct RIL_Env *, int, char **),
            RIL_SOCKET_TYPE, int, char **);
    char prop[PROPERTY_VALUE_MAX] = {0};
    const char *rilLibName = NULL;
    const char *vendorRilLibName = NULL;

    int i;
    // ril/socket id received as -c parameter, otherwise set to 0
    const char *clientId = NULL;

    RLOGD("**RIL Daemon Started**");
    RLOGD("**RILd param count=%d**", argc);

    umask(S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);
    for (i = 1; i < argc ;) {
        if (0 == strcmp(argv[i], "-l") && (argc - i > 1)) {
            rilLibPath = argv[i + 1];
            i += 2;
        } else if (0 == strcmp(argv[i], "--")) {
            i++;
            hasLibArgs = 1;
            break;
        } else if (0 == strcmp(argv[i], "-c") &&  (argc - i > 1)) {
            clientId = argv[i+1];
            i += 2;
        } else {
            usage(argv[0]);
        }
    }

    if (clientId == NULL) {
        clientId = "0";
    } else if (atoi(clientId) >= MAX_RILDS) {
        RLOGE("Max Number of rild's supported is: %d", MAX_RILDS);
        exit(0);
    }

    property_get(PHONE_COUNT_PROPERTY, prop, "2");
    RLOGD("phone count: %s", prop);
    if (strcmp(prop, "1") == 0) {
        rilLibName = "librilsprd-single.so";
        vendorRilLibName = "libsprd-ril-single.so";
    } else {
        rilLibName = "librilsprd.so";
        vendorRilLibName = "libsprd-ril.so";
    }

    rilDlHandle = dlopen(rilLibName, RTLD_NOW);
    if (rilDlHandle == NULL) {
        RLOGE("dlopen failed: %s", dlerror());
        exit(EXIT_FAILURE);
    }

    dlHandle = dlopen(vendorRilLibName, RTLD_NOW);
    if (dlHandle == NULL) {
        RLOGE("dlopen failed: %s", dlerror());
        exit(EXIT_FAILURE);
    }

    rilStartEventLoop = (const struct RIL_Env *(*)())dlsym(rilDlHandle, "RIL_startEventLoop");
    if (rilStartEventLoop == NULL) {
        RLOGE("RIL_startEventLoop not defined or exported in librilsprd.so or librilsprd-single.so");
        exit(EXIT_FAILURE);
    }

    dlerror(); // Clear any previous dlerror
    rilRegister = (void (*)(const RIL_RadioFunctions *))dlsym(rilDlHandle, "RIL_register");
    if (rilRegister == NULL) {
        RLOGE("RIL_register not defined or exported in librilsprd.so or librilsprd-single.so");
        exit(EXIT_FAILURE);
    }

    dlerror(); // Clear any previous dlerror
    rilcThreadPool = (void (*)())dlsym(rilDlHandle, "rilc_thread_pool");
    if (rilcThreadPool == NULL) {
        RLOGE("rilc_thread_pool not defined or exported in librilsprd.so or librilsprd-single.so");
        exit(EXIT_FAILURE);
    }

    dlerror(); // Clear any previous dlerror
    rilRegisterSocket = (void (*)())dlsym(rilDlHandle, "RIL_register_socket");
    if (rilRegisterSocket == NULL) {
        RLOGE("RIL_register_socket not defined or exported in librilsprd.so or librilsprd-single.so");
        exit(EXIT_FAILURE);
    }

    rilEnv = rilStartEventLoop();

    dlerror(); // Clear any previous dlerror
    rilInit =
        (const RIL_RadioFunctions *(*)(const struct RIL_Env *, int, char **))
        dlsym(dlHandle, "RIL_Init");

    if (rilInit == NULL) {
        RLOGE("RIL_Init not defined or exported in %s\n", rilLibPath);
        exit(EXIT_FAILURE);
    }

    dlerror(); // Clear any previous dlerror
    rilUimInit =
        (const RIL_RadioFunctions *(*)(const struct RIL_Env *, int, char **))
        dlsym(dlHandle, "RIL_SAP_Init");
    err_str = dlerror();
    if (err_str) {
        RLOGW("RIL_SAP_Init not defined or exported in %s: %s\n", rilLibPath, err_str);
    } else if (!rilUimInit) {
        RLOGW("RIL_SAP_Init defined as null in %s. SAP Not usable\n", rilLibPath);
    }

    if (hasLibArgs) {
        rilArgv = argv + i - 1;
        argc = argc -i + 1;
    } else {
        static char * newArgv[MAX_LIB_ARGS];
        rilArgv = newArgv;
        argc = make_argv("", rilArgv);
    }

    rilArgv[argc++] = "-c";
    rilArgv[argc++] = (char*)clientId;
    RLOGD("RIL_Init argc = %d clientId = %s", argc, rilArgv[argc-1]);

    // Make sure there's a reasonable argv[0]
    rilArgv[0] = argv[0];

    funcs = rilInit(rilEnv, argc, rilArgv);
    RLOGD("RIL_Init rilInit completed");

    rilRegister(funcs);

    RLOGD("RIL_Init RIL_register completed");

    if (rilUimInit) {
        RLOGD("RIL_register_socket started");
        rilRegisterSocket(rilUimInit, RIL_SAP_SOCKET, argc, rilArgv);
    }

    RLOGD("RIL_register_socket completed");

    rilcThreadPool();

    RLOGD("RIL_Init starting sleep loop");
    while (true) {
        sleep(UINT32_MAX);
    }
}
