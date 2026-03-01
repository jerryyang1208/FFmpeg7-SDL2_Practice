<div align="right">
  <a href="README.md">简体中文</a>
</div>

# FFmpeg7-SDL2 Multimedia Practice Project

![FFmpeg](https://img.shields.io/badge/FFmpeg-7.0.2-green)
![SDL2](https://img.shields.io/badge/SDL2-2.30.6-blue)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)

A multimedia player practice project based on FFmpeg 7.0.2 and SDL2 2.30.6, featuring four independent examples: audio playback, PCM playback, video playback, and SDL graphics rendering.

Blog: https://zhuanlan.zhihu.com/p/700478133

## 📦 Dependencies
- **[FFmpeg 7.0.2](https://ffmpeg.org/)** - Core audio/video decoding library
- **[SDL2 2.30.6](https://www.libsdl.org/)** - Cross-platform multimedia rendering library

## 🚀 Quick Start

### Environment Setup
1. Download the necessary library package from [Releases](https://github.com/jerryyang1208/FFmpeg7-SDL2_Practice/releases)

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
├── README.md                     # Project documentation
│
├── AudioPlayer.cpp               # Audio player source code
├── PCM_AudioPlayer.cpp           # PCM audio player source code
├── SDL_Pics.cpp                  # SDL graphics rendering source code
├── VideoPlayer.cpp               # Video player source code
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

3. Ensure DLL files are in the same directory as the executables or in the system PATH. These dynamic link library files are required at runtime.

### Compilation Environment
- **Compiler**: MSVC (Visual Studio) or MinGW (VS Code)
- **Standard**: C++11/14
- **Include Directories**: Add FFmpeg and SDL2 include paths
- **Library Directories**: Add FFmpeg and SDL2 lib paths
- **Link Libraries**: 
  - FFmpeg: avformat, avcodec, avutil, swresample, swscale
  - SDL2: SDL2
  - Windows: comdlg32 (for file dialog and choose local files)

## 📁 Project File Description

### 1. 🎵 AudioPlayer.cpp - Universal Audio Player
**Function**: Supports playback of various audio formats (MP3, FLAC, Opus, WAV, M4A, OGG, AAC, etc.)

**Core Features**:
- Chinese path file selection support
- Automatic audio resampling (unified output to 16-bit stereo)
- Real-time playback progress display
- Display audio metadata (title, artist, album, sample rate, etc.)

**Usage**:
1. Run the program, file selection dialog pops up
2. Select an audio file
3. Automatically plays and displays progress bar
4. Automatically exits after playback completes

**Technical Highlights**:
- Uses FFmpeg to decode various audio formats
- Uses Swresample for audio format conversion
- Uses SDL audio device for playback, using SDL_QueueAudio

---

### 2. 🎛️ PCM_AudioPlayer.cpp - Raw PCM Audio Player
**Function**: Plays raw PCM audio files (44.1kHz, 16-bit, stereo)

**Core Features**:
- Directly reads PCM file into memory
- Uses SDL audio queue
- Simple file selection interface

**Usage**:
1. Run the program, select a .pcm file
2. Program automatically plays (Note: PCM file must be 44.1kHz/16-bit/stereo format)
3. Automatically exits after playback completes

**Important Notes**:
- ⚠️ Only supports fixed parameters: 44.1kHz sample rate, 16-bit depth, stereo channels
- If PCM file parameters don't match, playback speed will be abnormal or produce noise

---

### 3. 🖼️ SDL_Pics.cpp - SDL2 Graphics Rendering Example
**Function**: Demonstrates basic SDL2 drawing capabilities

**Core Features**:
- Creates borderless window
- Draws filled rectangles (large blue rectangle, small cyan rectangle)
- Draws white triangle lines
- Red background

**Usage**:
1. Run the program, SDL graphics window appears
2. Window automatically closes after 5 seconds

**Display Effects**:
- Background: Red
- Large blue rectangle: Position(50,50), Size 300x200
- White triangle: Closed line formed by four points
- Small cyan rectangle: Position(400,300), Size 100x100

---

### 4. 🎬 VideoPlayer.cpp - Video Player
**Function**: Plays various video formats (MP4, MKV, AVI, FLV, MOV, etc.)

**Core Features**:
- Chinese path file selection support
- Automatic audio/video synchronization
- Resizable window (画面自动拉伸)
- YUV420P format rendering (ensures compatibility)

**Usage**:
1. Run the program, select a video file
2. Video automatically plays
3. Close window or automatically exits when playback completes

**Technical Highlights**:
- Uses FFmpeg to decode video frames
- Uses Swscale to convert various pixel formats to YUV420P
- Uses SDL texture for video rendering
- PTS (Presentation Time Stamp) based audio/video synchronization

## ⚙️ VS Code Configuration Instructions

The project includes the `.vscode` configuration folder:
- `c_cpp_properties.json` - C/C++ plugin configuration (includes library paths)
- `launch.json` - Debugging configuration
- `settings.json` - Editor settings
- `tasks.json` - Build task configuration

## 🔧 Frequently Asked Questions

### Q1: Can't find FFmpeg/SDL2 header files during compilation
**A**: Check if the include paths in `.vscode/c_cpp_properties.json` are correct

### Q2: Missing DLL error at runtime
**A**: Ensure all DLL files are in the same directory as the executables or added to system PATH

### Q3: PCM playback speed is wrong or has noise
**A**: PCM_AudioPlayer.cpp only supports 44.1kHz/16-bit/stereo format, check your PCM file parameters

### Q4: Audio file won't play
**A**: AudioPlayer.cpp supports mainstream formats, but some specially encoded files may require additional decoders

## 📝 Changelog

### v1.0.0 (2026.02.28)
- Implemented four independent functional modules
- Added Chinese path file selection support
- Added detailed metadata display

## 📄 License

This project is for learning and communication purposes only. FFmpeg and SDL2 follow their respective open-source licenses.

## 👤 Author

- GitHub: [@jerryyang1208](https://github.com/jerryyang1208)

## ⭐ Acknowledgements

- [FFmpeg](https://ffmpeg.org/) Community
- [SDL](https://www.libsdl.org/) Development Team