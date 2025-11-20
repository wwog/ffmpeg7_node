/**
 * @file atomic_api.c
 * @brief Mid-level Atomic API - Exposes FFmpeg core components to JavaScript
 * @description Provides fine-grained FFmpeg operation interfaces, allowing JS to flexibly control encoding/decoding processes
 */

#include <node_api.h>
#include <stdlib.h>
#include <string.h>

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/opt.h"
#include "libavutil/dict.h"
#include "libavutil/pixdesc.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"

// ============================================================================
// Context Management - Manages FFmpeg context objects using handle mapping table
// ============================================================================

#define MAX_CONTEXTS 8192

typedef enum {
    CTX_TYPE_INPUT_FORMAT,
    CTX_TYPE_OUTPUT_FORMAT,
    CTX_TYPE_ENCODER,
    CTX_TYPE_DECODER,
    CTX_TYPE_FRAME,
    CTX_TYPE_PACKET,
    CTX_TYPE_SWS,
    CTX_TYPE_SWR
} ContextType;

typedef struct {
    int id;
    ContextType type;
    void *ptr;
    int in_use;
    AVDictionary *options; // For encoder/decoder options
    int64_t frame_counter; // Frame counter for encoders
} ContextEntry;

static ContextEntry context_table[MAX_CONTEXTS] = {0};
static int next_context_id = 1;

// Global array to store encoder time_bases and stream mappings
typedef struct {
    int encoder_ctx_id;
    int output_ctx_id;
    int stream_idx;
    AVRational encoder_time_base;
    int in_use;
} EncoderStreamMapping;

static EncoderStreamMapping encoder_stream_mappings[MAX_CONTEXTS] = {0};
static int mapping_count = 0;

// Helper to clean up encoder stream mappings for a context
static void cleanup_encoder_mappings(int encoder_ctx_id) {
    for (int i = 0; i < mapping_count; i++) {
        if (encoder_stream_mappings[i].in_use && 
            encoder_stream_mappings[i].encoder_ctx_id == encoder_ctx_id) {
            encoder_stream_mappings[i].in_use = 0;
        }
    }
}

// Allocate context ID
static int alloc_context_id(ContextType type, void *ptr) {
    for (int i = 0; i < MAX_CONTEXTS; i++) {
        if (!context_table[i].in_use) {
            context_table[i].id = next_context_id++;
            context_table[i].type = type;
            context_table[i].ptr = ptr;
            context_table[i].in_use = 1;
            context_table[i].options = NULL;
            return context_table[i].id;
        }
    }
    return -1;
}

// Get context pointer (exported for audio_fifo.c)
void* get_context_ptr(int id, ContextType expected_type) {
    for (int i = 0; i < MAX_CONTEXTS; i++) {
        if (context_table[i].in_use && 
            context_table[i].id == id && 
            context_table[i].type == expected_type) {
            return context_table[i].ptr;
        }
    }
    return NULL;
}

// Get context entry (for accessing options dictionary)
static ContextEntry* get_context_entry(int id) {
    for (int i = 0; i < MAX_CONTEXTS; i++) {
        if (context_table[i].in_use && context_table[i].id == id) {
            return &context_table[i];
        }
    }
    return NULL;
}

// Free context ID
static void free_context_id(int id) {
    for (int i = 0; i < MAX_CONTEXTS; i++) {
        if (context_table[i].in_use && context_table[i].id == id) {
            context_table[i].in_use = 0;
            context_table[i].ptr = NULL;
            if (context_table[i].options) {
                av_dict_free(&context_table[i].options);
                context_table[i].options = NULL;
            }
            break;
        }
    }
}

// ============================================================================
// 1. Input/Output Management
// ============================================================================

/**
 * Open input file
 * @param filePath - Input file path
 * @returns contextId - Context handle ID
 */
napi_value atomic_open_input(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 1;
    napi_value argv[1];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Expected file path argument");
        return NULL;
    }
    
    // Get file path
    char file_path[1024];
    size_t str_len;
    status = napi_get_value_string_utf8(env, argv[0], file_path, sizeof(file_path), &str_len);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to get file path");
        return NULL;
    }
    
    // Open input file
    AVFormatContext *fmt_ctx = NULL;
    int ret = avformat_open_input(&fmt_ctx, file_path, NULL, NULL);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        napi_throw_error(env, NULL, errbuf);
        return NULL;
    }
    
    // Read stream info
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        avformat_close_input(&fmt_ctx);
        napi_throw_error(env, NULL, "Failed to find stream info");
        return NULL;
    }
    
    // Allocate context ID
    int ctx_id = alloc_context_id(CTX_TYPE_INPUT_FORMAT, fmt_ctx);
    if (ctx_id < 0) {
        avformat_close_input(&fmt_ctx);
        napi_throw_error(env, NULL, "Too many open contexts");
        return NULL;
    }
    
    napi_value result;
    status = napi_create_int32(env, ctx_id, &result);
    return result;
}

/**
 * Create output context
 * @param filePath - Output file path
 * @param format - Output format (optional, e.g. "mp4")
 * @returns contextId - Context handle ID
 */
napi_value atomic_create_output(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 2;
    napi_value argv[2];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Expected file path argument");
        return NULL;
    }
    
    // Get file path
    char file_path[1024];
    size_t str_len;
    status = napi_get_value_string_utf8(env, argv[0], file_path, sizeof(file_path), &str_len);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to get file path");
        return NULL;
    }
    
    // Get format (optional)
    char format_name[64] = {0};
    if (argc >= 2) {
        napi_valuetype valuetype;
        napi_typeof(env, argv[1], &valuetype);
        if (valuetype == napi_string) {
            napi_get_value_string_utf8(env, argv[1], format_name, sizeof(format_name), &str_len);
        }
    }
    
    // Create output context
    AVFormatContext *fmt_ctx = NULL;
    int ret = avformat_alloc_output_context2(&fmt_ctx, NULL, 
                                             format_name[0] ? format_name : NULL, 
                                             file_path);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        napi_throw_error(env, NULL, errbuf);
        return NULL;
    }
    
    // Allocate context ID
    int ctx_id = alloc_context_id(CTX_TYPE_OUTPUT_FORMAT, fmt_ctx);
    if (ctx_id < 0) {
        avformat_free_context(fmt_ctx);
        napi_throw_error(env, NULL, "Too many open contexts");
        return NULL;
    }
    
    napi_value result;
    status = napi_create_int32(env, ctx_id, &result);
    return result;
}

/**
 * Get input stream information
 * @param contextId - Input context ID
 * @returns Stream information array
 */
napi_value atomic_get_input_streams(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 1;
    napi_value argv[1];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Expected context ID");
        return NULL;
    }
    
    int ctx_id;
    status = napi_get_value_int32(env, argv[0], &ctx_id);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Invalid context ID");
        return NULL;
    }
    
    AVFormatContext *fmt_ctx = get_context_ptr(ctx_id, CTX_TYPE_INPUT_FORMAT);
    if (!fmt_ctx) {
        napi_throw_error(env, NULL, "Invalid input context");
        return NULL;
    }
    
    // Create stream information array
    napi_value result_array;
    status = napi_create_array(env, &result_array);
    
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        AVStream *stream = fmt_ctx->streams[i];
        AVCodecParameters *codecpar = stream->codecpar;
        
        napi_value stream_obj;
        napi_create_object(env, &stream_obj);
        
        // Stream index
        napi_value index_val;
        napi_create_int32(env, i, &index_val);
        napi_set_named_property(env, stream_obj, "index", index_val);
        
        // Media type
        const char *media_type = av_get_media_type_string(codecpar->codec_type);
        if (media_type) {
            napi_value type_val;
            napi_create_string_utf8(env, media_type, NAPI_AUTO_LENGTH, &type_val);
            napi_set_named_property(env, stream_obj, "type", type_val);
        }
        
        // Codec name
        const AVCodecDescriptor *desc = avcodec_descriptor_get(codecpar->codec_id);
        if (desc) {
            napi_value codec_val;
            napi_create_string_utf8(env, desc->name, NAPI_AUTO_LENGTH, &codec_val);
            napi_set_named_property(env, stream_obj, "codec", codec_val);
        }
        
        // Video stream specific properties
        if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            napi_value width_val, height_val, fps_val;
            napi_create_int32(env, codecpar->width, &width_val);
            napi_create_int32(env, codecpar->height, &height_val);
            napi_set_named_property(env, stream_obj, "width", width_val);
            napi_set_named_property(env, stream_obj, "height", height_val);
            
            // Frame rate
            if (stream->avg_frame_rate.den > 0) {
                double fps = av_q2d(stream->avg_frame_rate);
                napi_create_double(env, fps, &fps_val);
                napi_set_named_property(env, stream_obj, "fps", fps_val);
            }
        }
        
        // Audio stream specific properties
        if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            napi_value sample_rate_val, channels_val;
            napi_create_int32(env, codecpar->sample_rate, &sample_rate_val);
            napi_create_int32(env, codecpar->ch_layout.nb_channels, &channels_val);
            napi_set_named_property(env, stream_obj, "sampleRate", sample_rate_val);
            napi_set_named_property(env, stream_obj, "channels", channels_val);
        }
        
        // Bit rate
        if (codecpar->bit_rate > 0) {
            napi_value bitrate_val;
            napi_create_int64(env, codecpar->bit_rate, &bitrate_val);
            napi_set_named_property(env, stream_obj, "bitrate", bitrate_val);
        }
        
        napi_set_element(env, result_array, i, stream_obj);
    }
    
    return result_array;
}

/**
 * Add output stream
 * @param outputContextId - Output context ID
 * @param codecName - Codec name (e.g. "libx264")
 * @returns streamIndex - New stream index
 */
napi_value atomic_add_output_stream(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 2;
    napi_value argv[2];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 2) {
        napi_throw_error(env, NULL, "Expected context ID and codec name");
        return NULL;
    }
    
    int ctx_id;
    status = napi_get_value_int32(env, argv[0], &ctx_id);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Invalid context ID");
        return NULL;
    }
    
    char codec_name[64];
    size_t str_len;
    status = napi_get_value_string_utf8(env, argv[1], codec_name, sizeof(codec_name), &str_len);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to get codec name");
        return NULL;
    }
    
    AVFormatContext *fmt_ctx = get_context_ptr(ctx_id, CTX_TYPE_OUTPUT_FORMAT);
    if (!fmt_ctx) {
        napi_throw_error(env, NULL, "Invalid output context");
        return NULL;
    }
    
    // Find codec
    const AVCodec *codec = avcodec_find_encoder_by_name(codec_name);
    if (!codec) {
        napi_throw_error(env, NULL, "Codec not found");
        return NULL;
    }
    
    // Create stream
    AVStream *stream = avformat_new_stream(fmt_ctx, NULL);
    if (!stream) {
        napi_throw_error(env, NULL, "Failed to create stream");
        return NULL;
    }
    
    // Set codec parameters
    stream->codecpar->codec_id = codec->id;
    stream->codecpar->codec_type = codec->type;
    
    napi_value result;
    napi_create_int32(env, stream->index, &result);
    return result;
}

