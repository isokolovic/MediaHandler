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

/// @brief File name, extension and compressed status (true if compressed)
struct TestFile {
    const char* basename;
    const char* ext;
    bool compress;
};
extern const TestFile TEST_FILES[];
extern const char* EXTENSIONS[];

bool ensure_target_dir();
void cleanup_target_dir();
std::string read_log_content(const char* path);
void log_test_outcome(const char* name, bool pass);
int count_valid_outputs();
std::vector<std::string> list_files_in_dir(const char* dir, const char* ext = nullptr);

#endif
