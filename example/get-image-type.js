/**
 * 使用 FFmpeg run 方法获取图片元数据示例
 * 
 * 功能：
 * 1. 使用高级 API run() 方法执行 FFmpeg 命令
 * 2. 通过日志监听器捕获图片元数据信息
 * 3. 解析并显示图片的格式、尺寸、编码等信息
 */

const path = require('path');
const { run, addLogListener, clearLogListener } = require('../dist/index.js');

/**
 * 使用 run 方法获取图片元数据
 * @param {string} imagePath - 图片文件路径
 */
function getImageMetadata(imagePath) {
  console.log('=== 获取图片元数据 ===\n');
  console.log(`图片路径: ${imagePath}\n`);

  // 用于存储捕获的日志信息
  let capturedLogs = [];

  // 添加日志监听器，捕获 FFmpeg 的输出
  addLogListener((level, message) => {
    capturedLogs.push({ level, message });
    // 实时输出日志
    console.log(`[日志级别 ${level}] ${message}`);
  });

  try {
    // 使用 run 方法执行 FFmpeg 命令
    // -i: 输入文件
    // -f null: 不输出任何内容（只是为了触发 FFmpeg 分析文件）
    // -: 输出到标准输出（但因为是 null 格式，所以不会有实际输出）
    const exitCode = run([
      '-i', imagePath,  // 输入图片
      '-f', 'null',     // 不输出，只分析
      '-'               // 输出到标准输出
    ]);

    console.log(`\n✓ FFmpeg 执行完成，退出码: ${exitCode}\n`);

    // 解析捕获的日志信息
    parseImageInfo(capturedLogs, imagePath);

  } catch (error) {
    console.error('❌ 错误:', error.message);
  } finally {
    // 清理日志监听器
    clearLogListener();
  }
}

/**
 * 根据编码格式判断真实的图片格式
 * @param {string} codec - FFmpeg 识别的编码格式
 * @param {string} container - 容器格式
 * @returns {string} 真实的图片格式描述
 */
function getActualImageFormat(codec, container) {
  const formatMap = {
    'mjpeg': 'JPEG/JPG',
    'jpeg': 'JPEG/JPG',
    'png': 'PNG',
    'webp': 'WebP',
    'gif': 'GIF',
    'bmp': 'BMP',
    'tiff': 'TIFF/DNG',
    'hevc': 'HEIF/HEIC',
    'av1': 'AVIF',
    'vp9': 'WebP (动画)',
    'apng': 'APNG (动画PNG)',
    'jpegls': 'JPEG-LS',
    'jpeg2000': 'JPEG 2000',
    'psd': 'Photoshop PSD',
    'exr': 'OpenEXR',
    'dpx': 'DPX',
    'svg': 'SVG',
    'raw': 'RAW',
  };

  // 特殊处理：如果是 tiff 且容器显示可能是 DNG
  if (codec === 'tiff' && container && container.includes('tiff')) {
    return 'TIFF (可能是 DNG)';
  }

  return formatMap[codec.toLowerCase()] || codec.toUpperCase();
}

/**
 * 从 FFmpeg 日志中解析图片信息
 * @param {Array} logs - 捕获的日志数组
 * @param {string} imagePath - 图片路径
 */
