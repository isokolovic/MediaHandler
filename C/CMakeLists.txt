cmake_minimum_required(VERSION 3.10)
project(MediaMigration C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

# Main target
add_executable(media_migration
    src/main.c
    src/file_handler.c
    src/logger.c
    src/image_compressor.c
    src/video_compressor.c
)

# Testing target
add_executable(media_migration_tests
    tests/test_logger.cpp
)

# Include directories
target_include_directories(media_migration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_include_directories(media_migration_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

if(UNIX)
    find_package(PkgConfig REQUIRED)
    find_package(GTest CONFIG REQUIRED)

    pkg_check_modules(EXIF REQUIRED libexif)
    pkg_check_modules(HEIF REQUIRED libheif)
    pkg_check_modules(AVFORMAT REQUIRED libavformat)
    pkg_check_modules(AVCODEC REQUIRED libavcodec)
    pkg_check_modules(AVUTIL REQUIRED libavutil)
    pkg_check_modules(SWS REQUIRED libswscale)

    target_include_directories(media_migration PRIVATE
        ${JPEG_INCLUDE_DIRS} ${PNG_INCLUDE_DIRS}
        ${EXIF_INCLUDE_DIRS} ${HEIF_INCLUDE_DIRS}
        ${AVFORMAT_INCLUDE_DIRS} ${AVCODEC_INCLUDE_DIRS}
        ${AVUTIL_INCLUDE_DIRS} ${SWS_INCLUDE_DIRS}
    )

    target_compile_options(media_migration PRIVATE
        ${JPEG_CFLAGS_OTHER} ${PNG_CFLAGS_OTHER}
        ${EXIF_CFLAGS_OTHER} ${HEIF_CFLAGS_OTHER}
        ${AVFORMAT_CFLAGS_OTHER} ${AVCODEC_CFLAGS_OTHER}
        ${AVUTIL_CFLAGS_OTHER} ${SWS_CFLAGS_OTHER}
    )

    target_link_libraries(media_migration PRIVATE
        ${JPEG_LIBRARIES} ${PNG_LIBRARIES}
        ${EXIF_LIBRARIES} ${HEIF_LIBRARIES}
        ${AVFORMAT_LIBRARIES} ${AVCODEC_LIBRARIES}
        ${AVUTIL_LIBRARIES} ${SWS_LIBRARIES}
    )

    target_link_libraries(media_migration_tests PRIVATE
    GTest::gtest
    )
endif()