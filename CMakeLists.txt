# This script was sloppily frankensteined from a handful of tux4kids'
# other CMakeLists.txt and is not safe for human consumption. 
# Please hack responsibly.
# Migrated to SDL3 from SDL1

cmake_minimum_required(VERSION 3.16)  # SDL3 requires minimum CMake 3.16
set(PACKAGE_NAME "Tux4Kids_common")
set(PACKAGE_VERSION 1.0.0)
set(T4K_COMMON_PACKAGE_STRING "${PACKAGE_NAME}-${PACKAGE_VERSION}")
set(LIB_NAME "t4k_common")
set(VERSION ${PACKAGE_VERSION})
project(${PACKAGE_NAME} C CXX)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake-modules")
include(T4K_Sugar)

# Enable modern CMake features
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set compiler warnings
set(T4K_DEVELOPER_WARNINGS OFF CACHE STRING "Change or override to enable warnings")
if(CMAKE_BUILD_TYPE MATCHES Release)
    set(T4K_DEVELOPER_WARNINGS ON)
endif()                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 

# Find SDL3 and required dependencies
find_package(SDL3 REQUIRED)
find_package(SDL3_ttf REQUIRED)
find_package(SDL3_image REQUIRED)
find_package(SDL3_mixer REQUIRED)

# Set SDL3 compile definitions
add_compile_definitions(SDL_MAIN_HANDLED)

# Find SDL3 companion libraries
find_package(SDL3_image REQUIRED)
find_package(SDL3_mixer REQUIRED) 
find_package(SDL3_ttf REQUIRED)

# Configure paths and dependencies
set(CMAKE_PREFIX_PATH 
    ${CMAKE_PREFIX_PATH}
    /usr/local/lib/cmake/SDL3
    /usr/local/lib/cmake/SDL3_image
    /usr/local/lib/cmake/SDL3_mixer
    /usr/local/lib/cmake/SDL3_ttf
)

# Set compiler flags for SDL3
add_compile_definitions(SDL_MAIN_HANDLED)
add_compile_definitions(SDL_DISABLE_OLD_FUNCTIONS)

find_package(SDL3_mixer)
if(SDL3_mixer_FOUND)
    set(HAVE_SDL3_MIXER 1)
    message(STATUS "Found SDL3_mixer")
else()
    message(WARNING "SDL3_mixer not found - will use alternative audio")
    # Alternative audio libraries
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(OPENAL openal)
        pkg_check_modules(VORBIS vorbisfile vorbis ogg)
    endif()
endif()

# Text rendering - SDL3 doesn't have SDL_ttf equivalent yet
# Check for SDL_Pango first (if it works with SDL3), then alternatives
find_package(SDL_Pango)
if(SDLPANGO_FOUND)
    set(HAVE_LIBSDL_PANGO 1)
    message(STATUS "Found SDL_Pango - checking SDL3 compatibility")
else()
    # Use modern text rendering alternatives
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(PANGO pango pangocairo)
        pkg_check_modules(FREETYPE freetype2)
        pkg_check_modules(HARFBUZZ harfbuzz)
    endif()
    
    if(PANGO_FOUND)
        set(HAVE_LIBPANGO 1)
        message(STATUS "Using Pango for text rendering")
    elseif(FREETYPE_FOUND)
        set(HAVE_LIBFREETYPE 1) 
        message(STATUS "Using FreeType for text rendering")
    else()
        message(FATAL_ERROR "No text rendering library found")
    endif()
endif()

# SVG loading and caching
find_package(PNG)
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(RSVG librsvg-2.0)
    pkg_check_modules(XML2 libxml-2.0)
endif()

set(HAVE_RSVG ${RSVG_FOUND})
set(HAVE_LIBPNG ${PNG_FOUND})
set(HAVE_LIBXML2 ${XML2_FOUND})

# Networking - SDL3 has no SDL_net equivalent, use alternatives
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(CURL libcurl)
endif()

if(CURL_FOUND)
    set(HAVE_LIBCURL 1)
    message(STATUS "Using libcurl for networking")
