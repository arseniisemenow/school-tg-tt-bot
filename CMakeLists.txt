cmake_minimum_required(VERSION 3.20)
project(school_tg_tt_bot VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Conan 2.x integration
# If using Conan, include the toolchain file:
# cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/conan_toolchain.cmake")
    include("${CMAKE_CURRENT_BINARY_DIR}/conan_toolchain.cmake")
    message(STATUS "Using Conan toolchain")
    
    # Fix for ZLIB and OpenSSL package issues: Conan packages don't properly declare system libraries
    # We need to find system libraries and link them when needed
    find_library(ZLIB_Z_LIBRARY z PATHS /usr/lib /usr/lib/x86_64-linux-gnu)
    if(ZLIB_Z_LIBRARY)
        message(STATUS "Found system zlib library: ${ZLIB_Z_LIBRARY}")
        # Create an imported target for 'z' library if it doesn't exist
        if(NOT TARGET zlib::z)
            add_library(zlib::z INTERFACE IMPORTED)
            set_target_properties(zlib::z PROPERTIES
                INTERFACE_LINK_LIBRARIES "${ZLIB_Z_LIBRARY}"
            )
        endif()
    endif()
    
    # Fix for OpenSSL: find system OpenSSL libraries
    find_library(OPENSSL_SSL_LIBRARY ssl PATHS /usr/lib /usr/lib/x86_64-linux-gnu)
    find_library(OPENSSL_CRYPTO_LIBRARY crypto PATHS /usr/lib /usr/lib/x86_64-linux-gnu)
    if(OPENSSL_SSL_LIBRARY AND OPENSSL_CRYPTO_LIBRARY)
        message(STATUS "Found system OpenSSL libraries: ${OPENSSL_SSL_LIBRARY}, ${OPENSSL_CRYPTO_LIBRARY}")
        # Create imported targets for OpenSSL libraries if they don't exist
        if(NOT TARGET OpenSSL::SSL)
            add_library(OpenSSL::SSL INTERFACE IMPORTED)
            set_target_properties(OpenSSL::SSL PROPERTIES
                INTERFACE_LINK_LIBRARIES "${OPENSSL_SSL_LIBRARY}"
            )
        endif()
        if(NOT TARGET OpenSSL::Crypto)
            add_library(OpenSSL::Crypto INTERFACE IMPORTED)
            set_target_properties(OpenSSL::Crypto PROPERTIES
                INTERFACE_LINK_LIBRARIES "${OPENSSL_CRYPTO_LIBRARY}"
            )
        endif()
    endif()
else()
    message(STATUS "Conan toolchain not found. Run: conan install . --output-folder=build --build=missing")
endif()

# Fetch tgbotxx using FetchContent
include(FetchContent)
FetchContent_Declare(tgbotxx
    GIT_REPOSITORY "https://github.com/baderouaich/tgbotxx"
    GIT_TAG "v1.1.9.2"  # Compatible with Telegram Api 9.2
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(tgbotxx)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.h"
)

# Headers
file(GLOB_RECURSE HEADERS
    "include/*.h"
    "include/*.hpp"
)

# Executable
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    main.cpp
)

# Find packages (via Conan CMakeDeps or system)
# Conan generates these find_package() calls automatically
find_package(nlohmann_json REQUIRED)

# Use system CURL (Conan CURL package has CMake target issues with ALIAS targets)
# Skip Conan's CURL and use system CURL directly
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(CURL libcurl)
endif()
if(NOT CURL_FOUND)
    # Use system CURL directly
    find_library(CURL_LIBRARIES 
        NAMES curl libcurl
        PATHS /usr/lib /usr/lib/x86_64-linux-gnu
    )
    find_path(CURL_INCLUDE_DIRS 
        NAMES curl/curl.h
        PATHS /usr/include
    )
    if(CURL_LIBRARIES AND CURL_INCLUDE_DIRS)
        set(CURL_FOUND TRUE)
        message(STATUS "Found system CURL: ${CURL_LIBRARIES}")
    else()
        message(FATAL_ERROR "CURL not found. Please install libcurl4-openssl-dev")
    endif()
endif()
find_package(libpqxx REQUIRED)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    nlohmann_json::nlohmann_json
    libpqxx::pqxx
    tgbotxx
)

# Ensure libpqxx include directories are added
# Note: Conan builds libpqxx in Release mode, but we're building in Debug
# The target uses generator expressions, so we need to ensure includes work for both
if(TARGET libpqxx::pqxx)
    # Read the include directory from the Conan-generated file
    include("${CMAKE_CURRENT_BINARY_DIR}/libpqxx-release-x86_64-data.cmake")
    if(libpqxx_INCLUDE_DIRS_RELEASE)
        message(STATUS "Adding libpqxx include dir: ${libpqxx_INCLUDE_DIRS_RELEASE}")
        target_include_directories(${PROJECT_NAME} PRIVATE ${libpqxx_INCLUDE_DIRS_RELEASE})
    endif()
endif()

# Link CURL (use system CURL, skip Conan's CURL due to ALIAS target issues)
if(CURL_FOUND)
    if(CURL_LIBRARIES)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${CURL_LIBRARIES})
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE curl)
    endif()
    if(CURL_INCLUDE_DIRS)
        target_include_directories(${PROJECT_NAME} PRIVATE ${CURL_INCLUDE_DIRS})
    endif()
else()
    message(FATAL_ERROR "CURL not found")
endif()

message(STATUS "Found nlohmann_json")
message(STATUS "Found CURL")
message(STATUS "Found libpqxx")

# Compiler options
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# Debug symbols
set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${PROJECT_NAME} PRIVATE -g)
endif()
