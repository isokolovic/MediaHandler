#include "video_compressor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>
#include <libheif/heif.h>
#include <libexif/exif-data.h> 
#include <png.h>

void cleanup_video(VideoCompressionContext* ctx) {
	if (ctx->pkt) av_packet_free(&ctx->pkt);
	if (ctx->frame) av_frame_free(&ctx->frame);
	if (ctx->converted_frame) av_frame_free(&ctx->converted_frame);
	if (ctx->out_ctx && !((ctx->out_ctx)->oformat->flags & AVFMT_NOFILE)) avio_closep(&ctx->out_ctx->pb);
	if (ctx->dec_contexts && ctx->enc_contexts && ctx->sws_contexts) {
		for (unsigned int i = 0; i < ctx->nb_streams; i++) {
			if (ctx->dec_contexts[i]) avcodec_free_context(&ctx->dec_contexts[i]);
			if (ctx->enc_contexts[i]) avcodec_free_context(&ctx->enc_contexts[i]);
			if (ctx->sws_contexts[i]) sws_freeContext(ctx->sws_contexts[i]);
		}
	}
	if (ctx->last_progress) av_free(ctx->last_progress);
	if (ctx->sws_contexts) av_free(ctx->sws_contexts);
	if (ctx->enc_contexts) av_free(ctx->enc_contexts);
	if (ctx->dec_contexts) av_free(ctx->dec_contexts);
	if (ctx->stream_mapping) av_free(ctx->stream_mapping);
	if (ctx->out_ctx) avformat_free_context(ctx->out_ctx);
	if (ctx->in_ctx) avformat_close_input(&ctx->in_ctx);
}

