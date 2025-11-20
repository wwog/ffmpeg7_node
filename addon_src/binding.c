#include <node_api.h>

// Declare napi functions from ffmpeg.c
extern napi_value ffmpeg_run(napi_env env, napi_callback_info info);

// Declare napi functions from utils.c
extern napi_value get_video_duration(napi_env env, napi_callback_info info);
extern napi_value get_video_format_info(napi_env env, napi_callback_info info);
extern napi_value add_log_listener(napi_env env, napi_callback_info info);
extern napi_value clear_log_listener(napi_env env, napi_callback_info info);

// Declare mid-level API functions from atomic_api.c
extern napi_value atomic_open_input(napi_env env, napi_callback_info info);
extern napi_value atomic_create_output(napi_env env, napi_callback_info info);
extern napi_value atomic_get_input_streams(napi_env env, napi_callback_info info);
extern napi_value atomic_add_output_stream(napi_env env, napi_callback_info info);
extern napi_value atomic_close_context(napi_env env, napi_callback_info info);
extern napi_value atomic_create_encoder(napi_env env, napi_callback_info info);
extern napi_value atomic_set_encoder_option(napi_env env, napi_callback_info info);
extern napi_value atomic_open_encoder(napi_env env, napi_callback_info info);
extern napi_value atomic_create_decoder(napi_env env, napi_callback_info info);
extern napi_value atomic_copy_decoder_params(napi_env env, napi_callback_info info);
extern napi_value atomic_open_decoder(napi_env env, napi_callback_info info);
extern napi_value atomic_get_encoder_list(napi_env env, napi_callback_info info);
extern napi_value atomic_get_muxer_list(napi_env env, napi_callback_info info);
extern napi_value atomic_set_output_option(napi_env env, napi_callback_info info);
extern napi_value atomic_write_header(napi_env env, napi_callback_info info);
extern napi_value atomic_write_trailer(napi_env env, napi_callback_info info);
extern napi_value atomic_copy_stream_params(napi_env env, napi_callback_info info);
extern napi_value atomic_copy_encoder_to_stream(napi_env env, napi_callback_info info);
extern napi_value atomic_read_packet(napi_env env, napi_callback_info info);
extern napi_value atomic_write_packet(napi_env env, napi_callback_info info);
extern napi_value atomic_free_packet(napi_env env, napi_callback_info info);
extern napi_value atomic_alloc_frame(napi_env env, napi_callback_info info);
extern napi_value atomic_free_frame(napi_env env, napi_callback_info info);
extern napi_value atomic_alloc_packet(napi_env env, napi_callback_info info);
extern napi_value atomic_send_packet(napi_env env, napi_callback_info info);
extern napi_value atomic_receive_frame(napi_env env, napi_callback_info info);
extern napi_value atomic_send_frame(napi_env env, napi_callback_info info);
extern napi_value atomic_receive_packet(napi_env env, napi_callback_info info);

// Frame data access and manipulation
extern napi_value atomic_frame_get_buffer(napi_env env, napi_callback_info info);
extern napi_value atomic_set_frame_property(napi_env env, napi_callback_info info);
extern napi_value atomic_get_frame_property(napi_env env, napi_callback_info info);
extern napi_value atomic_get_frame_data(napi_env env, napi_callback_info info);
extern napi_value atomic_set_frame_data(napi_env env, napi_callback_info info);

// Packet data access and manipulation
extern napi_value atomic_get_packet_data(napi_env env, napi_callback_info info);
extern napi_value atomic_set_packet_data(napi_env env, napi_callback_info info);
extern napi_value atomic_get_packet_property(napi_env env, napi_callback_info info);
extern napi_value atomic_set_packet_property(napi_env env, napi_callback_info info);

// Video scaling (SwsContext)
extern napi_value atomic_create_sws_context(napi_env env, napi_callback_info info);
extern napi_value atomic_sws_scale(napi_env env, napi_callback_info info);

