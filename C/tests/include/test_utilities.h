#ifndef TEST_UTILS_H
#define TEST_UTILS_H

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

bool ensure_trget_dir();

#endif
