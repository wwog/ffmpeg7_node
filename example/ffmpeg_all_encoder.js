const ffmpeg = require('../dist/index.js');
const path = require('path');

// ffmpeg.run(['-encoders']);
const inputFile = path.join(__dirname, 'input.mp4');
const outputFile = path.join(__dirname, 't.png')
ffmpeg.run(['-i', inputFile, '-vf', 'thumbnail', '-frames:v', "1", outputFile]);