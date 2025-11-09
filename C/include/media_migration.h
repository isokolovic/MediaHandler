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
	/// @param src_dir Root source directory
	/// @param src_log Current subdirectory being processed
	/// @param tgt_dir Destination directory for compressed media
	/// @param processed Pointer to count of processed files
	/// @param failed Pointer to count of failed files
	void run_media_migration(const char* src_dir, const char* src_log, const char* tgt_dir, int* processed, int* failed);

#ifdef __cplusplus
}
#endif

#endif // MEDIA_MIGRATION
