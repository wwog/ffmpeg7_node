# FFmpeg Node.js 原生插件

<div align="center">

[English](README.md) | [中文](#)

</div>

---

## 简介

一个高性能的 Node.js 原生插件，基于 FFmpeg 7.1.2，通过简单的 API 提供对 FFmpeg 功能的直接访问。该模块封装了 FFmpeg Cli，允许你在 Node.js 中直接使用 FFmpeg。

> 构建参数如下: -- Building Options: --enable-pic --disable-doc --enable-debug --enable-runtime-cpudetect --disable-autodetect --target-os=darwin --enable-appkit --enable-avfoundation --enable-coreimage --enable-audiotoolbox --enable-videotoolbox --cc=cc --host_cc=cc --cxx=c++ --nm=nm --ar='ar' --ranlib=ranlib --strip=strip --enable-gpl --disable-ffmpeg --disable-ffplay --disable-ffprobe --enable-avcodec --enable-avdevice --enable-avformat --enable-avfilter --disable-postproc --enable-swresample --enable-swscale --disable-alsa --disable-amf --disable-libaom --disable-libass --disable-avisynth --disable-bzlib --disable-libdav1d --disable-libfdk-aac --disable-libfontconfig --disable-libharfbuzz --disable-libfreetype --disable-libfribidi --disable-iconv --disable-libilbc --disable-lzma --disable-libmp3lame --disable-libmodplug --disable-cuda --disable-nvenc --disable-nvdec  --disable-cuvid --disable-ffnvcodec --disable-opencl --disable-opengl --disable-libopenh264 --disable-libopenjpeg --disable-libopenmpt --disable-openssl --disable-libopus --disable-sdl2 --disable-libsnappy --disable-libsoxr --disable-libspeex --disable-libssh --disable-libtensorflow --disable-libtesseract --disable-libtheora --disable-libvorbis --enable-libvpx --disable-vulkan --disable-libwebp --enable-libx264 --enable-libx265 --disable-libxml2 --disable-zlib --disable-libsrt --disable-libmfx --disable-vaapi --enable-cross-compile --pkg-config="/opt/homebrew/bin/pkg-config" --pkg-config-flags=--static

## 特性

- ✅ **FFmpeg 7.1.2** - 最新稳定版本
- ✅ **原生性能** - 直接 C/C++ 集成，无进程启动开销
- ✅ **跨平台** - 支持 macOS (ARM64/x64) 和 Windows (x64)
- ✅ **预编译二进制** - 包含预编译的静态库
- ✅ **Node原生模块** - 轻松参与工具链构建，不需要用户安装，较为丰富的功能特性支持
- ✅ **双层 API** - 高级 CLI 封装和细粒度的中级 API

## 扩展支持平台

https://github.com/wwog/vcpkg_node_ffmpeg

## 安装

```bash
npm install ffmpeg7
# 或
pnpm install ffmpeg7
# 或
yarn add ffmpeg7
```

## 系统要求

- Node.js >= 14.0.0
- Python 3.x (用于构建)
- 构建工具：
  - macOS: Xcode Command Line Tools
  - Windows: Visual Studio Build Tools

## 快速开始

```javascript
const ffmpeg = require('ffmpeg7');

// 转换视频
const exitCode = ffmpeg.run([
  '-i', 'input.mp4',
  '-c:v', 'libx264',
  '-c:a', 'aac',
  'output.mp4'
]);

if (exitCode === 0) {
  console.log('转换成功！');
} else {
  console.error('转换失败，退出码：', exitCode);
}
```

## API 文档

本包提供两个层次的 API：

### 📘 高级 API（CLI 封装）

简单的 FFmpeg 命令行接口，适合快速操作。

```javascript
const ffmpeg = require('ffmpeg7');

// 使用命令行参数运行 FFmpeg
ffmpeg.run(['-i', 'input.mp4', '-c:v', 'libx264', 'output.mp4']);
```

**主要函数：**
- `run(args)` - 使用 CLI 参数执行 FFmpeg
- `getVideoDuration(filePath)` - 获取视频时长
- `getVideoFormatInfo(filePath)` - 获取详细格式信息
- `addLogListener(callback)` - 监听 FFmpeg 日志

### 📗 中级 API（细粒度控制）

用于自定义编解码工作流的高级 API，提供帧级别访问。

```javascript
const { MidLevel } = require('ffmpeg7');

// 打开输入并获取流信息
const inputCtx = MidLevel.openInput('video.mp4');
const streams = MidLevel.getInputStreams(inputCtx);

// 创建自定义设置的编码器
const encoder = MidLevel.createEncoder('libx264');
MidLevel.setEncoderOption(encoder, 'preset', 'fast');
MidLevel.setEncoderOption(encoder, 'crf', '23');
```

**核心功能：**
- 🎬 **手动编解码控制** - 从数据包到帧的完整流程控制
- 🖼️ **帧级数据访问** - 读写原始视频和音频数据
- 🔄 **视频缩放** - SwsContext 进行分辨率和格式转换
- 🎵 **音频重采样** - SwrContext 进行音频格式转换
- 📦 **AudioFIFO** - 专业的音频缓冲管理
- ⚙️ **高级选项** - Faststart、元数据、自定义编解码器参数
- 🚀 **零拷贝操作** - 直接访问媒体数据的 Buffer

**📚 完整文档：**

👉 **[中级 API 指南](docs/MID_LEVEL_API.md)**（英文）- 包含示例的综合指南

该指南包括：
- 所有函数的完整 API 参考
- 实际工作流示例（视频转码、音频重缓冲、帧提取）
- 最佳实践和性能优化
- 故障排除指南

### API 快速示例

#### 高级 API 示例

```javascript
// 从视频中提取音频
ffmpeg.run(['-i', 'video.mp4', '-vn', '-acodec', 'copy', 'audio.aac']);

// 调整视频尺寸
ffmpeg.run([
  '-i', 'input.mp4',
  '-vf', 'scale=1280:720',
  '-c:v', 'libx264',
  '-preset', 'medium',
  '-crf', '23',
  'output.mp4'
]);

// 转换格式
ffmpeg.run(['-i', 'input.avi', '-c:v', 'libx264', '-c:a', 'aac', 'output.mp4']);
```

#### 中级 API 示例

```javascript
const { MidLevel } = require('ffmpeg7');

// 使用自定义设置转码视频
const inputCtx = MidLevel.openInput('input.mp4');
const encoder = MidLevel.createEncoder('libx264');
MidLevel.setEncoderOption(encoder, 'width', 1280);
MidLevel.setEncoderOption(encoder, 'height', 720);
MidLevel.setEncoderOption(encoder, 'preset', 'medium');
MidLevel.openEncoder(encoder);

// ... 编码流程（详见文档中的完整示例）
```

## 支持的平台

| 平台    | 架构   | 状态           |
|---------|--------|----------------|
| macOS   | ARM64  | ✅ 支持        |
| macOS   | x64    | ✅ 支持        |
| Windows | x64    | ✅ 支持        |
| Linux   | x64    | 🔄 即将支持    |

## 从源码构建

如果需要从源码构建：

```bash
# 克隆仓库
git clone https://github.com/wwog/ffmpeg7_node.git
cd ffmpeg-node

# 安装依赖
pnpm install

# 构建原生模块
pnpm run build
# 或
node-gyp rebuild
```

## 项目结构

```
ffmpeg-node-7.1.2/
├── addon_src/          # 原生插件源代码
│   ├── binding.c      # N-API 绑定
│   ├── ffmpeg.c       # FFmpeg 集成
│   └── utils.c        # 工具函数
├── ffmpeg/            # FFmpeg 源代码 (7.1.2)
├── prebuild/         # 预编译的静态库
│   ├── mac-arm64/
│   ├── mac-x64/
│   └── win-x64/
└── binding.gyp       # 构建配置
```

## 许可证

ISC

## 作者

wwog

## 文档

- 📚 [中级 API 指南](docs/MID_LEVEL_API.md)（英文）- 细粒度 FFmpeg 控制的完整指南
- 📝 [示例：360p 转码](example/360p-transcode-demo.js) - 完整工作流演示

## 相关链接

- [GitHub 仓库](https://github.com/wwog/ffmpeg7_node)
- [FFmpeg 官方网站](https://ffmpeg.org/)
- [预构建生产项目](https://github.com/wwog/vcpkg_node_ffmpeg)

---

<div align="center">

[English](README.md) | [中文](#)

</div>

