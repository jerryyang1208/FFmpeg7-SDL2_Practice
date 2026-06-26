#include "common.h"

// ============ 全局播放器上下文 ============
PlayerContext g;

// ============ FramePool ============
FramePool::FramePool(int count, int w, int h) : m_w(w), m_h(h) {
    for (int i = 0; i < count; ++i) {
        AVFrame* f = av_frame_alloc();
        if (!f) continue;
        f->format = AV_PIX_FMT_YUV420P; f->width = w; f->height = h;
        if (av_frame_get_buffer(f, 0) < 0) { av_frame_free(&f); continue; }
        m_all.push_back(f); m_free.push(f);
    }
}
FramePool::~FramePool() { shutdown(); for (auto* f : m_all) av_frame_free(&f); }
FramePool::Ptr FramePool::acquire() {
    AVFrame* f = nullptr;
    return (m_free.pop(f) && f) ? Ptr(f, Recycler{this}) : Ptr(nullptr, Recycler{this});
}
void FramePool::recycle(AVFrame* f) {
    if (!f) return;
    av_frame_unref(f); f->format = AV_PIX_FMT_YUV420P; f->width = m_w; f->height = m_h;
    if (f->data[0] == nullptr) av_frame_get_buffer(f, 0);
    m_free.push(f);
}
void FramePool::shutdown() { m_free.abort(); }

// ============ 辅助函数 ============
std::string SelectFileDialog() {
    char szFile[260] = {0};
    OPENFILENAMEA ofn = {sizeof(ofn)};
    ofn.lpstrFile = szFile; ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Media Files\0*.mp4;*.mkv;*.avi;*.flv;*.mov;*.mp3;*.flac;*.wav;*.m4a;*.ogg\0All Files\0*.*\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameA(&ofn) ? std::string(szFile) : "";
}

std::string AnsiToUtf8(const std::string& s) {
    if (s.empty()) return "";
    int nw = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, nullptr, 0);
    std::vector<wchar_t> wb(nw);
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, wb.data(), nw);
    int n = WideCharToMultiByte(CP_UTF8, 0, wb.data(), -1, nullptr, 0, nullptr, nullptr);
    std::vector<char> b(n);
    WideCharToMultiByte(CP_UTF8, 0, wb.data(), -1, b.data(), n, nullptr, nullptr);
    return std::string(b.data());
}

double get_audio_clock() {
    if (g.audio_dev == 0 || g.audio_samplerate == 0) {
        static int64_t t0 = av_gettime();
        return (av_gettime() - t0) / 1000000.0;
    }
    int64_t played = g.audio_samples_pushed.load() - (int64_t)(SDL_GetQueuedAudioSize(g.audio_dev) / (2 * AUDIO_CHANNELS));
    return (double)(played > 0 ? played : 0) / g.audio_samplerate;
}

double get_frame_pts(const AVFrame* frame) {
    if (!frame || !g.video_stream) return 0.0;
    int64_t pts = frame->pts != AV_NOPTS_VALUE ? frame->pts : frame->best_effort_timestamp;
    return pts != AV_NOPTS_VALUE ? pts * av_q2d(g.video_stream->time_base) : 0.0;
}

// ============ 解复用线程 ============
void demux_thread_func() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    auto pkt = PacketPtr(av_packet_alloc());
    while (!g.quit) {
        if (av_read_frame(g.fmt_ctx.get(), pkt.get()) < 0) break;
        if (pkt->stream_index == g.audio_stream_idx) {
            g.audio_packet_queue.push(std::move(pkt));
            pkt = PacketPtr(av_packet_alloc());
        } else if (pkt->stream_index == g.video_stream_idx) {
            g.video_packet_queue.push(std::move(pkt));
            pkt = PacketPtr(av_packet_alloc());
        } else av_packet_unref(pkt.get());
    }
    g.audio_packet_queue.push(PacketPtr());
    g.video_packet_queue.push(PacketPtr());
}

