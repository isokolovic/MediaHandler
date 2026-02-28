#include <gtest/gtest.h>
#include "utils/retry_log.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace media_handler::utils;

class RetryLogTest : public ::testing::Test {
protected:
    fs::path dir;
    std::shared_ptr<spdlog::logger> logger;

    void SetUp() override {
        dir = fs::temp_directory_path() / "retry_log_test";
        fs::create_directories(dir);
        logger = spdlog::default_logger();
    }

    void TearDown() override {
        fs::remove_all(dir);
    }

    RetryLog make_log() { return RetryLog(dir, logger); }

    const fs::path file_a = fs::temp_directory_path() / "a.jpg";
    const fs::path file_b = fs::temp_directory_path() / "b.mp4";
    const fs::path file_c = fs::temp_directory_path() / "c.png";
};

/// @brief Verify that a new RetryLog starts with empty completed and failed sets, and counts are zero.
TEST_F(RetryLogTest, FreshLogIsEmpty) {
    auto log = make_log();
    EXPECT_EQ(log.completed_count(), 0u);
    EXPECT_EQ(log.failed_count(), 0u);
}

/// @brief Verify that mark_completed() correctly updates state and counts, and that is_completed() and is_failed() return expected values
TEST_F(RetryLogTest, MarkCompleted) {
    auto log = make_log();
    log.mark_completed(file_a);

    EXPECT_TRUE(log.is_completed(file_a));
    EXPECT_FALSE(log.is_failed(file_a));
    EXPECT_EQ(log.completed_count(), 1u);
    EXPECT_EQ(log.failed_count(), 0u);
}

/// @brief Verify that mark_failed() correctly updates state and counts, and that is_failed() and is_completed() return expected values
TEST_F(RetryLogTest, MarkFailed) {
    auto log = make_log();
    log.mark_failed(file_a);

    EXPECT_TRUE(log.is_failed(file_a));
    EXPECT_FALSE(log.is_completed(file_a));
    EXPECT_EQ(log.failed_count(), 1u);
    EXPECT_EQ(log.completed_count(), 0u);
}

/// @brief Verify that marking a file as completed after it was previously marked as failed clears the failure state and updates counts accordingly.
TEST_F(RetryLogTest, MarkCompleted_ClearsPriorFailure) {
    auto log = make_log();
    log.mark_failed(file_a);
    ASSERT_TRUE(log.is_failed(file_a));

    log.mark_completed(file_a);
    EXPECT_TRUE(log.is_completed(file_a));
    EXPECT_FALSE(log.is_failed(file_a));
    EXPECT_EQ(log.failed_count(), 0u);
}

/// @brief Verify that marking a file as failed after it was previously marked as completed clears the completed state and updates counts accordingly.
TEST_F(RetryLogTest, MultipleFiles) {
    auto log = make_log();
    log.mark_completed(file_a);
    log.mark_failed(file_b);

    EXPECT_TRUE(log.is_completed(file_a));
    EXPECT_TRUE(log.is_failed(file_b));
    EXPECT_FALSE(log.is_completed(file_b));
    EXPECT_FALSE(log.is_failed(file_a));
    EXPECT_FALSE(log.is_completed(file_c));
    EXPECT_FALSE(log.is_failed(file_c));
}

/// @brief Verify that querying is_completed() and is_failed() for a file that has not been marked returns false, and does not throw 
TEST_F(RetryLogTest, UnknownFile_ReturnsFalse) {
    auto log = make_log();

    EXPECT_FALSE(log.is_completed(file_a));
    EXPECT_FALSE(log.is_failed(file_a));
}

/// @brief Verify that after saving state with save(), loading with load() preserves the completed and failed sets and counts correctly.
TEST_F(RetryLogTest, SaveAndLoad_PreservesState) {
    {
        auto log = make_log();
        log.mark_completed(file_a);
        log.mark_failed(file_b);
        log.save();
    }

    auto log2 = make_log();
    log2.load();

    EXPECT_TRUE(log2.is_completed(file_a));
    EXPECT_FALSE(log2.is_failed(file_a));
    EXPECT_TRUE(log2.is_failed(file_b));
    EXPECT_FALSE(log2.is_completed(file_b));
    EXPECT_EQ(log2.completed_count(), 1u);
    EXPECT_EQ(log2.failed_count(), 1u);
}

