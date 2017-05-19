#ifndef REQUEST_THREADS_H
#define REQUEST_THREADS_H

void requestThreadsInit();
int getRequestChannel(RIL_SOCKET_ID socket_id, int request);
void putRequestChannel(RIL_SOCKET_ID socket_id, int request, int channelID);

#endif  // REQUEST_THREADS_H
