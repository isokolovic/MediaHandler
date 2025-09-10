#include <libavformat/avformat.h>

#ifndef LOG_H
#define LOG_H
typedef enum{
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_DEBUG
} LogLevel;

void init_logger(const char* filename); 
void log_message(LogLevel level, const char* format, ...); 
void close_logger(void); 

/// @brief Function to capture errors during video compression
static void ffmpeg_log_callback(void* ptr, int level, const char* fmt, va_list vl) {
    if (level <= AV_LOG_ERROR) {
        char buf[1024];
        vsnprintf(buf, sizeof(buf), fmt, vl);
        log_message(LOG_ERROR, "FFmpeg: %s", buf);
    }
}
#endif