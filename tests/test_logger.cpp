#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>
#include <future>
#include <string>
#include <nlohmann/json.hpp>
#include "utils/utils.h"

class LoggerTest : public ::testing::Test {
protected:
	std::filesystem::path test_log_dir;

	void SetUp() override {
		// Create a unique log directory for each test
		auto now = std::chrono::system_clock::now().time_since_epoch();
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

		test_log_dir = std::filesystem::temp_directory_path() /
			("media_handler_test_" + std::to_string(ms) + "_" +
				::testing::UnitTest::GetInstance()->current_test_info()->name());

		std::filesystem::create_directories(test_log_dir);

		media_handler::utils::log_dir = test_log_dir.string();
	}

	void TearDown() override {
		// Flush all loggers after each test and remove test log directory
		media_handler::utils::Logger::flush_all();
		try {
			std::filesystem::remove_all(test_log_dir);
		}
		catch (...) {
			// Ignore errors during cleanup
		}
	}

	/// @brief Return content as string
	std::string read_file(const std::string& path) {
		if (!std::filesystem::exists(path)) return "";
		std::ifstream file(path);
		return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	}
};

/// @brief Test creating a logger and logging messages
TEST_F(LoggerTest, CreateTxtLogger) {
	//will show info and debug
	auto logger = media_handler::utils::Logger::create("CreateTxtLogger", spdlog::level::debug); 
	ASSERT_TRUE(logger != nullptr);
	
	auto info_msg = "This is an info message";
	auto debug_msg = "This is a debug message";

	logger->info(info_msg);
	logger->debug(debug_msg);
	
	media_handler::utils::Logger::flush_all();
	//avoid race condition on file write and read
	std::this_thread::sleep_for(std::chrono::milliseconds(100)); 

	std::string log_content = read_file("logs/media_handler.log");

	ASSERT_NE(log_content.find(info_msg), std::string::npos);
	ASSERT_NE(log_content.find(debug_msg), std::string::npos);
}

/// @brief Check if JSON file is parseable
TEST_F(LoggerTest, JsonParseable) {
	auto logger = media_handler::utils::Logger::create("JsonParseable", spdlog::level::info, true);
	ASSERT_TRUE(logger != nullptr);
	
	logger->info("Test message");
	logger->error("Error message");
	media_handler::utils::Logger::flush_all();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	std::string content = read_file((test_log_dir / "media_handler.json").string());
	ASSERT_FALSE(content.empty());

	// Parse line by line
	std::istringstream stream(content);
	std::string line;
	int info_count = 0, error_count = 0;

	while (std::getline(stream, line)) {
		if (line.empty()) continue;
		try {
			auto json = nlohmann::json::parse(line);
			if (json.contains("message")) {
				
				auto msg = json["message"];
				auto lvl = json["level"]; 

				if (msg == "Test message" && lvl == "info") {
					++info_count;
				}
				if (msg == "Error message" && lvl == "error") {
					++error_count;
				}
			}
		}
		catch (const nlohmann::json::parse_error& e) {
			FAIL() << "Invalid JSON line: " << line.substr(0, 100) << " - " << e.what();
		}
	}

	EXPECT_EQ(info_count, 1);
	EXPECT_EQ(error_count, 1);
}

/// @brief Test flush latency under load
TEST_F(LoggerTest, FlushLatency) {
	auto logger = media_handler::utils::Logger::create("FlushLatency", spdlog::level::info, true);
	ASSERT_TRUE(logger != nullptr);

	// Queue 100 messages to simulate load
	for (int i = 0; i < 100; ++i) {
		logger->info("Latency test {}", i);
	}

	// Measure flush time (signal only, not full I/O)
	auto start = std::chrono::high_resolution_clock::now();
	media_handler::utils::Logger::flush_all();
	auto end = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	// Expect fast signal (<50ms for 100 msgs)
	EXPECT_LE(duration, 50);
}

/// @brief Test multiple independent loggers
TEST_F(LoggerTest, MultipleIndependentLoggers) {
	auto logger1 = media_handler::utils::Logger::create("MultipleIndependentLoggers_Logger1", spdlog::level::info);
	auto logger2 = media_handler::utils::Logger::create("MultipleIndependentLoggers_Logger2", spdlog::level::info, true);

	logger1->info("From Logger1");
	logger2->error("From Logger2");

	media_handler::utils::Logger::flush_all();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	std::string txt = read_file((test_log_dir / "media_handler.log").string());
	std::string json = read_file((test_log_dir / "media_handler.json").string());

	EXPECT_TRUE(txt.find("From Logger1") != std::string::npos);
	EXPECT_TRUE(json.find("\"message\": \"From Logger2\"") != std::string::npos);
}

/// @brief Checks if error-level log messages trigger immediate flush and log-level remain buffered
TEST_F(LoggerTest, FlushOnErrorImmediate) {
	auto logger = media_handler::utils::Logger::create("FlushOnErrorImmediate", spdlog::level::info);
	logger->info("This should not be flushed yet");
	logger->error("This should flush immediately");

	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	std::string content = read_file((test_log_dir / "media_handler.log").string());
	EXPECT_TRUE(content.find("This should flush immediately") != std::string::npos);
}

/// @brief  Validates that changing a logger’s level at runtime suppresses lower-level messages and enables new ones
TEST_F(LoggerTest, RuntimeLevelChange) {
	auto logger = media_handler::utils::Logger::create("RuntimeLevelChange", spdlog::level::err);
	logger->info("Should not appear");
	logger->error("Should appear");

	logger->set_level(spdlog::level::debug);
	logger->debug("Now debug appears");

	media_handler::utils::Logger::flush_all();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	std::string content = read_file((test_log_dir / "media_handler.log").string());
	EXPECT_TRUE(content.find("Should not appear") == std::string::npos);
	EXPECT_TRUE(content.find("Should appear") != std::string::npos);
	EXPECT_TRUE(content.find("Now debug appears") != std::string::npos);
}

/// @brief Ensures logger instances remain valid across copy and move operations, and all messages are preserved.
TEST_F(LoggerTest, LoggerCopyMove) {
	auto logger1 = media_handler::utils::Logger::create("LoggerCopyMove", spdlog::level::info);
	logger1->info("First");

	{
		auto logger2 = logger1; // copy
		logger2->info("Second");
		auto logger3 = std::move(logger2); // move
		logger3->info("Third");
	} // logger2/logger3 destroyed here

	logger1->info("Fourth"); // logger1 still alive

	media_handler::utils::Logger::flush_all();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	std::string content = read_file((test_log_dir / "media_handler.log").string());
	EXPECT_TRUE(content.find("First") != std::string::npos);
	EXPECT_TRUE(content.find("Second") != std::string::npos);
	EXPECT_TRUE(content.find("Third") != std::string::npos);
	EXPECT_TRUE(content.find("Fourth") != std::string::npos);
}