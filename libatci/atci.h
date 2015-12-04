#ifndef ATCI_H_
#define ATCI_H_

#ifdef __cplusplus
extern "C" {
#endif

const char *sendCmd(int phoneId, const char *atCmd);

#ifdef __cplusplus
}
#endif

#endif  // ATCI_H_
