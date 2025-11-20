/*
 * utils.c - Video utility functions
 * Provides functions to get video duration, size and format information
 */

#include <node_api.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/dict.h>
#include <libavutil/rational.h>
#include <libavutil/channel_layout.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#include <pthread.h>
#endif

/**
 * Get video duration
 * Args: [file path]
 * Returns: Duration (seconds, double)
 */
napi_value get_video_duration(napi_env env, napi_callback_info info)
{
    napi_status status;
    size_t argc = 1;
    napi_value argv[1];
    napi_value result;
    char filepath[1024];
    size_t filepath_len;
    AVFormatContext *fmt_ctx = NULL;
    int ret;
    double duration = 0.0;
    
    // Get arguments
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to get callback info");
        return NULL;
    }
    
    if (argc < 1) {
        napi_throw_type_error(env, NULL, "Expected file path as argument");
        return NULL;
    }
    
    // Get file path string
    status = napi_get_value_string_utf8(env, argv[0], filepath, sizeof(filepath), &filepath_len);
    if (status != napi_ok) {
        napi_throw_type_error(env, NULL, "Invalid file path");
        return NULL;
    }
    
    // Initialize FFmpeg
    av_log_set_level(AV_LOG_QUIET);
    
    // Open input file
    ret = avformat_open_input(&fmt_ctx, filepath, NULL, NULL);
    if (ret < 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Could not open file: %s", filepath);
        napi_throw_error(env, NULL, error_msg);
        return NULL;
    }
    
    // Find stream information
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        avformat_close_input(&fmt_ctx);
        napi_throw_error(env, NULL, "Could not find stream information");
        return NULL;
    }
    
    // Get duration (seconds)
    if (fmt_ctx->duration != AV_NOPTS_VALUE) {
        duration = (double)fmt_ctx->duration / AV_TIME_BASE;
    }
    
    // Clean up resources
    avformat_close_input(&fmt_ctx);
    
    // Return result
    status = napi_create_double(env, duration, &result);
    if (status != napi_ok) {
        return NULL;
    }
    
    return result;
}

/**
 * Get video format information (metadata)
 * Args: [file path]
 * Returns: Object containing format information
 */
