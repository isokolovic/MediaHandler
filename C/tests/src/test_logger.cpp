#include <gtest/gtest.h>
#include "test_utilities.h"

class LoggerTests : public ::testing::Test {
protected: //Need access from GTests
	void SetUp() override {
		//ASSERT_TRUE(ensure_target_dir());
		remove(LOG_FILE); //Start clean
	}
};
