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

#include "FAudio_internal.h"

void LinkedList_AddEntry(
	LinkedList **start,
	void* toAdd,
	FAudioMutex lock,
	FAudioMallocFunc pMalloc
) {
	LinkedList *newEntry, *latest;
	newEntry = (LinkedList*) pMalloc(sizeof(LinkedList));
	newEntry->entry = toAdd;
	newEntry->next = NULL;
	FAudio_PlatformLockMutex(lock);
	if (*start == NULL)
	{
		*start = newEntry;
	}
	else
	{
		latest = *start;
		while (latest->next != NULL)
		{
			latest = latest->next;
		}
		latest->next = newEntry;
	}
	FAudio_PlatformUnlockMutex(lock);
}

void LinkedList_PrependEntry(
	LinkedList **start,
	void* toAdd,
	FAudioMutex lock,
	FAudioMallocFunc pMalloc
) {
	LinkedList *newEntry;
	newEntry = (LinkedList*) pMalloc(sizeof(LinkedList));
	newEntry->entry = toAdd;
	FAudio_PlatformLockMutex(lock);
	newEntry->next = *start;
	*start = newEntry;
	FAudio_PlatformUnlockMutex(lock);
}

void LinkedList_RemoveEntry(
	LinkedList **start,
	void* toRemove,
	FAudioMutex lock,
	FAudioFreeFunc pFree
) {
	LinkedList *latest, *prev;
	latest = *start;
	prev = latest;
	FAudio_PlatformLockMutex(lock);
	while (latest != NULL)
	{
		if (latest->entry == toRemove)
		{
			if (latest == prev) /* First in list */
			{
				*start = latest->next;
			}
			else
			{
				prev->next = latest->next;
			}
			pFree(latest);
			FAudio_PlatformUnlockMutex(lock);
			return;
		}
		prev = latest;
		latest = latest->next;
	}
	FAudio_PlatformUnlockMutex(lock);
	FAudio_assert(0 && "LinkedList element not found!");
}

void FAudio_INTERNAL_InsertSubmixSorted(
	LinkedList **start,
	FAudioSubmixVoice *toAdd,
	FAudioMutex lock,
	FAudioMallocFunc pMalloc
) {
	LinkedList *newEntry, *latest;
	newEntry = (LinkedList*) pMalloc(sizeof(LinkedList));
	newEntry->entry = toAdd;
	newEntry->next = NULL;
	FAudio_PlatformLockMutex(lock);
	if (*start == NULL)
	{
		*start = newEntry;
	}
	else
	{
		latest = *start;
		while (	latest->next != NULL &&
			((FAudioSubmixVoice *) latest->next->entry)->mix.processingStage < toAdd->mix.processingStage	)
		{
			latest = latest->next;
		}
		newEntry->next = latest->next;
		latest->next = newEntry;
	}
	FAudio_PlatformUnlockMutex(lock);
}

