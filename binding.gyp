{
  "variables": {
    "triplet%": "<!(node -e \"const os = require('os'); const arch = os.arch(); const platform = os.platform(); let triplet = 'linux-x64'; if (platform === 'win32') { triplet = 'win-x64'; } else if (platform === 'darwin') { triplet = arch === 'arm64' ? 'mac-arm64' : 'mac-x64'; } else if (platform === 'linux') { triplet = 'linux-x64'; } console.log(triplet);\")"
  },
  "targets": [
    {
      "target_name": "ffmpeg_node",
      "sources": [
        "./addon_src/binding.c",
        "./addon_src/ffmpeg.c",
        "./addon_src/utils.c",
        "./addon_src/atomic_api.c",
        "./addon_src/audio_fifo.c",
        "./ffmpeg/fftools/cmdutils.c",
        "./ffmpeg/fftools/ffmpeg_dec.c",
        "./ffmpeg/fftools/ffmpeg_demux.c",
        "./ffmpeg/fftools/ffmpeg_enc.c",
        "./ffmpeg/fftools/ffmpeg_filter.c",
        "./ffmpeg/fftools/ffmpeg_hw.c",
        "./ffmpeg/fftools/ffmpeg_mux_init.c",
        "./ffmpeg/fftools/ffmpeg_mux.c",
        "./ffmpeg/fftools/ffmpeg_opt.c",
        "./ffmpeg/fftools/ffmpeg_sched.c",
        "./ffmpeg/fftools/opt_common.c",
        "./ffmpeg/fftools/sync_queue.c",
        "./ffmpeg/fftools/thread_queue.c",
        "./ffmpeg/fftools/objpool.c"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "<!@(node -p \"require('path').dirname(process.execPath) + '/include/node'\")",
        "./ffmpeg",
        "./ffmpeg/fftools",
        "<(module_root_dir)/prebuild/<(triplet)/include"
      ],
      "conditions": [
        ["OS=='win'", {
          "include_dirs": [
            "./ffmpeg/compat/atomics/win32"
          ],
          "msvs_settings": {
            "VCCLCompilerTool": {
              "ExceptionHandling": 0
            },
            "VCLinkerTool": {
              "AdditionalLibraryDirectories": [
                "<(module_root_dir)/prebuild/<(triplet)/lib"
              ],
              "AdditionalDependencies": [
                "avcodec.lib",
                "avformat.lib",
                "avutil.lib",
                "avfilter.lib",
                "swscale.lib",
                "swresample.lib",
                "avdevice.lib",
                "libx264.lib",
                "x265-static.lib",
                "vpx.lib",
                "ws2_32.lib",
                "secur32.lib",
                "bcrypt.lib",
                "strmiids.lib",
                "ole32.lib",
                "oleaut32.lib",
                "vfw32.lib",
                "mfplat.lib",
                "mfuuid.lib",
                "shlwapi.lib",
                "user32.lib",
                "gdi32.lib",
                "winmm.lib",
                "psapi.lib",
                "zlib.lib"
              ]
            }
          },
          "msvs_configurations": {
            "Release": {
              "msvs_settings": {
                "VCCLCompilerTool": {
                  "CompileAs": "1"
                }
              }
            }
          }
        }],
        ["OS=='mac'", {
          "libraries": [
            "-L<(module_root_dir)/prebuild/<(triplet)/lib",
            "-lavcodec",
            "-lavformat",
            "-lavutil",
            "-lavfilter",
            "-lswscale",
            "-lswresample",
            "-lavdevice",
            "-lx264",
            "-lx265",
            "-lvpx",
            "-lz"
          ],
          "cflags": [
            "-std=c11",
            "-DHAVE_LIBC_M",
            "-mmacosx-version-min=11.0"
          ],
          "xcode_settings": {
            "MACOSX_DEPLOYMENT_TARGET": "11.0",
            "OTHER_CFLAGS": [
              "-std=c11",
              "-DHAVE_LIBC_M",
              "-mmacosx-version-min=11.0"
            ],
            "GCC_WARN_INHIBIT_ALL_WARNINGS": "YES",
            "OTHER_LDFLAGS": [
              "-mmacosx-version-min=11.0",
              "-Wl,-platform_version,macos,11.0,26.0",
              "-framework", "OpenGL",
              "-framework", "CoreVideo",
              "-framework", "CoreMedia",
              "-framework", "CoreGraphics",
              "-framework", "AVFoundation",
              "-framework", "AudioToolbox",
              "-framework", "CoreAudio",
              "-framework", "CoreFoundation",
              "-framework", "Foundation",
              "-framework", "AppKit"
            ]
          },
          "defines": [
            "HAVE_LIBC_M=1"
          ],
        }]
      ]
    }
  ]
}
