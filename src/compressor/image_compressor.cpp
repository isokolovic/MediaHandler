# include "compressor/image_compressor.h"

namespace media_handler::compressor {
	ImageProcessor::ImageProcessor(const utils::Config& cfg, std::shared_ptr<spdlog::logger> logger)
		: config(cfg), logger(logger) {}

	bool ImageProcessor::compress(const std::filesystem::path& input, const std::filesystem::path& output) {
		return true;
	}
}