static void FAudio_INTERNAL_DecodeBuffers(
	FAudioSourceVoice *voice,
	uint64_t *toDecode
) {
	uint32_t end, endRead, decoding, wanted, decoded = 0;
	FAudioBuffer *buffer = &voice->src.bufferList->buffer;
	FAudioBufferEntry *toDelete;

	/* This should never go past the max ratio size */
	FAudio_assert(*toDecode <= voice->src.decodeSamples);

	while (decoded < *toDecode && buffer != NULL)
	{
		decoding = (uint32_t) *toDecode - decoded;
		wanted = decoding;

		/* Start-of-buffer behavior */
		if (voice->src.newBuffer)
		{
			voice->src.newBuffer = 0;
			if (	voice->src.callback != NULL &&
				voice->src.callback->OnBufferStart != NULL	)
			{
				voice->src.callback->OnBufferStart(
					voice->src.callback,
					buffer->pContext
				);
			}
		}

		/* Check for end-of-buffer */
		end = (buffer->LoopCount > 0) ?
			(buffer->LoopBegin + buffer->LoopLength) :
			buffer->PlayBegin + buffer->PlayLength;

		/* Decode... */
		voice->src.decode(
			voice,
			buffer,
			&decoding,
			end,
			voice->audio->decodeCache + (
				decoded * voice->src.format->nChannels
			)
		);

		voice->src.totalSamples += decoding;

		/* End-of-buffer behavior */
		if (decoding < wanted)
		{
			if (buffer->LoopCount > 0)
			{
				voice->src.curBufferOffset = buffer->LoopBegin;
				if (buffer->LoopCount < FAUDIO_LOOP_INFINITE)
				{
					buffer->LoopCount -= 1;
				}
				if (	voice->src.callback != NULL &&
					voice->src.callback->OnLoopEnd != NULL	)
				{
					voice->src.callback->OnLoopEnd(
						voice->src.callback,
						buffer->pContext
					);
				}
			}
			else
			{
				/* For EOS we can stop storing fraction offsets */
				if (buffer->Flags & FAUDIO_END_OF_STREAM)
				{
					voice->src.curBufferOffsetDec = 0;
					voice->src.totalSamples = 0;
				}

				/* Change active buffer, delete finished buffer */
				toDelete = voice->src.bufferList;
				voice->src.bufferList = voice->src.bufferList->next;
				if (voice->src.bufferList != NULL)
				{
					buffer = &voice->src.bufferList->buffer;
					voice->src.curBufferOffset = buffer->PlayBegin;
					voice->src.newBuffer = 1;
				}
				else
				{
					buffer = NULL;

					/* FIXME: I keep going past the buffer so fuck it */
					FAudio_zero(
						voice->audio->decodeCache + (
							(decoded + decoding) *
							voice->src.format->nChannels
						),
						sizeof(float) * (
							(*toDecode - (decoded + decoding)) *
							voice->src.format->nChannels
						)
					);
				}

				/* Callbacks */
				if (voice->src.callback != NULL)
				{
					if (voice->src.callback->OnBufferEnd != NULL)
					{
						voice->src.callback->OnBufferEnd(
							voice->src.callback,
							toDelete->buffer.pContext
						);
					}
					if (	toDelete->buffer.Flags & FAUDIO_END_OF_STREAM &&
						voice->src.callback->OnStreamEnd != NULL	)
					{
						voice->src.callback->OnStreamEnd(
							voice->src.callback
						);
					}

					/* One last chance at redemption */
					if (buffer == NULL && voice->src.bufferList != NULL)
					{
						buffer = &voice->src.bufferList->buffer;
						voice->src.curBufferOffset = buffer->PlayBegin;
						voice->src.newBuffer = 1;
					}
				}

				voice->audio->pFree(toDelete);
			}
		}

		/* Finally. */
		decoded += decoding;
	}

	/* ... FIXME: I keep going past the buffer so fuck it */
	if (buffer)
	{
		end = (buffer->LoopCount > 0) ?
			(buffer->LoopBegin + buffer->LoopLength) :
			buffer->PlayBegin + buffer->PlayLength;
		endRead = EXTRA_DECODE_PADDING;

		voice->src.decode(
			voice,
			buffer,
			&endRead,
			end,
			voice->audio->decodeCache + (
				decoded * voice->src.format->nChannels
			)
		);

		if (endRead < EXTRA_DECODE_PADDING)
		{
			FAudio_zero(
				voice->audio->decodeCache + (
					decoded * voice->src.format->nChannels
				),
				sizeof(float) * (
					EXTRA_DECODE_PADDING - endRead *
					voice->src.format->nChannels
				)
			);
		}
	}
	else
	{
		FAudio_zero(
			voice->audio->decodeCache + (
				decoded * voice->src.format->nChannels
			),
			sizeof(float) * (
				EXTRA_DECODE_PADDING *
				voice->src.format->nChannels
			)
		);
	}

	*toDecode = decoded;
}

static inline void FAudio_INTERNAL_FilterVoice(
	const FAudioFilterParameters *filter,
	FAudioFilterState *filterState,
	float *samples,
	uint32_t numSamples,
	uint16_t numChannels
) {
	uint32_t j, ci;

	/* Apply a digital state-variable filter to the voice.
	 * The difference equations of the filter are:
	 *
	 * Yl(n) = F Yb(n - 1) + Yl(n - 1)
	 * Yh(n) = x(n) - Yl(n) - OneOverQ Yb(n - 1)
	 * Yb(n) = F Yh(n) + Yb(n - 1)
	 * Yn(n) = Yl(n) + Yh(n)
	 *
	 * Please note that FAudioFilterParameters.Frequency is defined as:
	 *
	 * (2 * sin(pi * (desired filter cutoff frequency) / sampleRate))
	 *
	 * - @JohanSmet
	 */

	for (j = 0; j < numSamples; j += 1)
	for (ci = 0; ci < numChannels; ci += 1)
	{
		filterState[ci][FAudioLowPassFilter] = filterState[ci][FAudioLowPassFilter] + (filter->Frequency * filterState[ci][FAudioBandPassFilter]);
		filterState[ci][FAudioHighPassFilter] = samples[j * numChannels + ci] - filterState[ci][FAudioLowPassFilter] - (filter->OneOverQ * filterState[ci][FAudioBandPassFilter]);
		filterState[ci][FAudioBandPassFilter] = (filter->Frequency * filterState[ci][FAudioHighPassFilter]) + filterState[ci][FAudioBandPassFilter];
		filterState[ci][FAudioNotchFilter] = filterState[ci][FAudioHighPassFilter] + filterState[ci][FAudioLowPassFilter];
		samples[j * numChannels + ci] = filterState[ci][filter->Type];
	}
}