/**
 * Close context
 * @param contextId - Context ID
 */
napi_value atomic_close_context(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 1;
    napi_value argv[1];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Expected context ID");
        return NULL;
    }
    
    int ctx_id;
    status = napi_get_value_int32(env, argv[0], &ctx_id);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Invalid context ID");
        return NULL;
    }
    
    // Find context
    for (int i = 0; i < MAX_CONTEXTS; i++) {
        if (context_table[i].in_use && context_table[i].id == ctx_id) {
            void *ptr = context_table[i].ptr;
            ContextType type = context_table[i].type;
            
            // Release resources based on type
            if (type == CTX_TYPE_INPUT_FORMAT) {
                AVFormatContext *fmt_ctx = (AVFormatContext *)ptr;
                avformat_close_input(&fmt_ctx);
            } else if (type == CTX_TYPE_OUTPUT_FORMAT) {
                AVFormatContext *fmt_ctx = (AVFormatContext *)ptr;
                if (fmt_ctx->pb) {
                    avio_closep(&fmt_ctx->pb);
                }
                avformat_free_context(fmt_ctx);
            } else if (type == CTX_TYPE_ENCODER || type == CTX_TYPE_DECODER) {
                AVCodecContext *codec_ctx = (AVCodecContext *)ptr;
                avcodec_free_context(&codec_ctx);
                // Clean up encoder stream mappings
                if (type == CTX_TYPE_ENCODER) {
                    cleanup_encoder_mappings(ctx_id);
                }
            } else if (type == CTX_TYPE_SWS) {
                struct SwsContext *sws_ctx = (struct SwsContext *)ptr;
                sws_freeContext(sws_ctx);
            } else if (type == CTX_TYPE_SWR) {
                struct SwrContext *swr_ctx = (struct SwrContext *)ptr;
                swr_free(&swr_ctx);
            }
            
            free_context_id(ctx_id);
            break;
        }
    }
    
    return NULL;
}

// ============================================================================
// 2. Codec Management
// ============================================================================

/**
 * Create encoder
 * @param codecName - Codec name
 * @returns codecContextId - Encoder context ID
 */
napi_value atomic_create_encoder(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 1;
    napi_value argv[1];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Expected codec name");
        return NULL;
    }
    
    char codec_name[64];
    size_t str_len;
    status = napi_get_value_string_utf8(env, argv[0], codec_name, sizeof(codec_name), &str_len);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to get codec name");
        return NULL;
    }
    
    // Find codec
    const AVCodec *codec = avcodec_find_encoder_by_name(codec_name);
    if (!codec) {
        napi_throw_error(env, NULL, "Encoder not found");
        return NULL;
    }
    
    // Create codec context
    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        napi_throw_error(env, NULL, "Failed to allocate codec context");
        return NULL;
    }
    
    // Allocate context ID with ENCODER type
    int ctx_id = alloc_context_id(CTX_TYPE_ENCODER, codec_ctx);
    if (ctx_id < 0) {
        avcodec_free_context(&codec_ctx);
        napi_throw_error(env, NULL, "Too many open contexts");
        return NULL;
    }
    
    napi_value result;
    napi_create_int32(env, ctx_id, &result);
    return result;
}

/**
 * Set encoder option
 * @param codecContextId - Encoder context ID
 * @param key - Option name (e.g. "threads", "preset", "crf")
 * @param value - Option value
 */
napi_value atomic_set_encoder_option(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 3;
    napi_value argv[3];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 3) {
        napi_throw_error(env, NULL, "Expected context ID, key, and value");
        return NULL;
    }
    
    int ctx_id;
    status = napi_get_value_int32(env, argv[0], &ctx_id);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Invalid context ID");
        return NULL;
    }
    
    char key[64];
    size_t key_len;
    status = napi_get_value_string_utf8(env, argv[1], key, sizeof(key), &key_len);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Invalid key");
        return NULL;
    }
    
    AVCodecContext *codec_ctx = get_context_ptr(ctx_id, CTX_TYPE_ENCODER);
    ContextEntry *entry = get_context_entry(ctx_id);
    if (!codec_ctx || !entry) {
        napi_throw_error(env, NULL, "Invalid encoder context");
        return NULL;
    }
    
    // Set option based on value type
    napi_valuetype valuetype;
    napi_typeof(env, argv[2], &valuetype);
    
    int ret = 0;
    if (valuetype == napi_number) {
        int32_t int_val;
        napi_get_value_int32(env, argv[2], &int_val);
        
        // Special handling for common codec context fields
        if (strcmp(key, "threads") == 0) {
            codec_ctx->thread_count = int_val;
        } else if (strcmp(key, "width") == 0) {
            codec_ctx->width = int_val;
        } else if (strcmp(key, "height") == 0) {
            codec_ctx->height = int_val;
        } else if (strcmp(key, "bitrate") == 0) {
            codec_ctx->bit_rate = int_val;
        } else if (strcmp(key, "sample_rate") == 0) {
            codec_ctx->sample_rate = int_val;
        } else if (strcmp(key, "channels") == 0) {
            av_channel_layout_default(&codec_ctx->ch_layout, int_val);
        } else if (strcmp(key, "time_base_num") == 0) {
            codec_ctx->time_base.num = int_val;
        } else if (strcmp(key, "time_base_den") == 0) {
            codec_ctx->time_base.den = int_val;
        } else if (strcmp(key, "framerate_num") == 0) {
            codec_ctx->framerate.num = int_val;
        } else if (strcmp(key, "framerate_den") == 0) {
            codec_ctx->framerate.den = int_val;
        } else if (strcmp(key, "gop_size") == 0) {
            codec_ctx->gop_size = int_val;
        } else if (strcmp(key, "max_b_frames") == 0) {
            codec_ctx->max_b_frames = int_val;
        } else {
            // Store in options dictionary for later use in avcodec_open2
            char val_str[32];
            snprintf(val_str, sizeof(val_str), "%d", int_val);
            ret = av_dict_set(&entry->options, key, val_str, 0);
        }
    } else if (valuetype == napi_string) {
        char str_val[256];
        size_t str_len;
        napi_get_value_string_utf8(env, argv[2], str_val, sizeof(str_val), &str_len);
        
        // Special handling for pixel format
        if (strcmp(key, "pix_fmt") == 0) {
            enum AVPixelFormat pix_fmt = av_get_pix_fmt(str_val);
            if (pix_fmt != AV_PIX_FMT_NONE) {
                codec_ctx->pix_fmt = pix_fmt;
            } else {
                char errbuf[256];
                snprintf(errbuf, sizeof(errbuf), "Invalid pixel format: %s", str_val);
                napi_throw_error(env, NULL, errbuf);
                return NULL;
            }
        } else if (strcmp(key, "sample_fmt") == 0) {
            enum AVSampleFormat sample_fmt = av_get_sample_fmt(str_val);
            if (sample_fmt != AV_SAMPLE_FMT_NONE) {
                codec_ctx->sample_fmt = sample_fmt;
            } else {
                char errbuf[256];
                snprintf(errbuf, sizeof(errbuf), "Invalid sample format: %s", str_val);
                napi_throw_error(env, NULL, errbuf);
                return NULL;
            }
        } else {
            // Store in options dictionary for later use in avcodec_open2
            ret = av_dict_set(&entry->options, key, str_val, 0);
        }
    }
    
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        napi_throw_error(env, NULL, errbuf);
        return NULL;
    }
    
    return NULL;
}

/**
 * Open encoder
 * @param codecContextId - Encoder context ID
 */
napi_value atomic_open_encoder(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 1;
    napi_value argv[1];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Expected context ID");
        return NULL;
    }
    
    int ctx_id;
    status = napi_get_value_int32(env, argv[0], &ctx_id);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Invalid context ID");
        return NULL;
    }
    
    AVCodecContext *codec_ctx = get_context_ptr(ctx_id, CTX_TYPE_ENCODER);
    ContextEntry *entry = get_context_entry(ctx_id);
    if (!codec_ctx || !entry) {
        napi_throw_error(env, NULL, "Invalid encoder context");
        return NULL;
    }
    
    // Open encoder with options dictionary
    AVDictionary *options = NULL;
    if (entry->options) {
        av_dict_copy(&options, entry->options, 0);
    }
    
    int ret = avcodec_open2(codec_ctx, codec_ctx->codec, &options);
    
    // Check for unrecognized options
    if (options) {
        AVDictionaryEntry *e = NULL;
        while ((e = av_dict_get(options, "", e, AV_DICT_IGNORE_SUFFIX))) {
            char warnbuf[256];
            snprintf(warnbuf, sizeof(warnbuf), "Warning: Unrecognized option '%s'", e->key);
            // Note: In production, you might want to log this instead of ignoring
        }
        av_dict_free(&options);
    }
    
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        napi_throw_error(env, NULL, errbuf);
        return NULL;
    }
    
    return NULL;
}

// ============================================================================
// 3. Transcoding Operations - Core transcoding functions
// ============================================================================

/**
 * Set output format option (e.g., movflags for faststart)
 * @param contextId - Output context ID
 * @param key - Option key (e.g., "movflags")
 * @param value - Option value (e.g., "+faststart")
 */
napi_value atomic_set_output_option(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 3;
    napi_value argv[3];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 3) {
        napi_throw_error(env, NULL, "Expected context ID, key, and value");
        return NULL;
    }
    
    int ctx_id;
    status = napi_get_value_int32(env, argv[0], &ctx_id);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Invalid context ID");
        return NULL;
    }
    
    AVFormatContext *fmt_ctx = get_context_ptr(ctx_id, CTX_TYPE_OUTPUT_FORMAT);
    ContextEntry *entry = get_context_entry(ctx_id);
    if (!fmt_ctx || !entry) {
        napi_throw_error(env, NULL, "Invalid output context");
        return NULL;
    }
    
    char key[256];
    char value[256];
    size_t str_len;
    
    status = napi_get_value_string_utf8(env, argv[1], key, sizeof(key), &str_len);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to get key");
        return NULL;
    }
    
    status = napi_get_value_string_utf8(env, argv[2], value, sizeof(value), &str_len);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to get value");
        return NULL;
    }
    
    // Store option in context entry's options dictionary
    // These options will be passed to avformat_write_header
    int ret = av_dict_set(&entry->options, key, value, 0);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        napi_throw_error(env, NULL, errbuf);
        return NULL;
    }
    
    return NULL;
}

