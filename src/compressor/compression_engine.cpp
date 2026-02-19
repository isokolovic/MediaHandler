#include "compressor/compression_engine.h"
#include "compressor/image_processor.h"
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
            : utils::Logger::create("Engine", cfg.log_level)){}

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

        // Determine number of threads (hardware_concurrency() can return 0 on some exotic platforms)
        unsigned int num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;

        logger->info("Starting migration of {} files using {} threads", files.size(), num_threads);

        // === Thread-pool + work-queue pattern ===
        // We use a single shared queue + condition variable so threads can safely
        // pick up work without busy-waiting. This is a classic producer-consumer setup.
        std::queue<fs::path> work;
        for (const auto& f : files) work.push(f);

        std::mutex m;                     // protects the work queue and 'done' flag
        std::condition_variable cv;       // workers sleep here until work is available
        bool done = false;                // main thread sets this when no more files will be added
        std::latch finished(static_cast<std::ptrdiff_t>(num_threads));
        // std::latch constructor expects std::ptrdiff_t.
        // We cast to suppress narrowing warnings on compilers that treat
        // unsigned → signed conversion strictly.

        // Single shared ImageProcessor (its compress() method is thread-safe)
        media_handler::compressor::ImageProcessor image_processor(config, logger);

        // Worker lambda - each thread runs this until the queue is empty and done == true
        auto worker = [this, &work, &m, &cv, &done, &finished, &image_processor]() {
            while (true) {
                fs::path file;
                {
                    std::unique_lock<std::mutex> lock(m);
                    cv.wait(lock, [&] { return !work.empty() || done; });

                    if (done && work.empty()) break;   // No more work

                    file = std::move(work.front());
                    work.pop();
                }

                try {
                    // Preserve original directory structure (important with recursive scan!)
                    fs::path relative = fs::relative(file, config.input_dir);
                    fs::path output = config.output_dir / relative;
                    fs::create_directories(output.parent_path());

                    logger->info("[THREAD] Processing: {}", relative.string());

                    // If destination already exists and is smaller, it is already compressed -> skip
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

                    auto res = image_processor.compress(file, output);

                    if (res.success) {
                        logger->info("[THREAD] Success: {}", relative.string());
                    }
                    else {
                        logger->warn("[THREAD] Failed: {} → {}", relative.string(), res.message);
                    }
                }
                catch (const std::exception& e) {
                    logger->error("[THREAD] Exception on {}: {}", file.string(), e.what());
                }
                catch (...) {
                    logger->error("[THREAD] Unknown exception on {}", file.string());
                }
            }

            finished.count_down();   // Signal this worker has finished
            };

        // Launch all worker threads
        std::vector<std::jthread> threads;
        for (unsigned int i = 0; i < num_threads; ++i) {
            threads.emplace_back(worker);
        }

        // Tell workers we are done feeding the queue
        {
            std::lock_guard<std::mutex> lock(m);
            done = true;
        }
        cv.notify_all();

        finished.wait();   // Block until every worker has called count_down()
        logger->info("Migration complete");
    }

} // namespace media_handler