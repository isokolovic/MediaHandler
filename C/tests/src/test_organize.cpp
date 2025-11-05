#include <algorithm>

#include "test_utilities.h"

class OrganizeTest : public ::testing::Test {
protected:
    void SetUp() override { 
        ASSERT_TRUE(ensure_target_dir()); 
        init_logger(LOG_FILE, "r");
    }
    void TearDown() override { 
        close_logger(); 
        remove(LOG_FILE); 
        cleanup_target_dir(); }
};

/// @brief Verifies that organizing mode correctly creates year-based folders and places media files accordingly.
/// This test runs last as it modifies the source folder structure.
/// @param SOURCE_DIR Source directory.
/// @param TARGET_DIR Target directory.
TEST_F(OrganizeTest, ZzzCreatesYearFolders) {// Zzz since it has to run last (moves files from source folder)
    int processed = 0, failed = 0;
    organize_files(SOURCE_DIR, SOURCE_DIR, TARGET_DIR, &processed, &failed);

    ASSERT_GT(processed, 0);
    ASSERT_EQ(failed, 0);

    auto dirs = list_subdirs_in_dir(TARGET_DIR);
    int years = 0;
    for (const auto& d : dirs)
        // Count folder if its name is exactly 4 digits (e.g. "2023") (valid year folder)
        if (d.size() == 4 && std::all_of(d.begin(), d.end(), ::isdigit)) years++;
    ASSERT_GT(years, 0) << "No year folders created in target directory";

    auto files = list_files_in_dir(TARGET_DIR);
    int misplaced = 0;
    for (const auto& in : files) {
        const char* file = in.c_str();
        if (!is_image(file) && !is_video(file)) continue;

        if (!file_in_correct_year_folder(TARGET_DIR, file)) {
            log_message(LOG_ERROR, "File %s was not put to correct sub-folder", file);
            misplaced++;
        }
    }

    ASSERT_EQ(misplaced, 0) << "Some files were not placed in the correct year folder";
    log_test_outcome("OrganizeByYear", true);
}
