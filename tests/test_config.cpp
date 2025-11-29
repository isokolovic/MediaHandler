#include "test_common.h"
#include "utils/config.h"
#include "utils/utils.h"
#include <fstream>

#define INVALID_CONFIG_FILE "invalid_config.json"

class ConfigTest : public TestCommon {};

/// @brief Test loading invalid config file
TEST_F(ConfigTest, MissingFile_ReturnsError) {
    auto result = media_handler::utils::Config::load(INVALID_CONFIG_FILE);
    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(result.error().empty());
}

/// @brief Test that invalid JSON returns parse error
TEST_F(ConfigTest, InvalidJson_ReturnsError) {
    {
        std::ofstream f(std::filesystem::path(CONFIG_FILE));
        f << "{ invalid json";  // malformed
    }

    auto result = media_handler::utils::Config::load(std::filesystem::path(CONFIG_FILE).string());
    EXPECT_FALSE(result.has_value()) << "Invalid JSON should fail";
    EXPECT_TRUE(result.error().find("JSON") != std::string::npos ||
        result.error().find("parse") != std::string::npos);
}

/// @brief Test that valid full config loads all fields correctly
TEST_F(ConfigTest, ValidConfig_LoadsAllFieldsCorrectly) {
    {
        std::ofstream f(std::filesystem::path(CONFIG_FILE));
        f << R"({
            "video": {
                "codec": "libx265",
                "preset": "slow",
                "crf": "18",
                "maxrate": "10M",
                "bufsize": "20M"
            },
            "audio": {
                "codec": "opus",
                "bitrate": "320k"
            },
            "general": {
                "container": "mkv",
                "output_dir": "converted",
                "threads": 16,
                "json_log": true,
                "log_level": "debug"
            }
        })";
    }

    auto result = media_handler::utils::Config::load(std::filesystem::path(CONFIG_FILE).string());
    ASSERT_TRUE(result.has_value()) << "Valid config should load successfully";

    const auto& cfg = result.value();

    EXPECT_EQ(cfg.video_codec, "libx265");
    EXPECT_EQ(cfg.video_preset, "slow");
    EXPECT_EQ(cfg.crf, "18");
    EXPECT_EQ(cfg.maxrate, "10M");
    EXPECT_EQ(cfg.bufsize, "20M");
    EXPECT_EQ(cfg.audio_codec, "opus");
    EXPECT_EQ(cfg.audio_bitrate, "320k");
    EXPECT_EQ(cfg.container, "mkv");
    EXPECT_EQ(cfg.output_dir, "converted");
    EXPECT_EQ(cfg.threads, 16);
    EXPECT_TRUE(cfg.json_log);
    EXPECT_EQ(cfg.log_level, spdlog::level::debug);
}

/// @brief Test that partial config uses defaults for missing fields
TEST_F(ConfigTest, PartialConfig_UsesDefaults) {
    std::ofstream f(std::filesystem::path(CONFIG_FILE));
    f << R"({ "general": { "threads": 8 } })";
    f.close();

    auto result = media_handler::utils::Config::load(std::filesystem::path(CONFIG_FILE).string());
    ASSERT_TRUE(result.has_value());
    const auto& cfg = result.value();

    // Default values:
    EXPECT_EQ(cfg.threads, 8);
    EXPECT_EQ(cfg.crf, "23");           
    EXPECT_EQ(cfg.video_preset, "medium"); 
    EXPECT_EQ(cfg.video_codec, "libx264"); 
    EXPECT_EQ(cfg.container, "mp4");    
    EXPECT_FALSE(cfg.json_log);      
}

/// @brief Test that empty config file returns error
TEST_F(ConfigTest, EmptyFile_ReturnsError) {
    // simulate empty file (open for writing - truncate to 0 length)
    std::ofstream f(std::filesystem::path(CONFIG_FILE));
    f.close();

    auto result = media_handler::utils::Config::load(std::filesystem::path(CONFIG_FILE).string());
    EXPECT_FALSE(result.has_value());
}

/// @brief Test that wrong field types are ignored (uses defaults)
TEST_F(ConfigTest, WrongTypes_AreIgnored) {
    {
        std::ofstream f(std::filesystem::path(CONFIG_FILE));
        f << R"({
            "general": { "threads": "not_a_number", "json_log": "yes" }
        })";
    }

    auto result = media_handler::utils::Config::load(std::filesystem::path(CONFIG_FILE).string());
    ASSERT_TRUE(result.has_value());

    const auto& cfg = result.value();
    // Default - string ignored:
    EXPECT_EQ(cfg.threads, 4);     
    EXPECT_FALSE(cfg.json_log);    
}