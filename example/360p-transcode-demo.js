/**
 * 360p视频转码示例 - 演示完整的手动编解码流程
 * 
 * 功能：
 * 1. 视频缩放到最大边360p（保持宽高比）
 * 2. MOOV原子前置（faststart，优化流媒体播放）
 * 3. 码率调整（视频和音频）
 * 4. 完全手动的解码->处理->编码流程
 * 5. 音频帧缓冲重组（处理AAC编码器的固定帧大小要求）
 */

const path = require('path');
const { MidLevel } = require('../dist/index.js');

const {
  // 上下文管理
  openInput,
  createOutput,
  getInputStreams,
  addOutputStream,
  closeContext,

  // 编解码器
  createEncoder,
  createDecoder,
  setEncoderOption,
  openEncoder,
  copyDecoderParams,
  openDecoder,

  // 输出选项
  setOutputOption,
  writeHeader,
  writeTrailer,
  copyEncoderToStream,

  // 帧和包操作
  allocFrame,
  allocPacket,
  freeFrame,
  freePacket,
  readPacket,
  writePacket,
  sendPacket,
  receiveFrame,
  sendFrame,
  receivePacket,

  // 帧数据操作
  frameGetBuffer,
  getFrameProperty,
  setFrameProperty,
  getFrameData,
  setFrameData,

  // 视频缩放
  createSwsContext,
  swsScale,

  // 包属性
  getPacketProperty,

  // AudioFIFO API - 专业音频缓冲管理
  audioFifoAlloc,
  audioFifoFree,
  audioFifoWrite,
  audioFifoRead,
  audioFifoSize,
  audioFifoSpace,
  audioFifoReset,
} = MidLevel;

/**
 * 音频帧缓冲器 - 使用FFmpeg AudioFIFO实现专业级音频缓冲重组
 * 用于满足AAC编码器的固定帧大小要求（1024采样/帧）
 * 
 * ✨ 新版本特性：
 * - 使用原生FFmpeg AudioFIFO API（高性能C实现）
 * - 自动处理动态大小扩展
 * - 零拷贝数据传输（Frame -> FIFO -> Frame）
 * - 完美处理不规则采样数的音频帧
 */
class AudioFrameBuffer {
  constructor(targetSamples, channels, sampleFormat) {
    this.targetSamples = targetSamples; // 目标采样数 (AAC = 1024)
    this.channels = channels;
    this.sampleFormat = sampleFormat; // 8 = AV_SAMPLE_FMT_FLTP
    
    // 创建AudioFIFO（初始容量为目标采样数的2倍）
    this.fifoId = audioFifoAlloc(sampleFormat, channels, targetSamples * 2);
    
    // 保存第一帧的格式信息
    this.firstFrameFormat = null;
    
    console.log(`✓ AudioFIFO已创建 (format=${sampleFormat}, channels=${channels}, capacity=${targetSamples * 2})`);
  }

  /**
   * 添加音频帧到FIFO缓冲区
   * @param {number} frameId - 解码后的音频帧ID
   */
  addFrame(frameId) {
    // 第一次添加帧时，保存格式信息
    if (!this.firstFrameFormat) {
      this.firstFrameFormat = {
        format: getFrameProperty(frameId, 'format'),
        sampleRate: getFrameProperty(frameId, 'sample_rate'),
      };
    }
    
    // 直接写入FIFO（零拷贝）
    const samplesWritten = audioFifoWrite(this.fifoId, frameId);
    
    return samplesWritten;
  }

  /**
   * 检查FIFO是否有足够的采样输出完整帧
   */
  hasEnoughSamples() {
    return audioFifoSize(this.fifoId) >= this.targetSamples;
  }

  /**
   * 从FIFO提取一个完整的目标大小帧
   * @param {number} outputFrameId - 预分配的输出帧ID
   * @returns {boolean} 是否成功提取
   */
  extractFrame(outputFrameId) {
    if (!this.hasEnoughSamples() || !this.firstFrameFormat) {
      return false;
    }

    // 设置输出帧的属性（必须在frameGetBuffer之前）
    setFrameProperty(outputFrameId, 'format', this.firstFrameFormat.format);
    setFrameProperty(outputFrameId, 'sample_rate', this.firstFrameFormat.sampleRate);
    setFrameProperty(outputFrameId, 'channels', this.channels); // ✨ 使用新的channels属性
    setFrameProperty(outputFrameId, 'nb_samples', this.targetSamples);
    
    // 分配帧缓冲区（现在channels已设置，不会失败）
    frameGetBuffer(outputFrameId, 0);

    // 从FIFO读取数据到输出帧（零拷贝）
    const samplesRead = audioFifoRead(this.fifoId, outputFrameId, this.targetSamples);
    
    return samplesRead === this.targetSamples;
  }

