#include "file_handler.h"
#include "image_compressor.h"
#include "video_compressor.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <string.h>
#include <direct.h>
#include <Windows.h>
#define STAT_STRUCT struct _stat
#define STAT_FUNC _stat
#define mkdir _mkdir
#define PATH_SEPARATOR '\\'
#else
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#define STAT_STRUCT struct stat
#define STAT_FUNC stat
#define mkdir(path) mkdir(path, 0755)
#define PATH_SEPARATOR '/'
#endif

#define PATH_MAX 4096

static const char* image_extensions[] = { ".jpg", ".jpeg", ".heic", ".png", ".bmp" };
static const char* video_extensions[] = { ".mp4", ".avi", ".mov"/*, ".3gp"*/ }; //3gp not supported currently (encoder compatibility)
static const char* other_extensions[] = { ".gif", ".mp3", ".3gp"};
//Think twice before modifying (. = file extension, " " used in folder naming, etc.) 
static const char* special_chars = " %:/,\\{}~[]<>*?čćžđšČĆŽŠĐ";
static const char* folder_special_chars = "%:/,.\\{}~[]<>*?čćžđšČĆŽŠĐ";

#ifdef _WIN32
bool is_directory(const char* path){
    DWORD attrs = GetFileAttributesA(path);
    return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}
#else
bool is_directory(const char* path){
    struct stat st;
    if(stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}
#endif

bool is_image(const char* filename) {
    const char* extension = strrchr(filename, '.');
    if (!extension) return false;
    for (size_t i = 0; i < sizeof(image_extensions) / sizeof(image_extensions[0]); i++) {
        if (strcasecmp(extension, image_extensions[i]) == 0) return true;
    }
    return false;
}

bool is_video(const char* filename) {
    const char* extension = strrchr(filename, '.');
    if (!extension) return false;
    for (size_t i = 0; i < sizeof(video_extensions) / sizeof(video_extensions[0]); i++) {
        if (strcasecmp(extension, video_extensions[i]) == 0) return true;
    }
    return false;
}

bool is_gif_misc(const char* filename) {
    const char* extension = strrchr(filename, '.');
    if (!extension) return false;
    for (size_t i = 0; i < sizeof(other_extensions) / sizeof(other_extensions[0]); i++) {
        if (strcasecmp(extension, other_extensions[i]) == 0) return true;
    }
    return false;
}

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

bool create_directory(const char* path)
{
    if (mkdir(path) == 0 || errno == EEXIST) return true;
    log_message(LOG_ERROR, "Failed to create directory %s: %s", path, strerror(errno)); 
    return false;

    char temp[1024]; 
    strncpy(temp, path, sizeof(temp) - 1); 
    temp[sizeof(temp) - 1] = '\0';

#ifdef _WIN32
    for (char* p = temp; *p; p++) if (*p == '/') *p = '\\';
#else
    for (char* p = temp; *p; p++) if (*p == '//') *p = '/';
#endif

    char* p = temp; 
    if (p[0] == '/' || p[0] == '\\') p++; 
    while (*p) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            if (strlen(temp) > 0) {
                if (mkdir(temp) != 0 && errno != EEXIST) {
                    log_message(LOG_ERROR, "Failed to create directory %s: %s", temp, strerror(errno));
                    return false;
                }
            }
            *p = '/';
        }
        p++;
    }
    if (mkdir(temp) != 0 && errno != EEXIST) {
        log_message(LOG_ERROR, "Failed to create directory %s: %s", temp, strerror(errno));
        return false;
    }
    return true;
}

bool file_exists(const char* path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

char* clean_name(const char* element, bool is_directory )
{
    if (!element) {
        log_message(LOG_ERROR, "NULL element in clean_name");
        return NULL;
    }

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
        if (is_directory) {
            return strdup(""); //Empty string for directories
        }
        else {
            cleaned_name = strdup(EMPTY_FILENAME); //Unnnamed files
            if (!cleaned_name) {
                log_message(LOG_ERROR, "Memory allocation failed for clean_name");
                return NULL;
            }
        }
        return cleaned_name;
    }
    return cleaned_name;
}