// Audio resampling (SwrContext)
extern napi_value atomic_create_swr_context(napi_env env, napi_callback_info info);
extern napi_value atomic_swr_convert_frame(napi_env env, napi_callback_info info);

// AudioFIFO functions from audio_fifo.c
extern napi_value audio_fifo_alloc(napi_env env, napi_callback_info info);
extern napi_value audio_fifo_free(napi_env env, napi_callback_info info);
extern napi_value audio_fifo_write(napi_env env, napi_callback_info info);
extern napi_value audio_fifo_read(napi_env env, napi_callback_info info);
extern napi_value audio_fifo_size(napi_env env, napi_callback_info info);
extern napi_value audio_fifo_space(napi_env env, napi_callback_info info);
extern napi_value audio_fifo_reset(napi_env env, napi_callback_info info);
extern napi_value audio_fifo_drain(napi_env env, napi_callback_info info);

// Auxiliary functions
extern napi_value atomic_seek_input(napi_env env, napi_callback_info info);
extern napi_value atomic_get_metadata(napi_env env, napi_callback_info info);
extern napi_value atomic_set_metadata(napi_env env, napi_callback_info info);
extern napi_value atomic_copy_metadata(napi_env env, napi_callback_info info);
extern napi_value atomic_get_supported_pix_fmts(napi_env env, napi_callback_info info);
extern napi_value atomic_get_supported_sample_fmts(napi_env env, napi_callback_info info);
extern napi_value atomic_get_supported_sample_rates(napi_env env, napi_callback_info info);

