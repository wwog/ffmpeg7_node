#!/usr/bin/env node

const fs = require('fs');
const path = require('path');

const distDir = path.join(__dirname, '..', 'dist');
const buildDir = path.join(__dirname, '..', 'build');

// Ensure dist directory exists
if (!fs.existsSync(distDir)) {
  fs.mkdirSync(distDir, { recursive: true });
}

// Function to copy native module (.node file only)
function copyNativeModule() {
  console.log('Copying native module to dist...');
  
  const nodeFile = path.join(buildDir, 'Release', 'ffmpeg_node.node');
  const distNodeFile = path.join(distDir, 'ffmpeg_node.node');

  if (fs.existsSync(nodeFile)) {
    fs.copyFileSync(nodeFile, distNodeFile);
    console.log('✓ Native module copied successfully');
  } else {
    console.warn('⚠ Native module file not found, it may not be built yet');
  }
}

// Main build process
try {
  console.log('Copying native module to dist...');
  copyNativeModule();
  console.log(`✓ Output directory: ${distDir}`);
} catch (error) {
  console.error('✗ Build failed:', error.message);
  process.exit(1);
}

