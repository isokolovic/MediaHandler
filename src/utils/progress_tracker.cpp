#include "utils/progress_tracker.h"
#include <format>

namespace media_handler::utils {

    namespace fs = std::filesystem;

    ProgressTracker::ProgressTracker(std::size_t total_files, std::shared_ptr<spdlog::logger> logger)
        : total(total_files)
        , logger(std::move(logger))
        , run_start(std::chrono::steady_clock::now()){
        std::lock_guard lock(mutex);
        stats.reserve(total_files);
        start_times.reserve(total_files);
    }

    std::size_t ProgressTracker::begin_file(const fs::path& file) {
        std::lock_guard lock(mutex);
        FileStats s;
        s.filename = file.filename().string();
        std::error_code ec;
        s.size_in = fs::file_size(file, ec);

        std::size_t token = stats.size();
        stats.push_back(std::move(s));
        start_times.push_back(std::chrono::steady_clock::now());

        return token;
    }

    void ProgressTracker::finish_file(std::size_t token, const fs::path& output, bool success, const std::string& error) {

        auto now = std::chrono::steady_clock::now();

        {
            std::lock_guard lock(mutex);
            auto& s = stats[token];
            s.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_times[token]);
            s.success = success;
            s.error = error;

            if (success) {
                std::error_code ec;
                s.size_out = fs::file_size(output, ec);
            }
        }

        if (success) {
            auto pos = ++completed + failed.load() + skipped.load();

            std::lock_guard lock(mutex);
            const auto& s = stats[token];
            double ratio = s.size_in > 0 ? (1.0 - static_cast<double>(s.size_out) / s.size_in) * 100.0 : 0.0;
            double mb_in = s.size_in / 1'048'576.0;
            double mb_out = s.size_out / 1'048'576.0;
            double mb_per_s = s.elapsed.count() > 0 ? mb_in / (s.elapsed.count() / 1000.0) : 0.0;

            logger->info("[{}/{}] OK {} | {:.1f}MB -> {:.1f}MB ({:.0f}% saved) | {:.1f}MB/s | {}ms", pos, total, s.filename, mb_in, mb_out, ratio, mb_per_s, s.elapsed.count());
        }
        else {
            auto pos = completed.load() + ++failed + skipped.load();
            std::lock_guard lock(mutex);
            const auto& s = stats[token];

            logger->error("[{}/{}] FAIL {} | {} | {}ms", pos, total, s.filename, error, s.elapsed.count());
        }
    }

    void ProgressTracker::skip_file(const fs::path& file) {
        auto pos = completed.load() + failed.load() + ++skipped;

        logger->debug("[{}/{}] SKIP {} (already completed)", pos, total, file.filename().string());
    }

    void ProgressTracker::print_summary() const {
        auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - run_start).count();

        std::uintmax_t in = 0, out = 0;
        {
            std::lock_guard lock(mutex);
            for (const auto& s : stats) {
                if (s.success && !s.skipped) { in += s.size_in; out += s.size_out; }
            }
        }

        double gb_in = in / 1'073'741'824.0;
        double gb_out = out / 1'073'741'824.0;
        double saved = in > 0 ? (1.0 - static_cast<double>(out) / in) * 100.0 : 0.0;

        long s = total_ms / 1000;
        long mins = s / 60; s %= 60;
        long hrs = mins / 60; mins %= 60;
        auto elapsed = hrs > 0 ? std::format("{}h {}m {}s", hrs, mins, s)
            : mins > 0 ? std::format("{}m {}s", mins, s)
            : std::format("{}s", s);

        logger->info("=================================================");

        logger->info("  Total   : {}", total);
        logger->info("  OK      : {}", completed.load());
        logger->info("  Failed  : {}", failed.load());
        logger->info("  Skipped : {}", skipped.load());
        logger->info("  Before  : {:.2f} GB", gb_in);
        logger->info("  After   : {:.2f} GB", gb_out);
        logger->info("  Saved   : {:.1f}%", saved);
        logger->info("  Time    : {}", elapsed);

        if (failed.load() > 0) {
            logger->warn("  {} file(s) failed — run with --retry", failed.load());
        }

        logger->info("=================================================");
    }

} // namespace media_handler::utils