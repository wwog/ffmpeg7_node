/**
 * 使用 FFmpeg 中级 API 获取图片元数据示例
 * 
 * 功能：
 * 1. 使用中级 API 的 openInput() 和 getInputStreams() 方法
 * 2. 直接从结构化数据中获取图片信息，无需解析日志
 * 3. 解析并显示图片的格式、尺寸、编码等信息
 * 
 * 对比高级API的优势：
 * - 不需要日志监听器和日志解析
 * - 直接获取结构化数据
 * - 代码更简洁、更可靠
 */

const path = require('path');
const { MidLevel } = require('../dist/index.js');
const { openInput, getInputStreams, getMetadata, closeContext } = MidLevel;

/**
 * 根据编码格式判断真实的图片格式
 * @param {string} codec - FFmpeg 识别的编码格式
 * @returns {string} 真实的图片格式描述
 */
function getActualImageFormat(codec) {
  const formatMap = {
    'mjpeg': 'JPEG/JPG',
    'jpeg': 'JPEG/JPG',
    'png': 'PNG',
    'webp': 'WebP',
    'gif': 'GIF',
    'bmp': 'BMP',
    'tiff': 'TIFF',
    'hevc': 'HEIF/HEIC',
    'av1': 'AVIF',
    'vp9': 'WebP (动画)',
    'apng': 'APNG (动画PNG)',
    'jpegls': 'JPEG-LS',
    'jpeg2000': 'JPEG 2000 (JP2)',
    'psd': 'Photoshop PSD',
    'exr': 'OpenEXR',
    'dpx': 'DPX',
    'svg': 'SVG',
    'rawvideo': 'RAW',
  };
  
  if (!codec) return '未知格式';
  
  return formatMap[codec.toLowerCase()] || codec.toUpperCase();
}

/**
 * 使用中级 API 获取图片元数据
 * @param {string} imagePath - 图片文件路径
 */
function getImageMetadata(imagePath) {
  console.log('=== 使用中级 API 获取图片元数据 ===\n');
  console.log(`图片路径: ${imagePath}\n`);

  let inputCtx;
  
  try {
    // 1. 打开输入文件
    inputCtx = openInput(imagePath);
    console.log(`✓ 成功打开文件，上下文 ID: ${inputCtx}\n`);

    // 2. 获取流信息
    const streams = getInputStreams(inputCtx);
    console.log(`✓ 找到 ${streams.length} 个流\n`);

    // 3. 解析流信息
    if (streams.length === 0) {
      console.log('⚠️  警告: 没有找到任何流信息');
      return;
    }

    console.log('=== 解析的图片信息 ===\n');

    // 提取文件信息
    const fileName = path.basename(imagePath);
    const fileExt = path.extname(imagePath).toLowerCase();
    console.log(`📁 文件名: ${fileName}`);
    console.log(`📝 文件扩展名: ${fileExt || '(无扩展名)'}`);

    // 4. 遍历所有流（通常图片只有一个视频流）
    streams.forEach((stream, idx) => {
      console.log(`\n--- 流 #${stream.index} (${stream.type}) ---`);
      
      if (stream.type === 'video') {
        // 这是视频流（图片也是视频流的一种）
        const actualFormat = getActualImageFormat(stream.codec);
        
        console.log(`🔍 真实格式: ${actualFormat}`);
        console.log(`🎨 编码格式: ${stream.codec || '未知'}`);
        
        // 检查扩展名是否匹配
        const extUpper = fileExt.replace('.', '').toUpperCase();
        if (fileExt && !actualFormat.toUpperCase().includes(extUpper)) {
          console.log(`⚠️  警告: 文件扩展名 ${fileExt} 与实际格式 ${actualFormat} 不匹配！`);
        }
        
        // 显示尺寸信息
        if (stream.width && stream.height) {
          console.log(`📐 图片尺寸: ${stream.width}x${stream.height} 像素`);
          console.log(`📊 宽高比: ${(stream.width / stream.height).toFixed(2)}`);
          
          // 计算像素总数
          const megapixels = (stream.width * stream.height / 1000000).toFixed(2);
          console.log(`🖼️  像素总数: ${megapixels} MP`);
        }
        
        // 显示帧率（对于动画图片）
        if (stream.fps && stream.fps > 0) {
          console.log(`🎬 帧率: ${stream.fps} fps`);
          console.log(`💡 提示: 这可能是动画图片 (GIF/APNG/WebP)`);
        }
        
        // 显示比特率
        if (stream.bitrate) {
          console.log(`💾 比特率: ${(stream.bitrate / 1000).toFixed(2)} kb/s`);
        }
      } else {
        // 其他类型的流（如音频、字幕等，在图片中不常见）
        console.log(`类型: ${stream.type}`);
        console.log(`编码: ${stream.codec || '未知'}`);
      }
    });

    // 5. 获取元数据
    console.log('\n=== 文件元数据 ===\n');
    try {
      const metadata = getMetadata(inputCtx);
      if (metadata && typeof metadata === 'object' && Object.keys(metadata).length > 0) {
        Object.entries(metadata).forEach(([key, value]) => {
          console.log(`  ${key}: ${value}`);
        });
      } else {
        console.log('(无元数据)');
      }
    } catch (err) {
      console.log('(无法读取元数据)');
    }

    // 6. 获取文件大小
    console.log('\n=== 文件信息 ===\n');
    const fs = require('fs');
    try {
      const stats = fs.statSync(imagePath);
      const fileSizeKB = (stats.size / 1024).toFixed(2);
      const fileSizeMB = (stats.size / 1024 / 1024).toFixed(2);
      console.log(`📦 文件大小: ${fileSizeKB} KB (${fileSizeMB} MB)`);
      
      // 计算每像素的比特数（如果有尺寸信息）
      const videoStream = streams.find(s => s.type === 'video');
      if (videoStream && videoStream.width && videoStream.height) {
        const totalPixels = videoStream.width * videoStream.height;
        const bitsPerPixel = (stats.size * 8 / totalPixels).toFixed(2);
        console.log(`📊 每像素比特数: ${bitsPerPixel} bits/pixel`);
      }
    } catch (e) {
      console.log('(无法读取文件大小)');
    }

  } catch (error) {
    console.error('❌ 错误:', error.message);
    throw error;
  } finally {
    // 7. 清理：关闭上下文
    if (inputCtx !== undefined) {
      try {
        closeContext(inputCtx);
        console.log('\n✓ 已清理资源');
      } catch (err) {
        console.error('⚠️  清理资源失败:', err.message);
      }
    }
  }
}

