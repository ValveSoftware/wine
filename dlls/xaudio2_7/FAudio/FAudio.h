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

#ifndef FAUDIO_H
#define FAUDIO_H

#ifdef _WIN32
#define FAUDIOAPI __declspec(dllexport)
#define FAUDIOCALL __cdecl
#else
#define FAUDIOAPI
#define FAUDIOCALL
#endif

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Type Declarations */

typedef struct FAudio FAudio;
typedef struct FAudioVoice FAudioVoice;
typedef FAudioVoice FAudioSourceVoice;
typedef FAudioVoice FAudioSubmixVoice;
typedef FAudioVoice FAudioMasteringVoice;
typedef struct FAudioEngineCallback FAudioEngineCallback;
typedef struct FAudioVoiceCallback FAudioVoiceCallback;

/* Enumerations */

typedef enum FAudioDeviceRole
{
	FAudioNotDefaultDevice =		0x0,
	FAudioDefaultConsoleDevice =		0x1,
	FAudioDefaultMultimediaDevice =		0x2,
	FAudioDefaultCommunicationsDevice =	0x4,
	FAudioDefaultGameDevice =		0x8,
	FAudioGlobalDefaultDevice =		0xF,
	FAudioInvalidDeviceRole = ~FAudioGlobalDefaultDevice
} FAudioDeviceRole;

typedef enum FAudioFilterType
{
	FAudioLowPassFilter,
	FAudioBandPassFilter,
	FAudioHighPassFilter,
	FAudioNotchFilter
} FAudioFilterType;

typedef enum FAudioStreamCategory
{
	FAudioStreamCategory_Other,
	FAudioStreamCategory_ForegroundOnlyMedia,
	FAudioStreamCategory_BackgroundCapableMedia,
	FAudioStreamCategory_Communications,
	FAudioStreamCategory_Alerts,
	FAudioStreamCategory_SoundEffects,
	FAudioStreamCategory_GameEffects,
	FAudioStreamCategory_GameMedia,
	FAudioStreamCategory_GameChat,
	FAudioStreamCategory_Speech,
	FAudioStreamCategory_Movie,
	FAudioStreamCategory_Media
} FAudioStreamCategory;

/* FIXME: The original enum violates ISO C and is platform specific anyway... */
typedef uint32_t FAudioProcessor;
#define FAUDIO_DEFAULT_PROCESSOR 0xFFFFFFFF

/* Structures */

#pragma pack(push, 1)

typedef struct FAudioGUID
{
	uint32_t Data1;
	uint16_t Data2;
	uint16_t Data3;
	uint8_t Data4[8];
} FAudioGUID;

typedef struct FAudioWaveFormatEx
{
	uint16_t wFormatTag;
	uint16_t nChannels;
	uint32_t nSamplesPerSec;
	uint32_t nAvgBytesPerSec;
	uint16_t nBlockAlign;
	uint16_t wBitsPerSample;
	uint16_t cbSize;
} FAudioWaveFormatEx;

typedef struct FAudioWaveFormatExtensible
{
	FAudioWaveFormatEx Format;
	union
	{
		uint16_t wValidBitsPerSample;
		uint16_t wSamplesPerBlock;
		uint16_t wReserved;
	} Samples;
	uint32_t dwChannelMask;
	FAudioGUID SubFormat;
} FAudioWaveFormatExtensible;

typedef struct FAudioADPCMCoefSet
{
	int16_t iCoef1;
	int16_t iCoef2;
} FAudioADPCMCoefSet;

typedef struct FAudioADPCMWaveFormat
{
	FAudioWaveFormatEx wfx;
	uint16_t wSamplesPerBlock;
	uint16_t wNumCoef;
	FAudioADPCMCoefSet aCoef[];
	/* MSADPCM has 7 coefficient pairs:
	 * {
	 *	{ 256,    0 },
	 *	{ 512, -256 },
	 *	{   0,    0 },
	 *	{ 192,   64 },
	 *	{ 240,    0 },
	 *	{ 460, -208 },
	 *	{ 392, -232 }
	 * }
	 */
} FAudioADPCMWaveFormat;

