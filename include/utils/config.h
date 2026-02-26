#pragma once
#include <string>
#include <filesystem>
#include <expected>
#include <memory>
#include <spdlog/spdlog.h>

#define CONFIG_FILE "config.json"

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
        std::string input_dir = "input";
        std::string output_dir = "output";
        uint32_t threads = 4;
        bool json_log = false;
        spdlog::level::level_enum log_level = spdlog::level::info;

        /// @brief Load configuration from JSON file
        static std::expected<Config, std::string> load(const std::filesystem::path& path, const std::shared_ptr<spdlog::logger>& logger);

		/// @brief Performs validation and returns either success or an error message. 
		std::expected<void, std::string> validate() const;
	};
}// namespace media_handler