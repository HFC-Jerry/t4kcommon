# This script was sloppily frankensteined from a handful of tux4kids'
# other CMakeLists.txt and is not safe for human consumption. 
# Please hack responsibly.
# Updated for SDL3 migration

cmake_minimum_required(VERSION 3.16)  # Match main CMakeLists.txt

# Set modern CMake policies
if(POLICY CMP0022)
    cmake_policy(SET CMP0022 NEW)
endif()

include(ConfigureChecks)
configure_file(${CMAKE_SOURCE_DIR}/config.h.cmake ${CMAKE_BINARY_DIR}/config.h)
include(T4K_Sugar)

# Cmake module contains the source files for this library
include(${CMAKE_SOURCE_DIR}/cmake-modules/Files.cmake)

find_package(LibXml2 REQUIRED)

# SDL3 and companion libraries should already be found in main CMakeLists.txt
# Use the modern target-based approach for include directories
include_directories(
    ${LIBXML2_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/gettext
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/src
    ${CMAKE_BINARY_DIR}/gettext
    ${CMAKE_BINARY_DIR}/src/linebreak
    ${CMAKE_SOURCE_DIR}/src/linebreak
)

# SDL3 includes are handled by target linking, but add if needed
if(SDL3_FOUND)
    # SDL3 uses target-based includes, handled automatically
endif()

# Look here for optional headers (updated for SDL3 equivalents)
if(SDL3_image_FOUND)
    # SDL3_image includes handled by target linking
elseif(PNG_FOUND)
    include_directories(${PNG_INCLUDE_DIRS})
endif()

if(SDLPANGO_FOUND)
    t4k_include_directory(SDLPANGO)
elseif(PANGO_FOUND)
    include_directories(${PANGO_INCLUDE_DIRS})
elseif(FREETYPE_FOUND)
    include_directories(${FREETYPE_INCLUDE_DIRS})
endif()

if(RSVG_FOUND)
    include_directories(${RSVG_INCLUDE_DIRS})
endif()

set(T4K_COMMON_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
if(T4K_DEVELOPER_WARNINGS)
    list(APPEND CMAKE_C_FLAGS -Wall) 
endif()

# Declare the library as a shared library
add_library(${LIB_NAME} SHARED
    ${T4K_COMMON_SOURCES} ${T4K_COMMON_HEADERS})

# Link SDL3 and companion libraries
target_link_libraries(${LIB_NAME} 
    PUBLIC 
        SDL3::SDL3
        SDL3_image::SDL3_image
        SDL3_mixer::SDL3_mixer 
        SDL3_ttf::SDL3_ttf
    PRIVATE
        ${LIBXML2_LIBRARIES}
        ${FREETYPE_LIBRARIES}
        ${RSVG_LIBRARIES}
        ${PANGO_LIBRARIES}
        ${LIBCURL_LIBRARIES}
)

# Text rendering libraries
if(SDLPANGO_FOUND)
    target_link_libraries(${LIB_NAME} PRIVATE ${SDLPANGO_LIBRARY})
elseif(PANGO_FOUND)
    target_link_libraries(${LIB_NAME} PRIVATE ${PANGO_LIBRARIES})
elseif(FREETYPE_FOUND)
    target_link_libraries(${LIB_NAME} PRIVATE ${FREETYPE_LIBRARIES})
endif()

# SVG support
if(RSVG_FOUND)
    target_link_libraries(${LIB_NAME} PRIVATE ${RSVG_LIBRARIES})
endif()

# Networking
if(CURL_FOUND)
    target_link_libraries(${LIB_NAME} PRIVATE ${CURL_LIBRARIES})
endif()

# Link linebreak library (now using target instead of file path)
target_link_libraries(${LIB_NAME} PRIVATE linebreak)

# Modern way to set compile definitions
target_compile_definitions(${LIB_NAME} PRIVATE
    COMMON_DATA_PREFIX="${DATA_PREFIX}"
    USE_T4K_PREFIX=1
    PACKAGE_STRING="${PACKAGE_NAME}"
)

# Apply additional compile flags if needed
if(RSVG_FOUND)
    target_compile_options(${LIB_NAME} PRIVATE ${RSVG_CFLAGS_OTHER})
endif()

t4k_include_definition(HAVE_LIBSDL_PANGO)
t4k_include_definition(HAVE_LIBPNG)

# Set target properties (fix the CMP0022 warning)
set_target_properties(${LIB_NAME} PROPERTIES 
    LINK_INTERFACE_LIBRARIES ""  # Clear old interface libraries
)

add_dependencies(${LIB_NAME} linebreak)

install(TARGETS ${LIB_NAME}
    DESTINATION lib) 
install(FILES ${T4K_COMMON_SOURCE_DIR}/t4k_common.h
    ${T4K_COMMON_SOURCE_DIR}/t4k_alphasort.h
    ${T4K_COMMON_SOURCE_DIR}/t4k_scandir.h
    DESTINATION include)

include(UseDoxygen OPTIONAL)

# Process subdirectories
add_subdirectory(linebreak)

# Build t4k_test app (updated for SDL3)
set(T4K_TESTAPP t4k_test)
add_executable(${T4K_TESTAPP} ${T4K_TEST_SOURCES})

# Link test app with modern target-based approach
target_link_libraries(${T4K_TESTAPP} PRIVATE
    ${LIB_NAME}  # This will automatically bring in SDL3 and other dependencies
    ${LIBXML2_LIBRARIES}
)

# Add any additional libraries needed for the test app
if(RSVG_FOUND)
    target_link_libraries(${T4K_TESTAPP} PRIVATE ${RSVG_LIBRARIES})
endif()

if(CURL_FOUND)
    target_link_libraries(${T4K_TESTAPP} PRIVATE ${CURL_LIBRARIES})
endif()