#pragma once
#include <spdlog/spdlog.h>
#include <memory>
#include <fstream>
#include <string>
#include "config.h"

namespace media_handler::utils {
	inline std::string log_dir = "logs";

    /// @brief Returns the path as a UTF-8 std::string on every platform
    inline std::string path_to_utf8(const std::filesystem::path& p) {
#ifdef _WIN32
        auto u8 = p.u8string(); // std::u8string in C++20
        return { reinterpret_cast<const char*>(u8.data()), u8.size() };
#else
        return p.string();
#endif
	}

	/// @brief Cross-platform fopen that handles UTF-8 paths correctly
    inline FILE* fopen_path(const std::filesystem::path& p, const char* mode) {
#ifdef _WIN32
        std::wstring wmode(mode, mode + std::strlen(mode));
        return _wfopen(p.wstring().c_str(), wmode.c_str());
#else
        return std::fopen(p.string().c_str(), mode);
#endif
    }

	/// @brief Used to read a file into a byte vector, handling UTF-8 paths correctly
    inline std::vector<unsigned char> read_file_bytes(const std::filesystem::path& p) {
        std::ifstream f(p, std::ios::binary);
        if (!f) return {};
        return { std::istreambuf_iterator<char>(f),
                 std::istreambuf_iterator<char>() };
    }

	/// @brief Utility functions for path handling and config file discovery
    class PathUtils {
    public:
        /// @brief Get the directory where the executable is located
        static std::filesystem::path get_executable_dir();

        /// @brief Find config.json in standard locations
        static std::filesystem::path find_config_file(const std::string& filename = CONFIG_FILE);
    };

	/// @brief Wrapper for spdlog logger creation
    class Logger {
    public:        
        /// @brief Creates async logger txt format
        static auto create(const std::string& name, spdlog::level::level_enum level = spdlog::level::info, bool json_format = false) -> std::shared_ptr<spdlog::logger>;

		/// @brief Creates async logger json format
		static auto create_json(const std::string& name, spdlog::level::level_enum level = spdlog::level::info) -> std::shared_ptr<spdlog::logger>;

		/// @brief Flush all loggers
		static void flush_all();
    };
} //namespace media_handler::utils