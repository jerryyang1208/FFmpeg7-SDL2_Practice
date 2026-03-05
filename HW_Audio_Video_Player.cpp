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
    #include <libavutil/hwcontext.h>
    #include <libswresample/swresample.h>
    #include <libswscale/swscale.h>
    #include <SDL.h>
}

// ------------------------------------------------------------
// 线程安全队列模板
// ------------------------------------------------------------
template<typename T>
class SafeQueue {
public:
    void push(T value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(std::move(value));
        m_cond.notify_one();
    }

    bool pop(T& value, bool block = true) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (block) {
            m_cond.wait(lock, [this] { return !m_queue.empty() || m_abort; });
            if (m_abort) return false;
        } else {
            if (m_queue.empty()) return false;
        }
        value = std::move(m_queue.front());
        m_queue.pop();
        return true;
    }

    void abort() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_abort = true;
        m_cond.notify_all();
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
    std::condition_variable m_cond;
    bool m_abort = false;
};

// ------------------------------------------------------------
// 音频帧数据结构（重采样后）
// ------------------------------------------------------------
struct AudioFrame {
    uint8_t* data = nullptr;
    int samples = 0;
    double pts = 0.0;
};

// ------------------------------------------------------------
// 视频帧数据结构（转换后）
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

SwsContext* sws_ctx = nullptr;
int video_width = 0;
int video_height = 0;
AVPixelFormat video_out_fmt = AV_PIX_FMT_YUV420P;

// 硬件加速相关
AVBufferRef* hw_device_ctx = nullptr;
enum AVPixelFormat hw_pix_fmt = AV_PIX_FMT_NONE;
SwsContext* sws_ctx_nv12 = nullptr;   // 用于 NV12 -> YUV420P

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
SDL_Texture* texture = nullptr;
SDL_AudioDeviceID audio_dev = 0;

SafeQueue<AVPacket*> audio_packet_queue;
SafeQueue<AVPacket*> video_packet_queue;
SafeQueue<AudioFrame> audio_frame_queue;
SafeQueue<VideoFrame> video_frame_queue;

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
// 获取当前音频时钟
// ------------------------------------------------------------
double get_audio_clock() {
    if (audio_dev == 0 || audio_samplerate == 0) {
        static int64_t start_time = av_gettime();
        return (av_gettime() - start_time) / 1000000.0;
    }
    int64_t pushed = audio_samples_pushed.load(std::memory_order_relaxed);
    Uint32 queued_bytes = SDL_GetQueuedAudioSize(audio_dev);
    int queued_samples = queued_bytes / (2 * audio_channels);
    int64_t played = pushed - queued_samples;
    if (played < 0) played = 0;
    return static_cast<double>(played) / audio_samplerate;
}

// ------------------------------------------------------------
// 解复用线程
// ------------------------------------------------------------
void demux_thread_func() {
    AVPacket* pkt = av_packet_alloc();
    while (!quit) {
        int ret = av_read_frame(fmt_ctx, pkt);
        if (ret < 0) break;

        if (pkt->stream_index == audio_stream_idx) {
            AVPacket* clone = av_packet_clone(pkt);
            audio_packet_queue.push(clone);
        } else if (pkt->stream_index == video_stream_idx) {
            AVPacket* clone = av_packet_clone(pkt);
            video_packet_queue.push(clone);
        } else {
            av_packet_unref(pkt);
        }
    }
    av_packet_free(&pkt);
    audio_packet_queue.push(nullptr);
    video_packet_queue.push(nullptr);
}

