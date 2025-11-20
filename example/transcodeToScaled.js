
const path = require('path');
const { MidLevel } = require('../dist/index.js');

const {
  openInput,
  createOutput,
  getInputStreams,
  addOutputStream,
  closeContext,

  createEncoder,
  createDecoder,
  setEncoderOption,
  openEncoder,
  copyDecoderParams,
  openDecoder,

  setOutputOption,
  writeHeader,
  writeTrailer,
  copyEncoderToStream,

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

  frameGetBuffer,
  getFrameProperty,
  setFrameProperty,
  getFrameData,
  setFrameData,

  createSwsContext,
  swsScale,

  getPacketProperty,

  audioFifoAlloc,
  audioFifoFree,
  audioFifoWrite,
  audioFifoRead,
  audioFifoSize,
  audioFifoSpace,
  audioFifoReset,
} = MidLevel;


class AudioFrameBuffer {
  constructor(targetSamples, channels, sampleFormat) {
    this.targetSamples = targetSamples; 
    this.channels = channels;
    this.sampleFormat = sampleFormat; 
    
    this.fifoId = audioFifoAlloc(sampleFormat, channels, targetSamples * 2);
    
    this.firstFrameFormat = null;
    
    console.log(`✓ AudioFIFO已创建 (format=${sampleFormat}, channels=${channels}, capacity=${targetSamples * 2})`);
  }

  
  addFrame(frameId) {
    if (!this.firstFrameFormat) {
      this.firstFrameFormat = {
        format: getFrameProperty(frameId, 'format'),
        sampleRate: getFrameProperty(frameId, 'sample_rate'),
      };
    }
    const samplesWritten = audioFifoWrite(this.fifoId, frameId);
    
    return samplesWritten;
  }


  hasEnoughSamples() {
    return audioFifoSize(this.fifoId) >= this.targetSamples;
  }

  extractFrame(outputFrameId) {
    if (!this.hasEnoughSamples() || !this.firstFrameFormat) {
      return false;
    }

    setFrameProperty(outputFrameId, 'format', this.firstFrameFormat.format);
    setFrameProperty(outputFrameId, 'sample_rate', this.firstFrameFormat.sampleRate);
    setFrameProperty(outputFrameId, 'channels', this.channels); 
    setFrameProperty(outputFrameId, 'nb_samples', this.targetSamples);
    
    frameGetBuffer(outputFrameId, 0);

    const samplesRead = audioFifoRead(this.fifoId, outputFrameId, this.targetSamples);
    
    return samplesRead === this.targetSamples;
  }


  get totalSamples() {
    return audioFifoSize(this.fifoId);
  }


  clear() {
    audioFifoReset(this.fifoId);
  }

  
  destroy() {
    if (this.fifoId !== null) {
      audioFifoFree(this.fifoId);
      this.fifoId = null;
      console.log('✓ AudioFIFO已释放');
    }
  }
}

class FrameRateLimiter {
  constructor(sourceFps, targetFps) {
    const safeSource = typeof sourceFps === 'number' && sourceFps > 0 ? sourceFps : targetFps;
    const safeTarget = typeof targetFps === 'number' && targetFps > 0 ? targetFps : safeSource;

    this.sourceFps = safeSource;
    this.targetFps = safeTarget;
    this.sourceFrameDuration = 1 / this.sourceFps;
    this.targetFrameDuration = 1 / this.targetFps;
    this.timeline = 0;
    this.nextEmitTime = 0;
    this.keptFrames = 0;
    this.droppedFrames = 0;
    this.active = this.sourceFps > this.targetFps + 1e-6;
  }

  shouldEmit() {
    if (!this.active) {
      this.keptFrames++;
      return true;
    }

    const shouldKeep = this.timeline + Number.EPSILON >= this.nextEmitTime;
    this.timeline += this.sourceFrameDuration;

    if (shouldKeep) {
      this.nextEmitTime += this.targetFrameDuration;
      this.keptFrames++;
      return true;
    }

    this.droppedFrames++;
    return false;
  }
}


function gcd(a, b) {
  let x = Math.abs(a);
  let y = Math.abs(b);
  while (y !== 0) {
    const temp = y;
    y = x % y;
    x = temp;
  }
  return x || 1;
}

