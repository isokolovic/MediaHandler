#include "test_utilities.h"

class OrganizeTest : public ::testing::Test {
protected:
    void SetUp() override { 
        ASSERT_TRUE(ensure_target_dir()); 
        init_logger(LOG_FILE); 
    }
    void TearDown() override { 
        close_logger(); 
        remove(LOG_FILE); 
        cleanup_target_dir(); }
};

TEST_F(OrganizeTest, ZzzCreatesYearFolders) { //Zzz since it has to run last
    int processed = 0, failed = 0;
    organize_files(SOURCE_DIR, SOURCE_DIR, TARGET_DIR, &processed, &failed);

    ASSERT_GT(processed, 0);
    ASSERT_EQ(failed, 0);

    auto dirs = list_files_in_dir(TARGET_DIR);
    int years = 0;
    for (const auto& d : dirs) if (d.size() == 4 && isdigit(d[0])) years++;
    ASSERT_GT(years, 0);
    log_test_outcome("OrganizeByYear", true);
}