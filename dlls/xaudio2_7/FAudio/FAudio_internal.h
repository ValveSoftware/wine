/* FAudio - XAudio Reimplementation for FNA
 *
 * Copyright (c) 2011-2018 Ethan Lee, Luigi Auriemma, and the MonoGame Team
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software in a
 * product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Ethan "flibitijibibo" Lee <flibitijibibo@flibitijibibo.com>
 *
 */

#include "FAudio.h"
#include "FAPOBase.h"

#ifdef FAUDIO_UNKNOWN_PLATFORM
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#define FAudio_malloc malloc
#define FAudio_realloc realloc
#define FAudio_free free
#define FAudio_alloca(x) alloca(uint8_t, x)
#define FAudio_dealloca(x) dealloca(x)
#define FAudio_zero(ptr, size) memset(ptr, '\0', size)
#define FAudio_memset(ptr, val, size) memset(ptr, val, size)
#define FAudio_memcpy(dst, src, size) memcpy(dst, src, size)
#define FAudio_memmove(dst, src, size) memmove(dst, src, size)
#define FAudio_memcmp(ptr1, ptr2, size) memcmp(ptr1, ptr2, size)

#define FAudio_strlen(ptr) strlen(ptr)
#define FAudio_strcmp(str1, str2) strcmp(str1, str2)
#define FAudio_strlcpy(ptr1, ptr2, size) strlcpy(ptr1, ptr2, size)

#define FAudio_pow(x, y) pow(x, y)
#define FAudio_log(x) log(x)
#define FAudio_log10(x) log10(x)
#define FAudio_sin(x) sin(x)
#define FAudio_cos(x) cos(x)
#define FAudio_tan(x) tan(x)
#define FAudio_acos(x) acos(x)
#define FAudio_ceil(x) ceil(x)
#define FAudio_floor(x) floor(x)
#define FAudio_abs(x) abs(x)
#define FAudio_ldexp(v, e) ldexp(v, e)
#define FAudio_exp(x) exp(x)

#define FAudio_cosf(x) cosf(x)
#define FAudio_sinf(x) sinf(x)
#define FAudio_sqrtf(x) sqrtf(x)
#define FAudio_acosf(x) acosf(x)
#define FAudio_atan2f(y, x) atan2f(y, x)
#define FAudio_fabsf(x) fabsf(x)

#define FAudio_qsort qsort

#define FAudio_assert assert
#else
#include <SDL2/SDL_stdinc.h> /* Wine change! */
#include <SDL2/SDL_assert.h> /* Wine change! */

#define FAudio_malloc SDL_malloc
#define FAudio_realloc SDL_realloc
#define FAudio_free SDL_free
#define FAudio_alloca(x) SDL_stack_alloc(uint8_t, x)
#define FAudio_dealloca(x) SDL_stack_free(x)
#define FAudio_zero(ptr, size) SDL_memset(ptr, '\0', size)
#define FAudio_memset(ptr, val, size) SDL_memset(ptr, val, size)
#define FAudio_memcpy(dst, src, size) SDL_memcpy(dst, src, size)
#define FAudio_memmove(dst, src, size) SDL_memmove(dst, src, size)
#define FAudio_memcmp(ptr1, ptr2, size) SDL_memcmp(ptr1, ptr2, size)

#define FAudio_strlen(ptr) SDL_strlen(ptr)
#define FAudio_strcmp(str1, str2) SDL_strcmp(str1, str2)
#define FAudio_strlcpy(ptr1, ptr2, size) SDL_strlcpy(ptr1, ptr2, size)

#define FAudio_pow(x, y) SDL_pow(x, y)
#define FAudio_log(x) SDL_log(x)
#define FAudio_log10(x) SDL_log10(x)
#define FAudio_sin(x) SDL_sin(x)
#define FAudio_cos(x) SDL_cos(x)
#define FAudio_tan(x) SDL_tan(x)
#define FAudio_acos(x) SDL_acos(x)
#define FAudio_ceil(x) SDL_ceil(x)
#define FAudio_floor(x) SDL_floor(x)
#define FAudio_abs(x) SDL_abs(x)
#define FAudio_ldexp(v, e) SDL_scalbn(v, e)
#define FAudio_exp(x) exp(x) /* TODO: SDL_exp */

#define FAudio_cosf(x) SDL_cosf(x)
#define FAudio_sinf(x) SDL_sinf(x)
#define FAudio_sqrtf(x) SDL_sqrtf(x)
#define FAudio_acosf(x) SDL_acosf(x)
#define FAudio_atan2f(y, x) SDL_atan2f(y, x)
#define FAudio_fabsf(x) SDL_fabsf(x)

#define FAudio_qsort SDL_qsort

#define FAudio_assert SDL_assert
#endif

