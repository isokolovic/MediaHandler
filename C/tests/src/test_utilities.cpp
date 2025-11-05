#include "test_utilities.h"
#include <fstream>
#include <sstream> 
#include <vector>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#include <sys/stat.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#endif

//CONFIGURATION
//Specify source and target folders:
const char* SOURCE_DIR = "";
const char* TARGET_DIR = "";

const char* LOG_FILE = LOG_FILE_PATH; 

/// @brief Ensure target directory exists
/// @return True if exists / created
bool ensure_target_dir() {
	if (is_directory(TARGET_DIR)) {
		return true;
	} 
	return create_directory(TARGET_DIR);	
}

/// @brief Cleanup target directory after test run
void cleanup_target_dir() {
#ifdef _WIN32
	char search_path[4096]; 
	snprintf(search_path, sizeof(search_path), "%s\\*.*", TARGET_DIR);
	WIN32_FIND_DATA find_data; 
	HANDLE handle = FindFirstFile(search_path, &find_data);
	if (handle == INVALID_HANDLE_VALUE) return; 

	do {
		if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) continue;
		char path[4096];
		snprintf(path, sizeof(path), "%s\\%s", TARGET_DIR, find_data.cFileName);
		remove(path);
	} while (FindNextFile(handle, &find_data));

	FindClose(handle);
#else
	DIR* dir = opendir(TARGET_DIR); 
	if (!dir) return;
	struct dirent* entry;

	while ((entry = readdir(dir)) != nullptr) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

		char path[4096];
		snprintf(path, sizeof(path), "%s/%s", TARGET_DIR, entry->d_name);

		struct stat st;
		if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
			if (remove(path) != 0) {
				perror(path);
			}
		}
	}
	closedir(dir);
#endif
}

/// @brief Read log file content
/// @param path Log file path
/// @return Log file stringstream
std::string read_log_content(const char* path) {
	std::ifstream f(path);
	if (!f) return "";
	std::ostringstream s;
	s << f.rdbuf();
	return s.str();
}

/// @brief Lot the outcome of logging test
/// @param name Test identifier
/// @param pass Test
void log_test_outcome(const char* name, bool pass)
{
	log_message(pass ? LOG_INFO : LOG_ERROR, "Test %s: %s", name, pass ? "PASSED" : "FAILED");
}

/// @brief Cout files in target directory
/// @return Number of files
int count_valid_outputs() {
	int count = 0;

#ifdef _WIN32
	char search_path[4096];
	snprintf(search_path, sizeof(search_path), "%s\\*.*", TARGET_DIR);

	WIN32_FIND_DATA find_data;
	HANDLE handle = FindFirstFile(search_path, &find_data);
	if (handle == INVALID_HANDLE_VALUE) return 0;

	do {
		if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
		if (is_file_type_valid(find_data.cFileName)) ++count;
	} while (FindNextFile(handle, &find_data));

	FindClose(handle);

#else
	DIR* dir = opendir(TARGET_DIR);
	if (!dir) return 0;

	struct dirent* entry;
	while ((entry = readdir(dir)) != nullptr) {
		if (entry->d_type == DT_REG && is_file_type_valid(entry->d_name)) ++count;
	}

	closedir(dir);
#endif

	return count;
}

/// @brief List files in target directory and subdirectories
/// @param dir Parent target directory
/// @return Vector of filenames
std::vector<std::string> list_files_in_dir(const char* dir) {
	std::vector<std::string> files;

#ifdef _WIN32
	std::string search_path = std::string(dir) + "\\*";
	WIN32_FIND_DATAA fd;
	HANDLE hFind = FindFirstFileA(search_path.c_str(), &fd);
	if (hFind == INVALID_HANDLE_VALUE) return files;

	do {
		std::string full_path = std::string(dir) + "\\" + fd.cFileName;

		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0) {
				std::vector<std::string> sub_files = list_files_in_dir(full_path.c_str());
				files.insert(files.end(), sub_files.begin(), sub_files.end());
			}
		}
		else {
			files.push_back(full_path);
		}
	} while (FindNextFileA(hFind, &fd));
	FindClose(hFind);

#else
	DIR* d = opendir(dir);
	if (!d) return files;
	struct dirent* entry;
	while ((entry = readdir(d)) != nullptr) {
		std::string name = entry->d_name;
		if (name == "." || name == "..") continue;

		std::string full_path = std::string(dir) + "/" + name;

		if (entry->d_type == DT_DIR) {
			std::vector<std::string> sub_files = list_files_in_dir(full_path.c_str());
			files.insert(files.end(), sub_files.begin(), sub_files.end());
		}
		else if (entry->d_type == DT_REG) {
			files.push_back(full_path);
		}
	}
	closedir(d);
#endif

	return files;
}