/**
 * Write output file header
 * @param contextId - Output context ID
 */
napi_value atomic_write_header(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 1;
    napi_value argv[1];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Expected context ID");
        return NULL;
    }
    
    int ctx_id;
    status = napi_get_value_int32(env, argv[0], &ctx_id);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Invalid context ID");
        return NULL;
    }
    
    AVFormatContext *fmt_ctx = get_context_ptr(ctx_id, CTX_TYPE_OUTPUT_FORMAT);
    ContextEntry *entry = get_context_entry(ctx_id);
    if (!fmt_ctx || !entry) {
        napi_throw_error(env, NULL, "Invalid output context");
        return NULL;
    }
    
    // Open output file
    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        int ret = avio_open(&fmt_ctx->pb, fmt_ctx->url, AVIO_FLAG_WRITE);
        if (ret < 0) {
            char errbuf[128];
            av_strerror(ret, errbuf, sizeof(errbuf));
            napi_throw_error(env, NULL, errbuf);
            return NULL;
        }
    }
    
    // Write file header with stored options (e.g., movflags=+faststart)
    AVDictionary *options = NULL;
    if (entry->options) {
        av_dict_copy(&options, entry->options, 0);
    }
    
    int ret = avformat_write_header(fmt_ctx, &options);
    
    // Check for unrecognized options
    if (options) {
        AVDictionaryEntry *t = NULL;
        while ((t = av_dict_get(options, "", t, AV_DICT_IGNORE_SUFFIX))) {
            fprintf(stderr, "Warning: Option '%s' not recognized by muxer\n", t->key);
        }
        av_dict_free(&options);
    }
    
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        napi_throw_error(env, NULL, errbuf);
        return NULL;
    }
    
    return NULL;
}

/**
 * Write output file trailer
 * @param contextId - Output context ID
 */
napi_value atomic_write_trailer(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 1;
    napi_value argv[1];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Expected context ID");
        return NULL;
    }
    
    int ctx_id;
    status = napi_get_value_int32(env, argv[0], &ctx_id);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Invalid context ID");
        return NULL;
    }
    
    AVFormatContext *fmt_ctx = get_context_ptr(ctx_id, CTX_TYPE_OUTPUT_FORMAT);
    if (!fmt_ctx) {
        napi_throw_error(env, NULL, "Invalid output context");
        return NULL;
    }
    
    int ret = av_write_trailer(fmt_ctx);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        napi_throw_error(env, NULL, errbuf);
        return NULL;
    }
    
    return NULL;
}

/**
 * Copy stream parameters from input to output stream
 * @param inputContextId - Input context ID
 * @param outputContextId - Output context ID
 * @param inputStreamIndex - Input stream index
 * @param outputStreamIndex - Output stream index
 */
napi_value atomic_copy_stream_params(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 4;
    napi_value argv[4];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 4) {
        napi_throw_error(env, NULL, "Expected input ctx, output ctx, input stream idx, output stream idx");
        return NULL;
    }
    
    int input_ctx_id, output_ctx_id, input_stream_idx, output_stream_idx;
    napi_get_value_int32(env, argv[0], &input_ctx_id);
    napi_get_value_int32(env, argv[1], &output_ctx_id);
    napi_get_value_int32(env, argv[2], &input_stream_idx);
    napi_get_value_int32(env, argv[3], &output_stream_idx);
    
    AVFormatContext *input_fmt_ctx = get_context_ptr(input_ctx_id, CTX_TYPE_INPUT_FORMAT);
    AVFormatContext *output_fmt_ctx = get_context_ptr(output_ctx_id, CTX_TYPE_OUTPUT_FORMAT);
    
    if (!input_fmt_ctx || !output_fmt_ctx) {
        napi_throw_error(env, NULL, "Invalid context");
        return NULL;
    }
    
    if (input_stream_idx >= (int)input_fmt_ctx->nb_streams || 
        output_stream_idx >= (int)output_fmt_ctx->nb_streams) {
        napi_throw_error(env, NULL, "Invalid stream index");
        return NULL;
    }
    
    AVStream *in_stream = input_fmt_ctx->streams[input_stream_idx];
    AVStream *out_stream = output_fmt_ctx->streams[output_stream_idx];
    
    int ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        napi_throw_error(env, NULL, errbuf);
        return NULL;
    }
    
    out_stream->time_base = in_stream->time_base;
    
    return NULL;
}

/**
 * Copy encoder parameters to output stream
 * @param encoderContextId - Encoder context ID
 * @param outputContextId - Output context ID
 * @param outputStreamIndex - Output stream index
 */
napi_value atomic_copy_encoder_to_stream(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 3;
    napi_value argv[3];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 3) {
        napi_throw_error(env, NULL, "Expected encoder ctx id, output ctx id, output stream idx");
        return NULL;
    }
    
    int encoder_ctx_id, output_ctx_id, output_stream_idx;
    napi_get_value_int32(env, argv[0], &encoder_ctx_id);
    napi_get_value_int32(env, argv[1], &output_ctx_id);
    napi_get_value_int32(env, argv[2], &output_stream_idx);
    
    AVCodecContext *codec_ctx = get_context_ptr(encoder_ctx_id, CTX_TYPE_ENCODER);
    AVFormatContext *output_fmt_ctx = get_context_ptr(output_ctx_id, CTX_TYPE_OUTPUT_FORMAT);
    
    if (!codec_ctx || !output_fmt_ctx) {
        napi_throw_error(env, NULL, "Invalid context");
        return NULL;
    }
    
    if (output_stream_idx >= (int)output_fmt_ctx->nb_streams) {
        napi_throw_error(env, NULL, "Invalid stream index");
        return NULL;
    }
    
    AVStream *out_stream = output_fmt_ctx->streams[output_stream_idx];
    
    // Copy encoder parameters to stream
    int ret = avcodec_parameters_from_context(out_stream->codecpar, codec_ctx);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        napi_throw_error(env, NULL, errbuf);
        return NULL;
    }
    
    // Let muxer set the time_base (it will set it when writing header)
    // But store the encoder's time_base for timestamp rescaling in writePacket
    // Find or create mapping slot
    int slot = -1;
    for (int i = 0; i < mapping_count; i++) {
        if (!encoder_stream_mappings[i].in_use) {
            slot = i;
            break;
        }
    }
    if (slot == -1 && mapping_count < MAX_CONTEXTS) {
        slot = mapping_count++;
    }
    
    if (slot >= 0) {
        encoder_stream_mappings[slot].encoder_ctx_id = encoder_ctx_id;
        encoder_stream_mappings[slot].output_ctx_id = output_ctx_id;
        encoder_stream_mappings[slot].stream_idx = output_stream_idx;
        encoder_stream_mappings[slot].encoder_time_base = codec_ctx->time_base;
        encoder_stream_mappings[slot].in_use = 1;
    }
    
    return NULL;
}

/**
 * Read packet from input
 * @param inputContextId - Input context ID
 * @returns packet object with data or null if EOF
 */
napi_value atomic_read_packet(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 1;
    napi_value argv[1];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Expected context ID");
        return NULL;
    }
    
    int ctx_id;
    status = napi_get_value_int32(env, argv[0], &ctx_id);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Invalid context ID");
        return NULL;
    }
    
    AVFormatContext *fmt_ctx = get_context_ptr(ctx_id, CTX_TYPE_INPUT_FORMAT);
    if (!fmt_ctx) {
        napi_throw_error(env, NULL, "Invalid input context");
        return NULL;
    }
    
    AVPacket *pkt = av_packet_alloc();
    if (!pkt) {
        napi_throw_error(env, NULL, "Failed to allocate packet");
        return NULL;
    }
    
    int ret = av_read_frame(fmt_ctx, pkt);
    if (ret < 0) {
        av_packet_free(&pkt);
        if (ret == AVERROR_EOF) {
            return NULL; // Return null for EOF
        }
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        napi_throw_error(env, NULL, errbuf);
        return NULL;
    }
    
    // Allocate packet context and return ID
    int pkt_id = alloc_context_id(CTX_TYPE_PACKET, pkt);
    if (pkt_id < 0) {
        av_packet_free(&pkt);
        napi_throw_error(env, NULL, "Too many open contexts");
        return NULL;
    }
    
    // Create packet info object
    napi_value packet_obj;
    napi_create_object(env, &packet_obj);
    
    napi_value pkt_id_val, stream_idx_val, pts_val, dts_val, duration_val;
    napi_create_int32(env, pkt_id, &pkt_id_val);
    napi_create_int32(env, pkt->stream_index, &stream_idx_val);
    napi_create_int64(env, pkt->pts, &pts_val);
    napi_create_int64(env, pkt->dts, &dts_val);
    napi_create_int64(env, pkt->duration, &duration_val);
    
    napi_set_named_property(env, packet_obj, "id", pkt_id_val);
    napi_set_named_property(env, packet_obj, "streamIndex", stream_idx_val);
    napi_set_named_property(env, packet_obj, "pts", pts_val);
    napi_set_named_property(env, packet_obj, "dts", dts_val);
    napi_set_named_property(env, packet_obj, "duration", duration_val);
    
    return packet_obj;
}

/**
 * Write packet to output
 * @param outputContextId - Output context ID
 * @param packetId - Packet ID (from readPacket or encoder)
 * @param outputStreamIndex - Output stream index
 * @param inputContextId - (Optional) Input context ID for time_base rescaling
 * @param inputStreamIndex - (Optional) Input stream index for time_base rescaling
 */