static inline float *FAudio_INTERNAL_ProcessEffectChain(
	FAudioVoice *voice,
	uint32_t channels,
	uint32_t sampleRate,
	float *buffer,
	uint32_t samples
) {
	uint32_t i;
	FAPO *fapo;
	FAPOProcessBufferParameters srcParams, dstParams;

	/* Set up the buffer to be written into */
	srcParams.pBuffer = buffer;
	srcParams.BufferFlags = FAPO_BUFFER_VALID;
	srcParams.ValidFrameCount = samples;

	FAudio_memcpy(&dstParams, &srcParams, sizeof(srcParams));

	/* Update parameters, process! */
	for (i = 0; i < voice->effects.count; i += 1)
	{
		fapo = voice->effects.desc[i].pEffect;

		if (!voice->effects.inPlaceProcessing[i])
		{
			if (dstParams.pBuffer == buffer)
			{
				FAudio_INTERNAL_ResizeEffectChainCache(
					voice->audio,
					voice->effects.desc[i].OutputChannels * samples
				);
				dstParams.pBuffer = voice->audio->effectChainCache;
			}
			else
			{
				dstParams.pBuffer = buffer;
			}
		}

		if (voice->effects.parameterUpdates[i])
		{
			fapo->SetParameters(
				fapo,
				voice->effects.parameters[i],
				voice->effects.parameterSizes[i]
			);
			voice->effects.parameterUpdates[i] = 0;
		}

		fapo->Process(
			fapo,
			1,
			&srcParams,
			1,
			&dstParams,
			voice->effects.desc[i].InitialState
		);

		FAudio_memcpy(&srcParams, &dstParams, sizeof(dstParams));
	}

	return (float*) dstParams.pBuffer;
}

