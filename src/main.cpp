#include "utils/app_args.h"
#include "utils/utils.h"
#include "compressor/compression_engine.h"
#include <iostream>

int main(int argc, char** argv) {
	// Create logger for early errors and parse CLI + config.json
    auto logger = media_handler::utils::Logger::create("MediaHandler");
    auto args = media_handler::utils::parse_command_line(argc, argv, logger);

    if (args.show_help) {
        return 0;
    }

    // Re-create logger with final settings
    logger = args.cfg.json_log
        ? media_handler::utils::Logger::create_json("MediaHandler", args.cfg.log_level)
        : media_handler::utils::Logger::create("MediaHandler", args.cfg.log_level);

    logger->info("MediaHandler started");
    logger->info("Input files: {}", args.inputs.size());
    logger->info("Output dir: {}", args.cfg.output_dir);
    logger->info("Threads: {}, CRF: {}", args.cfg.threads, args.cfg.crf);

	auto files = media_handler::compressor::CompressionEngine(args.cfg).scan_media_files(args.cfg.input_dir); //TODO args.inputs[0] is empty! have one from 1. input args 2. config.json 3. defaults. Throw warning if location cannot be found (e.g. default)

	media_handler::compressor::CompressionEngine engine(args.cfg);
	engine.migrate(files);


    logger->info("All done!");
    media_handler::utils::Logger::flush_all();

    return 0;
}