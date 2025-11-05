#ifndef FILE_HANDLER
#define FILE_HANDLER

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#define strcasecmp _stricmp
#else
#include <strings.h>
#define str_icmp strcasecmp
#endif

#define EMPTY_FILENAME "Unnamed" 

#include <stddef.h>
#include <stdbool.h>

/// @brief Check if source / destination folders are found upon user entry
/// @param folder Path to the source / destination folder
int get_valid_directory(const char* prompt, char* buffer, size_t size);

/// @brief Check if provided path is a directory
/// @param path Path provided by the user
/// @return 1 if the path is a directory, 0 otherwise
bool is_directory(const char* path);

/// @brief Check if file type is image
/// @param filename Path to a file
/// @return True if image
bool is_image(const char* filename);

/// @brief Check if file type is video
/// @param filename Path to a file
/// @return True if video
bool is_video(const char* filename);

/// @brief Check if file type is gif or other supported
/// @param filename Path to a file
/// @return True if gif or other supported type
bool is_gif_misc(const char* filename);

/// @brief Check if file needs to be processed
/// @param filename File path
/// @return True if file needs to be processed
bool is_file_type_valid(const char* filename);

/// @brief Create a directory
/// @param path Directory to be created path
/// @return True if successful
bool create_directory(const char* root);

/// @brief Check if file exist on specified path
/// @param path Path to a file
/// @return True if file exists
bool file_exists(const char* path);

/// @brief Copy file from the source to the destination
/// @param source Copy source
/// @param destination Copy destination
/// @return True if copying is successful
bool copy_file(const char* source, const char* destination);

/// @brief Clean file / folder name of any special characters
/// @param element Element to be cleaned
/// @return Cleaned name, NULL if error, EMTPY_FILENAME if strlen(cleaned_element) == 0
char* clean_name(const char* element_name, bool is_directory);

/// @brief Extract relative path (subfolder)
/// @param file_path Full file path
/// @return Subfolder
char* extract_relative_dir(const char* source_path, const char* file_path);

/// @brief Get file size 
/// @param path Path to a file
/// @return File size
long get_file_size(const char* path);

/// @brief Create folder if needed and copy compressed file to the destination
/// @param root Root folder 
/// @param destination_folder Destination folder
/// @param file File to bo processed
bool create_folder_process_file(const char* source_folder, const char* destination_folder, const char* filename);

/// @brief Main function for file compression
/// @param file File to be compressed
/// @param output_file Compressed file
/// @return True if successful
bool compress_file(const char* file, const char* output_file);

// Retry mode (-r)

/// @brief Function to parse LOG_FILE for failed files
/// @param log_path Path to the LOG_FILE
/// @param num_failed number of failed files (avoids looping through array to count elements)
/// @return NULL-terminated array of file paths (strings) - handling variable-length lists of paths without fixed-size buffers
char** get_failed_files_from_log(const char* log_path, int* num_failed);

/// @brief Retry failed file processing attempts.
/// @param root_source Source directory
/// @param destination_folder Destination directory
/// @param failed_files Array of failed file paths.
/// @param num_failed Number of failed files
/// @param total_processed Pointer to total processed counter
/// @param total_failed Pointer to total failed counter
void retry_failed_files(const char* root_source, const char* destination_folder, char** failed_files, int num_failed, int* total_processed, int* total_failed);

// Organize mode (-o)

/// @brief Returns the oldest timestamp year (creation or modification)
/// @param path Full path to the file
/// @return Year of oldest timestamp, or 0 on error
int get_file_creation_year(const char* file_path);

/// @brief Move file to year-based folder
/// @param root_source Root source folder
/// @param destination_folder Destination base folder
/// @param file_path File path
/// @param year File creation year
/// @return 1 on success, 0 on failure
int move_file_to_year_folder(const char* root_source, const char* destination_folder, const char* file_path, int year);

/// @brief Organize files by creation year
/// @param root_source Root source folder
/// @param source_folder Current folder to process
/// @param destination_folder Destination base folder
/// @param processed Pointer to processed counter
/// @param failed Pointer to failed counter
void organize_files(const char* root_source, const char* source_folder, const char* destination_folder, int* processed, int* failed);

/// @brief Extracts a path value from the log file
/// @param source_target "SOURCE_DIR=" or "TARGET_DIR="
/// @param out_path Extracted path
/// @param max_len Max length of the output buffer
/// @return True if the path was successfully extracted
bool extract_path_from_log(const char* source_target, char* out_path, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // FILE_HANDLER
