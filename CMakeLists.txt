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
# Use system CURL (Conan CURL package has CMake target issues)
# Try pkg-config first, then FindCURL
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(CURL libcurl)
endif()
if(NOT CURL_FOUND)
    find_package(CURL REQUIRED)
endif()
find_package(libpqxx REQUIRED)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    nlohmann_json::nlohmann_json
    libpqxx::pqxx
    tgbotxx
)

# Link CURL (handle Conan target issues)
if(TARGET CURL::libcurl)
    target_link_libraries(${PROJECT_NAME} PRIVATE CURL::libcurl)
elseif(TARGET libcurl::libcurl)
    target_link_libraries(${PROJECT_NAME} PRIVATE libcurl::libcurl)
elseif(CURL_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${CURL_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PRIVATE ${CURL_INCLUDE_DIRS})
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
