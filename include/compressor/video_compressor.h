# pragma once 
#include "utils/config.h"
#include <spdlog/spdlog.h>

namespace media_handler::compressor {
class VideoCompressor {
public:
	explicit VideoCompressor(const utils::Config& cfg, std::shared_ptr<spdlog::logger> logger);

	/// @brief Compress a video file
	/// @param input Input video path
	/// @param output Output video path
	/// @return True on success
	bool compress(const std::string& input, const std::string& output);
private:
	utils::Config config;
	std::shared_ptr<spdlog::logger> logger;
};
} // namespace media_handler::compressor
