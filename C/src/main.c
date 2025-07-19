#include "file_handler.h"
#include "logger.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <dirent.h>
#endif


/// @brief Main function for the migration
/// @param source_folder Folder containing original media
/// @param destination_folder Folder where compressed media will be put 
void run_media_migration(const char* source_folder, const char* destination_folder) {
    log_message(LOG_INFO, "Starting media migration with source: %s and destination %s", source_folder, destination_folder); 

    #ifdef _WIN32
    char search_path[1024]; 
    snprintf(search_path, sizeof(search_path), "%s\\*.*", source_folder);

    WIN32_FIND_DATA find_data;




    #else




    #endif

}

int main(void){
    init_logger("Migration_log.log");

    char source_folder[1024];
    char destination_folder[1024];

    if (get_valid_directory("Enter the path to the source folder: ", source_folder, sizeof(source_folder)) != 0) {
        close_logger();
        return 1;
    }

    if (get_valid_directory("Enter the path to the destination folder: ", destination_folder, sizeof(destination_folder)) != 0) {
        close_logger();
        return 1;
    }

    run_media_migration(source_folder, destination_folder); 
    close_logger(); 
    return 0;
}