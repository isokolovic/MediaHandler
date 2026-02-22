#pragma once
#include "utils/utils.h"
#include "utils/process_result.h"
#include <filesystem>
#include <memory>
#include <spdlog/spdlog.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

using namespace media_handler::utils;

namespace media_handler::compressor {

    class VideoProcessor {
    public:
        VideoProcessor(const utils::Config& cfg, std::shared_ptr<spdlog::logger> logger);
        ~VideoProcessor() = default;

        /// @brief Compress a video file
        /// @param input Input video file path
        /// @param output Output video file path
        /// @return ProcessResult indicating success or failure
        ProcessResult compress(const std::filesystem::path& input, const std::filesystem::path& output);

    private:
        const utils::Config& config;
        std::shared_ptr<spdlog::logger> logger;

        // Video quality settings
        static constexpr int DEFAULT_CRF = 23;
        static constexpr const char* DEFAULT_PRESET = "medium";

        /// @brief Verify file exists and is readable
        static bool file_exists_and_readable(const std::filesystem::path& path);

        /// @brief Verify video file signature
        static bool verify_video_signature(const std::filesystem::path& path);

        /// @brief Copy file as fallback
        ProcessResult fallback_copy(const std::filesystem::path& input, const std::filesystem::path& output);
    };

} // namespace media_handler::compressor