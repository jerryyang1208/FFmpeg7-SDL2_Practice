<div align="right">
  <a href="README_en.md">English</a>
</div>

# FFmpeg7-SDL2 多媒体实践项目

![FFmpeg](https://img.shields.io/badge/FFmpeg-7.0.2-green)
![SDL2](https://img.shields.io/badge/SDL2-2.30.6-blue)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)

基于 FFmpeg 7.0.2 和 SDL2 2.30.6 的多媒体播放器实践项目，包含音频播放、PCM 播放、视频播放、SDL 图形绘制、多线程音视频同步播放器以及硬件加速解码的音视频播放器。

## 📦 依赖库
- **[FFmpeg 7.0.2](https://ffmpeg.org/)** - 音视频解码核心库
- **[SDL2 2.30.6](https://www.libsdl.org/)** - 跨平台多媒体渲染库

## 🚀 快速开始

### 环境配置
1. 从 [Releases](https://github.com/jerryyang1208/FFmpeg7-SDL2_Practice/releases) 下载必需的库文件包

2. 解压到项目根目录，确保目录结构如下：
<pre>
ffmpeg+SDL2/
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
├── README_en.md                  # 英文项目说明
│
├── common.h                      # 公共头文件（类型定义、常量、函数声明）
├── common.cpp                    # 公共实现文件（全局上下文、队列、帧池、辅助函数）
├── AudioPlayer.cpp               # 音频播放器源代码
├── PCM_AudioPlayer.cpp           # PCM 音频播放器源代码
├── SDL_Pics.cpp                  # SDL 图形绘制源代码
├── VideoPlayer.cpp               # 视频播放器源代码
├── Audio_Video_Player.cpp        # 多线程音视频同步播放器（软件解码）
├── HW_Audio_Video_Player.cpp     # 硬件加速的音视频播放器（D3D11VA/DXVA2）
│
├── avcodec-61.dll                # FFmpeg 编解码库
├── avdevice-61.dll               # FFmpeg 设备库
├── avfilter-10.dll               # FFmpeg 滤镜库
├── avformat-61.dll               # FFmpeg 格式库
├── avutil-59.dll                 # FFmpeg 工具库
├── swresample-5.dll              # FFmpeg 音频重采样库
├── swscale-8.dll                 # FFmpeg 图像缩放库
└── SDL2.dll                      # SDL2 动态链接库
</pre>

3. 确保 DLL 文件在可执行文件同一目录或系统 PATH 中

### 编译环境
- **编译器**: MSVC (Visual Studio) 或 MinGW（VS Code）
- **标准**: C++17
- **包含目录**: 需添加 FFmpeg 和 SDL2 的 include 路径
- **库目录**: 需添加 FFmpeg 和 SDL2 的 lib 路径
- **链接库**: 
  - FFmpeg: avformat, avcodec, avutil, swresample, swscale, avfilter, avdevice
  - SDL2: SDL2, SDL2main
  - Windows: comdlg32

## 📁 项目文件说明

### 1. 🎵 AudioPlayer.cpp - 通用音频播放器
**功能**: 支持多种格式的音频文件播放（如 MP3, FLAC, Opus, WAV, M4A, OGG, AAC 等）

**核心特性**:
- 支持中文路径文件选择
- 自动音频重采样（统一输出 16 位立体声）
- 实时播放进度显示
- 显示音频元数据（标题、艺术家、专辑、采样率等）

**使用方法**:
1. 运行程序，弹出文件选择对话框
2. 选择音频文件
3. 自动播放并显示进度条
4. 播放完成后自动退出

---

### 2. 🎛️ PCM_AudioPlayer.cpp - PCM 原始音频播放器
**功能**: 播放原始 PCM 音频文件（44.1kHz, 16 位, 双声道）

**核心特性**:
- 直接读取 PCM 文件到内存
- 使用 SDL 队列音频数据
- 简单的文件选择界面

**使用方法**:
1. 运行程序，选择 .pcm 文件
2. 程序自动播放（注意：PCM 文件必须是 44.1kHz / 16 位 / 立体声格式）
3. 播放完成后自动退出

**注意事项**:
- ⚠️ 仅支持固定参数：44.1kHz 采样率、16 位深度、双声道

---

### 3. 🖼️ SDL_Pics.cpp - SDL2 图形绘制示例
**功能**: 展示 SDL2 的基本绘图功能

**核心特性**:
- 创建无边框窗口
- 绘制填充矩形（蓝色大矩形、青色小矩形）
- 绘制白色三角形线条
- 红色背景

**使用方法**:
1. 运行程序，显示 SDL 图形窗口
2. 窗口显示 5 秒后自动关闭

---

### 4. 🎬 VideoPlayer.cpp - 视频播放器
**功能**: 播放多种格式的视频文件（MP4, MKV, AVI, FLV, MOV等）

**核心特性**:
- 支持中文路径文件选择
- 自动音视频同步
- 可调整窗口大小（画面自动拉伸）
- YUV420P 格式渲染

**使用方法**:
1. 运行程序，选择视频文件
2. 视频自动播放
3. 关闭窗口或播放完毕自动退出

---

### 5. 🎯 Audio_Video_Player.cpp - 多线程音视频同步播放器（软件解码）
**功能**: 基于**多线程解码 + 队列缓冲 + 音频为主同步**的专业级音视频播放器，支持音视频文件播放，纯音频文件无窗口。

**核心特性**:
- ✅ **多级流水线架构**：解复用线程、音频解码线程、视频解码线程独立运行，通过生产者-消费者模型解耦 IO、解码、渲染
- ✅ **线程安全队列**：使用自定义 `SafeQueue` 模板实现有界阻塞队列，支持背压控制防止内存无限膨胀
- ✅ **智能资源管理**：利用 RAII 与 `std::unique_ptr` 自定义删除器封装 FFmpeg/SDL 资源，实现自动生命周期管理
- ✅ **双向同步策略**：以音频时钟为主时钟，视频超前或滞后超过 100ms 时丢弃该帧快速追赶，超前 20ms 内微休眠精确等待
- ✅ **帧池复用**：设计 `FramePool` 预分配复用 YUV420P 帧缓冲区，避免每帧重复分配与冗余拷贝
- ✅ **动态滤镜支持**：引入 libavfilter 构建动态滤镜图，支持基于 display matrix 的视频旋转实时处理
- ✅ **智能流识别**：自动排除封面图片流，纯音频播放时不弹出视频窗口
- ✅ **实时进度显示**：在控制台显示当前播放进度（分:秒格式）
- ✅ **窗口缩放适配**：实现窗口缩放时的动态 Viewport 裁剪，保持画面比例不失真

**数据流架构**:
```
文件 → [解复用线程] → 音频包队列(60) → [音频解码线程] → SDL_QueueAudio → 音频设备
                   → 视频包队列(20) → [视频解码线程] → 视频帧队列(10) → [主线程渲染] → 窗口
```

**技术要点**:
- 使用 FFmpeg 7.0 API（`avcodec_send_packet` / `avcodec_receive_frame`）
- 音频重采样为 S16 立体声，视频统一转换为 YUV420P
- 音频时钟计算：`已推送样本总数 - SDL 内部队列剩余样本数` / 采样率
- 视频解码器线程数：4（平衡性能与资源占用）
- 使用 `PlayerContext` 结构体统一管理全局播放器状态

**使用方法**:
1. 运行程序，选择媒体文件（音频或视频皆可）
2. 若为纯音频文件，无窗口弹出，控制台显示播放进度
3. 若为视频文件，弹出窗口开始播放，画面与声音同步
4. 播放完毕自动退出，或点击窗口关闭按钮退出

---

### 6. ⚡ HW_Audio_Video_Player.cpp - 硬件加速多线程音视频同步播放器
**功能**: 在 **Audio_Video_Player.cpp** 的基础上，增加 **D3D11VA/DXVA2 硬件解码支持**，显著降低 CPU 占用，尤其适合 4K/8K 高码率视频的流畅播放。当硬件加速不可用时自动回退到软件解码。

**核心特性**:
- ✅ **D3D11VA/DXVA2 硬件加速**：优先检测 D3D11VA，不可用则降级到 DXVA2，将解码任务交给 GPU，大幅降低 CPU 负载
- ✅ **多级流水线架构**：继承高效的多线程设计（解复用、音频解码、视频解码独立线程）
- ✅ **智能资源管理**：所有 FFmpeg/SDL 资源均使用 RAII 封装，配合原子标志与队列 abort 机制确保线程安全退出
- ✅ **双向自适应同步**：沿用音频时钟为主同步策略，视频超前或滞后超过 100ms 时丢弃该帧，超前 20ms 内微休眠等待
- ✅ **帧池复用**：与软件版本一致的 `FramePool` 设计，预分配 YUV420P 帧缓冲区
- ✅ **动态滤镜支持**：支持基于 display matrix 的视频旋转实时处理
- ✅ **自动回退机制**：若硬件加速初始化失败，自动切换至软件解码，保证可用性
- ✅ **纯音频支持**：同样适用于纯音频文件，无窗口弹出

**数据流架构**:
```
文件 → [解复用线程] → 音频包队列(60) → [音频解码线程] → SDL_QueueAudio → 音频设备
                   → 视频包队列(20) → [视频解码线程(硬件加速)] → 显存帧 → av_hwframe_transfer_data → 主存帧 → 视频帧队列(10) → [主线程渲染] → 窗口
```

**技术要点**:
- **硬件加速初始化**：遍历解码器的 `AVCodecHWConfig`，优先选择 D3D11VA，其次 DXVA2，创建硬件设备上下文并关联到解码器
- **硬件帧传输**：使用 `av_hwframe_transfer_data()` 将 GPU 中的 NV12 帧传输到系统内存，再转换为 YUV420P
- **自动回退**：若硬件加速不可用，自动使用软件解码
- **视频解码器线程数**：4

**性能对比**（以 4K 60fps 高码率视频测试为例）：
- 软件解码版本：CPU 占用 ≈ 30%
- 硬件加速版本：CPU 占用 ≈ 7%（降低约 77%）

**使用方法**:
1. 运行程序，选择媒体文件（音频或视频皆可）
2. 程序自动检测硬件加速能力，若支持则启用（控制台输出 `HW acceleration: d3d11va` 或 `dxva2`）
3. 若硬件加速不可用，则自动使用软件解码，输出提示并继续播放
4. 播放过程中与软件版本体验完全一致，但 CPU 占用大幅降低

---

## ⚙️ VS Code 配置说明

项目包含 `.vscode` 配置文件夹：
- `c_cpp_properties.json` - C/C++ 插件配置（包含库路径）
- `launch.json` - 调试配置
- `settings.json` - 编辑器设置
- `tasks.json` - 构建任务配置

## 🔧 常见问题

### Q1: 编译时找不到 FFmpeg/SDL2 头文件
**A**: 检查 `.vscode/c_cpp_properties.json` 中的 include 路径是否正确

### Q2: 运行时提示缺少 DLL
**A**: 确保所有 DLL 文件在可执行文件同一目录，或已添加到系统 PATH

### Q3: PCM 播放速度不对或有噪音
**A**: PCM_AudioPlayer.cpp 只支持 44.1kHz / 16 位 / 立体声格式，请检查 PCM 文件参数

### Q4: 音频文件无法播放
**A**: AudioPlayer.cpp 支持主流格式，但某些特殊编码的文件可能需要额外的解码器

### Q5: 多线程播放器运行时 CPU 占用高？
**A**: 正常现象，多线程解码会充分利用 CPU。主循环中已添加 `SDL_Delay(5)` 避免空转

### Q6: 硬件加速启用失败怎么办？
**A**: 程序会自动回退到软件解码，不影响播放。可检查显卡是否支持 D3D11VA/DXVA2，或更新显卡驱动

## 📝 更新日志

### v1.0.0 (2026.2.28)
- 实现四个独立功能模块
- 支持中文路径文件选择
- 添加详细的元数据显示

### v1.1.0 (2026.3.3)
- **新增**：`Audio_Video_Player.cpp` 多线程音视频同步播放器
- **优化**：完善流识别逻辑，纯音频文件不再弹出视频窗口

### v1.2.0 (2026.3.6)
- **新增**：`HW_Audio_Video_Player.cpp` 硬件加速多线程音视频同步播放器
- **优化**：所有队列均采用有界设计，引入背压机制，解决内存暴涨问题

### v1.3.0 (2026.3.8)
- **新增**：引入 FFmpeg 滤镜库，支持视频旋转伸缩适配
- **性能**：硬件加速版本内存占用优化

### v1.4.0 (2026.6.26)
- **重构**：提取公共代码到 `common.h` / `common.cpp`，使用 `PlayerContext` 结构体统一管理全局状态
- **优化**：引入 RAII 智能指针封装所有 FFmpeg/SDL 资源
- **优化**：实现 `FramePool` 帧池复用机制
- **优化**：同步策略升级为双向自适应丢帧（超前/滞后均处理）
- **优化**：硬件加速优先 D3D11VA，降级 DXVA2
- **修复**：帧池 recycle 时 buffer 重新分配问题

## 📄 许可证

本项目仅供学习交流使用，FFmpeg 和 SDL2 遵循其各自的开源许可证。

## 👤 作者

- GitHub: [@jerryyang1208](https://github.com/jerryyang1208)

## ⭐ 致谢

- [FFmpeg](https://ffmpeg.org/) 社区
- [SDL](https://www.libsdl.org/) 开发团队