char* extract_relative_dir(const char* source_path, const char* file_path)
{
    if (!source_path || !file_path) {
        log_message(LOG_ERROR, "Null parameter in extract_relative_dir");
        return NULL;
    }

    char norm_source[PATH_MAX];
    char norm_file[PATH_MAX];

    // Safe copy
    strncpy(norm_source, source_path, sizeof(norm_source) - 1);
    norm_source[sizeof(norm_source) - 1] = '\0';
    strncpy(norm_file, file_path, sizeof(norm_file) - 1);
    norm_file[sizeof(norm_file) - 1] = '\0';

    for (char* p = norm_source; *p; p++) {
        if (*p == '/' || *p == '\\') *p = PATH_SEPARATOR;
    }
    for (char* p = norm_file; *p; p++) {
        if (*p == '/' || *p == '\\') *p = PATH_SEPARATOR;
    }

    // Ensure source ends with separator
    size_t slen = strlen(norm_source);
    if (slen > 0 && norm_source[slen - 1] != PATH_SEPARATOR) {
        if (slen + 1 < sizeof(norm_source)) {
            norm_source[slen] = PATH_SEPARATOR;
            norm_source[slen + 1] = '\0';
            slen++;
        }
    }

    // Must match prefix
    if (strncmp(norm_file, norm_source, slen) != 0) {
        log_message(LOG_WARNING, "File not under source: %s vs %s", norm_file, norm_source);
        return strdup("");
    }

    const char* rel_part = norm_file + slen;

    // Copy relative part
    char* result = strdup(rel_part);
    if (!result) {
        log_message(LOG_ERROR, "strdup failed in extract_relative_dir");
        return NULL;
    }

    // Remove filename: find last separator and truncate
    char* last_sep = strrchr(result, PATH_SEPARATOR);
    if (last_sep) {
        *last_sep = '\0';
    }
    else {
        // No path — file is in root
        result[0] = '\0';
    }

    return result;
}

long get_file_size(const char* path)
{
    STAT_STRUCT st;
    if (STAT_FUNC(path, &st) == 0) return (long long)st.st_size;
    
    log_message(LOG_ERROR, "File %s doesn't exist", path, strerror(errno));
    return -1;
}

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

bool is_file_type_valid(const char* filename) {
    return is_image(filename) || is_video(filename) || is_gif_misc(filename);
}

bool compress_file(const char* file, const char* output_file) {
    const char* extension = strrchr(file, '.');

    bool status = false; //File processing status

    if (!file || !output_file) {
        log_message(LOG_ERROR, "Null input or output path in compress_file");                
    }
    else if (!extension) {
        log_message(LOG_ERROR, "No extension found for %s", file);        
    }
    else if (is_image(file)) {
        status = compress_image(file, output_file, extension);        
    }
    else if (is_video(file)) {
        status = compress_video(file, output_file);
    }
    else {
        status = true; //Only copy other file types
    }
    log_file_processing(file, status); 
    return status;
}