napi_value Init(napi_env env, napi_value exports)
{
    napi_status status;
    napi_value fn;
    
    // Create run function
    status = napi_create_function(env, NULL, 0, ffmpeg_run, NULL, &fn);
    if (status != napi_ok) {
        return NULL;
    }
    status = napi_set_named_property(env, exports, "run", fn);
    if (status != napi_ok) {
        return NULL;
    }
    
    // Create getVideoDuration function
    status = napi_create_function(env, NULL, 0, get_video_duration, NULL, &fn);
    if (status != napi_ok) {
        return NULL;
    }
    status = napi_set_named_property(env, exports, "getVideoDuration", fn);
    if (status != napi_ok) {
        return NULL;
    }
    
    // Create getVideoFormatInfo function
    status = napi_create_function(env, NULL, 0, get_video_format_info, NULL, &fn);
    if (status != napi_ok) {
        return NULL;
    }
    status = napi_set_named_property(env, exports, "getVideoFormatInfo", fn);
    if (status != napi_ok) {
        return NULL;
    }
    
    // Create addLogListener function
    status = napi_create_function(env, NULL, 0, add_log_listener, NULL, &fn);
    if (status != napi_ok) {
        return NULL;
    }
    status = napi_set_named_property(env, exports, "addLogListener", fn);
    if (status != napi_ok) {
        return NULL;
    }
    
    // Create clearLogListener function
    status = napi_create_function(env, NULL, 0, clear_log_listener, NULL, &fn);
    if (status != napi_ok) {
        return NULL;
    }
    status = napi_set_named_property(env, exports, "clearLogListener", fn);
    if (status != napi_ok) {
        return NULL;
    }
    
    // ===== Register Mid-level API Functions =====
    
    // Input/Output Management
    status = napi_create_function(env, NULL, 0, atomic_open_input, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "openInput", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_create_output, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "createOutput", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_get_input_streams, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "getInputStreams", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_add_output_stream, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "addOutputStream", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_close_context, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "closeContext", fn);
    if (status != napi_ok) return NULL;
    
    // Codec Management - Encoder
    status = napi_create_function(env, NULL, 0, atomic_create_encoder, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "createEncoder", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_set_encoder_option, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "setEncoderOption", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_open_encoder, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "openEncoder", fn);
    if (status != napi_ok) return NULL;
    
    // Codec Management - Decoder
    status = napi_create_function(env, NULL, 0, atomic_create_decoder, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "createDecoder", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_copy_decoder_params, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "copyDecoderParams", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_open_decoder, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "openDecoder", fn);
    if (status != napi_ok) return NULL;
    
    // Helper Functions
    status = napi_create_function(env, NULL, 0, atomic_get_encoder_list, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "getEncoderList", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_get_muxer_list, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "getMuxerList", fn);
    if (status != napi_ok) return NULL;
    
    // Transcoding Operations
    status = napi_create_function(env, NULL, 0, atomic_set_output_option, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "setOutputOption", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_write_header, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "writeHeader", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_write_trailer, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "writeTrailer", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_copy_stream_params, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "copyStreamParams", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_copy_encoder_to_stream, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "copyEncoderToStream", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_read_packet, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "readPacket", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_write_packet, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "writePacket", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_free_packet, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "freePacket", fn);
    if (status != napi_ok) return NULL;
    
    // Frame and Packet Operations
    status = napi_create_function(env, NULL, 0, atomic_alloc_frame, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "allocFrame", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_free_frame, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "freeFrame", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_alloc_packet, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "allocPacket", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_send_packet, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "sendPacket", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_receive_frame, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "receiveFrame", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_send_frame, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "sendFrame", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_receive_packet, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "receivePacket", fn);
    if (status != napi_ok) return NULL;
    
    // Frame Data Access
    status = napi_create_function(env, NULL, 0, atomic_frame_get_buffer, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "frameGetBuffer", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_set_frame_property, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "setFrameProperty", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_get_frame_property, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "getFrameProperty", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_get_frame_data, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "getFrameData", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_set_frame_data, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "setFrameData", fn);
    if (status != napi_ok) return NULL;
    
    // Packet Data Access
    status = napi_create_function(env, NULL, 0, atomic_get_packet_data, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "getPacketData", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_set_packet_data, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "setPacketData", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_get_packet_property, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "getPacketProperty", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_set_packet_property, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "setPacketProperty", fn);
    if (status != napi_ok) return NULL;
    
    // Video Scaling
    status = napi_create_function(env, NULL, 0, atomic_create_sws_context, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "createSwsContext", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_sws_scale, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "swsScale", fn);
    if (status != napi_ok) return NULL;
    
    // Audio Resampling
    status = napi_create_function(env, NULL, 0, atomic_create_swr_context, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "createSwrContext", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_swr_convert_frame, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "swrConvertFrame", fn);
    if (status != napi_ok) return NULL;
    
    // Auxiliary Functions
    status = napi_create_function(env, NULL, 0, atomic_seek_input, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "seekInput", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_get_metadata, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "getMetadata", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_set_metadata, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "setMetadata", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_copy_metadata, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "copyMetadata", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_get_supported_pix_fmts, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "getSupportedPixFmts", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_get_supported_sample_fmts, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "getSupportedSampleFmts", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, atomic_get_supported_sample_rates, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "getSupportedSampleRates", fn);
    if (status != napi_ok) return NULL;
    
    // AudioFIFO API
    status = napi_create_function(env, NULL, 0, audio_fifo_alloc, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "audioFifoAlloc", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, audio_fifo_free, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "audioFifoFree", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, audio_fifo_write, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "audioFifoWrite", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, audio_fifo_read, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "audioFifoRead", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, audio_fifo_size, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "audioFifoSize", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, audio_fifo_space, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "audioFifoSpace", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, audio_fifo_reset, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "audioFifoReset", fn);
    if (status != napi_ok) return NULL;
    
    status = napi_create_function(env, NULL, 0, audio_fifo_drain, NULL, &fn);
    if (status != napi_ok) return NULL;
    status = napi_set_named_property(env, exports, "audioFifoDrain", fn);
    if (status != napi_ok) return NULL;
    
    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
