#include "utils/app_args.h"
#include "utils/utils.h"
#include "compressor/compression_engine.h"
#include <iostream>

namespace fs = std::filesystem;
using namespace media_handler;

int main(int argc, char** argv) {
    try {   

    // Initialize logger
    auto logger = media_handler::utils::Logger::create("MediaHandler");
    logger->info("MediaHandler started");

    // Parse command line (handles config loading + CLI parsing)
    auto args = media_handler::utils::parse_command_line(argc, argv, logger);

    // Update logger level if changed
    logger->set_level(args.cfg.log_level);

    logger->info("Input files: {}", args.inputs.size());
    logger->info("Output dir: {}", args.cfg.output_dir);
    logger->info("Threads: {}, CRF: {}", args.cfg.threads, args.cfg.crf);

    // Validate input directory
    if (args.cfg.input_dir.empty() || !fs::exists(args.cfg.input_dir)) {
        logger->error("Input directory does not exist: {}", args.cfg.input_dir);
        return 1;
    }

    // Create output directory if needed
    if (!fs::exists(args.cfg.output_dir)) {
        fs::create_directories(args.cfg.output_dir);
        logger->info("Created output directory: {}", args.cfg.output_dir);
    }

    // Run compression engine
    compressor::CompressionEngine engine(args.cfg);

    // Scan for files (assuming first input is directory)
    auto files = engine.scan_media_files(args.cfg.input_dir);

    if (files.empty()) {
        logger->warn("No media files found");
        return 0;
    }

    engine.migrate(files);

    logger->info("MediaHandler finished successfully");
    utils::Logger::flush_all();

    return 0;

    }catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}