/**
 * 对比函数：显示高级API和中级API的区别
 */
function showComparison() {
  console.log('\n╔═══════════════════════════════════════════╗');
  console.log('║  高级 API vs 中级 API 对比               ║');
  console.log('╚═══════════════════════════════════════════╝\n');
  
  console.log('📌 高级 API (run方法):');
  console.log('  ✓ 简单易用，类似命令行');
  console.log('  ✗ 需要解析日志文本');
  console.log('  ✗ 不够可靠（日志格式可能变化）');
  console.log('  ✗ 需要正则表达式提取信息');
  
  console.log('\n📌 中级 API (openInput/getInputStreams):');
  console.log('  ✓ 直接返回结构化数据');
  console.log('  ✓ 类型安全（TypeScript支持）');
  console.log('  ✓ 更可靠、更精确');
  console.log('  ✓ 代码更简洁');
  console.log('  ✓ 更适合生产环境');
  
  console.log('\n💡 推荐: 使用中级 API 获取元数据信息！\n');
}

// ========== 使用示例 ==========

// 命令行参数处理
const args = process.argv.slice(2);

if (args.length === 0) {
  console.log('用法: node get-image-type-midlevel.js <图片路径>');
  console.log('\n示例:');
  console.log('  node get-image-type-midlevel.js test.jpg');
  console.log('  node get-image-type-midlevel.js /path/to/image.png');
  console.log('  node get-image-type-midlevel.js image.webp');
  console.log('\n支持的图片格式:');
  console.log('  - JPEG/JPG (.jpg, .jpeg)');
  console.log('  - PNG (.png)');
  console.log('  - WebP (.webp)');
  console.log('  - GIF (.gif)');
  console.log('  - BMP (.bmp)');
  console.log('  - TIFF (.tiff, .tif)');
  console.log('  - HEIF/HEIC (.heif, .heic)');
  console.log('  - AVIF (.avif)');
  console.log('  - 以及 FFmpeg 支持的其他图片格式');
  console.log('\n特点:');
  console.log('  ✓ 使用中级 API，无需解析日志');
  console.log('  ✓ 直接获取结构化数据');
  console.log('  ✓ 更可靠、更快速');
  
  showComparison();
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
console.log('║  FFmpeg 中级 API 图片元数据提取工具      ║');
console.log('╚═══════════════════════════════════════════╝\n');

try {
  getImageMetadata(imagePath);
  console.log('\n✅ 完成！');
} catch (error) {
  console.error('\n❌ 执行失败:', error.message);
  process.exit(1);
}

// 显示API对比
if (process.env.SHOW_COMPARISON) {
  showComparison();
}