/// @brief Discover files in source directory
/// @return Vector of source files
std::vector<std::string> discover_source_files() {
	std::vector<std::string> files;

#ifdef _WIN32
	std::string search_path = std::string(SOURCE_DIR) + "\\*";
	WIN32_FIND_DATAA fd;
	HANDLE hFind = FindFirstFileA(search_path.c_str(), &fd);
	if (hFind == INVALID_HANDLE_VALUE) return files;

	do {
		if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			files.push_back(fd.cFileName);
		}
	} while (FindNextFileA(hFind, &fd));
	FindClose(hFind);
#else
	DIR* d = opendir(SOURCE_DIR);
	if (!d) return files;
	struct dirent* entry;
	while ((entry = readdir(d)) != nullptr) {
		if (entry->d_type == DT_REG)
			files.push_back(entry->d_name);
	}
	closedir(d);
#endif

	return files;
}

/// @brief Creates a mock log file from files in source directory
/// @param filename File to be logged as failed
/// @brief Appends a mock FAILED entry to the log file for testing retry logic.
/// @param filename File to be logged as failed (relative to SOURCE_DIR).
void create_mock_log(const char* filename) {
	FILE* log = fopen(LOG_FILE, "a");
	if (!log) {
		log_message(LOG_ERROR, "Failed to open log file: %s", LOG_FILE);
		return;
	}
#ifdef _WIN32
	fprintf(log, "File Processing: %s/%s - FAILED\n", SOURCE_DIR, filename);
#else
	// Use backslash on Linux
	fprintf(log, "File Processing: %s\\%s - FAILED\n", SOURCE_DIR, filename);
#endif
	fclose(log);
}

/// @brief Get full target dir file path
/// @param file_name File name
/// @return Full file path
char* get_output_path(const char* file_name) {
	char* path = (char*)malloc(4096);
	snprintf(path, 4096, "%s/%s", TARGET_DIR, file_name);
	return path;
}

/// @brief Check if file is compressed
/// @param in_file Input file
/// @param out_file Output file
/// @return True if output is smaller than input
bool is_compressed_smaller(const char* in_file, const char* out_file) {
	long in = get_file_size(in_file), out = get_file_size(out_file);
	return out > 0 && out < in;
}

/// @brief Check if file is copied
/// @param in_file Input file
/// @param out_file Output file
/// @return True if output file size is equal to input file
bool is_compressed_equal(const char* in_file, const char* out_file) {
	long in = get_file_size(in_file), out = get_file_size(out_file);
	return out > 0 && out == in;
}

/// @brief Check if target file path is put to correct year in dest folder
/// @param target_dir Target directory
/// @param file_path Source file
/// @return True if file is put to correct sub-folder
bool file_in_correct_year_folder(const char* target_dir, const char* file_path)
{
	int file_creation_year = get_file_creation_year(file_path);
	if (file_creation_year <= 0) return false;

	// Build expected folder path: <target_dir>/<year>/
	char expected_folder[4096];
	snprintf(expected_folder, sizeof(expected_folder), "%s/%d", target_dir, file_creation_year);

	// Check if file_path starts with expected_folder
#ifdef _WIN32
	// Normalize slashes for comparison
	char normalized_path[4096];
	strncpy(normalized_path, file_path, sizeof(normalized_path));
	for (char* p = normalized_path; *p; ++p) {
		if (*p == '\\') *p = '/';
	}
	return strstr(normalized_path, expected_folder) != NULL;
#else
	return strstr(file_path, expected_folder) != NULL;
#endif
}

/// @brief List subdirectories in tarted dir
/// @param dir target dir
/// @return Subdirectories
std::vector<std::string> list_subdirs_in_dir(const char* dir) {
	std::vector<std::string> subdirs;

#ifdef _WIN32
	std::string search_path = std::string(dir) + "\\*";
	WIN32_FIND_DATAA fd;
	HANDLE hFind = FindFirstFileA(search_path.c_str(), &fd);
	if (hFind == INVALID_HANDLE_VALUE) return subdirs;

	do {
		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
			strcmp(fd.cFileName, ".") != 0 &&
			strcmp(fd.cFileName, "..") != 0) {
			subdirs.push_back(fd.cFileName);
		}
	} while (FindNextFileA(hFind, &fd));
	FindClose(hFind);

#else
	DIR* d = opendir(dir);
	if (!d) return subdirs;
	struct dirent* entry;
	while ((entry = readdir(d)) != nullptr) {
		if (entry->d_type == DT_DIR &&
			strcmp(entry->d_name, ".") != 0 &&
			strcmp(entry->d_name, "..") != 0) {
			subdirs.push_back(entry->d_name);
		}
	}
	closedir(d);
#endif

	return subdirs;
}