napi_value atomic_write_packet(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 5;
    napi_value argv[5];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 3) {
        napi_throw_error(env, NULL, "Expected output ctx, packet id, output stream idx");
        return NULL;
    }
    
    int output_ctx_id, pkt_id, output_stream_idx;
    napi_get_value_int32(env, argv[0], &output_ctx_id);
    napi_get_value_int32(env, argv[1], &pkt_id);
    napi_get_value_int32(env, argv[2], &output_stream_idx);
    
    AVFormatContext *fmt_ctx = get_context_ptr(output_ctx_id, CTX_TYPE_OUTPUT_FORMAT);
    AVPacket *pkt = get_context_ptr(pkt_id, CTX_TYPE_PACKET);
    
    if (!fmt_ctx || !pkt) {
        napi_throw_error(env, NULL, "Invalid context or packet");
        return NULL;
    }
    
    if (output_stream_idx >= (int)fmt_ctx->nb_streams) {
        napi_throw_error(env, NULL, "Invalid stream index");
        return NULL;
    }
    
    // Create a copy of the packet to modify
    AVPacket *out_pkt = av_packet_clone(pkt);
    if (!out_pkt) {
        napi_throw_error(env, NULL, "Failed to clone packet");
        return NULL;
    }
    
    out_pkt->stream_index = output_stream_idx;
    
    AVStream *out_stream = fmt_ctx->streams[output_stream_idx];
    
    // Determine source time_base for timestamp rescaling
    AVRational src_tb = {0, 1};
    
    // If input context and stream index provided, use input stream time_base
    if (argc >= 5) {
        int input_ctx_id, input_stream_idx;
        napi_valuetype input_ctx_type, input_stream_type;
        napi_typeof(env, argv[3], &input_ctx_type);
        napi_typeof(env, argv[4], &input_stream_type);
        
        if (input_ctx_type == napi_number && input_stream_type == napi_number) {
            napi_get_value_int32(env, argv[3], &input_ctx_id);
            napi_get_value_int32(env, argv[4], &input_stream_idx);
            
            AVFormatContext *input_fmt_ctx = get_context_ptr(input_ctx_id, CTX_TYPE_INPUT_FORMAT);
            if (input_fmt_ctx && input_stream_idx < (int)input_fmt_ctx->nb_streams) {
                src_tb = input_fmt_ctx->streams[input_stream_idx]->time_base;
            }
        }
    }
    
    // If no input time_base, check for encoder time_base mapping
    if (src_tb.num == 0) {
        for (int i = 0; i < mapping_count; i++) {
            if (encoder_stream_mappings[i].in_use &&
                encoder_stream_mappings[i].output_ctx_id == output_ctx_id &&
                encoder_stream_mappings[i].stream_idx == output_stream_idx) {
                src_tb = encoder_stream_mappings[i].encoder_time_base;
                break;
            }
        }
    }
    
    // Rescale timestamps if we have a valid source time_base
    if (src_tb.num != 0 && out_stream->time_base.num != 0) {
        av_packet_rescale_ts(out_pkt, src_tb, out_stream->time_base);
    }
    
    int ret = av_interleaved_write_frame(fmt_ctx, out_pkt);
    
    av_packet_free(&out_pkt);
    
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        napi_throw_error(env, NULL, errbuf);
        return NULL;
    }
    
    return NULL;
}

/**
 * Free packet
 * @param packetId - Packet ID
 */
napi_value atomic_free_packet(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 1;
    napi_value argv[1];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Expected packet ID");
        return NULL;
    }
    
    int pkt_id;
    status = napi_get_value_int32(env, argv[0], &pkt_id);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Invalid packet ID");
        return NULL;
    }
    
    AVPacket *pkt = get_context_ptr(pkt_id, CTX_TYPE_PACKET);
    if (pkt) {
        av_packet_free(&pkt);
        free_context_id(pkt_id);
    }
    
    return NULL;
}

// ============================================================================
// 4. Decoder Management
// ============================================================================

/**
 * Create decoder
 * @param codecName - Codec name
 * @returns codecContextId - Decoder context ID
 */
napi_value atomic_create_decoder(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 1;
    napi_value argv[1];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Expected codec name");
        return NULL;
    }
    
    char codec_name[64];
    size_t str_len;
    status = napi_get_value_string_utf8(env, argv[0], codec_name, sizeof(codec_name), &str_len);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to get codec name");
        return NULL;
    }
    
    // Find decoder
    const AVCodec *codec = avcodec_find_decoder_by_name(codec_name);
    if (!codec) {
        napi_throw_error(env, NULL, "Decoder not found");
        return NULL;
    }
    
    // Create codec context
    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        napi_throw_error(env, NULL, "Failed to allocate codec context");
        return NULL;
    }
    
    // Allocate context ID with DECODER type
    int ctx_id = alloc_context_id(CTX_TYPE_DECODER, codec_ctx);
    if (ctx_id < 0) {
        avcodec_free_context(&codec_ctx);
        napi_throw_error(env, NULL, "Too many open contexts");
        return NULL;
    }
    
    napi_value result;
    napi_create_int32(env, ctx_id, &result);
    return result;
}

/**
 * Copy codec parameters from stream to decoder context
 * @param inputContextId - Input format context ID
 * @param decoderContextId - Decoder context ID
 * @param streamIndex - Stream index
 */
napi_value atomic_copy_decoder_params(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 3;
    napi_value argv[3];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 3) {
        napi_throw_error(env, NULL, "Expected input context ID, decoder context ID, and stream index");
        return NULL;
    }
    
    int input_ctx_id, decoder_ctx_id, stream_idx;
    napi_get_value_int32(env, argv[0], &input_ctx_id);
    napi_get_value_int32(env, argv[1], &decoder_ctx_id);
    napi_get_value_int32(env, argv[2], &stream_idx);
    
    AVFormatContext *fmt_ctx = get_context_ptr(input_ctx_id, CTX_TYPE_INPUT_FORMAT);
    AVCodecContext *codec_ctx = get_context_ptr(decoder_ctx_id, CTX_TYPE_DECODER);
    
    if (!fmt_ctx || !codec_ctx) {
        napi_throw_error(env, NULL, "Invalid context");
        return NULL;
    }
    
    if (stream_idx < 0 || stream_idx >= fmt_ctx->nb_streams) {
        napi_throw_error(env, NULL, "Invalid stream index");
        return NULL;
    }
    
    // Copy parameters from stream to decoder
    int ret = avcodec_parameters_to_context(codec_ctx, fmt_ctx->streams[stream_idx]->codecpar);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        napi_throw_error(env, NULL, errbuf);
        return NULL;
    }
    
    return NULL;
}

/**
 * Open decoder
 * @param codecContextId - Decoder context ID
 */
napi_value atomic_open_decoder(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 1;
    napi_value argv[1];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Expected context ID");
        return NULL;
    }
    
    int ctx_id;
    status = napi_get_value_int32(env, argv[0], &ctx_id);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Invalid context ID");
        return NULL;
    }
    
    AVCodecContext *codec_ctx = get_context_ptr(ctx_id, CTX_TYPE_DECODER);
    if (!codec_ctx) {
        napi_throw_error(env, NULL, "Invalid decoder context");
        return NULL;
    }
    
    int ret = avcodec_open2(codec_ctx, codec_ctx->codec, NULL);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        napi_throw_error(env, NULL, errbuf);
        return NULL;
    }
    
    return NULL;
}

// ============================================================================
// 5. Frame and Packet Processing
// ============================================================================

/**
 * Allocate a new frame
 * @returns frameId - Frame ID
 */
napi_value atomic_alloc_frame(napi_env env, napi_callback_info info) {
    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        napi_throw_error(env, NULL, "Failed to allocate frame");
        return NULL;
    }
    
    int frame_id = alloc_context_id(CTX_TYPE_FRAME, frame);
    if (frame_id < 0) {
        av_frame_free(&frame);
        napi_throw_error(env, NULL, "Too many open contexts");
        return NULL;
    }
    
    napi_value result;
    napi_create_int32(env, frame_id, &result);
    return result;
}

/**
 * Send packet to decoder
 * @param decoderContextId - Decoder context ID
 * @param packetId - Packet ID (or null to flush)
 * @returns Status: 0 (success), -1 (EAGAIN), -2 (EOF), -3 (error)
 */
napi_value atomic_send_packet(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 2;
    napi_value argv[2];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Expected decoder context ID");
        return NULL;
    }
    
    int decoder_ctx_id;
    napi_get_value_int32(env, argv[0], &decoder_ctx_id);
    
    AVCodecContext *codec_ctx = get_context_ptr(decoder_ctx_id, CTX_TYPE_DECODER);
    if (!codec_ctx) {
        napi_throw_error(env, NULL, "Invalid decoder context");
        return NULL;
    }
    
    AVPacket *pkt = NULL;
    if (argc >= 2) {
        napi_valuetype valuetype;
        napi_typeof(env, argv[1], &valuetype);
        if (valuetype != napi_null && valuetype != napi_undefined) {
            int pkt_id;
            napi_get_value_int32(env, argv[1], &pkt_id);
            pkt = get_context_ptr(pkt_id, CTX_TYPE_PACKET);
        }
    }
    
    int ret = avcodec_send_packet(codec_ctx, pkt);
    
    napi_value result;
    if (ret == 0) {
        napi_create_int32(env, 0, &result);
    } else if (ret == AVERROR(EAGAIN)) {
        napi_create_int32(env, -1, &result);
    } else if (ret == AVERROR_EOF) {
        napi_create_int32(env, -2, &result);
    } else {
        napi_create_int32(env, -3, &result);
    }
    
    return result;
}

/**
 * Receive frame from decoder
 * @param decoderContextId - Decoder context ID
 * @param frameId - Frame ID to receive into
 * @returns Status: 0 (success), -1 (EAGAIN), -2 (EOF), -3 (error)
 */
napi_value atomic_receive_frame(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 2;
    napi_value argv[2];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 2) {
        napi_throw_error(env, NULL, "Expected decoder context ID and frame ID");
        return NULL;
    }
    
    int decoder_ctx_id, frame_id;
    napi_get_value_int32(env, argv[0], &decoder_ctx_id);
    napi_get_value_int32(env, argv[1], &frame_id);
    
    AVCodecContext *codec_ctx = get_context_ptr(decoder_ctx_id, CTX_TYPE_DECODER);
    AVFrame *frame = get_context_ptr(frame_id, CTX_TYPE_FRAME);
    
    if (!codec_ctx || !frame) {
        napi_throw_error(env, NULL, "Invalid context or frame");
        return NULL;
    }
    
    int ret = avcodec_receive_frame(codec_ctx, frame);
    
    napi_value result;
    if (ret == 0) {
        napi_create_int32(env, 0, &result);
    } else if (ret == AVERROR(EAGAIN)) {
        napi_create_int32(env, -1, &result);
    } else if (ret == AVERROR_EOF) {
        napi_create_int32(env, -2, &result);
    } else {
        napi_create_int32(env, -3, &result);
    }
    
    return result;
}

/**
 * Send frame to encoder
 * @param encoderContextId - Encoder context ID
 * @param frameId - Frame ID (or null to flush)
 * @returns Status: 0 (success), -1 (EAGAIN), -2 (EOF), -3 (error)
 */
