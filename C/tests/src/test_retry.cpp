#include "test_utilities.h"

class RetryTest : public ::testing::Test {
protected:
    void SetUp() override { 
        ASSERT_TRUE(ensure_target_dir()); 
        init_logger(LOG_FILE); 
    }
    void TearDown() override { 
        close_logger(); 
        remove(LOG_FILE); 
        cleanup_target_dir(); 
    }
};

/// @brief Verifies retry logic for failes marked as failed in LOG filelog.
/// @param SOURCE_DIR Source directory
/// @param TARGET_DIR Target directory
TEST_F(RetryTest, RetriesAllFailedFiles) {
    auto files = discover_source_files();
    ASSERT_FALSE(files.empty()) << "No files found in source directory";

    // Log all files as FAILED
    for (const auto& f : files)
        create_mock_log(f.c_str());

    // Load failed entries
    int num = 0;
    char** failed = get_failed_files_from_log(LOG_FILE, &num);
    ASSERT_EQ(num, files.size());

    // Retry
    int processed = 0, failed_count = 0;
    retry_failed_files(SOURCE_DIR, TARGET_DIR, failed, num, &processed, &failed_count);

    ASSERT_EQ(processed, num);
    ASSERT_EQ(failed_count, 0);

    for (int i = 0; i < num; ++i) free(failed[i]);
    free(failed);

    log_test_outcome("RetryAllFiles", true);
}
