# Mid-Level API Guide

## Table of Contents

- [Overview](#overview)
- [Getting Started](#getting-started)
- [API Categories](#api-categories)
- [Complete API Reference](#complete-api-reference)
  - [1. Input and Output Management](#1-input-and-output-management)
  - [2. Codec Management](#2-codec-management)
  - [3. Transcoding Operations](#3-transcoding-operations)
  - [4. Decoder Management](#4-decoder-management)
  - [5. Frame and Packet Processing](#5-frame-and-packet-processing)
  - [6. Frame Data Access](#6-frame-data-access)
  - [7. Packet Data Access](#7-packet-data-access)
  - [8. Video Scaling (SwsContext)](#8-video-scaling-swscontext)
  - [9. Audio Resampling (SwrContext)](#9-audio-resampling-swrcontext)
  - [10. Auxiliary Functions](#10-auxiliary-functions)
  - [11. AudioFIFO API](#11-audiofifo-api)
- [Best Practices](#best-practices)
- [Troubleshooting](#troubleshooting)


## Overview

The **Mid-Level API** provides fine-grained control over FFmpeg operations in Node.js. Unlike the high-level `run()` function which wraps the FFmpeg CLI, the mid-level API exposes individual FFmpeg components, allowing you to:

- **Custom Processing Pipelines**: Build custom encoding/decoding workflows
- **Frame-Level Access**: Read and manipulate raw video/audio frame data
- **Precise Control**: Control every aspect of encoding parameters, timestamps, and metadata
- **Memory Efficient**: Zero-copy operations where possible
- **Production Ready**: Based on FFmpeg 7.1.2 with all modern APIs


### Architecture

The mid-level API uses a **handle-based context management** system:

- All FFmpeg objects (contexts, frames, packets) are identified by numeric handles
- Handles are managed in C layer and passed to JavaScript
- Automatic resource tracking prevents memory leaks
- Multiple contexts can coexist independently


## Getting Started

### Installation

```bash
npm install ffmpeg7
# or
pnpm install ffmpeg7
```

### Basic Import

```javascript
const { MidLevel } = require('ffmpeg7');

// Or import specific functions
const { 
  openInput, 
  createOutput, 
  createEncoder,
  closeContext 
} = require('ffmpeg7').MidLevel;
```

### TypeScript Support

```typescript
import { MidLevel } from 'ffmpeg7';
import type { StreamInfo } from 'ffmpeg7';

const inputCtx = MidLevel.openInput('video.mp4');
const streams: StreamInfo[] = MidLevel.getInputStreams(inputCtx);
```

## API Categories

The mid-level API is organized into 11 functional categories:

| Category | Description | Key Functions |
|----------|-------------|---------------|
| **Input/Output** | File operations | `openInput`, `createOutput`, `writeHeader` |
| **Codec Management** | Encoder/decoder setup | `createEncoder`, `setEncoderOption`, `openEncoder` |
| **Transcoding** | Stream operations | `copyStreamParams`, `readPacket`, `writePacket` |
| **Decoder** | Decoding setup | `createDecoder`, `copyDecoderParams`, `openDecoder` |
| **Frame/Packet** | Encode/decode flow | `sendPacket`, `receiveFrame`, `sendFrame`, `receivePacket` |
| **Frame Data** | Frame manipulation | `getFrameData`, `setFrameData`, `setFrameProperty` |
| **Packet Data** | Packet manipulation | `getPacketData`, `setPacketProperty` |
| **Video Scaling** | Resolution/format conversion | `createSwsContext`, `swsScale` |
| **Audio Resampling** | Audio conversion | `createSwrContext`, `swrConvertFrame` |
| **Auxiliary** | Utility functions | `seekInput`, `getMetadata`, `getSupportedPixFmts` |
| **AudioFIFO** | Audio buffer management | `audioFifoAlloc`, `audioFifoWrite`, `audioFifoRead` |


## Complete API Reference

### 1. Input and Output Management

#### `openInput(filePath: string): number`

Open an input file and return a context handle ID.

```typescript
const inputCtx = openInput('video.mp4');
```

**Parameters:**
- `filePath` (string): Path to the input file

**Returns:**
- `number`: Context ID for subsequent operations

**Throws:**
- `TypeError`: If file path is not a string
- `Error`: If file cannot be opened or parsed


#### `createOutput(filePath: string, format?: string): number`

Create an output file context.

```typescript
const outputCtx = createOutput('output.mp4', 'mp4');
```

**Parameters:**
- `filePath` (string): Output file path
- `format` (string, optional): Output format (e.g., "mp4", "mkv")

**Returns:**
- `number`: Context ID

**Throws:**
- `TypeError`: If parameter types are incorrect
- `Error`: If context creation fails


#### `getInputStreams(contextId: number): StreamInfo[]`

Get information about all streams in the input file.

```typescript
const streams = getInputStreams(inputCtx);

streams.forEach(stream => {
  console.log(`Stream ${stream.index}: ${stream.type} - ${stream.codec}`);
  if (stream.type === 'video') {
    console.log(`  Resolution: ${stream.width}x${stream.height}`);
    console.log(`  FPS: ${stream.fps}`);
    console.log(`  avg_frame_rate: ${stream.avg_frame_rate}`);
    console.log(`  r_frame_rate: ${stream.r_frame_rate}`);
  }
});
```

**Parameters:**
- `contextId` (number): Input context ID

**Returns:**
- `StreamInfo[]`: Array of stream information objects

**StreamInfo Interface:**
```typescript
interface Rational {
  num: number;
  den: number;
}

interface StreamInfo {
  index: number;
  type: 'video' | 'audio' | 'subtitle' | 'data' | 'unknown';
  codec: string;
  width?: number;        // Video only
  height?: number;       // Video only
  fps?: number;          // Video only
  avgFrameRate?: Rational;
  avg_frame_rate?: string;
  rFrameRate?: Rational;
  r_frame_rate?: string;
  pixelFormat?: string;  // Video only
  sampleRate?: number;   // Audio only
  channels?: number;     // Audio only
}
```


#### `addOutputStream(contextId: number, codecName: string): number`

Add a stream to the output context.

```typescript
const videoStreamIdx = addOutputStream(outputCtx, 'libx264');
const audioStreamIdx = addOutputStream(outputCtx, 'aac');
```

**Parameters:**
- `contextId` (number): Output context ID
- `codecName` (string): Codec name (e.g., "libx264", "aac")

**Returns:**
- `number`: New stream index


#### `closeContext(contextId: number): void`

Close and release context resources. Works for all context types:
- Input/output contexts
- Encoder/decoder contexts
- SwsContext (video scaling)
- SwrContext (audio resampling)

```typescript
closeContext(inputCtx);
closeContext(encoder);
closeContext(swsCtx);
```


### 2. Codec Management

#### `createEncoder(codecName: string): number`

Create an encoder context.

```typescript
const encoder = createEncoder('libx264');
```

**Parameters:**
- `codecName` (string): Encoder name

**Common Encoders:**
- **Video**: `libx264`, `libx265`, `libvpx`, `libvpx-vp9`
- **Audio**: `aac`, `libmp3lame`, `libopus`

**Returns:**
- `number`: Encoder context ID


#### `setEncoderOption(codecContextId: number, key: string, value: number | string): void`

Set encoder options before opening the encoder.

```typescript
// Video encoder settings
setEncoderOption(encoder, 'width', 1920);
setEncoderOption(encoder, 'height', 1080);
setEncoderOption(encoder, 'pix_fmt', 0);      // YUV420P
setEncoderOption(encoder, 'time_base_num', 1);
setEncoderOption(encoder, 'time_base_den', 30);
setEncoderOption(encoder, 'bit_rate', 2000000);

// H.264 specific options
setEncoderOption(encoder, 'preset', 'medium');
setEncoderOption(encoder, 'crf', '23');
setEncoderOption(encoder, 'profile', 'high');

// Audio encoder settings
setEncoderOption(audioEncoder, 'sample_rate', 44100);
setEncoderOption(audioEncoder, 'channels', 2);
setEncoderOption(audioEncoder, 'sample_fmt', 8); // AV_SAMPLE_FMT_FLTP
setEncoderOption(audioEncoder, 'bit_rate', 128000);
```

**Common Options:**

| Category | Option | Type | Description |
|----------|--------|------|-------------|
| **Video** | `width` | number | Frame width |
| | `height` | number | Frame height |
| | `pix_fmt` | number | Pixel format (0=YUV420P) |
| | `bit_rate` | number | Target bitrate (bps) |
| | `time_base_num` | number | Time base numerator |
| | `time_base_den` | number | Time base denominator |
| **H.264** | `preset` | string | Encoding speed preset |
| | `crf` | string | Constant rate factor (0-51) |
| | `profile` | string | H.264 profile |
| **Audio** | `sample_rate` | number | Sample rate (Hz) |
| | `channels` | number | Channel count |
| | `sample_fmt` | number | Sample format |


#### `openEncoder(codecContextId: number): void`

Open the encoder after setting all options.

```typescript
setEncoderOption(encoder, 'width', 1920);
setEncoderOption(encoder, 'height', 1080);
openEncoder(encoder); // Must call after setting options
```


#### `getSupportedPixFmts(codecContextId: number): string[]`

Get supported pixel formats for the encoder.

```typescript
const formats = getSupportedPixFmts(encoder);
console.log(formats); // ['yuv420p', 'yuvj420p', 'yuv422p', ...]
```


#### `getSupportedSampleFmts(codecContextId: number): string[]`

Get supported sample formats for audio encoder.

```typescript
const formats = getSupportedSampleFmts(audioEncoder);
console.log(formats); // ['fltp', 's16', ...]
```


#### `getSupportedSampleRates(codecContextId: number): number[]`

Get supported sample rates for audio encoder.

```typescript
const rates = getSupportedSampleRates(audioEncoder);
console.log(rates); // [96000, 88200, 64000, 48000, 44100, ...]
```


### 3. Transcoding Operations

#### `setOutputOption(contextId: number, key: string, value: string): void`

Set output format options (call before `writeHeader`).

```typescript
// Enable faststart for streaming-optimized MP4
setOutputOption(outputCtx, 'movflags', '+faststart');

// Other common options
setOutputOption(outputCtx, 'movflags', '+frag_keyframe+empty_moov');
setOutputOption(outputCtx, 'brand', 'mp42');
```

**Common Options:**

| Key | Value | Description |
|-----|-------|-------------|
| `movflags` | `+faststart` | Move MOOV atom to file beginning (streaming) |
| `movflags` | `+frag_keyframe` | Fragment at keyframes |
| `brand` | `mp42` | MP4 brand identifier |


#### `writeHeader(contextId: number): void`

Write the output file header. Must call after adding all streams and before writing packets.

```typescript
writeHeader(outputCtx);
```


#### `writeTrailer(contextId: number): void`

Write the output file trailer. Must call after writing all packets to finalize the file.

```typescript
writeTrailer(outputCtx);
```


#### `copyStreamParams(inputContextId: number, outputContextId: number, inputStreamIndex: number, outputStreamIndex: number): void`

Copy stream parameters from input to output (used in remuxing).

```typescript
copyStreamParams(inputCtx, outputCtx, 0, 0);
```


#### `copyEncoderToStream(encoderContextId: number, outputContextId: number, outputStreamIndex: number): void`

Copy encoder parameters to output stream.

```typescript
copyEncoderToStream(encoder, outputCtx, streamIdx);
```


#### `readPacket(contextId: number): PacketInfo | null`

Read a packet from input. Returns `null` at end of file.

```typescript
const packet = readPacket(inputCtx);
if (packet) {
  console.log(`Read packet from stream ${packet.streamIndex}`);
  console.log(`PTS: ${packet.pts}, DTS: ${packet.dts}`);
  freePacket(packet.id);
}
```

**Returns:**
```typescript
interface PacketInfo {
  id: number;         // Packet handle ID
  streamIndex: number;
  pts: number;        // Presentation timestamp
  dts: number;        // Decode timestamp
  duration: number;
}
```


#### `writePacket(outputContextId: number, packetId: number, outputStreamIndex: number, inputContextId?: number, inputStreamIndex?: number): void`

Write a packet to output. Optional input context parameters enable automatic timestamp rescaling.

```typescript
// Simple write (encoder output)
writePacket(outputCtx, packetId, 0);

// Write with timestamp rescaling (remuxing)
writePacket(outputCtx, packetId, 0, inputCtx, inputStreamIdx);
```


#### `freePacket(packetId: number): void`

Free packet resources. Always call when done with a packet.

```typescript
freePacket(packet.id);
```


### 4. Decoder Management

#### `createDecoder(codecName: string): number`

Create a decoder context.

```typescript
const decoder = createDecoder('h264');
```

**Common Decoders:**
- **Video**: `h264`, `hevc`, `vp8`, `vp9`, `mpeg4`
- **Audio**: `aac`, `mp3`, `opus`, `vorbis`


#### `copyDecoderParams(inputContextId: number, decoderContextId: number, streamIndex: number): void`

Copy codec parameters from input stream to decoder.

```typescript
copyDecoderParams(inputCtx, decoder, 0);
```


#### `openDecoder(codecContextId: number): void`

Open the decoder after copying parameters.

```typescript
openDecoder(decoder);
```


### 5. Frame and Packet Processing

This section covers the core encode/decode data flow.

#### Decode Flow: Packet → Frame

```
readPacket() → sendPacket() → receiveFrame()
```

#### Encode Flow: Frame → Packet

```
sendFrame() → receivePacket() → writePacket()
```


#### `allocFrame(): number`

Allocate a new frame.

```typescript
const frame = allocFrame();
```


#### `freeFrame(frameId: number): void`

Free frame resources.

```typescript
freeFrame(frame);
```


#### `allocPacket(): number`

Allocate a new packet.

```typescript
const pkt = allocPacket();
```


#### `sendPacket(decoderContextId: number, packetId: number | null): number`

Send a packet to the decoder. Pass `null` to flush the decoder.

**Return Values:**
- `0`: Success
- `-1`: EAGAIN (need to read more frames first)
- `-2`: EOF
- `-3`: Error

```typescript
const ret = sendPacket(decoder, packetId);
if (ret === 0) {
  // Packet accepted
}

// Flush decoder at end
sendPacket(decoder, null);
```


#### `receiveFrame(decoderContextId: number, frameId: number): number`

Receive a decoded frame.

**Return Values:** Same as `sendPacket`

```typescript
while (true) {
  const ret = receiveFrame(decoder, frame);
  if (ret !== 0) break;
  // Process frame...
}
```


#### `sendFrame(encoderContextId: number, frameId: number | null): number`

Send a frame to the encoder. Pass `null` to flush the encoder.

**Return Values:** Same as `sendPacket`

```typescript
sendFrame(encoder, frame);

// Flush encoder at end
sendFrame(encoder, null);
```


#### `receivePacket(encoderContextId: number, packetId: number): number`

Receive an encoded packet.

**Return Values:** Same as `sendPacket`

```typescript
while (true) {
  const ret = receivePacket(encoder, pkt);
  if (ret !== 0) break;
  writePacket(outputCtx, pkt, streamIdx);
}
```


### 6. Frame Data Access

#### `frameGetBuffer(frameId: number, align?: number): void`

Allocate frame buffer. Must call after setting width/height/format.

```typescript
setFrameProperty(frame, 'width', 1920);
setFrameProperty(frame, 'height', 1080);
setFrameProperty(frame, 'format', 0); // YUV420P
frameGetBuffer(frame, 32); // 32-byte alignment
```


#### `setFrameProperty(frameId: number, property: string, value: number): void`

Set frame property.

**Supported Properties:**

| Property | Type | Description |
|----------|------|-------------|
| `pts` | number | Presentation timestamp |
| `width` | number | Frame width (video) |
| `height` | number | Frame height (video) |
| `format` | number | Pixel/sample format |
| `pict_type` | number | Picture type (0=None, 1=I, 2=P, 3=B) |
| `key_frame` | number | Key frame flag (0/1) |
| `sample_rate` | number | Audio sample rate |
| `nb_samples` | number | Audio sample count |
| `channels` | number | Audio channel count |

```typescript
setFrameProperty(frame, 'pts', 1000);
setFrameProperty(frame, 'key_frame', 1);
```


#### `getFrameProperty(frameId: number, property: string): number | number[]`

Get frame property. Returns array for `linesize` property.

```typescript
const width = getFrameProperty(frame, 'width');
const linesize = getFrameProperty(frame, 'linesize'); // [y, u, v]
```


#### `getFrameData(frameId: number, planeIndex: number): Buffer | null`

Get frame plane data as a Node.js Buffer.

```typescript
// Video YUV420P
const yPlane = getFrameData(frame, 0); // Y plane
const uPlane = getFrameData(frame, 1); // U plane
const vPlane = getFrameData(frame, 2); // V plane

// Audio (planar or interleaved)
const audioData = getFrameData(audioFrame, 0);
```

**Returns:**
- `Buffer`: Frame data (zero-copy)
- `null`: For unused planes


#### `setFrameData(frameId: number, planeIndex: number, buffer: Buffer): void`

Set frame plane data from Buffer.

```typescript
const yData = Buffer.alloc(width * height);
// ... fill data ...
setFrameData(frame, 0, yData);
```


### 7. Packet Data Access

#### `getPacketData(packetId: number): Buffer | null`

Get packet data as Buffer.

```typescript
const data = getPacketData(packetId);
console.log(`Packet size: ${data?.length} bytes`);
```


#### `setPacketData(packetId: number, buffer: Buffer): void`

Set packet data from Buffer.

```typescript
const customData = Buffer.from([0x00, 0x01, 0x02, 0x03]);
setPacketData(packetId, customData);
```


#### `getPacketProperty(packetId: number, property: string): number`

Get packet property.

**Supported Properties:** `pts`, `dts`, `duration`, `streamIndex`, `flags`, `size`

```typescript
const pts = getPacketProperty(packetId, 'pts');
const size = getPacketProperty(packetId, 'size');
```


#### `setPacketProperty(packetId: number, property: string, value: number): void`

Set packet property.

**Supported Properties:** `pts`, `dts`, `duration`, `streamIndex`, `flags`

```typescript
setPacketProperty(packetId, 'pts', 12345);
setPacketProperty(packetId, 'flags', 1); // Key frame
```


### 8. Video Scaling (SwsContext)

#### `createSwsContext(srcWidth: number, srcHeight: number, srcFormat: string | number, dstWidth: number, dstHeight: number, dstFormat: string | number, flags?: number): number`

Create a software scaler context for video scaling and format conversion.

```typescript
const swsCtx = createSwsContext(
  1920, 1080, 'yuv420p',  // Source
  1280, 720, 'yuv420p',   // Destination
  2  // SWS_BILINEAR
);
```

**Common Formats:**
- `yuv420p`, `yuv422p`, `yuv444p`
- `rgb24`, `bgr24`, `rgba`, `bgra`
- `nv12`, `nv21`

**Scaling Flags:**
- `0`: SWS_FAST_BILINEAR (default, fastest)
- `1`: SWS_BILINEAR (good quality)
- `2`: SWS_BICUBIC (better quality)
- `4`: SWS_LANCZOS (best quality)


#### `swsScale(swsContextId: number, srcFrameId: number, dstFrameId: number): void`

Scale/convert a frame. Destination frame must have buffer allocated.

```typescript
// Prepare destination frame
const dstFrame = allocFrame();
setFrameProperty(dstFrame, 'width', 1280);
setFrameProperty(dstFrame, 'height', 720);
setFrameProperty(dstFrame, 'format', 0); // YUV420P
frameGetBuffer(dstFrame, 32);

// Perform scaling
swsScale(swsCtx, srcFrame, dstFrame);
```


### 9. Audio Resampling (SwrContext)

#### `createSwrContext(srcSampleRate: number, srcChannels: number, srcFormat: string | number, dstSampleRate: number, dstChannels: number, dstFormat: string | number): number`

Create an audio resampler context.

```typescript
const swrCtx = createSwrContext(
  48000, 2, 's16',   // Source: 48kHz, stereo, 16-bit
  44100, 2, 'fltp'   // Destination: 44.1kHz, stereo, float planar
);
```

**Common Formats:**
- `s16`, `s32`: Signed integer
- `flt`, `dbl`: Float/double
- `s16p`, `fltp`: Planar variants


#### `swrConvertFrame(swrContextId: number, srcFrameId: number | null, dstFrameId: number): number`

Resample audio frame. Pass `null` as source to flush.

**Returns:**
- Number of samples output per channel

```typescript
const samplesOut = swrConvertFrame(swrCtx, srcFrame, dstFrame);
console.log(`Resampled ${samplesOut} samples`);

// Flush remaining samples
swrConvertFrame(swrCtx, null, dstFrame);
```


### 10. Auxiliary Functions

#### `seekInput(inputContextId: number, timestamp: number, streamIndex?: number, flags?: number): void`

Seek to a specific timestamp in the input file.

```typescript
// Seek to 5 seconds (assuming AV_TIME_BASE = 1000000)
seekInput(inputCtx, 5 * 1000000, 0);
```

**Flags:**
- `0`: AVSEEK_FLAG_BACKWARD (seek to keyframe before)
- `1`: AVSEEK_FLAG_BYTE (seek by byte position)
- `2`: AVSEEK_FLAG_ANY (seek to any frame)
- `4`: AVSEEK_FLAG_FRAME (seek by frame number)


#### `getMetadata(inputContextId: number, key?: string): string | Record<string, string> | null`

Get metadata from input.

```typescript
// Get all metadata
const allMeta = getMetadata(inputCtx);
// { title: '...', artist: '...', ... }

// Get specific key
const title = getMetadata(inputCtx, 'title');
```


#### `setMetadata(outputContextId: number, key: string, value: string): void`

Set output metadata.

```typescript
setMetadata(outputCtx, 'title', 'My Video');
setMetadata(outputCtx, 'artist', 'John Doe');
setMetadata(outputCtx, 'comment', 'Created with FFmpeg');
```


#### `copyMetadata(inputContextId: number, outputContextId: number): void`

Copy all metadata from input to output.

```typescript
copyMetadata(inputCtx, outputCtx);
```


#### `getEncoderList(type?: 'video' | 'audio'): string[]`

Get list of available encoders.

```typescript
// Get all encoders
const allEncoders = getEncoderList();

// Get video encoders only
const videoEncoders = getEncoderList('video');
// ['libx264', 'libx265', 'libvpx', ...]

// Get audio encoders only
const audioEncoders = getEncoderList('audio');
// ['aac', 'libmp3lame', 'libopus', ...]
```


#### `getMuxerList(): string[]`

Get list of available output formats (muxers).

```typescript
const formats = getMuxerList();
// ['mp4', 'mkv', 'avi', 'webm', ...]
```


### 11. AudioFIFO API

Professional audio buffer management for handling irregular audio frame sizes.

#### Overview

AudioFIFO provides a ring buffer for audio rebuffering:
- **Input**: Variable-size decoded audio frames
- **Output**: Fixed-size frames for encoding
- **Zero-copy**: Direct frame-to-frame data transfer in C layer
- **Automatic growth**: Buffer expands dynamically as needed

#### Use Cases

- **Audio transcoding**: Decoder produces variable frames, encoder requires fixed size
- **Format conversion**: Bridge between different frame size requirements
- **Audio synchronization**: Buffer management for complex workflows


#### `audioFifoAlloc(sampleFormat: number, channels: number, nbSamples: number): number`

Create an AudioFIFO buffer.

```typescript
// Create FIFO for FLTP format (float planar), 2 channels, initial size 1024 samples
const fifoId = audioFifoAlloc(8, 2, 1024);
```

**Parameters:**
- `sampleFormat` (number): Sample format enum (e.g., 8 for AV_SAMPLE_FMT_FLTP)
- `channels` (number): Number of audio channels
- `nbSamples` (number): Initial buffer size in samples (can grow)

**Common Sample Formats:**
- `0`: AV_SAMPLE_FMT_U8
- `1`: AV_SAMPLE_FMT_S16
- `8`: AV_SAMPLE_FMT_FLTP (float planar)


#### `audioFifoFree(fifoId: number): void`

Free an AudioFIFO buffer.

```typescript
audioFifoFree(fifoId);
```


#### `audioFifoWrite(fifoId: number, frameId: number): number`

Write audio data from frame to FIFO.

```typescript
const samplesWritten = audioFifoWrite(fifoId, decodedFrameId);
console.log(`Wrote ${samplesWritten} samples to FIFO`);
```

**Returns:**
- Number of samples written


#### `audioFifoRead(fifoId: number, frameId: number, nbSamples: number): number`

Read audio data from FIFO into frame.

```typescript
// Frame must have buffer allocated
const frameId = allocFrame();
setFrameProperty(frameId, 'format', 8);      // FLTP
setFrameProperty(frameId, 'channels', 2);
setFrameProperty(frameId, 'nb_samples', 1024);
frameGetBuffer(frameId, 0);

const samplesRead = audioFifoRead(fifoId, frameId, 1024);
console.log(`Read ${samplesRead} samples from FIFO`);
```

**Returns:**
- Number of samples read


#### `audioFifoSize(fifoId: number): number`

Get number of samples currently in the FIFO.

```typescript
const available = audioFifoSize(fifoId);
if (available >= 1024) {
  // Enough samples to read a frame
}
```


#### `audioFifoSpace(fifoId: number): number`

Get available space in the FIFO (samples that can be written without reallocation).

```typescript
const space = audioFifoSpace(fifoId);
console.log(`FIFO has space for ${space} samples`);
```


#### `audioFifoReset(fifoId: number): void`

Clear all data from the FIFO.

```typescript
audioFifoReset(fifoId);
```


#### `audioFifoDrain(fifoId: number, nbSamples: number): void`

Discard a specific number of samples from the FIFO.

```typescript
audioFifoDrain(fifoId, 512); // Discard 512 samples
```


#### AudioFIFO Complete Example

```typescript
// Create FIFO
const fifoId = audioFifoAlloc(8, 2, 1024); // FLTP, stereo

// Write decoded frames (variable size)
while (receiveFrame(decoder, frame) === 0) {
  audioFifoWrite(fifoId, frame);
}

// Read fixed-size frames for encoding
while (audioFifoSize(fifoId) >= 1024) {
  const encFrame = allocFrame();
  setFrameProperty(encFrame, 'format', 8);
  setFrameProperty(encFrame, 'channels', 2);
  setFrameProperty(encFrame, 'nb_samples', 1024);
  frameGetBuffer(encFrame, 0);
  
  audioFifoRead(fifoId, encFrame, 1024);
  
  sendFrame(encoder, encFrame);
  // ... receive and write packets ...
  
  freeFrame(encFrame);
}

// Cleanup
audioFifoFree(fifoId);
```

## Best Practices

### 1. Resource Management

**Always pair allocations with deallocations:**

```typescript
// ✓ Good
const frame = allocFrame();
try {
  // ... use frame ...
} finally {
  freeFrame(frame);
}

// ✗ Bad: Memory leak
const frame = allocFrame();
// ... forgot to call freeFrame()
```

**Close contexts in reverse order:**

```typescript
// ✓ Good: Last created, first closed
const inputCtx = openInput('input.mp4');
const decoder = createDecoder('h264');
const encoder = createEncoder('libx264');
const outputCtx = createOutput('output.mp4');

// ... process ...

closeContext(outputCtx);
closeContext(encoder);
closeContext(decoder);
closeContext(inputCtx);
```


### 2. Error Handling

**Check return values:**

```typescript
// ✓ Good
const ret = sendPacket(decoder, packetId);
if (ret === -3) {
  console.error('Decoding error occurred');
  // Handle error...
}

// ✗ Bad: Ignoring return value
sendPacket(decoder, packetId);
```

**Use try-catch for exceptions:**

```typescript
try {
  const ctx = openInput('video.mp4');
  // ... process ...
} catch (err) {
  console.error('Failed to open input:', err.message);
}
```


### 3. Frame and Packet Reuse

**Reuse frames/packets in loops:**

```typescript
// ✓ Good: Reuse same frame
const frame = allocFrame();
while (receiveFrame(decoder, frame) === 0) {
  // Process frame...
  // Frame is automatically overwritten on next receiveFrame()
}
freeFrame(frame);

// ✗ Bad: Creating new frame each iteration
while (true) {
  const frame = allocFrame(); // Memory leak!
  if (receiveFrame(decoder, frame) !== 0) break;
  // ... forgot to free ...
}
```


### 4. Buffer Alignment

**Use 32-byte alignment for performance:**

```typescript
// ✓ Good: Aligned buffer for SIMD operations
frameGetBuffer(frame, 32);

// ✓ Acceptable: Default alignment
frameGetBuffer(frame, 0);

// ✗ Avoid: Odd alignments
frameGetBuffer(frame, 7);
```

### 5. Timestamp Management

**Let encoder handle PTS automatically:**

```typescript
// ✓ Good: Encoder auto-increments PTS
let pts = 0;
while (/* ... */) {
  setFrameProperty(frame, 'pts', pts++);
  sendFrame(encoder, frame);
}
```

**Use writePacket's rescaling for remuxing:**

```typescript
// ✓ Good: Automatic timestamp rescaling
writePacket(outputCtx, packetId, outputStreamIdx, inputCtx, inputStreamIdx);

// ✗ Manual rescaling (error-prone)
const pts = getPacketProperty(packetId, 'pts');
const rescaledPts = /* ... manual calculation ... */;
setPacketProperty(packetId, 'pts', rescaledPts);
writePacket(outputCtx, packetId, outputStreamIdx);
```

### 6. Encoder Flushing

**Always flush encoders at end:**

```typescript
// ✓ Good: Proper flushing
// Send all frames...
sendFrame(encoder, null); // Flush signal
while (receivePacket(encoder, packet) === 0) {
  writePacket(outputCtx, packet, streamIdx);
}

// ✗ Bad: Missing flush (output file may be corrupted)
// Send all frames...
writeTrailer(outputCtx); // Some frames may be lost!
```


### 7. AudioFIFO Management

**Check FIFO size before reading:**

```typescript
// ✓ Good
while (audioFifoSize(fifoId) >= 1024) {
  audioFifoRead(fifoId, frame, 1024);
  // ... encode ...
}

// ✗ Bad: Reading without checking size
audioFifoRead(fifoId, frame, 1024); // May fail if insufficient data
```


### 8. Format Compatibility

**Check supported formats before encoding:**

```typescript
// ✓ Good
const encoder = createEncoder('libx264');
const supportedFormats = getSupportedPixFmts(encoder);
console.log('Supported formats:', supportedFormats);

if (!supportedFormats.includes('yuv420p')) {
  console.error('yuv420p not supported, using alternative...');
}
```


## Troubleshooting

### Common Issues and Solutions

| Issue | Cause | Solution |
|-------|-------|----------|
| **"Invalid context"** | Context already closed or invalid ID | Check ID validity, avoid double-closing |
| **"Buffer not allocated"** | Forgot to call `frameGetBuffer` | Set width/height/format, then call `frameGetBuffer` |
| **Return value -1 (EAGAIN)** | Encoder/decoder needs more input/output | Normal flow control, continue sending/receiving |
| **Return value -2 (EOF)** | Stream ended | Normal termination signal |
| **Timestamp errors** | Incorrect time base | Use automatic rescaling in `writePacket` |
| **Crash on frame access** | Accessing frame after freeing | Keep track of frame lifecycle |
| **Memory leak** | Not freeing allocated resources | Use try-finally, pair alloc/free calls |
| **Corrupted output** | Forgot to flush encoder | Send `null` frame to flush before `writeTrailer` |
| **Audio glitches** | Frame size mismatch | Use AudioFIFO for rebuffering |


### Debugging Tips

**1. Enable FFmpeg logging:**

```typescript
const { HighLevel } = require('ffmpeg7');

HighLevel.addLogListener((level, message) => {
  console.log(`[FFmpeg ${level}] ${message}`);
});
```

**2. Check stream information:**

```typescript
const streams = getInputStreams(inputCtx);
console.log('Input streams:', JSON.stringify(streams, null, 2));
```

**3. Validate frame properties:**

```typescript
console.log('Frame width:', getFrameProperty(frame, 'width'));
console.log('Frame height:', getFrameProperty(frame, 'height'));
console.log('Frame format:', getFrameProperty(frame, 'format'));
console.log('Frame PTS:', getFrameProperty(frame, 'pts'));
```

**4. Monitor packet flow:**

```typescript
const packet = readPacket(inputCtx);
if (packet) {
  console.log(`Packet: stream=${packet.streamIndex}, pts=${packet.pts}, size=${getPacketProperty(packet.id, 'size')}`);
}
```

### Performance Optimization

**1. Reuse contexts:**
- Create encoder/decoder once, reuse for multiple files
- Reuse SwsContext/SwrContext for consistent transformations

**2. Buffer pooling:**
- Reuse frame/packet objects instead of allocating new ones

**3. Batch operations:**
- Process multiple packets before writing to reduce I/O overhead

**4. Choose appropriate presets:**
- `ultrafast`: Real-time encoding
- `medium`: Balanced (default)
- `slow`, `veryslow`: Maximum quality for archival

**5. Hardware acceleration:**
- Use hardware encoders when available (`h264_videotoolbox` on macOS)
- Check `getEncoderList()` for available options


## FFmpeg 7.1.2 Features

This binding uses FFmpeg 7.1.2 APIs, including:

✅ **Modern APIs:**
- `av_channel_layout_default()` - New audio channel layout API
- `sws_scale_frame()` - Frame-based scaling API
- `swr_convert_frame()` - Frame-based resampling API

✅ **Improved Error Handling:**
- Better error messages and return codes
- Consistent EAGAIN/EOF behavior

✅ **Enhanced Options:**
- `AVDictionary` for flexible option passing
- Support for codec-specific private options

## Additional Resources

### Example Files
- `example/360p-transcode-demo.js` - Complete 360p transcoding with AudioFIFO

### FFmpeg Documentation
- [FFmpeg Official Documentation](https://ffmpeg.org/documentation.html)
- [Codec Options](https://ffmpeg.org/ffmpeg-codecs.html)
- [Format Options](https://ffmpeg.org/ffmpeg-formats.html)
