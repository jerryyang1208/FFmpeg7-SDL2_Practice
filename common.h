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
#include <algorithm>
#include <cstdio>

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
    #include <libavfilter/avfilter.h>
    #include <libavfilter/buffersink.h>
    #include <libavfilter/buffersrc.h>
    #include <libavutil/opt.h>
    #include <SDL.h>
}

// 线程安全队列模板（支持最大容量和阻塞推入）
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

// 视频帧数据结构
struct VideoFrame {
    AVFrame* frame = nullptr;
    double pts = 0.0;
};

// 队列大小常量
const int DEFAULT_AUDIO_QUEUE_SIZE = 30;
const int DEFAULT_VIDEO_QUEUE_SIZE = 20;
const int DEFAULT_FRAME_QUEUE_SIZE = 10;

// 公共全局变量声明
extern AVFormatContext* fmt_ctx;
extern int audio_stream_idx;
extern int video_stream_idx;
extern AVCodecContext* audio_codec_ctx;
extern AVCodecContext* video_codec_ctx;
extern AVStream* audio_stream;
extern AVStream* video_stream;

extern SwrContext* swr_ctx;
extern int audio_samplerate;
extern int audio_channels;
extern AVSampleFormat audio_out_fmt;

extern AVFilterGraph* filter_graph;
extern AVFilterContext* buffersrc_ctx;
extern AVFilterContext* buffersink_ctx;
extern std::atomic<bool> video_filter_enabled;

extern SwsContext* sws_ctx;
extern int video_width;
extern int video_height;

extern SDL_Window* window;
extern SDL_Renderer* renderer;
extern SDL_Texture* texture;
extern SDL_AudioDeviceID audio_dev;

extern SafeQueue<AVPacket*> audio_packet_queue;
extern SafeQueue<AVPacket*> video_packet_queue;
extern SafeQueue<VideoFrame> video_frame_queue;

extern std::atomic<int64_t> audio_samples_pushed;
extern std::atomic<bool> quit;
extern std::thread demux_thread;
extern std::thread audio_dec_thread;
extern std::thread video_dec_thread;

extern int64_t total_duration_sec;

// 辅助函数
double get_audio_clock(int64_t samples_pushed, SDL_AudioDeviceID dev, int samplerate, int channels);
double get_frame_pts(AVFrame* frame, AVStream* stream);
std::string SelectFileDialog();
std::string AnsiToUtf8(const std::string& ansiStr);

// 线程优先级设置
void set_thread_priority(std::thread::native_handle_type handle, int priority);

#endif // COMMON_H
