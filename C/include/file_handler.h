#ifndef FILE_HANDLER
#define FILE_HANDLER

#include <stddef.h>
#include <stdbool.h>

int get_valid_directory(const char* prompt, char* buffer, size_t size);
bool is_directory(const char* path);
bool is_file_type_valid(const char* filename); 
bool create_folder(const char* root);
#endif