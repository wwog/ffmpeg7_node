import { defineConfig } from 'rolldown';

export default defineConfig({
    input: 'src/index.ts',
    output: {
        file: 'dist/index.js',
        format: 'cjs',
        sourcemap: false,
    },
    external: [
        // Mark .node native module as external dependency
        /\.node$/,
    ],
});