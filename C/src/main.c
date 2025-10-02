#include "file_handler.h"
#include "logger.h"
#include <stdio.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <dirent.h>
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

        //If a directory, recursibely process subdirectories.
        //Else create dir if needed and process the file
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            run_media_migration(root_source, full_path, destination_folder, processed, failed);
        } else if (is_file_type_valid(find_data.cFileName)){
            log_message(LOG_INFO, "Processing file: %s", find_data.cFileName);
            
            if (create_folder_process_file(root_source, destination_folder, full_path)) {
                (*processed)++;
            }
            else {
                (*processed)++;
                (*failed)++;
            }            
        }

    } while (FindNextFile(search_handle, &find_data)); 

    FindClose(search_handle);
#else
    //TODO Linux implementation
#endif
}

int main(void){
    init_logger("Log.log");

    char source_folder[1024];
    char destination_folder[1024];

    //Processed files counters: 
    int processed = 0; 
    int failed = 0; 

    //if (get_valid_directory("Enter the path to the source folder: ", source_folder, sizeof(source_folder)) != 0) {
    //    close_logger();
    //    return 1;
    //}
    //if (get_valid_directory("Enter the path to the destination folder: ", destination_folder, sizeof(destination_folder)) != 0) {
    //    close_logger();
    //    return 1;
    //}

    strcpy(source_folder, "C:\\Users\\isoko\\Desktop\\New folder\\S");
    strcpy(destination_folder, "C:\\Users\\isoko\\Desktop\\New folder\\D");

    run_media_migration(source_folder, source_folder, destination_folder, &processed, &failed); 
    close_logger(); 

    //Print summary to stdout
    printf("\n======= PROCESSING SUMMARY =======\n");
    printf("Processed %d files. Failed: %d.\n", processed, failed);
    printf("To retry compression for failed files, run program with -r argument: \n\n");
    printf(".\media_migration.exe -r OR ./media_migration.out -r. \n\n");
    printf("==================================\n\n");

    return 0;
}