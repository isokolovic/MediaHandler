cmake_minimum_required(VERSION 3.10)
project(MediaMigration C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

# Shared source files
set(MEDIA_CORE_SOURCES
    src/file_handler.c
    src/logger.c
    src/image_compressor.c
    src/video_compressor.c
)

# Shared include directory
set(PROJECT_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

# Create core library
add_library(media_core STATIC ${MEDIA_CORE_SOURCES})
target_include_directories(media_core PUBLIC ${PROJECT_INCLUDE_DIR})

# Main executable
add_executable(media_migration src/main.c)
target_link_libraries(media_migration PRIVATE media_core)
target_include_directories(media_migration PRIVATE ${PROJECT_INCLUDE_DIR})

# Test executable
add_executable(media_migration_tests tests/test_logger.cpp)
target_link_libraries(media_migration_tests PRIVATE media_core)
target_include_directories(media_migration_tests PRIVATE ${PROJECT_INCLUDE_DIR})

# Linux-specific setup
if(UNIX)
    # Required packages
    find_package(PkgConfig REQUIRED)
    find_package(GTest CONFIG REQUIRED)

    # Detect media libraries via pkg-config
    foreach(lib IN ITEMS libjpeg libpng libexif libheif libavformat libavcodec libavutil libswscale)
        string(TOUPPER ${lib} LIB_UPPER)
        pkg_check_modules(${LIB_UPPER} REQUIRED ${lib})

        message(STATUS "${lib} found: ${${LIB_UPPER}_FOUND}")
        message(STATUS "${lib} libs: ${${LIB_UPPER}_LIBRARIES}")
        
        target_compile_options(media_core PRIVATE ${${LIB_UPPER}_CFLAGS_OTHER})
        target_link_libraries(media_core PRIVATE ${${LIB_UPPER}_LIBRARIES})
    endforeach()

    # Link Google Test
    target_link_libraries(media_migration_tests PRIVATE GTest::gtest GTest::gtest_main)
endif()
