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

    //Prompt for a source folder
    printf("Enter the path to the source folder: ");
    if(fgets(source_folder, sizeof(source_folder), stdin) == NULL){
        log_message(LOG_ERROR, "Failed to read the source folder path. ");
        close_logger();
        return 1;        
    }

    //Set value of first newline character to '\0' (remove newline)
    source_folder[strcspn(source_folder, "\n")] = 0; 

    //Validate the source folder
    if(!is_directory(source_folder)){
        log_message(LOG_ERROR, "Source folder %s does not exist", source_folder);
        close_logger(); 
        return 1;
    }

    //Prompt for a destination folder
    

}