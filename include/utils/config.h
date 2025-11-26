#pragma once
#include <string>
#include <cstdint>
#include <expected>
#include <spdlog/spdlog.h>

#define VALID_CONFIG_FILE "config.json"

namespace media_handler::utils {

	/// @brief App configuration with default values
	struct Config {
        // Video
        std::string video_codec = "libx264";
        std::string video_preset = "medium";
        std::string crf = "23";
        std::string maxrate = "";
        std::string bufsize = "";

        // Audio
        std::string audio_codec = "aac";
        std::string audio_bitrate = "192k";

        // General
        std::string container = "mp4";
        std::string output_dir = "output";
        uint32_t    threads = 4;
        bool        json_log = false;
        spdlog::level::level_enum log_level = spdlog::level::info;

        /// @brief Load configuration from JSON file
		/// @param path Path to config.json
		/// @return Config on success, error message on failure
        static std::expected<Config, std::string> load(const std::string& path);

        /// @brief Savve current configuration to config.json
		/// @param path path to config.json
        void save(const std::string& path) const;
	};
}// namespace media_handler