// ============ 音频解码线程 ============
void audio_decoder_thread_func() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    auto frame = FramePtr(av_frame_alloc());
    auto out_buf = AvBufPtr((uint8_t*)av_malloc(MAX_AUDIO_SAMPLES * AUDIO_CHANNELS * 2));
    if (!frame || !out_buf) { std::cerr << "Failed to allocate audio resources." << std::endl; return; }

    PacketPtr pkt;
    while (g.audio_packet_queue.pop(pkt) && pkt) {
        if (avcodec_send_packet(g.audio_codec_ctx.get(), pkt.get()) == 0) {
            while (avcodec_receive_frame(g.audio_codec_ctx.get(), frame.get()) == 0) {
                uint8_t* buf_ptr = out_buf.get();
                int out_samples = swr_convert(g.swr_ctx.get(), &buf_ptr, MAX_AUDIO_SAMPLES * 2, (const uint8_t**)frame->data, frame->nb_samples);
                if (out_samples > 0) {
                    int max_queued = (int)(g.audio_samplerate * AUDIO_CHANNELS * 2 * 0.5);
                    while (!g.quit && SDL_GetQueuedAudioSize(g.audio_dev) > max_queued) SDL_Delay(5);
                    if (!g.quit) {
                        SDL_QueueAudio(g.audio_dev, out_buf.get(), out_samples * AUDIO_CHANNELS * 2);
                        g.audio_samples_pushed.fetch_add(out_samples, std::memory_order_relaxed);
                    }
                }
                av_frame_unref(frame.get());
            }
        }
    }
}

// ============ 初始化 ============
bool init_audio_codec() {
    if (g.audio_stream_idx == -1) return true;
    g.audio_stream = g.fmt_ctx->streams[g.audio_stream_idx];
    const AVCodec* codec = avcodec_find_decoder(g.audio_stream->codecpar->codec_id);
    if (!codec) { std::cerr << "Could not find audio decoder." << std::endl; return false; }
    auto ctx = CodecCtxPtr(avcodec_alloc_context3(codec));
    if (!ctx || avcodec_parameters_to_context(ctx.get(), g.audio_stream->codecpar) < 0) {
        std::cerr << "Failed audio codec ctx." << std::endl; return false;
    }
    if (avcodec_open2(ctx.get(), codec, nullptr) < 0) { std::cerr << "Failed to open audio codec." << std::endl; return false; }
    AVChannelLayout out_ch = AV_CHANNEL_LAYOUT_STEREO;
    SwrContext* swr_raw = nullptr;
    swr_alloc_set_opts2(&swr_raw, &out_ch, AUDIO_OUT_FMT, ctx->sample_rate,
                        &ctx->ch_layout, ctx->sample_fmt, ctx->sample_rate, 0, nullptr);
    auto swr = SwrPtr(swr_raw);
    if (swr_init(swr.get()) < 0) { std::cerr << "Failed to init audio resampler." << std::endl; return false; }
    g.audio_samplerate = ctx->sample_rate;
    g.audio_codec_ctx = std::move(ctx);
    g.swr_ctx = std::move(swr);
    return true;
}

