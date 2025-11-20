/**
 * @file audio_fifo.c
 * @brief AudioFIFO API - Professional audio buffer management for resampling and rebuffering
 * @description Provides FFmpeg AudioFIFO wrapper for JavaScript, enabling proper audio frame buffering
 */

#include <node_api.h>
#include <stdlib.h>
#include <string.h>

#include "libavutil/audio_fifo.h"
#include "libavutil/frame.h"
#include "libavutil/samplefmt.h"
#include "libavformat/avformat.h"

// ============================================================================
// Context Management for AudioFIFO
// ============================================================================

#define MAX_AUDIO_FIFOS 1024

typedef struct {
    int id;
    AVAudioFifo *fifo;
    enum AVSampleFormat sample_fmt;
    int channels;
    int in_use;
} AudioFIFOEntry;

static AudioFIFOEntry audio_fifo_table[MAX_AUDIO_FIFOS] = {0};
static int next_fifo_id = 1;

// Allocate AudioFIFO ID
static int alloc_audio_fifo_id(AVAudioFifo *fifo, enum AVSampleFormat sample_fmt, int channels) {
    for (int i = 0; i < MAX_AUDIO_FIFOS; i++) {
        if (!audio_fifo_table[i].in_use) {
            audio_fifo_table[i].id = next_fifo_id++;
            audio_fifo_table[i].fifo = fifo;
            audio_fifo_table[i].sample_fmt = sample_fmt;
            audio_fifo_table[i].channels = channels;
            audio_fifo_table[i].in_use = 1;
            return audio_fifo_table[i].id;
        }
    }
    return -1;
}

// Get AudioFIFO entry
static AudioFIFOEntry* get_audio_fifo_entry(int id) {
    for (int i = 0; i < MAX_AUDIO_FIFOS; i++) {
        if (audio_fifo_table[i].in_use && audio_fifo_table[i].id == id) {
            return &audio_fifo_table[i];
        }
    }
    return NULL;
}

// Free AudioFIFO ID
static void free_audio_fifo_id(int id) {
    for (int i = 0; i < MAX_AUDIO_FIFOS; i++) {
        if (audio_fifo_table[i].in_use && audio_fifo_table[i].id == id) {
            if (audio_fifo_table[i].fifo) {
                av_audio_fifo_free(audio_fifo_table[i].fifo);
            }
            audio_fifo_table[i].in_use = 0;
            audio_fifo_table[i].fifo = NULL;
            return;
        }
    }
}

// ============================================================================
// External context access (for getting frames from atomic_api.c)
// ============================================================================

// These functions are defined in atomic_api.c
extern void* get_context_ptr(int id, int expected_type);
#define CTX_TYPE_FRAME 4  // Must match the enum value in atomic_api.c

// ============================================================================
// AudioFIFO API Implementation
// ============================================================================

/**
 * Create an AudioFIFO buffer
 * @param sample_format - Sample format (int, e.g., 8 for AV_SAMPLE_FMT_FLTP)
 * @param channels - Number of audio channels
 * @param nb_samples - Initial size in samples (can grow dynamically)
 * @returns AudioFIFO ID
 */
napi_value audio_fifo_alloc(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 3;
    napi_value argv[3];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 3) {
        napi_throw_error(env, NULL, "Expected sample_format, channels, and nb_samples");
        return NULL;
    }
    
    int32_t sample_format, channels, nb_samples;
    napi_get_value_int32(env, argv[0], &sample_format);
    napi_get_value_int32(env, argv[1], &channels);
    napi_get_value_int32(env, argv[2], &nb_samples);
    
    if (nb_samples < 1) {
        nb_samples = 1024; // Default size
    }
    
    AVAudioFifo *fifo = av_audio_fifo_alloc(
        (enum AVSampleFormat)sample_format,
        channels,
        nb_samples
    );
    
    if (!fifo) {
        napi_throw_error(env, NULL, "Failed to allocate AudioFIFO");
        return NULL;
    }
    
    int fifo_id = alloc_audio_fifo_id(fifo, (enum AVSampleFormat)sample_format, channels);
    if (fifo_id < 0) {
        av_audio_fifo_free(fifo);
        napi_throw_error(env, NULL, "Too many AudioFIFO contexts");
        return NULL;
    }
    
    napi_value result;
    napi_create_int32(env, fifo_id, &result);
    return result;
}

/**
 * Free an AudioFIFO buffer
 * @param fifoId - AudioFIFO ID
 */
napi_value audio_fifo_free(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 1;
    napi_value argv[1];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Expected AudioFIFO ID");
        return NULL;
    }
    
    int fifo_id;
    napi_get_value_int32(env, argv[0], &fifo_id);
    
    free_audio_fifo_id(fifo_id);
    
    return NULL;
}

/**
 * Write audio data from a frame to the AudioFIFO
 * @param fifoId - AudioFIFO ID
 * @param frameId - Frame ID containing audio data
 * @returns Number of samples written, or -1 on error
 */
