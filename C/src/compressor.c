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

	char* clean_filename = clean_name(base_filename, false); 
	if (!clean_filename) {
		log_message(LOG_ERROR, "Failed to clean filename %s", base_filename); 
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


}
