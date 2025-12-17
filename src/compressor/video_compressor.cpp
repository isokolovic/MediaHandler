#include "compressor/video_compressor.h"
#include <utility>

namespace media_handler::compressor {
	VideoCompressor::VideoCompressor(const utils::Config& cfg, std::shared_ptr<spdlog::logger> logger)
		: config(cfg), logger(std::move(logger)) {}



	}

