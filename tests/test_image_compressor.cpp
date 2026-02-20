#include <gtest/gtest.h>
#include "compressor/image_processor.h"
#include "utils/utils.h"
#include "test_common.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>

namespace fs = std::filesystem;

class ImageCompressionTest : public media_handler::tests::TestCommon {
protected:
    // Set your test data directory here
    fs::path test_data_dir = R"(C:\Users\isoko\Desktop\New folder\S)";

    // Set max allowed size percentage (e.g., 70 means output can be up to 70% of input size)
    double max_size_increase_percent = 70;

    void SetUp() override {
        if (!fs::exists(test_data_dir)) {
            GTEST_FAIL() << "Test data directory does not exist: " << test_data_dir;
        }

        TestCommon::SetUp();
    }

    void TearDown() override {
        TestCommon::TearDown();
    }

    /// @brief Find the first file with one of the given extensions in the test data directory
    fs::path find_test_file(const std::vector<std::string>& extensions) {
        if (!fs::exists(test_data_dir)) return fs::path();

        for (const auto& entry : fs::directory_iterator(test_data_dir)) {
            if (!entry.is_regular_file()) continue;

            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            for (const auto& wanted_ext : extensions) {
                if (ext == wanted_ext) {
                    return entry.path();
                }
            }
        }

        return fs::path();
    }

    /// @brief Verify that the file has a valid JPEG signature (SOI marker)
    static bool verify_jpeg_signature(const fs::path& file) {
        std::ifstream f(file, std::ios::binary);
        if (!f) return false;
        unsigned char sig[2];
        f.read(reinterpret_cast<char*>(sig), 2);
        return sig[0] == 0xFF && sig[1] == 0xD8;
    }

    /// @brief Verify that the file has a valid PNG signature 
    static bool verify_png_signature(const fs::path& file) {
        std::ifstream f(file, std::ios::binary);
        if (!f) return false;
        unsigned char sig[8];
        f.read(reinterpret_cast<char*>(sig), 8);
        return memcmp(sig, "\x89PNG\r\n\x1A\n", 8) == 0;
    }

    /// @brief Verify output size is within acceptable range
    void verify_size(const fs::path& input, const fs::path& output) {
        auto input_size = fs::file_size(input);
        auto output_size = fs::file_size(output);

        double size_ratio = (static_cast<double>(output_size) / input_size) * 100.0;

        EXPECT_LE(size_ratio, max_size_increase_percent)
            << "Output size (" << output_size << " bytes) is " << size_ratio
            << "% of input size (" << input_size << " bytes), exceeds limit of "
            << max_size_increase_percent << "%";
    }
};

// Tests

/// @brief Test compressing a JPEG file and verify output is valid JPEG 
TEST_F(ImageCompressionTest, CompressJPEG) {
    auto input = find_test_file({ ".jpg", ".jpeg" });
    if (input.empty()) {
        GTEST_SKIP() << "No JPEG found in " << test_data_dir;
    }

    media_handler::utils::Config config;
    config.output_dir = test_dir.string();
    auto logger = media_handler::utils::Logger::create("ImageCompressionTest");

    media_handler::compressor::ImageProcessor processor(config, logger);
    fs::path output = test_dir / input.filename();  // Keep original name

    auto result = processor.compress(input, output);

    ASSERT_TRUE(result.success) << "Failed: " << result.message;
    ASSERT_TRUE(fs::exists(output));
    EXPECT_TRUE(verify_jpeg_signature(output));
    verify_size(input, output);
}

/// @brief Test compressing a PNG file and verify output is valid PNG 
TEST_F(ImageCompressionTest, CompressPNG) {
    auto input = find_test_file({ ".png" });
    if (input.empty()) {
        GTEST_SKIP() << "No PNG found in " << test_data_dir;
    }

    media_handler::utils::Config config;
    config.output_dir = test_dir.string();
    auto logger = media_handler::utils::Logger::create("ImageCompressionTest");

    media_handler::compressor::ImageProcessor processor(config, logger);
    fs::path output = test_dir / input.filename(); 

    auto result = processor.compress(input, output);

    ASSERT_TRUE(result.success) << "Failed: " << result.message;
    ASSERT_TRUE(fs::exists(output));
    EXPECT_TRUE(verify_png_signature(output));
    verify_size(input, output);
}

/// @brief Test compressing a HEIC file and verify output is converted to JPEG 
TEST_F(ImageCompressionTest, CompressHEIC_ConvertsToJPEG) {
    auto input = find_test_file({ ".heic", ".heif" });
    if (input.empty()) {
        GTEST_SKIP() << "No HEIC found in " << test_data_dir;
    }

    media_handler::utils::Config config;
    config.output_dir = test_dir.string();
    auto logger = media_handler::utils::Logger::create("ImageCompressionTest");

    media_handler::compressor::ImageProcessor processor(config, logger);
    fs::path output = test_dir / input.filename();

    auto result = processor.compress(input, output);

    ASSERT_TRUE(result.success) << "Failed: " << result.message;

    // Should convert to JPEG (with .jpg extension)
    fs::path expected_jpeg = test_dir / input.stem();
    expected_jpeg.replace_extension(".jpg");

    ASSERT_TRUE(fs::exists(expected_jpeg)) << "Expected JPEG at: " << expected_jpeg;
    EXPECT_TRUE(verify_jpeg_signature(expected_jpeg));
    verify_size(input, expected_jpeg);
}

/// @brief Test handling of non-existent input file 
TEST_F(ImageCompressionTest, HandleNonExistentFile) {
    media_handler::utils::Config config;
    config.output_dir = test_dir.string();
    auto logger = media_handler::utils::Logger::create("ImageCompressionTest");

    media_handler::compressor::ImageProcessor processor(config, logger);

    auto result = processor.compress("nonexistent.jpg", test_dir / "out.jpg");

    EXPECT_FALSE(result.success);
}

/// @brief Test that file extensions are handled case-insensitively 
TEST_F(ImageCompressionTest, CaseInsensitiveExtensions) {
    auto input = find_test_file({ ".jpg", ".jpeg" });
    if (input.empty()) {
        GTEST_SKIP() << "No JPEG found in " << test_data_dir;
    }

    // Copy with uppercase extension
    fs::path upper_input = test_dir / "TEST.JPG";
    fs::copy_file(input, upper_input, fs::copy_options::overwrite_existing);

    media_handler::utils::Config config;
    config.output_dir = test_dir.string();
    auto logger = media_handler::utils::Logger::create("ImageCompressionTest");

    media_handler::compressor::ImageProcessor processor(config, logger);
    fs::path output = test_dir / "TEST_OUTPUT.JPG";

    auto result = processor.compress(upper_input, output);

    ASSERT_TRUE(result.success);
    EXPECT_TRUE(verify_jpeg_signature(output));
    verify_size(upper_input, output);
}