static void FAudio_INTERNAL_MixSource(FAudioSourceVoice *voice)
{
	/* Iterators */
	uint32_t i;
	/* Decode/Resample variables */
	uint64_t toDecode;
	uint64_t toResample;
	/* Output mix variables */
	float *stream;
	uint32_t mixed;
	uint32_t oChan;
	FAudioVoice *out;
	uint32_t outputRate;
	double stepd;
	float *effectOut;

	/* Calculate the resample stepping value */
	if (voice->src.resampleFreq != voice->src.freqRatio * voice->src.format->nSamplesPerSec)
	{
		FAudio_PlatformLockMutex(voice->sendLock);
		out = (voice->sends.SendCount == 0) ?
			voice->audio->master : /* Barf */
			voice->sends.pSends->pOutputVoice;
		FAudio_PlatformUnlockMutex(voice->sendLock);
		outputRate = (out->type == FAUDIO_VOICE_MASTER) ?
			out->master.inputSampleRate :
			out->mix.inputSampleRate;
		stepd = (
			voice->src.freqRatio *
			(double) voice->src.format->nSamplesPerSec /
			(double) outputRate
		);
		voice->src.resampleStep = DOUBLE_TO_FIXED(stepd);
		voice->src.resampleFreq = voice->src.freqRatio * voice->src.format->nSamplesPerSec;
	}

	/* Last call for buffer data! */
	if (	voice->src.callback != NULL &&
		voice->src.callback->OnVoiceProcessingPassStart != NULL)
	{
		voice->src.callback->OnVoiceProcessingPassStart(
			voice->src.callback,
			voice->src.decodeSamples * sizeof(int16_t)
		);
	}

	if (voice->src.active == 2)
	{
		/* We're just playing tails, skip all buffer stuff */
		mixed = voice->src.resampleSamples;
		FAudio_zero(voice->audio->resampleCache, mixed * sizeof(float));
		goto sendwork;
	}

	FAudio_PlatformLockMutex(voice->src.bufferLock);

	/* Nothing to do? */
	if (voice->src.bufferList == NULL)
	{
		FAudio_PlatformUnlockMutex(voice->src.bufferLock);
		goto end;
	}

	/* Base decode size, int to fixed... */
	toDecode = voice->src.resampleSamples * voice->src.resampleStep;
	/* ... rounded up based on current offset... */
	toDecode += voice->src.curBufferOffsetDec + FIXED_FRACTION_MASK;
	/* ... fixed to int, truncating extra fraction from rounding. */
	toDecode >>= FIXED_PRECISION;

	/* Decode... */
	FAudio_INTERNAL_DecodeBuffers(voice, &toDecode);

	/* Nothing to resample? */
	if (toDecode == 0)
	{
		FAudio_PlatformUnlockMutex(voice->src.bufferLock);
		goto end;
	}

	/* int to fixed... */
	toResample = toDecode << FIXED_PRECISION;
	/* ... round back down based on current offset... */
	toResample -= voice->src.curBufferOffsetDec;
	/* ... undo step size, fixed to int. */
	toResample /= voice->src.resampleStep;
	/* FIXME: I feel like this should be an assert but I suck */
	toResample = FAudio_min(toResample, voice->src.resampleSamples);

	/* Resample... */
	if (voice->src.resampleStep == FIXED_ONE)
	{
		/* Actually, just copy directly... */
		FAudio_memcpy(
			voice->audio->resampleCache,
			voice->audio->decodeCache,
			(size_t) toResample * voice->src.format->nChannels * sizeof(float)
		);
	}
	else
	{
		voice->src.resample(
			voice->audio->decodeCache,
			voice->audio->resampleCache,
			&voice->src.resampleOffset,
			voice->src.resampleStep,
			toResample,
			voice->src.format->nChannels
		);
	}

	/* Update buffer offsets */
	if (voice->src.bufferList != NULL)
	{
		/* Increment fixed offset by resample size, int to fixed... */
		voice->src.curBufferOffsetDec += toResample * voice->src.resampleStep;
		/* ... chop off any ints we got from the above increment */
		voice->src.curBufferOffsetDec &= FIXED_FRACTION_MASK;
	}
	else
	{
		voice->src.curBufferOffsetDec = 0;
		voice->src.curBufferOffset = 0;
	}

	/* Done with buffers, finally. */
	FAudio_PlatformUnlockMutex(voice->src.bufferLock);
	mixed = (uint32_t) toResample;

sendwork:
	FAudio_PlatformLockMutex(voice->sendLock);

	/* Nowhere to send it? Just skip the rest...*/
	if (voice->sends.SendCount == 0)
	{
		FAudio_PlatformUnlockMutex(voice->sendLock);
		goto end;
	}

	/* Filters */
	if (voice->flags & FAUDIO_VOICE_USEFILTER)
	{
		FAudio_PlatformLockMutex(voice->filterLock);
		FAudio_INTERNAL_FilterVoice(
			&voice->filter,
			voice->filterState,
			voice->audio->resampleCache,
			mixed,
			voice->src.format->nChannels
		);
		FAudio_PlatformUnlockMutex(voice->filterLock);
	}

	/* Process effect chain */
	effectOut = voice->audio->resampleCache;
	FAudio_PlatformLockMutex(voice->effectLock);
	if (voice->effects.count > 0)
	{
		effectOut = FAudio_INTERNAL_ProcessEffectChain(
			voice,
			voice->src.format->nChannels,
			voice->src.format->nSamplesPerSec,
			voice->audio->resampleCache,
			mixed
		);
	}
	FAudio_PlatformUnlockMutex(voice->effectLock);

	/* Send float cache to sends */
	FAudio_PlatformLockMutex(voice->volumeLock);
	for (i = 0; i < voice->sends.SendCount; i += 1)
	{
		out = voice->sends.pSends[i].pOutputVoice;
		if (out->type == FAUDIO_VOICE_MASTER)
		{
			stream = out->master.output;
			oChan = out->master.inputChannels;
		}
		else
		{
			stream = out->mix.inputCache;
			oChan = out->mix.inputChannels;
		}

		voice->sendMix[i](
			mixed,
			voice->outputChannels,
			oChan,
			voice->volume,
			effectOut,
			stream,
			voice->channelVolume,
			voice->sendCoefficients[i]
		);

		if (voice->flags & FAUDIO_VOICE_USEFILTER)
		{
			FAudio_INTERNAL_FilterVoice(
				&voice->sendFilter[i],
				voice->sendFilterState[i],
				stream,
				mixed,
				oChan
			);
		}
	}
	FAudio_PlatformUnlockMutex(voice->volumeLock);

	FAudio_PlatformUnlockMutex(voice->sendLock);

	/* Done, finally. */
end:
	if (	voice->src.callback != NULL &&
		voice->src.callback->OnVoiceProcessingPassEnd != NULL)
	{
		voice->src.callback->OnVoiceProcessingPassEnd(
			voice->src.callback
		);
	}
}

