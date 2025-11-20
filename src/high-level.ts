/**
 * @fileoverview high-level API - simple and easy to use FFmpeg operation interface
 * @module ffmpeg7/high-level
 * @description provide a simple and easy to use FFmpeg operation interface, suitable for rapid development
 */

import type { VideoFormatInfo, LogCallback } from './types';

const addon = require('./ffmpeg_node.node');


/**
 * Run FFmpeg with command-line arguments
 * 
 * @param args - Array of FFmpeg command-line arguments
 * @returns Exit code (0 for success)
 * 
 * @example
 * ```typescript
 * import { run } from 'ffmpeg7';
 * 
 * run([
 *   '-i', 'input.mp4',
 *   '-vcodec', 'libx264',
 *   '-preset', 'medium',
 *   '-crf', '23',
 *   '-y', 'output.mp4'
 * ]);
 * ```
 * 
 * @throws {TypeError} If arguments are invalid
 * @throws {Error} If FFmpeg execution fails
 */
export function run(args: string[]): number {
    if (!Array.isArray(args)) {
        throw new TypeError('Expected an array of arguments');
    }

    if (args.length === 0) {
        throw new Error('At least one argument is required');
    }

    // Validate all arguments are strings
    for (let i = 0; i < args.length; i++) {
        if (typeof args[i] !== 'string') {
            throw new TypeError(`Argument at index ${i} must be a string, got ${typeof args[i]}`);
        }
    }

    return addon.run(args);
}

/**
 * Get the duration of a video file in seconds.
 * 
 * @param filePath - Path to the video file
 * @returns Duration in seconds (double precision)
 * 
 * @example
 * ```typescript
 * import { getVideoDuration } from 'ffmpeg7';
 * 
 * const duration = getVideoDuration('video.mp4');
 * console.log(`Video duration: ${duration} seconds`);
 * ```
 * 
 * @throws {TypeError} If file path is not a string
 * @throws {Error} If the file cannot be opened or parsed
 */
export function getVideoDuration(filePath: string): number {
    if (typeof filePath !== 'string') {
        throw new TypeError('Expected file path to be a string');
    }

    return addon.getVideoDuration(filePath);
}

/**
 * Get detailed format information about a video file.
 * 
 * @param filePath - Path to the video file
 * @returns Object containing video format information
 * 
 * @example
 * ```typescript
 * import { getVideoFormatInfo } from 'ffmpeg7';
 * 
 * const info = getVideoFormatInfo('video.mp4');
 * console.log(`Format: ${info.format}`);
 * console.log(`Duration: ${info.duration}s`);
 * console.log(`Resolution: ${info.width}x${info.height}`);
 * console.log(`Video Codec: ${info.videoCodec}`);
 * console.log(`Audio Codec: ${info.audioCodec}`);
 * ```
 * 
 * @throws {TypeError} If file path is not a string
 * @throws {Error} If the file cannot be opened or parsed
 */
export function getVideoFormatInfo(filePath: string): VideoFormatInfo {
    if (typeof filePath !== 'string') {
        throw new TypeError('Expected file path to be a string');
    }

    return addon.getVideoFormatInfo(filePath);
}

/**
 * Add a log listener to receive FFmpeg log messages.
 * 
 * @param callback - Callback function that receives log level and message
 * 
 * @example
 * ```typescript
 * import { addLogListener, clearLogListener, LogLevel } from 'ffmpeg7';
 * 
 * addLogListener((level, message) => {
 *   if (level <= LogLevel.ERROR) {
 *     console.error('FFmpeg Error:', message);
 *   } else if (level <= LogLevel.WARNING) {
 *     console.warn('FFmpeg Warning:', message);
 *   } else {
 *     console.log('FFmpeg Info:', message);
 *   }
 * });
 * 
 * // ... run FFmpeg operations ...
 * 
 * clearLogListener();
 * ```
 * 
 * @throws {TypeError} If callback is not a function
 */
export function addLogListener(callback: LogCallback): void {
    if (typeof callback !== 'function') {
        throw new TypeError('Expected callback to be a function');
    }

    addon.addLogListener(callback);
}

/**
 * Clear the log listener and restore default FFmpeg logging behavior.
 * 
 * @example
 * ```typescript
 * import { clearLogListener } from 'ffmpeg7';
 * 
 * clearLogListener();
 * ```
 */
export function clearLogListener(): void {
    addon.clearLogListener();
}

