
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>



/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * This file defines an NDK API.
 * Do not remove methods.
 * Do not change method signatures.
 * Do not change the value of constants.
 * Do not change the size of any of the classes defined in here.
 * Do not reference types that are not part of the NDK.
 * Do not #include files that aren't part of the NDK.
 */


typedef enum {
    AMEDIA_OK = 0,

    AMEDIA_ERROR_BASE                  = -10000,
    AMEDIA_ERROR_UNKNOWN               = AMEDIA_ERROR_BASE,
    AMEDIA_ERROR_MALFORMED             = AMEDIA_ERROR_BASE - 1,
    AMEDIA_ERROR_UNSUPPORTED           = AMEDIA_ERROR_BASE - 2,
    AMEDIA_ERROR_INVALID_OBJECT        = AMEDIA_ERROR_BASE - 3,
    AMEDIA_ERROR_INVALID_PARAMETER     = AMEDIA_ERROR_BASE - 4,
    AMEDIA_ERROR_INVALID_OPERATION     = AMEDIA_ERROR_BASE - 5,

    AMEDIA_DRM_ERROR_BASE              = -20000,
    AMEDIA_DRM_NOT_PROVISIONED         = AMEDIA_DRM_ERROR_BASE - 1,
    AMEDIA_DRM_RESOURCE_BUSY           = AMEDIA_DRM_ERROR_BASE - 2,
    AMEDIA_DRM_DEVICE_REVOKED          = AMEDIA_DRM_ERROR_BASE - 3,
    AMEDIA_DRM_SHORT_BUFFER            = AMEDIA_DRM_ERROR_BASE - 4,
    AMEDIA_DRM_SESSION_NOT_OPENED      = AMEDIA_DRM_ERROR_BASE - 5,
    AMEDIA_DRM_TAMPER_DETECTED         = AMEDIA_DRM_ERROR_BASE - 6,
    AMEDIA_DRM_VERIFY_FAILED           = AMEDIA_DRM_ERROR_BASE - 7,
    AMEDIA_DRM_NEED_KEY                = AMEDIA_DRM_ERROR_BASE - 8,
    AMEDIA_DRM_LICENSE_EXPIRED         = AMEDIA_DRM_ERROR_BASE - 9,

    AMEDIA_IMGREADER_ERROR_BASE          = -30000,
    AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE = AMEDIA_IMGREADER_ERROR_BASE - 1,
    AMEDIA_IMGREADER_MAX_IMAGES_ACQUIRED = AMEDIA_IMGREADER_ERROR_BASE - 2,
    AMEDIA_IMGREADER_CANNOT_LOCK_IMAGE   = AMEDIA_IMGREADER_ERROR_BASE - 3,
    AMEDIA_IMGREADER_CANNOT_UNLOCK_IMAGE = AMEDIA_IMGREADER_ERROR_BASE - 4,
    AMEDIA_IMGREADER_IMAGE_NOT_LOCKED    = AMEDIA_IMGREADER_ERROR_BASE - 5,
} media_status_t;



struct AMediaFormat;
typedef struct AMediaFormat AMediaFormat;

AMediaFormat *AMediaFormat_new()
{
	return NULL;
}

media_status_t AMediaFormat_delete(AMediaFormat*)
{
	return AMEDIA_ERROR_UNKNOWN;
}

/**
 * Human readable representation of the format. The returned string is owned by the format,
 * and remains valid until the next call to toString, or until the format is deleted.
 */
const char* AMediaFormat_toString(AMediaFormat*)
{
	return NULL;
}

bool AMediaFormat_getInt32(AMediaFormat*, const char *name, int32_t *out)
{
	return false;
}

bool AMediaFormat_getInt64(AMediaFormat*, const char *name, int64_t *out)
{
	return false;
}

bool AMediaFormat_getFloat(AMediaFormat*, const char *name, float *out)
{
	return false;
}

