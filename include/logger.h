#include <libavformat/avformat.h>

#ifndef LOGGER
#define LOGGER

#ifdef __cplusplus
extern "C" {
#endif

//Log levels
typedef enum{
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_DEBUG
} LogLevel;

#define LOG_FILE_PATH "Log.log" //Log file name (and path!)

/// @brief Opens the log file and prepares the logger for use. 
/// @param filename Path to the file
/// @param mode "a" (preserve log) or "r" (clean log)
void init_logger(const char* filename, const char* mode);

/// @brief Handle messages logging
/// @param level Info / Warning / Error
/// @param format Custom message
void log_message(LogLevel level, const char *format, ...);

/// @brief Closes the log file
/// @param No param 
void close_logger(void);

/// @brief Log the file processing status - passed or failed (if user wants to retry failed compressions)
/// @param file_path File path
/// @param success Compression status
void log_file_processing(const char* file_path, int success);

/// @brief Function to capture errors during video compression
static void ffmpeg_log_callback(void* ptr, int level, const char* fmt, va_list vl) {
    if (level <= AV_LOG_ERROR) {
        char buf[1024];
        vsnprintf(buf, sizeof(buf), fmt, vl);
        log_message(LOG_ERROR, "FFmpeg: %s", buf);
    }
}

/// @brief Final log 
static void log_summary(int processed, int failed) {
    printf("\n======= PROCESSING SUMMARY =======\n");
    printf("Processed %d files. Failed: %d.\n", processed, failed);
    printf("To retry compression for failed files, run program with -r argument: \n\n");
    printf(".\media_migration.exe -r OR ./media_migration.out -r. \n\n");
    printf("==================================\n\n");
}

#ifdef __cplusplus
}
#endif

#endif //LOGGER