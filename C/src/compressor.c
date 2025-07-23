#include <stdio.h>
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

	char* clean_filename = clean_name(base_filename); 







//	char source_path[1024]; 
//#ifdef _WIN32
//	snprintf(source_path, sizeof(source_path), "%s\\%s", source_folder, filename); 
//#else
//	snprintf(source_folder, sizeof(source_path), "%s/%s", source_folder, filename); 
//#endif 
//
//
//	char* relative_dir = extract_relative_dir(source_path, filename); 
//	if (!relative_dir) {
//		log_message(LOG_ERROR, "Failed to extract relative directory for %s", filename); 
//	}
//
//	char destination_dir[1024]; 
//	if (relative_dir != NULL && strlen(relative_dir) > 0) {
//#ifdef _WIN32
//		snprintf(destination_dir, sizeof(destination_dir), "%s\\%s", destination_folder, relative_dir); 
//#else
//		snprintf(destination_dir, sizeof(destination_dir), "%s/%s", destination_folder, relative_dir); 
//#endif 
//	}


}