typedef struct FAudioDeviceDetails
{
	int16_t DeviceID[256]; /* Win32 wchar_t */
	int16_t DisplayName[256]; /* Win32 wchar_t */
	FAudioDeviceRole Role;
	FAudioWaveFormatExtensible OutputFormat;
} FAudioDeviceDetails;

typedef struct FAudioVoiceDetails
{
	uint32_t CreationFlags;
	uint32_t ActiveFlags;
	uint32_t InputChannels;
	uint32_t InputSampleRate;
} FAudioVoiceDetails;

typedef struct FAudioSendDescriptor
{
	uint32_t Flags;
	FAudioVoice *pOutputVoice;
} FAudioSendDescriptor;

typedef struct FAudioVoiceSends
{
	uint32_t SendCount;
	FAudioSendDescriptor *pSends;
} FAudioVoiceSends;

struct FAPO;
typedef struct FAPO FAPO;

typedef struct FAudioEffectDescriptor
{
	FAPO *pEffect;
	uint8_t InitialState;
	uint32_t OutputChannels;
} FAudioEffectDescriptor;

typedef struct FAudioEffectChain
{
	uint32_t EffectCount;
	FAudioEffectDescriptor *pEffectDescriptors;
} FAudioEffectChain;

typedef struct FAudioFilterParameters
{
	FAudioFilterType Type;
	float Frequency;
	float OneOverQ;
} FAudioFilterParameters;

typedef struct FAudioBuffer
{
	uint32_t Flags;
	uint32_t AudioBytes;
	const uint8_t *pAudioData;
	uint32_t PlayBegin;
	uint32_t PlayLength;
	uint32_t LoopBegin;
	uint32_t LoopLength;
	uint32_t LoopCount;
	void *pContext;
} FAudioBuffer;

typedef struct FAudioBufferWMA
{
	const uint32_t *pDecodedPacketCumulativeBytes;
	uint32_t PacketCount;
} FAudioBufferWMA;

typedef struct FAudioVoiceState
{
	void *pCurrentBufferContext;
	uint32_t BuffersQueued;
	uint64_t SamplesPlayed;
} FAudioVoiceState;

typedef struct FAudioPerformanceData
{
	uint64_t AudioCyclesSinceLastQuery;
	uint64_t TotalCyclesSinceLastQuery;
	uint32_t MinimumCyclesPerQuantum;
	uint32_t MaximumCyclesPerQuantum;
	uint32_t MemoryUsageInBytes;
	uint32_t CurrentLatencyInSamples;
	uint32_t GlitchesSinceEngineStarted;
	uint32_t ActiveSourceVoiceCount;
	uint32_t TotalSourceVoiceCount;
	uint32_t ActiveSubmixVoiceCount;
	uint32_t ActiveResamplerCount;
	uint32_t ActiveMatrixMixCount;
	uint32_t ActiveXmaSourceVoices;
	uint32_t ActiveXmaStreams;
} FAudioPerformanceData;

typedef struct FAudioDebugConfiguration
{
	uint32_t TraceMask;
	uint32_t BreakMask;
	uint8_t LogThreadID;
	uint8_t LogFileline;
	uint8_t LogFunctionName;
	uint8_t LogTiming;
} FAudioDebugConfiguration;

#pragma pack(pop)

/* Constants */

#define FAUDIO_E_OUT_OF_MEMORY		0x8007000e
#define FAUDIO_E_INVALID_ARG		0x80070057
#define FAUDIO_E_UNSUPPORTED_FORMAT	0x88890008
#define FAUDIO_E_INVALID_CALL		0x88960001
#define FAUDIO_E_DEVICE_INVALIDATED	0x88960004
#define FAPO_E_FORMAT_UNSUPPORTED	0x88970001

#define FAUDIO_MAX_BUFFER_BYTES		0x80000000
#define FAUDIO_MAX_QUEUED_BUFFERS	64
#define FAUDIO_MAX_AUDIO_CHANNELS	64
#define FAUDIO_MIN_SAMPLE_RATE		1000
#define FAUDIO_MAX_SAMPLE_RATE		200000
#define FAUDIO_MAX_VOLUME_LEVEL		16777216.0f
#define FAUDIO_MIN_FREQ_RATIO		(1.0f / 1024.0f)
#define FAUDIO_MAX_FREQ_RATIO		1024.0f
#define FAUDIO_DEFAULT_FREQ_RATIO	2.0f
#define FAUDIO_MAX_FILTER_ONEOVERQ	1.5f
#define FAUDIO_MAX_FILTER_FREQUENCY	1.0f
#define FAUDIO_MAX_LOOP_COUNT		254