else()
    message(WARNING "No networking library found - network features disabled")
    set(HAVE_LIBSDL_NET 0)
endif()

message("SDL_Pango: ${SDLPANGO_FOUND}")
message("Pango: ${PANGO_FOUND}")
message("FreeType: ${FREETYPE_FOUND}")
message("libcurl: ${CURL_FOUND}")

# Platform-specific data paths (unchanged from original)
if(APPLE)
    set(MACOSX_BUNDLE_NAME ${PACKAGE_NAME})
    set(DATA_PREFIX ${T4K_COMMON_MACOSX_BUNDLE_NAME}.app/Contents/Resources)
elseif(UNIX)
    set(DATA_PREFIX ${CMAKE_INSTALL_PREFIX}/share/${LIB_NAME})
elseif(WIN32)
    t4k_to_unix_path(DATA_PREFIX "\$ENV{APPDATA}/${PACKAGE_NAME}")
else()
    message(SEND_ERROR "What OS are you on?")
endif()

set(LINEBREAK_BINARY_DIR ${CMAKE_BINARY_DIR}/src/linebreak)
set(LINEBREAK_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/linebreak)

# Process subdirectories
add_subdirectory(src)
add_subdirectory(data)

include(CPack)

# Updated package dependencies for SDL3
set(DEP_PKGS "sdl3 >= 3.0.0")

# Add image support
if(SDL3_image_FOUND)
    set(DEP_PKGS "${DEP_PKGS} SDL3_image")
elseif(PNG_FOUND)
    set(DEP_PKGS "${DEP_PKGS} libpng")
endif()

# Add SVG support
if(RSVG_FOUND)
    set(DEP_PKGS "${DEP_PKGS} librsvg-2.0")
endif()
if(XML2_FOUND)
    set(DEP_PKGS "${DEP_PKGS} libxml-2.0")
endif()

# Add text rendering
if(SDLPANGO_FOUND)
    set(DEP_PKGS "${DEP_PKGS} SDL_Pango")
elseif(PANGO_FOUND)
    set(DEP_PKGS "${DEP_PKGS} pango pangocairo")
elseif(FREETYPE_FOUND)
    set(DEP_PKGS "${DEP_PKGS} freetype2")
endif()

# Libraries that need to be linked
set(DEP_LIBS "")

if(SDL3_mixer_FOUND)
    set(DEP_LIBS "${DEP_LIBS} -lSDL3_mixer")
elseif(OPENAL_FOUND)
    set(DEP_LIBS "${DEP_LIBS} ${OPENAL_LIBRARIES}")
endif()

if(CURL_FOUND)
    set(DEP_LIBS "${DEP_LIBS} ${CURL_LIBRARIES}")
endif()

if(VORBIS_FOUND)
    set(DEP_LIBS "${DEP_LIBS} ${VORBIS_LIBRARIES}")
endif()

# Platform-specific private libraries (updated for SDL3)
if(WIN32)
    # Windows - updated for SDL3 requirements
    set(DEP_LIBS_PRIVATE "-luser32 -lgdi32 -lwinmm -lole32 -loleaut32 -limm32 -lversion -lshell32 -lsetupapi")
    if(MINGW)
        set(DEP_LIBS_PRIVATE "${DEP_LIBS_PRIVATE} -lmingw32 -mwindows")
    endif()
elseif(APPLE)
    # macOS - SDL3 frameworks
    set(DEP_LIBS_PRIVATE "-framework Cocoa -framework IOKit -framework CoreVideo -framework CoreAudio -framework AudioToolbox -framework ForceFeedback -framework Metal -framework GameController")
else()
    # Linux/Unix
    set(DEP_LIBS_PRIVATE "-lm -lpthread")
    if(FREETYPE_FOUND)
        set(DEP_LIBS_PRIVATE "${DEP_LIBS_PRIVATE} ${FREETYPE_LIBRARIES}")
    endif()
    if(PANGO_FOUND)
        set(DEP_LIBS_PRIVATE "${DEP_LIBS_PRIVATE} ${PANGO_LIBRARIES}")
    endif()
endif()