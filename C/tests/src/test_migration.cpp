#include "media_migration.h"
#include "test_utilities.h"

class MigrationTest : public ::testing::Test {
protected:
    void SetUp() override { 
        ASSERT_TRUE(ensure_target_dir()); 
        init_logger(LOG_FILE, "w");
    }
    void TearDown() override { 
        close_logger(); 
        remove(LOG_FILE); 
        cleanup_target_dir(); 
    }
};

/// @brief Verifies that the migration successfully processes all source files
/// @param SOURCE_DIR Source directory
/// @param TARGET_DIR Target directory
TEST_F(MigrationTest, FullMigration) {
    int processed = 0, failed = 0;
    run_media_migration(SOURCE_DIR, SOURCE_DIR, TARGET_DIR, &processed, &failed);

    ASSERT_GT(processed, 0);
    ASSERT_EQ(failed, 0);
    ASSERT_EQ(count_valid_outputs(), processed);
    log_test_outcome("FullMigration", true);
}