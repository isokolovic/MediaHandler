#pragma once
#include <spdlog/spdlog.h>
#include <memory>
#include <string>

namespace media_handler::utils {

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

}