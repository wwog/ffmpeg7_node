

const ffmpeg = require('../dist/index.js');
const path = require('path');


let totalDuration = 0;
let lastProgressLine = '';
let logBuffer = ''; // 用于累积日志片段
function addlog() {
    ffmpeg.HighLevel.addLogListener((level, message) => {
        const cleanMessage = message.trim();

        // 忽略 VERBOSE 和 DEBUG 日志
        if (level > ffmpeg.LogLevel.INFO) {
            return;
        }

        // 累积日志片段（FFmpeg 的日志可能被分割成多行）
        logBuffer += cleanMessage + ' ';

        // 提取视频总时长（从 Duration: 后面的时间）
        if (totalDuration === 0 && logBuffer.includes('Duration')) {
            const durationMatch = logBuffer.match(/Duration.*?(\d{2}):(\d{2}):(\d{2}\.\d{2})/);
            if (durationMatch) {
                const hours = parseInt(durationMatch[1]);
                const minutes = parseInt(durationMatch[2]);
                const seconds = parseFloat(durationMatch[3]);
                totalDuration = hours * 3600 + minutes * 60 + seconds;
                console.log(`视频时长: ${durationMatch[1]}:${durationMatch[2]}:${durationMatch[3]}`);
            }
        }

        // 提取进度信息（包含 frame=, time=, speed= 的行）
        if (logBuffer.includes('frame=') && logBuffer.includes('time=') && logBuffer.includes('bitrate=')) {
            // 提取当前处理时间
            const timeMatch = logBuffer.match(/time=(\d{2}):(\d{2}):(\d{2}\.\d{2})/);
            if (timeMatch && totalDuration > 0) {
                const hours = parseInt(timeMatch[1]);
                const minutes = parseInt(timeMatch[2]);
                const seconds = parseFloat(timeMatch[3]);
                const currentTime = hours * 3600 + minutes * 60 + seconds;
                const progress = Math.min((currentTime / totalDuration * 100), 100).toFixed(1);

                // 提取帧数、fps、速度
                const frameMatch = logBuffer.match(/frame=\s*(\d+)/);
                const fpsMatch = logBuffer.match(/fps=\s*([\d.]+)/);
                const speedMatch = logBuffer.match(/speed=\s*([\d.]+)x/);

                const frame = frameMatch ? frameMatch[1] : '0';
                const fps = fpsMatch ? parseFloat(fpsMatch[1]).toFixed(1) : '0';
                const speed = speedMatch ? speedMatch[1] : '0';

                // 生成进度条
                const barLength = 30;
                const filledLength = Math.round(barLength * currentTime / totalDuration);
                const bar = '█'.repeat(filledLength) + '░'.repeat(barLength - filledLength);

                // 清除上一行，打印新进度（使用 \r 实现同行更新）
                const progressLine = `\r进度: [${bar}] ${progress}% | 帧: ${frame} | FPS: ${fps} | 速度: ${speed}x`;
                process.stdout.write(progressLine);
                lastProgressLine = progressLine;

                // 清空缓冲区
                logBuffer = '';
            }
        }

        // 如果缓冲区太大，清空它（避免内存泄漏）
        if (logBuffer.length > 5000) {
            logBuffer = '';
        }

        // 显示错误和警告
        if (level <= ffmpeg.LogLevel.WARNING && cleanMessage.length > 1) {
            if (lastProgressLine) {
                console.log(''); // 换行
            }
            console.log(`\x1b[33m⚠ 警告: ${cleanMessage}\x1b[0m`);
        }
        if (level <= ffmpeg.LogLevel.ERROR && cleanMessage.length > 1) {
            if (lastProgressLine) {
                console.log(''); // 换行
            }
            console.log(`\x1b[31m✗ 错误: ${cleanMessage}\x1b[0m`);
        }
    });

}
// 添加日志监听器 - 只处理进度信息

// 执行 FFmpeg 命令
const inputFile = path.join(__dirname, 'input.mp4');
const outputDir = path.join(__dirname, 'output');
const outputFile = path.join(outputDir, 'log-demo-output.mp4');

// 确保输出目录存在
const fs = require('fs');
if (!fs.existsSync(outputDir)) {
    fs.mkdirSync(outputDir, { recursive: true });
    console.log('✓ 创建输出目录:', outputDir);
}

console.log('输入文件:', inputFile);
console.log('输出文件:', outputFile);
console.log('\n开始转换...\n');

try {
    addlog()
    let startTime = Date.now();
    const result = ffmpeg.run([
        '-i', inputFile,
        '-vcodec', 'libx264',
        '-preset', 'medium',      // 平衡速度和压缩率
        '-crf', '28',             // 提高压缩率（23→28，值越高压缩越大）
        '-maxrate', '1.2M',       // 限制最大码率 1.2Mbps
        '-bufsize', '2.4M',       // 缓冲区大小
        '-acodec', 'aac',
        '-b:a', '96k',            // 降低音频码率（128k→96k）
        '-ac', '2',               // 立体声
        '-y',
        outputFile
    ]);
    let endTime = Date.now();
    console.log(`转换完成时间: ${endTime - startTime}ms`);
    // 确保进度条完成后换行
    if (lastProgressLine) {
        console.log(''); // 换行
    }

    console.log('\n\x1b[32m✓ 转换完成！\x1b[0m');

    // 清除日志监听器
    ffmpeg.HighLevel.clearLogListener();
    // addlog()
    // let startTime1 = Date.now();
    // ffmpeg.run([
    //     '-i', inputFile,
    //     '-vcodec', 'libx264',
    //     '-preset', 'medium',      // 平衡速度和压缩率
    //     '-crf', '28',             // 提高压缩率（23→28，值越高压缩越大）
    //     '-maxrate', '1.2M',       // 限制最大码率 1.2Mbps
    //     '-bufsize', '2.4M',       // 缓冲区大小
    //     '-acodec', 'aac',
    //     '-b:a', '96k',            // 降低音频码率（128k→96k）
    //     '-ac', '2',               // 立体声
    //     '-y',
    //     outputFile
    // ]);
    // let endTime1 = Date.now();
    // console.log(`转换完成时间: ${endTime1 - startTime1}ms`);
    // // 确保进度条完成后换行
    // if (lastProgressLine) {
    //     console.log(''); // 换行
    // }

    // console.log('\n\x1b[32m✓ 转换完成！\x1b[0m');


} catch (error) {
    console.log(''); // 换行
    console.error('\n\x1b[31m✗ 转换失败:', error.message, '\x1b[0m');

    // 确保清除日志监听器
    ffmpeg.clearLogListener();

    process.exit(1);
}

console.log('\n=== DONE ===');

