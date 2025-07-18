cmake_minimum_required(VERSION 3.10)
project(MediaMigration C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add executable
add_executable(media_migration
    src/main.c
    src/file_handler.c    
    src/logger.c
)

#Include directories
target_include_directories(media_migration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)