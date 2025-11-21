#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/async.h>
#include <filesystem>
#include "utils/utils.h"

namespace media_handler::utils {

	auto Logger::create(const std::string& name, spdlog::level::level_enum level, bool json_format) -> std::shared_ptr<spdlog::logger> {
		std::filesystem::create_directories(log_dir);

		//spdlog::init_thread_pool(8192, 1); //Queue size, thread count
		static std::once_flag flag;
		std::call_once(flag, [] {
			spdlog::init_thread_pool(8192, 1); // queue size, thread count
			});

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