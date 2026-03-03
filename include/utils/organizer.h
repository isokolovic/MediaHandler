#pragma once
#include <filesystem>
#include <optional>
#include <chrono>
#include <vector>
#include <memory>
#include <spdlog/spdlog.h>

namespace media_handler::utils {

    class Organizer {
    public:
        struct Result {
            bool success = false;
            std::string error;
            std::filesystem::path destination;

            static Result OK(std::filesystem::path dst) { return { true,  {}, std::move(dst) }; }
            static Result Err(std::string msg) { return { false, std::move(msg), {} }; }
        };

        explicit Organizer(std::shared_ptr<spdlog::logger> logger);

        /// @brief Move file into output_root/<YYYY>/<filename> using earliest known date.
        Result organize(const std::filesystem::path& file, const std::filesystem::path& output_root) const;

    private:
        std::shared_ptr<spdlog::logger> logger;

        /// @brief Collect all available dates and return the earliest.
        std::optional<std::chrono::year_month_day> earliest_date(const std::filesystem::path& file) const;

        /// @brief Get exif DateTimeOriginal and DateTime
        void collect_exif_dates(const std::filesystem::path& file, std::vector<std::chrono::year_month_day>& out) const;

        /// @brief Get mtime and birth time (platform-specific)
        void collect_fs_dates(const std::filesystem::path& file, std::vector<std::chrono::year_month_day>& out) const;

        /// @brief Create YYYY/ dir, resolve collisions, move file.
        Result move_to_year_dir(const std::filesystem::path& file, const std::filesystem::path& output_root, std::chrono::year_month_day  date) const;
    };

} // namespace media_handler::utils