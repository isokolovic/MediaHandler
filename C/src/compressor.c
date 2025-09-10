#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>
#include <libheif/heif.h>
#include <libexif/exif-data.h> 
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <png.h>
#include "compressor.h"
#include "logger.h"
#include "file_handler.h"

/// @brief Main function for file compression
/// @param file File to be compressed
/// @param output_file Compressed file
/// @return True if successful
bool compress_file(const char* file, const char* output_file) {
	if (!file || !output_file) {
		log_message(LOG_ERROR, "Null input or output path in compress_file"); 
		return false;
	}
	if (is_image(file)) {
		return compress_image(file, output_file);
	}
	if (is_video(file)) {
		return compress_video(file, output_file); 
	}	
}

/// @brief Compress image file
/// @param file Input image
/// @param output_file Compressed image
/// @return True if compression successful
bool compress_image(const char* file, const char* output_file)
{	
	FILE* in_file = NULL;
	FILE* out_file = NULL;

	const char* extension = strrchr(file, '.');
	if (!extension) {
		log_message(LOG_ERROR, "No extension found for %s", file); 
		return false;
	}

	//Libheif compression used for .heic files
	if (strcmp(extension, ".heic") == 0) {
		struct heif_context* context = NULL;
		struct heif_image_handle* handle = NULL;

		struct heif_image* image = NULL;
		struct heif_encoder* encoder = NULL;

		const char* const* dirs = heif_get_plugin_directories();
		if (dirs) {
			for (int i = 0; dirs[i]; ++i) {
				if (dirs[i][0] != '\0') // Only print non-empty directories
					printf("libheif plugin dir: %s\n", dirs[i]);
			}
			heif_free_plugin_directories(dirs);
		}


		context = heif_context_alloc();
		if (!context) {
			log_message(LOG_ERROR, "Failed to allocate heif conext for %s", file); 
			return false;
		}

		//Read the file
		struct heif_error err = heif_context_read_from_file(context, file, NULL); 
		if (err.code != heif_error_Ok) {
			heif_context_free(context);
			log_message(LOG_ERROR, "Failed to read .heic file %s: %s", file, err.message);
			return false;
		}

		//Get image handle (metadata and decoding functions)
		err = heif_context_get_primary_image_handle(context, &handle);
		if (err.code != heif_error_Ok) {
			heif_context_free(context);
			log_message(LOG_ERROR, "Failed to get image handle for %s: %s", file, err.message);
			return false;
		}

		//Decode a photo into usable format 
		err = heif_decode_image(handle, &image, heif_colorspace_RGB, heif_chroma_interleaved_RGB, NULL);
		if (err.code != heif_error_Ok) {
			heif_context_free(context);
			log_message(LOG_ERROR, "Failed to decode .heic photo: %s: %s", file, err.message);
			return false;
		}

		//Get encoder (compression engine)
		err = heif_context_get_encoder_for_format(context, heif_compression_HEVC, &encoder);
		if (err.code != heif_error_Ok) {
			heif_context_free(context);
			log_message(LOG_ERROR, "Failed to get .heic encoder for %s: %s", file, err.message);
			return false;
		}

		//Set compression quality
		heif_encoder_set_lossy_quality(encoder, PHOTO_QUALITY);

		//Compress the image
		err = heif_context_encode_image(context, image, encoder, NULL, NULL);
		if (err.code != heif_error_Ok) {			
			heif_context_free(context);
			heif_image_release(image);
			heif_encoder_release(encoder);
			heif_image_handle_release(handle);
			log_message(LOG_ERROR, "Failed to encode .hec file: %s: %s", file, err.message);
			return false;
		}

		//Write to file and free created objects
		err = heif_context_write_to_file(context, output_file);		
		heif_context_free(context);
		heif_image_release(image);
		heif_encoder_release(encoder);
		heif_image_handle_release(handle);
		if (err.code != heif_error_Ok) {
			log_message(LOG_ERROR, "Failed to save compressed .heic file: %s: %s", output_file, err.message);
			return false;
		}

		return true;
	}
	//Libjpeg-turbo compression used for .jpg/.jpeg files
	else if ((strcasecmp(extension, ".jpg") == 0) || strcasecmp(extension, ".jpeg") == 0){
		struct jpeg_decompress_struct cinfo;
		struct jpeg_compress_struct cinfo_out; 
		struct jpeg_error_mgr jerr;

		//Open file for reading
		in_file = fopen(file, "rb"); 
		if (!in_file) {
			log_message(LOG_ERROR, "Failed to open %s for writing", file);
			return false;
		}

		//Open output file for writing
		out_file = fopen(output_file, "wb"); 
		if (!out_file) {
			fclose(file); 
			log_message(LOG_ERROR, "Failed to open %s for writing", file); 
			return false;
		}

		// Initialize .jpeg decompression and read image header from input file
		cinfo.err = jpeg_std_error(&jerr); 
		jpeg_create_decompress(&cinfo); 
		jpeg_stdio_src(&cinfo, in_file);
		jpeg_read_header(&cinfo, TRUE); 

		// Set up .jpeg compression and link output file as destination
		cinfo_out.err = jpeg_std_error(&jerr); 
		jpeg_create_compress(&cinfo_out); 
		jpeg_stdio_dest(&cinfo_out, out_file); 

		//Check if original photo smaller than trim dimensions
		cinfo_out.image_width = cinfo.image_width > PHOTO_TRIM_WIDTH ? PHOTO_TRIM_WIDTH : cinfo.image_width; 
		cinfo_out.image_height = cinfo.image_height > PHOTO_TRIM_HEIGHT ? PHOTO_TRIM_HEIGHT : cinfo.image_height;
		cinfo_out.input_components = cinfo.jpeg_color_space == JCS_GRAYSCALE ? 1 : 3; //If not grayscale, RGB
		cinfo_out.in_color_space = cinfo.jpeg_color_space == JCS_GRAYSCALE ? JCS_GRAYSCALE : JCS_RGB;
		
		jpeg_set_defaults(&cinfo_out); 
		jpeg_set_quality(&cinfo_out, PHOTO_QUALITY, TRUE); 
		jpeg_start_compress(&cinfo_out, TRUE); 

		// Allocate a one-row scanline buffer to hold decompressed pixel data.
		// Each row contains (image_width × num_components) bytes.
		// This buffer will be reused for each scanline during JPEG decompression.
		JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)(
			(j_common_ptr)&cinfo,
			JPOOL_IMAGE,
			cinfo.image_width * cinfo.num_components,
			1);

		// Loop through each scanline of the input JPEG image.
		// Read one scanline from the decompressor (cinfo) into buffer.
		// Then write that scanline to the output compressor (cinfo_out), ensuring we don't exceed the image height.
		while (cinfo.output_scanline < cinfo.image_height) {
			jpeg_read_scanlines(&cinfo, buffer, 1);
			
			if (cinfo.output_scanline <= cinfo.image_height) {
				jpeg_write_scanlines(&cinfo_out, buffer, 1);
			}
		}

		//Clean up
		jpeg_finish_compress(&cinfo_out);
		jpeg_destroy_compress(&cinfo_out); 
		jpeg_destroy_compress(&cinfo);
		fclose(in_file);
		fclose(out_file); 
		return true;
	}
	//Libpng compression used for .png files
	else if((strcasecmp(extension, ".png") == 0)){
		png_structp png_read_ptr = NULL;
		png_infop read_info_ptr = NULL;
		png_structp png_write_ptr = NULL;
		png_infop write_info_ptr = NULL;
		unsigned char* image_data = NULL; 

		//Open input file
		in_file = fopen(file, "rb"); 
		if (!in_file) {
			log_message(LOG_ERROR, "Failed to open input file %s: %s", file, strerror(errno)); 
			return false;
		}
		
		//Initialize libpng for reading
		png_read_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL); 
		if (!png_read_ptr) {
			fclose(in_file); 
			log_message(LOG_ERROR, "Failed to create read structure for file %s", file); 
			return false;
		}

		read_info_ptr = png_create_info_struct(png_read_ptr);
		if (!read_info_ptr) {
			png_destroy_read_struct(&png_read_ptr, NULL, NULL); 
			fclose(in_file); 
			log_message(LOG_ERROR, "Failed to create png read info struct for file %s", file); 
			return false; 
		}

		//Handle png read errors using libpng setjmp mechanism to safely clean up and exit.
		//If libpng error occurs during reading, control jumps via setjmp and cleanup resources. 
		if (setjmp(png_jmpbuf(png_read_ptr))) {
			png_destroy_read_struct(&png_read_ptr, &read_info_ptr, NULL); 
			if (image_data) free(image_data);
			fclose(in_file);

			log_message(LOG_ERROR, "Error reading png file %s", file); 
			return false;
		}

		//Initialize png input
		png_init_io(png_read_ptr, in_file); 
		png_read_info(png_read_ptr, read_info_ptr); 

		//Get image info
		png_uint_32 width = png_get_image_width(png_read_ptr, read_info_ptr); 
		png_uint_32 height = png_get_image_height(png_read_ptr, read_info_ptr); 
		int color_type = png_get_color_type(png_read_ptr, read_info_ptr); 
		int bit_depth = png_get_bit_depth(png_read_ptr, read_info_ptr); 

		//Convert to RGB if necessary
		if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png_read_ptr); // Converts indexed colors to standard RGB for easier processing
		if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png_read_ptr); // Ensures grayscale images use full 8-bit range for better quality
		if (png_get_valid(png_read_ptr, read_info_ptr, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png_read_ptr);// Adds transparency info as an alpha channel for proper rendering
		if (bit_depth == 16) png_set_strip_16(png_read_ptr); // Reduces 16-bit images to 8-bit to simplify handling and save memory

		// Update info after transformations
		png_read_update_info(png_read_ptr, read_info_ptr);
		color_type = png_get_color_type(png_read_ptr, read_info_ptr); 
		bit_depth = png_get_bit_depth(png_read_ptr, read_info_ptr);

		//Allocate image data
		image_data = malloc(height * png_get_rowbytes(png_read_ptr, read_info_ptr));
		if (!image_data) {
			png_destroy_read_struct(&png_read_ptr, &read_info_ptr, NULL); 
			fclose(in_file); 
			log_message(LOG_ERROR, "Failed to allocate memory for png data in %s", file); 
			return false;
		}

		png_bytep* row_pointers = malloc(height * sizeof(png_bytep)); 
		if (!row_pointers) {
			free(image_data); 
			png_destroy_read_struct(&png_read_ptr, &read_info_ptr, NULL); 
			fclose(in_file); 
			log_message(LOG_ERROR, "Failed to allocate row pointer for %s", file); 
			return false; 
		}

		// Initialize row pointers for each scanline in the image
		for (png_uint_32 y = 0; y < height; y++) {
			//Each pointer tells libpng where to store pixel data for that row
			//New row offset = start of image pointer (image_data) + row index * nomber of bytes per row
			row_pointers[y] = image_data + y * png_get_rowbytes(png_read_ptr, read_info_ptr); 
		}

		//Read png image and clean up png reading
		png_read_image(png_read_ptr, row_pointers); 
		
		png_destroy_read_struct(&png_read_ptr, &read_info_ptr, NULL); 
		fclose(in_file); 

		//Resize output file dimensions
		png_uint_32 out_width = width > PHOTO_TRIM_WIDTH ? PHOTO_TRIM_WIDTH : width; 
		png_uint_32 out_height = height > PHOTO_TRIM_HEIGHT ? PHOTO_TRIM_HEIGHT : height; 

		//Initialize libpng for writing
		out_file = fopen(output_file, "wb"); 
		if (!out_file) {
			free(image_data); 
			free(row_pointers); 
			log_message(LOG_ERROR, "Failed to open output file %s: %s", file, strerror(errno)); 
			return false;
		}
		
		//Create write and info structs
		png_write_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		if (!png_write_ptr) {
			free(image_data);
			free(row_pointers);
			fclose(out_file);
			log_message(LOG_ERROR, "Failed to create png write struct for %s", output_file);
			return false;
		}

		write_info_ptr = png_create_info_struct(png_write_ptr);
		if (!write_info_ptr) {
			png_destroy_write_struct(&png_write_ptr, NULL);
			free(image_data);
			free(row_pointers);
			fclose(out_file);
			log_message(LOG_ERROR, "Failed to create png write info struct for %s", output_file);
			return false;
		}

		// Set up error handling for writing
		if (setjmp(png_jmpbuf(png_write_ptr))) {
			png_destroy_write_struct(&png_write_ptr, &write_info_ptr);
			free(image_data);
			free(row_pointers);
			fclose(out_file);
			log_message(LOG_ERROR, "Error writing png file %s", output_file);
			return false;
		}

		//Initialize png output file properties
		png_init_io(png_write_ptr, out_file);
		png_set_IHDR(png_write_ptr, write_info_ptr, out_width, out_height, bit_depth,
			color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
			PNG_FILTER_TYPE_DEFAULT);
		png_set_compression_level(png_write_ptr, 9); // Maximum compression
		png_write_info(png_write_ptr, write_info_ptr);

		//Write scaled row to output file
		for (png_uint_32 y = 0; y < out_height; y++) {
			//Map output row to corresponding source row
			png_uint_32 src_y = (png_uint_32)((double)y * height / out_height);
			png_write_row(png_write_ptr, row_pointers[src_y]);
		}

		//Clean up
		png_write_end(png_write_ptr, write_info_ptr);
		png_destroy_write_struct(&png_write_ptr, &write_info_ptr);
		free(image_data);
		free(row_pointers);
		fclose(out_file);
		return true;
	}
	else {
		log_message(LOG_WARNING, "%s extension not supported for compression. Copying raw file: %s", extension, file);
		if (!copy_file(file, output_file)) {
			log_message(LOG_ERROR, "Failed to copy %s to %s", file, output_file);
			return false;
		}
		return true;
	}
}