bool init_video_filter(AVPixelFormat pix_fmt) {
    double theta = 0.0;
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    auto* dm = (const int32_t*)av_stream_get_side_data(g.video_stream, AV_PKT_DATA_DISPLAYMATRIX, nullptr);
    #pragma GCC diagnostic pop
    if (dm) theta = av_display_rotation_get(dm);

    std::string desc;
    if (theta == -90.0 || theta == 270.0) desc = "transpose=1";
    else if (theta == 90.0 || theta == -270.0) desc = "transpose=2";
    else if (theta == 180.0 || theta == -180.0) desc = "hflip,vflip";
    if (desc.empty()) return true;

    auto fg = FilterGraphPtr(avfilter_graph_alloc());
    if (!fg) return true;

    char args[512];
    snprintf(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             g.video_codec_ctx->width, g.video_codec_ctx->height, (int)pix_fmt,
             g.video_stream->time_base.num, g.video_stream->time_base.den,
             g.video_codec_ctx->sample_aspect_ratio.num, g.video_codec_ctx->sample_aspect_ratio.den);

    if (avfilter_graph_create_filter(&g.buffersrc_ctx, avfilter_get_by_name("buffer"), "in", args, nullptr, fg.get()) < 0 ||
        avfilter_graph_create_filter(&g.buffersink_ctx, avfilter_get_by_name("buffersink"), "out", nullptr, nullptr, fg.get()) < 0) {
        return true;
    }
    AVPixelFormat fmts[] = {AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE};
    av_opt_set_int_list(g.buffersink_ctx, "pix_fmts", fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);

    AVFilterInOut* out = avfilter_inout_alloc(), *in = avfilter_inout_alloc();
    out->name = av_strdup("in"); out->filter_ctx = g.buffersrc_ctx; out->pad_idx = 0; out->next = nullptr;
    in->name = av_strdup("out"); in->filter_ctx = g.buffersink_ctx; in->pad_idx = 0; in->next = nullptr;

    if (avfilter_graph_parse_ptr(fg.get(), desc.c_str(), &in, &out, nullptr) >= 0 &&
        avfilter_graph_config(fg.get(), nullptr) >= 0) {
        g.video_filter_enabled = true;
        g.video_width = g.buffersink_ctx->inputs[0]->w;
        g.video_height = g.buffersink_ctx->inputs[0]->h;
        std::cout << "Video rotation filter: " << desc << std::endl;
    } else {
        g.buffersrc_ctx = g.buffersink_ctx = nullptr;
        return true;
    }
    avfilter_inout_free(&in); avfilter_inout_free(&out);
    g.filter_graph = std::move(fg);
    return true;
}

bool init_player() {
    av_log_set_level(AV_LOG_ERROR);
    Uint32 flags = SDL_INIT_AUDIO | SDL_INIT_TIMER;
    if (g.video_stream_idx != -1) flags |= SDL_INIT_VIDEO;
    if (SDL_Init(flags) < 0) { std::cerr << "SDL init failed: " << SDL_GetError() << std::endl; return false; }

    if (g.video_stream_idx != -1) {
        SDL_Rect db;
        if (SDL_GetDisplayBounds(0, &db) != 0) { db.w = 1920; db.h = 1080; }
        double s = std::min(db.w * 0.9 / g.video_width, db.h * 0.9 / g.video_height);
        int ww = (s < 1.0) ? (int)(g.video_width * s) : g.video_width;
        int wh = (s < 1.0) ? (int)(g.video_height * s) : g.video_height;
        g.window = SDLWindowPtr(SDL_CreateWindow("FFmpeg+SDL2 Player", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ww, wh, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE));
        if (!g.window) { std::cerr << "SDL_CreateWindow: " << SDL_GetError() << std::endl; return false; }
        g.renderer = SDLRendererPtr(SDL_CreateRenderer(g.window.get(), -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC));
        if (!g.renderer) { std::cerr << "SDL_CreateRenderer: " << SDL_GetError() << std::endl; return false; }
        g.texture = SDLTexturePtr(SDL_CreateTexture(g.renderer.get(), SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, g.video_width, g.video_height));
        if (!g.texture) { std::cerr << "SDL_CreateTexture: " << SDL_GetError() << std::endl; return false; }
    }

    if (g.audio_stream_idx != -1) {
        SDL_AudioSpec spec = {g.audio_samplerate, AUDIO_S16SYS, (Uint8)AUDIO_CHANNELS, 0, 1024};
        g.audio_dev = SDL_OpenAudioDevice(nullptr, 0, &spec, nullptr, 0);
        if (g.audio_dev == 0) { std::cerr << "SDL_OpenAudioDevice: " << SDL_GetError() << std::endl; return false; }
        SDL_PauseAudioDevice(g.audio_dev, 0);
    }
    return true;
}