function parseImageInfo(logs, imagePath) {
  console.log('=== 解析的图片元数据 ===\n');

  // 提取文件扩展名（仅作参考）
  const path = require('path');
  const fileExt = path.extname(imagePath).toLowerCase();
  const fileName = path.basename(imagePath);

  console.log(`📁 文件名: ${fileName}`);

  // 将所有日志合并为一个字符串便于搜索
  const fullLog = logs.map(l => l.message).join('\n');

  // 提取 FFmpeg 容器格式信息
  let containerFormat = '';
  const inputMatch = fullLog.match(/Input #\d+, ([^,]+)/);
  if (inputMatch) {
    containerFormat = inputMatch[1];
  }

  // 提取流信息（包含图片编码格式和尺寸）
  // 示例: "Stream #0:0: Video: mjpeg (Baseline), yuvj420p(pc, bt470bg/unknown/unknown), 544x960"
  const streamMatch = fullLog.match(/Video:\s+([^\s,\(]+)(?:\s*\([^)]*\))?,\s*([^\s,\(]+)(?:\([^)]*\))?,\s*(\d+)x(\d+)/);
  if (streamMatch) {
    const codec = streamMatch[1];
    const pixelFormat = streamMatch[2];
    const width = streamMatch[3];
    const height = streamMatch[4];

    // 根据编码格式判断真实格式
    const actualFormat = getActualImageFormat(codec, containerFormat);

    console.log(`🔍 真实格式: ${actualFormat}`);
    console.log(`📝 文件扩展名: ${fileExt || '(无扩展名)'}`);

    // 如果扩展名和实际格式不匹配，给出警告
    const extUpper = fileExt.replace('.', '').toUpperCase();
    if (fileExt && !actualFormat.toUpperCase().includes(extUpper)) {
      console.log(`⚠️  警告: 文件扩展名 ${fileExt} 与实际格式 ${actualFormat} 不匹配！`);
    }

    console.log(`📦 容器格式: ${containerFormat}`);
    console.log(`🎨 编码格式: ${codec}`);
    console.log(`📐 图片尺寸: ${width}x${height} 像素`);
    console.log(`📊 宽高比: ${(width / height).toFixed(2)}`);
    console.log(`🖼️  像素格式: ${pixelFormat}`);
  } else {
    // 如果没有匹配到编码信息，仍然显示基本信息
    console.log(`📝 文件扩展名: ${fileExt || '(无扩展名)'}`);
    if (containerFormat) {
      console.log(`📦 容器格式: ${containerFormat}`);
    }
  }

  // 提取帧率信息（如果有）
  const fpsMatch = fullLog.match(/(\d+(?:\.\d+)?)\s+fps/);
  if (fpsMatch) {
    console.log(`🎬 帧率: ${fpsMatch[1]} fps`);
  }

  // 提取比特率信息
  const bitrateMatch = fullLog.match(/bitrate:\s*(\d+)\s*kb\/s/);
  if (bitrateMatch) {
    console.log(`💾 比特率: ${bitrateMatch[1]} kb/s`);
  }

  // 提取时长信息
  const durationMatch = fullLog.match(/Duration:\s*(\d{2}):(\d{2}):(\d{2})\.(\d{2})/);
  if (durationMatch) {
    console.log(`⏱️  时长: ${durationMatch[1]}:${durationMatch[2]}:${durationMatch[3]}.${durationMatch[4]}`);
  }

  // 文件大小（需要从其他方式获取）
  const fs = require('fs');
  try {
    const stats = fs.statSync(imagePath);
    const fileSizeKB = (stats.size / 1024).toFixed(2);
    const fileSizeMB = (stats.size / 1024 / 1024).toFixed(2);
    console.log(`📦 文件大小: ${fileSizeKB} KB (${fileSizeMB} MB)`);
  } catch (e) {
    // 忽略文件大小获取错误
  }

  console.log('\n=== 完整日志输出 ===\n');
  console.log('（已在上方实时显示）');
}

/**
 * 方法2: 获取更详细的格式信息（使用 ffprobe 风格的命令）
 */
function getDetailedImageInfo(imagePath) {
  console.log('\n=== 获取详细的图片信息（ffprobe 风格）===\n');

  let capturedLogs = [];

  addLogListener((level, message) => {
    capturedLogs.push({ level, message });
    console.log(message);
  });

  try {
    // 使用更详细的选项
    const exitCode = run([
      '-i', imagePath,
      '-hide_banner'   // 隐藏版本信息，让输出更清晰
    ]);

    console.log(`\n✓ 执行完成，退出码: ${exitCode}`);
  } catch (error) {
    console.error('错误:', error.message);
  } finally {
    clearLogListener();
  }
}

// ========== 使用示例 ==========

// 命令行参数处理
const args = process.argv.slice(2);

if (args.length === 0) {
  console.log('用法: node get-image-type.js <图片路径>');
  console.log('\n示例:');
  console.log('  node get-image-type.js test.jpg');
  console.log('  node get-image-type.js /path/to/image.png');
  console.log('  node get-image-type.js image.webp');
  console.log('\n支持的图片格式:');
  console.log('  - JPEG/JPG (.jpg, .jpeg)');
  console.log('  - PNG (.png)');
  console.log('  - WebP (.webp)');
  console.log('  - GIF (.gif)');
  console.log('  - BMP (.bmp)');
  console.log('  - TIFF (.tiff, .tif)');
  console.log('  - 以及 FFmpeg 支持的其他图片格式');
  process.exit(1);
}

const imagePath = args[0];

// 检查文件是否存在
const fs = require('fs');
if (!fs.existsSync(imagePath)) {
  console.error(`❌ 错误: 文件不存在: ${imagePath}`);
  process.exit(1);
}

// 执行元数据提取
console.log('╔═══════════════════════════════════════════╗');
console.log('║    FFmpeg 图片元数据提取工具              ║');
console.log('╚═══════════════════════════════════════════╝\n');

getImageMetadata(imagePath);

// 可选：获取更详细的信息
// getDetailedImageInfo(imagePath);

console.log('\n✅ 完成！');