/* Easy Macros */
#define FAudio_min(val1, val2) \
	(val1 < val2 ? val1 : val2)
#define FAudio_max(val1, val2) \
	(val1 > val2 ? val1 : val2)
#define FAudio_clamp(val, min, max) \
	(val > max ? max : (val < min ? min : val))

/* Windows/Visual Studio cruft */
#ifdef _WIN32
	#ifdef __cplusplus
		/* C++ should have `inline`, but not `restrict` */
		#define restrict
	#else
		#define inline __inline
		#if defined(_MSC_VER)
			#if (_MSC_VER >= 1700) /* VS2012+ */
				#define restrict __restrict
			#else /* VS2010- */
				#define restrict
			#endif
		#endif
	#endif
#endif

/* C++ does not have restrict (though VS2012+ does have __restrict) */
#if defined(__cplusplus) && !defined(restrict)
#define restrict
#endif

/* Threading Types */

typedef void* FAudioThread;
typedef void* FAudioMutex;
typedef int32_t (FAUDIOCALL * FAudioThreadFunc)(void* data);
typedef enum FAudioThreadPriority
{
	FAUDIO_THREAD_PRIORITY_LOW,
	FAUDIO_THREAD_PRIORITY_NORMAL,
	FAUDIO_THREAD_PRIORITY_HIGH,
} FAudioThreadPriority;

/* Linked Lists */

typedef struct LinkedList LinkedList;
struct LinkedList
{
	void* entry;
	LinkedList *next;
};
void LinkedList_AddEntry(
	LinkedList **start,
	void* toAdd,
	FAudioMutex lock,
	FAudioMallocFunc pMalloc
);
void LinkedList_PrependEntry(
	LinkedList **start,
	void* toAdd,
	FAudioMutex lock,
	FAudioMallocFunc pMalloc
);
void LinkedList_RemoveEntry(
	LinkedList **start,
	void* toRemove,
	FAudioMutex lock,
	FAudioFreeFunc pFree
);

/* Internal FAudio Types */

typedef enum FAudioVoiceType
{
	FAUDIO_VOICE_SOURCE,
	FAUDIO_VOICE_SUBMIX,
	FAUDIO_VOICE_MASTER
} FAudioVoiceType;

typedef struct FAudioBufferEntry FAudioBufferEntry;
struct FAudioBufferEntry
{
	FAudioBuffer buffer;
#ifdef HAVE_FFMPEG
	FAudioBufferWMA bufferWMA;
#endif /* HAVE_FFMPEG */
	FAudioBufferEntry *next;
};

typedef void (FAUDIOCALL * FAudioDecodeCallback)(
	FAudioVoice *voice,
	FAudioBuffer *buffer,	/* Buffer to decode */
	uint32_t *samples,	/* On in/output, requested/actual number of samples decoded */
	uint32_t end,		/* Must decode no further than this offset */
	float *decodeCache	/* Decode into here */
);

typedef void (FAUDIOCALL * FAudioResampleCallback)(
	float *restrict dCache,
	float *restrict resampleCache,
	uint64_t *resampleOffset,
	uint64_t resampleStep,
	uint64_t toResample,
	uint8_t channels
);

typedef void (FAUDIOCALL * FAudioMixCallback)(
	uint32_t toMix,
	uint32_t srcChans,
	uint32_t dstChans,
	float baseVolume,
	float *restrict srcData,
	float *restrict dstData,
	float *restrict channelVolume,
	float *restrict coefficients
);

typedef void* FAudioPlatformFixedRateSRC;

typedef float FAudioFilterState[4];

/* Public FAudio Types */

struct FAudio
{
	uint8_t version;
	uint8_t active;
	uint32_t refcount;
	uint32_t updateSize;
	FAudioMasteringVoice *master;
	LinkedList *sources;
	LinkedList *submixes;
	LinkedList *callbacks;
	FAudioMutex sourceLock;
	FAudioMutex submixLock;
	FAudioMutex callbackLock;
	FAudioWaveFormatExtensible *mixFormat;

	/* Temp storage for processing, interleaved PCM32F */
	#define EXTRA_DECODE_PADDING 2
	uint32_t decodeSamples;
	uint32_t resampleSamples;
	uint32_t effectChainSamples;
	float *decodeCache;
	float *resampleCache;
	float *effectChainCache;

	/* Allocator callbacks */
	FAudioMallocFunc pMalloc;
	FAudioFreeFunc pFree;
	FAudioReallocFunc pRealloc;
};

struct FAudioVoice
{
	FAudio *audio;
	uint32_t flags;
	FAudioVoiceType type;

