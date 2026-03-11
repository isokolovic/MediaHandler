#pragma once
#include "utils/config.h"
#include "utils/utils.h"
#include <filesystem>
#include <vector>
#include <array>
#include <string>
#include <memory>
#include <spdlog/spdlog.h>

namespace media_handler::compressor {

    /// @brief Options parsed from CLI flags --retry and --organize.
    struct MigrateOptions {
        bool retry = false;
        bool organize = false;
    };

    class CompressionEngine {
    public:
        explicit CompressionEngine(const utils::Config& cfg); // No implicit conversions

        /// @brief Scan directory for supported media files
        std::vector<std::filesystem::path> scan_media_files(const std::filesystem::path& input_dir) const;

        /// @brief Migrate media files using a pool of worker threads
        void migrate(const std::vector<std::filesystem::path>& files, const MigrateOptions& opts = {});

    private:
        utils::Config config;
        std::shared_ptr<spdlog::logger> logger;

        // Supported types

        // Will be compressed: 
        static constexpr std::array<const char*, 4> video_exts = { ".mp4", ".avi", ".mov", ".mkv" };
        static constexpr std::array<const char*, 5> image_exts = { ".jpg", ".jpeg", ".png", ".heic", ".heif" };

        // Will be copied raw: 
        static constexpr std::array<const char*, 4> other_exts = { ".mp3", ".aac", ".wav", ".flac" };

        /// @brief Determines whether the given filesystem path is supported by this object.
        bool is_supported(const std::filesystem::path& p) const;

        /// @brief Scan over extensions array
        static bool ext_matches(const auto& arr, std::string_view ext) {
            return std::find(arr.begin(), arr.end(), ext) != arr.end();
        }
    };

} // namespace media_handler