#ifndef LOOPER_H
#define LOOPER_H

typedef void ALooper;
typedef int (*Looper_callbackFunc)(int fd, int events, void* data);

ALooper * ALooper_prepare(int opts);
void ALooper_wake(ALooper *looper);
bool ALooper_isPolling(ALooper *looper);
int ALooper_pollOnce(int timeoutMillis, int* outFd, int* outEvents, void** outData);
int ALooper_addFd(ALooper* looper, int fd, int ident, int events, Looper_callbackFunc callback, void* data);

#endif
