#include "test_common.h"
#include "media_handler/compression_engine.h"
#include "utils/config.h"
#include "utils/utils.h"
#include <fstream>

namespace media_handler::tests {
    class CompressionEngineTest : public TestCommon {};

	/// @brief Test that scan_media_files finds supported files
    TEST_F(CompressionEngineTest, ScanFindsFiles) {
        utils::Config cfg;
        cfg.threads = 4;

        media_handler::CompressionEngine engine(cfg);

        // Create test files in per-test temp dir
        std::ofstream(path("video.mp4")) << "fake";
        std::ofstream(path("image.jpg")) << "fake";
        std::ofstream(path("doc.txt")) << "fake";

        auto files = engine.scan_media_files(test_dir);  // scan our temp dir

        EXPECT_EQ(files.size(), 2);
    }

    /// @brief Test that migrate processes files in parallel without crashing
    TEST_F(CompressionEngineTest, MigrateRunsInParallel) {
        utils::Config cfg;
        cfg.threads = 4;
		cfg.json_log = true;  // simpler logging for test
		cfg.log_level = spdlog::level::debug;

        media_handler::CompressionEngine engine(cfg);

        std::vector<std::filesystem::path> files;
        for (int i = 0; i < 20; ++i) {
            std::filesystem::path p = path("file" + std::to_string(i) + ".mp4");
            std::ofstream(p) << "fake";
            files.push_back(p);
        }

        engine.migrate(files);

        SUCCEED();  // no crash = success
    }
} // namespace mediahandler::tests