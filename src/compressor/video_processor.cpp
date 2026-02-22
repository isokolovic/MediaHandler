#include "compressor/video_processor.h"
#include <fstream>
#include <format>
#include <vector>
#include <algorithm>

namespace media_handler::compressor {

    VideoProcessor::VideoProcessor(const utils::Config& cfg, std::shared_ptr<spdlog::logger> logger)
        : config(cfg), logger(std::move(logger)) {
    }

    bool VideoProcessor::file_exists_and_readable(const std::filesystem::path& path) {
        std::error_code ec;
        return std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec);
    }

    bool VideoProcessor::verify_video_signature(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;

        unsigned char sig[12];
        file.read(reinterpret_cast<char*>(sig), 12);

        if (file.gcount() >= 12 && memcmp(&sig[4], "ftyp", 4) == 0) return true;
        if (file.gcount() >= 12 && memcmp(sig, "RIFF", 4) == 0 && memcmp(&sig[8], "AVI ", 4) == 0) return true;
        if (file.gcount() >= 4 && memcmp(sig, "\x1A\x45\xDF\xA3", 4) == 0) return true;

        return false;
    }

    ProcessResult VideoProcessor::fallback_copy(const std::filesystem::path& input, const std::filesystem::path& output) {
        try {
            std::ifstream src(input, std::ios::binary);
            if (!src) return ProcessResult::Error("Failed to open input for copy");

            std::ofstream dst(output, std::ios::binary);
            if (!dst) return ProcessResult::Error("Failed to open output for copy");

            dst << src.rdbuf();
            return ProcessResult::OK();
        }
        catch (const std::filesystem::filesystem_error& e) {
            return ProcessResult::Error(std::format("Filesystem error during copy: {}", e.what()));
        }
    }

    ProcessResult VideoProcessor::compress(const std::filesystem::path& input, const std::filesystem::path& output) {
        try {
            if (!file_exists_and_readable(input)) {
                return ProcessResult::Error("Input file missing or unreadable");
            }

            if (!verify_video_signature(input)) {
                return ProcessResult::Error("Not a valid video file");
            }

            AVFormatContext* input_ctx = nullptr;
            AVFormatContext* output_ctx = nullptr;
            AVCodecContext* decoder_ctx = nullptr;
            AVCodecContext* encoder_ctx = nullptr;
            const AVCodec* decoder = nullptr;
            const AVCodec* encoder = nullptr;
            AVStream* in_stream = nullptr;
            AVStream* out_stream = nullptr;
            AVPacket* packet = nullptr;
            AVFrame* frame = nullptr;
            AVFrame* scaled_frame = nullptr;
            SwsContext* sws_ctx = nullptr;

            int video_stream_index = -1;
            int ret = 0;
                        
            // Open input

            ret = avformat_open_input(&input_ctx, input.string().c_str(), nullptr, nullptr);
            if (ret < 0)
                return ProcessResult::Error(std::format("Failed to open input file: {}", ret));

            ret = avformat_find_stream_info(input_ctx, nullptr);
            if (ret < 0) {
                avformat_close_input(&input_ctx);
                return ProcessResult::Error("Failed to find stream info");
            }

            for (unsigned int i = 0; i < input_ctx->nb_streams; i++) {
                if (input_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                    video_stream_index = i;
                    break;
                }
            }

            if (video_stream_index == -1) {
                avformat_close_input(&input_ctx);
                return ProcessResult::Error("No video stream found");
            }

            in_stream = input_ctx->streams[video_stream_index];

            // Measure source video bitrate - target 40 % of it.

            int64_t src_bitrate = in_stream->codecpar->bit_rate;
            if (src_bitrate <= 0)
                src_bitrate = input_ctx->bit_rate;
            if (src_bitrate <= 0) {
                int64_t file_size = avio_size(input_ctx->pb);
                double  duration = static_cast<double>(input_ctx->duration) / AV_TIME_BASE;
                if (file_size > 0 && duration > 0.0)
                    src_bitrate = static_cast<int64_t>((file_size * 8) / duration);
            }
            src_bitrate = std::clamp(src_bitrate, (int64_t)200'000, (int64_t)8'000'000);

            const int64_t target_bitrate = static_cast<int64_t>(src_bitrate * 0.40);
            const int64_t max_bitrate = static_cast<int64_t>(src_bitrate * 0.50);
            const int64_t buf_size = max_bitrate * 2;

            logger->info("Source bitrate: {}kbps  →  target: {}kbps  max: {}kbps",
                src_bitrate / 1000, target_bitrate / 1000, max_bitrate / 1000);

            // Decoder — all cores

            decoder = avcodec_find_decoder(in_stream->codecpar->codec_id);
            if (!decoder) {
                avformat_close_input(&input_ctx);
                return ProcessResult::Error("Decoder not found");
            }

            decoder_ctx = avcodec_alloc_context3(decoder);
            if (!decoder_ctx) {
                avformat_close_input(&input_ctx);
                return ProcessResult::Error("Failed to allocate decoder context");
            }

            ret = avcodec_parameters_to_context(decoder_ctx, in_stream->codecpar);
            if (ret < 0) {
                avcodec_free_context(&decoder_ctx);
                avformat_close_input(&input_ctx);
                return ProcessResult::Error("Failed to copy codec parameters");
            }

            decoder_ctx->thread_count = 0;
            decoder_ctx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;

            ret = avcodec_open2(decoder_ctx, decoder, nullptr);
            if (ret < 0) {
                avcodec_free_context(&decoder_ctx);
                avformat_close_input(&input_ctx);
                return ProcessResult::Error("Failed to open decoder");
            }

            // Output context + global metadata

            avformat_alloc_output_context2(&output_ctx, nullptr, nullptr, output.string().c_str());
            if (!output_ctx) {
                avcodec_free_context(&decoder_ctx);
                avformat_close_input(&input_ctx);
                return ProcessResult::Error("Failed to create output context");
            }

            // Preserve all container-level metadata (creation_time, location, etc.)
            av_dict_copy(&output_ctx->metadata, input_ctx->metadata, 0);

            // Map streams: video gets re-encoded, everything else is stream-copied
            std::vector<int> stream_map(input_ctx->nb_streams, -1);

            for (unsigned int i = 0; i < input_ctx->nb_streams; i++) {
                AVStream* in_s = input_ctx->streams[i];

                if ((int)i == video_stream_index) {
                    out_stream = avformat_new_stream(output_ctx, nullptr);
                    if (!out_stream) {
                        avformat_free_context(output_ctx);
                        avcodec_free_context(&decoder_ctx);
                        avformat_close_input(&input_ctx);
                        return ProcessResult::Error("Failed to create video output stream");
                    }
                    stream_map[i] = out_stream->index;
                }
                else {
                    AVStream* out_s = avformat_new_stream(output_ctx, nullptr);
                    if (!out_s) continue;
                    ret = avcodec_parameters_copy(out_s->codecpar, in_s->codecpar);
                    if (ret < 0) continue;
                    out_s->codecpar->codec_tag = 0;
                    out_s->time_base = in_s->time_base;
                    av_dict_copy(&out_s->metadata, in_s->metadata, 0);
                    stream_map[i] = out_s->index;
                }
            }

            // Encoder selection — hardware first, software fallback
            const char* encoder_names[] = { "h264_nvenc", "h264_amf", "h264_qsv", "libx264", nullptr };
            for (int i = 0; encoder_names[i]; i++) {
                encoder = avcodec_find_encoder_by_name(encoder_names[i]);
                if (encoder) { logger->info("Using encoder: {}", encoder_names[i]); break; }
            }

            if (!encoder) {
                avformat_free_context(output_ctx);
                avcodec_free_context(&decoder_ctx);
                avformat_close_input(&input_ctx);
                return ProcessResult::Error("No H.264 encoder found");
            }

            encoder_ctx = avcodec_alloc_context3(encoder);
            if (!encoder_ctx) {
                avformat_free_context(output_ctx);
                avcodec_free_context(&decoder_ctx);
                avformat_close_input(&input_ctx);
                return ProcessResult::Error("Failed to allocate encoder context");
            }

            encoder_ctx->width = decoder_ctx->width;
            encoder_ctx->height = decoder_ctx->height;
            encoder_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

            AVRational frame_rate = av_guess_frame_rate(input_ctx, in_stream, nullptr);
            if (frame_rate.num <= 0 || frame_rate.den <= 0) frame_rate = in_stream->avg_frame_rate;
            if (frame_rate.num <= 0 || frame_rate.den <= 0) {
                logger->warn("Could not determine frame rate, defaulting to 30 fps");
                frame_rate = { 30, 1 };
            }
            encoder_ctx->time_base = av_inv_q(frame_rate);
            encoder_ctx->framerate = frame_rate;
            encoder_ctx->thread_count = 0;

            if (output_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                encoder_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

            std::string encoder_name = encoder->name;

            if (encoder_name == "h264_nvenc") {
                // p4 balances speed and compression.
                av_opt_set(encoder_ctx->priv_data, "preset", "p4", 0);
                av_opt_set(encoder_ctx->priv_data, "tune", "hq", 0);
                av_opt_set(encoder_ctx->priv_data, "rc", "vbr", 0);
                av_opt_set_int(encoder_ctx->priv_data, "cq", 36, 0);
                av_opt_set_int(encoder_ctx->priv_data, "b", target_bitrate, 0);
                av_opt_set_int(encoder_ctx->priv_data, "maxrate", max_bitrate, 0);
                av_opt_set_int(encoder_ctx->priv_data, "bufsize", buf_size, 0);
                av_opt_set_int(encoder_ctx->priv_data, "max_b_frames", 0, 0);
                encoder_ctx->bit_rate = target_bitrate;
                logger->info("NVENC: cq=36, target={}kbps, max={}kbps", target_bitrate / 1000, max_bitrate / 1000);
            }
            else if (encoder_name == "h264_amf") {
                av_opt_set(encoder_ctx->priv_data, "quality", "balanced", 0);
                av_opt_set(encoder_ctx->priv_data, "rc", "vbr_latency", 0);
                av_opt_set_int(encoder_ctx->priv_data, "qp_i", 34, 0);
                av_opt_set_int(encoder_ctx->priv_data, "qp_p", 36, 0);
                av_opt_set_int(encoder_ctx->priv_data, "qp_b", 38, 0);
                av_opt_set_int(encoder_ctx->priv_data, "max_b_frames", 0, 0);
                encoder_ctx->bit_rate = target_bitrate;
                logger->info("AMF: qp=34/36/38, target={}kbps", target_bitrate / 1000);
            }
            else if (encoder_name == "h264_qsv") {
                av_opt_set(encoder_ctx->priv_data, "preset", "fast", 0);
                av_opt_set_int(encoder_ctx->priv_data, "global_quality", 36, 0);
                av_opt_set_int(encoder_ctx->priv_data, "max_b_frames", 0, 0);
                encoder_ctx->bit_rate = target_bitrate;
                logger->info("QSV: quality=36, target={}kbps", target_bitrate / 1000);
            }
            else if (encoder_name == "libx264") {
                // 'faster': better compression than 'ultrafast'; 3x faster than 'medium'.
                // CRF 33: targets 40% perceived quality.
                // Maxrate: caps size spikes at 50% source bitrate.
                encoder_ctx->bit_rate = 0; // pure CRF mode
                encoder_ctx->rc_max_rate = max_bitrate;
                encoder_ctx->rc_buffer_size = (int)buf_size;
                av_opt_set_int(encoder_ctx->priv_data, "crf", 33, 0);
                av_opt_set(encoder_ctx->priv_data, "preset", "faster", 0);
                av_opt_set(encoder_ctx->priv_data, "tune", "fastdecode", 0);
                logger->info("libx264: crf=33, preset=faster, maxrate={}kbps", max_bitrate / 1000);
            }
            else {
                encoder_ctx->bit_rate = target_bitrate;
                encoder_ctx->rc_max_rate = max_bitrate;
                encoder_ctx->rc_buffer_size = (int)buf_size;
                logger->warn("Unknown encoder '{}', target={}kbps", encoder_name, target_bitrate / 1000);
            }

            ret = avcodec_open2(encoder_ctx, encoder, nullptr);
            if (ret < 0) {
                avcodec_free_context(&encoder_ctx);
                avformat_free_context(output_ctx);
                avcodec_free_context(&decoder_ctx);
                avformat_close_input(&input_ctx);
                return ProcessResult::Error(std::format("Failed to open encoder: {}", ret));
            }

            ret = avcodec_parameters_from_context(out_stream->codecpar, encoder_ctx);
            if (ret < 0) {
                avcodec_free_context(&encoder_ctx);
                avformat_free_context(output_ctx);
                avcodec_free_context(&decoder_ctx);
                avformat_close_input(&input_ctx);
                return ProcessResult::Error("Failed to copy encoder parameters");
            }

            out_stream->time_base = encoder_ctx->time_base;
            av_dict_copy(&out_stream->metadata, in_stream->metadata, 0); // rotation, lang, etc.

            // Open output file
            if (!(output_ctx->oformat->flags & AVFMT_NOFILE)) {
                ret = avio_open(&output_ctx->pb, output.string().c_str(), AVIO_FLAG_WRITE);
                if (ret < 0) {
                    avcodec_free_context(&encoder_ctx);
                    avformat_free_context(output_ctx);
                    avcodec_free_context(&decoder_ctx);
                    avformat_close_input(&input_ctx);
                    return ProcessResult::Error("Failed to open output file");
                }
            }

            AVDictionary* mux_opts = nullptr;
            av_dict_set(&mux_opts, "movflags", "faststart", 0);
            ret = avformat_write_header(output_ctx, &mux_opts);
            av_dict_free(&mux_opts);
            if (ret < 0) {
                if (output_ctx->pb) avio_closep(&output_ctx->pb);
                avcodec_free_context(&encoder_ctx);
                avformat_free_context(output_ctx);
                avcodec_free_context(&decoder_ctx);
                avformat_close_input(&input_ctx);
                return ProcessResult::Error("Failed to write output header");
            }

            // Scaler (only when pixel format conversion is needed)
            if (decoder_ctx->pix_fmt != AV_PIX_FMT_YUV420P) {
                sws_ctx = sws_getContext(
                    decoder_ctx->width, decoder_ctx->height, decoder_ctx->pix_fmt,
                    encoder_ctx->width, encoder_ctx->height, encoder_ctx->pix_fmt,
                    SWS_FAST_BILINEAR, nullptr, nullptr, nullptr
                );
                if (!sws_ctx) {
                    av_write_trailer(output_ctx);
                    if (output_ctx->pb) avio_closep(&output_ctx->pb);
                    avcodec_free_context(&encoder_ctx);
                    avformat_free_context(output_ctx);
                    avcodec_free_context(&decoder_ctx);
                    avformat_close_input(&input_ctx);
                    return ProcessResult::Error("Failed to create scaling context");
                }
            }

            // Allocate working buffers
            packet = av_packet_alloc();
            frame = av_frame_alloc();
            scaled_frame = av_frame_alloc();

            if (!packet || !frame || !scaled_frame) {
                if (scaled_frame) av_frame_free(&scaled_frame);
                if (frame)        av_frame_free(&frame);
                if (packet)       av_packet_free(&packet);
                if (sws_ctx)      sws_freeContext(sws_ctx);
                av_write_trailer(output_ctx);
                if (output_ctx->pb) avio_closep(&output_ctx->pb);
                avcodec_free_context(&encoder_ctx);
                avformat_free_context(output_ctx);
                avcodec_free_context(&decoder_ctx);
                avformat_close_input(&input_ctx);
                return ProcessResult::Error("Failed to allocate packet/frame");
            }

            scaled_frame->format = encoder_ctx->pix_fmt;
            scaled_frame->width = encoder_ctx->width;
            scaled_frame->height = encoder_ctx->height;
            ret = av_frame_get_buffer(scaled_frame, 0);
            if (ret < 0) {
                av_frame_free(&scaled_frame);
                av_frame_free(&frame);
                av_packet_free(&packet);
                if (sws_ctx) sws_freeContext(sws_ctx);
                av_write_trailer(output_ctx);
                if (output_ctx->pb) avio_closep(&output_ctx->pb);
                avcodec_free_context(&encoder_ctx);
                avformat_free_context(output_ctx);
                avcodec_free_context(&decoder_ctx);
                avformat_close_input(&input_ctx);
                return ProcessResult::Error("Failed to allocate scaled frame buffer");
            }

            // Main packet loop
            int64_t pts_fallback = 0;

            while (av_read_frame(input_ctx, packet) >= 0) {
                int si = packet->stream_index;

                if (si == video_stream_index) {
                    ret = avcodec_send_packet(decoder_ctx, packet);
                    if (ret < 0) { av_packet_unref(packet); continue; }

                    while (ret >= 0) {
                        ret = avcodec_receive_frame(decoder_ctx, frame);
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
                        if (ret < 0) { av_packet_unref(packet); goto cleanup; }

                        // Reset pict_type to let encoder decide GOP structure and avoid warnings/slowdown.
                        frame->pict_type = AV_PICTURE_TYPE_NONE;

                        // Rescale PTS to encoder timebase.
                        int64_t src_ts = (frame->best_effort_timestamp != AV_NOPTS_VALUE)
                            ? frame->best_effort_timestamp : pts_fallback;
                        int64_t enc_pts = av_rescale_q(src_ts,
                            in_stream->time_base, encoder_ctx->time_base);

                        AVFrame* send_frame = nullptr;
                        if (sws_ctx) {
                            av_frame_make_writable(scaled_frame);
                            sws_scale(sws_ctx,
                                frame->data, frame->linesize, 0, decoder_ctx->height,
                                scaled_frame->data, scaled_frame->linesize);
                            scaled_frame->pts = enc_pts;
                            scaled_frame->pict_type = AV_PICTURE_TYPE_NONE;
                            send_frame = scaled_frame;
                        }
                        else {
                            frame->pts = enc_pts;
                            send_frame = frame;
                        }

                        pts_fallback = enc_pts + 1;

                        ret = avcodec_send_frame(encoder_ctx, send_frame);
                        if (ret < 0) { av_packet_unref(packet); goto cleanup; }

                        while (ret >= 0) {
                            AVPacket* out_pkt = av_packet_alloc();
                            ret = avcodec_receive_packet(encoder_ctx, out_pkt);
                            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                                av_packet_free(&out_pkt); break;
                            }
                            if (ret < 0) {
                                av_packet_free(&out_pkt);
                                av_packet_unref(packet);
                                goto cleanup;
                            }
                            av_packet_rescale_ts(out_pkt, encoder_ctx->time_base, out_stream->time_base);
                            out_pkt->stream_index = out_stream->index;
                            av_interleaved_write_frame(output_ctx, out_pkt);
                            av_packet_free(&out_pkt);
                        }
                    }
                }
                else if (si < (int)stream_map.size() && stream_map[si] >= 0) {
                    // Stream-copy for non-video tracks.
                    AVStream* in_s = input_ctx->streams[si];
                    AVStream* out_s = output_ctx->streams[stream_map[si]];
                    av_packet_rescale_ts(packet, in_s->time_base, out_s->time_base);
                    packet->stream_index = out_s->index;
                    av_interleaved_write_frame(output_ctx, packet);
                }

                av_packet_unref(packet);
            }

            // Flush encoder
            avcodec_send_frame(encoder_ctx, nullptr);
            while (true) {
                AVPacket* out_pkt = av_packet_alloc();
                ret = avcodec_receive_packet(encoder_ctx, out_pkt);
                if (ret == AVERROR_EOF || ret < 0) { av_packet_free(&out_pkt); break; }
                av_packet_rescale_ts(out_pkt, encoder_ctx->time_base, out_stream->time_base);
                out_pkt->stream_index = out_stream->index;
                av_interleaved_write_frame(output_ctx, out_pkt);
                av_packet_free(&out_pkt);
            }

        cleanup:
            av_write_trailer(output_ctx);
            av_frame_free(&scaled_frame);
            av_frame_free(&frame);
            av_packet_free(&packet);
            if (sws_ctx) sws_freeContext(sws_ctx);
            if (output_ctx && output_ctx->pb) avio_closep(&output_ctx->pb);
            avcodec_free_context(&encoder_ctx);
            avformat_free_context(output_ctx);
            avcodec_free_context(&decoder_ctx);
            avformat_close_input(&input_ctx);

            return ProcessResult::OK();
        }
        catch (const std::exception& e) {
            return ProcessResult::Error(std::format("Exception in compress: {}", e.what()));
        }
    }

} // namespace media_handler::compressor