#include "common.h"

// 辅助函数：文件选择
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

// 辅助函数：ANSI转UTF8
std::string AnsiToUtf8(const std::string& ansiStr) {
    if (ansiStr.empty()) return "";
    int nwLen = MultiByteToWideChar(CP_ACP, 0, ansiStr.c_str(), -1, NULL, 0);
    std::vector<wchar_t> wbuf(nwLen);
    MultiByteToWideChar(CP_ACP, 0, ansiStr.c_str(), -1, wbuf.data(), nwLen);
    int nLen = WideCharToMultiByte(CP_UTF8, 0, wbuf.data(), -1, NULL, 0, NULL, NULL);
    std::vector<char> buf(nLen);
    WideCharToMultiByte(CP_UTF8, 0, wbuf.data(), -1, buf.data(), nLen, NULL, NULL);
    return std::string(buf.data());
}

// 音频时钟（基于已播放样本数）
double get_audio_clock(int64_t audio_samples_pushed, SDL_AudioDeviceID audio_dev, int audio_samplerate, int audio_channels) {
    if (audio_dev == 0 || audio_samplerate == 0) {
        static int64_t start_time = av_gettime();
        return (av_gettime() - start_time) / 1000000.0;
    }
    int64_t pushed = audio_samples_pushed;
    Uint32 queued_bytes = SDL_GetQueuedAudioSize(audio_dev);
    int queued_samples = queued_bytes / (2 * audio_channels); // 16位立体声：每样本2字节 * 2声道 = 4字节/立体声帧
    int64_t played = pushed - queued_samples;
    if (played < 0) played = 0;
    return static_cast<double>(played) / audio_samplerate;
}

// 获取帧的PTS（Presentation Time Stamp）
double get_frame_pts(AVFrame* frame, AVStream* stream) {
    if (!frame || !stream) return 0.0;
    int64_t pts = frame->pts;
    if (pts == AV_NOPTS_VALUE)
        pts = frame->best_effort_timestamp;
    if (pts == AV_NOPTS_VALUE)
        return 0.0;
    if (stream->start_time != AV_NOPTS_VALUE)
        pts -= stream->start_time;
    return pts * av_q2d(stream->time_base);
}

// 设置线程优先级
void set_thread_priority(std::thread::native_handle_type handle, int priority) {
    SetThreadPriority(reinterpret_cast<HANDLE>(handle), priority);
}
