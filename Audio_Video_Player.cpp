#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <algorithm>

#include <windows.h>
#include <commdlg.h>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/avutil.h>
    #include <libavutil/time.h>
    #include <libavutil/channel_layout.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/frame.h>
    #include <libavutil/display.h>
    #include <libswresample/swresample.h>
    #include <libswscale/swscale.h>

    // 滤镜相关头文件
    #include <libavfilter/avfilter.h>
    #include <libavfilter/buffersink.h>
    #include <libavfilter/buffersrc.h>
    #include <libavutil/opt.h>

    #include <SDL.h>
}

// ------------------------------------------------------------
// 线程安全队列模板（支持最大容量和阻塞推入）
// ------------------------------------------------------------
template<typename T>
class SafeQueue {
public:
    explicit SafeQueue(size_t max_size = 0) : m_max_size(max_size), m_abort(false) {}

    bool push(T value) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_max_size > 0) {
            m_cond_push.wait(lock, [this] {
                return m_queue.size() < m_max_size || m_abort;
            });
            if (m_abort) return false;
        }
        m_queue.push(std::move(value));
        m_cond_pop.notify_one();
        return true;
    }

    bool pop(T& value, bool block = true) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (block) {
            m_cond_pop.wait(lock, [this] { return !m_queue.empty() || m_abort; });
            if (m_abort) return false;
        } else {
            if (m_queue.empty()) return false;
        }
        value = std::move(m_queue.front());
        m_queue.pop();
        if (m_max_size > 0) m_cond_push.notify_one();
        return true;
    }

    void abort() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_abort = true;
        m_cond_pop.notify_all();
        m_cond_push.notify_all();
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_cond_pop;
    std::condition_variable m_cond_push;
    size_t m_max_size;
    bool m_abort;
};

// ------------------------------------------------------------
// 视频帧数据结构
// ------------------------------------------------------------
struct VideoFrame {
    AVFrame* frame = nullptr;
    double pts = 0.0;
};

// ------------------------------------------------------------
// 全局变量
// ------------------------------------------------------------
AVFormatContext* fmt_ctx = nullptr;
int audio_stream_idx = -1;
int video_stream_idx = -1;
AVCodecContext* audio_codec_ctx = nullptr;
AVCodecContext* video_codec_ctx = nullptr;
AVStream* audio_stream = nullptr;
AVStream* video_stream = nullptr;

SwrContext* swr_ctx = nullptr;
int audio_samplerate = 0;
int audio_channels = 2;
AVSampleFormat audio_out_fmt = AV_SAMPLE_FMT_S16;

// 滤镜图相关
AVFilterGraph* filter_graph = nullptr;
AVFilterContext* buffersrc_ctx = nullptr;
AVFilterContext* buffersink_ctx = nullptr;
std::atomic<bool> video_filter_enabled(false);  // 标记滤镜是否成功初始化

SwsContext* sws_ctx = nullptr;
int video_width = 0;      // 最终输出视频宽度（可能已旋转）
int video_height = 0;     // 最终输出视频高度
AVPixelFormat video_out_fmt = AV_PIX_FMT_YUV420P;

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
SDL_Texture* texture = nullptr;
SDL_AudioDeviceID audio_dev = 0;

// 队列（设置最大容量）
SafeQueue<AVPacket*> audio_packet_queue(100);
SafeQueue<AVPacket*> video_packet_queue(50);
SafeQueue<VideoFrame> video_frame_queue(10);

std::atomic<int64_t> audio_samples_pushed(0);
std::atomic<bool> quit(false);
std::atomic<bool> pause_play(false);

std::thread demux_thread;
std::thread audio_dec_thread;
std::thread video_dec_thread;

int64_t total_duration_sec = 0;

// ------------------------------------------------------------
// 辅助函数：文件选择
// ------------------------------------------------------------
std::string SelectFileDialog() {
    char szFile[260] = { 0 };
    OPENFILENAMEA ofn = { sizeof(ofn) };
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Media Files\0*.mp4;*.mkv;*.avi;*.flv;*.mov;*.mp3;*.flac;*.wav;*.m4a;*.ogg\0All Files\0*.*\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn))
        return std::string(szFile);
    return "";
}

