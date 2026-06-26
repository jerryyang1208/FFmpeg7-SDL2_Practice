<div align="right">
  <a href="README.md">中文</a>
</div>

# FFmpeg7-SDL2 Multimedia Practice Project

![FFmpeg](https://img.shields.io/badge/FFmpeg-7.0.2-green)
![SDL2](https://img.shields.io/badge/SDL2-2.30.6-blue)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)

A multimedia player practice project based on FFmpeg 7.0.2 and SDL2 2.30.6, containing audio playback, PCM playback, video playback, SDL graphics rendering, multi-threaded audio/video synchronized player, and hardware-accelerated decoding player.

## 📦 Dependencies
- **[FFmpeg 7.0.2](https://ffmpeg.org/)** - Core audio/video decoding library
- **[SDL2 2.30.6](https://www.libsdl.org/)** - Cross-platform multimedia rendering library

## 🚀 Quick Start

### Environment Setup
1. Download the required library packages from [Releases](https://github.com/jerryyang1208/FFmpeg7-SDL2_Practice/releases)

2. Extract to the project root directory, ensuring the directory structure is as follows:
<pre>
ffmpeg+SDL2/
├── .vscode/                      # VS Code configuration files
│   ├── c_cpp_properties.json     # C/C++ plugin configuration
│   ├── launch.json               # Debug configuration
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
├── common.h                      # Common header file (type definitions, constants, function declarations)
├── common.cpp                    # Common implementation file (global context, queues, frame pool, helper functions)
├── AudioPlayer.cpp               # Audio player source code
├── PCM_AudioPlayer.cpp           # PCM audio player source code
├── SDL_Pics.cpp                  # SDL graphics rendering source code
├── VideoPlayer.cpp               # Video player source code
├── Audio_Video_Player.cpp        # Multi-threaded audio/video synchronized player (software decoding)
├── HW_Audio_Video_Player.cpp     # Hardware-accelerated audio/video player (D3D11VA/DXVA2)
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

3. Ensure DLL files are in the same directory as the executable or in the system PATH

### Compilation Environment
- **Compiler**: MSVC (Visual Studio) or MinGW (VS Code)
- **Standard**: C++17
- **Include Directories**: Need to add FFmpeg and SDL2 include paths
- **Library Directories**: Need to add FFmpeg and SDL2 lib paths
- **Link Libraries**: 
  - FFmpeg: avformat, avcodec, avutil, swresample, swscale, avfilter, avdevice
  - SDL2: SDL2, SDL2main
  - Windows: comdlg32

## 📁 Project File Descriptions

### 1. 🎵 AudioPlayer.cpp - Universal Audio Player
**Function**: Supports playback of various audio file formats (such as MP3, FLAC, Opus, WAV, M4A, OGG, AAC, etc.)

**Core Features**:
- Chinese path file selection support
- Automatic audio resampling (unified output to 16-bit stereo)
- Real-time playback progress display
- Audio metadata display (title, artist, album, sample rate, etc.)

**Usage**:
1. Run the program, file selection dialog pops up
2. Select audio file
3. Automatic playback with progress bar display
4. Automatically exits after playback completes

---

### 2. 🎛️ PCM_AudioPlayer.cpp - PCM Raw Audio Player
**Function**: Play raw PCM audio files (44.1kHz, 16-bit, stereo)

**Core Features**:
- Directly read PCM file into memory
- Use SDL queue for audio data
- Simple file selection interface

**Usage**:
1. Run the program, select a .pcm file
2. Program automatically plays (Note: PCM file must be 44.1kHz / 16-bit / stereo format)
3. Automatically exits after playback completes

**Important Notes**:
- ⚠️ Only supports fixed parameters: 44.1kHz sample rate, 16-bit depth, stereo

---

### 3. 🖼️ SDL_Pics.cpp - SDL2 Graphics Drawing Example
**Function**: Demonstrates basic drawing functions of SDL2

**Core Features**:
- Create borderless window
- Draw filled rectangles (large blue rectangle, small cyan rectangle)
- Draw white triangle outline
- Red background

**Usage**:
1. Run the program, SDL graphics window displays
2. Window automatically closes after 5 seconds

---

### 4. 🎬 VideoPlayer.cpp - Video Player
**Function**: Play various video file formats (MP4, MKV, AVI, FLV, MOV, etc.)

**Core Features**:
- Chinese path file selection support
- Automatic audio/video synchronization
- Adjustable window size (automatic scaling)
- YUV420P format rendering

**Usage**:
1. Run the program, select video file
2. Video automatically plays
3. Close window or automatically exit after playback completes

---

### 5. 🎯 Audio_Video_Player.cpp - Multi-threaded Audio/Video Synchronized Player (Software Decoding)
**Function**: Professional-grade audio/video player based on **multi-threaded decoding + queue buffering + audio-based synchronization**, supporting audio/video file playback, with no window for pure audio files.

**Core Features**:
- ✅ **Multi-stage Pipeline Architecture**: Independent demuxing thread, audio decoding thread, video decoding thread, decoupling IO, decoding and rendering through producer-consumer model
- ✅ **Thread-safe Queue**: Bounded blocking queue using custom `SafeQueue` template, supporting backpressure control to prevent memory explosion
- ✅ **Intelligent Resource Management**: RAII with `std::unique_ptr` custom deleters encapsulating FFmpeg/SDL resources for automatic lifecycle management
- ✅ **Bidirectional Synchronization Strategy**: Audio clock as master, video ahead or behind by >100ms drops frame for fast catch-up, ahead by <20ms micro-sleep for precise waiting
- ✅ **Frame Pool Reuse**: `FramePool` pre-allocates and reuses YUV420P frame buffers, avoiding per-frame allocation and redundant copies
- ✅ **Dynamic Filter Support**: libavfilter dynamic filter graph for display matrix-based video rotation
- ✅ **Smart Stream Recognition**: Automatically excludes cover art streams, no video window for pure audio playback
- ✅ **Real-time Progress Display**: Shows current playback progress in console (minutes:seconds format)
- ✅ **Window Scaling Adaptation**: Dynamic Viewport cropping on window resize, maintaining aspect ratio

**Data Flow Architecture**:
```
File → [Demux Thread] → Audio Packet Queue(60) → [Audio Decode Thread] → SDL_QueueAudio → Audio Device
                     → Video Packet Queue(20) → [Video Decode Thread] → Video Frame Queue(10) → [Main Thread Render] → Window
```

**Technical Highlights**:
- Using FFmpeg 7.0 API (`avcodec_send_packet` / `avcodec_receive_frame`)
- Audio resampling to S16 stereo, video uniformly converted to YUV420P
- Audio clock calculation: `total pushed samples - SDL internal queue remaining samples` / sample rate
- Video decoder thread count: 4 (balanced performance vs resource usage)
- `PlayerContext` struct for unified global player state management

**Usage**:
1. Run the program, select media file (audio or video)
2. If pure audio file, no window pops up, console displays playback progress
3. If video file, window pops up and playback starts, video and audio synchronized
4. Automatically exits after playback completes, or click window close button to exit

---

### 6. ⚡ HW_Audio_Video_Player.cpp - Hardware-Accelerated Multi-threaded Audio/Video Synchronized Player
**Function**: Based on **Audio_Video_Player.cpp**, adds **D3D11VA/DXVA2 hardware decoding support**, significantly reducing CPU usage, especially suitable for smooth playback of 4K/8K high-bitrate videos. Automatically falls back to software decoding when hardware acceleration is unavailable.

**Core Features**:
- ✅ **D3D11VA/DXVA2 Hardware Acceleration**: Prioritizes D3D11VA, falls back to DXVA2 if unavailable, offloading decoding tasks to GPU, drastically reducing CPU load
- ✅ **Multi-stage Pipeline Architecture**: Inherits efficient multi-threaded design (independent demux, audio decode, video decode threads)
- ✅ **Intelligent Resource Management**: All FFmpeg/SDL resources use RAII encapsulation, with atomic flags and queue abort mechanism ensuring thread-safe exit
- ✅ **Bidirectional Adaptive Synchronization**: Maintains audio clock-based sync strategy, video ahead or behind by >100ms drops frame, ahead by <20ms micro-sleep
- ✅ **Frame Pool Reuse**: Same `FramePool` design as software version, pre-allocating YUV420P frame buffers
- ✅ **Dynamic Filter Support**: Display matrix-based video rotation real-time processing
- ✅ **Automatic Fallback**: If hardware acceleration initialization fails, automatically switches to software decoding
- ✅ **Pure Audio Support**: Also suitable for pure audio files, no window pops up

**Data Flow Architecture**:
```
File → [Demux Thread] → Audio Packet Queue(60) → [Audio Decode Thread] → SDL_QueueAudio → Audio Device
                     → Video Packet Queue(20) → [Video Decode Thread(HW)] → GPU Frame → av_hwframe_transfer_data → System Memory Frame → Video Frame Queue(10) → [Main Thread Render] → Window
```

**Technical Highlights**:
- **Hardware Acceleration Initialization**: Iterates through decoder's `AVCodecHWConfig`, prioritizes D3D11VA, then DXVA2, creates hardware device context and associates with decoder
- **Hardware Frame Transfer**: Uses `av_hwframe_transfer_data()` to transfer NV12 frames from GPU to system memory, then converts to YUV420P
- **Automatic Fallback**: If hardware acceleration unavailable, automatically uses software decoding
- **Video decoder thread count**: 4

**Performance Comparison** (tested with 4K 60fps high-bitrate video):
- Software decoding version: CPU usage ≈ 30%
- Hardware acceleration version: CPU usage ≈ 7% (≈ 77% reduction)

**Usage**:
1. Run the program, select media file (audio or video)
2. Program automatically detects hardware acceleration capability, if supported enables (console outputs `HW acceleration: d3d11va` or `dxva2`)
3. If hardware acceleration unavailable, automatically uses software decoding, outputs message and continues playback
4. Playback experience identical to software version, but CPU usage significantly reduced

---

## ⚙️ VS Code Configuration Description

Project includes `.vscode` configuration folder:
- `c_cpp_properties.json` - C/C++ plugin configuration (includes library paths)
- `launch.json` - Debug configuration
- `settings.json` - Editor settings
- `tasks.json` - Build task configuration

## 🔧 Frequently Asked Questions

### Q1: Cannot find FFmpeg/SDL2 header files during compilation
**A**: Check if include paths in `.vscode/c_cpp_properties.json` are correct

### Q2: Missing DLL errors at runtime
**A**: Ensure all DLL files are in the same directory as the executable, or added to system PATH

### Q3: PCM playback speed wrong or noise
**A**: PCM_AudioPlayer.cpp only supports 44.1kHz / 16-bit / stereo format, please check PCM file parameters

### Q4: Audio file won't play
**A**: AudioPlayer.cpp supports mainstream formats, but some special encoding files may require additional decoders

### Q5: Multi-threaded player high CPU usage at runtime?
**A**: Normal phenomenon, multi-threaded decoding fully utilizes CPU. Main loop has `SDL_Delay(5)` to avoid spinning

### Q6: What if hardware acceleration enable fails?
**A**: Program automatically falls back to software decoding, doesn't affect playback. Check if graphics card supports D3D11VA/DXVA2, or update graphics driver

## 📝 Changelog

### v1.0.0 (2026.2.28)
- Implemented four independent functional modules
- Chinese path file selection support
- Added detailed metadata display

### v1.1.0 (2026.3.3)
- **New**: `Audio_Video_Player.cpp` multi-threaded audio/video synchronized player
- **Optimization**: Improved stream recognition logic, pure audio files no longer pop up video window

### v1.2.0 (2026.3.6)
- **New**: `HW_Audio_Video_Player.cpp` hardware-accelerated multi-threaded audio/video synchronized player
- **Optimization**: All queues use bounded design with backpressure mechanism, solving memory explosion issue

### v1.3.0 (2026.3.8)
- **New**: Introduced FFmpeg filter library, supporting video rotation and scaling adaptation
- **Performance**: Hardware acceleration version memory usage optimization

### v1.4.0 (2026.6.26)
- **Refactor**: Extracted common code to `common.h` / `common.cpp`, using `PlayerContext` struct for unified global state management
- **Optimization**: RAII smart pointers encapsulating all FFmpeg/SDL resources
- **Optimization**: `FramePool` frame pool reuse mechanism
- **Optimization**: Synchronization strategy upgraded to bidirectional adaptive frame dropping (both ahead and behind)
- **Optimization**: Hardware acceleration prioritizes D3D11VA, falls back to DXVA2
- **Fix**: Frame pool recycle buffer reallocation issue

## 📄 License

This project is for learning and communication purposes only. FFmpeg and SDL2 follow their respective open source licenses.

## 👤 Author

- GitHub: [@jerryyang1208](https://github.com/jerryyang1208)

## ⭐ Acknowledgements

- [FFmpeg](https://ffmpeg.org/) community
- [SDL](https://www.libsdl.org/) development team
