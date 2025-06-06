# Build script for the "linebreak" library, which provides the
# "linebreak" utilities from libgettextpo
#
# Timothy E. Holy, 2009
# Modernized CMake version

cmake_minimum_required(VERSION 3.10)
project(linebreak LANGUAGES C)

set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(CheckFunctionExists)
include(T4K_Sugar)

# If you're building this as part of a larger package, the following
# variables can be set upon entry:
#   LINEBREAK_BINARY_DIR
#   LINEBREAK_SOURCE_DIR
#   LOCALE_DIR
#   TOP_SRC_DIR
#   PREFIX

# Find iconv - use SDL_iconv as fallback if not found
find_package(Iconv)
set(HAVE_ICONV ${ICONV_FOUND})
t4k_include_definition(HAVE_ICONV)

# Configure internationalization checks
include(ConfigureChecksIntl)
t4k_gentle_set(LINEBREAK_BINARY_DIR ${CMAKE_BINARY_DIR})
t4k_gentle_set(LINEBREAK_SOURCE_DIR ${CMAKE_SOURCE_DIR})

# Generate config.h
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/linebreak_config.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
)

# Gather source files
file(GLOB_RECURSE LINEBREAK_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
file(GLOB_RECURSE LINEBREAK_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

# Verify we found sources
if(NOT LINEBREAK_SOURCES)
    message(FATAL_ERROR "No source files found for linebreak library")
endif()

# Create the library target
add_library(linebreak STATIC ${LINEBREAK_SOURCES} ${LINEBREAK_HEADERS})

# Target-specific include directories
target_include_directories(linebreak
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
)

# Target-specific compile definitions
target_compile_definitions(linebreak PRIVATE
    HAVE_CONFIG_H=1
    MAKE_LINEBREAK_LIB
    IN_LIBLINEBREAK
    ENABLE_RELOCATABLE=1
    IN_LIBRARY
    NO_XMALLOC
    set_relocation_prefix=liblinebreak_set_relocation_prefix
    relocate=liblinebreak_relocate
    DEPENDS_ON_LIBICONV=1
)

# Platform-specific definitions
if(APPLE)
    target_compile_definitions(linebreak PRIVATE
        LOCALEDIR=\"${LOCALE_DIR}\"
        LOCALE_ALIAS_PATH=\"${LOCALE_DIR}\"
        LIBDIR=\"${TOP_SRC_DIR}\"
        INSTALLDIR=\"${PREFIX}\"
    )
else()
    target_compile_definitions(linebreak PRIVATE
        LOCALEDIR=\"${LOCALE_DIR}\"
        LOCALE_ALIAS_PATH=\"${LOCALE_DIR}\"
        LIBDIR=\"${TOP_SRC_DIR}\"
        INSTALLDIR=\"${PREFIX}\"
    )
endif()

# Link dependencies
if(ICONV_FOUND)
    target_link_libraries(linebreak PRIVATE ${ICONV_LIBRARIES})
    target_include_directories(linebreak PRIVATE ${ICONV_INCLUDE_DIRS})
endif()

# Installation rules (if needed)
install(TARGETS linebreak
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${LINEBREAK_HEADERS}
    DESTINATION include/linebreak
)