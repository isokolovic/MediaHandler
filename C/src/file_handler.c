#include "file_handler.h"
#include "logger.h"
#include <stdio.h>

/// @brief Check if source / destination folders are OK upon user entry
/// @param folder Path to the source / destination folder
int get_valid_dir(const char* prompt, char* folder, size_t size){    
    //Prompt for a folder
    printf(prompt);
    if(fgets(folder, size, stdin) == NULL){
        log_message(LOG_ERROR, "Failed to read the source folder path. ");
        close_logger();
        return 1;        
    }

    //Set value of first newline character to '\0' (remove newline)
    folder[strcspn(folder, "\n")] = 0; 

    //Validate the source folder
    if(!is_directory(folder)){
        log_message(LOG_ERROR, "Source folder %s does not exist", folder);
        close_logger(); 
        return 1;
    }

    return 0;
}