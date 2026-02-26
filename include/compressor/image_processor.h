# pragma once
#include "utils/config.h"
#include "utils/process_result.h"
#include "compressor/compression_engine.h"
#include <png.h>
#include <cstdio>
#include <vector>
#include <setjmp.h>

using namespace media_handler::utils;

namespace media_handler::compressor {

	constexpr int PHOTO_QUALITY = 80; 
	constexpr int PHOTO_TRIM_WIDTH = 1920; 
	constexpr int PHOTO_TRIM_HEIGHT = 1080;

	class ImageProcessor {
	public:
		ImageProcessor(const utils::Config& cfg, std::shared_ptr<spdlog::logger> logger);
		
		/// @brief Compress an image file (jpg, png, heic)
		ProcessResult compress(const std::filesystem::path& input, const std::filesystem::path& output);

	private:
		utils::Config config;
		std::shared_ptr<spdlog::logger> logger;

		/// @brief Compresses an image and writes it as a JPEG file to the specified output path.
		ProcessResult compress_jpeg(const std::filesystem::path& input, const std::filesystem::path& output);

		/// @brief Compresses a PNG file from the specified input path and writes the compressed result to the specified output path.
		ProcessResult compress_png(const std::filesystem::path& input, const std::filesystem::path& output);

		/// @brief Compresses the source image to HEIC format and writes the result to the specified output path.
		ProcessResult compress_heic(const std::filesystem::path& input, const std::filesystem::path& output);

		/// @brief Performs a copy from the input path to the output path, using fallback methods if a standard copy attempt fails.
		ProcessResult fallback_copy(const std::filesystem::path& input, const std::filesystem::path& output);
	};
} // namespace media_handler::compressor