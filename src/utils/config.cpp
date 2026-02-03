#include "utils/config.h"
#include <filesystem>
#include <expected>
#include <fstream>
#include <format>
#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace media_handler::utils {

    std::expected<Config, std::string> Config::load(const std::filesystem::path& path, const std::shared_ptr<spdlog::logger>& logger) {
        Config cfg;

        std::ifstream file(path);
        if (!file.is_open()) {
            return std::unexpected(std::format("Config file not found: {}", path.string()));
        }

        nlohmann::json j;
        try {
            file >> j;
        }
        catch (const nlohmann::json::parse_error& e) {
            return std::unexpected(std::format("JSON parse error: {}", e.what()));
        }

        try {
            if (j.contains("video") && j["video"].is_object()) {
                const auto& v = j["video"];
                cfg.video_codec = v.value("codec", cfg.video_codec);
                cfg.video_preset = v.value("preset", cfg.video_preset);
                cfg.crf = v.value("crf", cfg.crf);
                cfg.maxrate = v.value("maxrate", cfg.maxrate);
                cfg.bufsize = v.value("bufsize", cfg.bufsize);
            }

            if (j.contains("audio") && j["audio"].is_object()) {
                const auto& a = j["audio"];
                cfg.audio_codec = a.value("codec", cfg.audio_codec);
                cfg.audio_bitrate = a.value("bitrate", cfg.audio_bitrate);
            }

            if (j.contains("general") && j["general"].is_object()) {
                const auto& g = j["general"];
                cfg.container = g.value("container", cfg.container);
                cfg.input_dir = g.value("input_dir", cfg.input_dir);
                cfg.output_dir = g.value("output_dir", cfg.output_dir);
                cfg.threads = g.value("threads", cfg.threads);
                cfg.json_log = g.value("json_log", cfg.json_log);

                if (g.contains("log_level")) {
                    cfg.log_level = spdlog::level::from_str(g.value("log_level", "info"));
                }
            }
        }
        catch (const nlohmann::json::type_error& e) {
            return std::unexpected(std::format("Config type mismatch: {}", e.what()));
        }

        auto val = cfg.validate();
        if (!val) {
            return std::unexpected(val.error());
        }

        return cfg;
    }

    std::expected<void, std::string> Config::validate() const {
        if (threads == 0) return std::unexpected("config.json: threads must be >= 1");
        if (input_dir.empty()) return std::unexpected("config.json: input_dir must not be empty");
        if (output_dir.empty()) return std::unexpected("config.json: output_dir must not be empty");
        return {};
    }
}