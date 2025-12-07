cmake_minimum_required(VERSION 3.20)
project(school_tg_tt_bot VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Fetch tgbotxx using FetchContent
include(FetchContent)
FetchContent_Declare(tgbotxx
    GIT_REPOSITORY "https://github.com/baderouaich/tgbotxx"
    GIT_TAG "v1.1.9.2"
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(tgbotxx)

# Fetch Google Test
FetchContent_Declare(googletest
    GIT_REPOSITORY "https://github.com/google/googletest.git"
    GIT_TAG "v1.14.0"
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(googletest)

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

# Find system packages (installed via apt-get)
find_package(nlohmann_json REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)

# Find libpqxx using pkg-config (libpqxx-dev doesn't provide CMake config files)
pkg_check_modules(PQXX libpqxx REQUIRED)
if(NOT PQXX_FOUND)
    # Fallback: manually find libpqxx
    find_library(PQXX_LIBRARIES pqxx PATHS /usr/lib /usr/lib/x86_64-linux-gnu)
    find_path(PQXX_INCLUDE_DIRS pqxx/pqxx PATHS /usr/include)
    if(PQXX_LIBRARIES AND PQXX_INCLUDE_DIRS)
        set(PQXX_FOUND TRUE)
        set(PQXX_LIBRARIES ${PQXX_LIBRARIES})
        set(PQXX_INCLUDE_DIRS ${PQXX_INCLUDE_DIRS})
    else()
        message(FATAL_ERROR "libpqxx not found. Please install libpqxx-dev")
    endif()
endif()

# Create imported target for libpqxx (since it doesn't provide CMake config files)
if(NOT TARGET libpqxx::pqxx)
    add_library(libpqxx::pqxx INTERFACE IMPORTED)
    set_target_properties(libpqxx::pqxx PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${PQXX_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "${PQXX_LIBRARIES};pq"
    )
endif()

# Find CURL
pkg_check_modules(CURL libcurl)
if(NOT CURL_FOUND)
    find_library(CURL_LIBRARIES curl PATHS /usr/lib /usr/lib/x86_64-linux-gnu)
    find_path(CURL_INCLUDE_DIRS curl/curl.h PATHS /usr/include)
    if(CURL_LIBRARIES AND CURL_INCLUDE_DIRS)
        set(CURL_FOUND TRUE)
    else()
        message(FATAL_ERROR "CURL not found. Please install libcurl4-openssl-dev")
    endif()
endif()

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    nlohmann_json::nlohmann_json
    libpqxx::pqxx
    OpenSSL::SSL
    OpenSSL::Crypto
    tgbotxx
)

# Link CURL
if(CURL_FOUND)
    if(CURL_LIBRARIES)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${CURL_LIBRARIES})
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE curl)
    endif()
    if(CURL_INCLUDE_DIRS)
        target_include_directories(${PROJECT_NAME} PRIVATE ${CURL_INCLUDE_DIRS})
    endif()
endif()

# Compiler options
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# Debug symbols
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${PROJECT_NAME} PRIVATE -g)
endif()

# Enable testing
enable_testing()

# Test source files
file(GLOB_RECURSE TEST_SOURCES "tests/**/*.cpp")

if(TEST_SOURCES)
    file(GLOB_RECURSE TEST_LIB_SOURCES
        "src/database/*.cpp"
        "src/repositories/*.cpp"
        "src/utils/*.cpp"
        "src/observability/*.cpp"
        "src/bot/*.cpp"
        "src/school21/*.cpp"
        "src/config/*.cpp"
    )
    
    add_executable(${PROJECT_NAME}_tests
        ${TEST_SOURCES}
        ${TEST_LIB_SOURCES}
    )
    
    target_include_directories(${PROJECT_NAME}_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
    )
    
    target_link_libraries(${PROJECT_NAME}_tests PRIVATE
        nlohmann_json::nlohmann_json
        libpqxx::pqxx
        OpenSSL::SSL
        OpenSSL::Crypto
        tgbotxx
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
    )
    
    if(CURL_FOUND)
        if(CURL_LIBRARIES)
            target_link_libraries(${PROJECT_NAME}_tests PRIVATE ${CURL_LIBRARIES})
        else()
            target_link_libraries(${PROJECT_NAME}_tests PRIVATE curl)
        endif()
        if(CURL_INCLUDE_DIRS)
            target_include_directories(${PROJECT_NAME}_tests PRIVATE ${CURL_INCLUDE_DIRS})
        endif()
    endif()
    
    target_compile_options(${PROJECT_NAME}_tests PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
    
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(${PROJECT_NAME}_tests PRIVATE -g)
    endif()
    
    add_test(NAME ${PROJECT_NAME}_tests COMMAND ${PROJECT_NAME}_tests)
endif()
