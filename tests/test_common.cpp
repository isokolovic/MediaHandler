#include "test_common.h"
#include <chrono>

void TestCommon::SetUp() {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    std::string test_name = info ? info->name() : "unknown";

    test_dir = fs::temp_directory_path() /
        ("media_handler_test_" + std::to_string(now) + "_" + test_name);

    fs::create_directories(test_dir);
}

void TestCommon::TearDown() {
    try {
        if (!test_dir.empty()) {
            fs::remove_all(test_dir);
        }
    }
    catch (...) {
        // Windows file lock — ignore
    }
}

fs::path TestCommon::path(const std::string& filename) const {
    return test_dir / filename;
}