char** get_failed_files_from_log(const char* log_path, int* num_failed) {
    FILE* log = fopen(log_path, "r");
    if (!log) {
        *num_failed = 0;
        log_message(LOG_ERROR, "Log file not found!");
        return NULL;
    }

    char** failed_files = NULL;
    int count = 0;
    char line[1024];

    while (fgets(line, sizeof(line), log)) {
        const char* marker = "File Processing: ";
        const char* status_marker = " - FAILED";

        char* start = strstr(line, marker);
        if (!start) continue;
        start += strlen(marker);

        char* end = strstr(start, status_marker);
        if (!end) continue;

        size_t len = end - start;
        if (len <= 0 || len >= sizeof(line)) continue;

        char file_path[512];
        memcpy(file_path, start, len);
        file_path[len] = '\0';

        // Extract just the filename from full path
        const char* name = strrchr(file_path, '/');
#ifdef _WIN32
        if (!name) name = strrchr(file_path, '\\');
#endif
        name = name ? name + 1 : file_path;

        char* new_entry = strdup(name);
        if (!new_entry) {
            log_message(LOG_ERROR, "Failed to strdup for failed file name");
            for (int i = 0; i < count; ++i) free(failed_files[i]);
            free(failed_files);
            *num_failed = 0;
            fclose(log);
            return NULL;
        }

        char** temp = realloc(failed_files, (count + 1) * sizeof(char*));
        if (!temp) {
            log_message(LOG_ERROR, "Failed to realloc for failed files array");
            free(new_entry);
            for (int i = 0; i < count; ++i) free(failed_files[i]);
            free(failed_files);
            *num_failed = 0;
            fclose(log);
            return NULL;
        }

        failed_files = temp;
        failed_files[count++] = new_entry;
    }

    fclose(log);
    *num_failed = count;
    return failed_files;
}

void retry_failed_files(const char* root_source, const char* destination_folder, char** failed_files, int num_failed, int* total_processed, int* total_failed)
{
    char full_path[PATH_MAX];
    for (int i = 0; i < num_failed; i++) {
        // Build full source path
        size_t root_len = strlen(root_source);
        int needs_sep = (root_len > 0 && root_source[root_len - 1] != PATH_SEPARATOR);
        if (needs_sep)
            snprintf(full_path, sizeof(full_path), "%s%c%s", root_source, PATH_SEPARATOR, failed_files[i]);
        else
            snprintf(full_path, sizeof(full_path), "%s%s", root_source, failed_files[i]);

        log_message(LOG_INFO, "Retrying file: %s", full_path);
        (*total_processed)++;

        if (!create_folder_process_file(root_source, destination_folder, full_path))
            (*total_failed)++;
    }
}

int get_file_creation_year(const char* path)
{
#ifdef _WIN32
    HANDLE hFile = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        log_message(LOG_ERROR, "Failed to open file %s: %lu", path, GetLastError());
        return 0;
    }

    FILETIME creation_time, modified_time;
    if (!GetFileTime(hFile, &creation_time, NULL, &modified_time)) {
        log_message(LOG_ERROR, "Failed to get file times for %s: %lu", path, GetLastError());
        CloseHandle(hFile);
        return 0;
    }
    CloseHandle(hFile);

    // Convert FILETIME to ULARGE_INTEGER for comparison
    ULARGE_INTEGER c, m;
    c.LowPart = creation_time.dwLowDateTime;
    c.HighPart = creation_time.dwHighDateTime;
    m.LowPart = modified_time.dwLowDateTime;
    m.HighPart = modified_time.dwHighDateTime;

    ULONGLONG oldest = (c.QuadPart < m.QuadPart) ? c.QuadPart : m.QuadPart;

    // Convert to time_t
    FILETIME ft;
    ULARGE_INTEGER ui;
    ui.QuadPart = oldest;
    ft.dwLowDateTime = ui.LowPart;
    ft.dwHighDateTime = ui.HighPart;

    SYSTEMTIME st;
    if (!FileTimeToSystemTime(&ft, &st)) {
        log_message(LOG_ERROR, "Failed to convert FILETIME to SYSTEMTIME for %s", path);
        return 0;
    }

    return st.wYear;
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        log_message(LOG_ERROR, "Failed to stat %s: %s", path, strerror(errno));
        return 0;
    }

    time_t oldest = st.st_mtime;
    struct tm* timeinfo = localtime(&oldest);
    if (!timeinfo) {
        log_message(LOG_ERROR, "Failed to convert timestamp for %s", path);
        return 0;
    }

    return timeinfo->tm_year + 1900;
