#include "utils/app_args.h"
#include <CLI/CLI.hpp>

namespace media_handler::utils {

    AppArgs parse_command_line(int argc, char** argv, std::shared_ptr<spdlog::logger>& logger) {
        AppArgs args;

        // Load config.json first; result overrides defaults
        if (auto result = Config::load(CONFIG_FILE, logger)) {
            args.cfg = std::move(*result);
        }
        else {
            logger->warn("Config load error: {} — using defaults", result.error());
        }

        CLI::App app{ "Media Handler — High-performance media converter" };

		// Direct binding to args and args.cfg. Overwritten only if input flag is present
        app.add_option("-i,--input", args.inputs, "Input file(s)/directory");
        app.add_option("-o,--output", args.cfg.output_dir, "Output directory");
        app.add_option("-t,--threads", args.cfg.threads, "Threads");
        app.add_option("--crf", args.cfg.crf, "CRF quality");
        app.add_option("--preset", args.cfg.video_preset, "Preset");
        app.add_flag("-j,--json", args.cfg.json_log, "JSON logging");
        app.add_flag("-r,--retry", args.retry_failed, "Retry failed");
        app.add_flag("--organize", args.organize_by_date, "Organize by date");

        // Handle log level with a temporary string for validation
        std::string log_level_str;
        app.add_option("-l,--log-level", log_level_str, "Log level")->check(CLI::IsMember({ "trace", "debug", "info", "warn", "error", "critical" }));

        try {
            app.parse(argc, argv);

            // Update log level only if provided
            if (!log_level_str.empty()) {
                args.cfg.log_level = spdlog::level::from_str(log_level_str);
            }
        }
        catch (const CLI::ParseError& e) {
            std::exit(app.exit(e));
        }

        return args;
    }

} // namespace media_handler::utils