#pragma once
#include <chrono>
#include <atomic>
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <filesystem>
#include <spdlog/spdlog.h>

namespace media_handler::utils {

    /// @brief File metrics captured during processing.
    struct FileStats {
        std::string filename;
        std::uintmax_t size_in = 0; // Input bytes.
        std::uintmax_t size_out = 0; // Output bytes (0 if failed).
        std::chrono::milliseconds elapsed = {};
        bool success = false;
        bool skipped = false;
        std::string error;
    };

    /// @brief File run summary.
    class ProgressTracker {
    public:

        ProgressTracker(std::size_t total_files, std::shared_ptr<spdlog::logger> logger);

        /// @brief Register file as started; returns token for finishFile().
        std::size_t begin_file(const std::filesystem::path& file);

        /// @brief Record result and emit one-line log: size, %, MB/s, ms.
        void finish_file(std::size_t token, const std::filesystem::path& output,
            bool success, const std::string& error = {});

        /// @brief Record file as skipped (completed in prior run).
        void skip_file(const std::filesystem::path& file);

        /// @brief Print GB / % saved / elapsed summary. Call after all workers join.
        void print_summary() const;

    private:
        std::size_t total;
        std::shared_ptr<spdlog::logger> logger;

        mutable std::mutex mutex;
        std::vector<FileStats> stats;
        std::vector<std::chrono::steady_clock::time_point> start_times;

		// std::atomic counters for summary stats - updated by workers without locking entire struct.
        std::atomic<std::size_t> completed{ 0 };
        std::atomic<std::size_t> failed{ 0 };
        std::atomic<std::size_t> skipped{ 0 };

        std::chrono::steady_clock::time_point run_start;
    };

} // namespace media_handler::utils