std::string AnsiToUtf8(const std::string& ansiStr) {
    if (ansiStr.empty()) return "";
    int nwLen = MultiByteToWideChar(CP_ACP, 0, ansiStr.c_str(), -1, NULL, 0);
    wchar_t* pwBuf = new wchar_t[nwLen];
    MultiByteToWideChar(CP_ACP, 0, ansiStr.c_str(), -1, pwBuf, nwLen);
    int nLen = WideCharToMultiByte(CP_UTF8, 0, pwBuf, -1, NULL, 0, NULL, NULL);
    char* pBuf = new char[nLen];
    WideCharToMultiByte(CP_UTF8, 0, pwBuf, -1, pBuf, nLen, NULL, NULL);
    std::string retStr(pBuf);
    delete[] pwBuf; delete[] pBuf;
    return retStr;
}

// ------------------------------------------------------------
// 音频时钟（基于已播放样本数）
// ------------------------------------------------------------
double get_audio_clock() {
    if (audio_dev == 0 || audio_samplerate == 0) {
        static int64_t start_time = av_gettime();
        return (av_gettime() - start_time) / 1000000.0;
    }
    int64_t pushed = audio_samples_pushed.load(std::memory_order_relaxed);
    Uint32 queued_bytes = SDL_GetQueuedAudioSize(audio_dev);
    int queued_samples = queued_bytes / (2 * audio_channels); // 16位立体声：每样本2字节 * 2声道 = 4字节/立体声帧
    int64_t played = pushed - queued_samples;
    if (played < 0) played = 0;
    return static_cast<double>(played) / audio_samplerate;
}

// ------------------------------------------------------------
// 解复用线程：读取数据包并分发到音频/视频队列
// ------------------------------------------------------------
void demux_thread_func() {
    AVPacket* pkt = av_packet_alloc();
    while (!quit) {
        int ret = av_read_frame(fmt_ctx, pkt);
        if (ret < 0) break;

        if (pkt->stream_index == audio_stream_idx) {
            AVPacket* clone = av_packet_clone(pkt);
            av_packet_unref(pkt);
            if (!audio_packet_queue.push(clone)) {
                av_packet_free(&clone);
                break;
            }
        } else if (pkt->stream_index == video_stream_idx) {
            AVPacket* clone = av_packet_clone(pkt);
            av_packet_unref(pkt);
            if (!video_packet_queue.push(clone)) {
                av_packet_free(&clone);
                break;
            }
        } else {
            av_packet_unref(pkt);
        }
    }
    av_packet_free(&pkt);

    // 发送空包表示结束
    if (audio_stream_idx != -1) audio_packet_queue.push(nullptr);
    if (video_stream_idx != -1) video_packet_queue.push(nullptr);
}

// ------------------------------------------------------------
// 音频解码线程：解码音频并直接推送到 SDL 队列
// ------------------------------------------------------------
void audio_decoder_thread_func() {
    AVPacket* pkt = nullptr;
    AVFrame* frame = av_frame_alloc();
    uint8_t* out_buf = (uint8_t*)av_malloc(192000 * 4); // 足够容纳重采样后的数据

    while (!quit) {
        if (!audio_packet_queue.pop(pkt)) break; // 队列被终止
        if (!pkt) break; // 正常结束

        if (avcodec_send_packet(audio_codec_ctx, pkt) == 0) {
            while (avcodec_receive_frame(audio_codec_ctx, frame) == 0) {
                int out_samples = swr_convert(swr_ctx, &out_buf, 192000,
                                               (const uint8_t**)frame->data, frame->nb_samples);
                if (out_samples > 0) {
                    int bytes = out_samples * 4; // S16立体声
                    // 控制 SDL 队列大小，避免堆积过多（不超过1秒数据）
                    const int max_queued_bytes = audio_samplerate * 4 * 1.0;
                    while (!quit && SDL_GetQueuedAudioSize(audio_dev) > max_queued_bytes) {
                        SDL_Delay(10);
                    }
                    SDL_QueueAudio(audio_dev, out_buf, bytes);
                    audio_samples_pushed.fetch_add(out_samples, std::memory_order_relaxed);
                }
                av_frame_unref(frame);
            }
        }
        av_packet_free(&pkt);
    }

    av_free(out_buf);
    av_frame_free(&frame);
}

