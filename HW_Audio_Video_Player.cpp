#include "common.h"

// 硬件加速相关全局变量
AVBufferRef* hw_device_ctx = nullptr;
enum AVPixelFormat hw_pix_fmt = AV_PIX_FMT_NONE;

// 本地变量
SDL_Rect dst_rect;
int current_window_w, current_window_h;
float video_aspect;

// 音频时钟（基于已播放样本数）
double get_audio_clock() {
    return ::get_audio_clock(audio_samples_pushed.load(), audio_dev, audio_samplerate, audio_channels);
}



void demux_thread_func() {
    // 设置线程优先级为高于正常
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    
    AVPacket* pkt = av_packet_alloc();
    while (!quit) {
        if (av_read_frame(fmt_ctx, pkt) < 0) break;
        if (pkt->stream_index == audio_stream_idx) audio_packet_queue.push(av_packet_clone(pkt));
        else if (pkt->stream_index == video_stream_idx) video_packet_queue.push(av_packet_clone(pkt));
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    audio_packet_queue.push(nullptr);
    video_packet_queue.push(nullptr);
}

// ---------- 优化后的音频解码线程 ----------
void audio_decoder_thread_func() {
    AVPacket* pkt = nullptr;
    AVFrame* frame = av_frame_alloc();
    // 增大输出缓冲区，避免重采样溢出
    uint8_t* out_buf = (uint8_t*)av_malloc(192000 * 4 * 2); // 两倍空间

    if (!frame || !out_buf) {
        std::cerr << "Failed to allocate audio frame or buffer." << std::endl;
        if (frame) av_frame_free(&frame);
        if (out_buf) av_free(out_buf);
        return;
    }

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    while (audio_packet_queue.pop(pkt) && pkt) {
        if (avcodec_send_packet(audio_codec_ctx, pkt) == 0) {
            while (avcodec_receive_frame(audio_codec_ctx, frame) == 0) {
                int out_samples = swr_convert(swr_ctx, &out_buf, 192000 * 2, (const uint8_t**)frame->data, frame->nb_samples);
                if (out_samples > 0) {
                    // 动态调整SDL队列大小，根据采样率和声道数计算
                    // 保持队列中最多有0.5秒的数据，避免过度堆积
                    const double max_queue_seconds = 0.5;
                    const int max_queued_bytes = static_cast<int>(audio_samplerate * audio_channels * 2 * max_queue_seconds);
                    
                    // 等待队列空间，使用指数退避策略，减少CPU占用
                    int wait_count = 0;
                    const int max_wait_count = 200; // 最多等待1秒
                    int delay_ms = 1; // 初始延迟1ms
                    while (!quit && SDL_GetQueuedAudioSize(audio_dev) > max_queued_bytes && wait_count < max_wait_count) {
                        SDL_Delay(delay_ms);
                        wait_count++;
                        // 指数退避，最多5ms
                        if (delay_ms < 5) {
                            delay_ms *= 2;
                        }
                    }
                    
                    // 只有在队列有足够空间时才推送数据
                    if (!quit && wait_count < max_wait_count) {
                        SDL_QueueAudio(audio_dev, out_buf, out_samples * 4);
                        audio_samples_pushed += out_samples;
                    }
                }
                av_frame_unref(frame);
            }
        }
        av_packet_free(&pkt);
    }
    av_free(out_buf);
    av_frame_free(&frame);
}

void video_decoder_thread_func() {
    // 设置线程优先级为高于正常，确保视频解码的流畅性
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    
    AVPacket* pkt = nullptr;
    AVFrame* frame = av_frame_alloc();
    AVFrame* sw_frame = av_frame_alloc();
    AVFrame* filt_frame = av_frame_alloc();
    AVFrame* out_frame = av_frame_alloc(); // 复用的输出帧

    if (!frame || !sw_frame || !filt_frame || !out_frame) {
        std::cerr << "Failed to allocate video frames." << std::endl;
        if (frame) av_frame_free(&frame);
        if (sw_frame) av_frame_free(&sw_frame);
        if (filt_frame) av_frame_free(&filt_frame);
        if (out_frame) av_frame_free(&out_frame);
        return;
    }

    // 初始化复用帧
    out_frame->format = AV_PIX_FMT_YUV420P;
    out_frame->width = video_width;
    out_frame->height = video_height;
    if (av_frame_get_buffer(out_frame, 0) < 0) {
        std::cerr << "Failed to allocate output frame buffer." << std::endl;
        av_frame_free(&frame);
        av_frame_free(&sw_frame);
        av_frame_free(&filt_frame);
        av_frame_free(&out_frame);
        return;
    }

    while (video_packet_queue.pop(pkt) && pkt) {
        if (avcodec_send_packet(video_codec_ctx, pkt) == 0) {
            while (avcodec_receive_frame(video_codec_ctx, frame) == 0) {
                AVFrame* process_frame = frame;
                if (hw_pix_fmt != AV_PIX_FMT_NONE && frame->format == hw_pix_fmt) {
                    if (av_hwframe_transfer_data(sw_frame, frame, 0) == 0) {
                        sw_frame->pts = frame->pts;
                        process_frame = sw_frame;
                    }
                }

                if (video_filter_enabled && buffersrc_ctx && buffersink_ctx) {
                    if (av_buffersrc_add_frame_flags(buffersrc_ctx, process_frame, AV_BUFFERSRC_FLAG_KEEP_REF) >= 0) {
                        while (av_buffersink_get_frame(buffersink_ctx, filt_frame) >= 0) {
                            double pts_sec = get_frame_pts(filt_frame, video_stream);
                            AVFrame* cloned_frame = av_frame_clone(filt_frame);
                            if (cloned_frame) {
                                video_frame_queue.push({cloned_frame, pts_sec});
                            }
                            av_frame_unref(filt_frame);
                        }
                    }
                } else {
                    // 复用out_frame，避免频繁的内存分配和释放
                    av_frame_unref(out_frame);
                    out_frame->format = AV_PIX_FMT_YUV420P;
                    out_frame->width = video_width;
                    out_frame->height = video_height;
                    
                    // 使用优化后的sws_scale进行格式转换
                    sws_scale(sws_ctx, process_frame->data, process_frame->linesize, 0, video_height, out_frame->data, out_frame->linesize);
                    
                    // 克隆帧以便队列使用
                    AVFrame* cloned_frame = av_frame_clone(out_frame);
                    if (cloned_frame) {
                        double pts_sec = get_frame_pts(process_frame, video_stream);
                        video_frame_queue.push({cloned_frame, pts_sec});
                    }
                }
                av_frame_unref(frame);
            }
        }
        av_packet_free(&pkt);
    }
    av_frame_free(&frame);
    av_frame_free(&sw_frame);
    av_frame_free(&filt_frame);
    av_frame_free(&out_frame);
}

// ------------------------------------------------------------
// 主函数
// ------------------------------------------------------------
int main(int argc, char* argv[]) {
	std::ios::sync_with_stdio(false); // 解除 C/C++ I/O同步（加速）
    std::cin.tie(nullptr);   // 解除 cin 与 cout 的绑定（减少刷新）

    std::string path = SelectFileDialog();
    if (path.empty()) return 0;
    std::string utf8Path = AnsiToUtf8(path);

    if (avformat_open_input(&fmt_ctx, utf8Path.c_str(), nullptr, nullptr) != 0) {
        std::cerr << "Could not open file." << std::endl;
        return -1;
    }
    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        std::cerr << "Could not find stream info." << std::endl;
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    for (int i = 0; i < (int)fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audio_stream_idx == -1) audio_stream_idx = i;
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video_stream_idx == -1) video_stream_idx = i;
    }

    if (fmt_ctx->duration != AV_NOPTS_VALUE) total_duration_sec = fmt_ctx->duration / AV_TIME_BASE;

    // 音频流处理
    if (audio_stream_idx != -1) {
        audio_stream = fmt_ctx->streams[audio_stream_idx];
        const AVCodec* a_codec = avcodec_find_decoder(audio_stream->codecpar->codec_id);
        audio_codec_ctx = avcodec_alloc_context3(a_codec);
        avcodec_parameters_to_context(audio_codec_ctx, audio_stream->codecpar);
        if (avcodec_open2(audio_codec_ctx, a_codec, nullptr) < 0) {
            std::cerr << "Failed to open audio codec." << std::endl;
            avcodec_free_context(&audio_codec_ctx);
            avformat_close_input(&fmt_ctx);
            return -1;
        }
        AVChannelLayout out_ch = AV_CHANNEL_LAYOUT_STEREO;
        swr_alloc_set_opts2(&swr_ctx, &out_ch, audio_out_fmt, audio_codec_ctx->sample_rate, &audio_codec_ctx->ch_layout, audio_codec_ctx->sample_fmt, audio_codec_ctx->sample_rate, 0, nullptr);
        if (swr_init(swr_ctx) < 0) {
            std::cerr << "Failed to initialize audio resampler." << std::endl;
            swr_free(&swr_ctx);
            avcodec_free_context(&audio_codec_ctx);
            avformat_close_input(&fmt_ctx);
            return -1;
        }
        audio_samplerate = audio_codec_ctx->sample_rate;
    }

    // 视频流处理 + 硬件加速
    if (video_stream_idx != -1) {
        video_stream = fmt_ctx->streams[video_stream_idx];
        const AVCodec* v_codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
        video_codec_ctx = avcodec_alloc_context3(v_codec);
        avcodec_parameters_to_context(video_codec_ctx, video_stream->codecpar);
        
        // 优化解码性能
        video_codec_ctx->thread_count = std::thread::hardware_concurrency();
        video_codec_ctx->thread_type = FF_THREAD_FRAME;
        
        // 尝试硬件加速 - 优先使用D3D11，然后是DXVA2
        AVHWDeviceType hw_type = AV_HWDEVICE_TYPE_NONE;
        for (int i = 0;; i++) {
            const AVCodecHWConfig* config = avcodec_get_hw_config(v_codec, i);
            if (!config) break;
            if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) {
                if (config->device_type == AV_HWDEVICE_TYPE_D3D11VA) {
                    hw_type = AV_HWDEVICE_TYPE_D3D11VA;
                    hw_pix_fmt = config->pix_fmt;
                    break;
                } else if (config->device_type == AV_HWDEVICE_TYPE_DXVA2 && hw_type == AV_HWDEVICE_TYPE_NONE) {
                    hw_type = AV_HWDEVICE_TYPE_DXVA2;
                    hw_pix_fmt = config->pix_fmt;
                }
            }
        }
        
        if (hw_pix_fmt != AV_PIX_FMT_NONE) {
            if (av_hwdevice_ctx_create(&hw_device_ctx, hw_type, nullptr, nullptr, 0) < 0) {
                std::cerr << "Failed to create hardware device context." << std::endl;
                hw_pix_fmt = AV_PIX_FMT_NONE;
            } else {
                video_codec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
                if (hw_type == AV_HWDEVICE_TYPE_D3D11VA) {
                    std::cout << "Hardware acceleration enabled: D3D11VA" << std::endl;
                } else if (hw_type == AV_HWDEVICE_TYPE_DXVA2) {
                    std::cout << "Hardware acceleration enabled: DXVA2" << std::endl;
                }
            }
        } else {
            std::cout << "Hardware acceleration not available, using software decoding." << std::endl;
        }
        
        if (avcodec_open2(video_codec_ctx, v_codec, nullptr) < 0) {
            std::cerr << "Failed to open video codec." << std::endl;
            if (hw_device_ctx) av_buffer_unref(&hw_device_ctx);
            avcodec_free_context(&video_codec_ctx);
            if (audio_codec_ctx) avcodec_free_context(&audio_codec_ctx);
            if (swr_ctx) swr_free(&swr_ctx);
            avformat_close_input(&fmt_ctx);
            return -1;
        }

        // 获取旋转并配置滤镜
        double theta = 0.0;
        const int32_t* display_matrix = nullptr;
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        display_matrix = (const int32_t*)av_stream_get_side_data(video_stream, AV_PKT_DATA_DISPLAYMATRIX, nullptr);
        #pragma GCC diagnostic pop
        if (display_matrix) {
            theta = av_display_rotation_get(display_matrix);
        }

        std::string filter_desc = "";
        if (theta == -90.0 || theta == 270.0) filter_desc = "transpose=1";
        else if (theta == 90.0 || theta == -270.0) filter_desc = "transpose=2";
        else if (theta == 180.0 || theta == -180.0) filter_desc = "hflip,vflip";

        if (!filter_desc.empty()) {
            filter_graph = avfilter_graph_alloc();
            char args[512];
            int src_pix_fmt = (hw_pix_fmt != AV_PIX_FMT_NONE) ? AV_PIX_FMT_NV12 : video_codec_ctx->pix_fmt;
            snprintf(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                     video_codec_ctx->width, video_codec_ctx->height, src_pix_fmt,
                     video_stream->time_base.num, video_stream->time_base.den,
                     video_codec_ctx->sample_aspect_ratio.num, video_codec_ctx->sample_aspect_ratio.den);

            avfilter_graph_create_filter(&buffersrc_ctx, avfilter_get_by_name("buffer"), "in", args, nullptr, filter_graph);
            avfilter_graph_create_filter(&buffersink_ctx, avfilter_get_by_name("buffersink"), "out", nullptr, nullptr, filter_graph);
            enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
            av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);

            AVFilterInOut *out = avfilter_inout_alloc(), *in = avfilter_inout_alloc();
            out->name = av_strdup("in"); out->filter_ctx = buffersrc_ctx; out->pad_idx = 0; out->next = nullptr;
            in->name = av_strdup("out"); in->filter_ctx = buffersink_ctx; in->pad_idx = 0; in->next = nullptr;

            if (avfilter_graph_parse_ptr(filter_graph, filter_desc.c_str(), &in, &out, nullptr) >= 0 && avfilter_graph_config(filter_graph, nullptr) >= 0) {
                video_filter_enabled = true;
                video_width = buffersink_ctx->inputs[0]->w;
                video_height = buffersink_ctx->inputs[0]->h;
            }
            avfilter_inout_free(&in); avfilter_inout_free(&out);
        } else {
            video_width = video_codec_ctx->width;
            video_height = video_codec_ctx->height;
        }

        // 使用更快的缩放算法
        sws_ctx = sws_getContext(video_codec_ctx->width, video_codec_ctx->height, 
                                 (hw_pix_fmt != AV_PIX_FMT_NONE ? AV_PIX_FMT_NV12 : video_codec_ctx->pix_fmt),
                                 video_codec_ctx->width, video_codec_ctx->height, AV_PIX_FMT_YUV420P, 
                                 SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    }

    // 窗口自适应初始化
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
		std::cerr << "SDL init failed: " << SDL_GetError() << std::endl;
		if (hw_device_ctx) av_buffer_unref(&hw_device_ctx);
		if (swr_ctx) swr_free(&swr_ctx);
		if (sws_ctx) sws_freeContext(sws_ctx);
		if (filter_graph) avfilter_graph_free(&filter_graph);
		if (audio_codec_ctx) avcodec_free_context(&audio_codec_ctx);
		if (video_codec_ctx) avcodec_free_context(&video_codec_ctx);
		avformat_close_input(&fmt_ctx);
		return -1;
	}

	// 计算初始窗口大小（不超过屏幕90%）
    int window_w = video_width, window_h = video_height;
    if (video_stream_idx != -1) {
        SDL_Rect display_bounds;
        if (SDL_GetDisplayBounds(0, &display_bounds) == 0) {
            double scale = std::min((display_bounds.w * 0.9) / video_width, 
                                   (display_bounds.h * 0.9) / video_height);
            if (scale < 1.0) {
                window_w = (int)(video_width * scale);
                window_h = (int)(video_height * scale);
            }
        }
    }

    // 创建可调整大小的窗口
    window = SDL_CreateWindow("FFmpeg + SDL2 音视频同步播放器", 
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                              window_w, window_h, 
                              SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        if (hw_device_ctx) av_buffer_unref(&hw_device_ctx);
        if (swr_ctx) swr_free(&swr_ctx);
        if (sws_ctx) sws_freeContext(sws_ctx);
        if (filter_graph) avfilter_graph_free(&filter_graph);
        if (audio_codec_ctx) avcodec_free_context(&audio_codec_ctx);
        if (video_codec_ctx) avcodec_free_context(&video_codec_ctx);
        avformat_close_input(&fmt_ctx);
        SDL_Quit();
        return -1;
    }
    
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        if (hw_device_ctx) av_buffer_unref(&hw_device_ctx);
        if (swr_ctx) swr_free(&swr_ctx);
        if (sws_ctx) sws_freeContext(sws_ctx);
        if (filter_graph) avfilter_graph_free(&filter_graph);
        if (audio_codec_ctx) avcodec_free_context(&audio_codec_ctx);
        if (video_codec_ctx) avcodec_free_context(&video_codec_ctx);
        avformat_close_input(&fmt_ctx);
        SDL_Quit();
        return -1;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, video_width, video_height);
    if (!texture) {
        std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        if (hw_device_ctx) av_buffer_unref(&hw_device_ctx);
        if (swr_ctx) swr_free(&swr_ctx);
        if (sws_ctx) sws_freeContext(sws_ctx);
        if (filter_graph) avfilter_graph_free(&filter_graph);
        if (audio_codec_ctx) avcodec_free_context(&audio_codec_ctx);
        if (video_codec_ctx) avcodec_free_context(&video_codec_ctx);
        avformat_close_input(&fmt_ctx);
        SDL_Quit();
        return -1;
    }

    // 音频设备初始化（保持不变）
    if (audio_stream_idx != -1) {
        SDL_AudioSpec spec = { audio_samplerate, AUDIO_S16SYS, (Uint8)audio_channels, 0, 4096 };
        audio_dev = SDL_OpenAudioDevice(nullptr, 0, &spec, nullptr, 0);
        if (audio_dev == 0) {
            std::cerr << "SDL_OpenAudioDevice failed: " << SDL_GetError() << std::endl;
            SDL_DestroyTexture(texture);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            if (hw_device_ctx) av_buffer_unref(&hw_device_ctx);
            if (swr_ctx) swr_free(&swr_ctx);
            if (sws_ctx) sws_freeContext(sws_ctx);
            if (filter_graph) avfilter_graph_free(&filter_graph);
            if (audio_codec_ctx) avcodec_free_context(&audio_codec_ctx);
            if (video_codec_ctx) avcodec_free_context(&video_codec_ctx);
            avformat_close_input(&fmt_ctx);
            SDL_Quit();
            return -1;
        }
        SDL_PauseAudioDevice(audio_dev, 0);
    }

    // 打印媒体信息
    std::cout << "\n========== Media Information ==========" << std::endl;
    std::cout << "Format: " << fmt_ctx->iformat->long_name << std::endl;
    if (audio_stream_idx != -1) {
		int64_t audio_bitrate = audio_stream->codecpar->bit_rate;
        std::cout << "Audio Codec: " << audio_codec_ctx->codec->long_name << std::endl;
		std::cout << "Audio Bitrate: " << audio_bitrate / 1000 << " kbps" << std::endl;
	}
    if (video_stream_idx != -1) {
		int64_t video_bitrate = video_stream->codecpar->bit_rate;
        std::cout << "Video Codec: " << video_codec_ctx->codec->long_name << std::endl;
		std::cout << "Video Bitrate: " << video_bitrate / 1000 << " kbps" << std::endl;
		std::cout << "Resolution: " << video_codec_ctx->width << "x" << video_codec_ctx->height << std::endl;
		double fps = 0.0;
        if (video_stream->avg_frame_rate.den && video_stream->avg_frame_rate.num) {
            fps = av_q2d(video_stream->avg_frame_rate);
        } else if (video_stream->r_frame_rate.den && video_stream->r_frame_rate.num) {
            fps = av_q2d(video_stream->r_frame_rate);
        }
        if (fps > 0) {
            std::cout << "Frame Rate: " << std::fixed << std::setprecision(2) << fps << " fps" << std::endl;
        }
	}
    std::cout << "Duration: " << total_duration_sec / 60 << ":"
              << std::setfill('0') << std::setw(2) << total_duration_sec % 60 << std::endl;
    std::cout << "=======================================\n" << std::endl;

    demux_thread = std::thread(demux_thread_func);
    if (audio_stream_idx != -1) audio_dec_thread = std::thread(audio_decoder_thread_func);
    if (video_stream_idx != -1) video_dec_thread = std::thread(video_decoder_thread_func);

    // 等待视频帧队列有一定数量的帧，减少开头黑屏
    if (video_stream_idx != -1) {
        int wait_cnt = 0;
        while (!quit && video_frame_queue.size() < 3 && wait_cnt < 100) {
            SDL_Delay(10);
            wait_cnt++;
        }
    }

    SDL_Event ev;
	int64_t last_update = 0;
	int64_t last_audio_check = av_gettime();

	if (video_stream_idx != -1) {
		// 初始化当前窗口大小和视频宽高比
		current_window_w = window_w;
		current_window_h = window_h;
		video_aspect = (float)video_width / video_height;
		
		// 计算初始目标矩形（保持宽高比）
		float window_aspect = (float)current_window_w / current_window_h;
		if (window_aspect > video_aspect) {
			// 窗口更宽，上下黑边
			dst_rect.w = (int)(current_window_h * video_aspect);
			dst_rect.h = current_window_h;
			dst_rect.x = (current_window_w - dst_rect.w) / 2;
			dst_rect.y = 0;
		} else {
			// 窗口更高，左右黑边
			dst_rect.w = current_window_w;
			dst_rect.h = (int)(current_window_w / video_aspect);
			dst_rect.x = 0;
			dst_rect.y = (current_window_h - dst_rect.h) / 2;
		}
	}

	while (!quit) {
		while (SDL_PollEvent(&ev)) {
			if (ev.type == SDL_QUIT) {
				quit = true;
			}
			// 添加窗口大小改变事件处理
			else if (ev.type == SDL_WINDOWEVENT) {
				if (ev.window.event == SDL_WINDOWEVENT_RESIZED || 
					ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
					// 更新当前窗口大小
					current_window_w = ev.window.data1;
					current_window_h = ev.window.data2;
					
					// 重新计算保持宽高比的渲染区域
					float window_aspect = (float)current_window_w / current_window_h;
					
					if (window_aspect > video_aspect) {
						// 窗口更宽，上下黑边
						dst_rect.w = (int)(current_window_h * video_aspect);
						dst_rect.h = current_window_h;
						dst_rect.x = (current_window_w - dst_rect.w) / 2;
						dst_rect.y = 0;
					} else {
						// 窗口更高，左右黑边
						dst_rect.w = current_window_w;
						dst_rect.h = (int)(current_window_w / video_aspect);
						dst_rect.x = 0;
						dst_rect.y = (current_window_h - dst_rect.h) / 2;
					}
				}
			}
		}

		// 视频渲染（如果有）
		if (video_stream_idx != -1) {
			VideoFrame vf;
			if (video_frame_queue.pop(vf, false)) {
				double audio_clock = get_audio_clock();
				double diff = vf.pts - audio_clock;
				
				// 优化时间同步，避免开头倍速跟进
				if (diff > 0.1) {
					// 差距太大，直接跳过
					av_frame_free(&vf.frame);
					continue;
				} else if (diff > 0.02) {
					// 小差距，适当等待
					std::this_thread::sleep_for(std::chrono::microseconds((int)((diff - 0.01) * 1000000)));
				}

				SDL_UpdateYUVTexture(texture, nullptr,
										vf.frame->data[0], vf.frame->linesize[0],
										vf.frame->data[1], vf.frame->linesize[1],
										vf.frame->data[2], vf.frame->linesize[2]);

				SDL_RenderClear(renderer);
				SDL_RenderCopy(renderer, texture, nullptr, &dst_rect);
				SDL_RenderPresent(renderer);

				av_frame_free(&vf.frame);
			} else {
				SDL_Delay(1); // 减少延迟，提高响应性
			}
		}

		// 进度条更新（每秒更新一次）
		if (av_gettime() - last_update > 1000000) {
			double current_time = get_audio_clock();
			// 确保当前时间不超过总时长
			if (current_time > total_duration_sec) {
				current_time = total_duration_sec;
			}
			int minutes = (int)current_time / 60;
			int seconds = (int)current_time % 60;
			int total_minutes = (int)total_duration_sec / 60;
			int total_seconds = (int)total_duration_sec % 60;
			
			// 使用 printf
			printf("\rTime: %02d:%02d / %02d:%02d", minutes, seconds, total_minutes, total_seconds);
			fflush(stdout);
			
			last_update = av_gettime();
		}

		// 退出条件检查
		if (audio_stream_idx != -1) {
			// 检查是否播放完毕
			if (audio_packet_queue.empty() && video_packet_queue.empty() && video_frame_queue.empty() && SDL_GetQueuedAudioSize(audio_dev) == 0) {
				// 最后一次更新进度条，确保显示完整时间
				double final_audio_clock = get_audio_clock();
				if (final_audio_clock > total_duration_sec) {
					final_audio_clock = total_duration_sec;
				}
				int minutes = (int)final_audio_clock / 60;
				int seconds = (int)final_audio_clock % 60;
				int total_minutes = (int)total_duration_sec / 60;
				int total_seconds = (int)total_duration_sec % 60;
				printf("\rTime: %02d:%02d / %02d:%02d", minutes, seconds, total_minutes, total_seconds);
				fflush(stdout);
				printf("\n");
				quit = true;
			} else {
				last_audio_check = av_gettime();
			}
		} else {
			// 没有音频流，检查视频队列
			if (video_packet_queue.empty() && video_frame_queue.empty()) {
				// 最后一次更新进度条，确保显示完整时间
				int total_minutes = (int)total_duration_sec / 60;
				int total_seconds = (int)total_duration_sec % 60;
				printf("\rTime: %02d:%02d / %02d:%02d", total_minutes, total_seconds, total_minutes, total_seconds);
				fflush(stdout);
				printf("\n");
				quit = true;
			} else {
				SDL_Delay(10);
			}
		}
	}

    quit = true;
    audio_packet_queue.abort();
    video_packet_queue.abort();
    video_frame_queue.abort();

    demux_thread.join();
    if (audio_dec_thread.joinable()) audio_dec_thread.join();
    if (video_dec_thread.joinable()) video_dec_thread.join();

    if (filter_graph) avfilter_graph_free(&filter_graph);
    if (sws_ctx) sws_freeContext(sws_ctx);
    if (swr_ctx) swr_free(&swr_ctx);
    if (audio_codec_ctx) avcodec_free_context(&audio_codec_ctx);
    if (video_codec_ctx) avcodec_free_context(&video_codec_ctx);
    if (fmt_ctx) avformat_close_input(&fmt_ctx);
    if (hw_device_ctx) av_buffer_unref(&hw_device_ctx);
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}