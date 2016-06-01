/**
 * debug.h --- debug.h implementation for the phoneserver
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 */

#ifndef DEBUG_H_
#define DEBUG_H_

#define LOG_TAG "RIL-PHS"
#include <utils/Log.h>

// #define PHS_DEBUG

#ifdef PHS_DEBUG
#define PHS_LOGD(x...) RLOGD(x)
#define PHS_LOGE(x...) RLOGE(x)
#define PHS_LOGI(x...) RLOGI(x)
#else
#define PHS_LOGD(x...) do {} while (0)
#define PHS_LOGI(x...) do {} while (0)
#define PHS_LOGE(x...) RLOGE(x)
#endif

#endif  // DEBUG_H_