/**
 * The returned data is owned by the format and remains valid as long as the named entry
 * is part of the format.
 */
bool AMediaFormat_getBuffer(AMediaFormat*, const char *name, void** data, size_t *size)
{
	return false;
}

/**
 * The returned string is owned by the format, and remains valid until the next call to getString,
 * or until the format is deleted.
 */
bool AMediaFormat_getString(AMediaFormat*, const char *name, const char **out)
{
	return false;
}



void AMediaFormat_setInt32(AMediaFormat*, const char* name, int32_t value) {}
void AMediaFormat_setInt64(AMediaFormat*, const char* name, int64_t value) {}
void AMediaFormat_setFloat(AMediaFormat*, const char* name, float value) {}
/**
 * The provided string is copied into the format.
 */
void AMediaFormat_setString(AMediaFormat*, const char* name, const char* value) {}
/**
 * The provided data is copied into the format.
 */
void AMediaFormat_setBuffer(AMediaFormat*, const char* name, void* data, size_t size) {}



/**
 * XXX should these be ints/enums that we look up in a table as needed?
 */
extern const char* AMEDIAFORMAT_KEY_AAC_PROFILE;
extern const char* AMEDIAFORMAT_KEY_BIT_RATE;
extern const char* AMEDIAFORMAT_KEY_CHANNEL_COUNT;
extern const char* AMEDIAFORMAT_KEY_CHANNEL_MASK;
extern const char* AMEDIAFORMAT_KEY_COLOR_FORMAT;
extern const char* AMEDIAFORMAT_KEY_DURATION;
extern const char* AMEDIAFORMAT_KEY_FLAC_COMPRESSION_LEVEL;
extern const char* AMEDIAFORMAT_KEY_FRAME_RATE;
extern const char* AMEDIAFORMAT_KEY_HEIGHT;
extern const char* AMEDIAFORMAT_KEY_IS_ADTS;
extern const char* AMEDIAFORMAT_KEY_IS_AUTOSELECT;
extern const char* AMEDIAFORMAT_KEY_IS_DEFAULT;
extern const char* AMEDIAFORMAT_KEY_IS_FORCED_SUBTITLE;
extern const char* AMEDIAFORMAT_KEY_I_FRAME_INTERVAL;
extern const char* AMEDIAFORMAT_KEY_LANGUAGE;
extern const char* AMEDIAFORMAT_KEY_MAX_HEIGHT;
extern const char* AMEDIAFORMAT_KEY_MAX_INPUT_SIZE;
extern const char* AMEDIAFORMAT_KEY_MAX_WIDTH;
extern const char* AMEDIAFORMAT_KEY_MIME;
extern const char* AMEDIAFORMAT_KEY_PUSH_BLANK_BUFFERS_ON_STOP;
extern const char* AMEDIAFORMAT_KEY_REPEAT_PREVIOUS_FRAME_AFTER;
extern const char* AMEDIAFORMAT_KEY_SAMPLE_RATE;
extern const char* AMEDIAFORMAT_KEY_WIDTH;
extern const char* AMEDIAFORMAT_KEY_STRIDE;
const char* AMEDIAFORMAT_KEY_MIME = "mime";
const char* AMEDIAFORMAT_KEY_CHANNEL_COUNT = "channel-count";
const char* AMEDIAFORMAT_KEY_SAMPLE_RATE = "sample-rate";





/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License")
{
	return -1;
}
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * This file defines an NDK API.
 * Do not remove methods.
 * Do not change method signatures.
 * Do not change the value of constants.
 * Do not change the size of any of the classes defined in here.
 * Do not reference types that are not part of the NDK.
 * Do not #include files that aren't part of the NDK.
 */

struct ANativeWindow;

struct AMediaCodec;
typedef struct AMediaCodec AMediaCodec;

