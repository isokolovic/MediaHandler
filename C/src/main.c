#include "media_migration.h"

//TODO If any other argument is added? Will it show prompt for source and target folders? -> Fix
//TODO Save source_folder and target_folder to log file and extract them in -r mode (not to show prompt)
//TODO organize mode - implement fallback for the lowes possible date (if created > modified: use modified)
int main(int argc, char* argv[]) {
    init_logger(LOG_FILE_PATH); //TODO check if log is cleaned when -r is run

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
            log_message(LOG_INFO, "Running in retry mode: Processing only failed files from " LOG_FILE_PATH);
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
        char** failed_files = get_failed_files_from_log(LOG_FILE_PATH, &num_failed);

        if (num_failed > 0 && failed_files) {
            retry_failed_files(source_folder, destination_folder, failed_files, num_failed, &processed, &failed);
            
            // Free the list
            for (int i = 0; i < num_failed; i++) {
                free(failed_files[i]);
            }
            free(failed_files);
        }
        else {
            log_message(LOG_WARNING, "No failed files found in " LOG_FILE_PATH);
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