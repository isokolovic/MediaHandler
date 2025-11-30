#include <utils/config.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <format>

using json = nlohmann::json;

namespace media_handler::utils {
    std::expected<Config, std::string> Config::load(const std::string& path) {
        Config cfg;

        std::ifstream file(path);
        if (!file.is_open()) {
            return std::unexpected("Config file not found: " + path);
        }

        json j;
        try {
            file >> j;
        }
        catch (const json::parse_error& e) {
            return std::unexpected(std::format("JSON parse error: {}", e.what()));
        }

        try {
            // Video
            if (j.contains("video") && j["video"].is_object()) {
                const auto& v = j["video"];
                cfg.video_codec = v.value("codec", cfg.video_codec);
                cfg.video_preset = v.value("preset", cfg.video_preset);
                cfg.crf = v.value("crf", cfg.crf);
                cfg.maxrate = v.value("maxrate", cfg.maxrate);
                cfg.bufsize = v.value("bufsize", cfg.bufsize);
            }

            // Audio
            if (j.contains("audio") && j["audio"].is_object()) {
                const auto& a = j["audio"];
                cfg.audio_codec = a.value("codec", cfg.audio_codec);
                cfg.audio_bitrate = a.value("bitrate", cfg.audio_bitrate);
            }

            // General
            if (j.contains("general") && j["general"].is_object()) {
                const auto& g = j["general"];
                cfg.container = g.value("container", cfg.container);
                cfg.output_dir = g.value("output_dir", cfg.output_dir);
                cfg.threads = g.value("threads", cfg.threads);
                cfg.json_log = g.value("json_log", cfg.json_log);

                if (g.contains("log_level")) {
                    std::string lvl = g.value("log_level", "info");
                    cfg.log_level = spdlog::level::from_str(lvl);
                }
            }
        }
        catch (...) {
            // Any conversion error: silently keep defaults
        }

        return cfg;
    }

	void Config::save(const std::string& path) const {
		json j;

		// Video
		j["video"]["codec"] = video_codec;
		j["video"]["preset"] = video_preset;
		j["video"]["crf"] = crf;
		if (!maxrate.empty()) j["video"]["maxrate"] = maxrate;
		if (!bufsize.empty()) j["video"]["bufsize"] = bufsize;

		// Audio
		j["audio"]["codec"] = audio_codec;
		j["audio"]["bitrate"] = audio_bitrate;

		// General
		j["general"]["container"] = container;
		j["general"]["output_dir"] = output_dir;
		j["general"]["threads"] = threads;
		j["general"]["json_log"] = json_log;
		j["general"]["log_level"] = spdlog::level::to_string_view(log_level);

		std::ofstream f(path);
		f << std::setw(4) << j << std::endl;
	}
}