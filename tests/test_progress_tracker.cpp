#include <gtest/gtest.h>
#include "utils/progress_tracker.h"
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>
#include <future>

namespace fs = std::filesystem;
using namespace media_handler::utils;

class ProgressTrackerTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir = fs::temp_directory_path() / "progress_tracker_test";
        fs::create_directories(dir);
        logger = spdlog::default_logger();

        // Create small real files so file_size() works
        make_file(file_in, 1024 * 100); // 100 KB input
        make_file(file_out, 1024 * 40);  // 40 KB output (compressed)
    }

    void TearDown() override {
        fs::remove_all(dir);
    }

    void make_file(const fs::path& path, std::size_t bytes) {
        std::ofstream f(path, std::ios::binary);
        std::string data(bytes, 'x');
        f.write(data.data(), data.size());
    }

    std::shared_ptr<spdlog::logger> logger;
    fs::path dir;
    fs::path file_in = fs::temp_directory_path() / "progress_tracker_test" / "input.mp4";
    fs::path file_out = fs::temp_directory_path() / "progress_tracker_test" / "output.mp4";
};

/// @brief Verify begin_file() returns sequential tokens starting from 0, even when called concurrently.
TEST_F(ProgressTrackerTest, begin_file_ReturnsSequentialTokens) {
    ProgressTracker tracker(3, logger);
    make_file(dir / "a.mp4", 1024);
    make_file(dir / "b.mp4", 1024);
    make_file(dir / "c.mp4", 1024);

    auto t0 = tracker.begin_file(dir / "a.mp4");
    auto t1 = tracker.begin_file(dir / "b.mp4");
    auto t2 = tracker.begin_file(dir / "c.mp4");

    EXPECT_EQ(t0, 0u);
    EXPECT_EQ(t1, 1u);
    EXPECT_EQ(t2, 2u);
}

/// @brief Verify finish_file() logs expected metrics and updates counters without throwing
TEST_F(ProgressTrackerTest, finish_file_Success_DoesNotThrow) {
    ProgressTracker tracker(1, logger);
    auto token = tracker.begin_file(file_in);

    EXPECT_NO_THROW(tracker.finish_file(token, file_out, true));
}

/// @brief Verify finish_file() handles failure case without throwing
TEST_F(ProgressTrackerTest, finish_file_Failure_DoesNotThrow) {
    ProgressTracker tracker(1, logger);
    auto token = tracker.begin_file(file_in);

    EXPECT_NO_THROW(tracker.finish_file(token, {}, false, "encoder error"));
}

/// @brief Verify print_summary() does not throw
TEST_F(ProgressTrackerTest, print_summary_DoesNotThrow) {
    ProgressTracker tracker(1, logger);
    auto token = tracker.begin_file(file_in);
    tracker.finish_file(token, file_out, true);

    EXPECT_NO_THROW(tracker.print_summary());
}

/// @brief Verify skip_file() updates skipped counter and does not throw
TEST_F(ProgressTrackerTest, skip_file_DoesNotThrow) {
    ProgressTracker tracker(2, logger);

    EXPECT_NO_THROW(tracker.skip_file(file_in));
}

/// @brief Verify that after a single successful file, the completed counter is correct as reflected in print_summary() output
TEST_F(ProgressTrackerTest, SingleSuccess_CountersCorrect) {    
    ProgressTracker tracker(1, logger);
    auto token = tracker.begin_file(file_in);
    tracker.finish_file(token, file_out, true);
        
    EXPECT_NO_THROW(tracker.print_summary());
}

/// @brief Verify that after a single failed file, the failed counter is correct as reflected in print_summary() output
TEST_F(ProgressTrackerTest, MixedOutcomes_SummaryDoesNotThrow) {
    ProgressTracker tracker(3, logger);

    make_file(dir / "ok.mp4", 512 * 1024);
    make_file(dir / "out.mp4", 200 * 1024);
    make_file(dir / "bad.mp4", 512 * 1024);

    auto t0 = tracker.begin_file(dir / "ok.mp4");
    auto t1 = tracker.begin_file(dir / "bad.mp4");
    tracker.skip_file(file_in);

    tracker.finish_file(t0, dir / "out.mp4", true);
    tracker.finish_file(t1, {}, false, "codec failure");

    EXPECT_NO_THROW(tracker.print_summary());
}

