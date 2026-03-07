<div align="right">
  <a href="README.md">中文</a>
</div>

# FFmpeg7-SDL2 Multimedia Practice Project

![FFmpeg](https://img.shields.io/badge/FFmpeg-7.0.2-green)
![SDL2](https://img.shields.io/badge/SDL2-2.30.6-blue)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)

A multimedia player practice project based on FFmpeg 7.0.2 and SDL2 2.30.6, including audio playback, PCM playback, video playback, SDL graphics drawing, multithreaded audio-video synchronized player, and a hardware-accelerated audio-video player — six independent examples in total.
The newly added hardware-accelerated multithreaded audio-video synchronized player supports DXVA2 hardware decoding, significantly reducing CPU usage and smoothly playing 4K high-frame-rate high-bitrate videos!

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
├── Audio_Video_Player.cpp        # Multithreaded audio-video synchronized player source code
├── HW_Audio_Video_Player.cpp     # Hardware-accelerated audio-video player source code
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

3. Ensure the DLL files are in the same directory as the executable or in the system PATH; these dynamic link libraries are required at runtime.

### Compilation Environment
- **Compiler**: MSVC (Visual Studio) or MinGW (VS Code)
- **Standard**: C++11/14
- **Include directories**: Must add FFmpeg and SDL2 include paths
- **Library directories**: Must add FFmpeg and SDL2 lib paths
- **Link libraries**:
  - FFmpeg: avformat, avcodec, avutil, swresample, swscale
  - SDL2: SDL2
  - Windows: comdlg32 (for file dialog after running to select local files)

## 📁 Project File Description

### 1. 🎵 AudioPlayer.cpp - General Audio Player
**Function**: Play audio files of various formats (e.g., MP3, FLAC, Opus, WAV, M4A, OGG, AAC, etc.)

**Core Features**:
- Support file selection with Chinese paths
- Automatic audio resampling (uniform output to 16-bit stereo)
- Real-time playback progress display
- Display audio metadata (title, artist, album, sample rate, etc.)

**Usage**:
1. Run the program, a file selection dialog will pop up
2. Select an audio file
3. Automatic playback with progress bar display
4. Automatically exit after playback finishes

**Technical Highlights**:
- Use FFmpeg to decode various audio formats
- Use Swresample for audio format conversion
- Use SDL to play audio via queue mode

---

### 2. 🎛️ PCM_AudioPlayer.cpp - Raw PCM Audio Player
**Function**: Play raw PCM audio files (44.1kHz, 16-bit, stereo)

**Core Features**:
- Directly read PCM file into memory
- Use SDL queue for audio data
- Simple file selection interface

**Usage**:
1. Run the program, select a .pcm file
2. Program plays automatically (Note: PCM file must be 44.1kHz / 16-bit / stereo format)
3. Automatically exit after playback finishes

**Notes**:
- ⚠️ Only supports fixed parameters: 44.1kHz sample rate, 16-bit depth, stereo
- If the PCM file parameters do not match, playback speed may be abnormal or noise may occur

---

### 3. 🖼️ SDL_Pics.cpp - SDL2 Graphics Drawing Example
**Function**: Demonstrate basic drawing functions of SDL2

**Core Features**:
- Create a borderless window
- Draw filled rectangles (large blue rectangle, small cyan rectangle)
- Draw white triangle lines
- Red background

**Usage**:
1. Run the program, an SDL graphics window will appear
2. Window closes automatically after 5 seconds

**Display Effects**:
- Background: Red
- Large blue rectangle: position (50,50), size 300x200
- White triangle: closed line composed of four points
- Small cyan rectangle: position (400,300), size 100x100

---

### 4. 🎬 VideoPlayer.cpp - Video Player
**Function**: Play video files of various formats (MP4, MKV, AVI, FLV, MOV, etc.)

**Core Features**:
- Support file selection with Chinese paths
- Automatic audio-video synchronization
- Resizable window (image automatically stretched)
- YUV420P format rendering (ensuring compatibility)

**Usage**:
1. Run the program, select a video file
2. Video plays automatically
3. Close window or automatically exit after playback finishes

**Technical Highlights**:
- Use FFmpeg to decode video frames
- Use Swscale to convert various pixel formats to YUV420P
- Use SDL texture to render video
- Audio-video synchronization based on PTS (Presentation Time Stamp)

---

### 5. 🎯 Audio_Video_Player.cpp - Multithreaded Audio-Video Synchronized Player (New ⭐)
**Function**: Professional-grade audio-video player based on **multithreaded decoding + queue buffering + audio-based synchronization**, supporting audio/video file playback; no window for pure audio files.

**Core Features**:
- ✅ **Multithreaded Architecture**: Demux thread, audio decoding thread, video decoding thread run independently, efficiently utilizing CPU
- ✅ **Thread-Safe Queue**: Custom `SafeQueue` template for lock-free data transfer
- ✅ **Intelligent Stream Detection**: Automatically excludes attached picture streams (e.g., MP3 album covers); no video window for pure audio playback
- ✅ **Precise Synchronization**: Based on audio clock, dynamically adjust video rendering timing (support early wait, late drop)
- ✅ **Real-Time Progress Display**: Show current playback progress (min:sec format) in console
- ✅ **Graceful Exit**: Support exit by closing window, automatically wait for audio playback to finish before ending
- ✅ **Queue Mode Audio Output**: No callback, directly push data with `SDL_QueueAudio`, simple and reliable

**Data Flow Architecture**:
File → [Demux Thread] → Audio Packet Queue → [Audio Decode Thread] → SDL_QueueAudio → Audio Device
                   → Video Packet Queue → [Video Decode Thread] → Video Frame Queue → [Main Thread Render] → Window

**Technical Highlights**:
- Use FFmpeg 7.0 API (`avcodec_send_packet` / `avcodec_receive_frame`)
- Audio resampled to S16 stereo, video uniformly converted to YUV420P
- Audio clock calculation: `total samples pushed - remaining samples in SDL internal queue` / sample rate
- Sync thresholds: video ahead >100ms wait, behind >300ms drop
- Perfectly supports audio files with covers (e.g., MP3, FLAC), no blank window

**Usage**:
1. Run the program, select a media file (audio or video)
2. If pure audio, no window appears; console shows playback progress
3. If video, window pops up and plays with synchronized audio
4. Automatically exit after playback finishes, or click the window close button to exit

---

### 6. ⚡ HW_Audio_Video_Player.cpp - Hardware-Accelerated Multithreaded Audio-Video Synchronized Player
**Function**: Based on **Audio_Video_Player.cpp**, adds **DXVA2 hardware decoding support**, significantly reducing CPU usage, especially suitable for smooth playback of 4K/8K high-bitrate videos. Automatically falls back to software decoding when hardware acceleration is unavailable.

**Core Features**:
- ✅ **DXVA2 Hardware Acceleration**: Automatically detects and enables DXVA2, offloading decoding tasks to GPU, greatly reducing CPU load
- ✅ **Multithreaded Architecture**: Inherits the efficient multithreaded design (demux, audio decode, video decode independent threads)
- ✅ **Bounded Queues & Backpressure Mechanism**: All queues have maximum capacity (audio packets 100, video packets 50, video frames 10) to prevent memory explosion
- ✅ **Intelligent Memory Management**: Fixed memory leaks in original version (unreleased AVPacket and AVFrame), stable memory usage over long runs
- ✅ **Automatic Fallback**: If hardware acceleration initialization fails (e.g., GPU does not support DXVA2), automatically switches to software decoding, ensuring usability
- ✅ **Precise Audio-Video Synchronization**: Retains audio-clock-based synchronization strategy, supporting early wait and late drop
- ✅ **Pure Audio Support**: Also applicable to pure audio files, no window pops up

**Data Flow Architecture** (same as software version):
File → [Demux Thread] → Audio Packet Queue → [Audio Decode Thread] → SDL_QueueAudio → Audio Device
                   → Video Packet Queue → [Video Decode Thread (Hardware Accelerated)] → Video Frame Queue → [Main Thread Render] → Window

**Technical Highlights**:
- **Hardware Acceleration Initialization**: Iterate through decoder's `AVCodecHWConfig`, select DXVA2 device type, create hardware device context and associate with decoder
- **Hardware Frame Download**: Use `av_hwframe_transfer_data()` to download frames from GPU to system memory; prioritize downloading as YUV420P, if failed try NV12 and convert
- **Persistent Download Frames**: Pre-allocate two download frames (YUV420P and NV12) in video decode thread to reduce frequent allocation
- **Memory Optimization**: Use bounded queues and backpressure mechanism, producers automatically block when queue is full, avoiding infinite accumulation; all `AVPacket` and `AVFrame` are properly freed
- **Compatibility**: Output remains YUV420P, consistent with SDL texture format, no modification to rendering code

**Performance Comparison** (tested with 4K 60fps 70Mbps high-stress video):
- Software decoding version: CPU usage ≈ 30%, memory ≈ 650MB
- Hardware-accelerated version: CPU usage ≈ 7%, memory ≈ 700MB (extra ~50MB for download frames, but gains 4x CPU performance improvement)

**Usage**:
1. Run the program, select a media file (audio or video)
2. Program automatically detects hardware acceleration capability; if supported, enables DXVA2, console outputs `Hardware acceleration (DXVA2) enabled.`
3. If hardware acceleration is unavailable, automatically uses software decoding, outputs a prompt and continues decoding playback
4. Playback experience is identical to software version, but CPU usage is greatly reduced, capable of handling higher-load video playback tasks

---

## ⚙️ VS Code Configuration Description

The project includes the `.vscode` configuration folder:
- `c_cpp_properties.json` - C/C++ plugin configuration (includes library paths)
- `launch.json` - Debugging configuration
- `settings.json` - Editor settings
- `tasks.json` - Build task configuration

## 🔧 Frequently Asked Questions

### Q1: Compilation cannot find FFmpeg/SDL2 headers
**A**: Check the include paths in `.vscode/c_cpp_properties.json`

### Q2: Missing DLLs at runtime
**A**: Ensure all DLL files are in the same directory as the executable, or added to the system PATH

### Q3: PCM playback speed is wrong or noisy
**A**: PCM_AudioPlayer.cpp only supports 44.1kHz / 16-bit / stereo format; check PCM file parameters

### Q4: Audio file cannot play
**A**: AudioPlayer.cpp supports mainstream formats, but some specially encoded files may require additional decoders

### Q5: Multithreaded player has high CPU usage?
**A**: Normal; multithreaded decoding fully utilizes CPU. The main loop already includes `SDL_Delay(10)` to avoid spinning; if still too high, increase the delay appropriately.

### Q6: Hardware-accelerated version uses more memory than software version?
**A**: The hardware-accelerated version pre-allocates two download frames (~20MB) for fetching data from GPU, which is normal. Compared to the software version, hardware acceleration reduces CPU usage by over 70%, which is worthwhile for high-resolution videos.

### Q7: What if hardware acceleration fails to enable?
**A**: The program automatically falls back to software decoding, unaffected. Check if your GPU supports DXVA2, or update your graphics driver.

## 📝 Changelog

### v1.0.0 (2026.2.28)
- Implemented four independent functional modules
- Support file selection with Chinese paths
- Added detailed metadata display

### v1.1.0 (2026.3.3)
- **New**: `Audio_Video_Player.cpp` multithreaded audio-video synchronized player
- **Optimization**: Improved stream detection logic; pure audio files no longer pop up video window
- **Documentation**: Updated README, detailed multithreaded player architecture

### v1.2.0 (2026.3.6)
- **New**: `HW_Audio_Video_Player.cpp` hardware-accelerated multithreaded audio-video synchronized player (DXVA2 support)
- **Optimization**: Both software/hardware versions now use bounded queues and backpressure mechanism, completely solving memory explosion issues
- **Fix**: Memory leaks in demux thread and decode threads (unreleased AVPacket and AVFrame)
- **Performance**: Hardware-accelerated version reduces CPU usage by about 70% when playing 4K high-bitrate videos
- **Documentation**: Added detailed description and performance comparison data for hardware-accelerated version

### v1.3.0 (2026.3.8)

## 📄 License

This project is for learning and exchange purposes only. FFmpeg and SDL2 are subject to their respective open-source licenses.

## 👤 Author

- GitHub: [@jerryyang1208](https://github.com/jerryyang1208)

## ⭐ Acknowledgements

- [FFmpeg](https://ffmpeg.org/) community
- [SDL](https://www.libsdl.org/) development team