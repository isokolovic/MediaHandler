# pragma once 
#include "utils/config.h"
#include <spdlog/spdlog.h>

namespace media_handler::compressor {
class VideoCompressor {
public:
	explicit VideoCompressor(const utils::Config& cfg, std::shared_ptr<spdlog::logger> logger);
	virtual ~VideoCompressor() = default;

	virtual bool compress(const std::string& input, const std::string& output) = 0;

protected:
	utils::Config config;
	std::shared_ptr<spdlog::logger> logger;

	bool fallback_copy(const std::string& input, const std::string& output) const;
};
}