#endif
}

int move_file_to_year_folder(const char* root_source, const char* destination_folder, const char* file_path, int year)  //TODO remove root_source?
{
    if (year <= 0) {
        log_message(LOG_ERROR, "Invalid year for %s", file_path);
        return 0;
    }

    char year_folder[1024];
    snprintf(year_folder, sizeof(year_folder), "%d", year);

    char dest_dir[1024];
#ifdef _WIN32
    snprintf(dest_dir, sizeof(dest_dir), "%s\\%s", destination_folder, year_folder);
#else
    snprintf(dest_dir, sizeof(dest_dir), "%s/%s", destination_folder, year_folder);
#endif

    if (!create_directory(dest_dir)) {
        log_message(LOG_ERROR, "Failed to create year folder %s", dest_dir);
        return 0;
    }

    const char* filename = strrchr(file_path, '\\');
    if (!filename) filename = strrchr(file_path, '/');
    if (!filename) filename = file_path;
    else filename++;  // Skip separator

    char dest_path[1024];
#ifdef _WIN32
    snprintf(dest_path, sizeof(dest_path), "%s\\%s", dest_dir, filename);
#else
    snprintf(dest_path, sizeof(dest_path), "%s/%s", dest_dir, filename);
#endif

    if (rename(file_path, dest_path) != 0) {
        log_message(LOG_ERROR, "Failed to move %s to %s: %s", file_path, dest_path, strerror(errno));
        return 0;
    }

    log_message(LOG_INFO, "Organized %s to %s/%d", file_path, destination_folder, year);
    return 1;
}

void organize_files(const char* root_source, const char* source_folder, const char* destination_folder, int* processed, int* failed)
{
    log_message(LOG_INFO, "Organizing files in %s to %s", source_folder, destination_folder);

#ifdef _WIN32
    char search_path[1024];
    snprintf(search_path, sizeof(search_path), "%s\\*.*", source_folder);

    WIN32_FIND_DATA find_data;
    HANDLE search_handle = FindFirstFile(search_path, &find_data);
    if (search_handle == INVALID_HANDLE_VALUE) {
        log_message(LOG_ERROR, "Failed to open directory %s: %lu", source_folder, GetLastError());
        return;
    }

    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) continue;

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s\\%s", source_folder, find_data.cFileName);

        struct stat st;
        if (stat(full_path, &st) != 0) {
            log_message(LOG_ERROR, "Failed to stat %s: %s", full_path, strerror(errno));
            (*failed)++;
            continue;
        }

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            organize_files(root_source, full_path, destination_folder, processed, failed);
        }
        else if (is_file_type_valid(find_data.cFileName)) {
            (*processed)++;

            int year = get_file_creation_year(full_path);
            if (!move_file_to_year_folder(root_source, destination_folder, full_path, year)) {
                (*failed)++;
            }
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
            organize_files(root_source, full_path, destination_folder, processed, failed);
        }
        else if (is_file_type_valid(entry->d_name)) {
            (*processed)++;

            int year = get_file_creation_year(full_path);
            if (!move_file_to_year_folder(root_source, destination_folder, full_path, year)) {
                (*failed)++;
            }
        }
    }

    closedir(dir);
#endif
}

bool extract_path_from_log(const char* source_target, char* out_path, size_t max_len)
{
    FILE* file = fopen(LOG_FILE_PATH, "r");
    if (!file) return false;

    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        char* match = strstr(line, source_target);
        if (match) {
            const char* value = match + strlen(source_target);
            size_t len = strcspn(value, "\r\n");
            if (len >= max_len) len = max_len - 1;

            strncpy(out_path, value, len);
            out_path[len] = '\0';
            fclose(file);
            return true;
        }
    }

    fclose(file);
    return false;
}