/// @brief Compress video file
/// @param file Input video
/// @param output_file Compressed video
/// @return True if compression successful
bool compress_video(const char* file, const char* output_file)
{
	AVFormatContext* in_ctx = NULL;
	AVFormatContext* out_ctx = NULL;
	AVPacket* pkt = NULL;
	AVFrame* frame = NULL;
	AVFrame* converted_frame = NULL;
	int* stream_mapping = NULL;
	AVCodecContext** dec_contexts = NULL;
	AVCodecContext** enc_contexts = NULL;
	struct SwsContext** sws_contexts = NULL;
	int stream_index = 0;
	bool status = false;

	//temp - detailed error logging: 
	av_log_set_level(AV_LOG_ERROR);
	av_log_set_callback(ffmpeg_log_callback);


	//Process input file

	// Open input file
	if (avformat_open_input(&in_ctx, file, NULL, NULL) < 0) {
		log_message(LOG_ERROR, "Failed to open video input %s", file);
		return false;
	}

	// Retrieve stream info
	if (avformat_find_stream_info(in_ctx, NULL) < 0) {
		log_message(LOG_ERROR, "Failed to find stream info for %s", file);
		avformat_close_input(&in_ctx);
		return false;
	}

	// Allocate output context
	if (avformat_alloc_output_context2(&out_ctx, NULL, NULL, output_file) < 0) {
		log_message(LOG_ERROR, "Failed to allocate output context for %s", output_file);
		avformat_close_input(&in_ctx);
		return false;
	}

	// Allocate stream mapping and codec context
	stream_mapping = (int*)av_calloc(in_ctx->nb_streams, sizeof(int));
	dec_contexts = (AVCodecContext**)av_calloc(in_ctx->nb_streams, sizeof(AVCodecContext*));
	enc_contexts = (AVCodecContext**)av_calloc(in_ctx->nb_streams, sizeof(AVCodecContext*));
	sws_contexts = (struct SwsContext**)av_calloc(in_ctx->nb_streams, sizeof(struct SwsContext*));

	if (!stream_mapping || !dec_contexts || !enc_contexts || !sws_contexts) {
		log_message(LOG_ERROR, "Failed to allocate stream mapping or codec contexts for %s", file);

		av_free(sws_contexts);
		av_free(enc_contexts);
		av_free(dec_contexts);
		av_free(stream_mapping);
		avformat_free_context(out_ctx);
		avformat_close_input(&in_ctx);
		return false;
	}

	//Process each input file stream
	//Video stream is compressed, audio is only copied (processing speed/file size optimization)
	for (unsigned int i = 0; i < in_ctx->nb_streams; i++) {
		AVStream* in_stream = in_ctx->streams[i];
		AVStream* out_stream = avformat_new_stream(out_ctx, NULL);

		if (!out_stream) {
			log_message(LOG_ERROR, "Failed to create output stream for %s", file);

			for (unsigned int j = 0; j < i; j++) {
				if (dec_contexts[j]) avcodec_free_context(&dec_contexts[j]);
				if (enc_contexts[j]) avcodec_free_context(&enc_contexts[j]);
				if (sws_contexts[j]) sws_freeContext(sws_contexts[j]);
			}

			av_free(sws_contexts);
			av_free(enc_contexts);
			av_free(dec_contexts);
			av_free(stream_mapping);
			avformat_free_context(out_ctx);
			avformat_close_input(&in_ctx);
			return false;
		}

		//Map the stream to next output index
		stream_mapping[i] = stream_index++;
		
		//Compress only video
		if (in_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {

			//Decoder

			const AVCodec* dec_codec = avcodec_find_decoder(in_stream->codecpar->codec_id);
			if (!dec_codec) {
				log_message(LOG_ERROR, "No decoder found for stream %d in %s", i, file);
				stream_mapping[i] = -1;
				continue;
			}

			dec_contexts[i] = avcodec_alloc_context3(dec_codec);
			if (!dec_contexts[i]) {
				log_message(LOG_ERROR, "Failed to allocate decoder context for video stream %d", i);
				stream_mapping[i] = -1;
				continue;
			}
			if (avcodec_parameters_to_context(dec_contexts[i], in_stream->codecpar) < 0) {
				log_message(LOG_ERROR, "Failed to copy parameters to decoder context for video stream %d", i);
				stream_mapping[i] = -1;
				avcodec_free_context(&dec_contexts[i]);
				continue;
			}
			if (avcodec_open2(dec_contexts[i], dec_codec, NULL) < 0) {
				log_message(LOG_ERROR, "Failed to open decoder for video stream %d", i);
				stream_mapping[i] = -1;
				avcodec_free_context(&dec_contexts[i]);
				continue;
			}

			//Encoder

			const AVCodec* enc_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
			if (!enc_codec) {
				log_message(LOG_ERROR, "No H.264 encoder found for video stream %d", i);
				stream_mapping[i] = -1;
				avcodec_free_context(&dec_contexts[i]);
				continue;
			}

			enc_contexts[i] = avcodec_alloc_context3(enc_codec);
			if (!enc_contexts[i]) {
				log_message(LOG_ERROR, "Failed to allocate H.264 encoder context for video stream %d", i);
				stream_mapping[i] = -1;
				avcodec_free_context(&dec_contexts[i]);
				continue;
			}

			// Configure encoder using decoder values and enforce h264_mf requirements
			enc_contexts[i]->height = dec_contexts[i]->height;
			enc_contexts[i]->width = dec_contexts[i]->width;
			enc_contexts[i]->sample_aspect_ratio = dec_contexts[i]->sample_aspect_ratio;
			enc_contexts[i]->pix_fmt = AV_PIX_FMT_NV12;// h264_mf requires NV12 pixel format

			// Set time base based on guessed frame rate (required by h264_mf)
			AVRational frame_rate = av_guess_frame_rate(in_ctx, in_ctx->streams[i], NULL);
			if (frame_rate.num <= 0 || frame_rate.den <= 0) {
				log_message(LOG_WARNING, "Invalid frame rate for stream %d, using fallback 25/1", i);
				frame_rate = (AVRational){ 25, 1 }; // Fallback to 25 fps
			}

			enc_contexts[i]->time_base = av_inv_q(frame_rate);
			if (enc_contexts[i]->time_base.num <= 0 || enc_contexts[i]->time_base.den <= 0) {
				log_message(LOG_WARNING, "Invalid time_base for stream %d, using fallback 1/25", i);
				enc_contexts[i]->time_base = (AVRational){ 1, 25 }; // Fallback
			}

			// Set nominal frame rate and bitrate (2 Mbps default)
			enc_contexts[i]->framerate = frame_rate;
			enc_contexts[i]->bit_rate = dec_contexts[i]->bit_rate ? dec_contexts[i]->bit_rate / 2 : 2000000;

			// Set encoder to use global headers if required by container format
			if (out_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
				enc_contexts[i]->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
			}

			// Initialize sws_context for pixel format conversion
			sws_contexts[i] = sws_getContext(
				dec_contexts[i]->width, dec_contexts[i]->height, dec_contexts[i]->pix_fmt,
				enc_contexts[i]->width, enc_contexts[i]->height, enc_contexts[i]->pix_fmt,
				SWS_BILINEAR, NULL, NULL, NULL);
			if (!sws_contexts[i]) {
				log_message(LOG_ERROR, "Failed to initialize sws_context for video stream %d", i);
				stream_mapping[i] = -1; //Mark stream as invalid
				avcodec_free_context(&dec_contexts[i]);
				avcodec_free_context(&enc_contexts[i]);
				continue;
			}

			//temp - Log encoder parameters for debugging
			log_message(LOG_INFO, "Encoder stream %d: w=%d h=%d pix_fmt=%d time_base=%d/%d framerate=%d/%d",
				i, enc_contexts[i]->width, enc_contexts[i]->height, enc_contexts[i]->pix_fmt,
				enc_contexts[i]->time_base.num, enc_contexts[i]->time_base.den,
				enc_contexts[i]->framerate.num, enc_contexts[i]->framerate.den);

			if (avcodec_open2(enc_contexts[i], enc_codec, NULL) < 0) {
				char err_buf[128];
				av_strerror(avcodec_open2(enc_contexts[i], enc_codec, NULL), err_buf, sizeof(err_buf));				
				log_message(LOG_ERROR, "Failed to open H.264 encoder for video stream %d: %s", i, err_buf);
				
				stream_mapping[i] = -1; //Mark stream as invalid
				avcodec_free_context(&dec_contexts[i]);
				avcodec_free_context(&enc_contexts[i]);
				sws_freeContext(sws_contexts[i]);
				continue;
			}

			if (avcodec_parameters_from_context(out_stream->codecpar, enc_contexts[i]) < 0) {
				log_message(LOG_ERROR, "Failed to copy encoder parameters to output stream %d", i);
				stream_mapping[i] = -1;
				avcodec_free_context(&dec_contexts[i]);
				avcodec_free_context(&enc_contexts[i]);
				sws_freeContext(sws_contexts[i]);
				continue;
			}

			out_stream->time_base = enc_contexts[i]->time_base;
		}
		//Copy audio and other streams
		else {
			if (avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar) < 0) {
				log_message(LOG_ERROR, "Failed to copy stream parameters for stream %d", i);
				stream_mapping[i] = -1;
				continue;
			}
			out_stream->time_base = in_stream->time_base;
		}
	}
	
	//Process output file

	//Open output file
	//Ceck agains AVFMT_NOFILE bitmask if format needs a file (not in-memory format)
	if (!(out_ctx->oformat->flags & AVFMT_NOFILE)) {
		if (avio_open(&out_ctx->pb, output_file, AVIO_FLAG_WRITE) < 0) {
			log_message(LOG_ERROR, "Failed to open output file %s", output_file);

			for (unsigned int i = 0; i < in_ctx->nb_streams; i++) {
				if (dec_contexts[i]) avcodec_free_context(&dec_contexts[i]);
				if (enc_contexts[i]) avcodec_free_context(&enc_contexts[i]);
				if (sws_contexts[i]) sws_freeContext(sws_contexts[i]);
			}

			av_free(sws_contexts);
			av_free(enc_contexts);
			av_free(dec_contexts);
			av_free(stream_mapping);
			avformat_free_context(out_ctx);
			avformat_close_input(&in_ctx);

			return false;
		}
	}

	// Write header
	if (avformat_write_header(out_ctx, NULL) < 0) {
		log_message(LOG_ERROR, "Failed to write header for %s", output_file);

		if (!(out_ctx->oformat->flags & AVFMT_NOFILE)) avio_closep(&out_ctx->pb);

		for (unsigned int i = 0; i < in_ctx->nb_streams; i++) {
			if (dec_contexts[i]) avcodec_free_context(&dec_contexts[i]);
			if (enc_contexts[i]) avcodec_free_context(&enc_contexts[i]);
			if (sws_contexts[i]) sws_freeContext(sws_contexts[i]);
		}

		av_free(sws_contexts);
		av_free(enc_contexts);
		av_free(dec_contexts);
		av_free(stream_mapping);
		avformat_free_context(out_ctx);
		avformat_close_input(&in_ctx);

		return false;
	}

	// Allocate packet and frame
	pkt = av_packet_alloc();
	if (!pkt) {
		log_message(LOG_ERROR, "Failed to allocate packet for %s", output_file);

		if (!(out_ctx->oformat->flags & AVFMT_NOFILE)) avio_closep(&out_ctx->pb);

		for (unsigned int i = 0; i < in_ctx->nb_streams; i++) {
			if (dec_contexts[i]) avcodec_free_context(&dec_contexts[i]);
			if (enc_contexts[i]) avcodec_free_context(&enc_contexts[i]);
			if (sws_contexts[i]) sws_freeContext(sws_contexts[i]);
		}

		av_free(sws_contexts);
		av_free(enc_contexts);
		av_free(dec_contexts);
		av_free(stream_mapping);
		avformat_free_context(out_ctx);
		avformat_close_input(&in_ctx);

		return false;
	}

	frame = av_frame_alloc();
	if (!frame) {
		log_message(LOG_ERROR, "Failed to allocate frame for %s", output_file);

		av_packet_free(&pkt);

		if (!(out_ctx->oformat->flags & AVFMT_NOFILE)) avio_closep(&out_ctx->pb);

		for (unsigned int i = 0; i < in_ctx->nb_streams; i++) {
			if (dec_contexts[i]) avcodec_free_context(&dec_contexts[i]);
			if (enc_contexts[i]) avcodec_free_context(&enc_contexts[i]);
			if (sws_contexts[i]) sws_freeContext(sws_contexts[i]);
		}

		av_free(sws_contexts);
		av_free(enc_contexts);
		av_free(dec_contexts);
		av_free(stream_mapping);
		avformat_free_context(out_ctx);
		avformat_close_input(&in_ctx);
		
		return false;
	}

	//Allocate converted_frame (holds converted pixel data for encoding)
	converted_frame = av_frame_alloc(); 
	if (!converted_frame) {
		log_message(LOG_ERROR, "Failed to allocate converted frame for %s", output_file);
		av_packet_free(&pkt);
		av_frame_free(&frame);

		if (!(out_ctx->oformat->flags & AVFMT_NOFILE)) avio_closep(&out_ctx->pb);

		for (unsigned int i = 0; i < in_ctx->nb_streams; i++) {
			if (dec_contexts[i]) avcodec_free_context(&dec_contexts[i]);
			if (enc_contexts[i]) avcodec_free_context(&enc_contexts[i]);
			if (sws_contexts[i]) sws_freeContext(sws_contexts[i]);
		}

		av_free(sws_contexts);
		av_free(enc_contexts);
		av_free(dec_contexts);
		av_free(stream_mapping);
		avformat_free_context(out_ctx);
		avformat_close_input(&in_ctx);
		return false;
	}

	// Process packets
	while (av_read_frame(in_ctx, pkt) >= 0) {

		//Unref packet if stream is not to be processed 
		if (stream_mapping[pkt->stream_index] < 0) {
			av_packet_unref(pkt);
			continue;
		}

		AVStream* in_stream = in_ctx->streams[pkt->stream_index];
		AVStream* out_stream = out_ctx->streams[stream_mapping[pkt->stream_index]];

		//Process only video
		if (in_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {

			if (!dec_contexts[pkt->stream_index] || !enc_contexts[pkt->stream_index] || !sws_contexts[pkt->stream_index]) {
				log_message(LOG_ERROR, "No codec context available for video stream %d", pkt->stream_index);
				av_packet_unref(pkt);
				continue;
			}

			//Send packed to decoder
			//logs for specific decoder error
			int ret = avcodec_send_packet(dec_contexts[pkt->stream_index], pkt);
			if (ret < 0) {
				char err_buf[128];
				av_strerror(ret, err_buf, sizeof(err_buf));
				log_message(LOG_ERROR, "Failed to send packet to decoder for stream %d: %s", pkt->stream_index, err_buf);
				av_packet_unref(pkt);
				continue;
			}

			//Read decoded frames 
			while (avcodec_receive_frame(dec_contexts[pkt->stream_index], frame) >= 0) {
				
				//temp - log frame parameters
				log_message(LOG_INFO, "Frame: w=%d h=%d fmt=%d", frame->width, frame->height, frame->format);

				//temp - validate frame parameters
				if (frame->width <= 0 || frame->height <= 0 || frame->format == AV_PIX_FMT_NONE) {
					log_message(LOG_ERROR, "Invalid frame parameters for stream %d", pkt->stream_index);
					av_frame_unref(frame);
					continue;
				}

				// convert pixel format to NV12
				converted_frame->width = frame->width;
				converted_frame->height = frame->height;
				converted_frame->format = enc_contexts[pkt->stream_index]->pix_fmt;
				if (av_frame_get_buffer(converted_frame, 0) < 0) {
					log_message(LOG_ERROR, "Failed to allocate buffer for converted frame");
					av_frame_unref(frame);
					continue;
				}
				if (sws_scale(sws_contexts[pkt->stream_index], frame->data, frame->linesize, 0, frame->height,
					converted_frame->data, converted_frame->linesize) <= 0) {
					log_message(LOG_ERROR, "Failed to convert frame for stream %d", pkt->stream_index);
					av_frame_unref(converted_frame);
					av_frame_unref(frame);
					continue;
				}

				converted_frame->pts = frame->pts;

				AVPacket* out_pkt = av_packet_alloc();
				if (!out_pkt) {
					log_message(LOG_ERROR, "Failed to allocate output packet");
					av_frame_unref(frame);
					continue;
				}

				//if (avcodec_send_frame(enc_contexts[pkt->stream_index], frame) < 0 || avcodec_receive_packet(enc_contexts[pkt->stream_index], out_pkt) < 0) {
				//	log_message(LOG_ERROR, "Failed to encode frame for stream %d", pkt->stream_index);
				//	av_packet_free(&out_pkt);
				//	av_frame_unref(frame);
				//	continue;
				//}

				////Rescale timestamps, making sure streams stay in sync 
				//out_pkt->pts = av_rescale_q_rnd(out_pkt->pts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
				//out_pkt->dts = av_rescale_q_rnd(out_pkt->dts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
				//out_pkt->duration = av_rescale_q(out_pkt->duration, in_stream->time_base, out_stream->time_base);
				//out_pkt->pos = -1;
				//out_pkt->stream_index = stream_mapping[pkt->stream_index];

				////Write compressed packet to output file 
				//if (av_interleaved_write_frame(out_ctx, out_pkt) < 0) {
				//	log_message(LOG_ERROR, "Error writing encoded frame to %s", output_file);
				//	av_packet_free(&out_pkt);
				//	av_frame_unref(frame);
				//	continue;
				//}

				// Send the converted frame to the encoder.
				// If sending fails, log the error and clean up resources
				ret = avcodec_send_frame(enc_contexts[pkt->stream_index], converted_frame);
				if (ret < 0) {
					char err_buf[128];
					av_strerror(ret, err_buf, sizeof(err_buf));
					log_message(LOG_ERROR, "Failed to send frame to encoder for stream %d: %s", pkt->stream_index, err_buf);

					av_packet_free(&out_pkt);
					av_frame_unref(converted_frame);
					av_frame_unref(frame);
					continue;
				}

				// Receive encoded packets from the encoder and write them to the output.
				// Handles EAGAIN (need more input), EOF (encoder flushed), and other errors.
				// Rescales timestamps and stream index before writing the packet.
				while (true) {
					ret = avcodec_receive_packet(enc_contexts[pkt->stream_index], out_pkt);
					if (ret == AVERROR(EAGAIN)) {
						break; // Need more frames
					}
					else if (ret == AVERROR_EOF) {
						log_message(LOG_WARNING, "Encoder for stream %d is flushed", pkt->stream_index);
						break; // No more packets
					}
					else if (ret < 0) {
						char err_buf[128];
						av_strerror(ret, err_buf, sizeof(err_buf));
						log_message(LOG_ERROR, "Failed to receive packet from encoder for stream %d: %s", pkt->stream_index, err_buf);
						av_packet_free(&out_pkt);
						av_frame_unref(converted_frame);
						av_frame_unref(frame);
						break;
					}

					out_pkt->pts = av_rescale_q_rnd(out_pkt->pts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
					out_pkt->dts = av_rescale_q_rnd(out_pkt->dts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
					out_pkt->duration = av_rescale_q(out_pkt->duration, in_stream->time_base, out_stream->time_base);
					out_pkt->pos = -1;
					out_pkt->stream_index = stream_mapping[pkt->stream_index];

					if (av_interleaved_write_frame(out_ctx, out_pkt) < 0) {
						log_message(LOG_ERROR, "Error writing encoded frame to %s", output_file);

						av_packet_free(&out_pkt);
						av_frame_unref(converted_frame);
						av_frame_unref(frame);
						break;
					}

					av_packet_unref(out_pkt);
				}

				av_packet_free(&out_pkt);
				av_frame_unref(converted_frame);
				av_frame_unref(frame);
			}
		} else {
			//Rescale audio timestamps 
			pkt->pts = av_rescale_q_rnd(pkt->pts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
			pkt->dts = av_rescale_q_rnd(pkt->dts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
			pkt->duration = av_rescale_q(pkt->duration, in_stream->time_base, out_stream->time_base);
			pkt->pos = -1;
			pkt->stream_index = stream_mapping[pkt->stream_index];

			//Write packets to output file
			if (av_interleaved_write_frame(out_ctx, pkt) < 0) {
				log_message(LOG_ERROR, "Error writing frame to %s", output_file);
				av_packet_unref(pkt);
				av_packet_free(&pkt);
				av_frame_free(&frame);
				av_frame_free(&converted_frame);

				if (!(out_ctx->oformat->flags & AVFMT_NOFILE)) avio_closep(&out_ctx->pb);

				for (unsigned int i = 0; i < in_ctx->nb_streams; i++) {
					if (dec_contexts[i]) avcodec_free_context(&dec_contexts[i]);
					if (enc_contexts[i]) avcodec_free_context(&enc_contexts[i]);
					if (sws_contexts[i]) sws_freeContext(sws_contexts[i]);
				}

				av_free(sws_contexts);
				av_free(enc_contexts);
				av_free(dec_contexts);
				av_free(stream_mapping);
				avformat_free_context(out_ctx);
				avformat_close_input(&in_ctx);

				return false;
			}
		}
		av_packet_unref(pkt);
	}

	// Flush decoders and encoders

	for (unsigned int i = 0; i < in_ctx->nb_streams; i++) {
		if (stream_mapping[i] < 0 || !dec_contexts[i] || !enc_contexts[i]) continue;

		// Flush decoder

		if (avcodec_send_packet(dec_contexts[i], NULL) < 0) {
			log_message(LOG_ERROR, "Failed to flush decoder for stream %d", i);
			continue;
		}

		while (avcodec_receive_frame(dec_contexts[i], frame) >= 0) {

			// temp - Log frame parameters
			log_message(LOG_INFO, "Flushed Frame: w=%d h=%d fmt=%d", frame->width, frame->height, frame->format);

			// temp - Validate frame parameters
			if (frame->width <= 0 || frame->height <= 0 || frame->format == AV_PIX_FMT_NONE) {
				log_message(LOG_ERROR, "Invalid flushed frame parameters for stream %d", i);
				av_frame_unref(frame);
				continue;
			}

			// temp - Convert pixel format for flushed frames
			converted_frame->width = frame->width;
			converted_frame->height = frame->height;
			converted_frame->format = enc_contexts[i]->pix_fmt;
			if (av_frame_get_buffer(converted_frame, 0) < 0) {
				log_message(LOG_ERROR, "Failed to allocate buffer for converted flushed frame");
				av_frame_unref(frame);
				continue;
			}
			if (sws_scale(sws_contexts[i], frame->data, frame->linesize, 0, frame->height,
				converted_frame->data, converted_frame->linesize) <= 0) {
				log_message(LOG_ERROR, "Failed to convert flushed frame for stream %d", i);
				av_frame_unref(converted_frame);
				av_frame_unref(frame);
				continue;
			}

			converted_frame->pts = frame->pts;

			AVPacket* out_pkt = av_packet_alloc();
			if (!out_pkt) {
				log_message(LOG_ERROR, "Failed to allocate output packet for flush");
				av_frame_unref(frame);
				continue;
			}


			/*if (avcodec_send_frame(enc_contexts[i], frame) < 0 ||
				avcodec_receive_packet(enc_contexts[i], out_pkt) < 0) {
				log_message(LOG_ERROR, "Failed to encode flushed frame for stream %d", i);
				av_packet_free(&out_pkt);
				av_frame_unref(frame);
				continue;
			}

			AVStream* in_stream = in_ctx->streams[i];
			AVStream* out_stream = out_ctx->streams[stream_mapping[i]];
			out_pkt->pts = av_rescale_q_rnd(out_pkt->pts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
			out_pkt->dts = av_rescale_q_rnd(out_pkt->dts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
			out_pkt->duration = av_rescale_q(out_pkt->duration, in_stream->time_base, out_stream->time_base);
			out_pkt->pos = -1;
			out_pkt->stream_index = stream_mapping[i];

			if (av_interleaved_write_frame(out_ctx, out_pkt) < 0) {
				log_message(LOG_ERROR, "Error writing flushed frame to %s", output_file);
				av_packet_free(&out_pkt);
				av_frame_unref(frame);
				continue;
			}*/


			// Flush encoder by sending a null frame (converted_frame reused here).
			// Continue with the next stream if flushing fails.
			int ret = avcodec_send_frame(enc_contexts[i], converted_frame);
			if (ret < 0) {
				char err_buf[128];
				av_strerror(ret, err_buf, sizeof(err_buf));
				log_message(LOG_ERROR, "Failed to send flushed frame to encoder for stream %d: %s", i, err_buf);
				av_packet_free(&out_pkt);
				av_frame_unref(converted_frame);
				av_frame_unref(frame);
				continue;
			}

			while (true) {
				ret = avcodec_receive_packet(enc_contexts[i], out_pkt);
				if (ret == AVERROR(EAGAIN)) {
					break; // Need more frames
				}
				else if (ret == AVERROR_EOF) {
					log_message(LOG_WARNING, "Encoder for stream %d is flushed", i);
					break; // No more packets
				}
				else if (ret < 0) {
					char err_buf[128];
					av_strerror(ret, err_buf, sizeof(err_buf));
					log_message(LOG_ERROR, "Failed to receive flushed packet for stream %d: %s", i, err_buf);
					av_packet_free(&out_pkt);
					av_frame_unref(converted_frame);
					av_frame_unref(frame);
					break;
				}

				AVStream* in_stream = in_ctx->streams[i];
				AVStream* out_stream = out_ctx->streams[stream_mapping[i]];
				out_pkt->pts = av_rescale_q_rnd(out_pkt->pts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
				out_pkt->dts = av_rescale_q_rnd(out_pkt->dts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
				out_pkt->duration = av_rescale_q(out_pkt->duration, in_stream->time_base, out_stream->time_base);
				out_pkt->pos = -1;
				out_pkt->stream_index = stream_mapping[i];

				if (av_interleaved_write_frame(out_ctx, out_pkt) < 0) {
					log_message(LOG_ERROR, "Error writing flushed frame to %s", output_file);
					av_packet_free(&out_pkt);
					av_frame_unref(converted_frame);
					av_frame_unref(frame);
					break;
				}

				av_packet_unref(out_pkt);
			}

			av_packet_free(&out_pkt);
			av_frame_unref(converted_frame);
			av_frame_unref(frame);
		}

		// Flush encoder

		if (avcodec_send_frame(enc_contexts[i], NULL) < 0) {
			log_message(LOG_ERROR, "Failed to flush encoder for stream %d", i);
			continue;
		}

		while (avcodec_receive_packet(enc_contexts[i], pkt) >= 0) {
			AVStream* in_stream = in_ctx->streams[i];
			AVStream* out_stream = out_ctx->streams[stream_mapping[i]];
			pkt->pts = av_rescale_q_rnd(pkt->pts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
			pkt->dts = av_rescale_q_rnd(pkt->dts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
			pkt->duration = av_rescale_q(pkt->duration, in_stream->time_base, out_stream->time_base);
			pkt->pos = -1;
			pkt->stream_index = stream_mapping[i];

			if (av_interleaved_write_frame(out_ctx, pkt) < 0) {
				log_message(LOG_ERROR, "Error writing flushed packet to %s", output_file);
				av_packet_unref(pkt);
				continue;
			}

			av_packet_unref(pkt);
		}
	}

	// Write final metadata chunk 
	if (av_write_trailer(out_ctx) < 0) {
		log_message(LOG_ERROR, "Failed to write trailer for %s", output_file);

		av_packet_free(&pkt);
		av_frame_free(&frame);
		av_frame_free(&converted_frame);

		if (!(out_ctx->oformat->flags & AVFMT_NOFILE)) avio_closep(&out_ctx->pb);

		for (unsigned int i = 0; i < in_ctx->nb_streams; i++) {
			if (dec_contexts[i]) avcodec_free_context(&dec_contexts[i]);
			if (enc_contexts[i]) avcodec_free_context(&enc_contexts[i]);
			if (sws_contexts[i]) sws_freeContext(sws_contexts[i]);
		}

		av_free(sws_contexts);
		av_free(enc_contexts);
		av_free(dec_contexts);
		av_free(stream_mapping);
		avformat_free_context(out_ctx);
		avformat_close_input(&in_ctx);
		return false;
	}

	status = true; 
	log_message(LOG_INFO, "Successfully compressed video %s", file);

	// Cleanup
	av_packet_free(&pkt);
	av_frame_free(&frame);
	av_frame_free(&converted_frame);

	if (!(out_ctx->oformat->flags & AVFMT_NOFILE)) avio_closep(&out_ctx->pb);

	for (unsigned int i = 0; i < in_ctx->nb_streams; i++) {
		if (dec_contexts[i]) avcodec_free_context(&dec_contexts[i]);
		if (enc_contexts[i]) avcodec_free_context(&enc_contexts[i]);
		if (sws_contexts[i]) sws_freeContext(sws_contexts[i]);
	}

	av_free(sws_contexts);
	av_free(enc_contexts);
	av_free(dec_contexts);
	av_free(stream_mapping);
	avformat_free_context(out_ctx);
	avformat_close_input(&in_ctx);

	return status;
}

/// @brief Create folder if needed and copy compressed file to the destination
/// @param root Root folder 
/// @param destination_folder Destination folder
/// @param file File to bo processed
void create_folder_process_file(const char* source_folder, const char* destination_folder, const char* filename)
{
	if (!source_folder || !destination_folder || !filename) {
		log_message(LOG_ERROR, "NULL parameter in create_folder_process_file");
		return;
	}

	char* base_filename = strrchr(filename, '/'); 
	if (!base_filename) base_filename = strrchr(filename, '\\'); 
	if (!base_filename) base_filename = (char*)filename;
	else base_filename++; //Increase pointer past the separator

	char* clean_filename = clean_name(base_filename, false); 
	if (!clean_filename) {
		log_message(LOG_ERROR, "Failed to clean filename %s", base_filename); 
		return;
	}

	//Extract relative path 
	char* subdirectory = extract_relative_dir(source_folder, filename); 
	if (!subdirectory) {
		log_message(LOG_ERROR, "Failed to extract subdirectory for %s", filename); 
		free(clean_filename); //malloc used in clean_name()
		return; 
	}

	//Create destination directory
	char destination_dir[1024]; 
	if (strlen(subdirectory) > 0) {
#ifdef _WIN32
		snprintf(destination_dir, sizeof(destination_dir), "%s\\%s", destination_folder, subdirectory);
#else
		snprintf(destination_dir, sizeof(destination_dir), "%s/%s", destination_folder, subdirectory); 
#endif 
	}
	else {
		strncpy(destination_dir, destination_folder, sizeof(destination_dir) - 1); 
		destination_dir[sizeof(destination_dir) - 1] = '\0';
	}
	free(subdirectory); 

	if (!create_directory(destination_dir)) {
		log_message(LOG_ERROR, "Failed to create destination directory %s", destination_dir);
		free(clean_filename); //malloc used in clean_name()
		return;
	}

	//Create a file path and make a copy on the destination location
	char destination_path[1024]; 
#ifdef _WIN32
	snprintf(destination_path, sizeof(destination_path), "%s\\%s", destination_path, clean_filename);
#else
	snprintf(destination_path, sizeof(destination_path), "%s/%s", destination_path, clean_filename);
#endif

	const char* last_step = strrchr(filename, '\\');
	if (!last_step) last_step = strrchr(filename, '/'); 
	const char* raw_filename = last_step ? last_step + 1 : filename;
	
	const char* extension = strrchr(raw_filename, '.'); //Find the extension
	char base_name[512]; 
	if (extension && extension != raw_filename) {
		size_t base_len = (size_t)(extension - raw_filename); 
		snprintf(base_name, sizeof(base_name), "%.*s", (int)base_len, raw_filename);
	}
	else {
		extension = NULL;
		snprintf(base_name, sizeof(base_name), "%s", raw_filename);
	}
	char* clean_basename = clean_name(base_name, false);
	if (!clean_basename) {
		log_message(LOG_ERROR, "Failed to clean basename"); 
		return;
	}

	//Create path of temporary copy to be compressed 
	char temp_filename[1024];
	if (extension) {
		snprintf(temp_filename, sizeof(temp_filename), "%s%s", clean_basename, extension);
	}
	else {
		snprintf(temp_filename, sizeof(temp_filename), "%s", clean_basename);
	}
	free(clean_basename);

	//Copy file to the destination path
	char temp_file[1024]; 
#ifdef _WIN32
	snprintf(temp_file, sizeof(temp_file), "%s\\%s", destination_dir, temp_filename);
#else
	snprintf(temp_file, sizeof(temp_file), "%s/%s", destination_dir, temp_filename);
#endif 

	if (file_exists(destination_path)) {
		long file_size = get_file_size(filename);
		long file_copy_size = get_file_size(destination_path);

		if (file_size > file_copy_size) {
			log_message(LOG_WARNING, "Compressed copy already exists at the location %s", destination_path);
			return;
		}
	}

	if (!copy_file(filename, temp_file)) {
		log_message(LOG_ERROR, "Failed to create temporary file copy for %s", temp_file); 
		free(clean_filename); 
		return;
	}

	//Compress the file copy
	bool compressed = compress_file(filename, temp_file);
}
