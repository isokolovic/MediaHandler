cmake_minimum_required(VERSION 3.26)
project(MediaHandler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# vcpkg integration
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "")
endif()

# Find packages
find_package(unofficial-libexif CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)

# Executable
add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME}
    PRIVATE
        src/main.cpp
        src/utils/config.cpp
        src/utils/logger.cpp
 )

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        unofficial::libexif::libexif
        spdlog::spdlog
        CLI11::CLI11
)

# IDE folder grouping
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Tests
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    find_package(nlohmann_json CONFIG REQUIRED)

    add_executable(media_handler_tests
        tests/test_common.cpp
        tests/test_config.cpp
        tests/test_logger.cpp
        src/utils/config.cpp
        src/utils/logger.cpp
    )

    target_include_directories(media_handler_tests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/tests
            ${PROJECT_SOURCE_DIR}/include
    )

    target_link_libraries(media_handler_tests
        PRIVATE
            GTest::gtest_main
            spdlog::spdlog
            nlohmann_json::nlohmann_json # For json parsing 
    )

    add_test(NAME media_handler_tests COMMAND media_handler_tests)
endif()