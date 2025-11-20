/**
 * @fileoverview mid-level API - atomic operation interface
 * @module ffmpeg7/mid-level
 * @description provide a fine-grained FFmpeg operation interface, allowing JS to flexibly control the encoding and decoding process
 */

import type { StreamInfo } from './types';

const addon = require('./ffmpeg_node.node');

// ────────────────────────────────────────────────────────────────────────────
// 1. input and output management
// ────────────────────────────────────────────────────────────────────────────

/**
 * open input file, return context handle ID
 * 
 * @param filePath - input file path
 * @returns contextId - context handle ID, for subsequent operations
 * 
 * @example
 * ```typescript
 * import { openInput, getInputStreams, closeContext } from 'ffmpeg7';
 * 
 * const inputCtx = openInput('video.mp4');
 * const streams = getInputStreams(inputCtx);
 * console.log(streams);
 * closeContext(inputCtx);
 * ```
 * 
 * @throws {TypeError} if file path is not a string
 * @throws {Error} if file cannot be opened or parsed
 */
export function openInput(filePath: string): number {
  if (typeof filePath !== 'string') {
    throw new TypeError('Expected file path to be a string');
  }
  return addon.openInput(filePath);
}

/**
 * create output file context
 * 
 * @param filePath - output file path
 * @param format - output format (optional, like "mp4", "mkv")
 * @returns contextId - context handle ID
 * 
 * @example
 * ```typescript
 * import { createOutput, addOutputStream, closeContext } from 'ffmpeg7';
 * 
 * const outputCtx = createOutput('output.mp4', 'mp4');
 * addOutputStream(outputCtx, 'libx264');
 * closeContext(outputCtx);
 * ```
 * 
 * @throws {TypeError} if parameter type is incorrect
 * @throws {Error} if context creation fails
 */
export function createOutput(filePath: string, format?: string): number {
  if (typeof filePath !== 'string') {
    throw new TypeError('Expected file path to be a string');
  }
  if (format !== undefined && typeof format !== 'string') {
    throw new TypeError('Expected format to be a string');
  }
  return addon.createOutput(filePath, format);
}

/**
 * get input file stream information
 * 
 * @param contextId - input context ID (returned by openInput)
 * @returns stream information array
 * 
 * @example
 * ```typescript
 * import { openInput, getInputStreams, closeContext } from 'ffmpeg7';
 * 
 * const inputCtx = openInput('video.mp4');
 * const streams = getInputStreams(inputCtx);
 * 
 * streams.forEach(stream => {
 *   console.log(`Stream ${stream.index}: ${stream.type} - ${stream.codec}`);
 *   if (stream.type === 'video') {
 *     console.log(`  Resolution: ${stream.width}x${stream.height}`);
 *     console.log(`  FPS: ${stream.fps}`);
 *   }
 * });
 * 
 * closeContext(inputCtx);
 * ```
 * 
 * @throws {TypeError} if context ID is not a number
 * @throws {Error} if context ID is invalid
 */
export function getInputStreams(contextId: number): StreamInfo[] {
  if (typeof contextId !== 'number') {
    throw new TypeError('Expected context ID to be a number');
  }
  return addon.getInputStreams(contextId);
}

/**
 * add stream to output context
 * 
 * @param contextId - output context ID (returned by createOutput)
 * @param codecName - codec name (like "libx264", "aac")
 * @returns streamIndex - new created stream index
 * 
 * @example
 * ```typescript
 * import { createOutput, addOutputStream, closeContext } from 'ffmpeg7';
 * 
 * const outputCtx = createOutput('output.mp4');
 * const videoStreamIndex = addOutputStream(outputCtx, 'libx264');
 * const audioStreamIndex = addOutputStream(outputCtx, 'aac');
 * closeContext(outputCtx);
 * ```
 * 
 * @throws {TypeError} if parameter type is incorrect
 * @throws {Error} if context is invalid or codec not found
 */
export function addOutputStream(contextId: number, codecName: string): number {
  if (typeof contextId !== 'number') {
    throw new TypeError('Expected context ID to be a number');
  }
  if (typeof codecName !== 'string') {
    throw new TypeError('Expected codec name to be a string');
  }
  return addon.addOutputStream(contextId, codecName);
}

/**
 * close and release context resources
 * 
 * @param contextId - context ID to close
 * 
 * @example
 * ```typescript
 * import { openInput, closeContext } from 'ffmpeg7';
 * 
 * const ctx = openInput('video.mp4');
 * // ... use context ...
 * closeContext(ctx); // release resources
 * ```
 * 
 * @throws {TypeError} if context ID is not a number
 */
export function closeContext(contextId: number): void {
  if (typeof contextId !== 'number') {
    throw new TypeError('Expected context ID to be a number');
  }
  addon.closeContext(contextId);
}

// ────────────────────────────────────────────────────────────────────────────
// 2. codec management
// ────────────────────────────────────────────────────────────────────────────

/**
 * create encoder context
 * 
 * @param codecName - codec name (like "libx264", "libx265", "aac")
 * @returns codecContextId - encoder context ID
 * 
 * @example
 * ```typescript
 * import { createEncoder, setEncoderOption, openEncoder, closeContext } from 'ffmpeg7';
 * 
 * const encoder = createEncoder('libx264');
 * setEncoderOption(encoder, 'threads', 4);
 * setEncoderOption(encoder, 'preset', 'medium');
 * setEncoderOption(encoder, 'crf', 23);
 * openEncoder(encoder);
 * // ... use encoder ...
 * closeContext(encoder);
 * ```
 * 
 * @throws {TypeError} if codec name is not a string
 * @throws {Error} if encoder not found
 */
export function createEncoder(codecName: string): number {
  if (typeof codecName !== 'string') {
    throw new TypeError('Expected codec name to be a string');
  }
  return addon.createEncoder(codecName);
}

/**
 * set encoder options
 * 
 * @param codecContextId - encoder context ID (returned by createEncoder)
 * @param key - option name
 * @param value - option value (can be a number or string)
 * 
 * @example
 * ```typescript
 * import { createEncoder, setEncoderOption } from 'ffmpeg7';
 * 
 * const encoder = createEncoder('libx264');
 * 
 * // set threads number (number)
 * setEncoderOption(encoder, 'threads', 4);
 * 
 * // set preset (string)
 * setEncoderOption(encoder, 'preset', 'medium');
 * 
 * // set CRF quality
 * setEncoderOption(encoder, 'crf', 23);
 * 
 * // set resolution
 * setEncoderOption(encoder, 'width', 1920);
 * setEncoderOption(encoder, 'height', 1080);
 * 
 * // set bitrate
 * setEncoderOption(encoder, 'bitrate', 2000000);
 * ```
 * 
 * @throws {TypeError} if parameter type is incorrect
 * @throws {Error} if context is invalid or option setting fails
 */
export function setEncoderOption(codecContextId: number, key: string, value: number | string): void {
  if (typeof codecContextId !== 'number') {
    throw new TypeError('Expected codec context ID to be a number');
  }
  if (typeof key !== 'string') {
    throw new TypeError('Expected key to be a string');
  }
  if (typeof value !== 'number' && typeof value !== 'string') {
    throw new TypeError('Expected value to be a number or string');
  }
  addon.setEncoderOption(codecContextId, key, value);
}

