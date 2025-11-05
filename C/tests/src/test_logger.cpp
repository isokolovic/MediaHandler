#include "test_utilities.h"

class LoggerTest : public ::testing::Test {
protected: //Need access from GTests
	void SetUp() override {
		ASSERT_TRUE(ensure_target_dir());
		remove(LOG_FILE); //Start clean
	}

	void TearDown() override {
		close_logger(); 
		remove(LOG_FILE);
		cleanup_target_dir();
	}
};

/// @brief Verifies that log messages are written
/// @param Logger class
/// @param Test identifier
TEST_F(LoggerTest, LogsMessages) {
    init_logger(LOG_FILE, "r");
    log_message(LOG_INFO, "TEST INFO");
    log_message(LOG_ERROR, "TEST WARNING");
    log_message(LOG_ERROR, "TEST ERROR");
    close_logger();

    std::string log = read_log_content(LOG_FILE);
    bool passed = true;

    if (log.find("INFO") == std::string::npos) {
        ADD_FAILURE() << "Missing INFO log entry";
        passed = false;
    }
    if (log.find("WARNING") == std::string::npos) {
        ADD_FAILURE() << "Missing WARNING log entry";
        passed = false;
    }
    if (log.find("ERROR") == std::string::npos) {
        ADD_FAILURE() << "Missing ERROR log entry";
        passed = false;
    }

    log_test_outcome(__func__, passed);
}

/// @brief Check if retreiving failed files from log works
/// @param Logger class
/// @param Test identifier
TEST_F(LoggerTest, ParsesFailedFiles) {
    init_logger(LOG_FILE, "r");
    log_file_processing("C:/a.jpg", false);
    int n = 0;
    char** failed = get_failed_files_from_log(LOG_FILE, &n);

    ASSERT_EQ(n, 1);
    ASSERT_NE(strstr(failed[0], "a.jpg"), nullptr);

    for (int i = 0; i < n; ++i) free(failed[i]);
    free(failed);

    log_test_outcome("ParsesFailedFiles", true);
}