napi_value atomic_send_frame(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 2;
    napi_value argv[2];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Expected encoder context ID");
        return NULL;
    }
    
    int encoder_ctx_id;
    napi_get_value_int32(env, argv[0], &encoder_ctx_id);
    
    AVCodecContext *codec_ctx = get_context_ptr(encoder_ctx_id, CTX_TYPE_ENCODER);
    ContextEntry *entry = get_context_entry(encoder_ctx_id);
    if (!codec_ctx || !entry) {
        napi_throw_error(env, NULL, "Invalid encoder context");
        return NULL;
    }
    
    AVFrame *frame = NULL;
    if (argc >= 2) {
        napi_valuetype valuetype;
        napi_typeof(env, argv[1], &valuetype);
        if (valuetype != napi_null && valuetype != napi_undefined) {
            int frame_id;
            napi_get_value_int32(env, argv[1], &frame_id);
            frame = get_context_ptr(frame_id, CTX_TYPE_FRAME);
            
            if (frame) {
                // 清除解码帧的类型信息，让编码器自己决定帧类型
                frame->pict_type = AV_PICTURE_TYPE_NONE;
                
                // 为帧设置正确的pts
                // 使用帧计数器来生成递增的pts，确保编码器输出正确的时间戳
                frame->pts = entry->frame_counter++;
            }
        }
        // Note: When flushing (null frame), don't reset the counter
        // The counter represents total frames sent, not current batch
    }
    
    int ret = avcodec_send_frame(codec_ctx, frame);
    
    napi_value result;
    if (ret == 0) {
        napi_create_int32(env, 0, &result);
    } else if (ret == AVERROR(EAGAIN)) {
        napi_create_int32(env, -1, &result);
    } else if (ret == AVERROR_EOF) {
        napi_create_int32(env, -2, &result);
    } else {
        napi_create_int32(env, -3, &result);
    }
    
    return result;
}

/**
 * Receive packet from encoder
 * @param encoderContextId - Encoder context ID
 * @param packetId - Packet ID to receive into
 * @returns Status: 0 (success), -1 (EAGAIN), -2 (EOF), -3 (error)
 */
napi_value atomic_receive_packet(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 2;
    napi_value argv[2];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 2) {
        napi_throw_error(env, NULL, "Expected encoder context ID and packet ID");
        return NULL;
    }
    
    int encoder_ctx_id, pkt_id;
    napi_get_value_int32(env, argv[0], &encoder_ctx_id);
    napi_get_value_int32(env, argv[1], &pkt_id);
    
    AVCodecContext *codec_ctx = get_context_ptr(encoder_ctx_id, CTX_TYPE_ENCODER);
    AVPacket *pkt = get_context_ptr(pkt_id, CTX_TYPE_PACKET);
    
    if (!codec_ctx || !pkt) {
        napi_throw_error(env, NULL, "Invalid context or packet");
        return NULL;
    }
    
    int ret = avcodec_receive_packet(codec_ctx, pkt);
    
    napi_value result;
    if (ret == 0) {
        napi_create_int32(env, 0, &result);
    } else if (ret == AVERROR(EAGAIN)) {
        napi_create_int32(env, -1, &result);
    } else if (ret == AVERROR_EOF) {
        napi_create_int32(env, -2, &result);
    } else {
        napi_create_int32(env, -3, &result);
    }
    
    return result;
}

/**
 * Free frame
 * @param frameId - Frame ID
 */
napi_value atomic_free_frame(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 1;
    napi_value argv[1];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Expected frame ID");
        return NULL;
    }
    
    int frame_id;
    status = napi_get_value_int32(env, argv[0], &frame_id);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Invalid frame ID");
        return NULL;
    }
    
    AVFrame *frame = get_context_ptr(frame_id, CTX_TYPE_FRAME);
    if (frame) {
        av_frame_free(&frame);
        free_context_id(frame_id);
    }
    
    return NULL;
}

/**
 * Allocate an output packet
 * @returns packetId - Packet ID
 */
napi_value atomic_alloc_packet(napi_env env, napi_callback_info info) {
    AVPacket *pkt = av_packet_alloc();
    if (!pkt) {
        napi_throw_error(env, NULL, "Failed to allocate packet");
        return NULL;
    }
    
    int pkt_id = alloc_context_id(CTX_TYPE_PACKET, pkt);
    if (pkt_id < 0) {
        av_packet_free(&pkt);
        napi_throw_error(env, NULL, "Too many open contexts");
        return NULL;
    }
    
    napi_value result;
    napi_create_int32(env, pkt_id, &result);
    return result;
}

// ============================================================================
// 6. Helper Functions - Get available codec and format lists
// ============================================================================

/**
 * Get available encoder list
 * @param type - Type filter ("video", "audio" or undefined for all)
 * @returns Encoder name array
 */
napi_value atomic_get_encoder_list(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 1;
    napi_value argv[1];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    
    enum AVMediaType filter_type = AVMEDIA_TYPE_UNKNOWN;
    if (argc >= 1) {
        char type_str[16];
        size_t str_len;
        napi_valuetype valuetype;
        napi_typeof(env, argv[0], &valuetype);
        
        if (valuetype == napi_string) {
            napi_get_value_string_utf8(env, argv[0], type_str, sizeof(type_str), &str_len);
            if (strcmp(type_str, "video") == 0) {
                filter_type = AVMEDIA_TYPE_VIDEO;
            } else if (strcmp(type_str, "audio") == 0) {
                filter_type = AVMEDIA_TYPE_AUDIO;
            }
        }
    }
    
    napi_value result_array;
    napi_create_array(env, &result_array);
    
    void *opaque = NULL;
    const AVCodec *codec = NULL;
    int index = 0;
    
    while ((codec = av_codec_iterate(&opaque))) {
        if (av_codec_is_encoder(codec)) {
            if (filter_type == AVMEDIA_TYPE_UNKNOWN || codec->type == filter_type) {
                napi_value codec_name;
                napi_create_string_utf8(env, codec->name, NAPI_AUTO_LENGTH, &codec_name);
                napi_set_element(env, result_array, index++, codec_name);
            }
        }
    }
    
    return result_array;
}

/**
 * Get available output format list
 * @returns Format name array
 */
napi_value atomic_get_muxer_list(napi_env env, napi_callback_info info) {
    napi_value result_array;
    napi_create_array(env, &result_array);
    
    void *opaque = NULL;
    const AVOutputFormat *fmt = NULL;
    int index = 0;
    
    while ((fmt = av_muxer_iterate(&opaque))) {
        napi_value fmt_name;
        napi_create_string_utf8(env, fmt->name, NAPI_AUTO_LENGTH, &fmt_name);
        napi_set_element(env, result_array, index++, fmt_name);
    }
    
    return result_array;
}

// ============================================================================
// 7. Frame Data Access and Manipulation
// ============================================================================

/**
 * Allocate frame buffer
 * @param frameId - Frame ID
 * @param align - Buffer alignment (0 for default)
 */
napi_value atomic_frame_get_buffer(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 2;
    napi_value argv[2];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Expected frame ID");
        return NULL;
    }
    
    int frame_id;
    napi_get_value_int32(env, argv[0], &frame_id);
    
    AVFrame *frame = get_context_ptr(frame_id, CTX_TYPE_FRAME);
    if (!frame) {
        napi_throw_error(env, NULL, "Invalid frame");
        return NULL;
    }
    
    int align = 0;
    if (argc >= 2) {
        napi_get_value_int32(env, argv[1], &align);
    }
    
    int ret = av_frame_get_buffer(frame, align);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        napi_throw_error(env, NULL, errbuf);
        return NULL;
    }
    
    return NULL;
}

/**
 * Set frame properties
 * @param frameId - Frame ID
 * @param property - Property name (pts, width, height, format, etc.)
 * @param value - Property value
 */
napi_value atomic_set_frame_property(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 3;
    napi_value argv[3];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 3) {
        napi_throw_error(env, NULL, "Expected frame ID, property name, and value");
        return NULL;
    }
    
    int frame_id;
    napi_get_value_int32(env, argv[0], &frame_id);
    
    char property[64];
    size_t str_len;
    napi_get_value_string_utf8(env, argv[1], property, sizeof(property), &str_len);
    
    AVFrame *frame = get_context_ptr(frame_id, CTX_TYPE_FRAME);
    if (!frame) {
        napi_throw_error(env, NULL, "Invalid frame");
        return NULL;
    }
    
    napi_valuetype valuetype;
    napi_typeof(env, argv[2], &valuetype);
    
    if (strcmp(property, "pts") == 0 && valuetype == napi_number) {
        int64_t pts;
        napi_get_value_int64(env, argv[2], &pts);
        frame->pts = pts;
    } else if (strcmp(property, "width") == 0 && valuetype == napi_number) {
        int32_t width;
        napi_get_value_int32(env, argv[2], &width);
        frame->width = width;
    } else if (strcmp(property, "height") == 0 && valuetype == napi_number) {
        int32_t height;
        napi_get_value_int32(env, argv[2], &height);
        frame->height = height;
    } else if (strcmp(property, "format") == 0 && valuetype == napi_number) {
        int32_t format;
        napi_get_value_int32(env, argv[2], &format);
        frame->format = format;
    } else if (strcmp(property, "pict_type") == 0 && valuetype == napi_number) {
        int32_t pict_type;
        napi_get_value_int32(env, argv[2], &pict_type);
        frame->pict_type = (enum AVPictureType)pict_type;
    } else if (strcmp(property, "key_frame") == 0 && valuetype == napi_number) {
        int32_t key_frame;
        napi_get_value_int32(env, argv[2], &key_frame);
        frame->key_frame = key_frame;
    } else if (strcmp(property, "sample_rate") == 0 && valuetype == napi_number) {
        int32_t sample_rate;
        napi_get_value_int32(env, argv[2], &sample_rate);
        frame->sample_rate = sample_rate;
    } else if (strcmp(property, "nb_samples") == 0 && valuetype == napi_number) {
        int32_t nb_samples;
        napi_get_value_int32(env, argv[2], &nb_samples);
        frame->nb_samples = nb_samples;
    } else if (strcmp(property, "channels") == 0 && valuetype == napi_number) {
        int32_t channels;
        napi_get_value_int32(env, argv[2], &channels);
        av_channel_layout_uninit(&frame->ch_layout);
        av_channel_layout_default(&frame->ch_layout, channels);
    } else {
        napi_throw_error(env, NULL, "Unknown or unsupported property");
        return NULL;
    }
    
    return NULL;
}

/**
 * Get frame property
 * @param frameId - Frame ID
 * @param property - Property name
 * @returns Property value
 */
