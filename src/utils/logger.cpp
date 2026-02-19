#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __linux__
#include <unistd.h>
#include <limits.h>
#endif

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/async.h>
#include <filesystem>
#include "utils/utils.h"

namespace fs = std::filesystem;

namespace media_handler::utils {

	// PathUtils implementation

	fs::path PathUtils::get_executable_dir() {
#ifdef _WIN32
		wchar_t path[MAX_PATH];
		DWORD length = GetModuleFileNameW(NULL, path, MAX_PATH);
		if (length == 0) {
			throw std::runtime_error("Failed to get executable path on Windows");
		}
		return fs::path(path).parent_path();
#else
		char path[PATH_MAX];
		ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
		if (count == -1) {
			throw std::runtime_error("Failed to get executable path on Linux");
		}
		return fs::path(std::string(path, count)).parent_path();
#endif
	}

	fs::path PathUtils::find_config_file(const std::string& filename) {
		try {
			fs::path exe_dir = get_executable_dir();

			// Search order:
			std::vector<fs::path> search_paths = {
				exe_dir / filename,                     // 1. Next to executable (primary)
				fs::current_path() / filename,          // 2. Current working directory
				exe_dir.parent_path() / filename,       // 3. One level up (for Debug/Release)
			};

			for (const auto& path : search_paths) {
				if (fs::exists(path)) {
					return path;
				}
			}
		}
		catch (const std::exception& e) {
			// Return empty
		}

		return fs::path(); // Not found
	}


	// Logger implementation

	auto Logger::create(const std::string& name, spdlog::level::level_enum level, bool json_format) -> std::shared_ptr<spdlog::logger> {
		std::filesystem::create_directories(log_dir);

		//spdlog::init_thread_pool(8192, 1); //Queue size, thread count
		static std::once_flag flag;
		std::call_once(flag, [] {
			spdlog::init_thread_pool(8192, 1); // queue size, thread count
			});

		// Reuse existing logger if present
		if (auto existing = spdlog::get(name)) {
			existing->set_level(level);
			existing->flush_on(spdlog::level::err);
			return existing;
		}

		auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		std::string file_name = json_format ? log_dir + "/media_handler.json" : log_dir + "/media_handler.log";
		auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(file_name, true);

		std::string pattern = json_format ?
			R"({"timestamp": "%Y-%m-%dT%H:%M:%S.%eZ", "level": "%l", "thread": "%t", "file": "%s:%#", "message": "%v"})"
			: "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] [%s:%#] %v";

		console_sink->set_pattern(pattern);
		file_sink->set_pattern(pattern);

		auto logger = std::make_shared<spdlog::async_logger>(name, spdlog::sinks_init_list{console_sink, file_sink}, spdlog::thread_pool(), spdlog::async_overflow_policy::block);
		spdlog::register_logger(logger);

		logger->set_level(level);
		logger->flush_on(spdlog::level::err);

		return logger;
	}

	auto media_handler::utils::Logger::create_json(const std::string& name, spdlog::level::level_enum level) -> std::shared_ptr<spdlog::logger>
	{
		return create(name, level, true);
	}

	void media_handler::utils::Logger::flush_all()
	{
		spdlog::apply_all([](std::shared_ptr<spdlog::logger> l) { l->flush(); });
	}

} // namespace media_handler::utils