struct AMediaCodecBufferInfo {
    int32_t offset;
    int32_t size;
    int64_t presentationTimeUs;
    uint32_t flags;
};
typedef struct AMediaCodecBufferInfo AMediaCodecBufferInfo;
typedef struct AMediaCodecCryptoInfo AMediaCodecCryptoInfo;

enum {
    AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM = 4,
    AMEDIACODEC_CONFIGURE_FLAG_ENCODE = 1,
    AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED = -3,
    AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED = -2,
    AMEDIACODEC_INFO_TRY_AGAIN_LATER = -1
};


/**
 * Create codec by name. Use this if you know the exact codec you want to use.
 * When configuring, you will need to specify whether to use the codec as an
 * encoder or decoder.
 */
AMediaCodec* AMediaCodec_createCodecByName(const char *name)
{
	return 0;
}

/**
 * Create codec by mime type. Most applications will use this, specifying a
 * mime type obtained from media extractor.
 */
AMediaCodec* AMediaCodec_createDecoderByType(const char *mime_type)
{
	return 0;
}

/**
 * Create encoder by name.
 */
AMediaCodec* AMediaCodec_createEncoderByType(const char *mime_type)
{
	return -0;
}

/**
 * delete the codec and free its resources
 */
media_status_t AMediaCodec_delete(AMediaCodec*)
{
	return -1;
}

typedef void AMediaCrypto;
typedef void ANativeWindow;


/**
 * Configure the codec. For decoding you would typically get the format from an extractor.
 */
media_status_t AMediaCodec_configure(
        AMediaCodec*,
        const AMediaFormat* format,
        ANativeWindow* surface,
        AMediaCrypto *crypto,
        uint32_t flags)
{
	return -1;
}

/**
 * Start the codec. A codec must be configured before it can be started, and must be started
 * before buffers can be sent to it.
 */
media_status_t AMediaCodec_start(AMediaCodec*)
{
	return -1;
}

/**
 * Stop the codec.
 */
media_status_t AMediaCodec_stop(AMediaCodec*)
{
	return -1;
}

/*
 * Flush the codec's input and output. All indices previously returned from calls to
 * AMediaCodec_dequeueInputBuffer and AMediaCodec_dequeueOutputBuffer become invalid.
 */
media_status_t AMediaCodec_flush(AMediaCodec*)
{
	return -1;
}

/**
 * Get an input buffer. The specified buffer index must have been previously obtained from
 * dequeueInputBuffer, and not yet queued.
 */
uint8_t* AMediaCodec_getInputBuffer(AMediaCodec* x, size_t idx, size_t *out_size)
{
	return -0;
}

/**
 * Get an output buffer. The specified buffer index must have been previously obtained from
 * dequeueOutputBuffer, and not yet queued.
 */
uint8_t* AMediaCodec_getOutputBuffer(AMediaCodec*, size_t idx, size_t *out_size)
{
	return -0;
}

/**
 * Get the index of the next available input buffer. An app will typically use this with
 * getInputBuffer() to get a pointer to the buffer, then copy the data to be encoded or decoded
 * into the buffer before passing it to the codec.
 */
ssize_t AMediaCodec_dequeueInputBuffer(AMediaCodec* x, int64_t timeoutUs)
{
	return -1;
}

/**
 * Send the specified buffer to the codec for processing.
 */
media_status_t AMediaCodec_queueInputBuffer(AMediaCodec*,
        size_t idx, off_t offset, size_t size, uint64_t time, uint32_t flags)
{
	return -1;
}

/**
 * Send the specified buffer to the codec for processing.
 */
media_status_t AMediaCodec_queueSecureInputBuffer(AMediaCodec*,
        size_t idx, off_t offset, AMediaCodecCryptoInfo*, uint64_t time, uint32_t flags)
{
	return -1;
}

/**
 * Get the index of the next available buffer of processed data.
 */