#define FAUDIO_COMMIT_NOW		0
#define FAUDIO_COMMIT_ALL		0
#define FAUDIO_INVALID_OPSET		(uint32_t) (-1)
#define FAUDIO_NO_LOOP_REGION		0
#define FAUDIO_LOOP_INFINITE		255
#define FAUDIO_DEFAULT_CHANNELS		0
#define FAUDIO_DEFAULT_SAMPLERATE	0

#define FAUDIO_DEBUG_ENGINE		0x01
#define FAUDIO_VOICE_NOPITCH		0x02
#define FAUDIO_VOICE_NOSRC		0x04
#define FAUDIO_VOICE_USEFILTER		0x08
#define FAUDIO_VOICE_MUSIC		0x10
#define FAUDIO_PLAY_TAILS		0x20
#define FAUDIO_END_OF_STREAM		0x40
#define FAUDIO_SEND_USEFILTER		0x80

#define FAUDIO_VOICE_NOSAMPLESPLAYED	0x0100

#define FAUDIO_DEFAULT_FILTER_TYPE	FAudioLowPassFilter
#define FAUDIO_DEFAULT_FILTER_FREQUENCY	FAUDIO_MAX_FILTER_FREQUENCY
#define FAUDIO_DEFAULT_FILTER_ONEOVERQ	1.0f

#define FAUDIO_LOG_ERRORS		0x0001
#define FAUDIO_LOG_WARNINGS		0x0002
#define FAUDIO_LOG_INFO			0x0004
#define FAUDIO_LOG_DETAIL		0x0008
#define FAUDIO_LOG_API_CALLS		0x0010
#define FAUDIO_LOG_FUNC_CALLS		0x0020
#define FAUDIO_LOG_TIMING		0x0040
#define FAUDIO_LOG_LOCKS		0x0080
#define FAUDIO_LOG_MEMORY		0x0100
#define FAUDIO_LOG_STREAMING		0x1000

#ifndef _SPEAKER_POSITIONS_
#define SPEAKER_FRONT_LEFT		0x00000001
#define SPEAKER_FRONT_RIGHT		0x00000002
#define SPEAKER_FRONT_CENTER		0x00000004
#define SPEAKER_LOW_FREQUENCY		0x00000008
#define SPEAKER_BACK_LEFT		0x00000010
#define SPEAKER_BACK_RIGHT		0x00000020
#define SPEAKER_FRONT_LEFT_OF_CENTER	0x00000040
#define SPEAKER_FRONT_RIGHT_OF_CENTER	0x00000080
#define SPEAKER_BACK_CENTER		0x00000100
#define SPEAKER_SIDE_LEFT		0x00000200
#define SPEAKER_SIDE_RIGHT		0x00000400
#define SPEAKER_TOP_CENTER		0x00000800
#define SPEAKER_TOP_FRONT_LEFT		0x00001000
#define SPEAKER_TOP_FRONT_CENTER	0x00002000
#define SPEAKER_TOP_FRONT_RIGHT		0x00004000
#define SPEAKER_TOP_BACK_LEFT		0x00008000
#define SPEAKER_TOP_BACK_CENTER		0x00010000
#define SPEAKER_TOP_BACK_RIGHT		0x00020000
#define _SPEAKER_POSITIONS_
#endif

#ifndef _SPEAKER_COMBINATIONS_
#define SPEAKER_MONO	SPEAKER_FRONT_CENTER
#define SPEAKER_STEREO	(SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT)
#define SPEAKER_2POINT1 \
	(	SPEAKER_FRONT_LEFT	| \
		SPEAKER_FRONT_RIGHT	| \
		SPEAKER_LOW_FREQUENCY	)
