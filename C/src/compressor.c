#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>
#include <libheif/heif.h>
#include <libexif/exif-data.h> 
#include <libavformat/avformat.h>
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

	//Libheif compression used for heic files
	if (strcmp(extension, ".heic") == 0) {
		struct heif_context* context = NULL;
		struct heif_image_handle* handle = NULL;

		struct heif_image* image = NULL;
		struct heif_encoder* encoder = NULL;


		//const char* const* dirs = heif_get_plugin_directories();
		//if (!dirs) {
		//	printf("No plugin directories found.\n");
		//	return;
		//}
		//for (int i = 0; dirs[i]; ++i) {
		//	printf("libheif plugin dir: %s\n", dirs[i]);
		//}
		//heif_free_plugin_directories(dirs);
		
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
	//Libjpeg-turbo compression used for jpg/jpeg files
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
	//Libpng compression used for png files
	else if((strcasecmp(extension, ".png") == 0)){
		png_structp png_read_ptr = NULL;
		png_infop read_info_ptr = NULL;
		png_structp png_write_ptr = NULL;
		png_infop write_info_ptr = NULL;
		unsigned char* image_data = NULL; 

		//Open input file
		in_file = fopen(file, 'rb'); 
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
	// Video compression using ffmpeg's command line interface
	// Uncomment if want to use that instead of manual.It's safer and less error-prone

	char cmd[1024];
	snprintf(cmd, sizeof(cmd), "ffmpeg -i \"%s\" -vcodec h264 \"%s\"", file, output_file);
	int ret = system(cmd);
	if (ret != 0) {
		log_message(LOG_ERROR, "Failed to compress video %s: %d", file, ret);
		return false;
	}
	return true;




	return false;
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