/**
 * open encoder (call after setting all options)
 * 
 * @param codecContextId - encoder context ID
 * 
 * @example
 * ```typescript
 * import { createEncoder, setEncoderOption, openEncoder } from 'ffmpeg7';
 * 
 * const encoder = createEncoder('libx264');
 * setEncoderOption(encoder, 'threads', 4);
 * setEncoderOption(encoder, 'preset', 'fast');
 * openEncoder(encoder); // must be called after setting all options
 * ```
 * 
 * @throws {TypeError} if context ID is not a number
 * @throws {Error} if encoder opening fails
 */
export function openEncoder(codecContextId: number): void {
  if (typeof codecContextId !== 'number') {
    throw new TypeError('Expected codec context ID to be a number');
  }
  addon.openEncoder(codecContextId);
}

// ────────────────────────────────────────────────────────────────────────────
// 3. transcoding operations
// ────────────────────────────────────────────────────────────────────────────

/**
 * set output format option (e.g., for faststart moov atom placement)
 * 
 * @param contextId - output context ID (returned by createOutput)
 * @param key - option key (e.g., 'movflags')
 * @param value - option value (e.g., '+faststart')
 * 
 * @example
 * ```typescript
 * import { createOutput, setOutputOption, writeHeader } from 'ffmpeg7';
 * 
 * const outputCtx = createOutput('output.mp4', 'mp4');
 * // Enable faststart for streaming-optimized MP4 (moov atom at beginning)
 * setOutputOption(outputCtx, 'movflags', '+faststart');
 * writeHeader(outputCtx);
 * ```
 * 
 * @throws {TypeError} if parameters are invalid
 * @throws {Error} if option setting fails
 */
export function setOutputOption(contextId: number, key: string, value: string): void {
  if (typeof contextId !== 'number') {
    throw new TypeError('Expected context ID to be a number');
  }
  if (typeof key !== 'string') {
    throw new TypeError('Expected key to be a string');
  }
  if (typeof value !== 'string') {
    throw new TypeError('Expected value to be a string');
  }
  addon.setOutputOption(contextId, key, value);
}

/**
 * write output file header
 * 
 * @param contextId - output context ID (returned by createOutput)
 * 
 * @example
 * ```typescript
 * import { createOutput, addOutputStream, setOutputOption, writeHeader } from 'ffmpeg7';
 * 
 * const outputCtx = createOutput('output.mp4', 'mp4');
 * addOutputStream(outputCtx, 'libx264');
 * setOutputOption(outputCtx, 'movflags', '+faststart'); // optional
 * writeHeader(outputCtx); // must call before writing packets
 * ```
 * 
 * @throws {TypeError} if context ID is not a number
 * @throws {Error} if header writing fails
 */
export function writeHeader(contextId: number): void {
  if (typeof contextId !== 'number') {
    throw new TypeError('Expected context ID to be a number');
  }
  addon.writeHeader(contextId);
}

/**
 * write output file trailer
 * 
 * @param contextId - output context ID (returned by createOutput)
 * 
 * @example
 * ```typescript
 * import { writeTrailer } from 'ffmpeg7';
 * 
 * // ... after writing all packets ...
 * writeTrailer(outputCtx); // finalize the file
 * ```
 * 
 * @throws {TypeError} if context ID is not a number
 * @throws {Error} if trailer writing fails
 */
export function writeTrailer(contextId: number): void {
  if (typeof contextId !== 'number') {
    throw new TypeError('Expected context ID to be a number');
  }
  addon.writeTrailer(contextId);
}

/**
 * copy stream parameters from input to output
 * 
 * @param inputContextId - input context ID
 * @param outputContextId - output context ID
 * @param inputStreamIndex - input stream index
 * @param outputStreamIndex - output stream index
 * 
 * @example
 * ```typescript
 * import { openInput, createOutput, addOutputStream, copyStreamParams } from 'ffmpeg7';
 * 
 * const inputCtx = openInput('input.mp4');
 * const outputCtx = createOutput('output.mp4');
 * const outStreamIdx = addOutputStream(outputCtx, 'libx264');
 * copyStreamParams(inputCtx, outputCtx, 0, outStreamIdx);
 * ```
 * 
 * @throws {TypeError} if parameter types are incorrect
 * @throws {Error} if copy fails
 */
export function copyStreamParams(
  inputContextId: number,
  outputContextId: number,
  inputStreamIndex: number,
  outputStreamIndex: number
): void {
  if (typeof inputContextId !== 'number' || typeof outputContextId !== 'number') {
    throw new TypeError('Expected context IDs to be numbers');
  }
  if (typeof inputStreamIndex !== 'number' || typeof outputStreamIndex !== 'number') {
    throw new TypeError('Expected stream indices to be numbers');
  }
  addon.copyStreamParams(inputContextId, outputContextId, inputStreamIndex, outputStreamIndex);
}

/**
 * copy encoder parameters to output stream
 * 
 * @param encoderContextId - encoder context ID
 * @param outputContextId - output context ID
 * @param outputStreamIndex - output stream index
 * 
 * @example
 * ```typescript
 * import { createEncoder, setEncoderOption, openEncoder, createOutput, addOutputStream, copyEncoderToStream } from 'ffmpeg7';
 * 
 * const encoder = createEncoder('libx264');
 * setEncoderOption(encoder, 'width', 1920);
 * setEncoderOption(encoder, 'height', 1080);
 * setEncoderOption(encoder, 'bitrate', 2000000);
 * openEncoder(encoder);
 * 
 * const outputCtx = createOutput('output.mp4');
 * const outStreamIdx = addOutputStream(outputCtx, 'libx264');
 * copyEncoderToStream(encoder, outputCtx, outStreamIdx); // copy encoder params to stream
 * ```
 * 
 * @throws {TypeError} if parameter types are incorrect
 * @throws {Error} if copy fails
 */
export function copyEncoderToStream(
  encoderContextId: number,
  outputContextId: number,
  outputStreamIndex: number
): void {
  if (typeof encoderContextId !== 'number' || typeof outputContextId !== 'number') {
    throw new TypeError('Expected context IDs to be numbers');
  }
  if (typeof outputStreamIndex !== 'number') {
    throw new TypeError('Expected stream index to be a number');
  }
  addon.copyEncoderToStream(encoderContextId, outputContextId, outputStreamIndex);
}

/**
 * read packet from input
 * 
 * @param contextId - input context ID (returned by openInput)
 * @returns packet object or null if EOF
 * 
 * @example
 * ```typescript
 * import { openInput, readPacket, freePacket } from 'ffmpeg7';
 * 
 * const inputCtx = openInput('input.mp4');
 * let packet;
 * while ((packet = readPacket(inputCtx)) !== null) {
 *   console.log(`Read packet from stream ${packet.streamIndex}`);
 *   freePacket(packet.id);
 * }
 * ```
 * 
 * @throws {TypeError} if context ID is not a number
 * @throws {Error} if reading fails
 */
export function readPacket(contextId: number): { id: number; streamIndex: number; pts: number; dts: number; duration: number } | null {
  if (typeof contextId !== 'number') {
    throw new TypeError('Expected context ID to be a number');
  }
  return addon.readPacket(contextId);
}

