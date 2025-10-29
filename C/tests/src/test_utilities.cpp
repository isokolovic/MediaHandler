#include "test_utilities.h"
#include <fstream>
#include <sstream> 

#ifdef _WIN32
#include <Windows.h>
#include <sys/stat.h>
#else
#include <dirent.h>
#endif


//TODO add warning to console/log if test fail with missing file(s) to specify source target here.

//CONFIGURATION
//Specify source and target folders:
const char* SOURCE_DIR = "/mnt/c/Users/isoko/Desktop/New folder/S";
const char* TARGET_DIR = "/mnt/c/Users/isoko/Desktop/New folder/D";
//const char* SOURCE_DIR = "C:/Users/isoko/Desktop/New folder/S";
//const char* TARGET_DIR = "C:/Users/isoko/Desktop/New folder/D";
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
