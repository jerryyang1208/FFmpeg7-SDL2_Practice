#include <iostream>
#include <string>
#include <windows.h>
#include <commdlg.h>
#include <iomanip>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/log.h>
#include <libavutil/time.h>
#include <SDL.h>
}

// 解决路径中文乱码：ANSI 转换为 UTF-8 供 FFmpeg 使用
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

// 增强的文件选择器：支持 FLAC, Opus, WAV, M4A, OGG 等
std::string SelectFileDialog() {
    char szFile[260] = { 0 };
    OPENFILENAMEA ofn = { sizeof(ofn) };
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    // 更新过滤器以包含更多格式
    ofn.lpstrFilter = "Audio Files\0*.mp3;*.flac;*.opus;*.wav;*.m4a;*.ogg;*.aac\0All Files\0*.*\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn)) return std::string(szFile);
    return "";
}

int main(int argc, char* argv[]) {
    av_log_set_level(AV_LOG_ERROR);

    std::string ansiPath = SelectFileDialog();
    if (ansiPath.empty()) return 0;
    std::string utf8Path = AnsiToUtf8(ansiPath);

    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) return -1;

    AVFormatContext* pFormatCtx = nullptr;
    if (avformat_open_input(&pFormatCtx, utf8Path.c_str(), nullptr, nullptr) != 0) {
        std::cerr << "Could not open file." << std::endl;
        return -1;
    }
    if (avformat_find_stream_info(pFormatCtx, nullptr) < 0) return -1;

    // --- 查找音频流 ---
    int audioStream = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audioStream < 0) {
        std::cerr << "No audio stream found." << std::endl;
        return -1;
    }

    // --- 配置解码器 (通用) ---
    const AVCodec* pCodec = avcodec_find_decoder(pFormatCtx->streams[audioStream]->codecpar->codec_id);
    AVCodecContext* pCodecCtx = avcodec_alloc_context3(pCodec);
    avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[audioStream]->codecpar);
    if (avcodec_open2(pCodecCtx, pCodec, nullptr) < 0) return -1;

    // --- 打印详细元数据 ---
    int64_t duration = pFormatCtx->duration;
    int total_sec = (int)(duration / AV_TIME_BASE);
    
    AVDictionaryEntry *t = av_dict_get(pFormatCtx->metadata, "title", nullptr, 0);
    AVDictionaryEntry *a = av_dict_get(pFormatCtx->metadata, "artist", nullptr, 0);
    AVDictionaryEntry *b = av_dict_get(pFormatCtx->metadata, "album", nullptr, 0);

    std::cout << "\n========== Media Information ==========" << std::endl;
    std::cout << "Format:  " << pFormatCtx->iformat->long_name << std::endl;
    std::cout << "Codec:   " << pCodec->long_name << std::endl;
    std::cout << "Title:   " << (t ? t->value : "Unknown") << std::endl;
    std::cout << "Artist:  " << (a ? a->value : "Unknown") << std::endl;
    std::cout << "Album:   " << (b ? b->value : "Unknown") << std::endl;
    std::cout << "Time:    " << total_sec / 60 << ":" << std::setfill('0') << std::setw(2) << total_sec % 60 << std::endl;
    std::cout << "Sample:  " << pCodecCtx->sample_rate << " Hz" << std::endl;
    std::cout << "=======================================\n" << std::endl;

    // --- 音频重采样设置 (适应不同采样率和声道) ---
    SwrContext* swr_ctx = nullptr;
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO; // 强制输出双声道
    swr_alloc_set_opts2(&swr_ctx, 
                        &out_ch_layout, AV_SAMPLE_FMT_S16, pCodecCtx->sample_rate,
                        &pCodecCtx->ch_layout, pCodecCtx->sample_fmt, pCodecCtx->sample_rate, 0, nullptr);
    swr_init(swr_ctx);

    // --- SDL 配置 (必须与重采样输出一致) ---
    SDL_AudioSpec wanted = {0};
    wanted.freq = pCodecCtx->sample_rate;
    wanted.format = AUDIO_S16SYS; 
    wanted.channels = 2; // 对应 STEREO
    wanted.samples = 1024;
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &wanted, nullptr, 0);
    if (dev == 0) return -1;
    SDL_PauseAudioDevice(dev, 0);

    // --- 播放循环 ---
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    // 缓冲区大小计算：最大采样率 * 声道 * 字节深度的富余空间
    uint8_t* out_buf = (uint8_t*)av_malloc(192000 * 2); 
    
    int64_t last_update = 0;
    AVRational time_base = pFormatCtx->streams[audioStream]->time_base;

    std::cout << "Playing..." << std::endl;
    while (av_read_frame(pFormatCtx, pkt) >= 0) {
        if (pkt->stream_index == audioStream) {
            if (avcodec_send_packet(pCodecCtx, pkt) == 0) {
                while (avcodec_receive_frame(pCodecCtx, frame) == 0) {
                    
                    // 更新进度条
                    int64_t now = av_gettime_relative();
                    if (now - last_update > 200000) { // 200ms 更新一次
                        double pts = frame->pts * av_q2d(time_base);
                        int cur = (int)pts;
                        std::cout << "\r> Progress: [" 
                                  << cur / 60 << ":" << std::setfill('0') << std::setw(2) << cur % 60 << " / "
                                  << total_sec / 60 << ":" << std::setfill('0') << std::setw(2) << total_sec % 60 
                                  << "]" << std::flush;
                        last_update = now;
                    }

                    // 核心转换
                    int out_samples = swr_convert(swr_ctx, &out_buf, 192000, 
                                                 (const uint8_t**)frame->data, frame->nb_samples);
                    int out_size = av_samples_get_buffer_size(nullptr, 2, out_samples, AV_SAMPLE_FMT_S16, 1);
                    
                    SDL_QueueAudio(dev, out_buf, out_size);
                }
            }
        }
        av_packet_unref(pkt);
        // 控制 SDL 队列，防止内存溢出 (保持约 0.5s 的缓冲)
        while (SDL_GetQueuedAudioSize(dev) > 192000) SDL_Delay(10);
    }

    std::cout << "\n\nPlayback completed." << std::endl;
    SDL_Delay(1000); 

    // --- 资源释放 ---
    av_free(out_buf);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    swr_free(&swr_ctx);
    avcodec_free_context(&pCodecCtx);
    avformat_close_input(&pFormatCtx);
    SDL_CloseAudioDevice(dev);
    SDL_Quit();
    return 0;
}