#include "utils/organizer.h"
#include "utils/utils.h"
#include <libexif/exif-data.h>
#include <format>
#include <algorithm>

#ifdef _WIN32
#   include <windows.h>
#elif defined(__linux__) && defined(STATX_BTIME)
#   include <sys/stat.h>
#   include <fcntl.h>
#endif

namespace media_handler::utils {

    namespace fs = std::filesystem;
    using ymd = std::chrono::year_month_day;
    using days = std::chrono::days;

    Organizer::Organizer(std::shared_ptr<spdlog::logger> logger)
        : logger(std::move(logger)) {
    }

    Organizer::Result Organizer::organize(const fs::path& file, const fs::path& output_root) const {
        auto date = earliest_date(file);
        if (!date) return Result::Err(std::format("Cannot determine date: {}", file.string()));
        return move_to_year_dir(file, output_root, *date);
    }

    std::optional<ymd> Organizer::earliest_date(const fs::path& file) const {
        std::vector<ymd> candidates;
        collect_exif_dates(file, candidates);
        collect_fs_dates(file, candidates);

        if (candidates.empty()) return std::nullopt;

        auto it = std::min_element(candidates.begin(), candidates.end(),
            [](const ymd& a, const ymd& b) {
                return std::chrono::sys_days(a) < std::chrono::sys_days(b);
            });

        logger->debug("Earliest date for {}: {}-{:02d}-{:02d}",
            file.filename().string(),
            static_cast<int>(it->year()),
            static_cast<unsigned>(it->month()),
            static_cast<unsigned>(it->day()));

        return *it;
    }

    void Organizer::collect_exif_dates(const fs::path& file, std::vector<ymd>& out) const {
        auto ext = file.extension().string();
        for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (ext != ".jpg" && ext != ".jpeg") return;

        auto raw = read_file_bytes(file);
        ExifData* ed = raw.empty()
            ? nullptr
            : exif_data_new_from_data(raw.data(), static_cast<unsigned int>(raw.size()));

        if (!ed) return;

        constexpr ExifTag tags[] = { EXIF_TAG_DATE_TIME_ORIGINAL, EXIF_TAG_DATE_TIME };
        for (auto tag : tags) {
            ExifEntry* entry = exif_data_get_entry(ed, tag);
            if (!entry) continue;

            char buf[32] = {};
            exif_entry_get_value(entry, buf, sizeof(buf));

            int y = 0, mo = 0, d = 0;
            if (std::sscanf(buf, "%d:%d:%d", &y, &mo, &d) == 3 && y > 1970 && mo >= 1 && mo <= 12) {
                out.push_back(ymd{
                    std::chrono::year{y},
                    std::chrono::month{static_cast<unsigned>(mo)},
                    std::chrono::day{static_cast<unsigned>(d)}
                    });
                logger->debug("EXIF date {}-{:02d}-{:02d}: {}", y, mo, d, file.filename().string());
            }
        }

        exif_data_unref(ed);
    }

    void Organizer::collect_fs_dates(const fs::path& file, std::vector<ymd>& out) const {

        auto from_ftime = [](fs::file_time_type ft) -> ymd {
            auto sys_now = std::chrono::system_clock::now();
            auto ft_now = fs::file_time_type::clock::now();
            auto sys = sys_now + std::chrono::duration_cast<std::chrono::system_clock::duration>(ft - ft_now);
            return ymd{ std::chrono::floor<days>(sys) };
            };

        // system_clock::time_point -> ymd
        auto from_sys = [](std::chrono::system_clock::time_point tp) -> ymd {
            return ymd{ std::chrono::floor<days>(tp) };
            };

        // mtime — std::filesystem
        std::error_code ec;
        auto mtime = fs::last_write_time(file, ec);
        if (!ec) out.push_back(from_ftime(mtime));

        // Birth time, platform-specific
#ifdef _WIN32
        HANDLE h = CreateFileW(file.wstring().c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (h != INVALID_HANDLE_VALUE) {
            FILETIME btime_ft;

            if (GetFileTime(h, &btime_ft, nullptr, nullptr)) {
                ULARGE_INTEGER ui;
                ui.LowPart = btime_ft.dwLowDateTime;
                ui.HighPart = btime_ft.dwHighDateTime;

                // FILETIME epoch is 1601-01-01; Unix epoch offset in 100ns intervals.
                constexpr uint64_t epoch_diff = 116444736000000000ULL;
                auto dur = std::chrono::duration<uint64_t, std::ratio<1, 10'000'000>>(ui.QuadPart - epoch_diff);
                out.push_back(from_sys(std::chrono::system_clock::time_point{ std::chrono::duration_cast<std::chrono::system_clock::duration>(dur) }));
            }

            CloseHandle(h);
        }
#elif defined(__linux__) && defined(STATX_BTIME)
        struct statx stx;
        if (statx(AT_FDCWD, file.string().c_str(), 0, STATX_BTIME, &stx) == 0 && (stx.stx_mask & STATX_BTIME)) {
            out.push_back(from_sys(std::chrono::system_clock::from_time_t(static_cast<time_t>(stx.stx_btime.tv_sec))));
        }
#endif
    }


    Organizer::Result Organizer::move_to_year_dir(const fs::path& file, const fs::path& output_root, ymd date) const
    {
        int y = static_cast<int>(date.year());

        auto year_dir = output_root / std::format("{:04d}", y);

        std::error_code ec;
        fs::create_directories(year_dir, ec);
        if (ec) return Result::Err(std::format("Cannot create {}: {}", year_dir.string(), ec.message()));

        auto dest = year_dir / file.filename();

        if (fs::exists(dest, ec)) {
            auto stem = file.stem().string();
            auto ext = file.extension().string();
            for (int n = 1; n < 10'000; ++n) {
                dest = year_dir / std::format("{}_{}{}", stem, n, ext);
                if (!fs::exists(dest, ec)) break;
            }
        }

        fs::rename(file, dest, ec);
        if (ec) {
            fs::copy_file(file, dest, fs::copy_options::overwrite_existing, ec);
            if (ec) return Result::Err(std::format("Cannot move {} -> {}: {}",
                file.string(), dest.string(), ec.message()));
            fs::remove(file, ec);
        }

        logger->info("Organized: {} -> {}", file.filename().string(), dest.string());
        return Result::OK(std::move(dest));
    }

} // namespace media_handler::utils