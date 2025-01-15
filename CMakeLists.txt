cmake_minimum_required(VERSION 3.16)

project(liburing
    VERSION 2.3.0
    DESCRIPTION "C library for Linux io_uring"
    LANGUAGES C
)

# ------------------------------------------------------------------------------
# User Options
# ------------------------------------------------------------------------------
option(LIBURING_ENABLE_SHARED "Build liburing as a shared library" ON)
option(LIBURING_ENABLE_STATIC "Build liburing as a static library" ON)
option(LIBURING_ENABLE_SANITIZERS "Enable Address/UB Sanitizers" OFF)
option(LIBURING_USE_LIBC "Use libc for liburing" OFF)

# ------------------------------------------------------------------------------
# Build Output Directories
# ------------------------------------------------------------------------------
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ------------------------------------------------------------------------------
# Check for linux/io_uring.h
# ------------------------------------------------------------------------------
include(CheckIncludeFile)
check_include_file("linux/io_uring.h" HAVE_IO_URING_H)
if(NOT HAVE_IO_URING_H)
    message(FATAL_ERROR "linux/io_uring.h not found. Cannot build liburing.")
endif()

# ------------------------------------------------------------------------------
# Compiler Settings
# ------------------------------------------------------------------------------
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

# ------------------------------------------------------------------------------
# Possibly-Generated Headers
# ------------------------------------------------------------------------------
# If your repo doesn't have compat.h, comment this out or provide a fallback.
set(HAS_COMPAT_H_IN_SOURCE FALSE)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/include/liburing/compat.h")
    set(HAS_COMPAT_H_IN_SOURCE TRUE)
endif()

set(GENERATED_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated/liburing")

if(HAS_COMPAT_H_IN_SOURCE)
    file(MAKE_DIRECTORY ${GENERATED_INCLUDE_DIR})

    # We'll just copy it to keep the build referencing a single location.
    set(COMPAT_HEADER ${GENERATED_INCLUDE_DIR}/compat.h)
    add_custom_command(
        OUTPUT ${COMPAT_HEADER}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/src/include/liburing/compat.h"
                "${COMPAT_HEADER}"
        MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/src/include/liburing/compat.h"
        COMMENT "Copying compat.h to build directory."
    )
    add_custom_target(GenerateHeaders ALL DEPENDS ${COMPAT_HEADER})
endif()

# ------------------------------------------------------------------------------
# Sources
# ------------------------------------------------------------------------------
set(LIBURING_SOURCES
    src/setup.c
    src/queue.c
    src/register.c
    src/syscall.c
    src/version.c
)

# ------------------------------------------------------------------------------
# Library Creation Helper
# ------------------------------------------------------------------------------
function(configure_uring_target target_name is_shared)
    # If we have a generate-headers step, make sure the target depends on it:
    if(TARGET GenerateHeaders)
        add_dependencies(${target_name} GenerateHeaders)
    endif()

    target_compile_definitions(${target_name} PRIVATE LIBURING_INTERNAL)

    if(${is_shared})
        set_target_properties(${target_name} PROPERTIES
            OUTPUT_NAME "uring"
            VERSION ${PROJECT_VERSION}
            SOVERSION 2
        )
    else()
        set_target_properties(${target_name} PROPERTIES OUTPUT_NAME "uring")
    endif()

    # Add our include dirs: source + potentially generated
    target_include_directories(${target_name} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/include>
        $<INSTALL_INTERFACE:include>
    )
    if(TARGET GenerateHeaders)
        target_include_directories(${target_name} PUBLIC
            $<BUILD_INTERFACE:${GENERATED_INCLUDE_DIR}>
        )
    endif()
endfunction()

# ------------------------------------------------------------------------------
# Create Library Targets
# ------------------------------------------------------------------------------
if(LIBURING_ENABLE_STATIC)
    add_library(liburing_static STATIC ${LIBURING_SOURCES})
    configure_uring_target(liburing_static FALSE)
endif()

if(LIBURING_ENABLE_SHARED)
    add_library(liburing_shared SHARED ${LIBURING_SOURCES})
    configure_uring_target(liburing_shared TRUE)
endif()

# ------------------------------------------------------------------------------
# Install Layout
# ------------------------------------------------------------------------------
set(INSTALL_INCLUDE_DIR "${CMAKE_INSTALL_PREFIX}/include/liburing")
set(INSTALL_LIB_DIR "${CMAKE_INSTALL_PREFIX}/lib")

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

# Always install the main headers
install(FILES
    src/include/liburing/io_uring.h
    src/include/liburing/barrier.h
    src/include/liburing/sanitize.h
    src/include/liburing/io_uring_version.h
    DESTINATION ${INSTALL_INCLUDE_DIR}
)

# If we actually have compat.h, install that too
if(HAS_COMPAT_H_IN_SOURCE)
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/include/liburing/compat.h"
        DESTINATION ${INSTALL_INCLUDE_DIR}
    )
endif()

# ------------------------------------------------------------------------------
# pkg-config and CMake package
# ------------------------------------------------------------------------------
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