bool create_folder_process_file(const char* source_folder, const char* destination_folder, const char* filename)
{
    if (!source_folder || !destination_folder || !filename) {
        log_message(LOG_ERROR, "NULL parameter in create_folder_process_file");
        return false;
    }

    char* base_filename = strrchr(filename, '/');
    if (!base_filename) base_filename = strrchr(filename, '\\');
    if (!base_filename) base_filename = (char*)filename;
    else base_filename++; //Increase pointer past the separator

    char* clean_filename = clean_name(base_filename, false);
    if (!clean_filename) {
        log_message(LOG_ERROR, "Failed to clean filename %s", base_filename);
        return false;
    }

    //Extract relative path 
    char* subdirectory = extract_relative_dir(source_folder, filename);
    if (!subdirectory) {
        log_message(LOG_ERROR, "Failed to extract subdirectory for %s", filename);
        free(clean_filename); //malloc used in clean_name()
        return false;
    }

    //Create destination directory
    char destination_dir[1024];
    if (strlen(subdirectory) > 0) {
#ifdef _WIN32
        snprintf(destination_dir, sizeof(destination_dir), "%s\\%s", destination_folder, subdirectory);
#else
        snprintf(destination_dir, sizeof(destination_dir), "%s/%s", destination_folder, subdirectory);
#endif 
    }
    else {
        strncpy(destination_dir, destination_folder, sizeof(destination_dir) - 1);
        destination_dir[sizeof(destination_dir) - 1] = '\0';
    }
    free(subdirectory);

    if (!create_directory(destination_dir)) {
        log_message(LOG_ERROR, "Failed to create destination directory %s", destination_dir);
        free(clean_filename); //malloc used in clean_name()
        return false;
    }

    //Create a file path and make a copy on the destination location
    char destination_path[1024];
#ifdef _WIN32
    snprintf(destination_path, sizeof(destination_path), "%s\\%s", destination_path, clean_filename);
#else
    snprintf(destination_path, sizeof(destination_path), "%s/%s", destination_path, clean_filename);
#endif

    const char* last_step = strrchr(filename, '\\');
    if (!last_step) last_step = strrchr(filename, '/');
    const char* raw_filename = last_step ? last_step + 1 : filename;

    const char* extension = strrchr(raw_filename, '.'); //Find the extension
    char base_name[512];
    if (extension && extension != raw_filename) {
        size_t base_len = (size_t)(extension - raw_filename);
        snprintf(base_name, sizeof(base_name), "%.*s", (int)base_len, raw_filename);
    }
    else {
        extension = NULL;
        snprintf(base_name, sizeof(base_name), "%s", raw_filename);
    }
    char* clean_basename = clean_name(base_name, false);
    if (!clean_basename) {
        log_message(LOG_ERROR, "Failed to clean basename");
        return false;
    }

    //Create path of temporary copy to be compressed 
    char temp_filename[1024];
    if (extension) {
        snprintf(temp_filename, sizeof(temp_filename), "%s%s", clean_basename, extension);
    }
    else {
        snprintf(temp_filename, sizeof(temp_filename), "%s", clean_basename);
    }
    free(clean_basename);

    //Copy file to the destination path
    char temp_file[1024];
#ifdef _WIN32
    snprintf(temp_file, sizeof(temp_file), "%s\\%s", destination_dir, temp_filename);
#else
    snprintf(temp_file, sizeof(temp_file), "%s/%s", destination_dir, temp_filename);
#endif 

    if (file_exists(destination_path)) {
        long file_size = get_file_size(filename);
        long file_copy_size = get_file_size(destination_path);

        if (file_size > file_copy_size) {
            log_message(LOG_WARNING, "Compressed copy already exists at the location %s", destination_path);
            return false;
        }
    }

    if (!copy_file(filename, temp_file)) {
        log_message(LOG_ERROR, "Failed to create temporary file copy for %s", temp_file);
        free(clean_filename);
        return false;
    }

    //Compress the file copy
    return compress_file(filename, temp_file);
}