static void FAudio_INTERNAL_MixSubmix(FAudioSubmixVoice *voice)
{
	uint32_t i;
	float *stream;
	uint32_t oChan;
	FAudioVoice *out;
	uint32_t resampled;
	float *effectOut;

	FAudio_PlatformLockMutex(voice->sendLock);

	/* Nothing to do? */
	if (voice->sends.SendCount == 0)
	{
		goto end;
	}

	/* Resample (if necessary) */
	resampled = FAudio_PlatformResample(
		voice->mix.resampler,
		voice->mix.inputCache,
		voice->mix.inputSamples,
		voice->audio->resampleCache,
		voice->mix.outputSamples * voice->mix.inputChannels
	);

	/* Submix overall volume is applied _before_ effects/filters, blech! */
	if (voice->volume != 1.0f)
	{
		FAudio_INTERNAL_Amplify(
			voice->audio->resampleCache,
			resampled,
			voice->volume
		);
	}
	resampled /= voice->mix.inputChannels;

	/* Filters */
	if (voice->flags & FAUDIO_VOICE_USEFILTER)
	{
		FAudio_PlatformLockMutex(voice->filterLock);
		FAudio_INTERNAL_FilterVoice(
			&voice->filter,
			voice->filterState,
			voice->audio->resampleCache,
			resampled,
			voice->mix.inputChannels
		);
		FAudio_PlatformUnlockMutex(voice->filterLock);
	}

	/* Process effect chain */
	effectOut = voice->audio->resampleCache;
	FAudio_PlatformLockMutex(voice->effectLock);
	if (voice->effects.count > 0)
	{
		effectOut = FAudio_INTERNAL_ProcessEffectChain(
			voice,
			voice->mix.inputChannels,
			voice->mix.inputSampleRate,
			voice->audio->resampleCache,
			resampled
		);
	}
	FAudio_PlatformUnlockMutex(voice->effectLock);

	/* Send float cache to sends */
	FAudio_PlatformLockMutex(voice->volumeLock);
	for (i = 0; i < voice->sends.SendCount; i += 1)
	{
		out = voice->sends.pSends[i].pOutputVoice;
		if (out->type == FAUDIO_VOICE_MASTER)
		{
			stream = out->master.output;
			oChan = out->master.inputChannels;
		}
		else
		{
			stream = out->mix.inputCache;
			oChan = out->mix.inputChannels;
		}

		voice->sendMix[i](
			resampled,
			voice->outputChannels,
			oChan,
			1.0f,
			effectOut,
			stream,
			voice->channelVolume,
			voice->sendCoefficients[i]
		);

		if (voice->flags & FAUDIO_VOICE_USEFILTER)
		{
			FAudio_INTERNAL_FilterVoice(
				&voice->sendFilter[i],
				voice->sendFilterState[i],
				stream,
				resampled,
				oChan
			);
		}
	}
	FAudio_PlatformUnlockMutex(voice->volumeLock);

	/* Zero this at the end, for the next update */
end:
	FAudio_PlatformUnlockMutex(voice->sendLock);
	FAudio_zero(
		voice->mix.inputCache,
		sizeof(float) * voice->mix.inputSamples
	);
}

void FAudio_INTERNAL_UpdateEngine(FAudio *audio, float *output)
{
	uint32_t totalSamples;
	LinkedList *list;
	FAudioSourceVoice *source;
	FAudioEngineCallback *callback;

	if (!audio->active)
	{
		return;
	}

	/* ProcessingPassStart callbacks */
	FAudio_PlatformLockMutex(audio->callbackLock);
	list = audio->callbacks;
	while (list != NULL)
	{
		callback = (FAudioEngineCallback*) list->entry;
		if (callback->OnProcessingPassStart != NULL)
		{
			callback->OnProcessingPassStart(
				callback
			);
		}
		list = list->next;
	}
	FAudio_PlatformUnlockMutex(audio->callbackLock);

	/* Writes to master will directly write to output */
	audio->master->master.output = output;

	/* Mix sources */
	FAudio_PlatformLockMutex(audio->sourceLock);
	list = audio->sources;
	while (list != NULL)
	{
		source = (FAudioSourceVoice*) list->entry;
		if (source->src.active)
		{
			FAudio_INTERNAL_MixSource(source);
		}
		list = list->next;
	}
	FAudio_PlatformUnlockMutex(audio->sourceLock);

	/* Mix submixes, ordered by processing stage */
	FAudio_PlatformLockMutex(audio->submixLock);
	list = audio->submixes;
	while (list != NULL)
	{
		FAudio_INTERNAL_MixSubmix((FAudioSubmixVoice*) list->entry);
		list = list->next;
	}
	FAudio_PlatformUnlockMutex(audio->submixLock);

	/* Apply master volume */
	totalSamples = audio->updateSize * audio->master->master.inputChannels;
	if (audio->master->volume != 1.0f)
	{
		FAudio_INTERNAL_Amplify(
			output,
			totalSamples,
			audio->master->volume
		);
	}

	/* Process master effect chain */
	FAudio_PlatformLockMutex(audio->master->effectLock);
	if (audio->master->effects.count > 0)
	{
		float *effectOut = FAudio_INTERNAL_ProcessEffectChain(
			audio->master,
			audio->master->master.inputChannels,
			audio->master->master.inputSampleRate,
			output,
			audio->updateSize
		);

		if (effectOut != output)
		{
			FAudio_memcpy(
				output,
				effectOut,
				audio->updateSize * audio->master->outputChannels * sizeof(float)
			);
		}
	}
	FAudio_PlatformUnlockMutex(audio->master->effectLock);

	/* OnProcessingPassEnd callbacks */
	FAudio_PlatformLockMutex(audio->callbackLock);
	list = audio->callbacks;
	while (list != NULL)
	{
		callback = (FAudioEngineCallback*) list->entry;
		if (callback->OnProcessingPassEnd != NULL)
		{
			callback->OnProcessingPassEnd(
				callback
			);
		}
		list = list->next;
	}
	FAudio_PlatformUnlockMutex(audio->callbackLock);
}

