#ifndef FILE_HANDLER
#define FILE_HANDLER

#define strcasecmp _stricmp
#define EMTPY_FILENAME "Unnamed"

#include <stddef.h>
#include <stdbool.h>

int get_valid_directory(const char* prompt, char* buffer, size_t size);
bool is_directory(const char* path);
bool is_image(const char* filename); 
bool is_video(const char* filename);
bool is_gif_misc(const char* filename);
bool is_file_type_valid(const char* filename); 
bool create_directory(const char* root);
bool file_exists(const char* path); 
bool copy_file(const char* source, const char* destination);

char* clean_name(const char* element_name, bool is_directory);
char* extract_relative_dir(const char* source_path, const char* file_path);
long get_file_size(const char* path);

bool create_folder_process_file(const char* source_folder, const char* destination_folder, const char* filename);
bool compress_file(const char* file, const char* output_file);

//For retry mode (-r)
char** get_failed_files_from_log(const char* log_path, int* num_failed);
void retry_failed_files(const char* root_source, const char* destination_folder, char** failed_files, int num_failed, int* total_processed, int* total_failed); 

#endif