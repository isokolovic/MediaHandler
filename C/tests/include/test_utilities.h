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

bool ensure_target_dir();
void cleanup_target_dir();
std::string read_log_content(const char* path);
void log_test_outcome(const char* name, bool pass);
int count_valid_outputs();
std::vector<std::string> list_files_in_dir(const char* dir);
std::vector<std::string> discover_source_files();
void create_mock_log(const char* filename);
char* get_output_path(const char* file_name);
bool is_compressed_smaller(const char* in_file, const char* out_file);
bool is_compressed_equal(const char* in_file, const char* out_file);
bool file_in_correct_year_folder(const char* target_dir, const char* file_path);
std::vector<std::string> list_subdirs_in_dir(const char* dir);

#endif