void FAudio_INTERNAL_ResizeDecodeCache(FAudio *audio, uint32_t samples)
{
	if (samples > audio->decodeSamples)
	{
		audio->decodeSamples = samples;
		audio->decodeCache = (float*) audio->pRealloc(
			audio->decodeCache,
			sizeof(float) * audio->decodeSamples
		);
	}
}

void FAudio_INTERNAL_ResizeResampleCache(FAudio *audio, uint32_t samples)
{
	if (samples > audio->resampleSamples)
	{
		audio->resampleSamples = samples;
		audio->resampleCache = (float*) audio->pRealloc(
			audio->resampleCache,
			sizeof(float) * audio->resampleSamples
		);
	}
}

void FAudio_INTERNAL_ResizeEffectChainCache(FAudio *audio, uint32_t samples)
{
	if (samples > audio->effectChainSamples)
	{
		audio->effectChainSamples = samples;
		audio->effectChainCache = (float*) audio->pRealloc(
			audio->effectChainCache,
			sizeof(float) * audio->effectChainSamples
		);
	}
}

static const float MATRIX_DEFAULTS[8][8][64] =
{
	#include "matrix_defaults.inl"
};

void FAudio_INTERNAL_SetDefaultMatrix(
	float *matrix,
	uint32_t srcChannels,
	uint32_t dstChannels
) {
	FAudio_assert(srcChannels > 0 && srcChannels < 9);
	FAudio_assert(dstChannels > 0 && dstChannels < 9);
	FAudio_memcpy(
		matrix,
		MATRIX_DEFAULTS[srcChannels - 1][dstChannels - 1],
		srcChannels * dstChannels * sizeof(float)
	);
}

void FAudio_INTERNAL_AllocEffectChain(
	FAudioVoice *voice,
	const FAudioEffectChain *pEffectChain
) {
	uint32_t i;

	voice->effects.count = pEffectChain->EffectCount;
	if (voice->effects.count == 0)
	{
		return;
	}

	for (i = 0; i < pEffectChain->EffectCount; i += 1)
	{
		pEffectChain->pEffectDescriptors[i].pEffect->AddRef(pEffectChain->pEffectDescriptors[i].pEffect);
	}

	voice->effects.desc = (FAudioEffectDescriptor*) voice->audio->pMalloc(
		voice->effects.count * sizeof(FAudioEffectDescriptor)
	);
	FAudio_memcpy(
		voice->effects.desc,
		pEffectChain->pEffectDescriptors,
		voice->effects.count * sizeof(FAudioEffectDescriptor)
	);
	#define ALLOC_EFFECT_PROPERTY(prop, type) \
		voice->effects.prop = (type*) voice->audio->pMalloc( \
			voice->effects.count * sizeof(type) \
		); \
		FAudio_zero( \
			voice->effects.prop, \
			voice->effects.count * sizeof(type) \
		);
	ALLOC_EFFECT_PROPERTY(parameters, void*)
	ALLOC_EFFECT_PROPERTY(parameterSizes, uint32_t)
	ALLOC_EFFECT_PROPERTY(parameterUpdates, uint8_t)
	ALLOC_EFFECT_PROPERTY(inPlaceProcessing, uint8_t)
	#undef ALLOC_EFFECT_PROPERTY
}