#define SPEAKER_SURROUND \
	(	SPEAKER_FRONT_LEFT	| \
		SPEAKER_FRONT_RIGHT	| \
		SPEAKER_FRONT_CENTER	| \
		SPEAKER_BACK_CENTER	)
#define SPEAKER_QUAD \
	(	SPEAKER_FRONT_LEFT	| \
		SPEAKER_FRONT_RIGHT	| \
		SPEAKER_BACK_LEFT	| \
		SPEAKER_BACK_RIGHT	)
#define SPEAKER_4POINT1 \
	(	SPEAKER_FRONT_LEFT	| \
		SPEAKER_FRONT_RIGHT	| \
		SPEAKER_LOW_FREQUENCY	| \
		SPEAKER_BACK_LEFT	| \
		SPEAKER_BACK_RIGHT	)
#define SPEAKER_5POINT1 \
	(	SPEAKER_FRONT_LEFT	| \
		SPEAKER_FRONT_RIGHT	| \
		SPEAKER_FRONT_CENTER	| \
		SPEAKER_LOW_FREQUENCY	| \
		SPEAKER_BACK_LEFT	| \
		SPEAKER_BACK_RIGHT	)
#define SPEAKER_7POINT1 \
	(	SPEAKER_FRONT_LEFT		| \
		SPEAKER_FRONT_RIGHT		| \
		SPEAKER_FRONT_CENTER		| \
		SPEAKER_LOW_FREQUENCY		| \
		SPEAKER_BACK_LEFT		| \
		SPEAKER_BACK_RIGHT		| \
		SPEAKER_FRONT_LEFT_OF_CENTER	| \
		SPEAKER_FRONT_RIGHT_OF_CENTER	)
#define SPEAKER_5POINT1_SURROUND \
	(	SPEAKER_FRONT_LEFT	| \
		SPEAKER_FRONT_RIGHT	| \
		SPEAKER_FRONT_CENTER	| \
		SPEAKER_LOW_FREQUENCY	| \
		SPEAKER_SIDE_LEFT	| \
		SPEAKER_SIDE_RIGHT	)
#define SPEAKER_7POINT1_SURROUND \
	(	SPEAKER_FRONT_LEFT	| \
		SPEAKER_FRONT_RIGHT	| \
		SPEAKER_FRONT_CENTER	| \
		SPEAKER_LOW_FREQUENCY	| \
		SPEAKER_BACK_LEFT	| \
		SPEAKER_BACK_RIGHT	| \
		SPEAKER_SIDE_LEFT	| \
		SPEAKER_SIDE_RIGHT	)
#define SPEAKER_XBOX SPEAKER_5POINT1
#define _SPEAKER_COMBINATIONS_
#endif

#define FAUDIO_FORMAT_PCM		1
#define FAUDIO_FORMAT_MSADPCM		2
#define FAUDIO_FORMAT_IEEE_FLOAT	3
#define FAUDIO_FORMAT_WMAUDIO2		0x0161
#define FAUDIO_FORMAT_EXTENSIBLE	0xFFFE

extern FAudioGUID DATAFORMAT_SUBTYPE_PCM;
extern FAudioGUID DATAFORMAT_SUBTYPE_IEEE_FLOAT;

/* FAudio Interface */

FAUDIOAPI uint32_t FAudioCreate(
	FAudio **ppFAudio,
	uint32_t Flags,
	FAudioProcessor XAudio2Processor
);

#define FAUDIO_TARGET_VERSION 8 /* targeting compatibility with XAudio 2.8 */

/* See "extensions/COMConstructEXT.txt" for more details */
FAUDIOAPI uint32_t FAudioCOMConstructEXT(FAudio **ppFAudio, uint8_t version);

FAUDIOAPI uint32_t FAudio_AddRef(FAudio *audio);

FAUDIOAPI uint32_t FAudio_Release(FAudio *audio);

/* FIXME: QueryInterface? Or just ignore COM garbage... -flibit */

FAUDIOAPI uint32_t FAudio_GetDeviceCount(FAudio *audio, uint32_t *pCount);

FAUDIOAPI uint32_t FAudio_GetDeviceDetails(
	FAudio *audio,
	uint32_t Index,
	FAudioDeviceDetails *pDeviceDetails
);