bool write_packet(AVFormatContext* out_ctx, AVPacket* pkt, AVStream* in_stream, AVStream* out_stream, int stream_index) {
	pkt->pts = av_rescale_q_rnd(pkt->pts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
	pkt->dts = av_rescale_q_rnd(pkt->dts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
	pkt->duration = av_rescale_q(pkt->duration, in_stream->time_base, out_stream->time_base);
	pkt->pos = -1;
	pkt->stream_index = stream_index;

	if (av_interleaved_write_frame(out_ctx, pkt) < 0) {
		log_message(LOG_ERROR, "Error writing packet to %s", out_ctx->url);
		return false;
	}
	return true;
}

bool process_video_frame(VideoCompressionContext* ctx, const char* filename, int stream_index)
{
	//======Uncomment for detailed error logging (log frame parameters)======
	//log_message(LOG_INFO, "Frame: w=%d h=%d fmt=%d", ctx->frame->width, ctx->frame->height, ctx->frame->format);

	//Validate frame parameters
	if (ctx->frame->width <= 0 || ctx->frame->height <= 0 || ctx->frame->format == AV_PIX_FMT_NONE) {
		log_message(LOG_ERROR, "Invalid frame parameters for stream %d", stream_index);
		av_frame_unref(ctx->frame);
		return false;
	}

	// convert pixel format to NV12
	ctx->converted_frame->width = ctx->frame->width;
	ctx->converted_frame->height = ctx->frame->height;
	ctx->converted_frame->format = ctx->enc_contexts[stream_index]->pix_fmt;
	if (av_frame_get_buffer(ctx->converted_frame, 0) < 0) {
		log_message(LOG_ERROR, "Failed to allocate buffer for converted frame");
		av_frame_unref(ctx->frame);
		return false;
	}
	if (sws_scale(ctx->sws_contexts[stream_index], ctx->frame->data, ctx->frame->linesize, 0, ctx->frame->height,
		ctx->converted_frame->data, ctx->converted_frame->linesize) <= 0) {
		log_message(LOG_ERROR, "Failed to convert frame for stream %d", stream_index);
		av_frame_unref(ctx->converted_frame);
		av_frame_unref(ctx->frame);
		return false;
	}

	ctx->converted_frame->pts = ctx->frame->pts;

	AVPacket* out_pkt = av_packet_alloc();
	if (!out_pkt) {
		log_message(LOG_ERROR, "Failed to allocate output packet");
		av_frame_unref(ctx->frame);
		return false;
	}

	int ret; //Return val for sending/receiving frames

	// Send the converted frame to the encoder.
	// If sending fails, log the error and clean up resources
	ret = avcodec_send_frame(ctx->enc_contexts[stream_index], ctx->converted_frame);
	if (ret < 0) {
		char err_buf[128];
		av_strerror(ret, err_buf, sizeof(err_buf));
		log_message(LOG_ERROR, "Failed to send frame to encoder for stream %d: %s", stream_index, err_buf);

		av_packet_free(&out_pkt);
		av_frame_unref(ctx->converted_frame);
		av_frame_unref(ctx->frame);
		return false;
	}

	// Receive encoded packets from the encoder and write them to the output.
	// Handles EAGAIN (need more input), EOF (encoder flushed), and other errors.
	// Rescales timestamps and stream index before writing the packet.
	while (true) {
		ret = avcodec_receive_packet(ctx->enc_contexts[stream_index], out_pkt);
		if (ret == AVERROR(EAGAIN)) {
			break; // Need more frames
		}
		else if (ret == AVERROR_EOF) {
			log_message(LOG_WARNING, "Encoder for stream %d is flushed", stream_index);
			break; // No more packets
		}
		else if (ret < 0) {
			char err_buf[128];
			av_strerror(ret, err_buf, sizeof(err_buf));
			log_message(LOG_ERROR, "Failed to receive packet from encoder for stream %d: %s", stream_index, err_buf);
			av_packet_free(&out_pkt);
			av_frame_unref(ctx->converted_frame);
			av_frame_unref(ctx->frame);
			break;
		}

		AVStream* in_s = ctx->in_ctx->streams[stream_index];
		AVStream* out_s = ctx->out_ctx->streams[ctx->stream_mapping[stream_index]];
		out_pkt->pts = av_rescale_q_rnd(out_pkt->pts, in_s->time_base, out_s->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
		out_pkt->dts = av_rescale_q_rnd(out_pkt->dts, in_s->time_base, out_s->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
		out_pkt->duration = av_rescale_q(out_pkt->duration, in_s->time_base, out_s->time_base);
		out_pkt->pos = -1;
		out_pkt->stream_index = ctx->stream_mapping[stream_index];

		if (av_interleaved_write_frame(ctx->out_ctx, out_pkt) < 0) {
			log_message(LOG_ERROR, "Error writing encoded frame to %s", ctx->out_ctx->url);

			av_packet_free(&out_pkt);
			av_frame_unref(ctx->converted_frame);
			av_frame_unref(ctx->frame);
			break;
		}

		av_packet_unref(out_pkt);
	}

	av_packet_free(&out_pkt);
	av_frame_unref(ctx->converted_frame);
	av_frame_unref(ctx->frame);
	return true;
}

bool compress_video(const char* file, const char* output_file) {
    VideoCompressionContext ctx = { 0 };
    bool status = false;

    //======Uncomment for detailed error logging======
    //av_log_set_level(AV_LOG_ERROR);
    //av_log_set_callback(ffmpeg_log_callback);

    //Process input file
    // Open input file
    if (avformat_open_input(&ctx.in_ctx, file, NULL, NULL) < 0) {
        log_message(LOG_ERROR, "Failed to open video input %s", file);
        return false;
    }

    // Retrieve stream info
    if (avformat_find_stream_info(ctx.in_ctx, NULL) < 0) {
        log_message(LOG_ERROR, "Failed to find stream info for %s", file);
        cleanup_video(&ctx);
        return false;
    }

    // Allocate output context
    if (avformat_alloc_output_context2(&ctx.out_ctx, NULL, NULL, output_file) < 0) {
        log_message(LOG_ERROR, "Failed to allocate output context for %s", output_file);
        cleanup_video(&ctx);
        return false;
    }

    // Allocate stream mapping and codec context
    ctx.nb_streams = ctx.in_ctx->nb_streams;
    ctx.stream_mapping = (int*)av_calloc(ctx.nb_streams, sizeof(int));
    ctx.dec_contexts = (AVCodecContext**)av_calloc(ctx.nb_streams, sizeof(AVCodecContext*));
    ctx.enc_contexts = (AVCodecContext**)av_calloc(ctx.nb_streams, sizeof(AVCodecContext*));
    ctx.sws_contexts = (struct SwsContext**)av_calloc(ctx.nb_streams, sizeof(struct SwsContext*));
    if (!ctx.stream_mapping || !ctx.dec_contexts || !ctx.enc_contexts || !ctx.sws_contexts) {
        log_message(LOG_ERROR, "Failed to allocate stream mapping or codec contexts for %s", file);
        cleanup_video(&ctx);
        return false;
    }

    //Process each input file stream
    //Video stream is compressed
    //Audio is only copied (processing speed/file size optimization)
    int out_stream_idx = 0;
    for (unsigned int i = 0; i < ctx.nb_streams; i++) {
        AVStream* in_stream = ctx.in_ctx->streams[i];
        AVStream* out_stream = avformat_new_stream(ctx.out_ctx, NULL);
        if (!out_stream) {
            log_message(LOG_ERROR, "Failed to create output stream for %s", file);
            cleanup_video(&ctx);
            return false;
        }

        //Map the stream to next output index
        ctx.stream_mapping[i] = out_stream_idx++;

        //Compress only video
        if (in_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            //Decoder
            const AVCodec* dec_codec = avcodec_find_decoder(in_stream->codecpar->codec_id);
            if (!dec_codec) {
                log_message(LOG_ERROR, "No decoder found for stream %d in %s", i, file);
                ctx.stream_mapping[i] = -1;
                continue;
            }
            ctx.dec_contexts[i] = avcodec_alloc_context3(dec_codec);
            if (!ctx.dec_contexts[i]) {
                log_message(LOG_ERROR, "Failed to allocate decoder context for video stream %d", i);
                ctx.stream_mapping[i] = -1;
                continue;
            }
            if (avcodec_parameters_to_context(ctx.dec_contexts[i], in_stream->codecpar) < 0) {
                log_message(LOG_ERROR, "Failed to copy parameters to decoder context for video stream %d", i);
                ctx.stream_mapping[i] = -1;
                avcodec_free_context(&ctx.dec_contexts[i]);
                continue;
            }
            ctx.dec_contexts[i]->time_base = in_stream->time_base;
            int ret_dec = avcodec_open2(ctx.dec_contexts[i], dec_codec, NULL);
            if (ret_dec < 0) {
                char err_buf[128];
                av_strerror(ret_dec, err_buf, sizeof(err_buf));
                log_message(LOG_ERROR, "Failed to open decoder for video stream %d: %s", i, err_buf);
                ctx.stream_mapping[i] = -1;
                avcodec_free_context(&ctx.dec_contexts[i]);
                continue;
            }

            //Encoder - FORCE libx264 (SKIP h264_mf COMPLETELY)
            const AVCodec* enc_codec = avcodec_find_encoder_by_name("libx264");
            if (!enc_codec) {
                log_message(LOG_ERROR, "libx264 encoder not found. Is FFmpeg built with --enable-libx264?");
                ctx.stream_mapping[i] = -1;
                avcodec_free_context(&ctx.dec_contexts[i]);
                continue;
            }

            //======Uncomment for detailed error logging (Log encoder parameters for debugging)======
            //log_message(LOG_INFO, "Using libx264 for stream %d", i);

            ctx.enc_contexts[i] = avcodec_alloc_context3(enc_codec);
            if (!ctx.enc_contexts[i]) {
                log_message(LOG_ERROR, "Failed to allocate libx264 encoder context for video stream %d", i);
                ctx.stream_mapping[i] = -1;
                avcodec_free_context(&ctx.dec_contexts[i]);
                continue;
            }

            // Configure encoder using decoder values
            ctx.enc_contexts[i]->height = ctx.dec_contexts[i]->height;
            ctx.enc_contexts[i]->width = ctx.dec_contexts[i]->width;
            ctx.enc_contexts[i]->sample_aspect_ratio = ctx.dec_contexts[i]->sample_aspect_ratio;
            ctx.enc_contexts[i]->pix_fmt = AV_PIX_FMT_YUV420P; // libx264 standard

            // Set time base based on guessed frame rate
            AVRational frame_rate = av_guess_frame_rate(ctx.in_ctx, ctx.in_ctx->streams[i], NULL);
            if (frame_rate.num <= 0 || frame_rate.den <= 0) {
                log_message(LOG_WARNING, "Invalid frame rate for stream %d, using fallback 25/1", i);
                frame_rate = (AVRational){ 25, 1 }; // Fallback to 25 fps
            }
            ctx.enc_contexts[i]->time_base = av_inv_q(frame_rate);
            if (ctx.enc_contexts[i]->time_base.num <= 0 || ctx.enc_contexts[i]->time_base.den <= 0) {
                log_message(LOG_WARNING, "Invalid time_base for stream %d, using fallback 1/25", i);
                ctx.enc_contexts[i]->time_base = (AVRational){ 1, 25 }; // Fallback
            }

            // Set nominal frame rate and bitrate (2 Mbps default)
            ctx.enc_contexts[i]->framerate = frame_rate;
            ctx.enc_contexts[i]->bit_rate = ctx.dec_contexts[i]->bit_rate ? ctx.dec_contexts[i]->bit_rate / 2 : 2000000;

            // Set encoder to use global headers if required by container format
            if (ctx.out_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
                ctx.enc_contexts[i]->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            }

            // libx264 quality settings
            av_opt_set(ctx.enc_contexts[i]->priv_data, "preset", "medium", 0);
            av_opt_set(ctx.enc_contexts[i]->priv_data, "crf", "23", 0);

            // Initialize sws_context for pixel format conversion
            ctx.sws_contexts[i] = sws_getContext(
                ctx.dec_contexts[i]->width, ctx.dec_contexts[i]->height, ctx.dec_contexts[i]->pix_fmt,
                ctx.enc_contexts[i]->width, ctx.enc_contexts[i]->height, ctx.enc_contexts[i]->pix_fmt,
                SWS_BILINEAR, NULL, NULL, NULL);
            if (!ctx.sws_contexts[i]) {
                log_message(LOG_ERROR, "Failed to initialize sws_context for video stream %d", i);
                ctx.stream_mapping[i] = -1;
                avcodec_free_context(&ctx.dec_contexts[i]);
                avcodec_free_context(&ctx.enc_contexts[i]);
                continue;
            }

            //======Uncomment for detailed error logging (Log encoder parameters for debugging)======
            //log_message(LOG_INFO, "Encoder stream %d: w=%d h=%d pix_fmt=%d time_base=%d/%d framerate=%d/%d",
            // i, ctx.enc_contexts[i]->width, ctx.enc_contexts[i]->height, ctx.enc_contexts[i]->pix_fmt,
            // ctx.enc_contexts[i]->time_base.num, ctx.enc_contexts[i]->time_base.den,
            // ctx.enc_contexts[i]->framerate.num, ctx.enc_contexts[i]->framerate.den);

            int ret_enc = avcodec_open2(ctx.enc_contexts[i], enc_codec, NULL);
            if (ret_enc < 0) {
                char err_buf[128];
                av_strerror(ret_enc, err_buf, sizeof(err_buf));
                log_message(LOG_ERROR, "Failed to open libx264 encoder for video stream %d: %s", i, err_buf);
                ctx.stream_mapping[i] = -1;
                avcodec_free_context(&ctx.dec_contexts[i]);
                avcodec_free_context(&ctx.enc_contexts[i]);
                sws_freeContext(ctx.sws_contexts[i]);
                continue;
            }

            if (avcodec_parameters_from_context(out_stream->codecpar, ctx.enc_contexts[i]) < 0) {
                log_message(LOG_ERROR, "Failed to copy encoder parameters to output stream %d", i);
                ctx.stream_mapping[i] = -1;
                avcodec_free_context(&ctx.dec_contexts[i]);
                avcodec_free_context(&ctx.enc_contexts[i]);
                sws_freeContext(ctx.sws_contexts[i]);
                continue;
            }
            out_stream->time_base = ctx.enc_contexts[i]->time_base;
        }
        //Copy audio and other streams
        else {
            if (avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar) < 0) {
                log_message(LOG_ERROR, "Failed to copy stream parameters for stream %d", i);
                ctx.stream_mapping[i] = -1;
                continue;
            }
            out_stream->time_base = in_stream->time_base;
        }
    }

    //Process output file
    //Open output file
    //Ceck agains AVFMT_NOFILE bitmask if format needs a file (not in-memory format)
    if (!(ctx.out_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&ctx.out_ctx->pb, output_file, AVIO_FLAG_WRITE) < 0) {
            log_message(LOG_ERROR, "Failed to open output file %s", output_file);
            cleanup_video(&ctx);
            return false;
        }
    }

    // Write header
    AVDictionary* opts = NULL;
    av_dict_set(&opts, "movflags", "+faststart", 0);
    if (avformat_write_header(ctx.out_ctx, &opts) < 0) {
        log_message(LOG_ERROR, "Failed to write header for %s", output_file);
        av_dict_free(&opts);
        cleanup_video(&ctx);
        return false;
    }
    av_dict_free(&opts);

    // Allocate packet and frame
    //Allocate converted_frame (holds converted pixel data for encoding)
    ctx.pkt = av_packet_alloc();
    ctx.frame = av_frame_alloc();
    ctx.converted_frame = av_frame_alloc();
    if (!ctx.pkt || !ctx.frame || !ctx.converted_frame) {
        log_message(LOG_ERROR, "Failed to allocate packet or frames for %s", output_file);
        cleanup_video(&ctx);
        return false;
    }

    // Process packets
    while (av_read_frame(ctx.in_ctx, ctx.pkt) >= 0) {
        //Unref packet if stream is not to be processed
        if (ctx.stream_mapping[ctx.pkt->stream_index] < 0) {
            av_packet_unref(ctx.pkt);
            continue;
        }
        AVStream* in_stream = ctx.in_ctx->streams[ctx.pkt->stream_index];
        AVStream* out_stream = ctx.out_ctx->streams[ctx.stream_mapping[ctx.pkt->stream_index]];

        //Process only video
        if (in_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            if (!ctx.dec_contexts[ctx.pkt->stream_index] || !ctx.enc_contexts[ctx.pkt->stream_index] || !ctx.sws_contexts[ctx.pkt->stream_index]) {
                log_message(LOG_ERROR, "No codec context available for video stream %d", ctx.pkt->stream_index);
                av_packet_unref(ctx.pkt);
                continue;
            }

            //Send packed to decoder
            //logs for specific decoder error
            int ret = avcodec_send_packet(ctx.dec_contexts[ctx.pkt->stream_index], ctx.pkt);
            if (ret < 0) {
                char err_buf[128];
                av_strerror(ret, err_buf, sizeof(err_buf));
                log_message(LOG_ERROR, "Failed to send packet to decoder for stream %d: %s", ctx.pkt->stream_index, err_buf);
                av_packet_unref(ctx.pkt);
                continue;
            }

            //Read decoded frames
            while (avcodec_receive_frame(ctx.dec_contexts[ctx.pkt->stream_index], ctx.frame) >= 0) {
                if (!process_video_frame(&ctx, file, ctx.pkt->stream_index)) {
                    av_frame_unref(ctx.frame);
                    continue;
                }
                av_frame_unref(ctx.frame);
            }
        }
        else {
            //Rescale audio timestamps
            if (!write_packet(ctx.out_ctx, ctx.pkt, in_stream, out_stream, ctx.stream_mapping[ctx.pkt->stream_index])) {
                cleanup_video(&ctx);
                return false;
            }
        }
        av_packet_unref(ctx.pkt);
    }

    // Flush decoders and encoders
    for (unsigned int i = 0; i < ctx.nb_streams; i++) {
        if (ctx.stream_mapping[i] < 0 || !ctx.dec_contexts[i] || !ctx.enc_contexts[i]) continue;

        // Flush decoder
        if (avcodec_send_packet(ctx.dec_contexts[i], NULL) < 0) {
            log_message(LOG_ERROR, "Failed to flush decoder for stream %d", i);
            continue;
        }
        while (avcodec_receive_frame(ctx.dec_contexts[i], ctx.frame) >= 0) {
            //======Uncomment for detailed error logging (log frame parameters)======
            //log_message(LOG_INFO, "Flushed Frame: w=%d h=%d fmt=%d", ctx.frame->width, ctx.frame->height, ctx.frame->format);

            if (!process_video_frame(&ctx, file, i)) {
                av_frame_unref(ctx.frame);
                continue;
            }
            av_frame_unref(ctx.frame);
        }

        // Flush encoder
        if (avcodec_send_frame(ctx.enc_contexts[i], NULL) < 0) {
            log_message(LOG_ERROR, "Failed to flush encoder for stream %d", i);
            continue;
        }
        while (avcodec_receive_packet(ctx.enc_contexts[i], ctx.pkt) >= 0) {
            AVStream* in_s = ctx.in_ctx->streams[i];
            AVStream* out_s = ctx.out_ctx->streams[ctx.stream_mapping[i]];
            ctx.pkt->pts = av_rescale_q_rnd(ctx.pkt->pts, in_s->time_base, out_s->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
            ctx.pkt->dts = av_rescale_q_rnd(ctx.pkt->dts, in_s->time_base, out_s->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
            ctx.pkt->duration = av_rescale_q(ctx.pkt->duration, in_s->time_base, out_s->time_base);
            ctx.pkt->pos = -1;
            ctx.pkt->stream_index = ctx.stream_mapping[i];
            if (av_interleaved_write_frame(ctx.out_ctx, ctx.pkt) < 0) {
                log_message(LOG_ERROR, "Error writing flushed packet to %s", output_file);
                av_packet_unref(ctx.pkt);
                continue;
            }
            av_packet_unref(ctx.pkt);
        }
    }

    // Write final metadata chunk
    if (av_write_trailer(ctx.out_ctx) < 0) {
        log_message(LOG_ERROR, "Failed to write trailer for %s", output_file);
        cleanup_video(&ctx);
        return false;
    }

    status = true;
    log_message(LOG_INFO, "Successfully compressed video %s", file);

    // Cleanup
    cleanup_video(&ctx);
    return status;
}