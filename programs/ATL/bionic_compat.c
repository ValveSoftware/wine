#include <stdio.h>
#include <stdlib.h>

/* for getting _r_debug out of the dynamic section */
/* this is needed by the shim bionic linker to register stuff with gdb */
#include <elf.h>
#include <link.h>

/* the dynamic section */
extern ElfW(Dyn) _DYNAMIC[];

extern struct r_debug *_r_debug_ptr;
/* this has to be called from the main executable, since that's the only one guaranteed to have the debug section filled in */
void init__r_debug() {
#if defined(_r_debug)
/* _r_debug is defined by glibc and is declared as extern in link.h*/
	_r_debug_ptr = &_r_debug;
#else
	int i = 0;
	ElfW(Dyn) current;

	do {
		current = _DYNAMIC[i];
		if(current.d_tag == DT_DEBUG) {
			_r_debug_ptr = (struct r_debug *)current.d_un.d_ptr;
			break;
		}
		i++;
	} while(current.d_tag != 0);

	if(!_r_debug_ptr) {
		fprintf(stderr, "error: no DEBUG tag in the dynamic section, treating this as fatal\n");
		exit(1);
	}
#endif
}

/* bionic stores some things relative to the thread pointer that are actually part of the ABI.
 * Some of these locations we can install our data at, some we cannot. Hopefully all the ones
 * any app decided are ABI are in the former category (or are compatible with glibc/musl ABI)
 *
 * So far, we've seen apps using -fstack-protect, resulting in accesses to (tp + 5).
 * this is what gnueabi programs do on x86(_64) as well, and on aarch64 we can control
 * the stack guard value (mainly to make sure it stays constant)
 */
