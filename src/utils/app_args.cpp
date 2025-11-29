#include "utils/app_args.h"
#include <CLI/CLI.hpp>
#include <iostream>

namespace media_handler::utils {

    AppArgs parse_command_line(int argc, char** argv,
        std::shared_ptr<spdlog::logger>& logger) {
        AppArgs args;

        // Load config.json first
        auto result = Config::load("config.json");
        if (result) {
            args.cfg = *result;
        }
        else if (!result.error().empty()) {
            logger->warn("Config error: {} — using defaults", result.error());
        }

        CLI::App app{ "Media Handler — High-performance media converter" };

        app.add_option("-i,--input", args.inputs, "Input file(s)/directory")->required();
        app.add_option("-o,--output", args.cfg.output_dir, "Output directory")->required();
        app.add_option("-t,--threads", args.cfg.threads, "Threads");
        app.add_option("--crf", args.cfg.crf, "CRF quality");
        app.add_option("--preset", args.cfg.video_preset, "Preset");
        app.add_flag("-j,--json", args.cfg.json_log, "JSON logging");
        app.add_option("-l,--log-level", args.cfg.log_level, "Log level")
            ->check(CLI::IsMember({ "trace","debug","info","warn","error","critical" }));
        app.add_flag("-r,--retry", args.retry_failed, "Retry failed");
        app.add_flag("--organize", args.organize_by_date, "Organize by date");

        try {
            app.parse(argc, argv);
        }
        catch (const CLI::ParseError& e) {
            logger->error("Invalid args");
            std::exit(app.exit(e));
        }

        return args;
    }

} // namespace media_handler::utils