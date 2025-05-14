#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "looper.h"

// dummy strong pointer class
struct sp {
	ALooper *ptr;
	/* the struct has to be larger then 16 bytes, because on aarch64 the
	* calling convention for returning structs larger than 16 bytes is the
	* same as the calling convention for returning large C++ objects */
	char filler[16];
};

/* --- */

struct sp _ZN7android6Looper12getForThreadEv(void);
ALooper * ALooper_forThread(void)
{
	return _ZN7android6Looper12getForThreadEv().ptr;
}

struct sp _ZN7android6Looper7prepareEi(int opts);
ALooper * ALooper_prepare(int opts)
{
	return _ZN7android6Looper7prepareEi(opts).ptr;
}

void _ZNK7android7RefBase9incStrongEPKv(ALooper *this, void *unused);
void ALooper_acquire(ALooper* looper) {
	_ZNK7android7RefBase9incStrongEPKv(looper, (void*)ALooper_acquire);
}

void _ZNK7android7RefBase9decStrongEPKv(ALooper *this, void *unused);
void ALooper_release(ALooper* looper) {
    _ZNK7android7RefBase9decStrongEPKv(looper, (void*)ALooper_acquire);
}

int _ZN7android6Looper7pollAllEiPiS1_PPv(ALooper *this, int timeoutMillis, int *outFd, int *outEvents, void **outData);
int ALooper_pollAll(int timeoutMillis, int* outFd, int* outEvents, void** outData)
{
	ALooper *looper = ALooper_forThread();
	if(!looper) {
		fprintf(stderr, "ALooper_pollAll: ALooper_forThread returned NULL\n");
		return 0;
	}

	return _ZN7android6Looper7pollAllEiPiS1_PPv(looper, timeoutMillis, outFd, outEvents,  outData);
}

int _ZN7android6Looper8pollOnceEiPiS1_PPv(ALooper *this, int timeoutMillis, int *outFd, int *outEvents, void **outData);
int ALooper_pollOnce(int timeoutMillis, int* outFd, int* outEvents, void** outData)
{
	ALooper *looper = ALooper_forThread();
	if(!looper) {
		fprintf(stderr, "ALooper_pollAll: ALooper_forThread returned NULL\n");
		return 0;
	}

	return _ZN7android6Looper8pollOnceEiPiS1_PPv(looper, timeoutMillis, outFd, outEvents,  outData);
}

int _ZN7android6Looper5addFdEiiiPFiiiPvES1_(ALooper *this, int fd, int ident, int events, Looper_callbackFunc callback, void *data);
int ALooper_addFd(ALooper* looper, int fd, int ident, int events, Looper_callbackFunc callback, void* data)
{
	return _ZN7android6Looper5addFdEiiiPFiiiPvES1_(looper, fd, ident, events, callback, data);
}

void _ZN7android6Looper4wakeEv(ALooper *this);
void ALooper_wake(ALooper *looper)
{
	_ZN7android6Looper4wakeEv(looper);
}

int _ZN7android6Looper8removeFdEi(ALooper* this, int fd);
int ALooper_removeFd(ALooper* looper, int fd)
{
	return _ZN7android6Looper8removeFdEi(looper, fd);
}

/* this is not part of the android API, but we use it internally */

bool _ZNK7android6Looper9isPollingEv(ALooper *this);
bool ALooper_isPolling(ALooper *looper)
{
	return _ZNK7android6Looper9isPollingEv(looper);
}
