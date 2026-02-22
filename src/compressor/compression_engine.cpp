#include "compressor/compression_engine.h"
#include "compressor/image_processor.h"
#include "compressor/video_processor.h"
#include "utils/utils.h"
#include <algorithm>
#include <format>
#include <queue>
#include <latch>

namespace media_handler::compressor {

    namespace fs = std::filesystem;

    CompressionEngine::CompressionEngine(const utils::Config& cfg)
        : config(cfg) // Fill params from config.json
        , logger(cfg.json_log // Initialize logger
            ? utils::Logger::create_json("Engine", cfg.log_level)
            : utils::Logger::create("Engine", cfg.log_level)) {
    }

    bool CompressionEngine::is_supported(const fs::path& p) const {
        auto ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        auto in_array = [&ext](const auto& arr) {
            return std::find(arr.begin(), arr.end(), ext) != arr.end();
            };

        return in_array(video_exts) || in_array(image_exts) || in_array(audio_exts);
    }

    std::vector<fs::path> CompressionEngine::scan_media_files(const fs::path& input_dir) const {
        std::vector<fs::path> files;

        if (!fs::exists(input_dir)) {
            logger->error("Input directory does not exist: {}", input_dir.string());
            return files;
        }

        logger->info("Scanning: {}", input_dir.string());

        for (const auto& entry : fs::recursive_directory_iterator(
            input_dir, fs::directory_options::skip_permission_denied))
        {
            if (entry.is_regular_file() && is_supported(entry.path())) {
                files.push_back(entry.path());
            }
        }

        logger->info("Found {} media files", files.size());
        return files;
    }

    void CompressionEngine::migrate(const std::vector<fs::path>& files) {
        if (files.empty()) {
            logger->info("No files to process");
            return;
        }

        // hardware_concurrency() can return 0 on some exotic platforms - fall back to 4.
        unsigned int num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;

        logger->info("Starting migration of {} files using {} threads", files.size(), num_threads);

        // Thread-pool + work-queue pattern.
        // A single shared queue + condition variable lets threads safely pick up work
        // without busy-waiting. Classic producer-consumer: main thread is the producer
        // (queue is fully populated before workers start), workers are consumers.
        std::queue<fs::path> work;
        for (const auto& f : files) work.push(f);

        std::mutex m;               // Protects the work queue and 'done' flag.
        std::condition_variable cv; // Workers sleep here until work is available.
        bool done = false;          // Set by main thread once no more files will be added.

        // std::latch counts down once per worker on exit so the main thread can block
        // until all workers have finished without needing join() on each thread.
        // Cast to std::ptrdiff_t to suppress narrowing warnings on strict compilers.
        std::latch finished(static_cast<std::ptrdiff_t>(num_threads));

        // Shared processors. compress() is thread-safe on both: no mutable state
        // beyond config and logger which are set at construction and never written again.
        ImageProcessor image_processor(config, logger);
        VideoProcessor video_processor(config, logger);

        // Worker lambda - each thread pulls from the shared queue until it is empty
        // and 'done' is set. done + empty = no more work will ever arrive.
        auto worker = [this, &work, &m, &cv, &done, &finished, &image_processor, &video_processor]() {
            while (true) {
                fs::path file;
                {
                    std::unique_lock<std::mutex> lock(m);
                    // Sleep until there is something to process or the producer signals done.
                    // Predicate is rechecked on every wake to guard against spurious wakeups.
                    cv.wait(lock, [&] { return !work.empty() || done; });

                    if (done && work.empty()) break; // No more work will ever arrive.

                    file = std::move(work.front());
                    work.pop();
                }

                try {
                    // Preserve original directory structure (important with recursive scan!)
                    fs::path relative = fs::relative(file, config.input_dir);
                    fs::path output = config.output_dir / relative;
                    fs::create_directories(output.parent_path());

                    logger->info("[THREAD] Processing: {}", relative.string());

                    // If the destination already exists and is smaller than the source,
                    // it has already been compressed on a previous run - skip it.
                    if (fs::exists(output)) {
                        const auto src_size = fs::file_size(file);
                        const auto dst_size = fs::file_size(output);

                        if (dst_size < src_size) {
                            logger->info("[THREAD] Skipping (already compressed): {} ({} < {})",
                                relative.string(), dst_size, src_size);
                            continue;
                        }
                        logger->info("[THREAD] Overwriting (destination larger/equal): {}", relative.string());
                    }

                    // Lowercase the extension so e.g. ".AVI" and ".avi" both match.
                    // Route using the same extension arrays declared in compression_engine.h
                    // so the list of supported types is defined in exactly one place.
                    auto ext = file.extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(),
                        [](unsigned char c) { return std::tolower(c); });

                    ProcessResult res;
                    if (std::find(video_exts.begin(), video_exts.end(), ext) != video_exts.end()) {
                        res = video_processor.compress(file, output);
                    }
                    else {
                        // Covers image_exts (compressed) and audio_exts (fallback_copy inside ImageProcessor).
                        res = image_processor.compress(file, output);
                    }

                    if (res.success) {
                        logger->info("[THREAD] Success: {}", relative.string());
                    }
                    else {
                        logger->warn("[THREAD] Failed: {} - {}", relative.string(), res.message);
                    }
                }
                catch (const std::exception& e) {
                    logger->error("[THREAD] Exception on {}: {}", file.string(), e.what());
                }
                catch (...) {
                    logger->error("[THREAD] Unknown exception on {}", file.string());
                }
            }

            finished.count_down(); // Signal this worker has finished.
            };

        // Launch all worker threads. std::jthread auto-joins on destruction,
        // but we wait on the latch below so all work is done before we return.
        std::vector<std::jthread> threads;
        threads.reserve(num_threads);
        for (unsigned int i = 0; i < num_threads; ++i) {
            threads.emplace_back(worker);
        }

        // Signal workers that the queue is fully populated and no new items will arrive.
        // Must be done under the lock so workers cannot miss the notification between
        // checking the predicate and going to sleep.
        {
            std::lock_guard<std::mutex> lock(m);
            done = true;
        }
        cv.notify_all();

        // Block until every worker has called count_down().
        finished.wait();
        logger->info("Migration complete");
    }

} // namespace media_handler::compressor