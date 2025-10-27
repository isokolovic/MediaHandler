#include "file_handler.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

/// @brief Main function for the migration
/// @param source_folder Folder containing original media
/// @param destination_folder Folder where compressed media will be put 
void run_media_migration(const char* root_source, const char* source_folder, const char* destination_folder, int* processed, int* failed) {
    log_message(LOG_INFO, "Starting media migration with source: %s and destination %s", source_folder, destination_folder);     
#ifdef _WIN32
    char search_path[1024]; 
    snprintf(search_path, sizeof(search_path), "%s\\*.*", source_folder);

    WIN32_FIND_DATA find_data;
    HANDLE search_handle = FindFirstFile(search_path, &find_data); 
    if (search_handle == INVALID_HANDLE_VALUE) {
        log_message(LOG_ERROR, "Failed to open a directory %s: %lu", source_folder, GetLastError()); 
        return;
    }

    do {
        //Skip current (.) and parent (..) directories -> infinite recursion
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) continue;

        char full_path[1024]; 
        snprintf(full_path, sizeof(full_path), "%s\\%s", source_folder, find_data.cFileName); 

        struct stat st; 
        if (stat(full_path, &st) != 0) {
            log_message(LOG_ERROR, "Failed to stat %s: %s", full_path, strerror(errno));
        }

        //If a directory, recursively process subdirectories.
        //Else create dir if needed and process the file
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            run_media_migration(root_source, full_path, destination_folder, processed, failed);
        } else if (is_file_type_valid(find_data.cFileName)){
            log_message(LOG_INFO, "Processing file: %s", find_data.cFileName);
            
            //Increment processed files number
            (*processed)++;
            if(!create_folder_process_file(root_source, destination_folder, full_path)) (*failed)++;      
        }

    } while (FindNextFile(search_handle, &find_data)); 

    FindClose(search_handle);
#else    
    DIR* dir = opendir(source_folder);
    if (!dir) {
        log_message(LOG_ERROR, "Failed to open directory %s: %s", source_folder, strerror(errno));
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", source_folder, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0) {
            log_message(LOG_ERROR, "Failed to stat %s: %s", full_path, strerror(errno));
            (*failed)++;
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            run_media_migration(root_source, full_path, destination_folder, processed, failed);
        }
        else if (is_file_type_valid(entry->d_name)) {
            (*processed)++;
            log_message(LOG_INFO, "Processing file: %s", entry->d_name);
            if (!create_folder_process_file(root_source, destination_folder, full_path)) {
                (*failed)++;
            }
        }
    }

    closedir(dir);
#endif
}

//TODO If any other argument is added? Will it show prompt for source and target folders? -> Fix
//TODO Save source_folder and target_folder to log file and extract them in -r mode (not to show prompt)
//TODO organize mode - implement fallback for the lowes possible date (if created > modified: use modified)
int main(int argc, char* argv[]) {
    init_logger(LOG_FILE); //TODO check if log is cleaned when -r is run

    char source_folder[1024];
    char destination_folder[1024];

    //Processed files counters: 
    int processed = 0; 
    int failed = 0; 

    if (get_valid_directory("Enter the path to the source folder: ", source_folder, sizeof(source_folder)) != 0) {
        close_logger();
        return 1;
    }
    if (get_valid_directory("Enter the path to the destination folder: ", destination_folder, sizeof(destination_folder)) != 0) {
        close_logger();
        return 1;
    }

    //Retry and organize modes
    int retry_mode = 0;
    int organize_mode = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0) {
            retry_mode = 1;
            log_message(LOG_INFO, "Running in retry mode: Processing only failed files from " LOG_FILE);
            break;
        }
        if (strcmp(argv[i], "-o") == 0) {
            organize_mode = 1;
            log_message(LOG_INFO, "Running in file organization mode");
            break;
        }
    }
        
    if (retry_mode) {
        int num_failed = 0;
        char** failed_files = get_failed_files_from_log(LOG_FILE, &num_failed);

        if (num_failed > 0 && failed_files) {
            retry_failed_files(source_folder, destination_folder, failed_files, num_failed, &processed, &failed);
            
            // Free the list
            for (int i = 0; i < num_failed; i++) {
                free(failed_files[i]);
            }
            free(failed_files);
        }
        else {
            log_message(LOG_WARNING, "No failed files found in " LOG_FILE);
        }
    }
    else if (organize_mode) {
        organize_files(source_folder, source_folder, destination_folder, &processed, &failed);
    }
    else {
        run_media_migration(source_folder, source_folder, destination_folder, &processed, &failed); 
    }

    close_logger();
    log_summary(processed, failed);

    return 0;
}