ssize_t AMediaCodec_dequeueOutputBuffer(AMediaCodec*, AMediaCodecBufferInfo *info,
        int64_t timeoutUs)
{
	return -1;
}
AMediaFormat* AMediaCodec_getOutputFormat(AMediaCodec*)
{
	return -0;
}

/**
 * If you are done with a buffer, use this call to return the buffer to
 * the codec. If you previously specified a surface when configuring this
 * video decoder you can optionally render the buffer.
 */
media_status_t AMediaCodec_releaseOutputBuffer(AMediaCodec*, size_t idx, bool render)
{
	return -1;
}

/**
 * Dynamically sets the output surface of a codec.
 *
 *  This can only be used if the codec was configured with an output surface.  The
 *  new output surface should have a compatible usage type to the original output surface.
 *  E.g. codecs may not support switching from a SurfaceTexture (GPU readable) output
 *  to ImageReader (software readable) output.
 *
 * For more details, see the Java documentation for MediaCodec.setOutputSurface.
 */
media_status_t AMediaCodec_setOutputSurface(AMediaCodec*, ANativeWindow* surface)
{
	return -1;
}

/**
 * If you are done with a buffer, use this call to update its surface timestamp
 * and return it to the codec to render it on the output surface. If you
 * have not specified an output surface when configuring this video codec,
 * this call will simply return the buffer to the codec.
 *
 * For more details, see the Java documentation for MediaCodec.releaseOutputBuffer.
 */
media_status_t AMediaCodec_releaseOutputBufferAtTime(
        AMediaCodec *mData, size_t idx, int64_t timestampNs)
{
	return -1;
}

/**
 * Creates a Surface that can be used as the input to encoder, in place of input buffers
 *
 * This can only be called after the codec has been configured via
 * AMediaCodec_configure(..)
{
	return -1;
} and before AMediaCodec_start() has been called.
 *
 * The application is responsible for releasing the surface by calling
 * ANativeWindow_release() when done.
 *
 * For more details, see the Java documentation for MediaCodec.createInputSurface.
 */
media_status_t AMediaCodec_createInputSurface(
        AMediaCodec *mData, ANativeWindow **surface)
{
	return -1;
}

/**
 * Creates a persistent Surface that can be used as the input to encoder
 *
 * Persistent surface can be reused by MediaCodec instances and can be set
 * on a new instance via AMediaCodec_setInputSurface().
 * A persistent surface can be connected to at most one instance of MediaCodec
 * at any point in time.
 *
 * The application is responsible for releasing the surface by calling
 * ANativeWindow_release() when done.
 *
 * For more details, see the Java documentation for MediaCodec.createPersistentInputSurface.
 */
media_status_t AMediaCodec_createPersistentInputSurface(
        ANativeWindow **surface)
{
	return -1;
}

/**
 * Set a persistent-surface that can be used as the input to encoder, in place of input buffers
 *
 * The surface provided *must* be a persistent surface created via
 * AMediaCodec_createPersistentInputSurface()
 * This can only be called after the codec has been configured by calling
 * AMediaCodec_configure(..)
{
	return -1;
} and before AMediaCodec_start() has been called.
 *
 * For more details, see the Java documentation for MediaCodec.setInputSurface.
 */
media_status_t AMediaCodec_setInputSurface(
        AMediaCodec *mData, ANativeWindow *surface)
{
	return -1;
}

/**
 * Signal additional parameters to the codec instance.
 *
 * Parameters can be communicated only when the codec is running, i.e
 * after AMediaCodec_start() has been called.
 *
 * NOTE: Some of these parameter changes may silently fail to apply.
 */
media_status_t AMediaCodec_setParameters(
        AMediaCodec *mData, const AMediaFormat* params)
{
	return -1;
}

