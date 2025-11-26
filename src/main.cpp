#include <CLI/CLI.hpp>
#include "utils/config.h"
#include "utils/utils.h"
#include <iostream>

int main(int argc, char** argv) {
    CLI::App app{ "Media Handler" };

    media_handler::utils::Config cfg;

    // Load config.json
    auto result = media_handler::utils::Config::load(VALID_CONFIG_FILE);
    if (result) {
        cfg = *result;
    }
    else if (!result.error().empty()) {
        std::cerr << "Config warning: " << result.error() << " — using defaults\n";
    }

    // CLI overrides .json values
    app.add_option("--log-level", cfg.log_level, "Log level")
        ->check(CLI::IsMember({ "trace", "debug", "info", "warn", "error", "critical" }));
    app.add_flag("--json", cfg.json_log, "Use JSON logging");
    app.add_option("--threads", cfg.threads, "Number of threads");
    app.add_option("--crf", cfg.crf, "CRF quality");

    CLI11_PARSE(app, argc, argv);

    // Final logger
    auto logger = cfg.json_log
        ? media_handler::utils::Logger::create_json("MediaHandler", cfg.log_level)
        : media_handler::utils::Logger::create("MediaHandler", cfg.log_level);

    logger->info("Started with {} threads, CRF {}", cfg.threads, cfg.crf);
    logger->info("Done.");
    media_handler::utils::Logger::flush_all();

    return 0;
}