/**
 * write packet to output
 * 
 * @param outputContextId - output context ID
 * @param packetId - packet ID (from readPacket)
 * @param outputStreamIndex - output stream index
 * @param inputContextId - optional input context ID for timestamp rescaling
 * @param inputStreamIndex - optional input stream index for timestamp rescaling
 * 
 * @example
 * ```typescript
 * import { readPacket, writePacket, freePacket } from 'ffmpeg7';
 * 
 * // Simple write (encoder output)
 * const packet = readPacket(inputCtx);
 * if (packet) {
 *   writePacket(outputCtx, packet.id, 0);
 *   freePacket(packet.id);
 * }
 * 
 * // Write with timestamp rescaling (remuxing)
 * const packet = readPacket(inputCtx);
 * if (packet) {
 *   writePacket(outputCtx, packet.id, 0, inputCtx, packet.streamIndex);
 *   freePacket(packet.id);
 * }
 * ```
 * 
 * @throws {TypeError} if parameter types are incorrect
 * @throws {Error} if writing fails
 */
export function writePacket(
  outputContextId: number, 
  packetId: number, 
  outputStreamIndex: number,
  inputContextId?: number,
  inputStreamIndex?: number
): void {
  if (typeof outputContextId !== 'number' || typeof packetId !== 'number' || typeof outputStreamIndex !== 'number') {
    throw new TypeError('Expected all parameters to be numbers');
  }
  if (inputContextId !== undefined && typeof inputContextId !== 'number') {
    throw new TypeError('inputContextId must be a number');
  }
  if (inputStreamIndex !== undefined && typeof inputStreamIndex !== 'number') {
    throw new TypeError('inputStreamIndex must be a number');
  }
  
  if (inputContextId !== undefined && inputStreamIndex !== undefined) {
    addon.writePacket(outputContextId, packetId, outputStreamIndex, inputContextId, inputStreamIndex);
  } else {
    addon.writePacket(outputContextId, packetId, outputStreamIndex);
  }
}

/**
 * free packet resources
 * 
 * @param packetId - packet ID (from readPacket)
 * 
 * @example
 * ```typescript
 * import { readPacket, freePacket } from 'ffmpeg7';
 * 
 * const packet = readPacket(inputCtx);
 * if (packet) {
 *   // ... use packet ...
 *   freePacket(packet.id); // always free when done
 * }
 * ```
 * 
 * @throws {TypeError} if packet ID is not a number
 */
export function freePacket(packetId: number): void {
  if (typeof packetId !== 'number') {
    throw new TypeError('Expected packet ID to be a number');
  }
  addon.freePacket(packetId);
}

// ────────────────────────────────────────────────────────────────────────────
// 4. auxiliary functions
// ────────────────────────────────────────────────────────────────────────────

/**
 * get available encoder list
 * 
 * @param type - filter type (optional, "video" or "audio")
 * @returns encoder name array
 * 
 * @example
 * ```typescript
 * import { getEncoderList } from 'ffmpeg7';
 * 
 * // get all encoders
 * const allEncoders = getEncoderList();
 * 
 * // get video encoders only
 * const videoEncoders = getEncoderList('video');
 * console.log(videoEncoders); // ['libx264', 'libx265', 'libvpx', ...]
 * 
 * // get audio encoders only
 * const audioEncoders = getEncoderList('audio');
 * console.log(audioEncoders); // ['aac', 'mp3', 'opus', ...]
 * ```
 * 
 * @throws {TypeError} if type parameter is invalid
 */
export function getEncoderList(type?: 'video' | 'audio'): string[] {
  if (type !== undefined && type !== 'video' && type !== 'audio') {
    throw new TypeError('Expected type to be "video" or "audio"');
  }
  return addon.getEncoderList(type);
}

/**
 * get available output format (muxer) list
 * 
 * @returns format name array
 * 
 * @example
 * ```typescript
 * import { getMuxerList } from 'ffmpeg7';
 * 
 * const formats = getMuxerList();
 * console.log(formats); // ['mp4', 'mkv', 'avi', 'webm', ...]
 * ```
 */
export function getMuxerList(): string[] {
  return addon.getMuxerList();
}

// ────────────────────────────────────────────────────────────────────────────
// 4. Decoder Management
// ────────────────────────────────────────────────────────────────────────────

/**
 * create decoder context
 * 
 * @param codecName - codec name (e.g., "h264", "aac")
 * @returns codecContextId - decoder context ID
 * 
 * @example
 * ```typescript
 * import { createDecoder, copyDecoderParams, openDecoder } from 'ffmpeg7';
 * 
 * const decoder = createDecoder('h264');
 * copyDecoderParams(inputCtx, decoder, 0); // copy from input stream 0
 * openDecoder(decoder);
 * ```
 * 
 * @throws {TypeError} if codec name is not a string
 * @throws {Error} if decoder not found
 */
export function createDecoder(codecName: string): number {
  if (typeof codecName !== 'string') {
    throw new TypeError('Expected codec name to be a string');
  }
  return addon.createDecoder(codecName);
}

/**
 * copy codec parameters from input stream to decoder context
 * 
 * @param inputContextId - input format context ID
 * @param decoderContextId - decoder context ID
 * @param streamIndex - input stream index
 * 
 * @example
 * ```typescript
 * import { openInput, createDecoder, copyDecoderParams, openDecoder } from 'ffmpeg7';
 * 
 * const inputCtx = openInput('video.mp4');
 * const decoder = createDecoder('h264');
 * copyDecoderParams(inputCtx, decoder, 0); // copy params from stream 0
 * openDecoder(decoder);
 * ```
 * 
 * @throws {TypeError} if parameter types are incorrect
 * @throws {Error} if copy fails
 */
export function copyDecoderParams(inputContextId: number, decoderContextId: number, streamIndex: number): void {
  if (typeof inputContextId !== 'number' || typeof decoderContextId !== 'number') {
    throw new TypeError('Expected context IDs to be numbers');
  }
  if (typeof streamIndex !== 'number') {
    throw new TypeError('Expected stream index to be a number');
  }
  addon.copyDecoderParams(inputContextId, decoderContextId, streamIndex);
}

/**
 * open decoder (call after copying parameters)
 * 
 * @param codecContextId - decoder context ID
 * 
 * @example
 * ```typescript
 * import { createDecoder, copyDecoderParams, openDecoder } from 'ffmpeg7';
 * 
 * const decoder = createDecoder('h264');
 * copyDecoderParams(inputCtx, decoder, 0);
 * openDecoder(decoder); // must be called after copying parameters
 * ```
 * 
 * @throws {TypeError} if context ID is not a number
 * @throws {Error} if decoder opening fails
 */
export function openDecoder(codecContextId: number): void {
  if (typeof codecContextId !== 'number') {
    throw new TypeError('Expected codec context ID to be a number');
  }
  addon.openDecoder(codecContextId);
}

// ────────────────────────────────────────────────────────────────────────────
// 5. Frame and Packet Processing
// ────────────────────────────────────────────────────────────────────────────

/**
 * allocate a new frame
 * 
 * @returns frameId - frame ID
 * 
 * @example
 * ```typescript
 * import { allocFrame, freeFrame } from 'ffmpeg7';
 * 
 * const frame = allocFrame();
 * // ... use frame ...
 * freeFrame(frame);
 * ```
 * 
 * @throws {Error} if frame allocation fails
 */
