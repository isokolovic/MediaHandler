#pragma once
#include <filesystem>
#include <string>
#include <unordered_set>
#include <memory>
#include <spdlog/spdlog.h>

namespace media_handler::utils {

    class RetryLog {
    public:
        
        explicit RetryLog(const std::filesystem::path& output_dir, std::shared_ptr<spdlog::logger> logger);

        /// @brief Load existing state from disk. Call before migrate().
        void load();

        /// @brief Save file processing state
        void save() const;

        /// @brief True if file succeeded in a prior run.
        bool is_completed(const std::filesystem::path& file) const;

        /// @brief True if file failed in a prior run.
        bool is_failed(const std::filesystem::path& file) const;

        /// @brief The failed file set (used for retry mode filtering).
        const std::unordered_set<std::string>& failedFiles() const { return failed; }

        /// @brief Mark file as successfully completed; clears any prior failure.
        void mark_completed(const std::filesystem::path& file);

        /// @brief Mark file as failed.
        void mark_failed(const std::filesystem::path& file);

        std::size_t completed_count() const { return completed.size(); }
        std::size_t failed_count() const { return failed.size(); }

    private:
        std::filesystem::path state_file;
        std::shared_ptr<spdlog::logger> logger;
        std::unordered_set<std::string> completed;
        std::unordered_set<std::string> failed;

        /// @brief Canonical absolute path used as stable lookup key.
        static std::string normalize(const std::filesystem::path& p);
    };

} // namespace media_handler::utils