// ------------------------------------------------------------
// 音频解码线程
// ------------------------------------------------------------
void audio_decoder_thread_func() {
    AVPacket* pkt = nullptr;
    AVFrame* frame = av_frame_alloc();
    uint8_t* out_buf = (uint8_t*)av_malloc(192000 * 4);

    while (!quit && audio_packet_queue.pop(pkt)) {
        if (!pkt) break;

        if (avcodec_send_packet(audio_codec_ctx, pkt) == 0) {
            while (avcodec_receive_frame(audio_codec_ctx, frame) == 0) {
                int out_samples = swr_convert(swr_ctx, &out_buf, 192000,
                                               (const uint8_t**)frame->data, frame->nb_samples);
                if (out_samples > 0) {
                    int bytes = out_samples * 4;
                    const int max_queued_bytes = audio_samplerate * 4 * 1.0;
                    while (!quit && SDL_GetQueuedAudioSize(audio_dev) > max_queued_bytes) {
                        SDL_Delay(10);
                    }
                    SDL_QueueAudio(audio_dev, out_buf, bytes);
                    audio_samples_pushed.fetch_add(out_samples, std::memory_order_relaxed);
                }
            }
        }
        av_packet_free(&pkt);
    }

    av_free(out_buf);
    av_frame_free(&frame);
}

// ------------------------------------------------------------
// 视频解码线程（支持硬件加速）
// ------------------------------------------------------------
void video_decoder_thread_func() {
    AVPacket* pkt = nullptr;
    AVFrame* frame = av_frame_alloc();
    AVFrame* sw_frame = nullptr;      // 用于 YUV420P 下载
    AVFrame* sw_frame_nv12 = nullptr; // 用于 NV12 下载

    while (!quit && video_packet_queue.pop(pkt)) {
        if (!pkt) break;

        if (avcodec_send_packet(video_codec_ctx, pkt) == 0) {
            while (avcodec_receive_frame(video_codec_ctx, frame) == 0) {
                AVFrame* out_frame = nullptr;

                // 硬件帧处理
                if (hw_pix_fmt != AV_PIX_FMT_NONE && frame->format == hw_pix_fmt) {
                    // 尝试直接下载为 YUV420P
                    if (!sw_frame) {
                        sw_frame = av_frame_alloc();
                        sw_frame->format = AV_PIX_FMT_YUV420P;
                        sw_frame->width = video_width;
                        sw_frame->height = video_height;
                        if (av_frame_get_buffer(sw_frame, 0) < 0) {
                            av_frame_free(&sw_frame);
                            sw_frame = nullptr;
                        }
                    }
                    if (sw_frame && av_hwframe_transfer_data(sw_frame, frame, 0) == 0) {
                        out_frame = av_frame_clone(sw_frame);
                    } else {
                        // 下载为 YUV420P 失败，尝试 NV12
                        if (!sw_frame_nv12) {
                            sw_frame_nv12 = av_frame_alloc();
                            sw_frame_nv12->format = AV_PIX_FMT_NV12;
                            sw_frame_nv12->width = video_width;
                            sw_frame_nv12->height = video_height;
                            if (av_frame_get_buffer(sw_frame_nv12, 0) < 0) {
                                av_frame_free(&sw_frame_nv12);
                                sw_frame_nv12 = nullptr;
                            }
                        }
                        if (sw_frame_nv12 && av_hwframe_transfer_data(sw_frame_nv12, frame, 0) == 0) {
                            out_frame = av_frame_alloc();
                            out_frame->format = AV_PIX_FMT_YUV420P;
                            out_frame->width = video_width;
                            out_frame->height = video_height;
                            if (av_frame_get_buffer(out_frame, 0) == 0) {
                                sws_scale(sws_ctx_nv12,
                                          sw_frame_nv12->data, sw_frame_nv12->linesize, 0, video_height,
                                          out_frame->data, out_frame->linesize);
                            } else {
                                av_frame_free(&out_frame);
                                out_frame = nullptr;
                            }
                        }
                    }
                }
                // 软件 YUV420P
                else if (frame->format == AV_PIX_FMT_YUV420P) {
                    out_frame = av_frame_clone(frame);
                }
                // 其他软件格式
                else {
                    out_frame = av_frame_alloc();
                    out_frame->format = video_out_fmt;
                    out_frame->width = video_width;
                    out_frame->height = video_height;
                    if (av_frame_get_buffer(out_frame, 0) < 0) {
                        av_frame_free(&out_frame);
                        continue;
                    }
                    sws_scale(sws_ctx,
                              frame->data, frame->linesize, 0, video_height,
                              out_frame->data, out_frame->linesize);
                }

                if (out_frame) {
                    double pts = frame->best_effort_timestamp * av_q2d(video_stream->time_base);
                    video_frame_queue.push({out_frame, pts});
                }
                av_frame_unref(frame);
            }
        }
        av_packet_free(&pkt);
    }

    av_frame_free(&frame);
    if (sw_frame) av_frame_free(&sw_frame);
    if (sw_frame_nv12) av_frame_free(&sw_frame_nv12);
}

