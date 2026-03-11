#include "compressor/compression_engine.h"
#include "compressor/image_processor.h"
#include "compressor/video_processor.h"
#include "utils/retry_log.h"
#include "utils/organizer.h"
#include "utils/progress_tracker.h"
#include "utils/utils.h"
#include <algorithm>
#include <format>
#include <queue>
#include <latch>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace media_handler::compressor {

    namespace fs = std::filesystem;
    using namespace media_handler::utils;

    CompressionEngine::CompressionEngine(const Config& cfg)
        : config(cfg)
        , logger(cfg.json_log
            ? Logger::create_json("Engine", cfg.log_level)
            : Logger::create("Engine", cfg.log_level)) {}

    bool CompressionEngine::is_supported(const fs::path& p) const {
        auto ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        auto in_array = [&ext](const auto& arr) {
            return std::find(arr.begin(), arr.end(), ext) != arr.end();
            };

        return in_array(video_exts) || in_array(image_exts) || in_array(other_exts);
    }

    std::vector<fs::path> CompressionEngine::scan_media_files(const fs::path& input_dir) const {
        std::vector<fs::path> files;

        if (!fs::exists(input_dir)) {
            logger->error("Input directory does not exist: {}", path_to_utf8(input_dir));
            return files;
        }

        logger->info("Scanning: {}", path_to_utf8(input_dir));

        for (const auto& entry : fs::recursive_directory_iterator(input_dir, fs::directory_options::skip_permission_denied))
        {
            if (entry.is_regular_file() && is_supported(entry.path())) {
                files.push_back(entry.path());
            }
        }

        logger->info("Found {} media files", files.size());
        return files;
    }

    void CompressionEngine::migrate(const std::vector<fs::path>& files, const MigrateOptions& opts) {

        if (files.empty()) { logger->info("No files to process"); return; }

        // Organize-only mode: move files into output_dir/<YYYY>/ with no compression.
        if (opts.organize && !opts.retry) {
            Organizer organizer(logger);
            for (const auto& file : files) {
                auto result = organizer.organize(file, config.output_dir);
                if (!result.success)
                    logger->warn("Organize failed for {}: {}", path_to_utf8(file.filename()), result.error);
            }
            logger->info("Organize complete");
            return;
        }

        // Load state from prior run.
        // Normal run: skip completed files (resume after crash).
        // Retry run: process only files marked failed in prior run.
        RetryLog retry_log(config.output_dir, logger);
        retry_log.load();

        std::vector<fs::path> work_files;
        work_files.reserve(files.size());
        std::size_t pre_skipped = 0;

        if (opts.retry) {
            if (retry_log.failed_count() == 0) { logger->info("Retry: no failed files recorded"); return; }
            for (const auto& f : files)
                if (retry_log.is_failed(f)) work_files.push_back(f);
            logger->info("Retry: {} file(s)", work_files.size());
        }
        else {
            for (const auto& f : files) {
                if (retry_log.is_completed(f)) {
                    ++pre_skipped;
                }
                else {
                    work_files.push_back(f);
                }
            }
            if (pre_skipped > 0) logger->info("Resuming: {} already done", pre_skipped);
        }

        if (work_files.empty()) { logger->info("Nothing to do"); return; }

        unsigned int num_threads = config.threads;
        if (num_threads == 0) num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;

        logger->info("Starting migration of {} files using {} threads", work_files.size(), num_threads);

        ProgressTracker tracker(work_files.size(), logger);

        // Mark skipped files explicitly in the tracker so counts are correct.
        for (const auto& f : files) {
            if (retry_log.is_completed(f)) {
                tracker.skip_file(f);
            }
        }

        // Thread-pool + work-queue pattern.
        // A single shared queue + condition variable = threads safely pick up work without busy-waiting.
        // Queue is fully populated before workers start.
        std::queue<fs::path> work;
        for (const auto& f : work_files) work.push(f);

        std::mutex queue_mutex; // Protects the work queue and 'done' flag.
        std::condition_variable cv; // Workers sleep here until work is available.
        bool done = false; // Set by main thread once no more files will be added.
        std::mutex state_mutex; // Serializes RetryLog and Organizer calls.

        std::latch finished(static_cast<std::ptrdiff_t>(num_threads));

        ImageProcessor image_proc(config, logger);
        VideoProcessor video_proc(config, logger);

        auto worker = [this, &work, &queue_mutex, &cv, &done, &finished, &image_proc, &video_proc, &retry_log, &state_mutex, &tracker, &opts]()
            {
                //Wrap the whole loop in a try/catch so OS exceptions can't terminate threads
                try {
                    while (true) {
                        fs::path file;

                        {
                            std::unique_lock<std::mutex> lock(queue_mutex);
                            cv.wait(lock, [&] { return !work.empty() || done; });
                            if (done && work.empty()) break;
                            file = std::move(work.front());
                            work.pop();
                        }

                        try {
                            fs::path relative;
                            try {
                                relative = fs::relative(file, config.input_dir);
                            }
                            catch (...) {
                                relative = file.filename();
                            }

                            fs::path output = config.output_dir / relative;
                            try {
                                if (!fs::exists(output.parent_path())) {
                                    fs::create_directories(output.parent_path());
                                }
                            }
                            catch (const std::exception& e) {
                                logger->error("[THREAD] Filesystem error creating directories for {}: {}", path_to_utf8(output.parent_path()), e.what());
                                std::lock_guard lock(state_mutex);
                                retry_log.mark_failed(file);
                                retry_log.save();
                                tracker.finish_file(tracker.begin_file(file), output, false, "mkdir failed");
                                continue;
                            }

                            logger->info("[THREAD] Processing: {}", path_to_utf8(relative));

                            if (fs::exists(output) && fs::is_regular_file(output) && fs::is_regular_file(file)) {
                                const auto src_size = fs::file_size(file);
                                const auto dst_size = fs::file_size(output);

                                if (dst_size < src_size) {
                                    logger->info("[THREAD] Skipping (already compressed): {} ({} < {})", path_to_utf8(relative), dst_size, src_size);
                                    tracker.finish_file(tracker.begin_file(file), output, true, "skipped (already compressed)");
                                    continue;
                                }

                                logger->info("[THREAD] Overwriting (destination larger/equal): {}", path_to_utf8(relative));
                            }

                            auto token = tracker.begin_file(file);

                            auto ext = file.extension().string();
                            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

                            ProcessResult res;
                            if (ext_matches(video_exts, ext))
                                res = video_proc.compress(file, output);
                            else
                                res = image_proc.compress(file, output);

                            tracker.finish_file(token, output, res.success, res.message);

                            {
                                std::lock_guard lock(state_mutex);
                                if (res.success) retry_log.mark_completed(file);
                                else retry_log.mark_failed(file);
                                retry_log.save();
                            }
                        }
                        catch (const std::exception& e) {
                            logger->error("[THREAD] Exception on {}: {}", path_to_utf8(file), e.what());
                            std::lock_guard lock(state_mutex);
                            retry_log.mark_failed(file);
                            retry_log.save();
                        }
                        catch (...) {
                            logger->error("[THREAD] Unknown exception on {}", path_to_utf8(file));
                            std::lock_guard lock(state_mutex);
                            retry_log.mark_failed(file);
                            retry_log.save();
                        }
                    }
                }
                catch (const std::exception& e) {
                    logger->error("[THREAD] Fatal exception (thread exiting): {}", e.what());
                }
                catch (...) {
                    logger->error("[THREAD] Fatal unknown exception (thread exiting)");
                }

                finished.count_down();
            };

        std::vector<std::jthread> threads;
        threads.reserve(num_threads);
        for (unsigned int i = 0; i < num_threads; ++i) {
            try {
                threads.emplace_back(worker);
            }
            catch (const std::exception& e) {
                logger->error("Failed to create thread {}: {}", i, e.what());
                // If thread creation fails, decrement latch so finished.wait() won't block forever.
                finished.count_down();
            }
        }

        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            done = true;
        }
        cv.notify_all();

        finished.wait();

        tracker.print_summary();

        if (retry_log.failed_count() > 0)
            logger->warn("{} file(s) failed — run with --retry", retry_log.failed_count());

        logger->info("Migration complete");
    }

} // namespace media_handler::compressor