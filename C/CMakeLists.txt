cmake_minimum_required(VERSION 3.10)
project(MediaMigration C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(ExternalProject)

# 📦 Install paths
set(DEPS_DIR ${CMAKE_BINARY_DIR}/_deps)
set(LIBHEIF_INSTALL_DIR ${DEPS_DIR}/libheif/install)
set(LIBJPEG_TURBO_INSTALL_DIR ${DEPS_DIR}/libjpeg-turbo/install)

# 🛠 Runtime library setting (Windows only)
if(MSVC)
    set(MSVC_RUNTIME MultiThreaded)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(MSVC_RUNTIME MultiThreadedDebug)
    endif()
endif()

# 🧱 External project: libheif
set(LIBHEIF_CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBHEIF_INSTALL_DIR}
    -DWITH_EXAMPLES=OFF
    -DENABLE_SHARED=OFF
    -DWITH_LIBDE265=OFF
    -DWITH_X265=OFF
)

if(NOT WIN32)
    list(APPEND LIBHEIF_CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})
endif()

ExternalProject_Add(libheif
    GIT_REPOSITORY https://github.com/strukturag/libheif.git
    GIT_TAG v1.18.2
    GIT_SHALLOW TRUE
    PREFIX ${DEPS_DIR}/libheif
    CMAKE_ARGS ${LIBHEIF_CMAKE_ARGS}
    BUILD_BYPRODUCTS
        ${LIBHEIF_INSTALL_DIR}/lib/libheif.a
)

# 🧱 External project: libjpeg-turbo
set(LIBJPEG_CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${LIBJPEG_TURBO_INSTALL_DIR}
    -DWITH_TURBOJPEG=OFF
    -DENABLE_SHARED=OFF
)

if(NOT WIN32)
    list(APPEND LIBJPEG_CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})
endif()

ExternalProject_Add(libjpeg-turbo
    GIT_REPOSITORY https://github.com/libjpeg-turbo/libjpeg-turbo.git
    GIT_TAG 3.0.3
    GIT_SHALLOW TRUE
    PREFIX ${DEPS_DIR}/libjpeg-turbo
    CMAKE_ARGS ${LIBJPEG_CMAKE_ARGS}
    BUILD_BYPRODUCTS
        ${LIBJPEG_TURBO_INSTALL_DIR}/lib/libjpeg.a
)

# 🎯 Main target
add_executable(media_migration
    src/main.c
    src/file_handler.c
    src/logger.c
    src/compressor.c
)

# ⏳ Build dependencies
add_dependencies(media_migration libheif)
add_dependencies(media_migration libjpeg-turbo)

# 📂 Include directories
target_include_directories(media_migration PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${LIBHEIF_INSTALL_DIR}/include
    ${LIBJPEG_TURBO_INSTALL_DIR}/include
)

# 🔗 Link libraries (platform-aware)
if(WIN32)
    target_link_libraries(media_migration PRIVATE
        ${LIBHEIF_INSTALL_DIR}/lib/heif.lib
        ${LIBJPEG_TURBO_INSTALL_DIR}/lib/jpeg-static.lib
    )
else()
    target_link_libraries(media_migration PRIVATE
        ${LIBHEIF_INSTALL_DIR}/lib/libheif.a
        ${LIBJPEG_TURBO_INSTALL_DIR}/lib/libjpeg.a
    )
endif()
