#include "compressor/compression_engine.h"
#include "compressor/image_processor.h"
#include "utils/utils.h"
#include <algorithm>
#include <format>
#include <queue>
#include <latch>

namespace media_handler::compressor {

    CompressionEngine::CompressionEngine(const utils::Config& cfg)
        : config(cfg) // Fill params from config.json
        , logger(cfg.json_log // Initialize logger
            ? utils::Logger::create_json("Engine", cfg.log_level)
            : utils::Logger::create("Engine", cfg.log_level)){}

    bool CompressionEngine::is_supported(const std::filesystem::path& p) const {
        auto ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        auto in_array = [&ext](const auto& arr) {
            return std::find(arr.begin(), arr.end(), ext) != arr.end();
            };
        
        return in_array(video_exts) || in_array(image_exts) || in_array(audio_exts);
    }

    std::vector<std::filesystem::path> CompressionEngine::scan_media_files(const std::filesystem::path& input_dir) const {
        std::vector<std::filesystem::path> files;

        if (!std::filesystem::exists(input_dir)) {
            logger->error("Input directory does not exist: {}", input_dir.string());
            return files;
        }

        logger->info("Scanning: {}", input_dir.string());

        for (const auto& entry : std::filesystem::recursive_directory_iterator(
            input_dir, std::filesystem::directory_options::skip_permission_denied))
        {
            if (entry.is_regular_file() && is_supported(entry.path())) {
                files.push_back(entry.path());
            }
        }

        logger->info("Found {} media files", files.size());
        return files;
    }

    void CompressionEngine::migrate(const std::vector<std::filesystem::path>& files) {
        // Determine number of threads to use (fallback = 4)
        unsigned int num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4; 

        logger->info("Starting migration of {} files using {} threads", files.size(), num_threads);

        // Shared queue of work items (file paths)
        std::queue<std::filesystem::path> work;
        for (auto& f : files) work.push(f);

		std::mutex m; // make sure only one thread can access the queue at a time
        std::condition_variable cv; // lets worker sleep until there's work available
		bool done = false; // signals no more work will be added
        std::latch finished(num_threads); // latch ensures we wait until all workers finish
        // TODO why not std::latch finished(static_cast<std::ptrdiff_t>(num_threads));

        // Create a single ImageProcessor instance (thread-safe process method) 
        media_handler::compressor::ImageProcessor image_processor(config, logger); 

        // Worker lambda: each thread repeatedly pops files and processes them
        auto worker = [this, &work, &m, &cv, &done, &finished, &image_processor]() {
            while (true) {
                std::filesystem::path file;
                {
					std::unique_lock<std::mutex> lock(m); // lock the queue
					cv.wait(lock, [&] { return !work.empty() || done; }); // wait for work or done signal
                    if (done && work.empty()) break;
                    file = work.front();
					work.pop(); // pop the file from queue
                }
                logger->info("[THREAD] Processing: {}", file.filename().string());

                try { 
                    std::filesystem::path output = config.output_dir / file.filename(); 
					auto res = image_processor.compress(file, output);
                        
                    if (res.success) { 
                        logger->info("[THREAD] Done: {}", file.filename().string()); 
                    } 
                    else { 
                        logger->warn("[THREAD] Failed: {} reason: {}", file.filename().string(), res.message); 
                    } 
                } 
                catch (const std::exception& e) { 
                    logger->error("[THREAD] Exception processing {}: {}", file.string(), e.what()); 
                }
                catch (...) { 
                    logger->error("[THREAD] Unknown exception processing {}", file.string()); 
                }

				// TODO : actual migration logic here
                //std::this_thread::sleep_for(std::chrono::milliseconds(50)); 

                logger->info("[THREAD] Done: {}", file.filename().string());
            }

            finished.count_down(); // signal this worker is done
            };

        // Launch worker threads
        std::vector<std::jthread> threads;
        for (unsigned int i = 0; i < num_threads; ++i)
            threads.emplace_back(worker);

        {
			std::lock_guard<std::mutex> lock(m); // lock to set done flag
            done = true;
        }
        cv.notify_all();

        finished.wait(); // wait until all workers have signaled completion

        logger->info("Migration complete");
    }

} // namespace media_handler