/// @brief Verify that if the state file is missing, load() does not throw and starts with empty state and zero counts.
TEST_F(RetryLogTest, LoadMissingStateFile_DoesNotThrow) {
    auto log = make_log();

    EXPECT_NO_THROW(log.load());
    EXPECT_EQ(log.completed_count(), 0u);
    EXPECT_EQ(log.failed_count(), 0u);
}

/// @brief Verify that if the state file is present but contains invalid JSON, load() does not throw and starts with empty state and zero counts.
TEST_F(RetryLogTest, LoadCorruptStateFile_StartsClean) {
    std::ofstream(dir / ".mediahandler_state") << "{ not valid json !!!";
    auto log = make_log();

    EXPECT_NO_THROW(log.load());
    EXPECT_EQ(log.completed_count(), 0u);
    EXPECT_EQ(log.failed_count(), 0u);
}

/// @brief Verify that after marking a file as completed and calling save(), the state file is created on disk with the expected name.
TEST_F(RetryLogTest, Save_CreatesStateFile) {
    auto log = make_log();
    log.mark_completed(file_a);
    log.save();

    EXPECT_TRUE(fs::exists(dir / ".mediahandler_state"));
}

/// @brief Verify that after marking a file as completed and calling save(), the temporary state file used during saving is removed and does not remain on disk.
TEST_F(RetryLogTest, Save_DoesNotLeaveTmpFile) {
    auto log = make_log();
    log.mark_completed(file_a);
    log.save();

    EXPECT_FALSE(fs::exists(dir / ".mediahandler_state.tmp"));
}

/// @brief Verify that simulating a failed file in one run and then marking it as completed in a subsequent run correctly updates the state and counts, and that loading the state in the final run reflects the successful completion without failure.
TEST_F(RetryLogTest, RetryRoundtrip_FailThenSucceed) {
    // Simulate two runs: first run fails a file, second run succeeds.
    {
        auto log = make_log();
        log.mark_failed(file_a);
        log.save();
    }
    {
        auto log = make_log();
        log.load();
        ASSERT_TRUE(log.is_failed(file_a));
        log.mark_completed(file_a);
        log.save();
    }
    {
        auto log = make_log();
        log.load();

        EXPECT_TRUE(log.is_completed(file_a));
        EXPECT_FALSE(log.is_failed(file_a));
        EXPECT_EQ(log.failed_count(), 0u);
    }
}

/// @brief Verify that two files with the same name but in different directories are treated as distinct entries in the log, and that marking one as completed does not affect the state of the other. 
TEST_F(RetryLogTest, SameFilenameInDifferentDirs_AreDistinct) {
    fs::path a1 = fs::temp_directory_path() / "2019" / "IMG_001.jpg";
    fs::path a2 = fs::temp_directory_path() / "2020" / "IMG_001.jpg";

    auto log = make_log();
    log.mark_completed(a1);

    EXPECT_TRUE(log.is_completed(a1));
    EXPECT_FALSE(log.is_completed(a2));
}

/// @brief Verify that after marking multiple files as failed, the failed count reflects the total number of unique failed files, and that each file is correctly reported as failed.
TEST_F(RetryLogTest, MarkFailed_MultipleFiles_CountIsCorrect) {
    auto log = make_log();
    log.mark_failed(file_a);
    log.mark_failed(file_b);
    log.mark_failed(file_c);

    EXPECT_EQ(log.failed_count(), 3u);
}

/// @brief Verify that marking the same file as failed multiple times does not increase the failed count beyond one, and that the file is still correctly reported as failed.
TEST_F(RetryLogTest, MarkFailed_SameFileTwice_CountStaysOne) {
    auto log = make_log();
    log.mark_failed(file_a);
    log.mark_failed(file_a);

    EXPECT_EQ(log.failed_count(), 1u);
}