FAUDIOAPI uint32_t FAudio_Initialize(
	FAudio *audio,
	uint32_t Flags,
	FAudioProcessor XAudio2Processor
);

FAUDIOAPI uint32_t FAudio_RegisterForCallbacks(
	FAudio *audio,
	FAudioEngineCallback *pCallback
);

FAUDIOAPI void FAudio_UnregisterForCallbacks(
	FAudio *audio,
	FAudioEngineCallback *pCallback
);

FAUDIOAPI uint32_t FAudio_CreateSourceVoice(
	FAudio *audio,
	FAudioSourceVoice **ppSourceVoice,
	const FAudioWaveFormatEx *pSourceFormat,
	uint32_t Flags,
	float MaxFrequencyRatio,
	FAudioVoiceCallback *pCallback,
	const FAudioVoiceSends *pSendList,
	const FAudioEffectChain *pEffectChain
);

FAUDIOAPI uint32_t FAudio_CreateSubmixVoice(
	FAudio *audio,
	FAudioSubmixVoice **ppSubmixVoice,
	uint32_t InputChannels,
	uint32_t InputSampleRate,
	uint32_t Flags,
	uint32_t ProcessingStage,
	const FAudioVoiceSends *pSendList,
	const FAudioEffectChain *pEffectChain
);

FAUDIOAPI uint32_t FAudio_CreateMasteringVoice(
	FAudio *audio,
	FAudioMasteringVoice **ppMasteringVoice,
	uint32_t InputChannels,
	uint32_t InputSampleRate,
	uint32_t Flags,
	uint32_t DeviceIndex,
	const FAudioEffectChain *pEffectChain
);

FAUDIOAPI uint32_t FAudio_CreateMasteringVoice8(
	FAudio *audio,
	FAudioMasteringVoice **ppMasteringVoice,
	uint32_t InputChannels,
	uint32_t InputSampleRate,
	uint32_t Flags,
	uint16_t *szDeviceId,
	const FAudioEffectChain *pEffectChain,
	FAudioStreamCategory StreamCategory
);

FAUDIOAPI uint32_t FAudio_StartEngine(FAudio *audio);

FAUDIOAPI void FAudio_StopEngine(FAudio *audio);

FAUDIOAPI uint32_t FAudio_CommitChanges(FAudio *audio);

FAUDIOAPI void FAudio_GetPerformanceData(
	FAudio *audio,
	FAudioPerformanceData *pPerfData
);

FAUDIOAPI void FAudio_SetDebugConfiguration(
	FAudio *audio,
	FAudioDebugConfiguration *pDebugConfiguration,
	void* pReserved
);

/* FAudioVoice Interface */

FAUDIOAPI void FAudioVoice_GetVoiceDetails(
	FAudioVoice *voice,
	FAudioVoiceDetails *pVoiceDetails
);

FAUDIOAPI uint32_t FAudioVoice_SetOutputVoices(
	FAudioVoice *voice,
	const FAudioVoiceSends *pSendList
);

FAUDIOAPI uint32_t FAudioVoice_SetEffectChain(
	FAudioVoice *voice,
	const FAudioEffectChain *pEffectChain
);

FAUDIOAPI uint32_t FAudioVoice_EnableEffect(
	FAudioVoice *voice,
	uint32_t EffectIndex,
	uint32_t OperationSet
);

FAUDIOAPI uint32_t FAudioVoice_DisableEffect(
	FAudioVoice *voice,
	uint32_t EffectIndex,
	uint32_t OperationSet
);

FAUDIOAPI void FAudioVoice_GetEffectState(
	FAudioVoice *voice,
	uint32_t EffectIndex,
	uint8_t *pEnabled
);

FAUDIOAPI uint32_t FAudioVoice_SetEffectParameters(
	FAudioVoice *voice,
	uint32_t EffectIndex,
	const void *pParameters,
	uint32_t ParametersByteSize,
	uint32_t OperationSet
);

FAUDIOAPI uint32_t FAudioVoice_GetEffectParameters(
	FAudioVoice *voice,
	uint32_t EffectIndex,
	void *pParameters,
	uint32_t ParametersByteSize
);

