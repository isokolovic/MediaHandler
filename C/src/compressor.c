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
	
}

/// @brief Compress image file
/// @param file Input image
/// @param output_file Compressed image
/// @return True if compression successful
bool compress_image(const char* file, const char* output_file)
{	
	FILE* infile = NULL;
	FILE* outfile = NULL;

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
		err = heif_decode_image(handle, &image, heif_colorspace_RGB, heif_chroma_420, NULL);
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
		infile = fopen(file, "rb"); 
		if (!infile) {
			log_message(LOG_ERROR, "Failed to open %s for writing", file);
			return false;
		}

		//Open output file for writing
		outfile = fopen(output_file, "wb"); 
		if (!outfile) {
			fclose(file); 
			log_message(LOG_ERROR, "Failed to open %s for writing", file); 
			return false;
		}

		// Initialize .jpeg decompression and read image header from input file
		cinfo.err = jpeg_std_error(&jerr); 
		jpeg_create_decompress(&cinfo); 
		jpeg_stdio_src(&cinfo, infile);
		jpeg_read_header(&cinfo, TRUE); 

		// Set up .jpeg compression and link output file as destination
		cinfo_out.err = jpeg_std_error(&jerr); 
		jpeg_create_compress(&cinfo_out); 
		jpeg_stdio_dest(&cinfo_out, outfile); 

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
		fclose(infile);
		fclose(outfile); 
		return true;
	}
	//Libpng compression used for png files
	else if((strcasecmp(extension, ".png") == 0)){
		png_structp png_read_ptr = NULL;
	}
	else {

	}
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
		strncpy(destination_dir, destination_folder, sizeof(destination_dir - 1)); 
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

	//Extract file extension from clean_filename and build filename: base + _copy + extension
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
	char compressed_filename[1024];
	if (extension) {
		snprintf(temp_filename, sizeof(temp_filename), "%s_copy%s", clean_basename, extension);
		snprintf(compressed_filename, sizeof(temp_filename), "%s%s", clean_basename, extension);
	}
	else {
		snprintf(temp_filename, sizeof(temp_filename), "%s_copy", clean_basename);
		snprintf(compressed_filename, sizeof(temp_filename), "%s", clean_basename);
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
	bool compressed = compress_file(temp_filename, compressed_filename);



}