function toRational(value, precision = 1000) {
  const denominator = precision;
  const numerator = Math.round(value * denominator);
  const divisor = gcd(numerator, denominator);
  const reducedNum = numerator / divisor;
  const reducedDen = denominator / divisor;
  return {
    num: reducedNum,
    den: reducedDen,
    value: reducedNum / reducedDen,
  };
}

function parseFrameRateCandidate(candidate) {
  if (!candidate) {
    return null;
  }

  if (typeof candidate === 'number' && isFinite(candidate) && candidate > 0) {
    return toRational(candidate);
  }

  if (typeof candidate === 'string') {
    const trimmed = candidate.trim();
    if (!trimmed) return null;
    if (trimmed.includes('/')) {
      const [numStr, denStr] = trimmed.split('/');
      const num = Number(numStr);
      const den = Number(denStr);
      if (num > 0 && den > 0) {
        const divisor = gcd(num, den);
        const reducedNum = num / divisor;
        const reducedDen = den / divisor;
        return {
          num: reducedNum,
          den: reducedDen,
          value: reducedNum / reducedDen,
        };
      }
    } else {
      const parsed = Number(trimmed);
      if (parsed > 0) {
        return toRational(parsed);
      }
    }
  }

  if (typeof candidate === 'object') {
    const maybeNum = candidate.num ?? candidate.numerator;
    const maybeDen = candidate.den ?? candidate.denominator;
    if (
      typeof maybeNum === 'number' &&
      typeof maybeDen === 'number' &&
      maybeNum > 0 &&
      maybeDen > 0
    ) {
      const divisor = gcd(maybeNum, maybeDen);
      const reducedNum = maybeNum / divisor;
      const reducedDen = maybeDen / divisor;
      return {
        num: reducedNum,
        den: reducedDen,
        value: reducedNum / reducedDen,
      };
    }
  }

  return null;
}

function resolveTargetFrameRate(videoStream, maxFps = 30) {
  const fpsKeys = ['fps', 'avgFrameRate', 'avg_frame_rate', 'r_frame_rate'];
  let parsed = null;

  for (const key of fpsKeys) {
    if (videoStream && Object.prototype.hasOwnProperty.call(videoStream, key)) {
      parsed = parseFrameRateCandidate(videoStream[key]);
    }
    if (parsed) break;
  }

  if (!parsed) {
    parsed = { num: maxFps, den: 1, value: maxFps };
  }

  const currentValue = parsed.value || parsed.num / parsed.den;
  if (currentValue <= maxFps + Number.EPSILON) {
    return { ...parsed, sourceValue: currentValue };
  }

  const limited = toRational(maxFps);
  return { ...limited, sourceValue: currentValue };
}

function calculateScaledSizeByMinEdge(width, height, targetMinEdge = 360) {
  if (!targetMinEdge || targetMinEdge <= 0) {
    return { width, height };
  }

  const minEdge = Math.min(width, height);
  if (minEdge <= targetMinEdge) {
    return { width, height };
  }

  const scale = targetMinEdge / minEdge;
  const makeEven = value => Math.max(2, Math.round(value / 2) * 2);

  return {
    width: makeEven(width * scale),
    height: makeEven(height * scale),
  };
}


