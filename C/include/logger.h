#include <libavformat/avformat.h>

#ifndef LOG_H
#define LOG_H
//Log levels
typedef enum{
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_DEBUG
} LogLevel;

#define LOG_FILE "Log.log" //Log file name (and path!)

void init_logger(const char* filename); 
#ifdef __cplusplus
extern "C" {
#endif

void log_message(LogLevel level, const char *format, ...);

#ifdef __cplusplus
}
#endif
void close_logger(void);
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

#endif