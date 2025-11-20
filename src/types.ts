/**
 * @fileoverview types for ffmpeg7
 * @module ffmpeg7/types
 */

/**
 * Video format information interface
 */
export interface VideoFormatInfo {
  /** Format name (e.g., "mov,mp4,m4a,3gp,3g2,mj2") */
  format?: string;
  /** Duration in seconds */
  duration?: number;
  /** Bitrate in bits per second */
  bitrate?: number;
  /** Video codec name (e.g., "h264") */
  videoCodec?: string;
  /** Audio codec name (e.g., "aac") */
  audioCodec?: string;
  /** Video width in pixels */
  width?: number;
  /** Video height in pixels */
  height?: number;
  /** Frames per second */
  fps?: number;
  /** Audio sample rate in Hz */
  sampleRate?: number;
  /** Number of audio channels */
  channels?: number;
  /** Metadata dictionary */
  metadata?: Record<string, string>;
}

/**
 * Stream information interface (中层 API)
 */
export interface Rational {
  /** numerator */
  num: number;
  /** denominator */
  den: number;
}

export interface StreamInfo {
  /** Stream index */
  index: number;
  /** Media type: "video", "audio", "subtitle", etc. */
  type: string;
  /** Codec name (e.g., "h264", "aac") */
  codec?: string;
  /** Video width (video streams only) */
  width?: number;
  /** Video height (video streams only) */
  height?: number;
  /** Frames per second (video streams only) */
  fps?: number;
  /** Average frame rate in rational form (video streams only) */
  avgFrameRate?: Rational;
  /** Average frame rate string, e.g. "60000/1001" */
  avg_frame_rate?: string;
  /** Real base frame rate in rational form */
  rFrameRate?: Rational;
  /** Real base frame rate string, e.g. "60000/1001" */
  r_frame_rate?: string;
  /** Sample rate (audio streams only) */
  sampleRate?: number;
  /** Number of channels (audio streams only) */
  channels?: number;
  /** Bitrate */
  bitrate?: number;
}

/**
 * Log callback function type
 * @param level - FFmpeg log level (AV_LOG_*)
 * @param message - Log message
 */
export type LogCallback = (level: number, message: string) => void;

/**
 * FFmpeg log levels
 */
export const LogLevel = {
  /** Print no output */
  QUIET: -8,
  /** Something went really wrong */
  PANIC: 0,
  /** Something went wrong */
  FATAL: 8,
  /** Something went wrong and cannot be recovered */
  ERROR: 16,
  /** Something might not work as expected */
  WARNING: 24,
  /** Standard information */
  INFO: 32,
  /** Detailed information */
  VERBOSE: 40,
  /** Debug information */
  DEBUG: 48,
  /** Trace information */
  TRACE: 56,
} as const;

/**
 * Log level type
 */
export type LogLevelType = typeof LogLevel;

