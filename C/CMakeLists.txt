cmake_minimum_required(VERSION 3.10)
project(MediaMigration C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

include(ExternalProject)

# ExternalProject for libjpeg-turbo
ExternalProject_Add(libjpeg-turbo
    GIT_REPOSITORY https://github.com/libjpeg-turbo/libjpeg-turbo.git
    GIT_TAG 3.0.3
    GIT_SHALLOW TRUE
    PREFIX ${CMAKE_BINARY_DIR}/_deps/libjpeg-turbo
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DWITH_TURBOJPEG=OFF
        -DENABLE_SHARED=OFF
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/_deps/libjpeg-turbo/install
        -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>
    BUILD_BYPRODUCTS
        ${CMAKE_BINARY_DIR}/_deps/libjpeg-turbo/install/lib/libjpeg.a
        ${CMAKE_BINARY_DIR}/_deps/libjpeg-turbo/install/lib/jpeg-static.lib
)

# Define include and library paths
set(LIBJPEG_TURBO_INCLUDE_DIR ${CMAKE_BINARY_DIR}/_deps/libjpeg-turbo/install/include)

if(WIN32)
    set(LIBJPEG_TURBO_LIBRARY ${CMAKE_BINARY_DIR}/_deps/libjpeg-turbo/install/lib/jpeg-static.lib)
else()
    set(LIBJPEG_TURBO_LIBRARY ${CMAKE_BINARY_DIR}/_deps/libjpeg-turbo/install/lib/libjpeg.a)
endif()

# Define imported target for libjpeg
add_library(libjpeg STATIC IMPORTED GLOBAL)
set_target_properties(libjpeg PROPERTIES
    IMPORTED_LOCATION ${LIBJPEG_TURBO_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${LIBJPEG_TURBO_INCLUDE_DIR}
)

# Add executable
add_executable(media_migration
    src/main.c
    src/file_handler.c    
    src/logger.c
    src/compressor.c
)

# Ensure libjpeg-turbo is built before media_migration
add_dependencies(media_migration libjpeg-turbo)

# Include directories
target_include_directories(media_migration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(media_migration PRIVATE libjpeg)

# Suppress POSIX deprecation warnings on MSVC
if(MSVC)
    target_compile_definitions(media_migration PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Debug output
message(STATUS "libjpeg-turbo include: ${LIBJPEG_TURBO_INCLUDE_DIR}")
message(STATUS "libjpeg-turbo library: ${LIBJPEG_TURBO_LIBRARY}")
message(STATUS "Project includes: ${CMAKE_SOURCE_DIR}/include")