void FAudio_INTERNAL_FreeEffectChain(FAudioVoice *voice)
{
	uint32_t i;

	if (voice->effects.count == 0)
	{
		return;
	}

	for (i = 0; i < voice->effects.count; i += 1)
	{
		voice->effects.desc[i].pEffect->UnlockForProcess(voice->effects.desc[i].pEffect);
		voice->effects.desc[i].pEffect->Release(voice->effects.desc[i].pEffect);
	}

	voice->audio->pFree(voice->effects.desc);
	voice->audio->pFree(voice->effects.parameters);
	voice->audio->pFree(voice->effects.parameterSizes);
	voice->audio->pFree(voice->effects.parameterUpdates);
	voice->audio->pFree(voice->effects.inPlaceProcessing);
}

/* PCM Decoding */

void FAudio_INTERNAL_DecodePCM8(
	FAudioVoice *voice,
	FAudioBuffer *buffer,
	uint32_t *samples,
	uint32_t end,
	float *decodeCache
) {
	*samples = FAudio_min(*samples, end - voice->src.curBufferOffset);
	FAudio_INTERNAL_Convert_U8_To_F32(
		((uint8_t*) buffer->pAudioData) + (
			voice->src.curBufferOffset * voice->src.format->nChannels
		),
		decodeCache,
		*samples * voice->src.format->nChannels
	);
	voice->src.curBufferOffset += *samples;
}

void FAudio_INTERNAL_DecodePCM16(
	FAudioVoice *voice,
	FAudioBuffer *buffer,
	uint32_t *samples,
	uint32_t end,
	float *decodeCache
) {
	*samples = FAudio_min(*samples, end - voice->src.curBufferOffset);
	FAudio_INTERNAL_Convert_S16_To_F32(
		((int16_t*) buffer->pAudioData) + (
			voice->src.curBufferOffset * voice->src.format->nChannels
		),
		decodeCache,
		*samples * voice->src.format->nChannels
	);
	voice->src.curBufferOffset += *samples;
}

void FAudio_INTERNAL_DecodePCM32F(
	FAudioVoice *voice,
	FAudioBuffer *buffer,
	uint32_t *samples,
	uint32_t end,
	float *decodeCache
) {
	*samples = FAudio_min(*samples, end - voice->src.curBufferOffset);
	FAudio_memcpy(
		decodeCache,
		((float*) buffer->pAudioData) + (
			voice->src.curBufferOffset * voice->src.format->nChannels
		),
		sizeof(float) * *samples * voice->src.format->nChannels
	);
	voice->src.curBufferOffset += *samples;
}

/* MSADPCM Decoding */

static inline int16_t FAudio_INTERNAL_ParseNibble(
	uint8_t nibble,
	uint8_t predictor,
	int16_t *delta,
	int16_t *sample1,
	int16_t *sample2
) {
	static const int32_t AdaptionTable[16] =
	{
		230, 230, 230, 230, 307, 409, 512, 614,
		768, 614, 512, 409, 307, 230, 230, 230
	};
	static const int32_t AdaptCoeff_1[7] =
	{
		256, 512, 0, 192, 240, 460, 392
	};
	static const int32_t AdaptCoeff_2[7] =
	{
		0, -256, 0, 64, 0, -208, -232
	};

	int8_t signedNibble;
	int32_t sampleInt;
	int16_t sample;

	signedNibble = (int8_t) nibble;
	if (signedNibble & 0x08)
	{
		signedNibble -= 0x10;
	}

	sampleInt = (
		(*sample1 * AdaptCoeff_1[predictor]) +
		(*sample2 * AdaptCoeff_2[predictor])
	) / 256;
	sampleInt += signedNibble * (*delta);
	sample = FAudio_clamp(sampleInt, -32768, 32767);

	*sample2 = *sample1;
	*sample1 = sample;
	*delta = (int16_t) (AdaptionTable[nibble] * (int32_t) (*delta) / 256);
	if (*delta < 16)
	{
		*delta = 16;
	}
	return sample;
}

#define READ(item, type) \
	item = *((type*) *buf); \
	*buf += sizeof(type);

static inline void FAudio_INTERNAL_DecodeMonoMSADPCMBlock(
	uint8_t **buf,
	int16_t *blockCache,
	uint32_t align
) {
	uint32_t i;

	/* Temp storage for ADPCM blocks */
	uint8_t predictor;
	int16_t delta;
	int16_t sample1;
	int16_t sample2;

	/* Preamble */
	READ(predictor, uint8_t)
	READ(delta, int16_t)
	READ(sample1, int16_t)
	READ(sample2, int16_t)
	align -= 7;

	/* Samples */
	*blockCache++ = sample2;
	*blockCache++ = sample1;
	for (i = 0; i < align; i += 1, *buf += 1)
	{
		*blockCache++ = FAudio_INTERNAL_ParseNibble(
			*(*buf) >> 4,
			predictor,
			&delta,
			&sample1,
			&sample2
		);
		*blockCache++ = FAudio_INTERNAL_ParseNibble(
			*(*buf) & 0x0F,
			predictor,
			&delta,
			&sample1,
			&sample2
		);
	}
}

