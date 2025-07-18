#include "file_handler.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <dirent.h>
#endif

int main(void){
    init_logger("Migration_log.log");

    char source_folder[1024];
    char dest_folder[1024];

    if (get_valid_directory("Enter the path to the source folder: ", source_folder, sizeof(source_folder)) != 0) {
        close_logger();
        return 1;
    }

    if (get_valid_directory("Enter the path to the destination folder: ", dest_folder, sizeof(dest_folder)) != 0) {
        close_logger();
        return 1;
    }
   


}