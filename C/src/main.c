#include "media_migration.h"

int main(int argc, char* argv[]) {
    bool retry_mode = false;
    bool organize_mode = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0) {
            retry_mode = true;
            break;
        }
        if (strcmp(argv[i], "-o") == 0) {
            organize_mode = true;
            break;
        }
    }

    init_logger(LOG_FILE_PATH, retry_mode ? "a" : "r");

    if (argc > 1 && !retry_mode && !organize_mode) {
        log_message(LOG_ERROR, "Invalid argument: %s. Use -r for retry or -o for organize.", argv[1]);
        close_logger();
        return 1;
    }

    char source_folder[1024];
    char destination_folder[1024];

    // Prompt for folders only if not in retry mode
    if (!retry_mode) {
        if (get_valid_directory("Enter the path to the source folder: ", source_folder, sizeof(source_folder)) != 0) {
            close_logger();
            return 1;
        }
        if (get_valid_directory("Enter the path to the destination folder: ", destination_folder, sizeof(destination_folder)) != 0) {
            close_logger();
            return 1;
        }

        // Log paths for reuse in retry mode
        log_message(LOG_INFO, "SOURCE_DIR=%s", source_folder);
        log_message(LOG_INFO, "TARGET_DIR=%s", destination_folder);
    }
    else {
        // Extract paths from log file in retry mode
        if (!extract_path_from_log("SOURCE_DIR=", source_folder, sizeof(source_folder)) ||
            !extract_path_from_log("TARGET_DIR=", destination_folder, sizeof(destination_folder))) {
            log_message(LOG_ERROR, "Failed to extract source or target folder from log file");
            close_logger();
            return 1;
        }
    }

    //Processed files counters: 
    int processed = 0; 
    int failed = 0; 
        
    if (retry_mode) {
        int num_failed = 0;
        char** failed_files = get_failed_files_from_log(LOG_FILE_PATH, &num_failed);

        if (num_failed > 0 && failed_files) {
            //Clean log before retry
            FILE* log = fopen(LOG_FILE_PATH, "w");
            if (log) fclose(log);

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