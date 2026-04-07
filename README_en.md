<div align="right">
  <a href="README.md">中文</a>
</div>

<div align="center">

# FFmpeg7-SDL2 Multimedia Practice Project

![FFmpeg](https://img.shields.io/badge/FFmpeg-7.0.2-green)
![SDL2](https://img.shields.io/badge/SDL2-2.30.6-blue)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)

</div>

A multimedia player practice project based on FFmpeg 7.0.2 and SDL2 2.30.6, focusing on high-performance audio and video playback. The project has been refactored to extract core functionality into common modules, implementing both software and hardware decoding versions of audio and video players that support smooth playback of 4K high-frame-rate videos.

The project adopts a modular design, extracting common code into common.h and common.cpp to improve code reusability and maintainability. Both software and hardware versions have basic playback functionality, media information display, and rotation and scaling adaptation for videos of different resolutions and orientations.

Blog: https://zhuanlan.zhihu.com/p/700478133

## 📦 Dependencies
- **[FFmpeg 7.0.2](https://ffmpeg.org/)** - Core audio and video decoding library
- **[SDL2 2.30.6](https://www.libsdl.org/)** - Cross-platform multimedia rendering library

## 🚀 Quick Start

### Environment Configuration
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
├── README.md                     # Project documentation
│
├── common.h                      # Common header file (shared data structures and function declarations)
├── common.cpp                    # Common implementation file (shared functionality implementation)
├── Audio_Video_Player.cpp        # Software decoding version of audio and video player source code
├── HW_Audio_Video_Player.cpp     # Hardware acceleration version of audio and video player source code
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

3. Ensure the DLL files are in the same directory as the executable or in the system PATH, as these dynamic link libraries are required for runtime

### Compilation Environment
- **Compiler**: MSVC (Visual Studio) or MinGW (VS Code), Linux version will be compiled with GCC/G++ (WSL VS Code) in future updates
- **Standard**: C++11/14
- **Include Directories**: Need to add FFmpeg and SDL2 include paths
- **Library Directories**: Need to add FFmpeg and SDL2 lib paths
- **Linked Libraries**: 
- FFmpeg: avformat, avcodec, avutil, swresample, swscale
- SDL2: SDL2
- Windows: comdlg32 (for file dialog when running, select local files), Linux version will use POSIX API (unistd) in future updates

## 📁 Project File Description

### 1. 📦 common.h - Common Header File
**Function**: Defines shared data structures, constants, and function declarations, providing a unified interface for both software and hardware versions.

**Core Content**:
- **Thread-safe Queue**: `SafeQueue` template class, implementing thread-safe data transfer
- **Video Frame Structure**: `VideoFrame` struct, used to store video frame data and timestamps
- **Common Constants**: Configuration parameters such as queue size and synchronization thresholds
- **Global Variable Declarations**: FFmpeg core contexts, SDL devices, threads, queues, etc.
- **Helper Function Declarations**: Common functions like file selection and string conversion

**Key Features**:
- Provides a unified queue implementation to ensure thread safety
- Defines standardized data structures to simplify code reuse
- Centralizes configuration parameters for easy unified adjustment

---

### 2. 🔧 common.cpp - Common Implementation File
**Function**: Implements helper functions declared in common.h, providing general functionality support for the players.

**Core Content**:
- **File Selection**: `SelectFileDialog` function, implementing graphical file selection interface
- **String Conversion**: `AnsiToUtf8` function, handling Chinese path encoding issues
- **Global Variable Definitions**: Initializing common global variables

**Technical Points**:
- Uses Windows API to implement file selection dialog
- Handles ANSI to UTF-8 encoding conversion to ensure Chinese paths work correctly
- Provides unified helper functions to reduce code duplication

---

### 3. 🎯 Audio_Video_Player.cpp - Software Decoding Version
**Function**: Multi-threaded audio and video synchronous player based on software decoding, supporting audio and video file playback, no window for pure audio files.

**Core Features**:
- ✅ **Multi-threaded Architecture**: Demuxing thread, audio decoding thread, and video decoding thread run independently
- ✅ **Thread-safe Queue**: Uses `SafeQueue` to implement lock-free data transfer
- ✅ **Intelligent Stream Recognition**: Automatically excludes cover image streams, no video window pops up for pure audio playback
- ✅ **Precise Synchronization**: Uses audio clock as the baseline, dynamically adjusts video rendering timing
- ✅ **Real-time Progress Display**: Shows current playback progress in the console
- ✅ **Graceful Exit**: Supports window close exit, automatically waits for audio playback to complete
- ✅ **Filter Support**: Rotates and scales videos of different resolutions and orientations

**Data Flow Architecture**:
File → [Demuxing Thread] → Audio Packet Queue → [Audio Decoding Thread] → SDL_QueueAudio → Audio Device
                   → Video Packet Queue → [Video Decoding Thread] → Video Frame Queue → [Main Thread Rendering] → Window

**Technical Points**:
- Uses FFmpeg 7.0 API for decoding
- Audio resampling to S16 stereo, video unified conversion to YUV420P
- Audio clock synchronization to ensure audio-video synchronization
- Queue mode audio output, simple and reliable implementation

---

### 4. ⚡ HW_Audio_Video_Player.cpp - Hardware Acceleration Version
**Function**: Based on the software version, adds DXVA2 hardware decoding support, significantly reducing CPU usage, suitable for smooth playback of 4K/8K high-bitrate videos.

**Core Features**:
- ✅ **DXVA2 Hardware Acceleration**: Automatically detects and enables DXVA2, offloading decoding tasks to GPU
- ✅ **Multi-threaded Architecture**: Inherits the efficient multi-threaded design of the software version
- ✅ **Intelligent Memory Management**: Uses bounded queues and backpressure mechanism to prevent memory explosion
- ✅ **Automatic Fallback**: If hardware acceleration is unavailable, automatically switches to software decoding
- ✅ **Precise Audio-Video Synchronization**: Continues to use audio clock as the main synchronization strategy
- ✅ **Filter Support**: Rotates and scales videos of different resolutions and orientations

**Technical Points**:
- **Hardware Acceleration Initialization**: Creates DXVA2 hardware device context and associates it with the decoder
- **Hardware Frame Download**: Downloads frames from GPU to system memory, supports YUV420P and NV12 formats
- **Memory Optimization**: Reuses download frames to reduce memory allocation
- **Performance Optimization**: Uses SWS_FAST_BILINEAR algorithm to improve scaling speed

**Performance Comparison** (tested with 4K 60fps 70Mbps high-pressure video):
- Software decoding version: CPU usage ≈ 30%, memory ≈ 350MB
- Hardware acceleration version: CPU usage ≈ 7%, memory ≈ 400MB (increase of about 50MB for download frames, but achieves 4x CPU performance improvement)

**Usage Method**:
1. **Compilation**: Open the project in VS Code, select the file to compile (Audio_Video_Player.cpp or HW_Audio_Video_Player.cpp), then run the build task
2. **Running**: After compilation is complete, run the generated executable file
3. **File Selection**: The program will pop up a file selection dialog, select the media file to play
4. **Playback Control**:
   - Video file: A playback window will pop up, displaying the video frame
   - Audio file: Playback progress will be displayed in the console, no window pops up
5. **Exit**: Automatically exit after playback is complete, or click the window close button to exit

---

## ⚙️ VS Code Configuration Description

The project includes a `.vscode` configuration folder:
- `c_cpp_properties.json` - C/C++ plugin configuration (including library paths)
- `launch.json` - Debug configuration
- `settings.json` - Editor settings
- `tasks.json` - Build task configuration

## 🔧 Common Issues

### Q1: Cannot find FFmpeg/SDL2 header files during compilation
**A**: Check if the include paths in `.vscode/c_cpp_properties.json` are correct

### Q2: Missing DLL error during runtime
**A**: Ensure all DLL files are in the same directory as the executable, or have been added to the system PATH

### Q3: PCM playback speed is incorrect or has noise
**A**: PCM_AudioPlayer.cpp only supports 44.1kHz / 16-bit / stereo format, please check PCM file parameters

### Q4: Audio files cannot be played
**A**: AudioPlayer.cpp supports mainstream formats, but some specially encoded files may require additional decoders

### Q5: High CPU usage when running multi-threaded player?
**A**: Normal phenomenon, multi-threaded decoding will fully utilize CPU. `SDL_Delay(10)` has been added in the main loop to avoid idling, if it's still too high, you can increase the delay appropriately

### Q6: Hardware acceleration version uses more memory than software version?
**A**: The hardware acceleration version pre-allocates two download frames (about 20MB) for getting data from GPU, which is normal. Compared to the software version, the hardware acceleration version can reduce CPU usage by more than 70%, which is worth it for high-resolution videos.

### Q7: What to do if hardware acceleration fails to enable?
**A**: The program will automatically fall back to software decoding, which does not affect playback. You can check if your graphics card supports DXVA2, or update your graphics card driver.

## 📝 Update Log

### v2.0.0 (2026.4.7)
- **Refactoring**: Project architecture refactoring, extracting common code into common.h and common.cpp
- **Optimization**: Hardware acceleration version uses SWS_FAST_BILINEAR algorithm to improve scaling speed
- **Optimization**: Further memory management optimization, reducing memory usage to 350MB when playing 4K high-bitrate videos
- **Fix**: Configuration file optimization, unifying path separators to Windows style
- **Documentation**: Update README.md, detailing the refactored project structure and functionality

### v1.4.0 (2026.3.13)
- **New**: WSL/Linux platform support with `Audio_Video_Player(WSL).cpp`
- **Optimization**: Cross-platform compatibility, using POSIX standard API instead of Windows-specific API

### v1.3.0 (2026.3.8)
- **New**: FFmpeg filter library support, implementing video rotation and scaling adaptation
- **Performance**: Hardware acceleration version memory usage reduced from 800MB to 350MB

### v1.2.0 (2026.3.6)
- **New**: `HW_Audio_Video_Player.cpp` hardware-accelerated multi-threaded audio and video synchronous player
- **Optimization**: Bounded queues and backpressure mechanism to solve memory explosion problem
- **Performance**: Hardware acceleration version CPU usage reduced by about 70%

### v1.1.0 (2026.3.3)
- **New**: `Audio_Video_Player.cpp` multi-threaded audio and video synchronous player
- **Optimization**: Improved stream recognition logic, no video window pops up for pure audio files

### v1.0.0 (2026.2.28)
- Implemented four independent functional modules
- Supported Chinese path file selection
- Added detailed metadata display

Future plans include linking with live555 and other transmission-related servers to expand the project's practicality and versatility.

## 📄 License

This project is for learning and exchange purposes only. FFmpeg and SDL2 follow their respective open-source licenses.

## 👤 Author

- GitHub: [@jerryyang1208](https://github.com/jerryyang1208)

## ⭐ Acknowledgments

- [FFmpeg](https://ffmpeg.org/) community
- [SDL](https://www.libsdl.org/) development team