#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

static FILE* log_file = NULL;

/// @brief Closes the log file
/// @param No param 
void close_logger(void){
    if(log_file){
        fclose(log_file);
        log_file = NULL;
    }
}

/// @brief Opens the log file and prepares the logger for use. 
/// @param filename Path to the file
void init_logger(const char* filename) {
    log_file = fopen(filename, "a");
    if (!log_file) {
        fprintf("Failed to open the log file %s\n", filename);
    }
}

/// @brief Handle messages logging
/// @param level Info / Warning / Error
/// @param format Custom message
void log_message(LogLevel level, const char* format, ...){
    if (!log_file) return;

    const char* level_str;

    //Convert message level to human readable format
    switch (level) {
        case LOG_INFO: level_str = "INFO"; break; 
        case LOG_WARNING: level_str = "WARNING"; break;
        case LOG_ERROR: level_str = "ERROR"; break;
        case LOG_DEBUG: level_str = "DEBUG"; break;
        default: level_str = "UNKNOWN"; break;
    }

    //Convert time to human readable
    time_t now = time(NULL); 
    struct tm* tm = localtime(&now); 
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm);

    //Save log messages to the log file
    fprintf(log_file, "%s - %s - ", time_str, level_str);
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args); 
    fprintf(log_file, "\n"); 
    fflush(log_file); 

    //Log messages to the console
    printf("%s - %s - ", time_str, level_str);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}