static inline void FAudio_INTERNAL_DecodeStereoMSADPCMBlock(
	uint8_t **buf,
	int16_t *blockCache,
	uint32_t align
) {
	uint32_t i;

	/* Temp storage for ADPCM blocks */
	uint8_t l_predictor;
	uint8_t r_predictor;
	int16_t l_delta;
	int16_t r_delta;
	int16_t l_sample1;
	int16_t r_sample1;
	int16_t l_sample2;
	int16_t r_sample2;

	/* Preamble */
	READ(l_predictor, uint8_t)
	READ(r_predictor, uint8_t)
	READ(l_delta, int16_t)
	READ(r_delta, int16_t)
	READ(l_sample1, int16_t)
	READ(r_sample1, int16_t)
	READ(l_sample2, int16_t)
	READ(r_sample2, int16_t)
	align -= 14;

	/* Samples */
	*blockCache++ = l_sample2;
	*blockCache++ = r_sample2;
	*blockCache++ = l_sample1;
	*blockCache++ = r_sample1;
	for (i = 0; i < align; i += 1, *buf += 1)
	{
		*blockCache++ = FAudio_INTERNAL_ParseNibble(
			*(*buf) >> 4,
			l_predictor,
			&l_delta,
			&l_sample1,
			&l_sample2
		);
		*blockCache++ = FAudio_INTERNAL_ParseNibble(
			*(*buf) & 0x0F,
			r_predictor,
			&r_delta,
			&r_sample1,
			&r_sample2
		);
	}
}

#undef READ

void FAudio_INTERNAL_DecodeMonoMSADPCM(
	FAudioVoice *voice,
	FAudioBuffer *buffer,
	uint32_t *samples,
	uint32_t end,
	float *decodeCache
) {
	/* Loop variables */
	uint32_t copy, done = 0;

	/* Read pointers */
	uint8_t *buf;
	int32_t midOffset;

	/* PCM block cache */
	int16_t blockCache[512]; /* Max block size */

	/* Block size */
	uint32_t bsize = (voice->src.format->nBlockAlign - 6) * 2;

	/* Where are we starting? */
	buf = (uint8_t*) buffer->pAudioData + (
		(voice->src.curBufferOffset / bsize) *
		voice->src.format->nBlockAlign
	);

	/* Are we starting in the middle? */
	midOffset = (voice->src.curBufferOffset % bsize);

	/* Read in each block directly to the decode cache */
	while (done < *samples && voice->src.curBufferOffset < end)
	{
		copy = FAudio_min(*samples - done, bsize - midOffset);
		FAudio_INTERNAL_DecodeMonoMSADPCMBlock(
			&buf,
			blockCache,
			voice->src.format->nBlockAlign
		);
		FAudio_INTERNAL_Convert_S16_To_F32(
			blockCache + midOffset,
			decodeCache,
			copy
		);
		decodeCache += copy;
		done += copy;
		voice->src.curBufferOffset += copy;
		midOffset = 0;
	}

	*samples = done;
}

void FAudio_INTERNAL_DecodeStereoMSADPCM(
	FAudioVoice *voice,
	FAudioBuffer *buffer,
	uint32_t *samples,
	uint32_t end,
	float *decodeCache
) {
	/* Loop variables */
	uint32_t copy, done = 0;

	/* Read pointers */
	uint8_t *buf;
	int32_t midOffset;

	/* PCM block cache */
	int16_t blockCache[1024]; /* Max block size */

	/* Align, block size */
	uint32_t bsize = ((voice->src.format->nBlockAlign / 2) - 6) * 2;

	/* Where are we starting? */
	buf = (uint8_t*) buffer->pAudioData + (
		(voice->src.curBufferOffset / bsize) *
		voice->src.format->nBlockAlign
	);

	/* Are we starting in the middle? */
	midOffset = (voice->src.curBufferOffset % bsize);

	/* Read in each block directly to the decode cache */
	while (done < *samples && voice->src.curBufferOffset < end)
	{
		copy = FAudio_min(*samples - done, bsize - midOffset);
		FAudio_INTERNAL_DecodeStereoMSADPCMBlock(
			&buf,
			blockCache,
			voice->src.format->nBlockAlign
		);
		FAudio_INTERNAL_Convert_S16_To_F32(
			blockCache + (midOffset * 2),
			decodeCache,
			copy * 2
		);
		decodeCache += copy * 2;
		done += copy;
		voice->src.curBufferOffset += copy;
		midOffset = 0;
	}

	*samples = done;
}