napi_value audio_fifo_write(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 2;
    napi_value argv[2];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 2) {
        napi_throw_error(env, NULL, "Expected AudioFIFO ID and frame ID");
        return NULL;
    }
    
    int fifo_id, frame_id;
    napi_get_value_int32(env, argv[0], &fifo_id);
    napi_get_value_int32(env, argv[1], &frame_id);
    
    AudioFIFOEntry *entry = get_audio_fifo_entry(fifo_id);
    if (!entry || !entry->fifo) {
        napi_throw_error(env, NULL, "Invalid AudioFIFO ID");
        return NULL;
    }
    
    AVFrame *frame = (AVFrame*)get_context_ptr(frame_id, CTX_TYPE_FRAME);
    if (!frame) {
        napi_throw_error(env, NULL, "Invalid frame ID");
        return NULL;
    }
    
    // Reallocate FIFO if needed
    int required_size = av_audio_fifo_size(entry->fifo) + frame->nb_samples;
    if (av_audio_fifo_space(entry->fifo) < frame->nb_samples) {
        if (av_audio_fifo_realloc(entry->fifo, required_size) < 0) {
            napi_throw_error(env, NULL, "Failed to reallocate AudioFIFO");
            return NULL;
        }
    }
    
    int ret = av_audio_fifo_write(entry->fifo, (void**)frame->data, frame->nb_samples);
    if (ret < 0) {
        napi_throw_error(env, NULL, "Failed to write to AudioFIFO");
        return NULL;
    }
    
    napi_value result;
    napi_create_int32(env, ret, &result);
    return result;
}

/**
 * Read audio data from AudioFIFO into a frame
 * @param fifoId - AudioFIFO ID
 * @param frameId - Frame ID to read into (must have buffer allocated)
 * @param nb_samples - Number of samples to read
 * @returns Number of samples read, or -1 on error
 */
napi_value audio_fifo_read(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 3;
    napi_value argv[3];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 3) {
        napi_throw_error(env, NULL, "Expected AudioFIFO ID, frame ID, and nb_samples");
        return NULL;
    }
    
    int fifo_id, frame_id, nb_samples;
    napi_get_value_int32(env, argv[0], &fifo_id);
    napi_get_value_int32(env, argv[1], &frame_id);
    napi_get_value_int32(env, argv[2], &nb_samples);
    
    AudioFIFOEntry *entry = get_audio_fifo_entry(fifo_id);
    if (!entry || !entry->fifo) {
        napi_throw_error(env, NULL, "Invalid AudioFIFO ID");
        return NULL;
    }
    
    AVFrame *frame = (AVFrame*)get_context_ptr(frame_id, CTX_TYPE_FRAME);
    if (!frame) {
        napi_throw_error(env, NULL, "Invalid frame ID");
        return NULL;
    }
    
    // Check if there are enough samples
    int available = av_audio_fifo_size(entry->fifo);
    if (available < nb_samples) {
        nb_samples = available;
    }
    
    if (nb_samples <= 0) {
        napi_value result;
        napi_create_int32(env, 0, &result);
        return result;
    }
    
    int ret = av_audio_fifo_read(entry->fifo, (void**)frame->data, nb_samples);
    if (ret < 0) {
        napi_throw_error(env, NULL, "Failed to read from AudioFIFO");
        return NULL;
    }
    
    // Update frame properties
    frame->nb_samples = ret;
    
    napi_value result;
    napi_create_int32(env, ret, &result);
    return result;
}

/**
 * Get the number of samples currently in the AudioFIFO
 * @param fifoId - AudioFIFO ID
 * @returns Number of samples in the FIFO
 */
napi_value audio_fifo_size(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 1;
    napi_value argv[1];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Expected AudioFIFO ID");
        return NULL;
    }
    
    int fifo_id;
    napi_get_value_int32(env, argv[0], &fifo_id);
    
    AudioFIFOEntry *entry = get_audio_fifo_entry(fifo_id);
    if (!entry || !entry->fifo) {
        napi_throw_error(env, NULL, "Invalid AudioFIFO ID");
        return NULL;
    }
    
    int size = av_audio_fifo_size(entry->fifo);
    
    napi_value result;
    napi_create_int32(env, size, &result);
    return result;
}

/**
 * Get the available space in the AudioFIFO
 * @param fifoId - AudioFIFO ID
 * @returns Number of samples that can be written without reallocation
 */
napi_value audio_fifo_space(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 1;
    napi_value argv[1];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Expected AudioFIFO ID");
        return NULL;
    }
    
    int fifo_id;
    napi_get_value_int32(env, argv[0], &fifo_id);
    
    AudioFIFOEntry *entry = get_audio_fifo_entry(fifo_id);
    if (!entry || !entry->fifo) {
        napi_throw_error(env, NULL, "Invalid AudioFIFO ID");
        return NULL;
    }
    
    int space = av_audio_fifo_space(entry->fifo);
    
    napi_value result;
    napi_create_int32(env, space, &result);
    return result;
}

/**
 * Reset/drain the AudioFIFO (remove all samples)
 * @param fifoId - AudioFIFO ID
 */
napi_value audio_fifo_reset(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 1;
    napi_value argv[1];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Expected AudioFIFO ID");
        return NULL;
    }
    
    int fifo_id;
    napi_get_value_int32(env, argv[0], &fifo_id);
    
    AudioFIFOEntry *entry = get_audio_fifo_entry(fifo_id);
    if (!entry || !entry->fifo) {
        napi_throw_error(env, NULL, "Invalid AudioFIFO ID");
        return NULL;
    }
    
    av_audio_fifo_reset(entry->fifo);
    
    return NULL;
}

/**
 * Drain samples from the AudioFIFO
 * @param fifoId - AudioFIFO ID
 * @param nb_samples - Number of samples to drain
 */
napi_value audio_fifo_drain(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 2;
    napi_value argv[2];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 2) {
        napi_throw_error(env, NULL, "Expected AudioFIFO ID and nb_samples");
        return NULL;
    }
    
    int fifo_id, nb_samples;
    napi_get_value_int32(env, argv[0], &fifo_id);
    napi_get_value_int32(env, argv[1], &nb_samples);
    
    AudioFIFOEntry *entry = get_audio_fifo_entry(fifo_id);
    if (!entry || !entry->fifo) {
        napi_throw_error(env, NULL, "Invalid AudioFIFO ID");
        return NULL;
    }
    
    av_audio_fifo_drain(entry->fifo, nb_samples);
    
    return NULL;
}