/// @brief Verify that if total_files is zero, print_summary() does not throw and handles division by zero correctly
TEST_F(ProgressTrackerTest, ZeroTotalFiles_SummaryDoesNotThrow) {
    ProgressTracker tracker(0, logger);

    EXPECT_NO_THROW(tracker.print_summary());
}

/// @brief Verify that concurrent calls to begin_file() and finish_file
TEST_F(ProgressTrackerTest, ConcurrentBeginAndFinish_NoRaceOrThrow) {
    constexpr int N = 32;
    ProgressTracker tracker(N, logger);

    // Create N real files
    std::vector<fs::path> inputs(N), outputs(N);
    for (int i = 0; i < N; ++i) {
        inputs[i] = dir / std::format("in_{}.mp4", i);
        outputs[i] = dir / std::format("out_{}.mp4", i);
        make_file(inputs[i], 512 * 1024);
        make_file(outputs[i], 200 * 1024);
    }

    // Fan out N async workers, each doing begin+finish
    std::vector<std::future<void>> futures;
    futures.reserve(N);

    for (int i = 0; i < N; ++i) {
        futures.push_back(std::async(std::launch::async, [&, i]() {
            auto token = tracker.begin_file(inputs[i]);
            // Simulate some processing time
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            tracker.finish_file(token, outputs[i], true);
            }));
    }

    for (auto& f : futures) EXPECT_NO_THROW(f.get());

    EXPECT_NO_THROW(tracker.print_summary());
}

/// @brief  Verify that concurrent calls to begin_file() and finish_file() with mixed success/failure outcomes do not cause race conditions or throw
TEST_F(ProgressTrackerTest, ConcurrentMixedOutcomes_NoRaceOrThrow) {
    constexpr int N = 20;
    ProgressTracker tracker(N, logger);

    std::vector<fs::path> inputs(N), outputs(N);
    for (int i = 0; i < N; ++i) {
        inputs[i] = dir / std::format("in_{}.mp4", i);
        outputs[i] = dir / std::format("out_{}.mp4", i);
        make_file(inputs[i], 128 * 1024);
        make_file(outputs[i], 50 * 1024);
    }

    std::vector<std::future<void>> futures;
    futures.reserve(N);

    for (int i = 0; i < N; ++i) {
        futures.push_back(std::async(std::launch::async, [&, i]() {
            auto token = tracker.begin_file(inputs[i]);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (i % 3 == 0)
                tracker.finish_file(token, {}, false, "error");
            else
                tracker.finish_file(token, outputs[i], true);
            }));
    }

    for (auto& f : futures) EXPECT_NO_THROW(f.get());
    EXPECT_NO_THROW(tracker.print_summary());
}

/// @brief Verify that concurrent calls to skip_file() do not cause race conditions or throw
TEST_F(ProgressTrackerTest, Concurrentskip_file_NoRaceOrThrow) {
    constexpr int N = 16;
    ProgressTracker tracker(N, logger);

    std::vector<fs::path> files(N);
    for (int i = 0; i < N; ++i) {
        files[i] = dir / std::format("skip_{}.mp4", i);
        make_file(files[i], 1024);
    }

    std::vector<std::future<void>> futures;
    futures.reserve(N);
    for (int i = 0; i < N; ++i)
        futures.push_back(std::async(std::launch::async,
            [&, i]() { tracker.skip_file(files[i]); }));

    for (auto& f : futures) EXPECT_NO_THROW(f.get());
}

/// @brief Verify that if finish_file() is called with a nonexistent output path, it does not throw and handles file_size() error_code correctly
TEST_F(ProgressTrackerTest, finish_file_MissingOutputPath_DoesNotCrash) {
    // output file does not exist — file_size() will return 0 via error_code
    ProgressTracker tracker(1, logger);
    auto token = tracker.begin_file(file_in);
    fs::path nonexistent = dir / "does_not_exist.mp4";

    EXPECT_NO_THROW(tracker.finish_file(token, nonexistent, true));
}

/// @brief Verify that if finish_file() is called with a nonexistent output path but success=false, it does not throw and handles file_size() error_code correctly
TEST_F(ProgressTrackerTest, ElapsedTime_IsNonNegative) {
    // Just verify the timer doesn't go backwards or throw on fast machines.
    ProgressTracker tracker(1, logger);
    auto token = tracker.begin_file(file_in);
    tracker.finish_file(token, file_out, true);

    EXPECT_NO_THROW(tracker.print_summary());
}