void cleanup_resources() {
    g.quit = true;
    g.audio_packet_queue.abort(); g.video_packet_queue.abort(); g.video_frame_queue.abort();
    if (g.video_frame_pool) g.video_frame_pool->shutdown();
    if (g.demux_thread.joinable()) g.demux_thread.join();
    if (g.audio_dec_thread.joinable()) g.audio_dec_thread.join();
    if (g.video_dec_thread.joinable()) g.video_dec_thread.join();
    if (g.audio_dev) SDL_CloseAudioDevice(g.audio_dev);
    g.texture.reset(); g.renderer.reset(); g.window.reset();
    g.swr_ctx.reset(); g.sws_ctx.reset(); g.filter_graph.reset();
    g.buffersrc_ctx = g.buffersink_ctx = nullptr;
    g.video_frame_pool.reset();
    g.audio_codec_ctx.reset(); g.video_codec_ctx.reset(); g.fmt_ctx.reset();
    SDL_Quit();
}

void render_video_frame(const VideoFrame& vf, const SDL_Rect& dst) {
    SDL_UpdateYUVTexture(g.texture.get(), nullptr,
                         vf.frame->data[0], vf.frame->linesize[0],
                         vf.frame->data[1], vf.frame->linesize[1],
                         vf.frame->data[2], vf.frame->linesize[2]);
    SDL_RenderClear(g.renderer.get());
    SDL_RenderCopy(g.renderer.get(), g.texture.get(), nullptr, &dst);
    SDL_RenderPresent(g.renderer.get());
}

void main_loop() {
    SDL_Event ev;
    bool running = true;
    int64_t last_update = 0;
    int cur_w = g.video_width, cur_h = g.video_height;
    float vaspect = (float)g.video_width / g.video_height;
    SDL_GetWindowSize(g.window.get(), &cur_w, &cur_h);

    SDL_Rect dst;
    auto calc_rect = [&]() {
        float wa = (float)cur_w / cur_h;
        if (wa > vaspect) { dst.w = (int)(cur_h*vaspect); dst.h = cur_h; dst.x = (cur_w-dst.w)/2; dst.y = 0; }
        else             { dst.w = cur_w; dst.h = (int)(cur_w/vaspect); dst.x = 0; dst.y = (cur_h-dst.h)/2; }
    };
    calc_rect();

    auto print_progress = [&](int cur) {
        if (cur > g.total_duration_sec) cur = g.total_duration_sec;
        std::cout << "\rProgress: " << cur/60 << ":" << std::setfill('0') << std::setw(2) << cur%60
                  << " / " << g.total_duration_sec/60 << ":" << std::setfill('0') << std::setw(2) << g.total_duration_sec%60 << std::flush;
    };

    while (running) {
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            else if (ev.type == SDL_WINDOWEVENT && (ev.window.event == SDL_WINDOWEVENT_RESIZED || ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)) {
                cur_w = ev.window.data1; cur_h = ev.window.data2; calc_rect();
            }
        }

        if (g.video_stream_idx != -1) {
            VideoFrame vf;
            if (g.video_frame_queue.pop(vf, false)) {
                double clk = get_audio_clock();
                bool af = (g.audio_stream_idx == -1) || (g.audio_packet_queue.empty() && SDL_GetQueuedAudioSize(g.audio_dev) == 0);
                if (!af) {
                    double diff = vf.pts - clk;
                    if (diff > 0.1) continue;
                    if (diff > 0.02) std::this_thread::sleep_for(std::chrono::microseconds((int)((diff-0.01)*1000000)));
                }
                render_video_frame(vf, dst);
                if (av_gettime() - last_update > 1000000) {
                    print_progress((int)clk);
                    last_update = av_gettime();
                }
            } else SDL_Delay(5);
        } else if (g.audio_stream_idx != -1) {
            if (av_gettime() - last_update > 1000000) {
                print_progress((int)get_audio_clock());
                last_update = av_gettime();
            }
            SDL_Delay(100);
        }

        bool audio_done = (g.audio_stream_idx == -1) || (g.audio_packet_queue.empty() && SDL_GetQueuedAudioSize(g.audio_dev) == 0);
        bool video_done = (g.video_stream_idx == -1) || (g.video_packet_queue.empty() && g.video_frame_queue.empty());
        if (audio_done && video_done) running = false;
    }
    std::cout << "\nPlayback finished." << std::endl;
}