napi_value atomic_get_frame_property(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 2;
    napi_value argv[2];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 2) {
        napi_throw_error(env, NULL, "Expected frame ID and property name");
        return NULL;
    }
    
    int frame_id;
    napi_get_value_int32(env, argv[0], &frame_id);
    
    char property[64];
    size_t str_len;
    napi_get_value_string_utf8(env, argv[1], property, sizeof(property), &str_len);
    
    AVFrame *frame = get_context_ptr(frame_id, CTX_TYPE_FRAME);
    if (!frame) {
        napi_throw_error(env, NULL, "Invalid frame");
        return NULL;
    }
    
    napi_value result;
    
    if (strcmp(property, "pts") == 0) {
        napi_create_int64(env, frame->pts, &result);
    } else if (strcmp(property, "width") == 0) {
        napi_create_int32(env, frame->width, &result);
    } else if (strcmp(property, "height") == 0) {
        napi_create_int32(env, frame->height, &result);
    } else if (strcmp(property, "format") == 0) {
        napi_create_int32(env, frame->format, &result);
    } else if (strcmp(property, "pict_type") == 0) {
        napi_create_int32(env, frame->pict_type, &result);
    } else if (strcmp(property, "key_frame") == 0) {
        napi_create_int32(env, frame->key_frame, &result);
    } else if (strcmp(property, "sample_rate") == 0) {
        napi_create_int32(env, frame->sample_rate, &result);
    } else if (strcmp(property, "nb_samples") == 0) {
        napi_create_int32(env, frame->nb_samples, &result);
    } else if (strcmp(property, "channels") == 0) {
        napi_create_int32(env, frame->ch_layout.nb_channels, &result);
    } else if (strcmp(property, "linesize") == 0) {
        napi_create_array(env, &result);
        for (int i = 0; i < AV_NUM_DATA_POINTERS; i++) {
            napi_value val;
            napi_create_int32(env, frame->linesize[i], &val);
            napi_set_element(env, result, i, val);
        }
    } else {
        napi_throw_error(env, NULL, "Unknown property");
        return NULL;
    }
    
    return result;
}

/**
 * Get frame data as Buffer
 * @param frameId - Frame ID
 * @param planeIndex - Plane index (0-7)
 * @returns Node.js Buffer containing plane data
 */
napi_value atomic_get_frame_data(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 2;
    napi_value argv[2];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 2) {
        napi_throw_error(env, NULL, "Expected frame ID and plane index");
        return NULL;
    }
    
    int frame_id, plane_idx;
    napi_get_value_int32(env, argv[0], &frame_id);
    napi_get_value_int32(env, argv[1], &plane_idx);
    
    AVFrame *frame = get_context_ptr(frame_id, CTX_TYPE_FRAME);
    if (!frame) {
        napi_throw_error(env, NULL, "Invalid frame");
        return NULL;
    }
    
    if (plane_idx < 0 || plane_idx >= AV_NUM_DATA_POINTERS) {
        napi_throw_error(env, NULL, "Invalid plane index");
        return NULL;
    }
    
    if (!frame->data[plane_idx]) {
        return NULL; // Return null for unused planes
    }
    
    // Calculate plane size
    size_t plane_size;
    if (frame->height > 0) {
        // Video frame
        plane_size = frame->linesize[plane_idx] * frame->height;
        // For chroma planes in some formats, height might be smaller
        if (plane_idx > 0 && frame->format != -1) {
            const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get((enum AVPixelFormat)frame->format);
            if (desc && desc->log2_chroma_h > 0) {
                plane_size = frame->linesize[plane_idx] * (frame->height >> desc->log2_chroma_h);
            }
        }
    } else {
        // Audio frame
        plane_size = frame->linesize[plane_idx];
    }
    
    napi_value buffer;
    void *data;
    status = napi_create_buffer_copy(env, plane_size, frame->data[plane_idx], &data, &buffer);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to create buffer");
        return NULL;
    }
    
    return buffer;
}

/**
 * Set frame data from Buffer
 * @param frameId - Frame ID
 * @param planeIndex - Plane index (0-7)
 * @param buffer - Node.js Buffer with data
 */
napi_value atomic_set_frame_data(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 3;
    napi_value argv[3];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 3) {
        napi_throw_error(env, NULL, "Expected frame ID, plane index, and buffer");
        return NULL;
    }
    
    int frame_id, plane_idx;
    napi_get_value_int32(env, argv[0], &frame_id);
    napi_get_value_int32(env, argv[1], &plane_idx);
    
    AVFrame *frame = get_context_ptr(frame_id, CTX_TYPE_FRAME);
    if (!frame) {
        napi_throw_error(env, NULL, "Invalid frame");
        return NULL;
    }
    
    if (plane_idx < 0 || plane_idx >= AV_NUM_DATA_POINTERS) {
        napi_throw_error(env, NULL, "Invalid plane index");
        return NULL;
    }
    
    bool is_buffer;
    status = napi_is_buffer(env, argv[2], &is_buffer);
    if (status != napi_ok || !is_buffer) {
        napi_throw_error(env, NULL, "Expected Buffer as third argument");
        return NULL;
    }
    
    void *buffer_data;
    size_t buffer_length;
    status = napi_get_buffer_info(env, argv[2], &buffer_data, &buffer_length);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to get buffer info");
        return NULL;
    }
    
    if (!frame->data[plane_idx]) {
        napi_throw_error(env, NULL, "Frame buffer not allocated. Call frameGetBuffer first.");
        return NULL;
    }
    
    // Calculate expected plane size
    size_t plane_size;
    if (frame->height > 0) {
        plane_size = frame->linesize[plane_idx] * frame->height;
        if (plane_idx > 0 && frame->format != -1) {
            const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get((enum AVPixelFormat)frame->format);
            if (desc && desc->log2_chroma_h > 0) {
                plane_size = frame->linesize[plane_idx] * (frame->height >> desc->log2_chroma_h);
            }
        }
    } else {
        plane_size = frame->linesize[plane_idx];
    }
    
    if (buffer_length > plane_size) {
        napi_throw_error(env, NULL, "Buffer size exceeds plane size");
        return NULL;
    }
    
    memcpy(frame->data[plane_idx], buffer_data, buffer_length);
    
    return NULL;
}

// ============================================================================
// 8. Packet Data Access and Manipulation
// ============================================================================

/**
 * Get packet data as Buffer
 * @param packetId - Packet ID
 * @returns Node.js Buffer containing packet data
 */
napi_value atomic_get_packet_data(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 1;
    napi_value argv[1];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Expected packet ID");
        return NULL;
    }
    
    int pkt_id;
    napi_get_value_int32(env, argv[0], &pkt_id);
    
    AVPacket *pkt = get_context_ptr(pkt_id, CTX_TYPE_PACKET);
    if (!pkt) {
        napi_throw_error(env, NULL, "Invalid packet");
        return NULL;
    }
    
    if (!pkt->data || pkt->size == 0) {
        return NULL; // Return null for empty packet
    }
    
    napi_value buffer;
    void *data;
    status = napi_create_buffer_copy(env, pkt->size, pkt->data, &data, &buffer);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to create buffer");
        return NULL;
    }
    
    return buffer;
}

/**
 * Set packet data from Buffer
 * @param packetId - Packet ID
 * @param buffer - Node.js Buffer with data
 */
napi_value atomic_set_packet_data(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 2;
    napi_value argv[2];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 2) {
        napi_throw_error(env, NULL, "Expected packet ID and buffer");
        return NULL;
    }
    
    int pkt_id;
    napi_get_value_int32(env, argv[0], &pkt_id);
    
    AVPacket *pkt = get_context_ptr(pkt_id, CTX_TYPE_PACKET);
    if (!pkt) {
        napi_throw_error(env, NULL, "Invalid packet");
        return NULL;
    }
    
    bool is_buffer;
    status = napi_is_buffer(env, argv[1], &is_buffer);
    if (status != napi_ok || !is_buffer) {
        napi_throw_error(env, NULL, "Expected Buffer as second argument");
        return NULL;
    }
    
    void *buffer_data;
    size_t buffer_length;
    status = napi_get_buffer_info(env, argv[1], &buffer_data, &buffer_length);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to get buffer info");
        return NULL;
    }
    
    // Free existing packet data
    av_packet_unref(pkt);
    
    // Allocate new buffer and copy data
    int ret = av_new_packet(pkt, buffer_length);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        napi_throw_error(env, NULL, errbuf);
        return NULL;
    }
    
    memcpy(pkt->data, buffer_data, buffer_length);
    
    return NULL;
}

/**
 * Get packet property
 * @param packetId - Packet ID
 * @param property - Property name (pts, dts, duration, streamIndex, flags, etc.)
 * @returns Property value
 */
napi_value atomic_get_packet_property(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 2;
    napi_value argv[2];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 2) {
        napi_throw_error(env, NULL, "Expected packet ID and property name");
        return NULL;
    }
    
    int pkt_id;
    napi_get_value_int32(env, argv[0], &pkt_id);
    
    char property[64];
    size_t str_len;
    napi_get_value_string_utf8(env, argv[1], property, sizeof(property), &str_len);
    
    AVPacket *pkt = get_context_ptr(pkt_id, CTX_TYPE_PACKET);
    if (!pkt) {
        napi_throw_error(env, NULL, "Invalid packet");
        return NULL;
    }
    
    napi_value result;
    
    if (strcmp(property, "pts") == 0) {
        napi_create_int64(env, pkt->pts, &result);
    } else if (strcmp(property, "dts") == 0) {
        napi_create_int64(env, pkt->dts, &result);
    } else if (strcmp(property, "duration") == 0) {
        napi_create_int64(env, pkt->duration, &result);
    } else if (strcmp(property, "streamIndex") == 0) {
        napi_create_int32(env, pkt->stream_index, &result);
    } else if (strcmp(property, "flags") == 0) {
        napi_create_int32(env, pkt->flags, &result);
    } else if (strcmp(property, "size") == 0) {
        napi_create_int32(env, pkt->size, &result);
    } else {
        napi_throw_error(env, NULL, "Unknown property");
        return NULL;
    }
    
    return result;
}

/**
 * Set packet property
 * @param packetId - Packet ID
 * @param property - Property name (pts, dts, duration, streamIndex, flags)
 * @param value - Property value
 */
