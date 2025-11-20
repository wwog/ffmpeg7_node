/**
 * @fileoverview FFmpeg Node.js Native Addon 
 * @module ffmpeg7
 * @description A high-performance Node.js native addon for FFmpeg 7.1.2
 */

// ============================================================================
// export types
// ============================================================================
export * from './types';

// ============================================================================
// default export (includes all APIs)
// ============================================================================
import * as HighLevel from './high-level';
import * as MidLevel from './mid-level';


export const run = HighLevel.run;
export { HighLevel, MidLevel };
