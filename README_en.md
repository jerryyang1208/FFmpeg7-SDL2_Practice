<div align="right">
  <a href="README.md">中文</a>
</div>

<div align="center">

# FFmpeg7-SDL2 Multimedia Practice Project

![FFmpeg](https://img.shields.io/badge/FFmpeg-7.0.2-green)
![SDL2](https://img.shields.io/badge/SDL2-2.30.6-blue)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)

</div>

A multimedia player practice project based on FFmpeg 7.0.2 and SDL2 2.30.6, containing six independent examples including audio playback, PCM playback, video playback, multi-threaded audio/video synchronized player (Windows/WSL dual versions), and HW hardware-accelerated audio/video player (with/without filter versions).

Both software and hardware versions of the audio/video player have basic playback functions and media information display, as well as rotation and scaling adaptation for videos of different resolutions and orientations. To facilitate comparison and understanding, the original hardware-accelerated audio/video player HW.cpp code is retained, which also supports DXVA2 hardware decoding. However, the new version significantly reduces CPU and memory usage, enabling smooth playback of 4K high-frame-rate, extremely high-bitrate videos.

Blog: https://zhuanlan.zhihu.com/p/700478133

## 📦 Dependencies
- **[FFmpeg 7.0.2](https://ffmpeg.org/)** - Core audio/video decoding library
- **[SDL2 2.30.6](https://www.libsdl.org/)** - Cross-platform multimedia rendering library

## 🚀 Quick Start

### Environment Setup
1. Download the required library package from [Releases](https://github.com/jerryyang1208/FFmpeg7-SDL2_Practice/releases)

2. Extract to the project root directory, ensuring the directory structure is as follows:
<pre>
ffmpeg+SDL2/
├── .vscode/                      # VS Code configuration files
│   ├── c_cpp_properties.json     # C/C++ plugin configuration
│   ├── launch.json               # Debugging configuration
│   ├── settings.json             # Editor settings
│   └── tasks.json                # Build task configuration
│
├── FFmpeg-n7.0.2-3-win64-lgpl.../# FFmpeg library folder
├── SDL2-2.30.6/                  # SDL2 library folder
│
├── .gitignore                    # Git ignore configuration
├── README.md                     # Project documentation (Chinese)
├── README_en.md                  # Project documentation (English)
│
├── AudioPlayer.cpp               # Audio player source code
├── PCM_AudioPlayer.cpp           # PCM audio player source code
├── SDL_Pics.cpp                  # SDL graphics drawing source code
├── VideoPlayer.cpp               # Video player source code
├── Audio_Video_Player.cpp        # Multi-threaded audio/video synchronized player source code
├── Audio_Video_Player(WSL).cpp   # WSL version audio/video player source code
├── HW_Audio_Video_Player.cpp     # Hardware-accelerated audio/video player source code
│
├── avcodec-61.dll                # FFmpeg codec library
├── avdevice-61.dll               # FFmpeg device library
├── avfilter-10.dll               # FFmpeg filter library
├── avformat-61.dll               # FFmpeg format library
├── avutil-59.dll                 # FFmpeg utility library
├── swresample-5.dll              # FFmpeg audio resampling library
├── swscale-8.dll                 # FFmpeg image scaling library
└── SDL2.dll                      # SDL2 dynamic link library
</pre>

3. Ensure DLL files are in the same directory as the executable or in the system PATH. These dynamic link library files are required at runtime.

### Compilation Environment
- **Compiler**: MSVC (Visual Studio) or MinGW (VS Code). The Linux version is compiled using GCC/G++ (WSL VS Code)
- **Standard**: C++11/14
- **Include Directories**: Need to add FFmpeg and SDL2 include paths
- **Library Directories**: Need to add FFmpeg and SDL2 lib paths
- **Link Libraries**: 
- FFmpeg: avformat, avcodec, avutil, swresample, swscale
- SDL2: SDL2
- Windows: comdlg32 (for file dialog after running to select local files). The Linux version uses POSIX API (unistd)

## 📁 Project File Description

### 1. 🎵 AudioPlayer.cpp - Universal Audio Player
**Function**: Supports playback of various audio format files (such as MP3, FLAC, Opus, WAV, M4A, OGG, AAC, etc.)

**Core Features**:
- Supports Chinese path file selection
- Automatic audio resampling (unified output to 16-bit stereo)
- Real-time playback progress display
- Displays audio metadata (title, artist, album, sample rate, etc.)

**Usage**:
1. Run the program, a file selection dialog appears
2. Select an audio file
3. Automatically plays and displays progress bar
4. Automatically exits after playback

**Technical Highlights**:
- Uses FFmpeg to decode various audio formats
- Uses Swresample for audio format conversion
- Uses SDL for audio playback via queue mode

---

### 2. 🎛️ PCM_AudioPlayer.cpp - Raw PCM Audio Player
**Function**: Plays raw PCM audio files (44.1kHz, 16-bit, stereo)

**Core Features**:
- Directly reads PCM file into memory
- Uses SDL queue for audio data
- Simple file selection interface

**Usage**:
1. Run the program, select a .pcm file
2. Program automatically plays (Note: PCM file must be 44.1kHz / 16-bit / stereo format)
3. Automatically exits after playback

**Notes**:
- ⚠️ Only supports fixed parameters: 44.1kHz sample rate, 16-bit depth, stereo
- If the PCM file parameters don't match, playback speed may be abnormal or noise may occur

---

### 3. 🖼️ SDL_Pics.cpp - SDL2 Graphics Drawing Example
**Function**: Demonstrates SDL2 basic drawing functions

**Core Features**:
- Creates borderless window
- Draws filled rectangles (large blue rectangle, small cyan rectangle)
- Draws white triangle lines
- Red background

**Usage**:
1. Run the program, displays SDL graphics window
2. Window automatically closes after 5 seconds

**Display Effect**:
- Background: Red
- Large blue rectangle: Position (50,50), size 300x200
- White triangle: Closed line formed by four points
- Small cyan rectangle: Position (400,300), size 100x100

---

### 4. 🎬 VideoPlayer.cpp - Video Player
**Function**: Plays various video format files (MP4, MKV, AVI, FLV, MOV, etc.)

**Core Features**:
- Supports Chinese path file selection
- Automatic audio/video synchronization
- Adjustable window size (image auto-stretches)
- YUV420P format rendering (ensures compatibility)

**Usage**:
1. Run the program, select a video file
2. Video automatically plays
3. Automatically exits when window is closed or playback finishes

**Technical Highlights**:
- Uses FFmpeg to decode video frames
- Uses Swscale to convert various pixel formats to YUV420P
- Uses SDL texture for video rendering
- PTS (Presentation Time Stamp) based audio/video synchronization

---

### 5. 🎯 Audio_Video_Player.cpp - Multi-threaded Audio/Video Synchronized Player
**Function**: Professional-grade audio/video player based on **multi-threaded decoding + queue buffering + audio-master synchronization**, supporting audio/video file playback. Pure audio files play without window.

**Core Features**:
- ✅ **Multi-threaded Architecture**: Demux thread, audio decoding thread, and video decoding thread run independently for efficient CPU utilization
- ✅ **Thread-safe Queues**: Custom `SafeQueue` template for lock-free data transfer
- ✅ **Intelligent Stream Recognition**: Automatically excludes cover image streams (e.g., MP3 album art); no video window for pure audio files
- ✅ **Precise Synchronization**: Uses audio clock as reference, dynamically adjusts video rendering timing (supports early waiting, late dropping)
- ✅ **Real-time Progress Display**: Shows current playback progress in console (min:sec format)
- ✅ **Graceful Exit**: Supports exit by window closing, automatically waits for audio playback to finish before ending
- ✅ **Queue Mode Audio Output**: No callback needed; directly pushes data with `SDL_QueueAudio` for simple and reliable operation

**Data Flow Architecture**: 
File → [Demux Thread] → Audio Packet Queue → [Audio Decode Thread] → SDL_QueueAudio → Audio Device
                   → Video Packet Queue → [Video Decode Thread] → Video Frame Queue → [Main Thread Render] → Window

**Technical Highlights**:
- Uses FFmpeg 7.0 API (`avcodec_send_packet` / `avcodec_receive_frame`)
- Audio resampled to S16 stereo, video uniformly converted to YUV420P
- Audio clock calculation: `(total pushed samples - samples remaining in SDL internal queue) / sample rate`
- Sync threshold: Wait if video >100ms ahead, drop if >300ms behind
- Perfectly handles audio files with embedded cover art (e.g., MP3, FLAC) without black window
- Integrates filter library to support rotation and scaling adaptation for videos of different resolutions and orientations

**Usage**:
1. Run the program, select media file (audio or video)
2. For pure audio files, no window appears; console shows playback progress
3. For video files, window appears and starts playback with synchronized audio
4. Automatically exits after playback, or click window close button to exit

**Dual Version**: Recently updated WSL version of audio/video synchronized player with all features above. Cross-platform adaptation requires only the following modifications:
1. Remove Windows version headers `<windows.h>` and `<commdlg.h>`, add WSL-required headers `<sys/stat.h>`, `<unistd.h>`, and `<limits.h>`
2. Windows version uses graphical file dialog with ANSI to UTF-8 conversion; WSL version maintains command-line interactive input, with default UTF-8 requiring no conversion
3. WSL version adds explicit constructor to `struct VideoFrame` to resolve potential GCC strict compilation issues
4. Core FFmpeg logic 100% reusable, platform-independent; UI layer fully compatible via SDL2 graphics rendering; uses POSIX standard APIs to replace Windows-specific APIs
5. Unified CMake management, automatic dependency detection via pkg-config, fully demonstrating cross-platform capabilities of FFmpeg + SDL2 combination

---

### 6. ⚡ HW_Audio_Video_Player.cpp - Hardware-Accelerated Multi-threaded Audio/Video Synchronized Player
**Function**: Based on **Audio_Video_Player.cpp**, adds **DXVA2 hardware decoding support**, significantly reducing CPU usage, especially suitable for smooth playback of 4K/8K high-bitrate videos. Automatically falls back to software decoding when hardware acceleration is unavailable.

**Core Features**:
- ✅ **DXVA2 Hardware Acceleration**: Automatically detects and enables DXVA2, offloading decoding tasks to GPU, significantly reducing CPU load
- ✅ **Multi-threaded Architecture**: Inherits efficient multi-threaded design from original (demux, audio decode, video decode independent threads)
- ✅ **Bounded Queues with Backpressure**: All queues have maximum capacity (audio packets 100, video packets 50, video frames 10) to prevent memory explosion
- ✅ **Intelligent Memory Management**: Fixed memory leaks in original version (unreleased AVPacket and AVFrame), stable memory usage during long-term operation
- ✅ **Automatic Fallback**: If hardware acceleration initialization fails (e.g., graphics card doesn't support DXVA2), automatically switches to software decoding, ensuring usability
- ✅ **Precise Audio/Video Synchronization**: Maintains audio-master sync strategy, supports early waiting and late dropping
- ✅ **Pure Audio Support**: Also works for pure audio files, no window appears

**Data Flow Architecture** (identical to software version):
File → [Demux Thread] → Audio Packet Queue → [Audio Decode Thread] → SDL_QueueAudio → Audio Device
                   → Video Packet Queue → [Video Decode Thread (Hardware Accelerated)] → Video Frame Queue → [Main Thread Render] → Window

![Hardware Acceleration Process](Pictures/HW_accelerate.png)

**Technical Highlights**:
- **Hardware Acceleration Initialization**: Iterates through decoder's `AVCodecHWConfig`, selects DXVA2 device type, creates hardware device context and associates with decoder
- **Hardware Frame Download**: Uses `av_hwframe_transfer_data()` to download frames from GPU to system memory; prioritizes downloading as YUV420P; if fails, attempts NV12 and converts
- **Persistent Download Frames**: Pre-allocates two download frames (YUV420P and NV12) to reduce frequent allocation, reused in video decoding thread
- **Memory Optimization**: Uses bounded queues with backpressure mechanism; producers automatically block when queue is full to prevent infinite accumulation; all `AVPacket` and `AVFrame` properly released
- **Compatibility**: Output remains YUV420P, consistent with SDL texture format, requiring no rendering code modifications

![Hardware Decoding Process](Pictures/Decode.png)

**Performance Comparison** (tested with 4K 60fps 70Mbps high-stress video):
- Software decoding version: CPU usage ≈ 30%, memory ≈ 650MB
- Hardware acceleration version: CPU usage ≈ 7%, memory ≈ 700MB (approximately 50MB increase for download frames, but gains 4x CPU performance improvement)

**Usage**:
1. Run the program, select media file (audio or video)
2. Program automatically detects hardware acceleration capability; if supported, enables DXVA2, console outputs `Hardware acceleration (DXVA2) enabled.`
3. If hardware acceleration unavailable, automatically uses software decoding, outputs prompt and continues decoding/playback
4. Playback experience identical to software version, but with significantly reduced CPU usage, capable of handling higher-load video playback tasks

**Dual Version**: The latest version of **HW_Audio_Video_Player.cpp** adds filter library support for rotation and scaling adaptation for videos of different resolutions and orientations. Additionally, it reduces thread count to improve single-thread efficiency, and optimizes memory management and fallback mechanisms. Memory usage when playing the same high-stress video is only half of the previous version!

---

## ⚙️ VS Code Configuration Description

The project includes the `.vscode` configuration folder:
- `c_cpp_properties.json` - C/C++ plugin configuration (includes library paths)
- `launch.json` - Debugging configuration
- `settings.json` - Editor settings
- `tasks.json` - Build task configuration

## 🔧 Frequently Asked Questions

### Q1: Can't find FFmpeg/SDL2 headers during compilation
**A**: Check if include paths in `.vscode/c_cpp_properties.json` are correct

### Q2: Missing DLL error at runtime
**A**: Ensure all DLL files are in the same directory as the executable or added to system PATH

### Q3: PCM playback speed incorrect or noise
**A**: PCM_AudioPlayer.cpp only supports 44.1kHz / 16-bit / stereo format; check PCM file parameters

### Q4: Audio file won't play
**A**: AudioPlayer.cpp supports mainstream formats, but some specially encoded files may require additional codecs

### Q5: High CPU usage when running multi-threaded player?
**A**: Normal phenomenon; multi-threaded decoding fully utilizes CPU. `SDL_Delay(10)` already added in main loop to prevent idle spinning; if still too high, increase delay appropriately

### Q6: Hardware acceleration version uses more memory than software version?
**A**: Hardware acceleration version pre-allocates two download frames (about 20MB) for retrieving data from GPU, which is normal. Compared to software version, hardware acceleration version reduces CPU usage by over 70%, making it worthwhile for high-resolution videos.

### Q7: What if hardware acceleration fails to enable?
**A**: Program automatically falls back to software decoding without affecting playback. Check if graphics card supports DXVA2, or update graphics drivers.

## 📝 Update Log

### v1.0.0 (2026.2.28)
- Implemented four independent functional modules
- Added Chinese path file selection support
- Added detailed metadata display

### v1.1.0 (2026.3.3)
- **New**: `Audio_Video_Player.cpp` multi-threaded audio/video synchronized player
- **Optimization**: Improved stream recognition logic; pure audio files no longer pop up video window
- **Documentation**: Updated README with detailed multi-threaded player architecture

### v1.2.0 (2026.3.6)
- **New**: `HW_Audio_Video_Player.cpp` hardware-accelerated multi-threaded audio/video synchronized player (DXVA2 support)
- **Optimization**: Software/hardware version code with bounded queues for all queues, introducing backpressure mechanism, completely solving memory inflation issues
- **Fix**: Memory leaks in demux thread and decode threads (unreleased AVPacket and AVFrame)
- **Performance**: Hardware acceleration version reduces CPU usage by about 70% when playing 4K high-bitrate videos
- **Documentation**: Added detailed description and performance comparison data for hardware acceleration version

### v1.3.0 (2026.3.8)
- **New**: `Audio_Video_Player.cpp` and `HW_Audio_Video_Player.cpp` both integrate FFmpeg filter library. Retained original hardware-accelerated player code without filter library as `HW.cpp` for future differential learning and performance comparison.
- **Optimization**: Both software/hardware version player codes add automatic rotation and scaling adaptation for mobile phone portrait videos through filters
- **Performance**: `HW_Audio_Video_Player.cpp` memory usage reduced from 800MB to 350MB when playing 4K high-bitrate videos
- **Documentation**: Added Zhihu blog article https://zhuanlan.zhihu.com/p/2013200050549958415 detailing hardware acceleration principles, processes, and code applications, as well as video rotation and scaling adaptation after filter library integration

### v1.4.0 (2026.3.13)
- **New**: Modified and optimized the existing `Audio_Video_Player(WSL).cpp`, added a WSL/Linux platform runnable version. Achieves all functions implemented on Windows platform, and inputs media file paths in WSL via CLI for decoding and playback.

Future attempts will integrate with live555 and other transmission-related servers to expand the project's practicality and versatility.

## 📄 License

This project is for learning and communication purposes only. FFmpeg and SDL2 follow their respective open-source licenses.

## 👤 Author

- GitHub: [@jerryyang1208](https://github.com/jerryyang1208)

## ⭐ Acknowledgements

- [FFmpeg](https://ffmpeg.org/) community
- [SDL](https://www.libsdl.org/) development team