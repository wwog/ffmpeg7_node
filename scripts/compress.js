const fs = require('fs');
const path = require('path');
const os = require('os');
const { createWriteStream } = require('fs');

const rootDir = path.join(__dirname, '..');

// Supported platforms
const platforms = ['mac-arm64', 'mac-x64', 'win-x64', 'linux-x64'];

function getTriplet() {
  const arch = os.arch();
  const platform = os.platform();
  if (platform === 'win32') {
    return 'win-x64';
  } else if (platform === 'darwin') {
    return arch === 'arm64' ? 'mac-arm64' : 'mac-x64';
  } else if (platform === 'linux') {
    return 'linux-x64';
  }
  return null;
}

async function compressTarGz(sourceDir, archivePath) {
  console.log(`Compressing ${path.basename(sourceDir)}...`);
  
  return new Promise((resolve, reject) => {
    const output = createWriteStream(archivePath);
    
    const tar = require('child_process').spawn('tar', [
      '-czf', '-',
      '-C', path.dirname(sourceDir),
      path.basename(sourceDir)
    ], {
      stdio: ['ignore', 'pipe', 'inherit']
    });

    tar.stdout.pipe(output);

    output.on('finish', () => {
      const stats = fs.statSync(archivePath);
      const sizeMB = (stats.size / 1024 / 1024).toFixed(2);
      console.log(`✓ ${path.basename(archivePath)} compressed successfully (${sizeMB} MB)`);
      resolve();
    });

    tar.on('error', reject);
    output.on('error', reject);
  });
}

async function compressAll() {
  console.log('Compressing FFmpeg dependencies...\n');

  // Compress ffmpeg directory (single archive)
  const ffmpegDir = path.join(rootDir, 'ffmpeg');
  const ffmpegArchive = path.join(rootDir, 'ffmpeg.tar.gz');
  
  if (fs.existsSync(ffmpegDir)) {
    if (fs.existsSync(ffmpegArchive)) {
      console.log('✓ ffmpeg.tar.gz already exists, skipping compression');
    } else {
      try {
        await compressTarGz(ffmpegDir, ffmpegArchive);
      } catch (error) {
        console.error(`Error compressing ffmpeg:`, error.message);
        process.exit(1);
      }
    }
  } else {
    console.log('⚠ ffmpeg directory not found, skipping');
  }

  // Compress prebuild directories (one archive per platform)
  const prebuildDir = path.join(rootDir, 'prebuild');
  
  if (!fs.existsSync(prebuildDir)) {
    console.log('⚠ prebuild directory not found, skipping');
  } else {
    console.log('\nCompressing prebuild directories by platform...\n');
    
    for (const platform of platforms) {
      const platformDir = path.join(prebuildDir, platform);
      const archiveName = `prebuild-${platform}.tar.gz`;
      const archivePath = path.join(rootDir, archiveName);

      // Check if platform directory exists
      if (!fs.existsSync(platformDir)) {
        console.log(`⚠ prebuild/${platform} directory not found, skipping`);
        continue;
      }

      // Skip compression if archive already exists
      if (fs.existsSync(archivePath)) {
        console.log(`✓ ${archiveName} already exists, skipping compression`);
        continue;
      }

      try {
        // Compress from prebuild directory so it extracts as prebuild/<platform>
        await compressTarGz(platformDir, archivePath);
      } catch (error) {
        console.error(`Error compressing prebuild-${platform}:`, error.message);
        process.exit(1);
      }
    }
  }

  console.log('\n✓ All files compressed successfully!');
  console.log('\nNote: You can temporarily remove ffmpeg and prebuild directories to reduce npm package size');
  console.log('They will be automatically extracted when users install the package');
}

// Run if executed directly
if (require.main === module) {
  compressAll().catch(error => {
    console.error('Compression failed:', error);
    process.exit(1);
  });
}

module.exports = { compressAll };