#if defined(__arm__) || defined(__aarch64__)
/* this is the **ONLY** thread-local variable in the main executable, which means it has a well-known placement relative to the thread pointer */
_Thread_local uintptr_t TLS[] = {
	/* these are occupied by musl/glibc internal structures */
	/* (tp - 3) =    0xXXXXXXXXXXXXXXXX */ // TLS_SLOT_STACK_MTE
	/* (tp - 2) =    0xXXXXXXXXXXXXXXXX */ // TLS_SLOT_NATIVE_BRIDGE_GUEST_STATE
	/* (tp - 1) =    0xXXXXXXXXXXXXXXXX */ // TLS_SLOT_BIONIC_TLS
	/* dtv (per spec) */
	/* (tp + 0) =    0xXXXXXXXXXXXXXXXX */ // TLS_SLOT_DTV
	/* internal on glibc, seems to be unused on musl */
	/* (tp + 1) =    0xXXXXXXXXXXXXXXXX */ // TLS_SLOT_THREAD_ID
	/* PT_TLS of main executable gets copied here (so this array!) */
	/* (tp + 2) = */ 0x5555555555555555,   // TLS_SLOT_APP
	/* (tp + 3) = */ 0x5555555555555555,   // TLS_SLOT_OPENGL
	/* (tp + 4) = */ 0x5555555555555555,   // TLS_SLOT_OPENGL_API
	/* (tp + 5) = */ 0x5555555555555555,   // TLS_SLOT_STACK_GUARD
	/* (tp + 6) = */ 0x5555555555555555,   // TLS_SLOT_SANITIZER
	/* (tp + 7) = */ 0x5555555555555555,   // TLS_SLOT_ART_THREAD_SELF
};
#elif defined(__i386__) || defined(__x86_64__)
	/*
	 * PT_TLS goes before the thread pointer but bionic's slots go after
	 * how fucked are we? let's see what an app will access on glibc/musl if it decides
	 * to consider a particular slot a part of the platform ABI:
	 */

	/* glibc (64bit): */
	/*
	 * typedef struct
	 * {
	 *	void *tcb;  // (tp + 0) TLS_SLOT_SELF
	 *
	 *	dtv_t *dtv; // (tp + 1) TLS_SLOT_THREAD_ID
	 *	void *self; // (tp + 2) TLS_SLOT_APP
	 *	int multiple_threads; // (tp + 3) TLS_SLOT_OPENGL
	 *	int gscope_flag;      // [cont]
	 *	uintptr_t sysinfo;    // (tp + 4) TLS_SLOT_OPENGL_API
         *	// it's not a coincidence that this matches, it was only on arm that google decided
         *	// they might as well have an incompatible -fstack-protect ABI
	 *	uintptr_t stack_guard;   // (tp + 5) TLS_SLOT_STACK_GUARD
	 *	uintptr_t pointer_guard; // (tp + 6) TLS_SLOT_SANITIZER
	 *	unsigned long int unused_vgetcpu_cache[2]; // (tp + 7/8) TLS_SLOT_ART_THREAD_SELF/TLS_SLOT_DTV
	 *
	 *	unsigned int feature_1; // (tp + 9) TLS_SLOT_BIONIC_TLS
	 *	int __glibc_unused1;    // [cont]
	 *	void *__private_tm[4];  // (tp + 10) TLS_SLOT_NATIVE_BRIDGE_GUEST_STATE
	 *	void *__private_ss;
	 *	unsigned long long int ssp_base;
	 *	__128bits __glibc_unused2[8][4] __attribute__ ((aligned (32)));
	 *
	 * void *__padding[8];
	 * } tcbhead_t;
	 */
	/* glibc (32bit): */
	/*
	 * typedef struct
	 * {
	 *	void *tcb;  // (tp + 0) TLS_SLOT_SELF
	 *
	 *	dtv_t *dtv; // (tp + 1) TLS_SLOT_THREAD_ID
	 *	void *self; // (tp + 2) TLS_SLOT_APP
	 *	int multiple_threads; // (tp + 3) TLS_SLOT_OPENGL
	 *	uintptr_t sysinfo;    // (tp + 4) TLS_SLOT_OPENGL_API
         *	// it's not a coincidence that this matches, it was only on arm that google decided
         *	// they might as well have an incompatible -fstack-protect ABI
	 *	uintptr_t stack_guard;   // (tp + 5) TLS_SLOT_STACK_GUARD
	 *	uintptr_t pointer_guard; // (tp + 6) TLS_SLOT_SANITIZER
	 *	int gscope_flag; // (tp + 7) TLS_SLOT_ART_THREAD_SELF
	 *
	 *	unsigned int feature_1; // (tp + 8) TLS_SLOT_DTV
	 *	void *__private_tm[3];  // (tp + 9/10) TLS_SLOT_BIONIC_TLS/TLS_SLOT_NATIVE_BRIDGE_GUEST_STATE
	 *	void *__private_ss;
	 *	unsigned long  ssp_base;
	 * } tcbhead_t;
	 */
	/* musl: */
	/*
	 * struct pthread {
	 *	struct pthread *self; // (tp + 0) TLS_SLOT_SELF
	 *	uintptr_t *dtv; // (tp + 1) TLS_SLOT_THREAD_ID
	 *	struct pthread *prev, *next; // (tp + 2/3) TLS_SLOT_APP/TLS_SLOT_OPENGL
	 *	uintptr_t sysinfo; // (tp + 4) TLS_SLOT_OPENGL_API
         *	// it's not a coincidence that this matches, it was only on arm that google decided
         *	// they might as well have an incompatible -fstack-protect ABI
	 *	uintptr_t canary;  // (tp + 5) TLS_SLOT_STACK_GUARD
	 *
	 *	int tid;           // (tp + 6) TLS_SLOT_SANITIZER
	 *	int errno_val;     // [cont]
	 *	volatile int detach_state; // (tp + 7) TLS_SLOT_ART_THREAD_SELF
	 *	volatile int cancel;       // [cont]
	 *	volatile unsigned char canceldisable, cancelasync; // (tp + 8) TLS_SLOT_DTV
	 *	unsigned char tsd_used:1;                          // [cont]
	 *	unsigned char dlerror_flag:1;                      // [cont]
	 *	unsigned char *map_base; // (tp + 9) TLS_SLOT_BIONIC_TLS
	 *	size_t map_size;         // (tp + 10) TLS_SLOT_NATIVE_BRIDGE_GUEST_STATE
	 *	void *stack;
	 *	size_t stack_size;
	 *	size_t guard_size;
	 *	void *result;
	 *	struct __ptcb *cancelbuf;
	 *	void **tsd;
	 *	struct {
	 *		volatile void *volatile head;
	 *		long off;
	 *		volatile void *volatile pending;
	 *	} robust_list;
	 *	int h_errno_val;
	 *	volatile int timer_id;
	 *	locale_t locale;
	 *	volatile int killlock[1];
	 *	char *dlerror_buf;
	 *	void *stdio_locks;
	 * };
	 */
#endif
