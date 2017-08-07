#ifndef RIL_THREADS_H_
#define RIL_THREADS_H_

#define MAX_THR                 3

typedef struct {
    int requestNumber;
    int (*freeFunction)(void *data, size_t datalen);
} MemoryManager;

extern MemoryManager s_memoryManager[];
extern MemoryManager s_oemMemoryManager[];
extern MemoryManager s_imsMemoryManager[];

#endif  // RIL_THREADS_H_
