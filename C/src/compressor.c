#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compressor.h"
#include "logger.h"
#include "file_handler.h"

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

	//Extract extension from clean_filename and build filename: base + _copy + extension
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

	//Create temporary copy to be compressed
	char temp_filename[1024]; 
	if (extension) {
		snprintf(temp_filename, sizeof(temp_filename), "%s_copy%s", clean_basename, extension);
	}
	else {
		snprintf(temp_filename, sizeof(temp_filename), "%s_copy", clean_basename);
	}
	free(clean_basename);

	//File copy destination path
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





}