napi_value atomic_set_packet_property(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 3;
    napi_value argv[3];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 3) {
        napi_throw_error(env, NULL, "Expected packet ID, property name, and value");
        return NULL;
    }
    
    int pkt_id;
    napi_get_value_int32(env, argv[0], &pkt_id);
    
    char property[64];
    size_t str_len;
    napi_get_value_string_utf8(env, argv[1], property, sizeof(property), &str_len);
    
    AVPacket *pkt = get_context_ptr(pkt_id, CTX_TYPE_PACKET);
    if (!pkt) {
        napi_throw_error(env, NULL, "Invalid packet");
        return NULL;
    }
    
    napi_valuetype valuetype;
    napi_typeof(env, argv[2], &valuetype);
    
    if (valuetype != napi_number) {
        napi_throw_error(env, NULL, "Expected number value");
        return NULL;
    }
    
    if (strcmp(property, "pts") == 0) {
        int64_t pts;
        napi_get_value_int64(env, argv[2], &pts);
        pkt->pts = pts;
    } else if (strcmp(property, "dts") == 0) {
        int64_t dts;
        napi_get_value_int64(env, argv[2], &dts);
        pkt->dts = dts;
    } else if (strcmp(property, "duration") == 0) {
        int64_t duration;
        napi_get_value_int64(env, argv[2], &duration);
        pkt->duration = duration;
    } else if (strcmp(property, "streamIndex") == 0) {
        int32_t stream_index;
        napi_get_value_int32(env, argv[2], &stream_index);
        pkt->stream_index = stream_index;
    } else if (strcmp(property, "flags") == 0) {
        int32_t flags;
        napi_get_value_int32(env, argv[2], &flags);
        pkt->flags = flags;
    } else {
        napi_throw_error(env, NULL, "Unknown or read-only property");
        return NULL;
    }
    
    return NULL;
}

// ============================================================================
// 9. Video Scaling (SwsContext)
// ============================================================================

/**
 * Create software scaler context
 * @param srcWidth - Source width
 * @param srcHeight - Source height
 * @param srcFormat - Source pixel format (string or enum value)
 * @param dstWidth - Destination width
 * @param dstHeight - Destination height
 * @param dstFormat - Destination pixel format (string or enum value)
 * @param flags - Scaling algorithm flags (optional, default: SWS_BILINEAR)
 * @returns swsContextId - Scaler context ID
 */
napi_value atomic_create_sws_context(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 7;
    napi_value argv[7];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 6) {
        napi_throw_error(env, NULL, "Expected srcWidth, srcHeight, srcFormat, dstWidth, dstHeight, dstFormat");
        return NULL;
    }
    
    int32_t src_width, src_height, dst_width, dst_height;
    napi_get_value_int32(env, argv[0], &src_width);
    napi_get_value_int32(env, argv[1], &src_height);
    napi_get_value_int32(env, argv[3], &dst_width);
    napi_get_value_int32(env, argv[4], &dst_height);
    
    // Parse source format
    enum AVPixelFormat src_fmt;
    napi_valuetype src_fmt_type;
    napi_typeof(env, argv[2], &src_fmt_type);
    if (src_fmt_type == napi_string) {
        char fmt_str[64];
        size_t str_len;
        napi_get_value_string_utf8(env, argv[2], fmt_str, sizeof(fmt_str), &str_len);
        src_fmt = av_get_pix_fmt(fmt_str);
        if (src_fmt == AV_PIX_FMT_NONE) {
            napi_throw_error(env, NULL, "Invalid source pixel format");
            return NULL;
        }
    } else {
        int32_t fmt_val;
        napi_get_value_int32(env, argv[2], &fmt_val);
        src_fmt = (enum AVPixelFormat)fmt_val;
    }
    
    // Parse destination format
    enum AVPixelFormat dst_fmt;
    napi_valuetype dst_fmt_type;
    napi_typeof(env, argv[5], &dst_fmt_type);
    if (dst_fmt_type == napi_string) {
        char fmt_str[64];
        size_t str_len;
        napi_get_value_string_utf8(env, argv[5], fmt_str, sizeof(fmt_str), &str_len);
        dst_fmt = av_get_pix_fmt(fmt_str);
        if (dst_fmt == AV_PIX_FMT_NONE) {
            napi_throw_error(env, NULL, "Invalid destination pixel format");
            return NULL;
        }
    } else {
        int32_t fmt_val;
        napi_get_value_int32(env, argv[5], &fmt_val);
        dst_fmt = (enum AVPixelFormat)fmt_val;
    }
    
    // Get scaling flags (optional)
    int32_t flags = SWS_BILINEAR;
    if (argc >= 7) {
        napi_get_value_int32(env, argv[6], &flags);
    }
    
    // Create scaler context
    struct SwsContext *sws_ctx = sws_getContext(
        src_width, src_height, src_fmt,
        dst_width, dst_height, dst_fmt,
        flags, NULL, NULL, NULL
    );
    
    if (!sws_ctx) {
        napi_throw_error(env, NULL, "Failed to create scaler context");
        return NULL;
    }
    
    // Allocate context ID
    int ctx_id = alloc_context_id(CTX_TYPE_SWS, sws_ctx);
    if (ctx_id < 0) {
        sws_freeContext(sws_ctx);
        napi_throw_error(env, NULL, "Too many open contexts");
        return NULL;
    }
    
    napi_value result;
    napi_create_int32(env, ctx_id, &result);
    return result;
}

/**
 * Scale frame using sws context
 * @param swsContextId - Scaler context ID
 * @param srcFrameId - Source frame ID
 * @param dstFrameId - Destination frame ID
 */
napi_value atomic_sws_scale(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 3;
    napi_value argv[3];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 3) {
        napi_throw_error(env, NULL, "Expected sws context ID, src frame ID, dst frame ID");
        return NULL;
    }
    
    int sws_ctx_id, src_frame_id, dst_frame_id;
    napi_get_value_int32(env, argv[0], &sws_ctx_id);
    napi_get_value_int32(env, argv[1], &src_frame_id);
    napi_get_value_int32(env, argv[2], &dst_frame_id);
    
    struct SwsContext *sws_ctx = get_context_ptr(sws_ctx_id, CTX_TYPE_SWS);
    AVFrame *src_frame = get_context_ptr(src_frame_id, CTX_TYPE_FRAME);
    AVFrame *dst_frame = get_context_ptr(dst_frame_id, CTX_TYPE_FRAME);
    
    if (!sws_ctx || !src_frame || !dst_frame) {
        napi_throw_error(env, NULL, "Invalid context or frame");
        return NULL;
    }
    
    // Perform scaling
    int ret = sws_scale(
        sws_ctx,
        (const uint8_t * const *)src_frame->data, src_frame->linesize,
        0, src_frame->height,
        dst_frame->data, dst_frame->linesize
    );
    
    if (ret < 0) {
        napi_throw_error(env, NULL, "Scaling failed");
        return NULL;
    }
    
    // Copy frame properties
    dst_frame->pts = src_frame->pts;
    dst_frame->pkt_dts = src_frame->pkt_dts;
    dst_frame->pict_type = src_frame->pict_type;
    dst_frame->key_frame = src_frame->key_frame;
    
    return NULL;
}

// ============================================================================
// 10. Audio Resampling (SwrContext)
// ============================================================================

/**
 * Create software resample context
 * @param srcSampleRate - Source sample rate
 * @param srcChannels - Source channel count
 * @param srcFormat - Source sample format (string or enum value)
 * @param dstSampleRate - Destination sample rate
 * @param dstChannels - Destination channel count
 * @param dstFormat - Destination sample format (string or enum value)
 * @returns swrContextId - Resample context ID
 */
napi_value atomic_create_swr_context(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 6;
    napi_value argv[6];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 6) {
        napi_throw_error(env, NULL, "Expected srcSampleRate, srcChannels, srcFormat, dstSampleRate, dstChannels, dstFormat");
        return NULL;
    }
    
    int32_t src_sample_rate, src_channels, dst_sample_rate, dst_channels;
    napi_get_value_int32(env, argv[0], &src_sample_rate);
    napi_get_value_int32(env, argv[1], &src_channels);
    napi_get_value_int32(env, argv[3], &dst_sample_rate);
    napi_get_value_int32(env, argv[4], &dst_channels);
    
    // Parse source format
    enum AVSampleFormat src_fmt;
    napi_valuetype src_fmt_type;
    napi_typeof(env, argv[2], &src_fmt_type);
    if (src_fmt_type == napi_string) {
        char fmt_str[64];
        size_t str_len;
        napi_get_value_string_utf8(env, argv[2], fmt_str, sizeof(fmt_str), &str_len);
        src_fmt = av_get_sample_fmt(fmt_str);
        if (src_fmt == AV_SAMPLE_FMT_NONE) {
            napi_throw_error(env, NULL, "Invalid source sample format");
            return NULL;
        }
    } else {
        int32_t fmt_val;
        napi_get_value_int32(env, argv[2], &fmt_val);
        src_fmt = (enum AVSampleFormat)fmt_val;
    }
    
    // Parse destination format
    enum AVSampleFormat dst_fmt;
    napi_valuetype dst_fmt_type;
    napi_typeof(env, argv[5], &dst_fmt_type);
    if (dst_fmt_type == napi_string) {
        char fmt_str[64];
        size_t str_len;
        napi_get_value_string_utf8(env, argv[5], fmt_str, sizeof(fmt_str), &str_len);
        dst_fmt = av_get_sample_fmt(fmt_str);
        if (dst_fmt == AV_SAMPLE_FMT_NONE) {
            napi_throw_error(env, NULL, "Invalid destination sample format");
            return NULL;
        }
    } else {
        int32_t fmt_val;
        napi_get_value_int32(env, argv[5], &fmt_val);
        dst_fmt = (enum AVSampleFormat)fmt_val;
    }
    
    // Create channel layouts
    AVChannelLayout src_ch_layout, dst_ch_layout;
    av_channel_layout_default(&src_ch_layout, src_channels);
    av_channel_layout_default(&dst_ch_layout, dst_channels);
    
    // Create resample context
    struct SwrContext *swr_ctx = NULL;
    int ret = swr_alloc_set_opts2(&swr_ctx,
        &dst_ch_layout, dst_fmt, dst_sample_rate,
        &src_ch_layout, src_fmt, src_sample_rate,
        0, NULL);
    
    av_channel_layout_uninit(&src_ch_layout);
    av_channel_layout_uninit(&dst_ch_layout);
    
    if (ret < 0 || !swr_ctx) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        napi_throw_error(env, NULL, errbuf);
        return NULL;
    }
    
    // Initialize resample context
    ret = swr_init(swr_ctx);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        swr_free(&swr_ctx);
        napi_throw_error(env, NULL, errbuf);
        return NULL;
    }
    
    // Allocate context ID
    int ctx_id = alloc_context_id(CTX_TYPE_SWR, swr_ctx);
    if (ctx_id < 0) {
        swr_free(&swr_ctx);
        napi_throw_error(env, NULL, "Too many open contexts");
        return NULL;
    }
    
    napi_value result;
    napi_create_int32(env, ctx_id, &result);
    return result;
}

/**
 * Resample audio frame using swr context
 * @param swrContextId - Resample context ID
 * @param srcFrameId - Source frame ID (or null for flushing)
 * @param dstFrameId - Destination frame ID
 * @returns Number of samples output per channel
 */
