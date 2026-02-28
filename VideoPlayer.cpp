#include <iostream>
#include <string>
#include <vector>
#include <windows.h> // 用于文件选择对话框
#include <commdlg.h>

extern "C" {
    #include <SDL.h>
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/avutil.h>
    #include <libavutil/time.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
}

// 简单的 Windows 文件选择器
std::string SelectFileDialog() {
    char szFile[260] = { 0 };
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Video Files\0*.mp4;*.mkv;*.avi;*.flv;*.mov\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        return std::string(szFile);
    }
    return "";
}

int main(int argc, char* argv[]) {
    // 1. 选择文件
    std::string filename = SelectFileDialog();
    if (filename.empty()) return 0;

    // 2. 初始化 SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
        return -1;
    }

    // 3. 打开视频文件
    AVFormatContext* formatCtx = nullptr;
    if (avformat_open_input(&formatCtx, filename.c_str(), nullptr, nullptr) != 0) {
        return -1;
    }

    if (avformat_find_stream_info(formatCtx, nullptr) < 0) return -1;

    int videoStream = -1;
    for (int i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
            break;
        }
    }
    if (videoStream == -1) return -1;

    // 4. 配置解码器
    AVCodecParameters* codecpar = formatCtx->streams[videoStream]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, codecpar);
    if (avcodec_open2(codecCtx, codec, nullptr) < 0) return -1;

    // 5. 创建窗口 (根据视频原始宽高)
    SDL_Window* window = SDL_CreateWindow("FFmpeg 7.0 Player", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                                          1280, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    // 6. 准备图像转换上下文 (解决画面扭曲和格式问题)
    // 统一转换为 YUV420P 渲染，这能保证绝大多数视频正常显示
    SwsContext* sws_ctx = sws_getContext(
        codecCtx->width, codecCtx->height, codecCtx->pix_fmt,
        codecCtx->width, codecCtx->height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, NULL, NULL, NULL
    );

    // 创建对应分辨率的纹理
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, 
                                             codecCtx->width, codecCtx->height);

    AVFrame* frame = av_frame_alloc();
    AVFrame* frameYUV = av_frame_alloc();
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, codecCtx->width, codecCtx->height, 1);
    uint8_t* buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(frameYUV->data, frameYUV->linesize, buffer, AV_PIX_FMT_YUV420P, codecCtx->width, codecCtx->height, 1);

    AVPacket* packet = av_packet_alloc();
    
    // 7. 播放控制变量
    bool running = true;
    SDL_Event event;
    double time_base = av_q2d(formatCtx->streams[videoStream]->time_base);
    int64_t start_time = av_gettime();

    while (running) {
        // 事件处理
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
        }

        if (av_read_frame(formatCtx, packet) >= 0) {
            if (packet->stream_index == videoStream) {
                if (avcodec_send_packet(codecCtx, packet) == 0) {
                    while (avcodec_receive_frame(codecCtx, frame) == 0) {
                        
                        // --- 同步处理 ---
                        double pts = frame->best_effort_timestamp * time_base;
                        int64_t now_time = av_gettime() - start_time;
                        if (pts > (double)now_time / 1000000.0) {
                            av_usleep((pts * 1000000.0) - now_time);
                        }

                        // --- 格式转换 ---
                        sws_scale(sws_ctx, (uint8_t const* const*)frame->data, frame->linesize, 0, 
                                  codecCtx->height, frameYUV->data, frameYUV->linesize);

                        // --- 渲染 ---
                        SDL_UpdateYUVTexture(texture, NULL, 
                                             frameYUV->data[0], frameYUV->linesize[0],
                                             frameYUV->data[1], frameYUV->linesize[1],
                                             frameYUV->data[2], frameYUV->linesize[2]);

                        SDL_RenderClear(renderer);
                        // SDL_RenderCopy 的最后一个参数传 NULL 会自动拉伸填满窗口，
                        // 如果需要保持比例，可以计算显示区域 Rect
                        SDL_RenderCopy(renderer, texture, NULL, NULL); 
                        SDL_RenderPresent(renderer);
                    }
                }
            }
            av_packet_unref(packet);
        } else {
            // 文件播放结束
            running = false;
        }
    }

    // 8. 释放资源
    av_free(buffer);
    av_frame_free(&frameYUV);
    av_frame_free(&frame);
    av_packet_free(&packet);
    sws_freeContext(sws_ctx);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&formatCtx);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}