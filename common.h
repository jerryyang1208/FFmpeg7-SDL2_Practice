#ifndef COMMON_H
#define COMMON_H

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
#include <memory>

#include <windows.h>
#include <commdlg.h>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/time.h>
    #include <libavutil/channel_layout.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/display.h>
    #include <libswresample/swresample.h>
    #include <libswscale/swscale.h>
    #include <libavfilter/avfilter.h>
    #include <libavfilter/buffersink.h>
    #include <libavfilter/buffersrc.h>
    #include <libavutil/opt.h>
    #include <SDL.h>
}

// ============ RAII 删除器 ============
struct PacketDeleter    { void operator()(AVPacket* p)       const { av_packet_free(&p); } };
struct FrameDeleter     { void operator()(AVFrame* f)        const { av_frame_free(&f); } };
struct AvFreeDeleter    { void operator()(void* p)           const { av_free(p); } };
struct FormatCtxDeleter { void operator()(AVFormatContext* f) const { avformat_close_input(&f); } };
struct CodecCtxDeleter  { void operator()(AVCodecContext* c) const { avcodec_free_context(&c); } };
struct SwrDeleter       { void operator()(SwrContext* s)     const { swr_free(&s); } };
struct SwsDeleter       { void operator()(SwsContext* s)     const { sws_freeContext(s); } };
struct FilterGraphDeleter { void operator()(AVFilterGraph* g) const { avfilter_graph_free(&g); } };
struct SDLWindowDeleter { void operator()(SDL_Window* w)     const { SDL_DestroyWindow(w); } };
struct SDLRendererDeleter{ void operator()(SDL_Renderer* r)  const { SDL_DestroyRenderer(r); } };
struct SDLTextureDeleter{ void operator()(SDL_Texture* t)    const { SDL_DestroyTexture(t); } };
struct HwDeviceDeleter  { void operator()(AVBufferRef* b)    const { av_buffer_unref(&b); } };

using PacketPtr       = std::unique_ptr<AVPacket, PacketDeleter>;
using FramePtr        = std::unique_ptr<AVFrame, FrameDeleter>;
using AvBufPtr        = std::unique_ptr<uint8_t, AvFreeDeleter>;
using FormatCtxPtr    = std::unique_ptr<AVFormatContext, FormatCtxDeleter>;
using CodecCtxPtr     = std::unique_ptr<AVCodecContext, CodecCtxDeleter>;
using SwrPtr          = std::unique_ptr<SwrContext, SwrDeleter>;
using SwsPtr          = std::unique_ptr<SwsContext, SwsDeleter>;
using FilterGraphPtr  = std::unique_ptr<AVFilterGraph, FilterGraphDeleter>;
using SDLWindowPtr    = std::unique_ptr<SDL_Window, SDLWindowDeleter>;
using SDLRendererPtr  = std::unique_ptr<SDL_Renderer, SDLRendererDeleter>;
using SDLTexturePtr   = std::unique_ptr<SDL_Texture, SDLTextureDeleter>;
using HwDevicePtr     = std::unique_ptr<AVBufferRef, HwDeviceDeleter>;

// ============ 常量 ============
constexpr int AUDIO_CHANNELS = 2;
constexpr auto AUDIO_OUT_FMT = AV_SAMPLE_FMT_S16;
constexpr int AUDIO_QUEUE_SIZE = 60;
constexpr int VIDEO_QUEUE_SIZE = 20;
constexpr int FRAME_QUEUE_SIZE = 10;
constexpr int FRAME_POOL_SIZE = 16;
constexpr int MAX_AUDIO_SAMPLES = 192000;

// ============ 线程安全队列 ============
template<typename T>
class SafeQueue {
public:
    explicit SafeQueue(size_t max = 0) : m_max(max) {}
    bool push(T v) {
        std::unique_lock<std::mutex> lk(m_mtx);
        if (m_max > 0) { m_cv_push.wait(lk, [this]{ return m_q.size() < m_max || m_abort; }); if (m_abort) return false; }
        m_q.push(std::move(v)); m_cv_pop.notify_one(); return true;
    }
    bool pop(T& v, bool block = true) {
        std::unique_lock<std::mutex> lk(m_mtx);
        if (block) { m_cv_pop.wait(lk, [this]{ return !m_q.empty() || m_abort; }); if (m_abort) return false; }
        else if (m_q.empty()) return false;
        v = std::move(m_q.front()); m_q.pop();
        if (m_max > 0) m_cv_push.notify_one(); return true;
    }
    void abort() { std::lock_guard<std::mutex> lk(m_mtx); m_abort = true; m_cv_pop.notify_all(); m_cv_push.notify_all(); }
    size_t size() { std::lock_guard<std::mutex> lk(m_mtx); return m_q.size(); }
    bool empty() { std::lock_guard<std::mutex> lk(m_mtx); return m_q.empty(); }
private:
    std::queue<T> m_q; std::mutex m_mtx;
    std::condition_variable m_cv_pop, m_cv_push;
    size_t m_max; bool m_abort = false;
};

// ============ 帧池 ============
class FramePool {
public:
    FramePool(int count, int w, int h);
    ~FramePool();
    struct Recycler { FramePool* pool; void operator()(AVFrame* f) const { pool->recycle(f); } };
    using Ptr = std::unique_ptr<AVFrame, Recycler>;
    Ptr acquire();
    void shutdown();
private:
    void recycle(AVFrame* f);
    SafeQueue<AVFrame*> m_free;
    std::vector<AVFrame*> m_all;
    int m_w, m_h;
};

struct VideoFrame { FramePool::Ptr frame; double pts = 0.0; };

// ============ 播放器上下文 ============
struct PlayerContext {
    FormatCtxPtr fmt_ctx;
    int audio_stream_idx = -1, video_stream_idx = -1;
    CodecCtxPtr audio_codec_ctx, video_codec_ctx;
    AVStream* audio_stream = nullptr, *video_stream = nullptr;
    SwrPtr swr_ctx;
    int audio_samplerate = 0;
    FilterGraphPtr filter_graph;
    AVFilterContext* buffersrc_ctx = nullptr, *buffersink_ctx = nullptr;
    std::atomic<bool> video_filter_enabled{false};
    SwsPtr sws_ctx;
    int video_width = 0, video_height = 0;
    SDLWindowPtr window;
    SDLRendererPtr renderer;
    SDLTexturePtr texture;
    SDL_AudioDeviceID audio_dev = 0;
    SafeQueue<PacketPtr> audio_packet_queue{AUDIO_QUEUE_SIZE};
    SafeQueue<PacketPtr> video_packet_queue{VIDEO_QUEUE_SIZE};
    SafeQueue<VideoFrame> video_frame_queue{FRAME_QUEUE_SIZE};
    std::unique_ptr<FramePool> video_frame_pool;
    std::atomic<int64_t> audio_samples_pushed{0};
    std::atomic<bool> quit{false};
    std::thread demux_thread, audio_dec_thread, video_dec_thread;
    int64_t total_duration_sec = 0;
};
extern PlayerContext g;

// ============ 函数声明 ============
std::string SelectFileDialog();
std::string AnsiToUtf8(const std::string& s);
double get_audio_clock();
double get_frame_pts(const AVFrame* frame);

void demux_thread_func();
void audio_decoder_thread_func();
bool init_audio_codec();
bool init_video_filter(AVPixelFormat pix_fmt);
bool init_player();
void cleanup_resources();
void render_video_frame(const VideoFrame& vf, const SDL_Rect& dst);
void main_loop();

#endif
