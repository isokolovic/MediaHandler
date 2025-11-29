#pragma once
#include <gtest/gtest.h>
#include <filesystem>
#include <string>
#include <utils/utils.h>

class TestCommon : public ::testing::Test {
protected:
    std::filesystem::path test_dir;

	/// @brief Initialize fixture before each test
    void SetUp() override;
	/// @brief Clean up fixture after each test
    void TearDown() override;

	/// @brief Get full path to a file in the test temp directory
    std::filesystem::path path(const std::string& filename) const;
};