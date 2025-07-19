#include "logger.h"
#include <stdio.h>

static FILE* log_file = NULL;

/// @brief Opens the log file and prepares the logger for use. 
/// @param filename Path to the file
void init_logger(const char* filename){
    log_file = fopen(filename, "a");
    if(!log_file){
        fprintf("Failed to open the log file %s\n", filename);
    }
}

/// @brief Closes the log file
/// @param No param 
void close_logger(void){
    if(log_file){
        fclose(log_file);
        log_file = NULL;
    }
}

void log_message(LogLevel level, const char* format, ...){
    
}