napi_value atomic_swr_convert_frame(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 3;
    napi_value argv[3];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 3) {
        napi_throw_error(env, NULL, "Expected swr context ID, src frame ID (or null), dst frame ID");
        return NULL;
    }
    
    int swr_ctx_id, dst_frame_id;
    napi_get_value_int32(env, argv[0], &swr_ctx_id);
    napi_get_value_int32(env, argv[2], &dst_frame_id);
    
    struct SwrContext *swr_ctx = get_context_ptr(swr_ctx_id, CTX_TYPE_SWR);
    AVFrame *dst_frame = get_context_ptr(dst_frame_id, CTX_TYPE_FRAME);
    
    if (!swr_ctx || !dst_frame) {
        napi_throw_error(env, NULL, "Invalid context or frame");
        return NULL;
    }
    
    // Check if src frame is provided or if flushing
    AVFrame *src_frame = NULL;
    napi_valuetype src_type;
    napi_typeof(env, argv[1], &src_type);
    if (src_type != napi_null && src_type != napi_undefined) {
        int src_frame_id;
        napi_get_value_int32(env, argv[1], &src_frame_id);
        src_frame = get_context_ptr(src_frame_id, CTX_TYPE_FRAME);
    }
    
    // Perform resampling
    int ret = swr_convert_frame(swr_ctx, dst_frame, src_frame);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        napi_throw_error(env, NULL, errbuf);
        return NULL;
    }
    
    // Copy frame properties if source is provided
    if (src_frame) {
        dst_frame->pts = src_frame->pts;
    }
    
    napi_value result;
    napi_create_int32(env, dst_frame->nb_samples, &result);
    return result;
}

// ============================================================================
// 11. Auxiliary Functions - Seek, Metadata, Format Query
// ============================================================================

/**
 * Seek to timestamp in input file
 * @param inputContextId - Input context ID
 * @param timestamp - Target timestamp
 * @param streamIndex - Stream index (-1 for default)
 * @param flags - Seek flags (optional, default: AVSEEK_FLAG_BACKWARD)
 */
napi_value atomic_seek_input(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 4;
    napi_value argv[4];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 2) {
        napi_throw_error(env, NULL, "Expected input context ID and timestamp");
        return NULL;
    }
    
    int ctx_id;
    int64_t timestamp;
    napi_get_value_int32(env, argv[0], &ctx_id);
    napi_get_value_int64(env, argv[1], &timestamp);
    
    AVFormatContext *fmt_ctx = get_context_ptr(ctx_id, CTX_TYPE_INPUT_FORMAT);
    if (!fmt_ctx) {
        napi_throw_error(env, NULL, "Invalid input context");
        return NULL;
    }
    
    int stream_idx = -1;
    if (argc >= 3) {
        napi_get_value_int32(env, argv[2], &stream_idx);
    }
    
    int flags = AVSEEK_FLAG_BACKWARD;
    if (argc >= 4) {
        napi_get_value_int32(env, argv[3], &flags);
    }
    
    int ret = av_seek_frame(fmt_ctx, stream_idx, timestamp, flags);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        napi_throw_error(env, NULL, errbuf);
        return NULL;
    }
    
    return NULL;
}

/**
 * Get metadata from input context
 * @param inputContextId - Input context ID
 * @param key - Metadata key (optional, returns all if not provided)
 * @returns Metadata object or string value
 */
napi_value atomic_get_metadata(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 2;
    napi_value argv[2];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Expected input context ID");
        return NULL;
    }
    
    int ctx_id;
    napi_get_value_int32(env, argv[0], &ctx_id);
    
    AVFormatContext *fmt_ctx = get_context_ptr(ctx_id, CTX_TYPE_INPUT_FORMAT);
    if (!fmt_ctx) {
        napi_throw_error(env, NULL, "Invalid input context");
        return NULL;
    }
    
    if (argc >= 2) {
        // Get specific key
        char key[256];
        size_t key_len;
        napi_get_value_string_utf8(env, argv[1], key, sizeof(key), &key_len);
        
        AVDictionaryEntry *tag = av_dict_get(fmt_ctx->metadata, key, NULL, 0);
        if (tag) {
            napi_value result;
            napi_create_string_utf8(env, tag->value, NAPI_AUTO_LENGTH, &result);
            return result;
        } else {
            return NULL; // Return null if key not found
        }
    } else {
        // Return all metadata
        napi_value result_obj;
        napi_create_object(env, &result_obj);
        
        AVDictionaryEntry *tag = NULL;
        while ((tag = av_dict_get(fmt_ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
            napi_value value;
            napi_create_string_utf8(env, tag->value, NAPI_AUTO_LENGTH, &value);
            napi_set_named_property(env, result_obj, tag->key, value);
        }
        
        return result_obj;
    }
}

/**
 * Set metadata on output context
 * @param outputContextId - Output context ID
 * @param key - Metadata key
 * @param value - Metadata value
 */
napi_value atomic_set_metadata(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 3;
    napi_value argv[3];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 3) {
        napi_throw_error(env, NULL, "Expected output context ID, key, and value");
        return NULL;
    }
    
    int ctx_id;
    napi_get_value_int32(env, argv[0], &ctx_id);
    
    char key[256], value[1024];
    size_t key_len, value_len;
    napi_get_value_string_utf8(env, argv[1], key, sizeof(key), &key_len);
    napi_get_value_string_utf8(env, argv[2], value, sizeof(value), &value_len);
    
    AVFormatContext *fmt_ctx = get_context_ptr(ctx_id, CTX_TYPE_OUTPUT_FORMAT);
    if (!fmt_ctx) {
        napi_throw_error(env, NULL, "Invalid output context");
        return NULL;
    }
    
    int ret = av_dict_set(&fmt_ctx->metadata, key, value, 0);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        napi_throw_error(env, NULL, errbuf);
        return NULL;
    }
    
    return NULL;
}

/**
 * Copy metadata from input to output
 * @param inputContextId - Input context ID
 * @param outputContextId - Output context ID
 */
napi_value atomic_copy_metadata(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 2;
    napi_value argv[2];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 2) {
        napi_throw_error(env, NULL, "Expected input and output context IDs");
        return NULL;
    }
    
    int input_ctx_id, output_ctx_id;
    napi_get_value_int32(env, argv[0], &input_ctx_id);
    napi_get_value_int32(env, argv[1], &output_ctx_id);
    
    AVFormatContext *input_fmt_ctx = get_context_ptr(input_ctx_id, CTX_TYPE_INPUT_FORMAT);
    AVFormatContext *output_fmt_ctx = get_context_ptr(output_ctx_id, CTX_TYPE_OUTPUT_FORMAT);
    
    if (!input_fmt_ctx || !output_fmt_ctx) {
        napi_throw_error(env, NULL, "Invalid context");
        return NULL;
    }
    
    int ret = av_dict_copy(&output_fmt_ctx->metadata, input_fmt_ctx->metadata, 0);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        napi_throw_error(env, NULL, errbuf);
        return NULL;
    }
    
    return NULL;
}

/**
 * Get supported pixel formats for encoder
 * @param codecContextId - Encoder context ID
 * @returns Array of pixel format names
 */
napi_value atomic_get_supported_pix_fmts(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 1;
    napi_value argv[1];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Expected encoder context ID");
        return NULL;
    }
    
    int ctx_id;
    napi_get_value_int32(env, argv[0], &ctx_id);
    
    AVCodecContext *codec_ctx = get_context_ptr(ctx_id, CTX_TYPE_ENCODER);
    if (!codec_ctx || !codec_ctx->codec) {
        napi_throw_error(env, NULL, "Invalid encoder context");
        return NULL;
    }
    
    napi_value result_array;
    napi_create_array(env, &result_array);
    
    const enum AVPixelFormat *pix_fmts = codec_ctx->codec->pix_fmts;
    if (pix_fmts) {
        int index = 0;
        while (pix_fmts[index] != AV_PIX_FMT_NONE) {
            const char *fmt_name = av_get_pix_fmt_name(pix_fmts[index]);
            if (fmt_name) {
                napi_value fmt_val;
                napi_create_string_utf8(env, fmt_name, NAPI_AUTO_LENGTH, &fmt_val);
                napi_set_element(env, result_array, index, fmt_val);
            }
            index++;
        }
    }
    
    return result_array;
}

/**
 * Get supported sample formats for encoder
 * @param codecContextId - Encoder context ID
 * @returns Array of sample format names
 */
napi_value atomic_get_supported_sample_fmts(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 1;
    napi_value argv[1];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Expected encoder context ID");
        return NULL;
    }
    
    int ctx_id;
    napi_get_value_int32(env, argv[0], &ctx_id);
    
    AVCodecContext *codec_ctx = get_context_ptr(ctx_id, CTX_TYPE_ENCODER);
    if (!codec_ctx || !codec_ctx->codec) {
        napi_throw_error(env, NULL, "Invalid encoder context");
        return NULL;
    }
    
    napi_value result_array;
    napi_create_array(env, &result_array);
    
    const enum AVSampleFormat *sample_fmts = codec_ctx->codec->sample_fmts;
    if (sample_fmts) {
        int index = 0;
        while (sample_fmts[index] != AV_SAMPLE_FMT_NONE) {
            const char *fmt_name = av_get_sample_fmt_name(sample_fmts[index]);
            if (fmt_name) {
                napi_value fmt_val;
                napi_create_string_utf8(env, fmt_name, NAPI_AUTO_LENGTH, &fmt_val);
                napi_set_element(env, result_array, index, fmt_val);
            }
            index++;
        }
    }
    
    return result_array;
}

/**
 * Get supported sample rates for encoder
 * @param codecContextId - Encoder context ID
 * @returns Array of supported sample rates
 */
napi_value atomic_get_supported_sample_rates(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 1;
    napi_value argv[1];
    
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Expected encoder context ID");
        return NULL;
    }
    
    int ctx_id;
    napi_get_value_int32(env, argv[0], &ctx_id);
    
    AVCodecContext *codec_ctx = get_context_ptr(ctx_id, CTX_TYPE_ENCODER);
    if (!codec_ctx || !codec_ctx->codec) {
        napi_throw_error(env, NULL, "Invalid encoder context");
        return NULL;
    }
    
    napi_value result_array;
    napi_create_array(env, &result_array);
    
    const int *sample_rates = codec_ctx->codec->supported_samplerates;
    if (sample_rates) {
        int index = 0;
        while (sample_rates[index] != 0) {
            napi_value rate_val;
            napi_create_int32(env, sample_rates[index], &rate_val);
            napi_set_element(env, result_array, index, rate_val);
            index++;
        }
    }
    
    return result_array;
}

