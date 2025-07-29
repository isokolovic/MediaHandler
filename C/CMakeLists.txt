cmake_minimum_required(VERSION 3.10)
project(MediaMigration C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(ExternalProject)

ExternalProject_Add(libjpeg-turbo
    GIT_REPOSITORY https://github.com/libjpeg-turbo/libjpeg-turbo.git
    GIT_TAG 3.0.3
    GIT_SHALLOW TRUE
    PREFIX ${CMAKE_BINARY_DIR}/_deps/libjpeg-turbo
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DWITH_TURBOJPEG=OFF
        -DENABLE_SHARED=OFF
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>
    INSTALL_DIR ${CMAKE_BINARY_DIR}/_deps/libjpeg-turbo/install
    BUILD_BYPRODUCTS
        ${CMAKE_BINARY_DIR}/_deps/libjpeg-turbo/install/lib/jpeg-static.lib
)

set(LIBJPEG_TURBO_INSTALL_DIR ${CMAKE_BINARY_DIR}/_deps/libjpeg-turbo/install)

add_executable(media_migration
    src/main.c
    src/file_handler.c
    src/logger.c
    src/compressor.c
)

add_dependencies(media_migration libjpeg-turbo)

target_include_directories(media_migration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${LIBJPEG_TURBO_INSTALL_DIR}/include
)

target_link_libraries(media_migration PRIVATE
    optimized ${LIBJPEG_TURBO_INSTALL_DIR}/lib/jpeg-static.lib
    debug ${LIBJPEG_TURBO_INSTALL_DIR}/lib/jpeg-static.lib
)

if(MSVC)
    target_compile_definitions(media_migration PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()