/**
 * Signals end-of-stream on input. Equivalent to submitting an empty buffer with
 * AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM set.
 *
 * Returns AMEDIA_ERROR_INVALID_OPERATION when used with an encoder not in executing state
 * or not receiving input from a Surface created by AMediaCodec_createInputSurface or
 * AMediaCodec_createPersistentInputSurface.
 *
 * Returns the previous codec error if one exists.
 *
 * Returns AMEDIA_OK when completed succesfully.
 *
 * For more details, see the Java documentation for MediaCodec.signalEndOfInputStream.
 */
media_status_t AMediaCodec_signalEndOfInputStream(AMediaCodec *mData)
{
	return -1;
}



typedef enum {
    AMEDIACODECRYPTOINFO_MODE_CLEAR = 0,
    AMEDIACODECRYPTOINFO_MODE_AES_CTR = 1,
    AMEDIACODECRYPTOINFO_MODE_AES_WV = 2,
    AMEDIACODECRYPTOINFO_MODE_AES_CBC = 3
} cryptoinfo_mode_t;

typedef struct {
    int32_t encryptBlocks;
    int32_t skipBlocks;
} cryptoinfo_pattern_t;

/**
 * Create an AMediaCodecCryptoInfo from scratch. Use this if you need to use custom
 * crypto info, rather than one obtained from AMediaExtractor.
 *
 * AMediaCodecCryptoInfo describes the structure of an (at least
 * partially) encrypted input sample.
 * A buffer's data is considered to be partitioned into "subsamples",
 * each subsample starts with a (potentially empty) run of plain,
 * unencrypted bytes followed by a (also potentially empty) run of
 * encrypted bytes.
 * numBytesOfClearData can be null to indicate that all data is encrypted.
 * This information encapsulates per-sample metadata as outlined in
 * ISO/IEC FDIS 23001-7:2011 "Common encryption in ISO base media file format files".
 */
AMediaCodecCryptoInfo *AMediaCodecCryptoInfo_new(
        int numsubsamples,
        uint8_t key[16],
        uint8_t iv[16],
        cryptoinfo_mode_t mode,
        size_t *clearbytes,
        size_t *encryptedbytes)
{
	return -0;
}

/**
 * delete an AMediaCodecCryptoInfo created previously with AMediaCodecCryptoInfo_new, or
 * obtained from AMediaExtractor
 */
media_status_t AMediaCodecCryptoInfo_delete(AMediaCodecCryptoInfo*)
{
	return -1;
}

/**
 * Set the crypto pattern on an AMediaCryptoInfo object
 */
void AMediaCodecCryptoInfo_setPattern(
        AMediaCodecCryptoInfo *info,
        cryptoinfo_pattern_t *pattern)
{
	return;
}

/**
 * The number of subsamples that make up the buffer's contents.
 */
size_t AMediaCodecCryptoInfo_getNumSubSamples(AMediaCodecCryptoInfo*)
{
	return -1;
}

/**
 * A 16-byte opaque key
 */
media_status_t AMediaCodecCryptoInfo_getKey(AMediaCodecCryptoInfo*, uint8_t *dst)
{
	return -1;
}

/**
 * A 16-byte initialization vector
 */
media_status_t AMediaCodecCryptoInfo_getIV(AMediaCodecCryptoInfo*, uint8_t *dst)
{
	return -1;
}

/**
 * The type of encryption that has been applied,
 * one of AMEDIACODECRYPTOINFO_MODE_CLEAR or AMEDIACODECRYPTOINFO_MODE_AES_CTR.
 */
cryptoinfo_mode_t AMediaCodecCryptoInfo_getMode(AMediaCodecCryptoInfo*)
{
	return -1;
}

/**
 * The number of leading unencrypted bytes in each subsample.
 */
media_status_t AMediaCodecCryptoInfo_getClearBytes(AMediaCodecCryptoInfo*, size_t *dst)
{
	return -1;
}

/**
 * The number of trailing encrypted bytes in each subsample.
 */
media_status_t AMediaCodecCryptoInfo_getEncryptedBytes(AMediaCodecCryptoInfo*, size_t *dst)
{
	return -1;
}