export function allocFrame(): number {
  return addon.allocFrame();
}

/**
 * free frame resources
 * 
 * @param frameId - frame ID
 * 
 * @example
 * ```typescript
 * import { allocFrame, freeFrame } from 'ffmpeg7';
 * 
 * const frame = allocFrame();
 * // ... use frame ...
 * freeFrame(frame); // always free when done
 * ```
 * 
 * @throws {TypeError} if frame ID is not a number
 */
export function freeFrame(frameId: number): void {
  if (typeof frameId !== 'number') {
    throw new TypeError('Expected frame ID to be a number');
  }
  addon.freeFrame(frameId);
}

/**
 * allocate an output packet
 * 
 * @returns packetId - packet ID
 * 
 * @example
 * ```typescript
 * import { allocPacket, freePacket } from 'ffmpeg7';
 * 
 * const pkt = allocPacket();
 * // ... use packet ...
 * freePacket(pkt);
 * ```
 * 
 * @throws {Error} if packet allocation fails
 */
export function allocPacket(): number {
  return addon.allocPacket();
}

/**
 * send packet to decoder
 * 
 * @param decoderContextId - decoder context ID
 * @param packetId - packet ID (or null to flush)
 * @returns status - 0 (success), -1 (EAGAIN), -2 (EOF), -3 (error)
 * 
 * @example
 * ```typescript
 * import { sendPacket, receiveFrame } from 'ffmpeg7';
 * 
 * const status = sendPacket(decoder, packetId);
 * if (status === 0) {
 *   // packet accepted
 * } else if (status === -1) {
 *   // need to read more frames first
 * }
 * ```
 * 
 * @throws {TypeError} if parameter types are incorrect
 */
export function sendPacket(decoderContextId: number, packetId: number | null): number {
  if (typeof decoderContextId !== 'number') {
    throw new TypeError('Expected decoder context ID to be a number');
  }
  return addon.sendPacket(decoderContextId, packetId);
}

/**
 * receive frame from decoder
 * 
 * @param decoderContextId - decoder context ID
 * @param frameId - frame ID to receive into
 * @returns status - 0 (success), -1 (EAGAIN), -2 (EOF), -3 (error)
 * 
 * @example
 * ```typescript
 * import { sendPacket, receiveFrame, allocFrame } from 'ffmpeg7';
 * 
 * const frame = allocFrame();
 * sendPacket(decoder, packetId);
 * const status = receiveFrame(decoder, frame);
 * if (status === 0) {
 *   // frame ready
 * } else if (status === -1) {
 *   // need to send more packets
 * }
 * ```
 * 
 * @throws {TypeError} if parameter types are incorrect
 */
export function receiveFrame(decoderContextId: number, frameId: number): number {
  if (typeof decoderContextId !== 'number' || typeof frameId !== 'number') {
    throw new TypeError('Expected decoder context ID and frame ID to be numbers');
  }
  return addon.receiveFrame(decoderContextId, frameId);
}

/**
 * send frame to encoder
 * 
 * @param encoderContextId - encoder context ID
 * @param frameId - frame ID (or null to flush)
 * @returns status - 0 (success), -1 (EAGAIN), -2 (EOF), -3 (error)
 * 
 * @example
 * ```typescript
 * import { sendFrame, receivePacket } from 'ffmpeg7';
 * 
 * const status = sendFrame(encoder, frameId);
 * if (status === 0) {
 *   // frame accepted
 * } else if (status === -1) {
 *   // need to read more packets first
 * }
 * ```
 * 
 * @throws {TypeError} if parameter types are incorrect
 */
export function sendFrame(encoderContextId: number, frameId: number | null): number {
  if (typeof encoderContextId !== 'number') {
    throw new TypeError('Expected encoder context ID to be a number');
  }
  return addon.sendFrame(encoderContextId, frameId);
}

/**
 * receive packet from encoder
 * 
 * @param encoderContextId - encoder context ID
 * @param packetId - packet ID to receive into
 * @returns status - 0 (success), -1 (EAGAIN), -2 (EOF), -3 (error)
 * 
 * @example
 * ```typescript
 * import { sendFrame, receivePacket, allocPacket } from 'ffmpeg7';
 * 
 * const pkt = allocPacket();
 * sendFrame(encoder, frameId);
 * const status = receivePacket(encoder, pkt);
 * if (status === 0) {
 *   // packet ready
 * } else if (status === -1) {
 *   // need to send more frames
 * }
 * ```
 * 
 * @throws {TypeError} if parameter types are incorrect
 */
export function receivePacket(encoderContextId: number, packetId: number): number {
  if (typeof encoderContextId !== 'number' || typeof packetId !== 'number') {
    throw new TypeError('Expected encoder context ID and packet ID to be numbers');
  }
  return addon.receivePacket(encoderContextId, packetId);
}

// ────────────────────────────────────────────────────────────────────────────
// 6. Frame Data Access and Manipulation
// ────────────────────────────────────────────────────────────────────────────

/**
 * allocate frame buffer
 * 
 * @param frameId - frame ID
 * @param align - buffer alignment (0 for default)
 * 
 * @example
 * ```typescript
 * import { allocFrame, frameGetBuffer, setFrameProperty } from 'ffmpeg7';
 * 
 * const frame = allocFrame();
 * setFrameProperty(frame, 'width', 1920);
 * setFrameProperty(frame, 'height', 1080);
 * setFrameProperty(frame, 'format', 0); // AV_PIX_FMT_YUV420P
 * frameGetBuffer(frame, 0); // allocate buffer
 * ```
 * 
 * @throws {TypeError} if parameter types are incorrect
 * @throws {Error} if buffer allocation fails
 */
export function frameGetBuffer(frameId: number, align?: number): void {
  if (typeof frameId !== 'number') {
    throw new TypeError('Expected frame ID to be a number');
  }
  if (align !== undefined && typeof align !== 'number') {
    throw new TypeError('Expected align to be a number');
  }
  addon.frameGetBuffer(frameId, align);
}

/**
 * set frame property
 * 
 * @param frameId - frame ID
 * @param property - property name (pts, width, height, format, pict_type, key_frame, sample_rate, nb_samples)
 * @param value - property value
 * 
 * @example
 * ```typescript
 * import { allocFrame, setFrameProperty } from 'ffmpeg7';
 * 
 * const frame = allocFrame();
 * setFrameProperty(frame, 'width', 1920);
 * setFrameProperty(frame, 'height', 1080);
 * setFrameProperty(frame, 'format', 0); // pixel format
 * setFrameProperty(frame, 'pts', 0);
 * ```
 * 
 * @throws {TypeError} if parameter types are incorrect
 * @throws {Error} if property is unknown or unsupported
 */
export function setFrameProperty(frameId: number, property: string, value: number): void {
  if (typeof frameId !== 'number') {
    throw new TypeError('Expected frame ID to be a number');
  }
  if (typeof property !== 'string') {
    throw new TypeError('Expected property to be a string');
  }
  if (typeof value !== 'number') {
    throw new TypeError('Expected value to be a number');
  }
  addon.setFrameProperty(frameId, property, value);
}

