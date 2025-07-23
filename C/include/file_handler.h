#ifndef FILE_HANDLER
#define FILE_HANDLER

#include <stddef.h>
#include <stdbool.h>

int get_valid_directory(const char* prompt, char* buffer, size_t size);
bool is_directory(const char* path);
bool is_file_type_valid(const char* filename); 
bool create_directory(const char* root);
char* clean_name(const char* element_name, bool is_directory);
char* extract_relative_dir(const char* source_path, const char* file_path);
long long get_file_size(const char* path);

#endif