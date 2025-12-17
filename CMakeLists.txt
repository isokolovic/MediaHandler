cmake_minimum_required(VERSION 3.31)
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
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)
find_package(libheif CONFIG REQUIRED)
find_package(FFMPEG REQUIRED COMPONENTS avcodec avformat avutil swscale)

# Executable
add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME}
    PRIVATE
        src/main.cpp
        src/utils/config.cpp
        src/utils/logger.cpp
        src/utils/app_args.cpp
        src/compressor/compression_engine.cpp
        src/compressor/video_compressor.cpp
        src/compressor/image_compressor.cpp
 )

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${FFMPEG_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        spdlog::spdlog
        CLI11::CLI11
        JPEG::JPEG
        PNG::PNG
        heif
        unofficial::libexif::libexif
        ${FFMPEG_LIBRARIES}
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
        tests/test_compression_engine.cpp
        tests/test_common.cpp
        tests/test_config.cpp
        tests/test_logger.cpp
        src/compressor/compression_engine.cpp
        src/compressor/video_compressor.cpp
        src/compressor/image_compressor.cpp
        src/utils/app_args.cpp
        src/utils/config.cpp
        src/utils/logger.cpp
    )

    target_include_directories(media_handler_tests
        PRIVATE
            ${PROJECT_SOURCE_DIR}/tests
            ${PROJECT_SOURCE_DIR}/include
            ${FFMPEG_INCLUDE_DIRS}
    )

    target_link_libraries(media_handler_tests
        PRIVATE
            GTest::gtest_main
            spdlog::spdlog
            nlohmann_json::nlohmann_json # For json parsing 
            CLI11::CLI11
            JPEG::JPEG
            PNG::PNG
            heif
            unofficial::libexif::libexif
            ${FFMPEG_LIBRARIES}
    )

    add_test(NAME media_handler_tests COMMAND media_handler_tests)
endif()