#include "file_handler.h"
#include "logger.h"
#include <stdio.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <Windows.h>

/// @brief Check if provided path is a directory for Windows OS
/// @param path Path provided by the user
/// @return 1 if the path is a directory, 0 otherwise
bool is_directory(const char* path){
    DWORD attrs = GetFileAttributesA(path);
    return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

#else
#include <sys/types.h>
#include <sys/stat.h>

/// @brief Check if provided path is a directory for Linux OS
/// @param path Path provided by the user
/// @return 1 if the path is a directory, 0 otherwise
bool is_directory(const char* path){
    struct stat st;
    if(stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}
#endif

/// @brief Check if source / destination folders are found upon user entry
/// @param folder Path to the source / destination folder
int get_valid_directory(const char* prompt, char* folder, size_t size){    
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