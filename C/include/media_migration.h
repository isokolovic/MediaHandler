#ifndef MEDIA_MIGRATION
#define MEDIA_MIGRATION

#include "file_handler.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/// @brief Main function for the migration
/// @param source_folder Folder containing original media
/// @param destination_folder Folder where compressed media will be put 
void run_media_migration(const char* src_dir, const char* src_log, const char* tgt_dir, int* processed, int* failed);

#ifdef __cplusplus
}
#endif

#endif // MEDIA_MIGRATION