/**
 * get frame property
 * 
 * @param frameId - frame ID
 * @param property - property name (pts, width, height, format, pict_type, key_frame, sample_rate, nb_samples, linesize)
 * @returns property value (number or array for linesize)
 * 
 * @example
 * ```typescript
 * import { allocFrame, getFrameProperty } from 'ffmpeg7';
 * 
 * const frame = allocFrame();
 * const width = getFrameProperty(frame, 'width');
 * const height = getFrameProperty(frame, 'height');
 * const pts = getFrameProperty(frame, 'pts');
 * const linesize = getFrameProperty(frame, 'linesize'); // returns array
 * ```
 * 
 * @throws {TypeError} if parameter types are incorrect
 * @throws {Error} if property is unknown
 */
export function getFrameProperty(frameId: number, property: string): number | number[] {
  if (typeof frameId !== 'number') {
    throw new TypeError('Expected frame ID to be a number');
  }
  if (typeof property !== 'string') {
    throw new TypeError('Expected property to be a string');
  }
  return addon.getFrameProperty(frameId, property);
}

/**
 * get frame data as Buffer
 * 
 * @param frameId - frame ID
 * @param planeIndex - plane index (0-7)
 * @returns Node.js Buffer containing plane data or null for unused planes
 * 
 * @example
 * ```typescript
 * import { allocFrame, getFrameData } from 'ffmpeg7';
 * 
 * const frame = allocFrame();
 * // ... after receiving decoded frame ...
 * const yPlane = getFrameData(frame, 0); // Y plane
 * const uPlane = getFrameData(frame, 1); // U plane
 * const vPlane = getFrameData(frame, 2); // V plane
 * ```
 * 
 * @throws {TypeError} if parameter types are incorrect
 * @throws {Error} if plane index is invalid
 */
export function getFrameData(frameId: number, planeIndex: number): Buffer | null {
  if (typeof frameId !== 'number' || typeof planeIndex !== 'number') {
    throw new TypeError('Expected frame ID and plane index to be numbers');
  }
  return addon.getFrameData(frameId, planeIndex);
}

/**
 * set frame data from Buffer
 * 
 * @param frameId - frame ID
 * @param planeIndex - plane index (0-7)
 * @param buffer - Node.js Buffer with data
 * 
 * @example
 * ```typescript
 * import { allocFrame, frameGetBuffer, setFrameData } from 'ffmpeg7';
 * 
 * const frame = allocFrame();
 * setFrameProperty(frame, 'width', 1920);
 * setFrameProperty(frame, 'height', 1080);
 * setFrameProperty(frame, 'format', 0);
 * frameGetBuffer(frame, 0);
 * 
 * const yData = Buffer.alloc(1920 * 1080);
 * setFrameData(frame, 0, yData);
 * ```
 * 
 * @throws {TypeError} if parameter types are incorrect
 * @throws {Error} if frame buffer is not allocated or buffer size is invalid
 */
export function setFrameData(frameId: number, planeIndex: number, buffer: Buffer): void {
  if (typeof frameId !== 'number' || typeof planeIndex !== 'number') {
    throw new TypeError('Expected frame ID and plane index to be numbers');
  }
  if (!Buffer.isBuffer(buffer)) {
    throw new TypeError('Expected buffer to be a Buffer');
  }
  addon.setFrameData(frameId, planeIndex, buffer);
}

// ────────────────────────────────────────────────────────────────────────────
// 7. Packet Data Access and Manipulation
// ────────────────────────────────────────────────────────────────────────────

/**
 * get packet data as Buffer
 * 
 * @param packetId - packet ID
 * @returns Node.js Buffer containing packet data or null for empty packet
 * 
 * @example
 * ```typescript
 * import { readPacket, getPacketData } from 'ffmpeg7';
 * 
 * const packet = readPacket(inputCtx);
 * if (packet) {
 *   const data = getPacketData(packet.id);
 *   console.log(`Packet size: ${data?.length} bytes`);
 * }
 * ```
 * 
 * @throws {TypeError} if packet ID is not a number
 * @throws {Error} if packet is invalid
 */
export function getPacketData(packetId: number): Buffer | null {
  if (typeof packetId !== 'number') {
    throw new TypeError('Expected packet ID to be a number');
  }
  return addon.getPacketData(packetId);
}

/**
 * set packet data from Buffer
 * 
 * @param packetId - packet ID
 * @param buffer - Node.js Buffer with data
 * 
 * @example
 * ```typescript
 * import { allocPacket, setPacketData } from 'ffmpeg7';
 * 
 * const pkt = allocPacket();
 * const data = Buffer.from([0x00, 0x01, 0x02, 0x03]);
 * setPacketData(pkt, data);
 * ```
 * 
 * @throws {TypeError} if parameter types are incorrect
 * @throws {Error} if packet is invalid or buffer allocation fails
 */
export function setPacketData(packetId: number, buffer: Buffer): void {
  if (typeof packetId !== 'number') {
    throw new TypeError('Expected packet ID to be a number');
  }
  if (!Buffer.isBuffer(buffer)) {
    throw new TypeError('Expected buffer to be a Buffer');
  }
  addon.setPacketData(packetId, buffer);
}

/**
 * get packet property
 * 
 * @param packetId - packet ID
 * @param property - property name (pts, dts, duration, streamIndex, flags, size)
 * @returns property value
 * 
 * @example
 * ```typescript
 * import { readPacket, getPacketProperty } from 'ffmpeg7';
 * 
 * const packet = readPacket(inputCtx);
 * if (packet) {
 *   const pts = getPacketProperty(packet.id, 'pts');
 *   const dts = getPacketProperty(packet.id, 'dts');
 *   const size = getPacketProperty(packet.id, 'size');
 * }
 * ```
 * 
 * @throws {TypeError} if parameter types are incorrect
 * @throws {Error} if property is unknown
 */
export function getPacketProperty(packetId: number, property: string): number {
  if (typeof packetId !== 'number') {
    throw new TypeError('Expected packet ID to be a number');
  }
  if (typeof property !== 'string') {
    throw new TypeError('Expected property to be a string');
  }
  return addon.getPacketProperty(packetId, property);
}

/**
 * set packet property
 * 
 * @param packetId - packet ID
 * @param property - property name (pts, dts, duration, streamIndex, flags)
 * @param value - property value
 * 
 * @example
 * ```typescript
 * import { allocPacket, setPacketProperty } from 'ffmpeg7';
 * 
 * const pkt = allocPacket();
 * setPacketProperty(pkt, 'pts', 1000);
 * setPacketProperty(pkt, 'dts', 1000);
 * setPacketProperty(pkt, 'duration', 40);
 * setPacketProperty(pkt, 'streamIndex', 0);
 * ```
 * 
 * @throws {TypeError} if parameter types are incorrect
 * @throws {Error} if property is unknown or read-only
 */
export function setPacketProperty(packetId: number, property: string, value: number): void {
  if (typeof packetId !== 'number') {
    throw new TypeError('Expected packet ID to be a number');
  }
  if (typeof property !== 'string') {
    throw new TypeError('Expected property to be a string');
  }
  if (typeof value !== 'number') {
    throw new TypeError('Expected value to be a number');
  }
  addon.setPacketProperty(packetId, property, value);
}

// ────────────────────────────────────────────────────────────────────────────
// 8. Video Scaling (SwsContext)
// ────────────────────────────────────────────────────────────────────────────

