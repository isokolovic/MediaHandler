#include "media_migration.h"

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
        }
        else if (is_file_type_valid(find_data.cFileName)) {
            log_message(LOG_INFO, "Processing file: %s", find_data.cFileName);

            //Increment processed files number
            (*processed)++;
            if (!create_folder_process_file(root_source, destination_folder, full_path)) (*failed)++;
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