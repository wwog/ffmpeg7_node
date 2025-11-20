const fs = require('fs');
const path = require('path');
const os = require('os');
const { createWriteStream, createReadStream } = require('fs');
const zlib = require('zlib');
const { spawn } = require('child_process');

const rootDir = path.join(__dirname, '..');

const giteeUrl = "https://gitee.com/Guohear/ffmpeg-static-prebuild/releases/download/ffmpeg-7.1.2-static-prebuild"
const gitHubRelease = "https://github.com/wwog/ffmpeg7_node/releases/download/prebuild_static"


function getPlatform(){
    const platform = os.platform()
    if (platform === 'win32') {
        return 'win-x64'
    } else if (platform === 'darwin') {
        return os.arch() === 'arm64' ? 'mac-arm64' : 'mac-x64'
    } 
    return 'not supported'
}

function createNetworkTestPromise(baseUrl, errorMessage) {
    return (async () => {
        const res = await fetch(baseUrl + "/ffmpeg.tar.gz", {
            method: "HEAD",
        });
        if (res.status === 200) {
            return baseUrl;
        } else {
            throw new Error(errorMessage);
        }
    })();
}

const raceSuccess = async (promises) => {
    const errors = [];
    return new Promise((resolve, reject) => {
        promises.forEach((promise, index) => {
            promise
                .then((result) => {
                    resolve(result);
                })
                .catch((error) => {
                    errors[index] = error;
                    if (errors.filter(e => e !== undefined).length === promises.length) {
                        reject(new Error('All network sources failed'));
                    }
                });
        });
    });
};

async function testNetwork() {
    const giteePromise = createNetworkTestPromise(giteeUrl, "giteeUrl not found");
    const githubPromise = createNetworkTestPromise(gitHubRelease, "gitHubRelease not found");

    return raceSuccess([giteePromise, githubPromise]);
}

class ProgressManager {
    constructor(totalFiles) {
        this.totalFiles = totalFiles;
        this.progressData = new Array(totalFiles).fill(null).map(() => ({
            fileName: '',
            downloadedSize: 0,
            totalSize: 0,
            completed: false
        }));
        this.lastRenderTime = 0;
    }

    update(fileIndex, fileName, downloadedSize, totalSize) {
        this.progressData[fileIndex] = {
            fileName,
            downloadedSize,
            totalSize,
            completed: false
        };
        this.render();
    }

    complete(fileIndex, fileName, totalSize) {
        this.progressData[fileIndex] = {
            fileName,
            downloadedSize: totalSize,
            totalSize,
            completed: true
        };
        this.render();
    }

    render() {
        const now = Date.now();
        if (now - this.lastRenderTime < 200 && !this.progressData.some(d => d && d.completed)) {
            return;
        }
        this.lastRenderTime = now;

        if (this.totalFiles > 0) {
            process.stdout.write(`\x1b[${this.totalFiles}A`);
        }

        for (let i = 0; i < this.totalFiles; i++) {
            const data = this.progressData[i];
            if (!data || !data.fileName) {
                process.stdout.write(`\x1b[2K\r\n`);
                continue;
            }

            process.stdout.write(`\x1b[2K\r`);

            if (data.completed) {
                if (data.totalSize > 0) {
                    const totalMB = (data.totalSize / 1024 / 1024).toFixed(2);
                    process.stdout.write(`[${i + 1}] ${data.fileName}: 100.0% (${totalMB}MB / ${totalMB}MB) ✓`);
                } else {
                    process.stdout.write(`[${i + 1}] ${data.fileName}: Complete ✓`);
                }
            } else {
                if (data.totalSize > 0) {
                    const percent = ((data.downloadedSize / data.totalSize) * 100).toFixed(1);
                    const downloadedMB = (data.downloadedSize / 1024 / 1024).toFixed(2);
                    const totalMB = (data.totalSize / 1024 / 1024).toFixed(2);
                    process.stdout.write(`[${i + 1}] ${data.fileName}: ${percent}% (${downloadedMB}MB / ${totalMB}MB)`);
                } else {
                    const downloadedMB = (data.downloadedSize / 1024 / 1024).toFixed(2);
                    process.stdout.write(`[${i + 1}] ${data.fileName}: ${downloadedMB}MB`);
                }
            }
            process.stdout.write('\n');
        }
    }

    cleanup() {
        this.render();
    }
}