/**
 * create software scaler context for video scaling/format conversion
 * 
 * @param srcWidth - source width
 * @param srcHeight - source height
 * @param srcFormat - source pixel format (string name or enum value)
 * @param dstWidth - destination width
 * @param dstHeight - destination height
 * @param dstFormat - destination pixel format (string name or enum value)
 * @param flags - scaling algorithm flags (optional, default: SWS_BILINEAR)
 * @returns swsContextId - scaler context ID
 * 
 * @example
 * ```typescript
 * import { createSwsContext, swsScale, closeContext } from 'ffmpeg7';
 * 
 * // scale from 1920x1080 to 1280x720
 * const swsCtx = createSwsContext(
 *   1920, 1080, 'yuv420p',
 *   1280, 720, 'yuv420p',
 *   2 // SWS_BILINEAR
 * );
 * 
 * swsScale(swsCtx, srcFrame, dstFrame);
 * closeContext(swsCtx);
 * ```
 * 
 * @throws {TypeError} if parameter types are incorrect
 * @throws {Error} if scaler context creation fails
 */
export function createSwsContext(
  srcWidth: number,
  srcHeight: number,
  srcFormat: string | number,
  dstWidth: number,
  dstHeight: number,
  dstFormat: string | number,
  flags?: number
): number {
  if (typeof srcWidth !== 'number' || typeof srcHeight !== 'number') {
    throw new TypeError('Expected source dimensions to be numbers');
  }
  if (typeof srcFormat !== 'string' && typeof srcFormat !== 'number') {
    throw new TypeError('Expected source format to be a string or number');
  }
  if (typeof dstWidth !== 'number' || typeof dstHeight !== 'number') {
    throw new TypeError('Expected destination dimensions to be numbers');
  }
  if (typeof dstFormat !== 'string' && typeof dstFormat !== 'number') {
    throw new TypeError('Expected destination format to be a string or number');
  }
  if (flags !== undefined && typeof flags !== 'number') {
    throw new TypeError('Expected flags to be a number');
  }
  return addon.createSwsContext(srcWidth, srcHeight, srcFormat, dstWidth, dstHeight, dstFormat, flags);
}

/**
 * scale frame using sws context
 * 
 * @param swsContextId - scaler context ID
 * @param srcFrameId - source frame ID
 * @param dstFrameId - destination frame ID
 * 
 * @example
 * ```typescript
 * import { createSwsContext, swsScale, allocFrame, frameGetBuffer } from 'ffmpeg7';
 * 
 * const swsCtx = createSwsContext(1920, 1080, 'yuv420p', 1280, 720, 'yuv420p');
 * 
 * // prepare destination frame
 * const dstFrame = allocFrame();
 * setFrameProperty(dstFrame, 'width', 1280);
 * setFrameProperty(dstFrame, 'height', 720);
 * setFrameProperty(dstFrame, 'format', 0);
 * frameGetBuffer(dstFrame, 0);
 * 
 * // perform scaling
 * swsScale(swsCtx, srcFrame, dstFrame);
 * ```
 * 
 * @throws {TypeError} if parameter types are incorrect
 * @throws {Error} if scaling fails
 */
export function swsScale(swsContextId: number, srcFrameId: number, dstFrameId: number): void {
  if (typeof swsContextId !== 'number' || typeof srcFrameId !== 'number' || typeof dstFrameId !== 'number') {
    throw new TypeError('Expected all parameters to be numbers');
  }
  addon.swsScale(swsContextId, srcFrameId, dstFrameId);
}

// ────────────────────────────────────────────────────────────────────────────
// 9. Audio Resampling (SwrContext)
// ────────────────────────────────────────────────────────────────────────────

/**
 * create software resample context for audio resampling/format conversion
 * 
 * @param srcSampleRate - source sample rate
 * @param srcChannels - source channel count
 * @param srcFormat - source sample format (string name or enum value)
 * @param dstSampleRate - destination sample rate
 * @param dstChannels - destination channel count
 * @param dstFormat - destination sample format (string name or enum value)
 * @returns swrContextId - resample context ID
 * 
 * @example
 * ```typescript
 * import { createSwrContext, swrConvertFrame, closeContext } from 'ffmpeg7';
 * 
 * // resample from 48kHz stereo to 44.1kHz stereo
 * const swrCtx = createSwrContext(
 *   48000, 2, 'fltp',  // source
 *   44100, 2, 'fltp'   // destination
 * );
 * 
 * swrConvertFrame(swrCtx, srcFrame, dstFrame);
 * closeContext(swrCtx);
 * ```
 * 
 * @throws {TypeError} if parameter types are incorrect
 * @throws {Error} if resample context creation fails
 */
export function createSwrContext(
  srcSampleRate: number,
  srcChannels: number,
  srcFormat: string | number,
  dstSampleRate: number,
  dstChannels: number,
  dstFormat: string | number
): number {
  if (typeof srcSampleRate !== 'number' || typeof srcChannels !== 'number') {
    throw new TypeError('Expected source parameters to be numbers');
  }
  if (typeof srcFormat !== 'string' && typeof srcFormat !== 'number') {
    throw new TypeError('Expected source format to be a string or number');
  }
  if (typeof dstSampleRate !== 'number' || typeof dstChannels !== 'number') {
    throw new TypeError('Expected destination parameters to be numbers');
  }
  if (typeof dstFormat !== 'string' && typeof dstFormat !== 'number') {
    throw new TypeError('Expected destination format to be a string or number');
  }
  return addon.createSwrContext(srcSampleRate, srcChannels, srcFormat, dstSampleRate, dstChannels, dstFormat);
}

/**
 * resample audio frame using swr context
 * 
 * @param swrContextId - resample context ID
 * @param srcFrameId - source frame ID (or null for flushing)
 * @param dstFrameId - destination frame ID
 * @returns number of samples output per channel
 * 
 * @example
 * ```typescript
 * import { createSwrContext, swrConvertFrame, allocFrame } from 'ffmpeg7';
 * 
 * const swrCtx = createSwrContext(48000, 2, 'fltp', 44100, 2, 'fltp');
 * 
 * const dstFrame = allocFrame();
 * const samplesOut = swrConvertFrame(swrCtx, srcFrame, dstFrame);
 * console.log(`Resampled ${samplesOut} samples`);
 * 
 * // flush remaining samples
 * swrConvertFrame(swrCtx, null, dstFrame);
 * ```
 * 
 * @throws {TypeError} if parameter types are incorrect
 * @throws {Error} if resampling fails
 */
export function swrConvertFrame(swrContextId: number, srcFrameId: number | null, dstFrameId: number): number {
  if (typeof swrContextId !== 'number' || typeof dstFrameId !== 'number') {
    throw new TypeError('Expected context and destination frame IDs to be numbers');
  }
  if (srcFrameId !== null && typeof srcFrameId !== 'number') {
    throw new TypeError('Expected source frame ID to be a number or null');
  }
  return addon.swrConvertFrame(swrContextId, srcFrameId, dstFrameId);
}

// ────────────────────────────────────────────────────────────────────────────
// 10. Auxiliary Functions - Seek, Metadata, Format Query
// ────────────────────────────────────────────────────────────────────────────

