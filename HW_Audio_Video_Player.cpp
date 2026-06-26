#include "common.h"

extern "C" {
#include <libavutil/hwcontext.h>
}

HwDevicePtr hw_device_ctx;
AVPixelFormat hw_pix_fmt = AV_PIX_FMT_NONE;

void video_decoder_thread_func() {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    auto frame = FramePtr(av_frame_alloc());
    auto sw_frame = FramePtr(av_frame_alloc());
    auto filt_frame = FramePtr(av_frame_alloc());
    if (!frame || !sw_frame || !filt_frame) { std::cerr << "Failed to allocate video frames." << std::endl; return; }

    PacketPtr pkt;
    while (g.video_packet_queue.pop(pkt) && pkt) {
        if (avcodec_send_packet(g.video_codec_ctx.get(), pkt.get()) == 0) {
            while (avcodec_receive_frame(g.video_codec_ctx.get(), frame.get()) == 0) {
                AVFrame* proc = frame.get();
                if (hw_pix_fmt != AV_PIX_FMT_NONE && frame->format == hw_pix_fmt) {
                    if (av_hwframe_transfer_data(sw_frame.get(), frame.get(), 0) == 0) {
                        sw_frame->pts = frame->pts; proc = sw_frame.get();
                    } else { std::cerr << "HW transfer failed." << std::endl; av_frame_unref(frame.get()); continue; }
                }

                if (g.video_filter_enabled) {
                    if (av_buffersrc_add_frame_flags(g.buffersrc_ctx, proc, AV_BUFFERSRC_FLAG_KEEP_REF) >= 0) {
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
                        sws_scale(g.sws_ctx.get(), proc->data, proc->linesize, 0, proc->height, out->data, out->linesize);
                        g.video_frame_queue.push({std::move(out), get_frame_pts(proc)});
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
        if (t == AVMEDIA_TYPE_VIDEO && g.video_stream_idx == -1) g.video_stream_idx = i;
    }
    if (g.fmt_ctx->duration != AV_NOPTS_VALUE) g.total_duration_sec = g.fmt_ctx->duration / AV_TIME_BASE;

    if (!init_audio_codec()) return -1;

    if (g.video_stream_idx != -1) {
        g.video_stream = g.fmt_ctx->streams[g.video_stream_idx];
        const AVCodec* codec = avcodec_find_decoder(g.video_stream->codecpar->codec_id);
        if (!codec) { std::cerr << "No video decoder." << std::endl; return -1; }
        auto vctx = CodecCtxPtr(avcodec_alloc_context3(codec));
        if (!vctx || avcodec_parameters_to_context(vctx.get(), g.video_stream->codecpar) < 0) { std::cerr << "Failed video codec ctx." << std::endl; return -1; }
        vctx->thread_count = 4; vctx->thread_type = FF_THREAD_FRAME;

        static const AVHWDeviceType hw_types[] = {AV_HWDEVICE_TYPE_D3D11VA, AV_HWDEVICE_TYPE_DXVA2, AV_HWDEVICE_TYPE_NONE};
        bool hw_ok = false;
        for (const auto* ht = hw_types; *ht != AV_HWDEVICE_TYPE_NONE && !hw_ok; ++ht) {
            for (int i = 0;; i++) {
                auto cfg = avcodec_get_hw_config(codec, i);
                if (!cfg) break;
                if (cfg->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && cfg->device_type == *ht) {
                    hw_pix_fmt = cfg->pix_fmt;
                    AVBufferRef* hw_raw = nullptr;
                    if (av_hwdevice_ctx_create(&hw_raw, *ht, nullptr, nullptr, 0) == 0) {
                        hw_device_ctx = HwDevicePtr(hw_raw);
                        vctx->hw_device_ctx = av_buffer_ref(hw_device_ctx.get());
                        std::cout << "HW acceleration: " << av_hwdevice_get_type_name(*ht) << std::endl;
                        hw_ok = true; break;
                    } else { hw_pix_fmt = AV_PIX_FMT_NONE; }
                }
            }
        }
        if (!hw_ok) std::cout << "HW not available, using SW decoding." << std::endl;

        if (avcodec_open2(vctx.get(), codec, nullptr) < 0) { std::cerr << "Failed to open video codec." << std::endl; return -1; }
        g.video_width = vctx->width; g.video_height = vctx->height;
        AVPixelFormat fpf = (hw_pix_fmt != AV_PIX_FMT_NONE) ? AV_PIX_FMT_NV12 : vctx->pix_fmt;
        if (!init_video_filter(fpf)) return -1;
        g.sws_ctx = SwsPtr(sws_getContext(vctx->width, vctx->height, fpf, vctx->width, vctx->height, AV_PIX_FMT_YUV420P, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr));
        if (!g.sws_ctx) { std::cerr << "Failed sws." << std::endl; return -1; }
        g.video_frame_pool = std::make_unique<FramePool>(FRAME_POOL_SIZE, g.video_width, g.video_height);
        g.video_codec_ctx = std::move(vctx);
    }

    if (!init_player()) return -1;

    std::cout << "\n========== Media Info ==========\nFormat: " << g.fmt_ctx->iformat->long_name << std::endl;
    if (g.audio_stream_idx != -1) std::cout << "Audio: " << g.audio_codec_ctx->codec->long_name << std::endl;
    if (g.video_stream_idx != -1) {
        double fps = g.video_stream->avg_frame_rate.num ? av_q2d(g.video_stream->avg_frame_rate) : av_q2d(g.video_stream->r_frame_rate);
        std::cout << "Video: " << g.video_codec_ctx->codec->long_name << ", " << g.video_width << "x" << g.video_height << ", " << fps << " fps, " << (hw_pix_fmt != AV_PIX_FMT_NONE ? "HW" : "SW") << std::endl;
    }
    std::cout << "Duration: " << g.total_duration_sec/60 << ":" << std::setfill('0') << std::setw(2) << g.total_duration_sec%60 << "\n===============================\n" << std::endl;

    g.demux_thread = std::thread(demux_thread_func);
    if (g.audio_stream_idx != -1) g.audio_dec_thread = std::thread(audio_decoder_thread_func);
    if (g.video_stream_idx != -1) g.video_dec_thread = std::thread(video_decoder_thread_func);

    main_loop();

    hw_device_ctx.reset();
    cleanup_resources();
    return 0;
}
