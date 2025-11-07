#include <filesystem>
#include "test_utilities.h"

namespace fs = std::filesystem;

class MediaCompressionTest : public ::testing::Test {
protected:
    std::vector<std::string> files;

    void SetUp() override {
        ASSERT_TRUE(ensure_target_dir());
        init_logger(LOG_FILE, "w");
        files = discover_source_files();
        ASSERT_FALSE(files.empty()) << "No media in " << SOURCE_DIR;
    }

    void TearDown() override {
        close_logger();
        remove(LOG_FILE);
        cleanup_target_dir();
    }
};

/// @brief Verifies that the migration successfully processes all source files.
/// Compresses or copies each media file and checks output validity and size constraints.
/// @param SOURCE_DIR Source directory
/// @param TARGET_DIR Target directory
TEST_F(MediaCompressionTest, AllMediaCompressOrCopy) {
    for (const auto& filename : files) {
        fs::path full_input_path = fs::path(SOURCE_DIR) / filename;
        std::string in_file_path_str = full_input_path.string();
        const char* in_file_path = in_file_path_str.c_str();

        std::string in_file_name_str = full_input_path.filename().string();
        const char* in_file_name = in_file_name_str.c_str();

        char* target_file_path = get_output_path(in_file_name);
        const char* extension = strrchr(target_file_path, '.');
        bool should_compress = is_video(in_file_name) || is_image(in_file_name);

        if (should_compress) {
            bool is_file_compressed = is_video(target_file_path)
                ? compress_video(in_file_path, target_file_path)
                : compress_image(in_file_path, target_file_path, extension);

            ASSERT_TRUE(is_file_compressed) << "Compression failed for: " << in_file_path;
            ASSERT_TRUE(get_file_size(target_file_path) > 0) << "Missing: " << target_file_path;
        }

        if (should_compress) {
            ASSERT_TRUE(is_compressed_smaller(in_file_path, target_file_path))
                << "Compressed file is not smaller than source file: " << in_file_path;
        }
        else if (is_gif_misc(target_file_path)) {
            ASSERT_TRUE(is_compressed_equal(in_file_path, target_file_path))
                << "Source and target file sizes don't match: " << in_file_path;
        }

        log_test_outcome(in_file_name, true);
        free(target_file_path);
    }
}
