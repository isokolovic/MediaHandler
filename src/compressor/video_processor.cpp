#include "compressor/video_processor.h"
#include <utility>

namespace media_handler::compressor {
	VideoCompressor::VideoCompressor(const utils::Config& cfg, std::shared_ptr<spdlog::logger> logger)
		: config(cfg), logger(std::move(logger)) {}

}

