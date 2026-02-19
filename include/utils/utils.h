#pragma once
#include <spdlog/spdlog.h>
#include <memory>
#include <string>
#include "config.h"

namespace media_handler::utils {
	inline std::string log_dir = "logs";

	/// @brief Utility functions for path handling and config file discovery
    class PathUtils {
    public:
        /// @brief Get the directory where the executable is located
        /// @return Absolute path to executable directory
        static std::filesystem::path get_executable_dir();

        /// @brief Find config.json in standard locations
        /// @param filename Config filename (default: "config.json")
        /// @return Path to config file if found, empty path otherwise
        static std::filesystem::path find_config_file(const std::string& filename = CONFIG_FILE);
    };

	/// @brief Wrapper for spdlog logger creation
    class Logger {
    public:        
        /// @brief Creates async logger txt format
        /// @param name Logger name
		/// @param level Log level
		/// @param json_format If true, use JSON format
		/// @return Loges to logs/media_handler.log and console
        static auto create(const std::string& name, spdlog::level::level_enum level = spdlog::level::info, bool json_format = false) -> std::shared_ptr<spdlog::logger>;

		/// @brief Creates async logger json format
		/// @param name 
		/// @param level 
		/// @return 
		static auto create_json(const std::string& name, spdlog::level::level_enum level = spdlog::level::info) -> std::shared_ptr<spdlog::logger>;

		/// @brief Flush all loggers
		static void flush_all();
    };
} //namespace media_handler::utils