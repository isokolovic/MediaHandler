#include <gtest/gtest.h>
#include "utils/organizer.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace media_handler::utils;

class OrganizerTest : public ::testing::Test {
protected:
    void SetUp() override {
        src_dir = fs::temp_directory_path() / "org_src";
        out_dir = fs::temp_directory_path() / "org_out";
        fs::create_directories(src_dir);
        fs::create_directories(out_dir);
        organizer = std::make_unique<Organizer>(spdlog::default_logger());
    }

    void TearDown() override {
        fs::remove_all(src_dir);
        fs::remove_all(out_dir);
    }

    fs::path make_file(const std::string& name) {
        auto p = src_dir / name;
        std::ofstream(p) << "data";
        return p;
    }

    fs::path src_dir, out_dir;
    std::unique_ptr<Organizer> organizer;
};

/// @brief Verify file is moved into out_dir/YYYY/filename and source is removed.
TEST_F(OrganizerTest, FileMovedToYearSubfolder) {
    auto file = make_file("video.mp4");
    auto result = organizer->organize(file, out_dir);

    ASSERT_TRUE(result.success) << result.error;
    EXPECT_TRUE(fs::exists(result.destination));
    EXPECT_FALSE(fs::exists(file));
    EXPECT_EQ(result.destination.parent_path().parent_path(), out_dir);
    EXPECT_EQ(result.destination.filename(), "video.mp4");
}

/// @brief Verify output directory and year subfolder are created automatically if absent.
TEST_F(OrganizerTest, OutputDirCreatedIfAbsent) {
    auto result = organizer->organize(make_file("clip.mp4"), out_dir / "new");
    ASSERT_TRUE(result.success) << result.error;
    EXPECT_TRUE(fs::exists(result.destination));
}

/// @brief Verify colliding filenames are both kept — second file gets a suffix, nothing is lost.
TEST_F(OrganizerTest, NameCollision_BothFilesKept) {
    auto r1 = organizer->organize(make_file("IMG_001.mp4"), out_dir);
    auto r2 = organizer->organize(make_file("IMG_001.mp4"), out_dir);

    ASSERT_TRUE(r1.success);
    ASSERT_TRUE(r2.success);
    EXPECT_TRUE(fs::exists(r1.destination));
    EXPECT_TRUE(fs::exists(r2.destination));
    EXPECT_NE(r1.destination, r2.destination);
}

/// @brief Verify organizing a non-existent file returns failure with a non-empty error message.
TEST_F(OrganizerTest, MissingFile_ReturnsError) {
    auto result = organizer->organize(src_dir / "ghost.mp4", out_dir);
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
}