// ------------------------------------------------------------
// 主函数
// ------------------------------------------------------------
int main(int argc, char* argv[]) {
    std::string ansiPath = SelectFileDialog();
    if (ansiPath.empty()) return 0;
    std::string utf8Path = AnsiToUtf8(ansiPath);

    av_log_set_level(AV_LOG_ERROR);

    if (avformat_open_input(&fmt_ctx, utf8Path.c_str(), nullptr, nullptr) != 0) {
        std::cerr << "Could not open file." << std::endl;
        return -1;
    }
    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) return -1;

    audio_stream_idx = -1;
    video_stream_idx = -1;
    for (int i = 0; i < fmt_ctx->nb_streams; i++) {
        AVStream* st = fmt_ctx->streams[i];
        if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audio_stream_idx == -1) {
            audio_stream_idx = i;
        } else if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video_stream_idx == -1) {
            if (st->disposition & AV_DISPOSITION_ATTACHED_PIC) continue;
            video_stream_idx = i;
        }
    }
    if (audio_stream_idx == -1 && video_stream_idx == -1) {
        std::cerr << "No audio or video stream found." << std::endl;
        return -1;
    }

    if (fmt_ctx->duration != AV_NOPTS_VALUE) {
        total_duration_sec = fmt_ctx->duration / AV_TIME_BASE;
    }

    // 初始化音频解码器
    if (audio_stream_idx != -1) {
        audio_stream = fmt_ctx->streams[audio_stream_idx];
        const AVCodec* audio_codec = avcodec_find_decoder(audio_stream->codecpar->codec_id);
        audio_codec_ctx = avcodec_alloc_context3(audio_codec);
        avcodec_parameters_to_context(audio_codec_ctx, audio_stream->codecpar);
        if (avcodec_open2(audio_codec_ctx, audio_codec, nullptr) < 0) return -1;

        AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
        swr_alloc_set_opts2(&swr_ctx,
                            &out_ch_layout, audio_out_fmt, audio_codec_ctx->sample_rate,
                            &audio_codec_ctx->ch_layout, audio_codec_ctx->sample_fmt, audio_codec_ctx->sample_rate,
                            0, nullptr);
        swr_init(swr_ctx);
        audio_samplerate = audio_codec_ctx->sample_rate;
    }

    // 初始化视频解码器
    if (video_stream_idx != -1) {
        video_stream = fmt_ctx->streams[video_stream_idx];
        const AVCodec* video_codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
        video_codec_ctx = avcodec_alloc_context3(video_codec);
        avcodec_parameters_to_context(video_codec_ctx, video_stream->codecpar);

        video_width = video_codec_ctx->width;
        video_height = video_codec_ctx->height;

        // 硬件加速初始化
        for (int i = 0;; i++) {
            const AVCodecHWConfig *config = avcodec_get_hw_config(video_codec, i);
            if (!config) break;
            if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                config->device_type == AV_HWDEVICE_TYPE_DXVA2) {
                hw_pix_fmt = config->pix_fmt;
                break;
            }
        }
        if (hw_pix_fmt != AV_PIX_FMT_NONE) {
            if (av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_DXVA2, nullptr, nullptr, 0) >= 0) {
                video_codec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
                video_codec_ctx->pix_fmt = hw_pix_fmt;
                // 创建 NV12 -> YUV420P 转换上下文
                sws_ctx_nv12 = sws_getContext(video_width, video_height, AV_PIX_FMT_NV12,
                                               video_width, video_height, AV_PIX_FMT_YUV420P,
                                               SWS_BILINEAR, nullptr, nullptr, nullptr);
                if (sws_ctx_nv12) {
                    std::cout << "Hardware acceleration (DXVA2) enabled." << std::endl;
                } else {
                    std::cerr << "Failed to create sws context for NV12, disabling hardware acceleration." << std::endl;
                    av_buffer_unref(&video_codec_ctx->hw_device_ctx);
                    hw_pix_fmt = AV_PIX_FMT_NONE;
                }
            } else {
                std::cerr << "Failed to create DXVA2 device, using software decoding." << std::endl;
                hw_pix_fmt = AV_PIX_FMT_NONE;
            }
        }

        video_codec_ctx->thread_count = std::thread::hardware_concurrency();
        video_codec_ctx->thread_type = FF_THREAD_FRAME;

        if (avcodec_open2(video_codec_ctx, video_codec, nullptr) < 0) return -1;

        sws_ctx = sws_getContext(video_width, video_height, video_codec_ctx->pix_fmt,
                                 video_width, video_height, video_out_fmt,
                                 SWS_BILINEAR, nullptr, nullptr, nullptr);
    }

    // 初始化 SDL
    Uint32 sdl_init_flags = SDL_INIT_AUDIO | SDL_INIT_TIMER;
    if (video_stream_idx != -1) sdl_init_flags |= SDL_INIT_VIDEO;
    if (SDL_Init(sdl_init_flags) < 0) {
        std::cerr << "SDL init failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    if (video_stream_idx != -1) {
        window = SDL_CreateWindow("FFmpeg + SDL2 音视频同步播放器",
                                  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  1280, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
                                    video_width, video_height);
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

    // 打印媒体信息
    std::cout << "\n========== Media Information ==========" << std::endl;
    std::cout << "Format: " << fmt_ctx->iformat->long_name << std::endl;
    if (audio_stream_idx != -1)
        std::cout << "Audio Codec: " << audio_codec_ctx->codec->long_name << std::endl;
    if (video_stream_idx != -1)
        std::cout << "Video Codec: " << video_codec_ctx->codec->long_name << std::endl;
    std::cout << "Duration: " << total_duration_sec / 60 << ":"
              << std::setfill('0') << std::setw(2) << total_duration_sec % 60 << std::endl;
    std::cout << "=======================================\n" << std::endl;

    // 启动线程
    demux_thread = std::thread(demux_thread_func);
    if (audio_stream_idx != -1) audio_dec_thread = std::thread(audio_decoder_thread_func);
    if (video_stream_idx != -1) video_dec_thread = std::thread(video_decoder_thread_func);

    // 主循环
    SDL_Event event;
    bool running = true;
    int64_t last_progress_update = 0;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
        }

        if (video_stream_idx != -1) {
            VideoFrame vf;
            if (video_frame_queue.pop(vf, false)) {
                double audio_clock = get_audio_clock();
                double diff = vf.pts - audio_clock;
                const double sync_threshold = 0.1;
                const double drop_threshold = -0.5;

                if (diff > sync_threshold) {
                    int64_t wait_us = static_cast<int64_t>(diff * 1000000);
                    if (wait_us > 0 && wait_us < 500000) {
                        std::this_thread::sleep_for(std::chrono::microseconds(wait_us));
                    }
                } else if (diff < drop_threshold) {
                    av_frame_free(&vf.frame);
                    continue;
                }

                SDL_UpdateYUVTexture(texture, nullptr,
                                      vf.frame->data[0], vf.frame->linesize[0],
                                      vf.frame->data[1], vf.frame->linesize[1],
                                      vf.frame->data[2], vf.frame->linesize[2]);
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, nullptr, nullptr);
                SDL_RenderPresent(renderer);

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

        if (video_frame_queue.empty() && audio_packet_queue.empty()) {
            if (audio_stream_idx != -1) {
                if (SDL_GetQueuedAudioSize(audio_dev) == 0) running = false;
            } else {
                running = false;
            }
        }
    }

    std::cout << "\nPlayback finished." << std::endl;

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
    if (sws_ctx_nv12) sws_freeContext(sws_ctx_nv12);
    if (audio_codec_ctx) avcodec_free_context(&audio_codec_ctx);
    if (video_codec_ctx) avcodec_free_context(&video_codec_ctx);
    if (hw_device_ctx) av_buffer_unref(&hw_device_ctx);
    if (fmt_ctx) avformat_close_input(&fmt_ctx);

    SDL_Quit();
    return 0;
}