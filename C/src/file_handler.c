#include "file_handler.h"
#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char* image_extensions[] = { ".jpg", ".jpeg", ".heic", ".png", ".bmp" };
static const char* video_extensions[] = { ".mp4", ".avi", ".mov", ".3gp" };
static const char* other_extensions[] = { ".gif", ".mp3" };
static const char* special_chars = " %:/,.\\{}~[]<>*?čćžđšČĆŽŠĐ";

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

/// @brief Create a directory
/// @param path Directory to be created path
/// @return True if successful
bool create_folder(const char* path)
{
#ifdef _WIN32
    if (mkdir(path) == 0 || errno == EEXIST) return true;
#else
    if (mkdir(path, 0755) == 0 || errno == EEXIST) return true;
#endif
    log_message(LOG_ERROR, "Failed to create directory %s: %s", path, strerror(errno));
    return false;
}

/// @brief Clean file / folder name of any special characters
/// @param element Element to be cleaned
/// @return Cleaned name, NULL if error, "Unnamed" strlen(cleaned_element) == 0
char* clean_name(const char* element)
{
    size_t length = strlen(element); 
    char* cleaned_name = malloc(length + 1); 
    if (!cleaned_name) {
        log_message(LOG_ERROR, "Memory allocation failed for clean_name");
        return NULL;
    }

    //Clean of special characters
    size_t j = 0; 
    for (size_t i = 0; i < length; i++) {
        char ch = element[i]; 
        if (!strchr(special_chars, ch)) {
            cleaned_name[j++] = ch;
        }
    }
    cleaned_name[j] = '\0'; 

    if (strlen(cleaned_name) == 0) {
        free(cleaned_name); 
        cleaned_name = strdump("Unnamed"); 
        if (!cleaned_name) {
            log_message(LOG_ERROR, "Memory allocation failed for clean_name");
            return NULL;
        }
    }
    return cleaned_name;
}

/// @brief Check if source / destination folders are found upon user entry
/// @param folder Path to the source / destination folder
int get_valid_directory(const char* prompt, char* folder, size_t size) {
    //Prompt for a folder
    printf(prompt);
    if (fgets(folder, size, stdin) == NULL) {
        log_message(LOG_ERROR, "Failed to read the source folder path. ");
        close_logger();
        return 1;
    }

    //Set value of first newline character to '\0' (remove newline)
    folder[strcspn(folder, "\n")] = 0;

    //Validate the source folder
    if (!is_directory(folder)) {
        log_message(LOG_ERROR, "Source folder %s does not exist", folder);
        close_logger();
        return 1;
    }

    return 0;
}


/// @brief Check if file needs to be processed
/// @param filename File path
/// @return True if file needs to be processed
bool is_file_type_valid(const char* filename) {
    //Strip filename for the extension
    char* extension = strrchr(filename, '.');
    if (!extension) return false;

    //Iterate through extensions and find possible match 
    for (size_t i = 0; sizeof(image_extensions) / sizeof(image_extensions[0]); i++) {
        if (strcasecmp(extension, image_extensions[i]) == 0) return true;
    }
    for (size_t i = 0; sizeof(video_extensions) / sizeof(video_extensions[0]); i++) {
        if (strcasecmp(extension, video_extensions[i]) == 0) return true;
    }
    for (size_t i = 0; sizeof(other_extensions) / sizeof(other_extensions[0]); i++) {
        if (strcasecmp(extension, other_extensions[i]) == 0) return true;
    }
    return false;
}