#include<utils/utils.h>
#include <CLI/CLI.hpp>
#include <thread>
#include <future>
#include <vector>
#include <iostream>
#include <chrono>

int main(int argc, char** argv){
	CLI::App app{ "Media Handler" };
	std::string log_level = "info";
	bool json_log = false;
	app.add_option("--log-level", log_level, "Log level")->default_val("info");
	app.add_flag("--json", json_log, "Use JSON");

	CLI11_PARSE(app, argc, argv);

	spdlog::level::level_enum lvl = spdlog::level::from_str(log_level);

	auto logger = json_log ? media_handler::utils::Logger::create("Test logger", lvl)
		: media_handler::utils::Logger::create_json("Test logger", lvl);

	// Simulate work
	std::this_thread::sleep_for(std::chrono::seconds(1));
	logger->debug("Debug: File scan complete");
	logger->warn("Warn: Low disk space");
	logger->error("Error: Failed file X.jpg");

	logger->info("App complete");
	media_handler::utils::Logger::flush_all();
	return 0;
}