#include <gtest/gtest.h>
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
    init_logger(LOG_FILE);
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