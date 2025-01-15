# This CMakeLists.txt file is intended to simplify integration with CMake-based 
# projects, and doesn't cover the tests, examples, and other components of liburing.
# The Makefile is still the primary build system for liburing R&D.
cmake_minimum_required(VERSION 3.16)

# Project details
project(liburing
    VERSION 2.3.0 # Update this to the actual version
    DESCRIPTION "C library for Linux io_uring"
    LANGUAGES C
)

# Configuration options
option(LIBURING_ENABLE_SHARED "Build liburing as a shared library" ON)
option(LIBURING_ENABLE_STATIC "Build liburing as a static library" ON)
option(LIBURING_ENABLE_SANITIZERS "Enable Address and Undefined Behavior Sanitizers" OFF)
option(LIBURING_USE_LIBC "Use libc for liburing" OFF)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Compiler settings
include(CheckIncludeFile)
check_include_file("linux/io_uring.h" HAVE_IO_URING_H)

if(NOT HAVE_IO_URING_H)
    message(FATAL_ERROR "The header file linux/io_uring.h is required but was not found.")
endif()

# Compiler flags
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(LIBURING_ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -g -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

if(LIBURING_USE_LIBC)
    add_compile_definitions(CONFIG_USE_LIBC)
else()
    add_compile_definitions(CONFIG_NOLIBC)
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/include)

# Sources
set(LIBURING_SOURCES
    src/setup.c
    src/queue.c
    src/register.c
    src/syscall.c
    src/version.c
)

# Define library targets
if(LIBURING_ENABLE_STATIC)
    add_library(liburing_static STATIC ${LIBURING_SOURCES})
    target_compile_definitions(liburing_static PRIVATE LIBURING_INTERNAL)
    set_target_properties(liburing_static PROPERTIES OUTPUT_NAME "uring")
    target_include_directories(liburing_static PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/include>
        $<INSTALL_INTERFACE:include>
    )
endif()

if(LIBURING_ENABLE_SHARED)
    add_library(liburing_shared SHARED ${LIBURING_SOURCES})
    target_compile_definitions(liburing_shared PRIVATE LIBURING_INTERNAL)
    set_target_properties(liburing_shared PROPERTIES OUTPUT_NAME "uring" VERSION ${PROJECT_VERSION} SOVERSION 2)
    target_include_directories(liburing_shared PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/include>
        $<INSTALL_INTERFACE:include>
    )
endif()

# Install settings
set(INSTALL_INCLUDE_DIR ${CMAKE_INSTALL_PREFIX}/include/liburing)
set(INSTALL_LIB_DIR ${CMAKE_INSTALL_PREFIX}/lib)

if(LIBURING_ENABLE_STATIC)
    install(TARGETS liburing_static
        ARCHIVE DESTINATION ${INSTALL_LIB_DIR}
        LIBRARY DESTINATION ${INSTALL_LIB_DIR}
        INCLUDES DESTINATION ${INSTALL_INCLUDE_DIR}
    )
endif()

if(LIBURING_ENABLE_SHARED)
    install(TARGETS liburing_shared
        ARCHIVE DESTINATION ${INSTALL_LIB_DIR}
        LIBRARY DESTINATION ${INSTALL_LIB_DIR}
        RUNTIME DESTINATION ${INSTALL_LIB_DIR}
        INCLUDES DESTINATION ${INSTALL_INCLUDE_DIR}
    )
endif()

install(FILES
    src/include/liburing/io_uring.h
    src/include/liburing/barrier.h
    src/include/liburing/sanitize.h
    src/include/liburing/io_uring_version.h
    DESTINATION ${INSTALL_INCLUDE_DIR}
)

# Package support
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_BINARY_DIR}/liburingConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/liburing.pc.in"
    "${CMAKE_BINARY_DIR}/liburing.pc" @ONLY
)

install(FILES
    "${CMAKE_BINARY_DIR}/liburing.pc"
    DESTINATION ${INSTALL_LIB_DIR}/pkgconfig
)

# Export package
export(PACKAGE liburing)