/**
 * seek to timestamp in input file
 * 
 * @param inputContextId - input context ID
 * @param timestamp - target timestamp
 * @param streamIndex - stream index (-1 for default)
 * @param flags - seek flags (optional, default: AVSEEK_FLAG_BACKWARD)
 * 
 * @example
 * ```typescript
 * import { openInput, seekInput } from 'ffmpeg7';
 * 
 * const inputCtx = openInput('video.mp4');
 * // seek to 5 seconds (assuming time_base 1/1000000)
 * seekInput(inputCtx, 5000000, -1);
 * ```
 * 
 * @throws {TypeError} if parameter types are incorrect
 * @throws {Error} if seek fails
 */
export function seekInput(inputContextId: number, timestamp: number, streamIndex?: number, flags?: number): void {
  if (typeof inputContextId !== 'number' || typeof timestamp !== 'number') {
    throw new TypeError('Expected context ID and timestamp to be numbers');
  }
  if (streamIndex !== undefined && typeof streamIndex !== 'number') {
    throw new TypeError('Expected stream index to be a number');
  }
  if (flags !== undefined && typeof flags !== 'number') {
    throw new TypeError('Expected flags to be a number');
  }
  addon.seekInput(inputContextId, timestamp, streamIndex, flags);
}

/**
 * get metadata from input context
 * 
 * @param inputContextId - input context ID
 * @param key - metadata key (optional, returns all metadata if not provided)
 * @returns metadata value or object with all metadata
 * 
 * @example
 * ```typescript
 * import { openInput, getMetadata } from 'ffmpeg7';
 * 
 * const inputCtx = openInput('video.mp4');
 * 
 * // get all metadata
 * const allMetadata = getMetadata(inputCtx);
 * console.log(allMetadata); // { title: '...', artist: '...', ... }
 * 
 * // get specific key
 * const title = getMetadata(inputCtx, 'title');
 * console.log(title);
 * ```
 * 
 * @throws {TypeError} if parameter types are incorrect
 * @throws {Error} if context is invalid
 */
export function getMetadata(inputContextId: number, key?: string): string | Record<string, string> | null {
  if (typeof inputContextId !== 'number') {
    throw new TypeError('Expected context ID to be a number');
  }
  if (key !== undefined && typeof key !== 'string') {
    throw new TypeError('Expected key to be a string');
  }
  return addon.getMetadata(inputContextId, key);
}

/**
 * set metadata on output context
 * 
 * @param outputContextId - output context ID
 * @param key - metadata key
 * @param value - metadata value
 * 
 * @example
 * ```typescript
 * import { createOutput, setMetadata } from 'ffmpeg7';
 * 
 * const outputCtx = createOutput('output.mp4');
 * setMetadata(outputCtx, 'title', 'My Video');
 * setMetadata(outputCtx, 'artist', 'John Doe');
 * setMetadata(outputCtx, 'comment', 'Created with FFmpeg');
 * ```
 * 
 * @throws {TypeError} if parameter types are incorrect
 * @throws {Error} if setting fails
 */
export function setMetadata(outputContextId: number, key: string, value: string): void {
  if (typeof outputContextId !== 'number') {
    throw new TypeError('Expected context ID to be a number');
  }
  if (typeof key !== 'string' || typeof value !== 'string') {
    throw new TypeError('Expected key and value to be strings');
  }
  addon.setMetadata(outputContextId, key, value);
}

/**
 * copy metadata from input to output
 * 
 * @param inputContextId - input context ID
 * @param outputContextId - output context ID
 * 
 * @example
 * ```typescript
 * import { openInput, createOutput, copyMetadata } from 'ffmpeg7';
 * 
 * const inputCtx = openInput('input.mp4');
 * const outputCtx = createOutput('output.mp4');
 * copyMetadata(inputCtx, outputCtx); // copy all metadata
 * ```
 * 
 * @throws {TypeError} if parameter types are incorrect
 * @throws {Error} if copy fails
 */
export function copyMetadata(inputContextId: number, outputContextId: number): void {
  if (typeof inputContextId !== 'number' || typeof outputContextId !== 'number') {
    throw new TypeError('Expected context IDs to be numbers');
  }
  addon.copyMetadata(inputContextId, outputContextId);
}

/**
 * get supported pixel formats for encoder
 * 
 * @param codecContextId - encoder context ID
 * @returns array of pixel format names
 * 
 * @example
 * ```typescript
 * import { createEncoder, getSupportedPixFmts } from 'ffmpeg7';
 * 
 * const encoder = createEncoder('libx264');
 * const formats = getSupportedPixFmts(encoder);
 * console.log(formats); // ['yuv420p', 'yuvj420p', 'yuv422p', ...]
 * ```
 * 
 * @throws {TypeError} if context ID is not a number
 * @throws {Error} if context is invalid
 */
export function getSupportedPixFmts(codecContextId: number): string[] {
  if (typeof codecContextId !== 'number') {
    throw new TypeError('Expected codec context ID to be a number');
  }
  return addon.getSupportedPixFmts(codecContextId);
}

/**
 * get supported sample formats for encoder
 * 
 * @param codecContextId - encoder context ID
 * @returns array of sample format names
 * 
 * @example
 * ```typescript
 * import { createEncoder, getSupportedSampleFmts } from 'ffmpeg7';
 * 
 * const encoder = createEncoder('aac');
 * const formats = getSupportedSampleFmts(encoder);
 * console.log(formats); // ['fltp', 's16', ...]
 * ```
 * 
 * @throws {TypeError} if context ID is not a number
 * @throws {Error} if context is invalid
 */
export function getSupportedSampleFmts(codecContextId: number): string[] {
  if (typeof codecContextId !== 'number') {
    throw new TypeError('Expected codec context ID to be a number');
  }
  return addon.getSupportedSampleFmts(codecContextId);
}

/**
 * get supported sample rates for encoder
 * 
 * @param codecContextId - encoder context ID
 * @returns array of supported sample rates
 * 
 * @example
 * ```typescript
 * import { createEncoder, getSupportedSampleRates } from 'ffmpeg7';
 * 
 * const encoder = createEncoder('aac');
 * const rates = getSupportedSampleRates(encoder);
 * console.log(rates); // [96000, 88200, 64000, 48000, 44100, ...]
 * ```
 * 
 * @throws {TypeError} if context ID is not a number
 * @throws {Error} if context is invalid
 */
export function getSupportedSampleRates(codecContextId: number): number[] {
  if (typeof codecContextId !== 'number') {
    throw new TypeError('Expected codec context ID to be a number');
  }
  return addon.getSupportedSampleRates(codecContextId);
}

// ────────────────────────────────────────────────────────────────────────────
// 11. AudioFIFO API - Professional audio buffer management
// ────────────────────────────────────────────────────────────────────────────

/**
 * Create an AudioFIFO buffer for audio rebuffering
 * 
 * @param sampleFormat - Sample format (e.g., 8 for AV_SAMPLE_FMT_FLTP)
 * @param channels - Number of audio channels
 * @param nbSamples - Initial buffer size in samples (can grow dynamically)
 * @returns fifoId - AudioFIFO handle ID
 * 
 * @example
 * ```typescript
 * import { audioFifoAlloc, audioFifoWrite, audioFifoRead, audioFifoFree } from 'ffmpeg7';
 * 
 * // Create FIFO for FLTP format, 2 channels, initial size 1024 samples
 * const fifoId = audioFifoAlloc(8, 2, 1024);
 * 
 * // Use the FIFO...
 * 
 * audioFifoFree(fifoId);
 * ```
 * 
 * @throws {TypeError} if parameters are not numbers
 * @throws {Error} if FIFO allocation fails
 */
