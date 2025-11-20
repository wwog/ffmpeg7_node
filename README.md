# FFmpeg Node.js Addon

<div align="center">

[English](#) | [中文](README.zh.md)

</div>

---

## Description

A high-performance Node.js native addon for FFmpeg 7.1.2, providing direct access to FFmpeg functionality through a simple API. This module wraps FFmpeg CLI and allows you to use FFmpeg directly in Node.js.

> Building Options: --enable-pic --disable-doc --enable-debug --enable-runtime-cpudetect --disable-autodetect --target-os=darwin --enable-appkit --enable-avfoundation --enable-coreimage --enable-audiotoolbox --enable-videotoolbox --cc=cc --host_cc=cc --cxx=c++ --nm=nm --ar='ar' --ranlib=ranlib --strip=strip --enable-gpl --disable-ffmpeg --disable-ffplay --disable-ffprobe --enable-avcodec --enable-avdevice --enable-avformat --enable-avfilter --disable-postproc --enable-swresample --enable-swscale --disable-alsa --disable-amf --disable-libaom --disable-libass --disable-avisynth --disable-bzlib --disable-libdav1d --disable-libfdk-aac --disable-libfontconfig --disable-libharfbuzz --disable-libfreetype --disable-libfribidi --disable-iconv --disable-libilbc --disable-lzma --disable-libmp3lame --disable-libmodplug --disable-cuda --disable-nvenc --disable-nvdec  --disable-cuvid --disable-ffnvcodec --disable-opencl --disable-opengl --disable-libopenh264 --disable-libopenjpeg --disable-libopenmpt --disable-openssl --disable-libopus --disable-sdl2 --disable-libsnappy --disable-libsoxr --disable-libspeex --disable-libssh --disable-libtensorflow --disable-libtesseract --disable-libtheora --disable-libvorbis --enable-libvpx --disable-vulkan --disable-libwebp --enable-libx264 --enable-libx265 --disable-libxml2 --disable-zlib --disable-libsrt --disable-libmfx --disable-vaapi --enable-cross-compile --pkg-config="/opt/homebrew/bin/pkg-config" --pkg-config-flags=--static

## Features

- ✅ **FFmpeg 7.1.2** - Latest stable version
- ✅ **Native Performance** - Direct C/C++ integration, no process spawning overhead
- ✅ **Cross-Platform** - Supports macOS (ARM64/x64) and Windows (x64)
- ✅ **Prebuilt Binaries** - Pre-compiled static libraries included
- ✅ **Node Native Module** - Easy to integrate into toolchain builds, no user installation required, with rich feature support
- ✅ **Dual API Levels** - High-level CLI wrapper and fine-grained mid-level API

## Extended Platform Support

https://github.com/wwog/vcpkg_node_ffmpeg

## Installation

```bash
npm install ffmpeg7
# or
pnpm install ffmpeg7
# or
yarn add ffmpeg7
```

## Requirements

- Node.js >= 14.0.0
- Python 3.x (for building)
- Build tools:
  - macOS: Xcode Command Line Tools
  - Windows: Visual Studio Build Tools

## Quick Start

```javascript
const ffmpeg = require('ffmpeg7');

// Convert video
const exitCode = ffmpeg.run([
  '-i', 'input.mp4',
  '-c:v', 'libx264',
  '-c:a', 'aac',
  'output.mp4'
]);

if (exitCode === 0) {
  console.log('Conversion successful!');
} else {
  console.error('Conversion failed with exit code:', exitCode);
}
```

## API Documentation

This package provides two levels of API:

### 📘 High-Level API (CLI Wrapper)

Simple FFmpeg command-line interface for quick operations.

```javascript
const ffmpeg = require('ffmpeg7');

// Run FFmpeg with command-line arguments
ffmpeg.run(['-i', 'input.mp4', '-c:v', 'libx264', 'output.mp4']);
```

**Key Functions:**
- `run(args)` - Execute FFmpeg with CLI arguments
- `getVideoDuration(filePath)` - Get video duration
- `getVideoFormatInfo(filePath)` - Get detailed format information (includes audio details when present via `info.audio`)
- `addLogListener(callback)` - Listen to FFmpeg logs

### 📗 Mid-Level API (Fine-Grained Control)

Advanced API for custom encoding/decoding workflows with frame-level access.

```javascript
const { MidLevel } = require('ffmpeg7');

// Open input and get stream info
const inputCtx = MidLevel.openInput('video.mp4');
const streams = MidLevel.getInputStreams(inputCtx);

// Create encoder with custom settings
const encoder = MidLevel.createEncoder('libx264');
MidLevel.setEncoderOption(encoder, 'preset', 'fast');
MidLevel.setEncoderOption(encoder, 'crf', '23');
```

**Key Features:**
- 🎬 **Manual encode/decode control** - Full pipeline control from packets to frames
- 🖼️ **Frame-level data access** - Read/write raw video and audio data
- 🔄 **Video scaling** - SwsContext for resolution and format conversion
- 🎵 **Audio resampling** - SwrContext for audio format conversion
- 📦 **AudioFIFO** - Professional audio buffer management
- ⚙️ **Advanced options** - Faststart, metadata, custom codec parameters
- 🚀 **Zero-copy operations** - Direct Buffer access to media data

**📚 Complete Documentation:**

👉 **[Mid-Level API Guide](docs/MID_LEVEL_API.md)** - Comprehensive guide with examples

The guide includes:
- Complete API reference for all functions
- Real-world workflow examples (video transcoding, audio rebuffering, frame extraction)
- Best practices and performance optimization
- Troubleshooting guide

### Quick API Examples

#### High-Level API Example

```javascript
// Extract audio from video
ffmpeg.run(['-i', 'video.mp4', '-vn', '-acodec', 'copy', 'audio.aac']);

// Resize video
ffmpeg.run([
  '-i', 'input.mp4',
  '-vf', 'scale=1280:720',
  '-c:v', 'libx264',
  '-preset', 'medium',
  '-crf', '23',
  'output.mp4'
]);

// Convert format
ffmpeg.run(['-i', 'input.avi', '-c:v', 'libx264', '-c:a', 'aac', 'output.mp4']);
```

#### Mid-Level API Example

```javascript
const { MidLevel } = require('ffmpeg7');

// Transcode video with custom settings
const inputCtx = MidLevel.openInput('input.mp4');
const encoder = MidLevel.createEncoder('libx264');
MidLevel.setEncoderOption(encoder, 'width', 1280);
MidLevel.setEncoderOption(encoder, 'height', 720);
MidLevel.setEncoderOption(encoder, 'preset', 'medium');
MidLevel.openEncoder(encoder);

// ... encoding pipeline (see full examples in docs)
```

## Supported Platforms

| Platform | Architecture | Status |
|----------|-------------|--------|
| macOS    | ARM64       | ✅ Supported |
| macOS    | x64         | ✅ Supported |
| Windows  | x64         | ✅ Supported |
| Linux    | x64         | 🔄 Coming soon |

## Building from Source

If you need to build from source:

```bash
# Clone the repository
git clone https://github.com/wwog/ffmpeg7_node.git
cd ffmpeg-node

# Install dependencies
pnpm install

# Build the native module
pnpm run build
# or
node-gyp rebuild
```

## Project Structure

```
ffmpeg-node-7.1.2/
├── addon_src/          # Native addon source code
│   ├── binding.c      # N-API bindings
│   ├── ffmpeg.c       # FFmpeg integration
│   └── utils.c        # Utility functions
├── ffmpeg/            # FFmpeg source code (7.1.2)
├── prebuild/         # Pre-compiled static libraries
│   ├── mac-arm64/
│   ├── mac-x64/
│   └── win-x64/
└── binding.gyp       # Build configuration
```

## License

ISC

## Author

wwog

## Documentation

- 📚 [Mid-Level API Guide](docs/MID_LEVEL_API.md) - Complete guide for fine-grained FFmpeg control
- 📝 [Example: 360p Transcoding](example/360p-transcode-demo.js) - Full workflow demonstration

## Links

- [GitHub Repository](https://github.com/wwog/ffmpeg7_node)
- [FFmpeg Official Website](https://ffmpeg.org/)
- [Prebuild Production Project](https://github.com/wwog/vcpkg_node_ffmpeg)

---

<div align="center">

[English](#) | [中文](README.zh.md)

</div>
