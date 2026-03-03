<div align="right">
  <a href="README.md">中文</a>
</div>

# FFmpeg7-SDL2 Multimedia Practice Project

![FFmpeg](https://img.shields.io/badge/FFmpeg-7.0.2-green)
![SDL2](https://img.shields.io/badge/SDL2-2.30.6-blue)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)

A multimedia player practice project based on FFmpeg 7.0.2 and SDL2 2.30.6, containing five independent examples: audio playback, PCM playback, video playback, SDL graphics rendering, and a multi-threaded audio-video synchronized player.

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
├── AudioPlayer.cpp               # Audio player source code
├── PCM_AudioPlayer.cpp           # PCM audio player source code
├── SDL_Pics.cpp                  # SDL graphics rendering source code
├── VideoPlayer.cpp               # Basic video player source code
├── Audio_Video_Player.cpp        # Multi-threaded audio-video player source code
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

3. Ensure the DLL files are in the same directory as the executable or in the system PATH. These dynamic link libraries are required at runtime.

### Compilation Environment
- **Compiler**: MSVC (Visual Studio) or MinGW (VS Code)
- **Standard**: C++11/14
- **Include Directories**: Must add FFmpeg and SDL2 include paths
- **Library Directories**: Must add FFmpeg and SDL2 lib paths
- **Link Libraries**:
  - FFmpeg: avformat, avcodec, avutil, swresample, swscale
  - SDL2: SDL2
  - Windows: comdlg32 (for file dialog to select local files)

## 📁 Project File Description

### 1. 🎵 AudioPlayer.cpp - Universal Audio Player
**Function**: Supports playback of various audio formats (MP3, FLAC, Opus, WAV, M4A, OGG, AAC, etc.)

**Core Features**:
- Chinese path file selection support
- Automatic audio resampling (unified output to 16-bit stereo)
- Real-time playback progress display
- Display audio metadata (title, artist, album, sample rate, etc.)

**Usage**:
1. Run the program, a file selection dialog pops up
2. Select an audio file
3. Automatically plays and displays progress bar
4. Automatically exits after playback completes

**Technical Points**:
- Uses FFmpeg to decode various audio formats
- Uses Swresample for audio format conversion
- Uses SDL queue mode for audio playback

---

### 2. 🎛️ PCM_AudioPlayer.cpp - Raw PCM Audio Player
**Function**: Plays raw PCM audio files (44.1kHz, 16-bit, stereo)

**Core Features**:
- Directly reads PCM file into memory
- Uses SDL audio queue for data
- Simple file selection interface

**Usage**:
1. Run the program, select a .pcm file
2. Program automatically plays (Note: PCM file must be 44.1kHz / 16-bit / stereo format)
3. Automatically exits after playback completes

**Important Notes**:
- ⚠️ Only supports fixed parameters: 44.1kHz sample rate, 16-bit depth, stereo
- If PCM file parameters don't match, it may cause abnormal playback speed or noise

---

### 3. 🖼️ SDL_Pics.cpp - SDL2 Graphics Rendering Example
**Function**: Demonstrates basic drawing functions of SDL2

**Core Features**:
- Creates borderless window
- Draws filled rectangles (large blue rectangle, small cyan rectangle)
- Draws white triangle lines
- Red background

**Usage**:
1. Run the program, displays SDL graphics window
2. Window automatically closes after 5 seconds

**Display Effects**:
- Background: Red
- Large blue rectangle: Position (50,50), size 300x200
- White triangle: Closed line formed by four points
- Small cyan rectangle: Position (400,300), size 100x100

---

### 4. 🎬 VideoPlayer.cpp - Basic Video Player
**Function**: Plays various video formats (MP4, MKV, AVI, FLV, MOV, etc.)

**Core Features**:
- Chinese path file selection support
- Automatic audio-video synchronization
- Resizable window (image automatically stretches)
- YUV420P format rendering (ensures compatibility)

**Usage**:
1. Run the program, select a video file
2. Video automatically plays
3. Close window or automatically exits when playback completes

**Technical Points**:
- Uses FFmpeg to decode video frames
- Uses Swscale to convert various pixel formats to YUV420P
- Uses SDL texture for video rendering
- PTS (Presentation Time Stamp) based audio-video synchronization

---

### 5. 🎯 Audio_Video_Player.cpp - Multi-threaded Audio-Video Synchronized Player (New ⭐)
**Function**: Professional-grade audio-video player based on **multi-threaded decoding + queue buffering + audio-master synchronization**, supports both audio and video files, with no window for pure audio files.

**Core Features**:
- ✅ **Multi-threaded Architecture**: Demux thread, audio decoding thread, video decoding thread run independently for efficient CPU utilization
- ✅ **Thread-safe Queues**: Uses custom `SafeQueue` template for lock-free data transfer
- ✅ **Intelligent Stream Recognition**: Automatically excludes attached picture streams (like MP3 album art), no video window for pure audio playback
- ✅ **Precise Synchronization**: Uses audio clock as reference, dynamically adjusts video rendering timing (supports early waiting, late dropping)
- ✅ **Real-time Progress Display**: Shows current playback progress (minutes:seconds format) in console
- ✅ **Graceful Exit**: Supports window close exit, automatically waits for audio playback to finish
- ✅ **Queue Mode Audio Output**: No callback, directly uses `SDL_QueueAudio` to push data, simple and reliable

**Data Flow Architecture**:
File → [Demux Thread] → Audio Packet Queue → [Audio Decode Thread] → SDL_QueueAudio → Audio Device
                      → Video Packet Queue → [Video Decode Thread] → Video Frame Queue → [Main Thread Render] → Window


**Technical Points**:
- Uses FFmpeg 7.0 API (`avcodec_send_packet` / `avcodec_receive_frame`)
- Audio resampled to S16 stereo, video uniformly converted to YUV420P
- Audio clock calculation: `(total pushed samples - SDL internal queue remaining samples)` / sample rate
- Synchronization thresholds: video ahead >100ms wait, behind >300ms drop
- Perfect support for audio files with embedded covers (MP3, FLAC), no black window

**Usage**:
1. Run the program, select a media file (audio or video)
2. For pure audio files: no window pops up, console displays playback progress
3. For video files: window opens, video and audio play synchronously
4. Automatically exits when playback completes, or click window close button to exit

## ⚙️ VS Code Configuration Description

The project includes the `.vscode` configuration folder:
- `c_cpp_properties.json` - C/C++ plugin configuration (includes library paths)
- `launch.json` - Debug configuration
- `settings.json` - Editor settings
- `tasks.json` - Build task configuration

## 🔧 Frequently Asked Questions

### Q1: Compilation error - FFmpeg/SDL2 headers not found
**A**: Check the include paths in `.vscode/c_cpp_properties.json`

### Q2: Runtime error - missing DLLs
**A**: Ensure all DLL files are in the same directory as the executable or added to system PATH

### Q3: PCM playback speed wrong or noisy
**A**: PCM_AudioPlayer.cpp only supports 44.1kHz / 16-bit / stereo format, check your PCM file parameters

### Q4: Audio files won't play
**A**: AudioPlayer.cpp supports mainstream formats, but some specially encoded files may require additional decoders

### Q5: High CPU usage with multi-threaded player?
**A**: Normal phenomenon, multi-threaded decoding fully utilizes CPU. `SDL_Delay(10)` is already added in the main loop to avoid idle spinning; if still too high, you can increase the delay appropriately

## 📝 Changelog

### v1.0.0 (2026.2.28)
- Implemented four independent functional modules
- Chinese path file selection support
- Added detailed metadata display

### v1.1.0 (2026.3.3)
- **New**: `Audio_Video_Player.cpp` multi-threaded audio-video synchronized player
- **Optimization**: Improved stream recognition logic, no video window for pure audio files
- **Documentation**: Updated README, detailed multi-threaded player architecture

## 📄 License

This project is for learning and communication purposes only. FFmpeg and SDL2 follow their respective open-source licenses.

## 👤 Author

- GitHub: [@jerryyang1208](https://github.com/jerryyang1208)

## ⭐ Acknowledgments

- [FFmpeg](https://ffmpeg.org/) Community
- [SDL](https://www.libsdl.org/) Development Team