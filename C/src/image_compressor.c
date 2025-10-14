#include "file_handler.h"
#include "image_compressor.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>
#include <libheif/heif.h>
#include <libexif/exif-data.h>
#include <png.h>

/// @brief Libjpeg-turbo compression used for .jpg/.jpeg files
/// @param file Input image
/// @param output_file Compressed image
/// @return True if compression successful
bool compress_jpeg(const char* file, const char* output_file) {
	FILE* in_file = NULL;
	FILE* out_file = NULL;
	struct jpeg_decompress_struct cinfo;
	struct jpeg_compress_struct cinfo_out;
	struct jpeg_error_mgr jerr;
	JSAMPARRAY buffer = NULL;

	// Open input file for reading
	in_file = fopen(file, "rb");
	if (!in_file) {
		log_message(LOG_ERROR, "Failed to open %s for reading: %s (errno %d)", file, strerror(errno), errno);
		return false;
	}

	// Open output file for writing
	out_file = fopen(output_file, "wb");
	if (!out_file) {
		log_message(LOG_ERROR, "Failed to open %s for writing: %s (errno %d)", output_file, strerror(errno), errno);
		fclose(in_file);
		return false;
	}

	// Initialize decompression and read header
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, in_file);
	if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
		log_message(LOG_ERROR, "Failed to read JPEG header for %s", file);
		goto cleanup;
	}

	// Start decompression (critical for output state)
	jpeg_start_decompress(&cinfo);

	// Initialize compression
	cinfo_out.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo_out);
	jpeg_stdio_dest(&cinfo_out, out_file);

	// Set parameters using output dimensions (no trimming)
	cinfo_out.image_width = cinfo.output_width;
	cinfo_out.image_height = cinfo.output_height;
	cinfo_out.input_components = cinfo.output_components;
	cinfo_out.in_color_space = cinfo.out_color_space;
	jpeg_set_defaults(&cinfo_out);
	jpeg_set_quality(&cinfo_out, PHOTO_QUALITY, TRUE);
	jpeg_start_compress(&cinfo_out, TRUE);

	//Retain exif data
	ExifData* exif_data = exif_data_new_from_file(file);
	if (exif_data) {
		unsigned char* exif_buf = NULL;
		unsigned int exif_len = 0;

		exif_data_save_data(exif_data, &exif_buf, &exif_len);
		//Exif header = 6 bytes
		if (exif_buf && exif_len > 6 && memcmp(exif_buf, "Exif\0\0", 6) == 0) {
			jpeg_write_marker(&cinfo_out, JPEG_APP0 + 1, exif_buf, exif_len);
		}
		//Clean up allocated memory 
		if (exif_buf) free(exif_buf);
		exif_data_unref(exif_data);
	}

	// Allocate buffer using output params
	buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, cinfo.output_width * cinfo.output_components, 1);
	if (!buffer) {
		log_message(LOG_ERROR, "Failed to allocate JPEG buffer for %s", file);
		goto cleanup;
	}

	// Process scanlines
	while (cinfo.output_scanline < cinfo.output_height) {
		jpeg_read_scanlines(&cinfo, buffer, 1);
		jpeg_write_scanlines(&cinfo_out, buffer, 1);
	}

	// Finish and clean up
	jpeg_finish_compress(&cinfo_out);
	jpeg_finish_decompress(&cinfo);
	if (buffer) cinfo.mem->free_pool((j_common_ptr)&cinfo, JPOOL_IMAGE);

cleanup:
	jpeg_destroy_compress(&cinfo_out);
	jpeg_destroy_decompress(&cinfo);
	fclose(in_file);
	fclose(out_file);
	return (buffer != NULL);  // Success if buffer allocated
}

/// @brief Libheif compression used for .heic files
/// @param file Input image
/// @param output_file Compressed image
/// @return True if compression successful
bool compress_heic(const char* file, const char* output_file) { //TODO add exif data
	struct heif_context* context = NULL;
	struct heif_image_handle* handle = NULL;

	struct heif_image* image = NULL;
	struct heif_encoder* encoder = NULL;

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

/// @brief Libpng compression used for .png files
/// @param file Input image
/// @param output_file Compressed image
/// @return True if compression successful
bool compress_png(const char* file, const char* output_file)
{
	png_structp png_read_ptr = NULL;
	png_infop read_info_ptr = NULL;
	png_structp png_write_ptr = NULL;
	png_infop write_info_ptr = NULL;
	unsigned char* image_data = NULL;

	FILE* in_file = NULL;
	FILE* out_file = NULL;

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
		log_message(LOG_ERROR, "Failed to open output file %s: %s", output_file, strerror(errno));
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

/// @brief Compress image file
/// @param file Input image
/// @param output_file Compressed image
/// @return True if compression successful
bool compress_image(const char* file, const char* output_file, const char* extension) {
    if (!file || !output_file || !extension) {
        log_message(LOG_ERROR, "Null input or output path in compress_image");
        return false;
    }

    if (strcasecmp(extension, ".heic") == 0) {
        return compress_heic(file, output_file);
    }
    else if (strcasecmp(extension, ".jpg") == 0 || strcasecmp(extension, ".jpeg") == 0) {
        return compress_jpeg(file, output_file);
    }
    else if (strcasecmp(extension, ".png") == 0) {
        return compress_png(file, output_file);
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