napi_value get_video_format_info(napi_env env, napi_callback_info info)
{
    napi_status status;
    size_t argc = 1;
    napi_value argv[1];
    napi_value result;
    napi_value format_name, duration, bitrate, video_codec, audio_codec;
    napi_value width, height, fps, metadata_obj;
    char filepath[1024];
    size_t filepath_len;
    AVFormatContext *fmt_ctx = NULL;
    AVCodecParameters *video_codecpar = NULL;
    AVCodecParameters *audio_codecpar = NULL;
    int ret;
    int video_stream_idx = -1;
    int audio_stream_idx = -1;
    
    // Get arguments
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to get callback info");
        return NULL;
    }
    
    if (argc < 1) {
        napi_throw_type_error(env, NULL, "Expected file path as argument");
        return NULL;
    }
    
    // Get file path string
    status = napi_get_value_string_utf8(env, argv[0], filepath, sizeof(filepath), &filepath_len);
    if (status != napi_ok) {
        napi_throw_type_error(env, NULL, "Invalid file path");
        return NULL;
    }
    
    // Initialize FFmpeg
    av_log_set_level(AV_LOG_QUIET);
    
    // Open input file
    ret = avformat_open_input(&fmt_ctx, filepath, NULL, NULL);
    if (ret < 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Could not open file: %s", filepath);
        napi_throw_error(env, NULL, error_msg);
        return NULL;
    }
    
    // Find stream information
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        avformat_close_input(&fmt_ctx);
        napi_throw_error(env, NULL, "Could not find stream information");
        return NULL;
    }
    
    // Find video and audio streams
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video_stream_idx < 0) {
            video_stream_idx = i;
        } else if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audio_stream_idx < 0) {
            audio_stream_idx = i;
        }
    }
    
    // Create result object
    status = napi_create_object(env, &result);
    if (status != napi_ok) {
        avformat_close_input(&fmt_ctx);
        return NULL;
    }
    
    // Set format name
    if (fmt_ctx->iformat && fmt_ctx->iformat->name) {
        status = napi_create_string_utf8(env, fmt_ctx->iformat->name, NAPI_AUTO_LENGTH, &format_name);
        if (status == napi_ok) {
            napi_set_named_property(env, result, "format", format_name);
        }
    }
    
    // Set duration
    if (fmt_ctx->duration != AV_NOPTS_VALUE) {
        double duration_sec = (double)fmt_ctx->duration / AV_TIME_BASE;
        status = napi_create_double(env, duration_sec, &duration);
        if (status == napi_ok) {
            napi_set_named_property(env, result, "duration", duration);
        }
    }
    
    // Set bit rate
    if (fmt_ctx->bit_rate > 0) {
        status = napi_create_int64(env, fmt_ctx->bit_rate, &bitrate);
        if (status == napi_ok) {
            napi_set_named_property(env, result, "bitrate", bitrate);
        }
    }
    
    // Set video codec
    if (video_stream_idx >= 0) {
        video_codecpar = fmt_ctx->streams[video_stream_idx]->codecpar;
        const AVCodec *codec = avcodec_find_decoder(video_codecpar->codec_id);
        if (codec && codec->name) {
            status = napi_create_string_utf8(env, codec->name, NAPI_AUTO_LENGTH, &video_codec);
            if (status == napi_ok) {
                napi_set_named_property(env, result, "videoCodec", video_codec);
            }
        }
        
        // Set video width and height
        if (video_codecpar->width > 0) {
            status = napi_create_int32(env, video_codecpar->width, &width);
            if (status == napi_ok) {
                napi_set_named_property(env, result, "width", width);
            }
        }
        
        if (video_codecpar->height > 0) {
            status = napi_create_int32(env, video_codecpar->height, &height);
            if (status == napi_ok) {
                napi_set_named_property(env, result, "height", height);
            }
        }
        
        // Set frame rate
        AVRational fps_rational = fmt_ctx->streams[video_stream_idx]->r_frame_rate;
        if (fps_rational.num > 0 && fps_rational.den > 0) {
            double fps_value = av_q2d(fps_rational);
            status = napi_create_double(env, fps_value, &fps);
            if (status == napi_ok) {
                napi_set_named_property(env, result, "fps", fps);
            }
        }
    }
    
    // Set audio codec
    if (audio_stream_idx >= 0) {
        audio_codecpar = fmt_ctx->streams[audio_stream_idx]->codecpar;
        const AVCodec *codec = avcodec_find_decoder(audio_codecpar->codec_id);
        if (codec && codec->name) {
            status = napi_create_string_utf8(env, codec->name, NAPI_AUTO_LENGTH, &audio_codec);
            if (status == napi_ok) {
                napi_set_named_property(env, result, "audioCodec", audio_codec);
            }
        }
        
        // Set audio sample rate
        if (audio_codecpar->sample_rate > 0) {
            napi_value sample_rate;
            status = napi_create_int32(env, audio_codecpar->sample_rate, &sample_rate);
            if (status == napi_ok) {
                napi_set_named_property(env, result, "sampleRate", sample_rate);
            }
        }
        
        // Set audio channel count
        {
            int nb_channels = 0;
            // Try to use new ch_layout API (FFmpeg 5.0+)
            if (audio_codecpar->ch_layout.nb_channels > 0) {
                nb_channels = audio_codecpar->ch_layout.nb_channels;
            }
            // If new API is not available, try to use old channels field
            #if LIBAVCODEC_VERSION_MAJOR < 59
            if (nb_channels == 0 && audio_codecpar->channels > 0) {
                nb_channels = audio_codecpar->channels;
            }
            #endif
            
            if (nb_channels > 0) {
                napi_value channels;
                status = napi_create_int32(env, nb_channels, &channels);
                if (status == napi_ok) {
                    napi_set_named_property(env, result, "channels", channels);
                }
            }
        }
    }
    
    // Set metadata
    if (fmt_ctx->metadata) {
        status = napi_create_object(env, &metadata_obj);
        if (status == napi_ok) {
            AVDictionaryEntry *tag = NULL;
            while ((tag = av_dict_get(fmt_ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
                napi_value key, value;
                status = napi_create_string_utf8(env, tag->key, NAPI_AUTO_LENGTH, &key);
                if (status == napi_ok) {
                    status = napi_create_string_utf8(env, tag->value, NAPI_AUTO_LENGTH, &value);
                    if (status == napi_ok) {
                        napi_set_property(env, metadata_obj, key, value);
                    }
                }
            }
            napi_set_named_property(env, result, "metadata", metadata_obj);
        }
    }
    
    // Clean up resources
    avformat_close_input(&fmt_ctx);
    
    return result;
}

// Log listener related variables
static napi_env log_callback_env = NULL;
static napi_ref log_callback_ref = NULL;
#ifdef _WIN32
static DWORD main_thread_id = 0;
#else
static pthread_t main_thread_id = 0;
#endif

// FFmpeg log callback function (hybrid mode: synchronous on main thread, silent failure on other threads)
static void custom_log_callback(void* ptr, int level, const char* fmt, va_list vl) {
    // If no listener is set, use default log handling
    if (log_callback_ref == NULL || log_callback_env == NULL) {
        av_log_default_callback(ptr, level, fmt, vl);
        return;
    }
    
    // Check if on main thread (JavaScript thread)
#ifdef _WIN32
    DWORD current_thread = GetCurrentThreadId();
    if (current_thread != main_thread_id) {
        // Not on main thread, fail silently to avoid V8 crash from cross-thread calls
        return;
    }
#else
    pthread_t current_thread = pthread_self();
    if (!pthread_equal(current_thread, main_thread_id)) {
        // Not on main thread, fail silently to avoid V8 crash from cross-thread calls
        return;
    }
#endif
    
    // Format log message
    char message[4096];
    vsnprintf(message, sizeof(message), fmt, vl);
    
    // Create handle scope to manage lifecycle of N-API objects
    napi_handle_scope scope;
    napi_status status = napi_open_handle_scope(log_callback_env, &scope);
    if (status != napi_ok) {
        // If unable to create handle scope, fail silently
        return;
    }
    
    // Synchronously call JavaScript callback
    napi_value callback, global, level_val, message_val;
    napi_value argv[2];
    napi_value result;
    bool call_success = true;
    
    // Get callback function
    status = napi_get_reference_value(log_callback_env, log_callback_ref, &callback);
    if (status != napi_ok) {
        call_success = false;
        goto cleanup;
    }
    
    // Get global object
    status = napi_get_global(log_callback_env, &global);
    if (status != napi_ok) {
        call_success = false;
        goto cleanup;
    }
    
    // Create arguments
    status = napi_create_int32(log_callback_env, level, &level_val);
    if (status != napi_ok) {
        call_success = false;
        goto cleanup;
    }
    
    status = napi_create_string_utf8(log_callback_env, message, NAPI_AUTO_LENGTH, &message_val);
    if (status != napi_ok) {
        call_success = false;
        goto cleanup;
    }
    
    argv[0] = level_val;
    argv[1] = message_val;
    
    // Synchronously execute callback function (real-time output on main thread)
    status = napi_call_function(log_callback_env, global, callback, 2, argv, &result);
    if (status != napi_ok) {
        call_success = false;
    }
    
cleanup:
    // Close handle scope
    napi_close_handle_scope(log_callback_env, scope);
    
    // If call fails, fail silently without affecting FFmpeg execution
    (void)call_success;
}

/**
 * Add log listener
 * Args: [callback function]
 * Returns: undefined
 */
napi_value add_log_listener(napi_env env, napi_callback_info info) {
    napi_status status;
    size_t argc = 1;
    napi_value argv[1];
    napi_value callback;
    napi_valuetype valuetype;
    
    // Get arguments
    status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to get callback info");
        return NULL;
    }
    
    if (argc < 1) {
        napi_throw_type_error(env, NULL, "Expected a callback function as argument");
        return NULL;
    }
    
    // Check if argument is a function
    status = napi_typeof(env, argv[0], &valuetype);
    if (status != napi_ok || valuetype != napi_function) {
        napi_throw_type_error(env, NULL, "Expected a callback function");
        return NULL;
    }
    
    callback = argv[0];
    
    // If listener already exists, clean it up first
    if (log_callback_ref != NULL) {
        napi_delete_reference(env, log_callback_ref);
        log_callback_ref = NULL;
    }
    
    // Save main thread ID (used for thread checking in log callback)
#ifdef _WIN32
    main_thread_id = GetCurrentThreadId();
#else
    main_thread_id = pthread_self();
#endif
    
    // Save environment and callback reference
    log_callback_env = env;
    status = napi_create_reference(env, callback, 1, &log_callback_ref);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to create callback reference");
        return NULL;
    }
    
    // Set FFmpeg log callback (synchronous call)
    av_log_set_callback(custom_log_callback);
    
    // Return undefined
    napi_value result;
    napi_get_undefined(env, &result);
    return result;
}

/**
 * Clear log listener
 * Args: none
 * Returns: undefined
 */
napi_value clear_log_listener(napi_env env, napi_callback_info info) {
    // Clean up callback reference
    if (log_callback_ref != NULL) {
        napi_delete_reference(env, log_callback_ref);
        log_callback_ref = NULL;
    }
    
    log_callback_env = NULL;
    main_thread_id = 0;
    
    // Restore default log callback
    av_log_set_callback(av_log_default_callback);
    
    // Return undefined
    napi_value result;
    napi_get_undefined(env, &result);
    return result;
}

