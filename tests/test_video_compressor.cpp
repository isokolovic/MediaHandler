#include <gtest/gtest.h>
#include "compressor/video_processor.h"
#include "utils/utils.h"
#include "test_common.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class VideoCompressionTest : public media_handler::tests::TestCommon {
protected:
    fs::path test_data_dir;
    double max_size_increase_percent = 60.0; //max allowed size

    void SetUp() override {
        // Set your test data directory here
        test_data_dir = R"(C:\Users\isoko\Desktop\New folder\S)";

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

    /// @brief Verify that the file has a valid MP4 signature
    static bool verify_mp4_signature(const fs::path& file) {
        std::ifstream f(file, std::ios::binary);
        if (!f) return false;
        unsigned char sig[12];
        f.read(reinterpret_cast<char*>(sig), 12);
        return f.gcount() >= 12 && memcmp(&sig[4], "ftyp", 4) == 0;
    }

    /// @brief Verify that the file has a valid AVI signature
    static bool verify_avi_signature(const fs::path& file) {
        std::ifstream f(file, std::ios::binary);
        if (!f) return false;
        unsigned char sig[12];
        f.read(reinterpret_cast<char*>(sig), 12);
        return f.gcount() >= 12 && memcmp(sig, "RIFF", 4) == 0 && memcmp(&sig[8], "AVI ", 4) == 0;
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

/// @brief Test compressing an MP4 file and verify output is valid MP4
TEST_F(VideoCompressionTest, CompressMP4) {
    auto input = find_test_file({ ".mp4" });
    if (input.empty()) {
        GTEST_SKIP() << "No MP4 found in " << test_data_dir;
    }

    media_handler::utils::Config config;
    config.output_dir = test_dir.string();
    config.crf = 23;
    config.video_preset = "medium";
    auto logger = media_handler::utils::Logger::create("VideoCompressionTest");

    media_handler::compressor::VideoProcessor processor(config, logger);
    fs::path output = test_dir / input.filename();

    auto result = processor.compress(input, output);

    ASSERT_TRUE(result.success) << "Failed: " << result.message;
    ASSERT_TRUE(fs::exists(output));
    EXPECT_TRUE(verify_mp4_signature(output));
    verify_size(input, output);
}

/// @brief Test compressing an AVI file
TEST_F(VideoCompressionTest, CompressAVI) {
    auto input = find_test_file({ ".avi" });
    if (input.empty()) {
        GTEST_SKIP() << "No AVI found in " << test_data_dir;
    }

    media_handler::utils::Config config;
    config.output_dir = test_dir.string();
    config.crf = 23;
    auto logger = media_handler::utils::Logger::create("VideoCompressionTest");

    media_handler::compressor::VideoProcessor processor(config, logger);
    fs::path output = test_dir / input.filename();

    auto result = processor.compress(input, output);

    ASSERT_TRUE(result.success) << "Failed: " << result.message;
    ASSERT_TRUE(fs::exists(output));
    verify_size(input, output);
}

/// @brief Test compressing an MOV file
TEST_F(VideoCompressionTest, CompressMOV) {
    auto input = find_test_file({ ".mov" });
    if (input.empty()) {
        GTEST_SKIP() << "No MOV found in " << test_data_dir;
    }

    media_handler::utils::Config config;
    config.output_dir = test_dir.string();
    config.crf = 23;
    auto logger = media_handler::utils::Logger::create("VideoCompressionTest");

    media_handler::compressor::VideoProcessor processor(config, logger);
    fs::path output = test_dir / input.filename();

    auto result = processor.compress(input, output);

    ASSERT_TRUE(result.success) << "Failed: " << result.message;
    ASSERT_TRUE(fs::exists(output));
    verify_size(input, output);
}

/// @brief Test handling of non-existent input file
TEST_F(VideoCompressionTest, HandleNonExistentFile) {
    media_handler::utils::Config config;
    config.output_dir = test_dir.string();
    auto logger = media_handler::utils::Logger::create("VideoCompressionTest");

    media_handler::compressor::VideoProcessor processor(config, logger);

    auto result = processor.compress("nonexistent.mp4", test_dir / "out.mp4");

    EXPECT_FALSE(result.success);
}

/// @brief Test CRF quality setting
TEST_F(VideoCompressionTest, CRFQualitySetting) {
    auto input = find_test_file({ ".mp4", ".mov", ".avi" });
    if (input.empty()) {
        GTEST_SKIP() << "No video file found in " << test_data_dir;
    }

    media_handler::utils::Config config_high;
    config_high.output_dir = test_dir.string();
    config_high.crf = 18;  // Higher quality
    auto logger = media_handler::utils::Logger::create("VideoCompressionTest");

    media_handler::compressor::VideoProcessor processor(config_high, logger);
    fs::path output_high = test_dir / "high_quality.mp4";

    auto result = processor.compress(input, output_high);

    ASSERT_TRUE(result.success) << "Failed: " << result.message;
    ASSERT_TRUE(fs::exists(output_high));
    EXPECT_GT(fs::file_size(output_high), 0);
}

/// @brief Test that file extensions are handled
TEST_F(VideoCompressionTest, PreservesFilename) {
    auto input = find_test_file({ ".mp4", ".mov", ".avi" });
    if (input.empty()) {
        GTEST_SKIP() << "No video file found in " << test_data_dir;
    }

    media_handler::utils::Config config;
    config.output_dir = test_dir.string();
    auto logger = media_handler::utils::Logger::create("VideoCompressionTest");

    media_handler::compressor::VideoProcessor processor(config, logger);
    fs::path output = test_dir / input.filename();

    auto result = processor.compress(input, output);

    ASSERT_TRUE(result.success);
    EXPECT_EQ(output.filename(), input.filename());
}