  /**
   * 获取FIFO中当前的采样数
   */
  get totalSamples() {
    return audioFifoSize(this.fifoId);
  }

  /**
   * 清空FIFO缓冲区
   */
  clear() {
    audioFifoReset(this.fifoId);
  }

  /**
   * 释放FIFO资源
   */
  destroy() {
    if (this.fifoId !== null) {
      audioFifoFree(this.fifoId);
      this.fifoId = null;
      console.log('✓ AudioFIFO已释放');
    }
  }
}

/**
 * 计算缩放后的尺寸（最大边360p，保持宽高比）
 */
function calculate360pSize(width, height) {
  const maxDim = 360;

  if (width > height) {
    // 横向视频
    if (width <= maxDim) {
      return { width, height };
    }
    const scale = maxDim / width;
    return {
      width: maxDim,
      height: Math.round(height * scale / 2) * 2, // 确保是偶数
    };
  } else {
    // 纵向或正方形视频
    if (height <= maxDim) {
      return { width, height };
    }
    const scale = maxDim / height;
    return {
      width: Math.round(width * scale / 2) * 2, // 确保是偶数
      height: maxDim,
    };
  }
}

/**
 * 转码视频到360p
 */
async function transcodeToG360p(inputPath, outputPath) {
  console.log('开始转码:', inputPath, '->', outputPath);

  // 1. 打开输入文件
  const inputCtx = openInput(inputPath);
  const streams = getInputStreams(inputCtx);

  console.log('输入流信息:');
  streams.forEach((stream, idx) => {
    console.log(`  流 #${idx}: ${stream.type}`, stream);
  });

  // 2. 找到视频和音频流
  const videoStreamIdx = streams.findIndex(s => s.type === 'video');
  const audioStreamIdx = streams.findIndex(s => s.type === 'audio');

  if (videoStreamIdx === -1) {
    throw new Error('未找到视频流');
  }

  const videoStream = streams[videoStreamIdx];
  const audioStream = audioStreamIdx !== -1 ? streams[audioStreamIdx] : null;

  // 3. 计算目标分辨率
  const { width: targetWidth, height: targetHeight } = calculate360pSize(
    videoStream.width,
    videoStream.height
  );

  console.log(`\n缩放: ${videoStream.width}x${videoStream.height} -> ${targetWidth}x${targetHeight}`);

  // 4. 创建解码器
  const videoDecoder = createDecoder(videoStream.codec);
  copyDecoderParams(inputCtx, videoDecoder, videoStreamIdx);
  openDecoder(videoDecoder);

  let audioDecoder = null;
  if (audioStream) {
    audioDecoder = createDecoder(audioStream.codec);
    copyDecoderParams(inputCtx, audioDecoder, audioStreamIdx);
    openDecoder(audioDecoder);
  }

  // 5. 创建输出文件
  const outputCtx = createOutput(outputPath, 'mp4');

  // **关键：设置faststart选项（MOOV原子前置）**
  setOutputOption(outputCtx, 'movflags', '+faststart');
  console.log('✓ 已启用faststart（MOOV前置）');

  // 6. 创建视频编码器（H.264）
  const videoEncoder = createEncoder('libx264');

  // 设置视频编码参数
  setEncoderOption(videoEncoder, 'width', targetWidth);
  setEncoderOption(videoEncoder, 'height', targetHeight);
  setEncoderOption(videoEncoder, 'pix_fmt', 'yuv420p'); // YUV420P
  setEncoderOption(videoEncoder, 'time_base_num', 1);
  setEncoderOption(videoEncoder, 'time_base_den', 30);
  setEncoderOption(videoEncoder, 'framerate_num', 30);
  setEncoderOption(videoEncoder, 'framerate_den', 1);

  // **码率控制**
  setEncoderOption(videoEncoder, 'bit_rate', 800000); // 800 kbps
  setEncoderOption(videoEncoder, 'gop_size', 60); // 2秒GOP (30fps)
  setEncoderOption(videoEncoder, 'max_b_frames', 2);

  // H.264特定选项
  setEncoderOption(videoEncoder, 'preset', 'medium'); // 编码速度/质量平衡
  setEncoderOption(videoEncoder, 'crf', '23'); // 质量控制 (18-28范围)

  openEncoder(videoEncoder);
  console.log(`✓ 视频编码器: H.264, ${targetWidth}x${targetHeight}, 800kbps, CRF=23`);

  // 7. 添加视频流
  const outputVideoStreamIdx = addOutputStream(outputCtx, 'libx264');
  copyEncoderToStream(videoEncoder, outputCtx, outputVideoStreamIdx);

  // 8. 创建音频编码器（如果有音频流）
  let audioEncoder = null;
  let outputAudioStreamIdx = -1;

  if (audioStream) {
    audioEncoder = createEncoder('aac');

    setEncoderOption(audioEncoder, 'sample_rate', audioStream.sampleRate || 44100);
    setEncoderOption(audioEncoder, 'channels', audioStream.channels || 1);
    setEncoderOption(audioEncoder, 'sample_fmt', 'fltp'); // AAC编码器支持fltp
    setEncoderOption(audioEncoder, 'bit_rate', 128000); // 128 kbps音频码率
    setEncoderOption(audioEncoder, 'time_base_num', 1);
    setEncoderOption(audioEncoder, 'time_base_den', audioStream.sampleRate || 44100);

    openEncoder(audioEncoder);
    console.log(`✓ 音频编码器: AAC, ${audioStream.sampleRate}Hz, ${audioStream.channels}ch, 128kbps`);

    outputAudioStreamIdx = addOutputStream(outputCtx, 'aac');
    copyEncoderToStream(audioEncoder, outputCtx, outputAudioStreamIdx);
  }

  // 9. 创建缩放上下文
  const swsCtx = createSwsContext(
    videoStream.width,
    videoStream.height,
    videoStream.pixelFormat || 'yuv420p',
    targetWidth,
    targetHeight,
    'yuv420p'
  );
  console.log('✓ 创建视频缩放上下文');

  // 10. 写入文件头
  writeHeader(outputCtx);
  console.log('✓ 写入文件头（包含faststart选项）\n');

  // 11. 分配工作帧和包
  const decodedVideoFrame = allocFrame();
  const scaledVideoFrame = allocFrame();
  const encodedVideoPacket = allocPacket();

  let decodedAudioFrame = null;
  let bufferedAudioFrame = null; // 用于存放缓冲区重组后的音频帧
  let encodedAudioPacket = null;
  let audioBuffer = null; // 音频帧缓冲器

  if (audioStream) {
    decodedAudioFrame = allocFrame();
    bufferedAudioFrame = allocFrame();
    encodedAudioPacket = allocPacket();
    
    // ✨ 创建AudioFIFO音频帧缓冲器 (AAC需要1024采样/帧)
    // format=8 对应 AV_SAMPLE_FMT_FLTP (planar float32)
    audioBuffer = new AudioFrameBuffer(1024, audioStream.channels, 8);
  }

  // 设置缩放后帧的属性
  setFrameProperty(scaledVideoFrame, 'width', targetWidth);
  setFrameProperty(scaledVideoFrame, 'height', targetHeight);
  setFrameProperty(scaledVideoFrame, 'format', 0); // YUV420P (AV_PIX_FMT_YUV420P = 0)
  frameGetBuffer(scaledVideoFrame, 32); // 32字节对齐

  // 12. 主转码循环
  let videoFrameCount = 0;
  let audioFrameCount = 0;
  let packetCount = 0;

  console.log('开始转码...');

  while (true) {
    // 读取输入包
    const packet = readPacket(inputCtx);
    if (!packet) {
      console.log('输入文件读取完毕，刷新编码器...');

      // 刷新视频编码器
      sendFrame(videoEncoder, null);
      while (true) {
        const ret = receivePacket(videoEncoder, encodedVideoPacket);
        if (ret !== 0) break;
        writePacket(outputCtx, encodedVideoPacket, outputVideoStreamIdx);
      }

      // 刷新音频编码器
      if (audioEncoder) {
        // 先处理缓冲器中剩余的音频数据
        while (audioBuffer.hasEnoughSamples()) {
          if (audioBuffer.extractFrame(bufferedAudioFrame)) {
            sendFrame(audioEncoder, bufferedAudioFrame);
            while (true) {
              const encRet = receivePacket(audioEncoder, encodedAudioPacket);
              if (encRet !== 0) break;
              writePacket(outputCtx, encodedAudioPacket, outputAudioStreamIdx);
              audioFrameCount++;
            }
          }
        }
        
        // 刷新音频编码器缓冲区
        sendFrame(audioEncoder, null);
        while (true) {
          const ret = receivePacket(audioEncoder, encodedAudioPacket);
          if (ret !== 0) break;
          writePacket(outputCtx, encodedAudioPacket, outputAudioStreamIdx);
        }
        
        console.log(`音频缓冲器剩余: ${audioBuffer.totalSamples} 采样未处理`);
      }

      break;
    }

    packetCount++;
    const streamIdx = getPacketProperty(packet.id, 'streamIndex');

    if (streamIdx === videoStreamIdx) {
      // 处理视频包
      sendPacket(videoDecoder, packet.id);

      while (true) {
        const ret = receiveFrame(videoDecoder, decodedVideoFrame);
        if (ret !== 0) break;

        // 缩放帧
        swsScale(swsCtx, decodedVideoFrame, scaledVideoFrame);

        // 编码缩放后的帧
        sendFrame(videoEncoder, scaledVideoFrame);

        // 接收编码包
        while (true) {
          const encRet = receivePacket(videoEncoder, encodedVideoPacket);
          if (encRet !== 0) break;

          writePacket(outputCtx, encodedVideoPacket, outputVideoStreamIdx);
          videoFrameCount++;
        }
      }

    } else if (audioStream && streamIdx === audioStreamIdx) {
      // 处理音频包
      sendPacket(audioDecoder, packet.id);

      while (true) {
        const ret = receiveFrame(audioDecoder, decodedAudioFrame);
        if (ret !== 0) break;

        // 获取音频帧的采样数
        const nbSamples = getFrameProperty(decodedAudioFrame, 'nb_samples');
        
        // ✨ 使用AudioFIFO缓冲重组
        // 将解码后的音频帧添加到FIFO缓冲区
        audioBuffer.addFrame(decodedAudioFrame);
        
        // 尝试从缓冲区提取完整的1024采样帧并编码
        while (audioBuffer.hasEnoughSamples()) {
          if (audioBuffer.extractFrame(bufferedAudioFrame)) {
            // 编码缓冲后的音频帧
            sendFrame(audioEncoder, bufferedAudioFrame);

            while (true) {
              const encRet = receivePacket(audioEncoder, encodedAudioPacket);
              if (encRet !== 0) break;

              writePacket(outputCtx, encodedAudioPacket, outputAudioStreamIdx);
              audioFrameCount++;
            }
          }
        }
      }
    }

    freePacket(packet.id);

    // 进度显示
    if (packetCount % 100 === 0) {
      process.stdout.write(`\r处理包: ${packetCount}, 视频帧: ${videoFrameCount}, 音频帧: ${audioFrameCount}`);
    }
  }

  console.log(`\n\n转码完成:`);
  console.log(`  - 处理包数: ${packetCount}`);
  console.log(`  - 视频帧数: ${videoFrameCount}`);
  console.log(`  - 音频帧数: ${audioFrameCount}`);

  // 13. 写入文件尾并清理
  writeTrailer(outputCtx);

  freeFrame(decodedVideoFrame);
  freeFrame(scaledVideoFrame);
  freePacket(encodedVideoPacket);

  if (audioStream) {
    freeFrame(decodedAudioFrame);
    freeFrame(bufferedAudioFrame);
    freePacket(encodedAudioPacket);
    audioBuffer.destroy(); // ✨ 释放AudioFIFO资源
  }

  closeContext(videoDecoder);
  closeContext(videoEncoder);
  closeContext(swsCtx);

  if (audioDecoder) closeContext(audioDecoder);
  if (audioEncoder) closeContext(audioEncoder);

  closeContext(inputCtx);
  closeContext(outputCtx);

  console.log('\n✓ 所有资源已清理');
  console.log(`✓ 输出文件: ${outputPath}`);
  console.log('✓ MOOV原子已前置（faststart），可直接流式播放');
  console.log('\n📊 新特性使用说明：');
  console.log('  ✨ AudioFIFO: 使用FFmpeg原生API进行音频缓冲重组');
  console.log('  ✨ Channels属性: 支持手动设置音频帧声道数');
  console.log('  ✨ 零拷贝: Frame数据直接在C层传输，无JS开销');
}

// 运行示例
const inputFile = path.join(__dirname, 'input.mp4');
const outputFile = path.join(__dirname, 'output/360p-output.mp4');

transcodeToG360p(inputFile, outputFile)
  .then(() => {
    console.log('\n成功！');
    process.exit(0);
  })
  .catch((error) => {
    console.error('\n错误:', error);
    process.exit(1);
  });

