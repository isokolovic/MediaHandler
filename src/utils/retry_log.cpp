#include "utils/retry_log.h"
#include <fstream>
#include <format>
#include <nlohmann/json.hpp>

namespace media_handler::utils {

    namespace fs = std::filesystem;
    using json = nlohmann::json;

    static constexpr const char* STATE_FILE = ".mediahandler_state";

    RetryLog::RetryLog(const fs::path& output_dir, std::shared_ptr<spdlog::logger> logger)
        : state_file(output_dir / STATE_FILE)
        , logger(std::move(logger)) {}

    std::string RetryLog::normalize(const fs::path& p) {
        std::error_code ec;
        auto cp = fs::weakly_canonical(p, ec);

        return ec ? p.string() : cp.string();
    }

    void RetryLog::load() {
        std::error_code ec;
        if (!fs::exists(state_file, ec)) {
            logger->info("No state file — fresh run"); 
            return; 
        }

        try {
            std::ifstream f(state_file);
            if (!f) { logger->warn("Cannot open state file: {}", state_file.string()); return; }

            json j = json::parse(f);
            for (const auto& e : j.value("completed", json::array())) completed.insert(e.get<std::string>());
            for (const auto& e : j.value("failed", json::array())) failed.insert(e.get<std::string>());

            logger->info("State: {} completed, {} failed", completed.size(), failed.size());
        }
        catch (const json::exception& e) {
            // Corrupt state — start fresh rather than aborting the run.
            logger->warn("Corrupt state file, starting fresh: {}", e.what());
            completed.clear();
            failed.clear();
        }
    }

    void RetryLog::save() const {
        try {
            json j;
            j["completed"] = completed;
            j["failed"] = failed;

            auto tmp = state_file;
            tmp += ".tmp";
            {
                std::ofstream f(tmp);
                if (!f) { logger->error("Cannot write state: {}", tmp.string()); return; }
                f << j.dump(2);
            }
            std::error_code ec;
            fs::rename(tmp, state_file, ec);
            if (ec) logger->error("State commit failed: {}", ec.message());
        }
        catch (const std::exception& e) {
            logger->error("Exception saving state: {}", e.what());
        }
    }

    bool RetryLog::is_completed(const fs::path& file) const { 
        return completed.count(normalize(file)) > 0; 
    }

    bool RetryLog::is_failed(const fs::path& file) const { 
        return failed.count(normalize(file)) > 0; 
    }

    void RetryLog::mark_completed(const fs::path& file) {
        auto key = normalize(file);
        completed.insert(key);
        failed.erase(key); // success clears any prior failure record
    }

    void RetryLog::mark_failed(const fs::path& file) {
        failed.insert(normalize(file));
    }

} // namespace media_handler::utils