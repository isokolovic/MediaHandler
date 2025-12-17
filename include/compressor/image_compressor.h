# pragma once
#include "utils/config.h"
#include "compressor/compression_engine.h"

namespace media_handler::compressor {

#define PHOTO_QUALITY 80
#define PHOTO_TRIM_WIDTH 1920
#define PHOTO_TRIM_HEIGHT 1080

	class ImageProcessor {
	public:
		ImageProcessor(const utils::Config& cfg, std::shared_ptr<spdlog::logger> logger);

		/// @brief Compress an image file (jpg, png, heic)
		/// @param input Input image path
		/// @param output Output image path
		/// @return True on success
		bool compress(const std::filesystem::path& input, const std::filesystem::path& output);
	private:
		utils::Config config;
		std::shared_ptr<spdlog::logger> logger;
	};
} // namespace media_handler::compressor