	FAudioVoiceSends sends;
	float **sendCoefficients;
	FAudioMixCallback *sendMix;
	FAudioFilterParameters *sendFilter;
	FAudioFilterState **sendFilterState;
	struct
	{
		uint32_t count;
		FAudioEffectDescriptor *desc;
		void **parameters;
		uint32_t *parameterSizes;
		uint8_t *parameterUpdates;
		uint8_t *inPlaceProcessing;
	} effects;
	FAudioFilterParameters filter;
	FAudioFilterState *filterState;
	FAudioMutex sendLock;
	FAudioMutex effectLock;
	FAudioMutex filterLock;

	float volume;
	float *channelVolume;
	uint32_t outputChannels;
	FAudioMutex volumeLock;

	union
	{
		struct
		{
			/* Sample storage */
			uint32_t decodeSamples;
			uint32_t resampleSamples;

			/* Resampler */
			float resampleFreq;
			uint64_t resampleStep;
			uint64_t resampleOffset;
			uint64_t curBufferOffsetDec;
			uint32_t curBufferOffset;

			/* FFmpeg */
#ifdef HAVE_FFMPEG
			struct FAudioFFmpeg *ffmpeg;
#endif /* HAVE_FFMPEG*/

			/* Read-only */
			float maxFreqRatio;
			FAudioWaveFormatEx *format;
			FAudioDecodeCallback decode;
			FAudioResampleCallback resample;
			FAudioVoiceCallback *callback;

			/* Dynamic */
			uint8_t active;
			float freqRatio;
			uint8_t newBuffer;
			uint64_t totalSamples;
			FAudioBufferEntry *bufferList;
			FAudioMutex bufferLock;
		} src;
		struct
		{
			/* Sample storage */
			uint32_t inputSamples;
			uint32_t outputSamples;
			float *inputCache;
			FAudioPlatformFixedRateSRC resampler;

			/* Read-only */
			uint32_t inputChannels;
			uint32_t inputSampleRate;
			uint32_t processingStage;
		} mix;
		struct
		{
			/* Output stream, allocated by Platform */
			float *output;

			/* Read-only */
			uint32_t inputChannels;
			uint32_t inputSampleRate;
		} master;
	};
};

/* Internal Functions */
void FAudio_INTERNAL_InsertSubmixSorted(
	LinkedList **start,
	FAudioSubmixVoice *toAdd,
	FAudioMutex lock,
	FAudioMallocFunc pMalloc
);
void FAudio_INTERNAL_UpdateEngine(FAudio *audio, float *output);
void FAudio_INTERNAL_ResizeDecodeCache(FAudio *audio, uint32_t size);
void FAudio_INTERNAL_ResizeResampleCache(FAudio *audio, uint32_t size);
void FAudio_INTERNAL_ResizeEffectChainCache(FAudio *audio, uint32_t samples);
void FAudio_INTERNAL_SetDefaultMatrix(
	float *matrix,
	uint32_t srcChannels,
	uint32_t dstChannels
);
void FAudio_INTERNAL_AllocEffectChain(
	FAudioVoice *voice,
	const FAudioEffectChain *pEffectChain
);
void FAudio_INTERNAL_FreeEffectChain(FAudioVoice *voice);

/* SIMD Stuff */

/* Callbacks declared as functions (rather than function pointers) are
 * scalar-only, for now. SIMD versions should be possible for these.
 */

extern void (*FAudio_INTERNAL_Convert_U8_To_F32)(
	const uint8_t *restrict src,
	float *restrict dst,
	uint32_t len
);
extern void (*FAudio_INTERNAL_Convert_S16_To_F32)(
	const int16_t *restrict src,
	float *restrict dst,
	uint32_t len
);

extern FAudioResampleCallback FAudio_INTERNAL_ResampleMono;
extern FAudioResampleCallback FAudio_INTERNAL_ResampleStereo;
extern void FAudio_INTERNAL_ResampleGeneric(
	float *restrict dCache,
	float *restrict resampleCache,
	uint64_t *resampleOffset,
	uint64_t resampleStep,
	uint64_t toResample,
	uint8_t channels
);

extern void (*FAudio_INTERNAL_Amplify)(
	float *output,
	uint32_t totalSamples,
	float volume
);

#define MIX_FUNC(type) \
	extern void FAudio_INTERNAL_Mix_##type##_Scalar( \
		uint32_t toMix, \
		uint32_t srcChans, \
		uint32_t dstChans, \
		float baseVolume, \
		float *restrict srcData, \
		float *restrict dstData, \
		float *restrict channelVolume, \
		float *restrict coefficients \
	);
MIX_FUNC(Generic)
MIX_FUNC(1in_1out)
MIX_FUNC(1in_2out)
MIX_FUNC(1in_6out)
MIX_FUNC(2in_1out)
MIX_FUNC(2in_2out)
MIX_FUNC(2in_6out)
#undef MIX_FUNC

void FAudio_INTERNAL_InitSIMDFunctions(uint8_t hasSSE2, uint8_t hasNEON);

