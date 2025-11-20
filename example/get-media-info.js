const { getVideoFormatInfo } = require('../dist/index.js');
const path = require('path');

const videoPath = path.join(__dirname, 'input.mp4');

const videoFormatInfo = getVideoFormatInfo(videoPath);

console.log(videoFormatInfo)