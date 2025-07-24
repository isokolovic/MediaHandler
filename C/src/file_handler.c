#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "file_handler.h"
#include "logger.h"

#ifdef _WIN32
#include <string.h>
#include <direct.h>
#include <Windows.h>
#define STAT_STRUCT struct _stat
#define STAT_FUNC _stat
#define strcasecmp _stricmp
#define mkdir _mkdir
#else
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#define STAT_STRUCT struct stat
#define STAT_FUNC stat
#define mkdir(path) mkdir(path, 0755)
#endif

static const char* image_extensions[] = { ".jpg", ".jpeg", ".heic", ".png", ".bmp" };
static const char* video_extensions[] = { ".mp4", ".avi", ".mov", ".3gp" };
static const char* other_extensions[] = { ".gif", ".mp3" };
//Think twice before modifying (. = file extension, " " used in folder naming, etc.) 
static const char* special_chars = " %:/,\\{}~[]<>*?čćžđšČĆŽŠĐ";
static const char* folder_special_chars = "%:/,.\\{}~[]<>*?čćžđšČĆŽŠĐ";

#ifdef _WIN32
/// @brief Check if provided path is a directory for Windows OS
/// @param path Path provided by the user
/// @return 1 if the path is a directory, 0 otherwise
bool is_directory(const char* path){
    DWORD attrs = GetFileAttributesA(path);
    return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}
#else
/// @brief Check if provided path is a directory for Linux OS
/// @param path Path provided by the user
/// @return 1 if the path is a directory, 0 otherwise
bool is_directory(const char* path){
    struct stat st;
    if(stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}
#endif

/// @brief Copy file from the source to the destination
/// @param source Copy source
/// @param destination Copy destination
/// @return True if copying is successful
bool copy_file(const char* source, const char* destination) {
    FILE* source_file = fopen(source, "rb"); 
    if (!source_file) {
        log_message(LOG_ERROR, "Failed to open the file: %s", source, strerror(errno));
        return false;
    }

    FILE* destination_file = fopen(destination, "wb");
    if (!destination_file) {
        log_message(LOG_ERROR, "Failed to open the file: %s", destination, strerror(errno));
        return false;
    }

#ifndef _WIN32
    if (chmod(fileno(destination_file), 0644) != 0) {
        log_message(LOG_WARNING, "Failed to set the permission for %s", destination, strerror(errno));
        //Try even without setting the permission
    }
#endif

    //Copy the file
    char buffer[8192];  //8KB temporary buffer to read from and write to in chunks
    size_t bytes;
    bool success = true;
    while ((bytes = fread(buffer, 1, sizeof(buffer), source_file)) > 0) {
        if (fwrite(buffer, 1, bytes, destination_file) != bytes) {
            log_message(LOG_ERROR, "Failed to write to %s: %s", destination, strerror(errno));
            success = false;
            break;
        }
    }

    if (ferror(source_file)) {
        log_message(LOG_ERROR, "Error reading from %s: %s", source, strerror(errno));
        success = false;
    }
    if (fclose(source_file) != 0) {
        log_message(LOG_ERROR, "Failed to close source %s: %s", source, strerror(errno)); 
        success = false;
    }
    if (fclose(destination_file) != 0) {
        log_message(LOG_ERROR, "Failed to close destination %s: %s", destination, strerror(errno));
        success = false;
    }

    return success;
}

/// @brief Create a directory
/// @param path Directory to be created path
/// @return True if successful
bool create_directory(const char* path)
{
    if (mkdir(path) == 0 || errno == EEXIST) return true;
    log_message(LOG_ERROR, "Failed to create directory %s: %s", path, strerror(errno)); 
    return false;
}

/// @brief Check if file exist on specified path
/// @param path Path to a file
/// @return True if file exists
bool file_exists(const char* path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

/// @brief Clean file / folder name of any special characters
/// @param element Element to be cleaned
/// @return Cleaned name, NULL if error, "Unnamed" if strlen(cleaned_element) == 0
char* clean_name(const char* element, bool is_directory )
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
        if (is_directory && !strchr(folder_special_chars, ch)) {
            cleaned_name[j++] = ch;
        }
        else if (!strchr(special_chars, ch)) {
            cleaned_name[j++] = ch;
        }
    }
    cleaned_name[j] = '\0'; 

    if (strlen(cleaned_name) == 0) {
        free(cleaned_name); 
        cleaned_name = strdup("Unnamed"); 
        if (!cleaned_name) {
            log_message(LOG_ERROR, "Memory allocation failed for clean_name");
            return NULL;
        }
    }
    return cleaned_name;
}

