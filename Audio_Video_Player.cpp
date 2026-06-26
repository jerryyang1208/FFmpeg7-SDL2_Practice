#include "common.h"

void video_decoder_thread_func() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    auto frame = FramePtr(av_frame_alloc());
    auto filt_frame = FramePtr(av_frame_alloc());
    if (!frame || !filt_frame) { std::cerr << "Failed to allocate video frames." << std::endl; return; }

    PacketPtr pkt;
    while (g.video_packet_queue.pop(pkt) && pkt) {
        if (avcodec_send_packet(g.video_codec_ctx.get(), pkt.get()) == 0) {
            while (avcodec_receive_frame(g.video_codec_ctx.get(), frame.get()) == 0) {
                if (g.video_filter_enabled) {
                    if (av_buffersrc_add_frame_flags(g.buffersrc_ctx, frame.get(), AV_BUFFERSRC_FLAG_KEEP_REF) >= 0) {
                        while (av_buffersink_get_frame(g.buffersink_ctx, filt_frame.get()) >= 0) {
                            auto out = g.video_frame_pool->acquire();
                            if (out) {
                                av_image_copy(out->data, out->linesize, (const uint8_t**)filt_frame->data, filt_frame->linesize, AV_PIX_FMT_YUV420P, g.video_width, g.video_height);
                                g.video_frame_queue.push({std::move(out), get_frame_pts(filt_frame.get())});
                            }
                            av_frame_unref(filt_frame.get());
                        }
                    }
                } else {
                    auto out = g.video_frame_pool->acquire();
                    if (out) {
                        sws_scale(g.sws_ctx.get(), frame->data, frame->linesize, 0, frame->height, out->data, out->linesize);
                        g.video_frame_queue.push({std::move(out), get_frame_pts(frame.get())});
                    }
                }
                av_frame_unref(frame.get());
            }
        }
    }
}

int main(int, char**) {
    std::string path = SelectFileDialog();
    if (path.empty()) return 0;
    path = AnsiToUtf8(path);

    AVFormatContext* fmt_raw = nullptr;
    if (avformat_open_input(&fmt_raw, path.c_str(), nullptr, nullptr) != 0) { std::cerr << "Could not open file." << std::endl; return -1; }
    g.fmt_ctx = FormatCtxPtr(fmt_raw);
    if (avformat_find_stream_info(g.fmt_ctx.get(), nullptr) < 0) { std::cerr << "Could not find stream info." << std::endl; return -1; }

    for (unsigned i = 0; i < g.fmt_ctx->nb_streams; i++) {
        auto t = g.fmt_ctx->streams[i]->codecpar->codec_type;
        if (t == AVMEDIA_TYPE_AUDIO && g.audio_stream_idx == -1) g.audio_stream_idx = i;
        else if (t == AVMEDIA_TYPE_VIDEO && g.video_stream_idx == -1 && !(g.fmt_ctx->streams[i]->disposition & AV_DISPOSITION_ATTACHED_PIC))
            g.video_stream_idx = i;
    }
    if (g.audio_stream_idx == -1 && g.video_stream_idx == -1) { std::cerr << "No stream found." << std::endl; return -1; }
    if (g.fmt_ctx->duration != AV_NOPTS_VALUE) g.total_duration_sec = g.fmt_ctx->duration / AV_TIME_BASE;

    if (!init_audio_codec()) return -1;

    if (g.video_stream_idx != -1) {
        g.video_stream = g.fmt_ctx->streams[g.video_stream_idx];
        const AVCodec* codec = avcodec_find_decoder(g.video_stream->codecpar->codec_id);
        if (!codec) { std::cerr << "No video decoder." << std::endl; return -1; }
        auto vctx = CodecCtxPtr(avcodec_alloc_context3(codec));
        if (!vctx || avcodec_parameters_to_context(vctx.get(), g.video_stream->codecpar) < 0) { std::cerr << "Failed video codec ctx." << std::endl; return -1; }
        vctx->thread_count = 4; vctx->thread_type = FF_THREAD_FRAME;
        if (avcodec_open2(vctx.get(), codec, nullptr) < 0) { std::cerr << "Failed to open video codec." << std::endl; return -1; }
        g.video_width = vctx->width; g.video_height = vctx->height;
        if (!init_video_filter(vctx->pix_fmt)) return -1;
        g.sws_ctx = SwsPtr(sws_getContext(vctx->width, vctx->height, vctx->pix_fmt, vctx->width, vctx->height, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr));
        if (!g.sws_ctx) { std::cerr << "Failed sws." << std::endl; return -1; }
        g.video_frame_pool = std::make_unique<FramePool>(FRAME_POOL_SIZE, g.video_width, g.video_height);
        g.video_codec_ctx = std::move(vctx);
    }

    if (!init_player()) return -1;

    std::cout << "\n========== Media Info ==========\nFormat: " << g.fmt_ctx->iformat->long_name << std::endl;
    if (g.audio_stream_idx != -1) std::cout << "Audio: " << g.audio_codec_ctx->codec->long_name << std::endl;
    if (g.video_stream_idx != -1) {
        double fps = g.video_stream->avg_frame_rate.num ? av_q2d(g.video_stream->avg_frame_rate) : av_q2d(g.video_stream->r_frame_rate);
        std::cout << "Video: " << g.video_codec_ctx->codec->long_name << ", " << g.video_width << "x" << g.video_height << ", " << fps << " fps" << std::endl;
    }
    std::cout << "Duration: " << g.total_duration_sec/60 << ":" << std::setfill('0') << std::setw(2) << g.total_duration_sec%60 << "\n===============================\n" << std::endl;

    g.demux_thread = std::thread(demux_thread_func);
    if (g.audio_stream_idx != -1) g.audio_dec_thread = std::thread(audio_decoder_thread_func);
    if (g.video_stream_idx != -1) g.video_dec_thread = std::thread(video_decoder_thread_func);

    main_loop();
    cleanup_resources();
    return 0;
}