/* Decoders */

#define DECODE_FUNC(type) \
	extern void FAudio_INTERNAL_Decode##type( \
		FAudioVoice *voice, \
		FAudioBuffer *buffer, \
		uint32_t *samples, \
		uint32_t end, \
		float *decodeCache \
	);
DECODE_FUNC(PCM8)
DECODE_FUNC(PCM16)
DECODE_FUNC(PCM32F)
DECODE_FUNC(MonoMSADPCM)
DECODE_FUNC(StereoMSADPCM)
DECODE_FUNC(FFMPEG)
#undef DECODE_FUNC

/* FFmpeg */

#ifdef HAVE_FFMPEG
uint32_t FAudio_FFMPEG_init(FAudioSourceVoice *pSourceVoice);
void FAudio_FFMPEG_free(FAudioSourceVoice *voice);
#endif /* HAVE_FFMPEG */

/* Platform Functions */

void FAudio_PlatformAddRef(void);
void FAudio_PlatformRelease(void);
void FAudio_PlatformInit(FAudio *audio, uint32_t deviceIndex);
void FAudio_PlatformQuit(FAudio *audio);

uint32_t FAudio_PlatformGetDeviceCount(void);
void FAudio_PlatformGetDeviceDetails(
	uint32_t index,
	FAudioDeviceDetails *details
);

FAudioPlatformFixedRateSRC FAudio_PlatformInitFixedRateSRC(
	uint32_t channels,
	uint32_t inputRate,
	uint32_t outputRate
);
void FAudio_PlatformCloseFixedRateSRC(FAudioPlatformFixedRateSRC resampler);
uint32_t FAudio_PlatformResample(
	FAudioPlatformFixedRateSRC resampler,
	float *input,
	uint32_t inLen,
	float *output,
	uint32_t outLen
);

/* Threading */

FAudioThread FAudio_PlatformCreateThread(
	FAudioThreadFunc func,
	const char *name,
	void* data
);
void FAudio_PlatformWaitThread(FAudioThread thread, int32_t *retval);
void FAudio_PlatformThreadPriority(FAudioThreadPriority priority);
FAudioMutex FAudio_PlatformCreateMutex(void);
void FAudio_PlatformDestroyMutex(FAudioMutex mutex);
void FAudio_PlatformLockMutex(FAudioMutex mutex);
void FAudio_PlatformUnlockMutex(FAudioMutex mutex);
void FAudio_sleep(uint32_t ms);

/* Time */

uint32_t FAudio_timems(void);

/* Resampling */

/* Okay, so here's what all this fixed-point goo is for:
 *
 * Inevitably you're going to run into weird sample rates,
 * both from WaveBank data and from pitch shifting changes.
 *
 * How we deal with this is by calculating a fixed "step"
 * value that steps from sample to sample at the speed needed
 * to get the correct output sample rate, and the offset
 * is stored as separate integer and fraction values.
 *
 * This allows us to do weird fractional steps between samples,
 * while at the same time not letting it drift off into death
 * thanks to floating point madness.
 *
 * Steps are stored in fixed-point with 32 bits for the fraction:
 *
 * 00000000000000000000000000000000 00000000000000000000000000000000
 * ^ Integer block (32)             ^ Fraction block (32)
 *
 * For example, to get 1.5:
 * 00000000000000000000000000000001 10000000000000000000000000000000
 *
 * The Integer block works exactly like you'd expect.
 * The Fraction block is divided by the Integer's "One" value.
 * So, the above Fraction represented visually...
 *   1 << 31
 *   -------
 *   1 << 32
 * ... which, simplified, is...
 *   1 << 0
 *   ------
 *   1 << 1
 * ... in other words, 1 / 2, or 0.5.
 */
#define FIXED_PRECISION		32
#define FIXED_ONE		(1LL << FIXED_PRECISION)

/* Quick way to drop parts */
#define FIXED_FRACTION_MASK	(FIXED_ONE - 1)
#define FIXED_INTEGER_MASK	~FIXED_FRACTION_MASK

/* Helper macros to convert fixed to float */
#define DOUBLE_TO_FIXED(dbl) \
	((uint64_t) (dbl * FIXED_ONE + 0.5))
#define FIXED_TO_DOUBLE(fxd) ( \
	(double) (fxd >> FIXED_PRECISION) + /* Integer part */ \
	((fxd & FIXED_FRACTION_MASK) * (1.0 / FIXED_ONE)) /* Fraction part */ \
)
#define FIXED_TO_FLOAT(fxd) ( \
	(float) (fxd >> FIXED_PRECISION) + /* Integer part */ \
	((fxd & FIXED_FRACTION_MASK) * (1.0f / FIXED_ONE)) /* Fraction part */ \
)
