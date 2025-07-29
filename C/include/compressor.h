#ifndef COMPRESS_H
#define COMPRESS_H

#include <stdbool.h>
#include "file_handler.h"

void create_folder_process_file(const char* root, const char* source_folder, const char* destination_folder);
bool compress_file(const char* file, const char* output_file);
bool compress_image(const char* file, const char* output_file);

#endif