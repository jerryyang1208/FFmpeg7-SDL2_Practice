# FFmpeg7-SDL2 多媒体实践项目

![FFmpeg](https://img.shields.io/badge/FFmpeg-7.0.2-green)
![SDL2](https://img.shields.io/badge/SDL2-2.30.6-blue)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)

基于 FFmpeg 7.0.2 和 SDL2 2.30.6 的多媒体播放器实践项目，包含音频播放、PCM播放、视频播放和SDL图形绘制四个独立示例。
博客：https://zhuanlan.zhihu.com/p/700478133

## 📦 依赖库
- **[FFmpeg 7.0.2](https://ffmpeg.org/)** - 音视频解码核心库
- **[SDL2 2.30.6](https://www.libsdl.org/)** - 跨平台多媒体渲染库

## 🚀 快速开始

### 环境配置
1. 从 [Releases](https://github.com/jerryyang1208/FFmpeg7-SDL2_Practice/releases) 下载库文件包

2. 解压到项目根目录，确保目录结构如下：
ffmpeg+SDL2/
│
├── .vscode/                      # VS Code 配置文件
│   ├── c_cpp_properties.json     # C/C++ 插件配置
│   ├── launch.json               # 调试配置
│   ├── settings.json             # 编辑器设置
│   └── tasks.json                # 构建任务配置
│
├── FFmpeg-n7.0.2-3-win64-lgpl.../# FFmpeg 库文件夹
├── SDL2-2.30.6/                  # SDL2 库文件夹
│
├── .gitignore                    # Git 忽略配置
├── README.md                     # 项目说明文档
│
├── AudioPlayer.cpp               # 音频播放器源代码
├── PCM_AudioPlayer.cpp           # PCM音频播放器源代码
├── SDL_Pics.cpp                  # SDL图形绘制源代码
├── VideoPlayer.cpp               # 视频播放器源代码
│
├── avcodec-61.dll                # FFmpeg 编解码库
├── avdevice-61.dll               # FFmpeg 设备库
├── avfilter-10.dll               # FFmpeg 滤镜库
├── avformat-61.dll               # FFmpeg 格式库
├── avutil-59.dll                 # FFmpeg 工具库
├── swresample-5.dll              # FFmpeg 音频重采样库
├── swscale-8.dll                 # FFmpeg 图像缩放库
└── SDL2.dll                      # SDL2 动态链接库

3. 确保DLL文件在可执行文件同一目录或系统 PATH 中，运行时必需这些动态链接库文件

### 编译环境
- **编译器**: MSVC (Visual Studio) 或 MinGW
- **标准**: C++11/14
- **包含目录**: 需添加FFmpeg和SDL2的include路径
- **库目录**: 需添加FFmpeg和SDL2的lib路径
- **链接库**: 
- FFmpeg: avformat, avcodec, avutil, swresample, swscale
- SDL2: SDL2
- Windows: comdlg32 (用于文件对话框)

## 📁 项目文件说明

### 1. 🎵 AudioPlayer.cpp - 通用音频播放器
**功能**: 支持多种格式的音频文件播放（MP3, FLAC, Opus, WAV, M4A, OGG, AAC等）

**核心特性**:
- 支持中文路径文件选择
- 自动音频重采样（统一输出16位立体声）
- 实时播放进度显示
- 显示音频元数据（标题、艺术家、专辑、采样率等）

**使用方法**:
1. 运行程序，弹出文件选择对话框
2. 选择音频文件
3. 自动播放并显示进度条
4. 播放完成后自动退出

**技术要点**:
- 使用 FFmpeg 解码各种音频格式
- 使用 Swresample 进行音频格式转换
- 使用 SDL 音频设备播放

---

### 2. 🎛️ PCM_AudioPlayer.cpp - PCM原始音频播放器
**功能**: 播放原始PCM音频文件（44.1kHz, 16位, 双声道）

**核心特性**:
- 直接读取PCM文件到内存
- 使用SDL队列音频数据
- 简单的文件选择界面

**使用方法**:
1. 运行程序，选择.pcm文件
2. 程序自动播放（注意：PCM文件必须是44.1kHz/16位/立体声格式）
3. 播放完成后自动退出

**注意事项**:
- ⚠️ 仅支持固定参数：44.1kHz采样率、16位深度、双声道
- 如果PCM文件参数不匹配，会导致播放速度异常或噪音

---

### 3. 🖼️ SDL_Pics.cpp - SDL2图形绘制示例
**功能**: 展示SDL2的基本绘图功能

**核心特性**:
- 创建无边框窗口
- 绘制填充矩形（蓝色大矩形、青色小矩形）
- 绘制白色三角形线条
- 红色背景

**使用方法**:
1. 运行程序，显示SDL图形窗口
2. 窗口显示5秒后自动关闭

**显示效果**:
- 背景：红色
- 蓝色大矩形：位置(50,50)，大小300x200
- 白色三角形：由四个点构成的闭合线条
- 青色小矩形：位置(400,300)，大小100x100

---

### 4. 🎬 VideoPlayer.cpp - 视频播放器
**功能**: 播放多种格式的视频文件（MP4, MKV, AVI, FLV, MOV等）

**核心特性**:
- 支持中文路径文件选择
- 自动音视频同步
- 可调整窗口大小（画面自动拉伸）
- YUV420P格式渲染（确保兼容性）

**使用方法**:
1. 运行程序，选择视频文件
2. 视频自动播放
3. 关闭窗口或播放完毕自动退出

**技术要点**:
- 使用 FFmpeg 解码视频帧
- 使用 Swscale 将各种像素格式转换为YUV420P
- 使用 SDL 纹理渲染视频
- 基于PTS（显示时间戳）的音视频同步

## ⚙️ VS Code 配置说明

项目包含`.vscode`配置文件夹：
- `c_cpp_properties.json` - C/C++插件配置（包含库路径）
- `launch.json` - 调试配置
- `settings.json` - 编辑器设置
- `tasks.json` - 构建任务配置

## 🔧 常见问题

### Q1: 编译时找不到FFmpeg/SDL2头文件
**A**: 检查`.vscode/c_cpp_properties.json`中的include路径是否正确

### Q2: 运行时提示缺少DLL
**A**: 确保所有DLL文件在可执行文件同一目录，或已添加到系统PATH

### Q3: PCM播放速度不对或有噪音
**A**: PCM_AudioPlayer.cpp只支持44.1kHz/16位/立体声格式，请检查PCM文件参数

### Q4: 视频播放画面扭曲
**A**: VideoPlayer.cpp已通过统一转换为YUV420P解决此问题，确保swscale正确初始化

### Q5: 音频文件无法播放
**A**: AudioPlayer.cpp支持主流格式，但某些特殊编码的文件可能需要额外的解码器

## 📝 更新日志

### v1.0.0 (2024-xx-xx)
- 初始版本发布
- 实现四个独立功能模块
- 支持中文路径文件选择
- 添加详细的元数据显示

## 📄 许可证

本项目仅供学习交流使用，FFmpeg和SDL2遵循其各自的开源许可证。

## 👤 作者

- GitHub: [@jerryyang1208](https://github.com/jerryyang1208)

## ⭐ 致谢

- [FFmpeg](https://ffmpeg.org/) 社区
- [SDL](https://www.libsdl.org/) 开发团队