async function downloadFile(url, filePath, fileIndex, progressManager) {
    return new Promise(async (resolve, reject) => {
        try {
            const headResponse = await fetch(url, { method: "HEAD" });
            if (headResponse.status !== 200) {
                throw new Error(`File not found: ${url} (status: ${headResponse.status})`);
            }

            const totalSize = parseInt(headResponse.headers.get('content-length') || '0', 10);
            const fileName = path.basename(filePath);

            const response = await fetch(url);
            if (!response.ok) {
                throw new Error(`Failed to download: ${url} (status: ${response.status})`);
            }

            const fileStream = createWriteStream(filePath);
            const reader = response.body.getReader();
            let downloadedSize = 0;
            let lastUpdateTime = Date.now();

            const updateProgress = () => {
                const now = Date.now();
                if (now - lastUpdateTime < 200 && downloadedSize !== totalSize) {
                    return;
                }
                lastUpdateTime = now;
                progressManager.update(fileIndex, fileName, downloadedSize, totalSize);
            };

            const pump = async () => {
                while (true) {
                    const { done, value } = await reader.read();
                    if (done) break;

                    downloadedSize += value.length;
                    fileStream.write(value);
                    updateProgress();
                }
                fileStream.end();
                
                if (downloadedSize < totalSize && totalSize > 0) {
                    downloadedSize = totalSize;
                }
                progressManager.update(fileIndex, fileName, downloadedSize, totalSize);
            };

            fileStream.on('finish', () => {
                downloadedSize = totalSize;
                progressManager.complete(fileIndex, fileName, totalSize);
                resolve();
            });

            fileStream.on('error', reject);
            pump().catch(reject);
        } catch (error) {
            reject(error);
        }
    });
}

async function extractTarGz(archivePath, destPath) {
    return new Promise((resolve, reject) => {
        const parentDir = path.dirname(destPath);
        if (!fs.existsSync(parentDir)) {
            fs.mkdirSync(parentDir, { recursive: true });
        }

        const fileName = path.basename(archivePath);
        console.log(`Extracting ${fileName}...`);

        const gunzip = zlib.createGunzip();
        const extract = spawn('tar', ['-xf', '-', '-C', parentDir], {
            stdio: ['pipe', 'inherit', 'inherit']
        });

        const input = createReadStream(archivePath);
        input.pipe(gunzip).pipe(extract.stdin);

        extract.on('close', (code) => {
            if (code === 0) {
                console.log(`✓ ${fileName} extracted successfully`);
                resolve();
            } else {
                reject(new Error(`Extraction failed with code: ${code}`));
            }
        });

        extract.on('error', reject);
        gunzip.on('error', reject);
        input.on('error', reject);
    });
}

async function downloadArchive(){
    const currentPlatform = getPlatform()
    if (currentPlatform === 'not supported') {
        throw new Error("platform not supported")
    }
    
    console.log('Detecting available download source...');
    const url = await testNetwork()
    console.log(`✓ Using download source: ${url}\n`);

    const files = [
        { url: url + `/ffmpeg.tar.gz`, name: 'ffmpeg.tar.gz', extractTo: path.join(rootDir, 'ffmpeg') },
        { url: url + `/prebuild-${currentPlatform}.tar.gz`, name: `prebuild-${currentPlatform}.tar.gz`, extractTo: path.join(rootDir, 'prebuild', currentPlatform) },
    ];

    const filesToDownload = [];
    for (const file of files) {
        const filePath = path.join(rootDir, file.name);
        if (fs.existsSync(filePath)) {
            console.log(`✓ ${file.name} already exists, skipping download`);
        } else {
            filesToDownload.push({ ...file, path: filePath });
        }
    }

    if (filesToDownload.length === 0) {
        console.log('\nAll files already exist, no download needed');
    } else {
        console.log(`\nStarting download of ${filesToDownload.length} file(s)...\n`);

        const progressManager = new ProgressManager(filesToDownload.length);
        for (let i = 0; i < filesToDownload.length; i++) {
            process.stdout.write('\n');
        }
        process.stdout.write(`\x1b[${filesToDownload.length}A`);

        try {
            await Promise.all(
                filesToDownload.map((file, index) => downloadFile(file.url, file.path, index, progressManager))
            );
            progressManager.cleanup();
            console.log('\n✓ All files downloaded successfully!');
        } catch (error) {
            progressManager.cleanup();
            console.error('\n✗ Download failed:', error.message);
            process.exit(1);
        }
    }

    console.log('\nExtracting archives...\n');

    for (const file of files) {
        const archivePath = path.join(rootDir, file.name);
        const destPath = file.extractTo;

        if (!fs.existsSync(archivePath)) {
            console.log(`⚠ ${file.name} not found, skipping extraction`);
            continue;
        }

        if (fs.existsSync(destPath)) {
            console.log(`✓ ${path.basename(destPath)} already exists, skipping extraction`);
            continue;
        }

        try {
            await extractTarGz(archivePath, destPath);
        } catch (error) {
            console.error(`Error extracting ${file.name}:`, error.message);
            process.exit(1);
        }
    }

    console.log('\n✓ All files extracted successfully!');
}

downloadArchive()