/// @brief Extract relative path (subfolder)
/// @param file_path Full file path
/// @return Subfolder
char* extract_relative_dir(const char* source_path, const char* file_path)
{
    if (!source_path || !file_path) {
        log_message(LOG_ERROR, "Null parameter in extract_relative_dir");
        return NULL;
    }

    //Search from end of the string and find a separator
    char* last_step = strrchr(file_path, '\\');
    if (!last_step) last_step = strrchr(file_path, ' / ');
    if (!last_step) {
        log_message(LOG_ERROR, "No subdirectory path in %s, returning empty path.");
        return strdup(""); 
    }

    size_t dir_len = last_step - file_path; 
    char* file_directory = malloc(dir_len + 1); 
    if (!file_directory) {
        log_message(LOG_ERROR, "Memory allocation for file_directory failed.");
        return strdup("");
    }
#ifdef _WIN32
    strncpy_s(file_directory, dir_len + 1, file_path, dir_len); 
#else
    strncpy(file_directory, file_path, dir_len); 
    file_directory[dir_len] = '\0';
#endif

    size_t source_dir_len = strlen(source_path);
    size_t folder_len = strlen(file_directory); 
    const char* relative_path = ""; 
    //Check if file directory starts with the same path as source directory
    if (strncmp(file_directory, source_path, source_dir_len) == 0 && folder_len > source_dir_len) {
        //Move the pointer forward past the base directory to get the remaining path
        relative_path = file_directory + source_dir_len;
        //If relative path starts with slashes, move pointer up by one byte (sizeof(char)) to clean slashes
        if (relative_path[0] == '/' || relative_path[0] == "\\") {
            relative_path++; 
        }
    }

    //Extract first subfolder
    char* next_step = strchr(relative_path, '\\'); 
    if (!next_step) next_step = strchr(relative_path, '/');
    
    size_t subfolder_length = next_step ? (size_t)(next_step - relative_path) : strlen(relative_path); 
    char* first_subfolder = malloc(subfolder_length + 1); 
    if (!first_subfolder) {
        log_message(LOG_ERROR, "Memory allocation for first_subfolder failed"); 
        free(file_directory); 
        return strdup(""); 
    }
#ifdef _WIN32
    strncpy_s(first_subfolder, subfolder_length + 1, relative_path, subfolder_length);
#else
    strncpy(first_subfolder, relative_path, subfolder_length); 
    first_subfolder[subfolder_length] = '\0'; 
#endif

    //Clean subfolder of special characters and return
    char* cleaned_subfolder = clean_name(relative_path, true); 
    if (!cleaned_subfolder) {
        log_message(LOG_ERROR, "Failed to create relative path %s", relative_path);
        free(file_directory); 
        free(first_subfolder); 
        return strdup("");
    }

    free(file_directory);
    free(first_subfolder);
    return cleaned_subfolder;
}

/// @brief Get file size 
/// @param path Path to a file
/// @return File size
long get_file_size(const char* path)
{
    STAT_STRUCT st;
    if (STAT_FUNC(path, &st) == 0) return (long long)st.st_size;
    
    log_message(LOG_ERROR, "File %s doesn't exist", path, strerror(errno));
    return -1;
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

    //Validate folder location
    if (!is_directory(folder)) {
        log_message(LOG_ERROR, "Folder %s does not exist", folder);
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