async function transcodeToScaled(inputPath, outputPath, options = {}) {
  const { targetMinEdge = 360 } = options;
  console.log('开始转码:', inputPath, '->', outputPath);

  
  const inputCtx = openInput(inputPath);
  const streams = getInputStreams(inputCtx);

  console.log('输入流信息:');
  streams.forEach((stream, idx) => {
    console.log(`  流 #${idx}: ${stream.type}`, stream);
  });

  
  const videoStreamIdx = streams.findIndex(s => s.type === 'video');
  const audioStreamIdx = streams.findIndex(s => s.type === 'audio');

  if (videoStreamIdx === -1) {
    throw new Error('未找到视频流');
  }

  const videoStream = streams[videoStreamIdx];
  const audioStream = audioStreamIdx !== -1 ? streams[audioStreamIdx] : null;
  const targetFrameRate = resolveTargetFrameRate(videoStream, 30);
  const targetFpsValue = targetFrameRate.num / targetFrameRate.den;
  const sourceFpsValue = targetFrameRate.sourceValue ?? targetFpsValue;
  const frameRateLimiter = new FrameRateLimiter(sourceFpsValue, targetFpsValue);

  
  const { width: targetWidth, height: targetHeight } = calculateScaledSizeByMinEdge(
    videoStream.width,
    videoStream.height,
    targetMinEdge
  );

  console.log(`\n缩放: ${videoStream.width}x${videoStream.height} -> ${targetWidth}x${targetHeight} (minEdge=${targetMinEdge})`);
  console.log(
    `帧率: 输入≈${sourceFpsValue?.toFixed ? sourceFpsValue.toFixed(3) : sourceFpsValue}fps -> 输出<=${targetFpsValue.toFixed(3)}fps`
  );
  if (frameRateLimiter.active) {
    const dropRatio = ((frameRateLimiter.sourceFps - frameRateLimiter.targetFps) / frameRateLimiter.sourceFps) * 100;
    console.log(`  ⚖️  检测到高帧率，将按时间采样丢弃多余帧 (预计丢弃比例≈${dropRatio.toFixed(2)}%)`);
  }

  
  const videoDecoder = createDecoder(videoStream.codec);
  copyDecoderParams(inputCtx, videoDecoder, videoStreamIdx);
  openDecoder(videoDecoder);

  let audioDecoder = null;
  if (audioStream) {
    audioDecoder = createDecoder(audioStream.codec);
    copyDecoderParams(inputCtx, audioDecoder, audioStreamIdx);
    openDecoder(audioDecoder);
  }

  
  const outputCtx = createOutput(outputPath, 'mp4');

  
  setOutputOption(outputCtx, 'movflags', '+faststart');
  console.log('✓ 已启用faststart（MOOV前置）');

  
  const videoEncoder = createEncoder('libx264');

  
  setEncoderOption(videoEncoder, 'width', targetWidth);
  setEncoderOption(videoEncoder, 'height', targetHeight);
  setEncoderOption(videoEncoder, 'pix_fmt', 'yuv420p'); 
  setEncoderOption(videoEncoder, 'time_base_num', targetFrameRate.den);
  setEncoderOption(videoEncoder, 'time_base_den', targetFrameRate.num);
  setEncoderOption(videoEncoder, 'framerate_num', targetFrameRate.num);
  setEncoderOption(videoEncoder, 'framerate_den', targetFrameRate.den);

  
  setEncoderOption(videoEncoder, 'bit_rate', 800000); 
  setEncoderOption(videoEncoder, 'gop_size', 30); 
  setEncoderOption(videoEncoder, 'max_b_frames', 2);

  openEncoder(videoEncoder);

  
  const outputVideoStreamIdx = addOutputStream(outputCtx, 'libx264');
  copyEncoderToStream(videoEncoder, outputCtx, outputVideoStreamIdx);

  
  let audioEncoder = null;
  let outputAudioStreamIdx = -1;

  if (audioStream) {
    audioEncoder = createEncoder('aac');

    setEncoderOption(audioEncoder, 'sample_rate', audioStream.sampleRate || 44100);
    setEncoderOption(audioEncoder, 'channels', audioStream.channels || 1);
    setEncoderOption(audioEncoder, 'sample_fmt', 'fltp'); 
    setEncoderOption(audioEncoder, 'bit_rate', 128000); 
    setEncoderOption(audioEncoder, 'time_base_num', 1);
    setEncoderOption(audioEncoder, 'time_base_den', audioStream.sampleRate || 44100);

    openEncoder(audioEncoder);
    console.log(`✓ 音频编码器: AAC, ${audioStream.sampleRate}Hz, ${audioStream.channels}ch, 128kbps`);

    outputAudioStreamIdx = addOutputStream(outputCtx, 'aac');
    copyEncoderToStream(audioEncoder, outputCtx, outputAudioStreamIdx);
  }

  
  const swsCtx = createSwsContext(
    videoStream.width,
    videoStream.height,
    videoStream.pixelFormat || 'yuv420p',
    targetWidth,
    targetHeight,
    'yuv420p',
    4
  );
  console.log('✓ 创建视频缩放上下文');

  
  writeHeader(outputCtx);
  console.log('✓ 写入文件头（包含faststart选项）\n');

  
  const decodedVideoFrame = allocFrame();
  const scaledVideoFrame = allocFrame();
  const encodedVideoPacket = allocPacket();

  let decodedAudioFrame = null;
  let bufferedAudioFrame = null; 
  let encodedAudioPacket = null;
  let audioBuffer = null; 

  if (audioStream) {
    decodedAudioFrame = allocFrame();
    bufferedAudioFrame = allocFrame();
    encodedAudioPacket = allocPacket();
    
    
    
    audioBuffer = new AudioFrameBuffer(1024, audioStream.channels, 8);
  }

  
  setFrameProperty(scaledVideoFrame, 'width', targetWidth);
  setFrameProperty(scaledVideoFrame, 'height', targetHeight);
  setFrameProperty(scaledVideoFrame, 'format', 0); 
  frameGetBuffer(scaledVideoFrame, 32); 

  
  let videoFrameCount = 0;
  let audioFrameCount = 0;
  let packetCount = 0;

  console.log('开始转码...');

  while (true) {
    
    const packet = readPacket(inputCtx);
    if (!packet) {
      console.log('输入文件读取完毕，刷新编码器...');

      
      sendFrame(videoEncoder, null);
      while (true) {
        const ret = receivePacket(videoEncoder, encodedVideoPacket);
        if (ret !== 0) break;
        writePacket(outputCtx, encodedVideoPacket, outputVideoStreamIdx);
      }

      
      if (audioEncoder) {
        
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
      
      sendPacket(videoDecoder, packet.id);

      while (true) {
        const ret = receiveFrame(videoDecoder, decodedVideoFrame);
        if (ret !== 0) break;

        if (!frameRateLimiter.shouldEmit()) {
          continue;
        }

        swsScale(swsCtx, decodedVideoFrame, scaledVideoFrame);

        
        sendFrame(videoEncoder, scaledVideoFrame);

        
        while (true) {
          const encRet = receivePacket(videoEncoder, encodedVideoPacket);
          if (encRet !== 0) break;

          writePacket(outputCtx, encodedVideoPacket, outputVideoStreamIdx);
          videoFrameCount++;
        }
      }

    } else if (audioStream && streamIdx === audioStreamIdx) {
      
      sendPacket(audioDecoder, packet.id);

      while (true) {
        const ret = receiveFrame(audioDecoder, decodedAudioFrame);
        if (ret !== 0) break;

        
        const nbSamples = getFrameProperty(decodedAudioFrame, 'nb_samples');
        
        
        
        audioBuffer.addFrame(decodedAudioFrame);
        
        
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
      }
    }

    freePacket(packet.id);

    
    if (packetCount % 100 === 0) {
          const dropInfo = frameRateLimiter.active ? `, 丢弃帧: ${frameRateLimiter.droppedFrames}` : '';
          process.stdout.write(`\r处理包: ${packetCount}, 视频帧: ${videoFrameCount}, 音频帧: ${audioFrameCount}${dropInfo}`);
    }
  }

  console.log(`\n\n转码完成:`);
  console.log(`  - 处理包数: ${packetCount}`);
  console.log(`  - 视频帧数: ${videoFrameCount}`);
  console.log(`  - 音频帧数: ${audioFrameCount}`);
  if (frameRateLimiter.active) {
    console.log(`  - 丢弃冗余帧数: ${frameRateLimiter.droppedFrames}`);
  }

  
  writeTrailer(outputCtx);

  freeFrame(decodedVideoFrame);
  freeFrame(scaledVideoFrame);
  freePacket(encodedVideoPacket);

  if (audioStream) {
    freeFrame(decodedAudioFrame);
    freeFrame(bufferedAudioFrame);
    freePacket(encodedAudioPacket);
    audioBuffer.destroy(); 
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


const inputFile = path.join(__dirname, 'input.mov');
const outputFile = path.join(__dirname, 'output.mp4');

transcodeToScaled(inputFile, outputFile, { targetMinEdge: 540 })
  .then(() => {
    console.log('\n成功！');
    process.exit(0);
  })
  .catch((error) => {
    console.error('\n错误:', error);
    process.exit(1);
  });