FAUDIOAPI uint32_t FAudioVoice_SetFilterParameters(
	FAudioVoice *voice,
	const FAudioFilterParameters *pParameters,
	uint32_t OperationSet
);

FAUDIOAPI void FAudioVoice_GetFilterParameters(
	FAudioVoice *voice,
	FAudioFilterParameters *pParameters
);

FAUDIOAPI uint32_t FAudioVoice_SetOutputFilterParameters(
	FAudioVoice *voice,
	FAudioVoice *pDestinationVoice,
	const FAudioFilterParameters *pParameters,
	uint32_t OperationSet
);

FAUDIOAPI void FAudioVoice_GetOutputFilterParameters(
	FAudioVoice *voice,
	FAudioVoice *pDestinationVoice,
	FAudioFilterParameters *pParameters
);

FAUDIOAPI uint32_t FAudioVoice_SetVolume(
	FAudioVoice *voice,
	float Volume,
	uint32_t OperationSet
);

FAUDIOAPI void FAudioVoice_GetVolume(
	FAudioVoice *voice,
	float *pVolume
);

FAUDIOAPI uint32_t FAudioVoice_SetChannelVolumes(
	FAudioVoice *voice,
	uint32_t Channels,
	const float *pVolumes,
	uint32_t OperationSet
);

FAUDIOAPI void FAudioVoice_GetChannelVolumes(
	FAudioVoice *voice,
	uint32_t Channels,
	float *pVolumes
);

FAUDIOAPI uint32_t FAudioVoice_SetOutputMatrix(
	FAudioVoice *voice,
	FAudioVoice *pDestinationVoice,
	uint32_t SourceChannels,
	uint32_t DestinationChannels,
	const float *pLevelMatrix,
	uint32_t OperationSet
);

FAUDIOAPI void FAudioVoice_GetOutputMatrix(
	FAudioVoice *voice,
	FAudioVoice *pDestinationVoice,
	uint32_t SourceChannels,
	uint32_t DestinationChannels,
	float *pLevelMatrix
);

FAUDIOAPI void FAudioVoice_DestroyVoice(FAudioVoice *voice);

/* FAudioSourceVoice Interface */

FAUDIOAPI uint32_t FAudioSourceVoice_Start(
	FAudioSourceVoice *voice,
	uint32_t Flags,
	uint32_t OperationSet
);

FAUDIOAPI uint32_t FAudioSourceVoice_Stop(
	FAudioSourceVoice *voice,
	uint32_t Flags,
	uint32_t OperationSet
);

FAUDIOAPI uint32_t FAudioSourceVoice_SubmitSourceBuffer(
	FAudioSourceVoice *voice,
	const FAudioBuffer *pBuffer,
	const FAudioBufferWMA *pBufferWMA
);

FAUDIOAPI uint32_t FAudioSourceVoice_FlushSourceBuffers(
	FAudioSourceVoice *voice
);

FAUDIOAPI uint32_t FAudioSourceVoice_Discontinuity(
	FAudioSourceVoice *voice
);

FAUDIOAPI uint32_t FAudioSourceVoice_ExitLoop(
	FAudioSourceVoice *voice,
	uint32_t OperationSet
);

FAUDIOAPI void FAudioSourceVoice_GetState(
	FAudioSourceVoice *voice,
	FAudioVoiceState *pVoiceState,
	uint32_t flags
);

FAUDIOAPI uint32_t FAudioSourceVoice_SetFrequencyRatio(
	FAudioSourceVoice *voice,
	float Ratio,
	uint32_t OperationSet
);

FAUDIOAPI void FAudioSourceVoice_GetFrequencyRatio(
	FAudioSourceVoice *voice,
	float *pRatio
);

FAUDIOAPI uint32_t FAudioSourceVoice_SetSourceSampleRate(
	FAudioSourceVoice *voice,
	uint32_t NewSourceSampleRate
);

/* FAudioMasteringVoice Interface */

FAUDIOAPI uint32_t FAudioMasteringVoice_GetChannelMask(
	FAudioMasteringVoice *voice,
	uint32_t *pChannelMask
);

/* FAudioEngineCallback Interface */