// ------------------------------------------------------------
// 视频解码线程：支持滤镜旋转，并在滤镜不可用时回退到直接转换
// ------------------------------------------------------------
void video_decoder_thread_func() {
    AVPacket* pkt = nullptr;
    AVFrame* frame = av_frame_alloc();
    AVFrame* filt_frame = av_frame_alloc(); // 滤镜输出帧（仅当滤镜启用时使用）

    while (!quit) {
        if (!video_packet_queue.pop(pkt)) break;
        if (!pkt) break;

        if (avcodec_send_packet(video_codec_ctx, pkt) == 0) {
            while (avcodec_receive_frame(video_codec_ctx, frame) == 0) {
                AVFrame* out_frame = nullptr;

                // 如果滤镜可用，使用滤镜处理，原子标志 video_filter_enabled 确保线程安全地检查滤镜状态
                if (video_filter_enabled && buffersrc_ctx && buffersink_ctx) {   // 减少内存拷贝
                    int ret = av_buffersrc_add_frame_flags(buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
                    if (ret >= 0) {
                        while (true) {
                            ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
                            if (ret < 0) break;
                            out_frame = av_frame_clone(filt_frame);
                            double pts = filt_frame->best_effort_timestamp * av_q2d(video_stream->time_base);
                            if (!video_frame_queue.push({out_frame, pts})) {
                                av_frame_free(&out_frame);
                                av_frame_unref(filt_frame);
                                break;
                            }
                            av_frame_unref(filt_frame);
                        }
                    } else {
                        std::cerr << "Warning: Failed to add frame to filter, fallback to direct conversion." << std::endl;
                    }
                }

                // 如果滤镜未产生输出帧（滤镜不可用或失败），则使用直接转换
                if (!out_frame) {
                    if (!sws_ctx) {
                        std::cerr << "Error: sws_ctx is null, cannot convert frame." << std::endl;
                        av_frame_unref(frame);
                        continue;
                    }

                    if (frame->format == AV_PIX_FMT_YUV420P) {
                        out_frame = av_frame_clone(frame);
                    } else {
                        out_frame = av_frame_alloc();
                        out_frame->format = video_out_fmt;
                        out_frame->width = video_width;   // 注意：video_width 已根据实际输出设置
                        out_frame->height = video_height;
                        if (av_frame_get_buffer(out_frame, 0) < 0) {
                            av_frame_free(&out_frame);
                            av_frame_unref(frame);
                            continue;
                        }
                        sws_scale(sws_ctx,
                                  frame->data, frame->linesize, 0, video_height,
                                  out_frame->data, out_frame->linesize);
                    }
                    double pts = frame->best_effort_timestamp * av_q2d(video_stream->time_base);
                    if (!video_frame_queue.push({out_frame, pts})) {
                        av_frame_free(&out_frame);
                    }
                }

                av_frame_unref(frame);
            }
        }
        av_packet_free(&pkt);
    }

    av_frame_free(&frame);
    av_frame_free(&filt_frame);
}

// ------------------------------------------------------------
// 主函数
// ------------------------------------------------------------
int main(int argc, char* argv[]) {
    // 1. 选择文件
    std::string ansiPath = SelectFileDialog();
    if (ansiPath.empty()) return 0;
    std::string utf8Path = AnsiToUtf8(ansiPath);

    // 2. 初始化 FFmpeg
    av_log_set_level(AV_LOG_ERROR);

    if (avformat_open_input(&fmt_ctx, utf8Path.c_str(), nullptr, nullptr) != 0) {
        std::cerr << "Could not open file." << std::endl;
        return -1;
    }
    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) return -1;

    // 查找音视频流
    audio_stream_idx = -1;
    video_stream_idx = -1;
    for (int i = 0; i < (int)fmt_ctx->nb_streams; i++) {
        AVStream* st = fmt_ctx->streams[i];
        if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audio_stream_idx == -1) {
            audio_stream_idx = i;
        } else if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video_stream_idx == -1) {
            if (st->disposition & AV_DISPOSITION_ATTACHED_PIC) continue; // 跳过专辑封面
            video_stream_idx = i;
        }
    }
    if (audio_stream_idx == -1 && video_stream_idx == -1) {
        std::cerr << "No audio or video stream found." << std::endl;
        return -1;
    }

    // 获取总时长
    if (fmt_ctx->duration != AV_NOPTS_VALUE) {
        total_duration_sec = fmt_ctx->duration / AV_TIME_BASE;
    }

    // 3. 初始化音频解码器
    if (audio_stream_idx != -1) {
        audio_stream = fmt_ctx->streams[audio_stream_idx];
        const AVCodec* audio_codec = avcodec_find_decoder(audio_stream->codecpar->codec_id);
        audio_codec_ctx = avcodec_alloc_context3(audio_codec);
        avcodec_parameters_to_context(audio_codec_ctx, audio_stream->codecpar);
        if (avcodec_open2(audio_codec_ctx, audio_codec, nullptr) < 0) {
            std::cerr << "Failed to open audio codec." << std::endl;
            return -1;
        }

        AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
        swr_alloc_set_opts2(&swr_ctx,
                            &out_ch_layout, audio_out_fmt, audio_codec_ctx->sample_rate,
                            &audio_codec_ctx->ch_layout, audio_codec_ctx->sample_fmt, audio_codec_ctx->sample_rate,
                            0, nullptr);
        if (swr_init(swr_ctx) < 0) {
            std::cerr << "Failed to initialize audio resampler." << std::endl;
            return -1;
        }
        audio_samplerate = audio_codec_ctx->sample_rate;
    }

    // 4. 初始化视频解码器及滤镜图（用于自动旋转）
    if (video_stream_idx != -1) {
        video_stream = fmt_ctx->streams[video_stream_idx];
        const AVCodec* video_codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
        video_codec_ctx = avcodec_alloc_context3(video_codec);
        avcodec_parameters_to_context(video_codec_ctx, video_stream->codecpar);
        video_codec_ctx->thread_count = std::thread::hardware_concurrency();
        video_codec_ctx->thread_type = FF_THREAD_FRAME;
        if (avcodec_open2(video_codec_ctx, video_codec, nullptr) < 0) {
            std::cerr << "Failed to open video codec." << std::endl;
            return -1;
        }

        // --- 获取视频旋转角度（临时忽略废弃警告）---
        double theta = 0.0;
        const int32_t* display_matrix = nullptr;
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        display_matrix = (const int32_t*)av_stream_get_side_data(video_stream, AV_PKT_DATA_DISPLAYMATRIX, nullptr);
        #pragma GCC diagnostic pop
        if (display_matrix) {
            theta = av_display_rotation_get(display_matrix);
        }

        // 根据角度确定滤镜描述（仅当需要旋转时创建）
        std::string filter_desc;
        if (theta == -90.0 || theta == 270.0) {
            filter_desc = "transpose=1"; // 顺时针旋转90度
        } else if (theta == 90.0 || theta == -270.0) {
            filter_desc = "transpose=2"; // 逆时针旋转90度
        } else if (theta == 180.0 || theta == -180.0) {
            filter_desc = "hflip,vflip"; // 旋转180度
        }

        if (!filter_desc.empty()) {
            // --- 创建滤镜图 ---
            filter_graph = avfilter_graph_alloc();
            if (!filter_graph) {
                std::cerr << "Failed to allocate filter graph, disable filter." << std::endl;
                goto no_filter;
            }

            const AVFilter* buffersrc = avfilter_get_by_name("buffer");
            const AVFilter* buffersink = avfilter_get_by_name("buffersink");
            if (!buffersrc || !buffersink) {
                std::cerr << "Filter 'buffer' or 'buffersink' not found, disable filter." << std::endl;
                avfilter_graph_free(&filter_graph);
                filter_graph = nullptr;
                goto no_filter;
            }

            char args[512];
            snprintf(args, sizeof(args),
                     "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                     video_codec_ctx->width, video_codec_ctx->height, video_codec_ctx->pix_fmt,
                     video_stream->time_base.num, video_stream->time_base.den,
                     video_codec_ctx->sample_aspect_ratio.num, video_codec_ctx->sample_aspect_ratio.den);

            bool filter_ok = true;
            if (avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, nullptr, filter_graph) < 0) {
                std::cerr << "Failed to create buffer source, disable filter." << std::endl;
                filter_ok = false;
            } else if (avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", nullptr, nullptr, filter_graph) < 0) {
                std::cerr << "Failed to create buffer sink, disable filter." << std::endl;
                filter_ok = false;
            } else {
                // 设置接收器像素格式为 YUV420P
                enum AVPixelFormat pix_fmts[] = { video_out_fmt, AV_PIX_FMT_NONE };
                if (av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN) < 0) {
                    std::cerr << "Failed to set sink pix_fmts, disable filter." << std::endl;
                    filter_ok = false;
                } else {
                    AVFilterInOut* outputs = avfilter_inout_alloc();
                    AVFilterInOut* inputs = avfilter_inout_alloc();
                    if (!outputs || !inputs) {
                        std::cerr << "Failed to allocate filter in/out, disable filter." << std::endl;
                        filter_ok = false;
                    } else {
                        outputs->name = av_strdup("in");
                        inputs->name = av_strdup("out");
                        if (!outputs->name || !inputs->name) {
                            std::cerr << "Failed to strdup filter names, disable filter." << std::endl;
                            filter_ok = false;
                        } else {
                            outputs->filter_ctx = buffersrc_ctx;
                            outputs->pad_idx = 0;
                            outputs->next = nullptr;
                            inputs->filter_ctx = buffersink_ctx;
                            inputs->pad_idx = 0;
                            inputs->next = nullptr;

                            if (avfilter_graph_parse_ptr(filter_graph, filter_desc.c_str(), &inputs, &outputs, nullptr) < 0) {
                                std::cerr << "Failed to parse filter graph, disable filter." << std::endl;
                                filter_ok = false;
                            } else if (avfilter_graph_config(filter_graph, nullptr) < 0) {
                                std::cerr << "Failed to config filter graph, disable filter." << std::endl;
                                filter_ok = false;
                            }
                        }
                        avfilter_inout_free(&inputs);
                        avfilter_inout_free(&outputs);
                    }
                }
            }

            if (filter_ok) {
                video_filter_enabled = true;
                if (buffersink_ctx && buffersink_ctx->inputs[0]) {
                    video_width = buffersink_ctx->inputs[0]->w;
                    video_height = buffersink_ctx->inputs[0]->h;
                } else {
                    video_width = video_codec_ctx->width;
                    video_height = video_codec_ctx->height;
                }
                std::cout << "Video rotation filter enabled: " << filter_desc << std::endl;
            } else {
                // 滤镜失败，释放相关资源
                if (filter_graph) avfilter_graph_free(&filter_graph);
                filter_graph = nullptr;
                buffersrc_ctx = buffersink_ctx = nullptr;
                video_filter_enabled = false;
            }
        }

        no_filter:
        // 如果滤镜未启用，则使用原始尺寸
        if (!video_filter_enabled) {
            video_width = video_codec_ctx->width;
            video_height = video_codec_ctx->height;
        }

        // 初始化图像转换上下文（用于直接转换的情况）
        sws_ctx = sws_getContext(video_codec_ctx->width, video_codec_ctx->height, video_codec_ctx->pix_fmt,
                                 video_codec_ctx->width, video_codec_ctx->height, video_out_fmt,
                                 SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!sws_ctx) {
            std::cerr << "Failed to create sws context, direct conversion may fail." << std::endl;
            // 继续执行，但后续会检查 sws_ctx 是否为 nullptr
        }
    }

    // 5. 初始化 SDL
    Uint32 sdl_init_flags = SDL_INIT_AUDIO | SDL_INIT_TIMER;
    if (video_stream_idx != -1) sdl_init_flags |= SDL_INIT_VIDEO;
    if (SDL_Init(sdl_init_flags) < 0) {
        std::cerr << "SDL init failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    // 计算窗口初始大小（基于最终输出的 video_width/video_height，并适应屏幕）
    int win_w = 1280, win_h = 720;
    if (video_stream_idx != -1) {
        SDL_Rect display_bounds;
        // 默认显示器分辨率，若获取失败则设为 1920x1080
        if (SDL_GetDisplayBounds(0, &display_bounds) != 0) {
            display_bounds.w = 1920;
            display_bounds.h = 1080;
        }

        // 目标窗口大小不超过屏幕的 90%，并保持宽高比
        double ratio = std::min((double)display_bounds.w * 0.9 / video_width,
                                (double)display_bounds.h * 0.9 / video_height);
        win_w = (ratio < 1.0) ? (int)(video_width * ratio) : video_width;
        win_h = (ratio < 1.0) ? (int)(video_height * ratio) : video_height;
    }

    if (video_stream_idx != -1) {
        window = SDL_CreateWindow("FFmpeg + SDL2 音视频同步播放器",
                                  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  win_w, win_h,
                                  SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        if (!window) {
            std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
            return -1;
        }
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) {
            std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
            return -1;
        }
        // 纹理尺寸必须与最终输出一致
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
                                    video_width, video_height);
        if (!texture) {
            std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << std::endl;
            return -1;
        }
    }

    if (audio_stream_idx != -1) {
        SDL_AudioSpec desired, obtained;
        SDL_zero(desired);
        desired.freq = audio_samplerate;
        desired.format = AUDIO_S16SYS;
        desired.channels = audio_channels;
        desired.samples = 1024;
        desired.callback = nullptr;
        desired.userdata = nullptr;

        audio_dev = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
        if (audio_dev == 0) {
            std::cerr << "SDL_OpenAudioDevice failed: " << SDL_GetError() << std::endl;
            return -1;
        }
        SDL_PauseAudioDevice(audio_dev, 0);
    }

    // 6. 打印媒体信息
    std::cout << "\n========== Media Information ==========" << std::endl;
    std::cout << "Format: " << fmt_ctx->iformat->long_name << std::endl;
    if (audio_stream_idx != -1)
        std::cout << "Audio Codec: " << audio_codec_ctx->codec->long_name << std::endl;
    if (video_stream_idx != -1)
        std::cout << "Video Codec: " << video_codec_ctx->codec->long_name << std::endl;
    std::cout << "Duration: " << total_duration_sec / 60 << ":"
              << std::setfill('0') << std::setw(2) << total_duration_sec % 60 << std::endl;
    std::cout << "=======================================\n" << std::endl;

    // 7. 启动线程
    demux_thread = std::thread(demux_thread_func);
    if (audio_stream_idx != -1)
        audio_dec_thread = std::thread(audio_decoder_thread_func);
    if (video_stream_idx != -1)
        video_dec_thread = std::thread(video_decoder_thread_func);

    // 8. 主循环：事件处理 + 视频渲染 + 进度显示
    SDL_Event event;
    bool running = true;
    int64_t last_progress_update = 0; // 上次更新进度的时间（微秒）

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
        }

        if (video_stream_idx != -1) {
            VideoFrame vf;
            if (video_frame_queue.pop(vf, false)) {
                double audio_clock = get_audio_clock();

                // 音视频同步（音频为主）
                bool audio_finished = (audio_stream_idx == -1) ||
                                      (audio_packet_queue.empty() && SDL_GetQueuedAudioSize(audio_dev) == 0);
                if (!audio_finished) {
                    double diff = vf.pts - audio_clock;
                    const double sync_threshold = 0.1;   // 100ms
                    const double drop_threshold = -0.5;  // 落后500ms丢弃
                    if (diff > sync_threshold) {
                        int64_t wait_us = static_cast<int64_t>(diff * 1000000);
                        if (wait_us > 0 && wait_us < 500000) {
                            std::this_thread::sleep_for(std::chrono::microseconds(wait_us));
                        }
                    } else if (diff < drop_threshold) {
                        av_frame_free(&vf.frame);
                        continue;
                    }
                }

                // 更新纹理
                SDL_UpdateYUVTexture(texture, nullptr,
                                      vf.frame->data[0], vf.frame->linesize[0],
                                      vf.frame->data[1], vf.frame->linesize[1],
                                      vf.frame->data[2], vf.frame->linesize[2]);

                // 获取当前窗口大小
                int cur_win_w, cur_win_h;
                SDL_GetWindowSize(window, &cur_win_w, &cur_win_h);

                // 计算目标矩形（保持原始宽高比，居中显示）
                double scale = std::min((double)cur_win_w / video_width,
                                        (double)cur_win_h / video_height);
                SDL_Rect dest_rect;
                dest_rect.w = static_cast<int>(video_width * scale);
                dest_rect.h = static_cast<int>(video_height * scale);
                dest_rect.x = (cur_win_w - dest_rect.w) / 2;
                dest_rect.y = (cur_win_h - dest_rect.h) / 2;

                // 渲染
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, nullptr, &dest_rect);
                SDL_RenderPresent(renderer);

                // 每秒更新进度显示
                int64_t now = av_gettime();
                if (now - last_progress_update > 1000000) {
                    int cur_sec = static_cast<int>(audio_clock);
                    std::cout << "\rProgress: " << cur_sec / 60 << ":"
                              << std::setfill('0') << std::setw(2) << cur_sec % 60
                              << " / " << total_duration_sec / 60 << ":"
                              << std::setfill('0') << std::setw(2) << total_duration_sec % 60
                              << std::flush;
                    last_progress_update = now;
                }

                av_frame_free(&vf.frame);
            } else {
                SDL_Delay(10);
            }
        } else {
            // 纯音频模式：只更新进度
            if (audio_stream_idx != -1) {
                double audio_clock = get_audio_clock();
                int64_t now = av_gettime();
                if (now - last_progress_update > 1000000) {
                    int cur_sec = static_cast<int>(audio_clock);
                    std::cout << "\rProgress: " << cur_sec / 60 << ":"
                              << std::setfill('0') << std::setw(2) << cur_sec % 60
                              << " / " << total_duration_sec / 60 << ":"
                              << std::setfill('0') << std::setw(2) << total_duration_sec % 60
                              << std::flush;
                    last_progress_update = now;
                }
                SDL_Delay(100);
            }
        }

        // 检查播放结束：所有队列为空且音频播放完
        bool audio_done = (audio_stream_idx == -1) || (SDL_GetQueuedAudioSize(audio_dev) == 0);
        if (video_frame_queue.empty() && audio_packet_queue.empty() && video_packet_queue.empty() && audio_done) {
            running = false;
        }
    }

    std::cout << "\nPlayback finished." << std::endl;

    // 9. 清理
    quit = true;
    audio_packet_queue.abort();
    video_packet_queue.abort();
    video_frame_queue.abort();

    if (demux_thread.joinable()) demux_thread.join();
    if (audio_dec_thread.joinable()) audio_dec_thread.join();
    if (video_dec_thread.joinable()) video_dec_thread.join();

    if (audio_dev != 0) SDL_CloseAudioDevice(audio_dev);
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);

    if (swr_ctx) swr_free(&swr_ctx);
    if (sws_ctx) sws_freeContext(sws_ctx);
    if (filter_graph) avfilter_graph_free(&filter_graph);
    if (audio_codec_ctx) avcodec_free_context(&audio_codec_ctx);
    if (video_codec_ctx) avcodec_free_context(&video_codec_ctx);
    if (fmt_ctx) avformat_close_input(&fmt_ctx);

    SDL_Quit();
    return 0;
}