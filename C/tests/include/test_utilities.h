#ifndef TEST_UTILITIES
#define TEST_UTILITIES

#include <gtest/gtest.h>
#include <string.h>
#include "file_handler.h"
#include "logger.h"
#include "image_compressor.h"
#include "video_compressor.h"

extern const char* SOURCE_DIR;
extern const char* TARGET_DIR;
extern const char* LOG_FILE;

/// @brief Ensure target directory exists
/// @return True if exists / created
bool ensure_target_dir();

/// @brief Cleanup target directory after test run
void cleanup_target_dir();

/// @brief Read log file content
/// @param path Log file path
/// @return Log file stringstream
std::string read_log_content(const char* path);

/// @brief Lot the outcome of logging test
/// @param name Test identifier
/// @param pass Test
void log_test_outcome(const char* name, bool pass);

/// @brief Cout files in target directory
/// @return Number of files
int count_valid_outputs();

/// @brief List files in target directory and subdirectories
/// @param dir Parent target directory
/// @return Vector of filenames
std::vector<std::string> list_files_in_dir(const char* dir);

/// @brief Discover files in source directory
/// @return Vector of source files
std::vector<std::string> discover_source_files();

/// @brief Creates a mock log file from files in source directory
/// @param filename File to be logged as failed
/// @brief Appends a mock FAILED entry to the log file for testing retry logic.
/// @param filename File to be logged as failed (relative to SOURCE_DIR).
void create_mock_log(const char* filename);

/// @brief Get full target dir file path
/// @param file_name File name
/// @return Full file path
char* get_output_path(const char* file_name);

/// @brief Check if file is compressed
/// @param in_file Input file
/// @param out_file Output file
/// @return True if output is smaller than input
bool is_compressed_smaller(const char* in_file, const char* out_file);

/// @brief Check if file is copied
/// @param in_file Input file
/// @param out_file Output file
/// @return True if output file size is equal to input file
bool is_compressed_equal(const char* in_file, const char* out_file);

/// @brief Check if target file path is put to correct year in dest folder
/// @param target_dir Target directory
/// @param file_path Source file
/// @return True if file is put to correct sub-folder
bool file_in_correct_year_folder(const char* target_dir, const char* file_path);

/// @brief List subdirectories in tarted dir
/// @param dir target dir
/// @return Subdirectories
std::vector<std::string> list_subdirs_in_dir(const char* dir);

#endif