typedef void (FAUDIOCALL * OnCriticalErrorFunc)(
	FAudioEngineCallback *callback,
	uint32_t Error
);
typedef void (FAUDIOCALL * OnProcessingPassEndFunc)(
	FAudioEngineCallback *callback
);
typedef void (FAUDIOCALL * OnProcessingPassStartFunc)(
	FAudioEngineCallback *callback
);

struct FAudioEngineCallback
{
	OnCriticalErrorFunc OnCriticalError;
	OnProcessingPassEndFunc OnProcessingPassEnd;
	OnProcessingPassStartFunc OnProcessingPassStart;
};

/* FAudioVoiceCallback Interface */

typedef void (FAUDIOCALL * OnBufferEndFunc)(
	FAudioVoiceCallback *callback,
	void *pBufferContext
);
typedef void (FAUDIOCALL * OnBufferStartFunc)(
	FAudioVoiceCallback *callback,
	void *pBufferContext
);
typedef void (FAUDIOCALL * OnLoopEndFunc)(
	FAudioVoiceCallback *callback,
	void *pBufferContext
);
typedef void (FAUDIOCALL * OnStreamEndFunc)(
	FAudioVoiceCallback *callback
);
typedef void (FAUDIOCALL * OnVoiceErrorFunc)(
	FAudioVoiceCallback *callback,
	void *pBufferContext,
	uint32_t Error
);
typedef void (FAUDIOCALL * OnVoiceProcessingPassEndFunc)(
	FAudioVoiceCallback *callback
);
typedef void (FAUDIOCALL * OnVoiceProcessingPassStartFunc)(
	FAudioVoiceCallback *callback,
	uint32_t BytesRequired
);

struct FAudioVoiceCallback
{
	OnBufferEndFunc OnBufferEnd;
	OnBufferStartFunc OnBufferStart;
	OnLoopEndFunc OnLoopEnd;
	OnStreamEndFunc OnStreamEnd;
	OnVoiceErrorFunc OnVoiceError;
	OnVoiceProcessingPassEndFunc OnVoiceProcessingPassEnd;
	OnVoiceProcessingPassStartFunc OnVoiceProcessingPassStart;
};

/* FAudio Custom Allocator API
 * See "extensions/CustomAllocatorEXT.txt" for more information.
 */

typedef void* (FAUDIOCALL * FAudioMallocFunc)(size_t size);
typedef void (FAUDIOCALL * FAudioFreeFunc)(void* ptr);
typedef void* (FAUDIOCALL * FAudioReallocFunc)(void* ptr, size_t size);

FAUDIOAPI uint32_t FAudioCreateWithCustomAllocatorEXT(
	FAudio **ppFAudio,
	uint32_t Flags,
	FAudioProcessor XAudio2Processor,
	FAudioMallocFunc customMalloc,
	FAudioFreeFunc customFree,
	FAudioReallocFunc customRealloc
);
FAUDIOAPI uint32_t FAudioCOMConstructWithCustomAllocatorEXT(
	FAudio **ppFAudio,
	uint8_t version,
	FAudioMallocFunc customMalloc,
	FAudioFreeFunc customFree,
	FAudioReallocFunc customRealloc
);

/* FAudio I/O API */

#define FAUDIO_SEEK_SET 0
#define FAUDIO_SEEK_CUR 1
#define FAUDIO_SEEK_END 2
#define FAUDIO_EOF -1

typedef size_t (FAUDIOCALL * FAudio_readfunc)(
	void *data,
	void *dst,
	size_t size,
	size_t count
);
typedef int64_t (FAUDIOCALL * FAudio_seekfunc)(
	void *data,
	int64_t offset,
	int whence
);
typedef int (FAUDIOCALL * FAudio_closefunc)(
	void *data
);

typedef struct FAudioIOStream
{
	void *data;
	FAudio_readfunc read;
	FAudio_seekfunc seek;
	FAudio_closefunc close;
} FAudioIOStream;

FAUDIOAPI FAudioIOStream* FAudio_fopen(const char *path);
FAUDIOAPI FAudioIOStream* FAudio_memopen(void *mem, int len);
FAUDIOAPI uint8_t* FAudio_memptr(FAudioIOStream *io, size_t offset);
FAUDIOAPI void FAudio_close(FAudioIOStream *io);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FAUDIO_H */
