#pragma once
#include "utils/config.h"
#include <filesystem>
#include <vector>
#include <array>
#include <string>

namespace media_handler::compressor {

    class CompressionEngine {
    public:
		explicit CompressionEngine(const utils::Config& cfg); // No implicit conversions

        /// @brief Scan directory for supported media files
        std::vector<std::filesystem::path> scan_media_files(const std::filesystem::path& input_dir) const;

        /// @brief Migrate media files using a pool of worker threads
        void migrate(const std::vector<std::filesystem::path>& files);

    private:
        utils::Config config;
        std::shared_ptr<spdlog::logger> logger;

        // Supported types

        // Will be compressed: 
        static constexpr std::array<const char*, 4> video_exts = {
            ".mp4", ".avi", ".mov", ".mkv"
        };
        static constexpr std::array<const char*, 5> image_exts = {
            ".jpg", ".jpeg", ".png", ".heic", ".heif"
        };

        // Will be copied raw: 
        static constexpr std::array<const char*, 4> audio_exts = {
            ".mp3", ".aac", ".wav", ".flac"
        };

        bool is_supported(const std::filesystem::path& p) const;
    };

} // namespace media_handler