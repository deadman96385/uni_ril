/**
 * Copyright (C) 2016 Spreadtrum Communications Inc.
 */

#ifndef TELEPHONYEX_H_
#define TELEPHONYEX_H_

#ifdef __cplusplus
extern "C" {
#endif

int updatePlmn(int simId, const char *mncmcc, char *updatedPlmn);

int updateNetworkList(int simId, char **networkList, size_t len,
                         char *updatedNetList);

#ifdef __cplusplus
}
#endif

#endif  // TELEPHONYEX_H_