export function audioFifoAlloc(sampleFormat: number, channels: number, nbSamples: number): number {
  if (typeof sampleFormat !== 'number' || typeof channels !== 'number' || typeof nbSamples !== 'number') {
    throw new TypeError('Expected all parameters to be numbers');
  }
  return addon.audioFifoAlloc(sampleFormat, channels, nbSamples);
}

/**
 * Free an AudioFIFO buffer
 * 
 * @param fifoId - AudioFIFO handle ID
 * 
 * @example
 * ```typescript
 * import { audioFifoAlloc, audioFifoFree } from 'ffmpeg7';
 * 
 * const fifoId = audioFifoAlloc(8, 2, 1024);
 * // ... use the FIFO
 * audioFifoFree(fifoId);
 * ```
 * 
 * @throws {TypeError} if fifoId is not a number
 * @throws {Error} if FIFO ID is invalid
 */
export function audioFifoFree(fifoId: number): void {
  if (typeof fifoId !== 'number') {
    throw new TypeError('Expected FIFO ID to be a number');
  }
  addon.audioFifoFree(fifoId);
}

/**
 * Write audio data from a frame to the AudioFIFO
 * 
 * @param fifoId - AudioFIFO handle ID
 * @param frameId - Frame ID containing audio data
 * @returns number of samples written
 * 
 * @example
 * ```typescript
 * import { audioFifoAlloc, audioFifoWrite, receiveFrame } from 'ffmpeg7';
 * 
 * const fifoId = audioFifoAlloc(8, 2, 1024);
 * const frameId = receiveFrame(decoderId);
 * 
 * if (frameId >= 0) {
 *   const samplesWritten = audioFifoWrite(fifoId, frameId);
 *   console.log(`Wrote ${samplesWritten} samples to FIFO`);
 * }
 * ```
 * 
 * @throws {TypeError} if parameters are not numbers
 * @throws {Error} if FIFO or frame ID is invalid, or write fails
 */
export function audioFifoWrite(fifoId: number, frameId: number): number {
  if (typeof fifoId !== 'number' || typeof frameId !== 'number') {
    throw new TypeError('Expected FIFO ID and frame ID to be numbers');
  }
  return addon.audioFifoWrite(fifoId, frameId);
}

/**
 * Read audio data from AudioFIFO into a frame
 * 
 * @param fifoId - AudioFIFO handle ID
 * @param frameId - Frame ID to read into (must have buffer allocated)
 * @param nbSamples - Number of samples to read
 * @returns number of samples read
 * 
 * @example
 * ```typescript
 * import { audioFifoAlloc, audioFifoRead, audioFifoSize, allocFrame, frameGetBuffer } from 'ffmpeg7';
 * 
 * const fifoId = audioFifoAlloc(8, 2, 1024);
 * // ... write data to FIFO
 * 
 * // Read 1024 samples
 * const frameId = allocFrame();
 * setFrameProperty(frameId, 'format', 8);
 * setFrameProperty(frameId, 'channels', 2);
 * setFrameProperty(frameId, 'nb_samples', 1024);
 * frameGetBuffer(frameId, 0);
 * 
 * const samplesRead = audioFifoRead(fifoId, frameId, 1024);
 * console.log(`Read ${samplesRead} samples from FIFO`);
 * ```
 * 
 * @throws {TypeError} if parameters are not numbers
 * @throws {Error} if FIFO or frame ID is invalid, or read fails
 */
export function audioFifoRead(fifoId: number, frameId: number, nbSamples: number): number {
  if (typeof fifoId !== 'number' || typeof frameId !== 'number' || typeof nbSamples !== 'number') {
    throw new TypeError('Expected all parameters to be numbers');
  }
  return addon.audioFifoRead(fifoId, frameId, nbSamples);
}

/**
 * Get the number of samples currently in the AudioFIFO
 * 
 * @param fifoId - AudioFIFO handle ID
 * @returns number of samples in the FIFO
 * 
 * @example
 * ```typescript
 * import { audioFifoAlloc, audioFifoSize } from 'ffmpeg7';
 * 
 * const fifoId = audioFifoAlloc(8, 2, 1024);
 * // ... write data to FIFO
 * 
 * const size = audioFifoSize(fifoId);
 * console.log(`FIFO contains ${size} samples`);
 * ```
 * 
 * @throws {TypeError} if fifoId is not a number
 * @throws {Error} if FIFO ID is invalid
 */
export function audioFifoSize(fifoId: number): number {
  if (typeof fifoId !== 'number') {
    throw new TypeError('Expected FIFO ID to be a number');
  }
  return addon.audioFifoSize(fifoId);
}

/**
 * Get the available space in the AudioFIFO
 * 
 * @param fifoId - AudioFIFO handle ID
 * @returns number of samples that can be written without reallocation
 * 
 * @example
 * ```typescript
 * import { audioFifoAlloc, audioFifoSpace } from 'ffmpeg7';
 * 
 * const fifoId = audioFifoAlloc(8, 2, 1024);
 * const space = audioFifoSpace(fifoId);
 * console.log(`FIFO has space for ${space} samples`);
 * ```
 * 
 * @throws {TypeError} if fifoId is not a number
 * @throws {Error} if FIFO ID is invalid
 */
export function audioFifoSpace(fifoId: number): number {
  if (typeof fifoId !== 'number') {
    throw new TypeError('Expected FIFO ID to be a number');
  }
  return addon.audioFifoSpace(fifoId);
}

/**
 * Reset/drain the AudioFIFO (remove all samples)
 * 
 * @param fifoId - AudioFIFO handle ID
 * 
 * @example
 * ```typescript
 * import { audioFifoAlloc, audioFifoReset } from 'ffmpeg7';
 * 
 * const fifoId = audioFifoAlloc(8, 2, 1024);
 * // ... use the FIFO
 * audioFifoReset(fifoId); // Clear all data
 * ```
 * 
 * @throws {TypeError} if fifoId is not a number
 * @throws {Error} if FIFO ID is invalid
 */
export function audioFifoReset(fifoId: number): void {
  if (typeof fifoId !== 'number') {
    throw new TypeError('Expected FIFO ID to be a number');
  }
  addon.audioFifoReset(fifoId);
}

/**
 * Drain samples from the AudioFIFO (discard without reading)
 * 
 * @param fifoId - AudioFIFO handle ID
 * @param nbSamples - Number of samples to drain
 * 
 * @example
 * ```typescript
 * import { audioFifoAlloc, audioFifoDrain } from 'ffmpeg7';
 * 
 * const fifoId = audioFifoAlloc(8, 2, 1024);
 * // ... write data to FIFO
 * audioFifoDrain(fifoId, 512); // Discard 512 samples
 * ```
 * 
 * @throws {TypeError} if parameters are not numbers
 * @throws {Error} if FIFO ID is invalid
 */
export function audioFifoDrain(fifoId: number, nbSamples: number): void {
  if (typeof fifoId !== 'number' || typeof nbSamples !== 'number') {
    throw new TypeError('Expected FIFO ID and nbSamples to be numbers');
  }
  addon.audioFifoDrain(fifoId, nbSamples);
}

