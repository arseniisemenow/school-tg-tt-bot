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

# Ensure libpqxx include directories and libraries are added
# Note: Conan builds libpqxx in Release mode, but we're building in Debug
# The target uses generator expressions, so we need to ensure includes and libs work for both
if(TARGET libpqxx::pqxx)
    # Read the include directory and library path from the Conan-generated file
    include("${CMAKE_CURRENT_BINARY_DIR}/libpqxx-release-x86_64-data.cmake")
    if(libpqxx_INCLUDE_DIRS_RELEASE)
        message(STATUS "Adding libpqxx include dir: ${libpqxx_INCLUDE_DIRS_RELEASE}")
        target_include_directories(${PROJECT_NAME} PRIVATE ${libpqxx_INCLUDE_DIRS_RELEASE})
    endif()
    # Manually link the library since generator expressions don't work for Debug builds
    if(libpqxx_PACKAGE_FOLDER_RELEASE)
        find_library(LIBPQXX_LIB 
            NAMES pqxx libpqxx libpqxx-7.9
            PATHS "${libpqxx_PACKAGE_FOLDER_RELEASE}/lib"
            NO_DEFAULT_PATH
        )
        if(LIBPQXX_LIB)
            message(STATUS "Found libpqxx library: ${LIBPQXX_LIB}")
            target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBPQXX_LIB})
        endif()
        # Also link PostgreSQL client library (libpq) and its dependencies
        # Find libpq from Conan cache - need to link all required libraries
        file(GLOB LIBPQ_CONAN_PATHS "/root/.conan2/p/b/libpq*/p/lib/libpq.a")
        if(LIBPQ_CONAN_PATHS)
            get_filename_component(LIBPQ_DIR "${LIBPQ_CONAN_PATHS}" DIRECTORY)
            list(GET LIBPQ_CONAN_PATHS 0 LIBPQ_LIB)
            message(STATUS "Found libpq library: ${LIBPQ_LIB}")
            target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBPQ_LIB})
            # Link additional PostgreSQL libraries that libpq depends on
            find_library(LIBPQ_COMMON_LIB pgcommon PATHS "${LIBPQ_DIR}" NO_DEFAULT_PATH)
            find_library(LIBPQ_PORT_LIB pgport PATHS "${LIBPQ_DIR}" NO_DEFAULT_PATH)
            if(LIBPQ_COMMON_LIB)
                target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBPQ_COMMON_LIB})
            endif()
            if(LIBPQ_PORT_LIB)
                target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBPQ_PORT_LIB})
            endif()
        else()
            # Fallback to system libpq
            find_library(LIBPQ_SYSTEM_LIB pq PATHS /usr/lib /usr/lib/x86_64-linux-gnu)
            if(LIBPQ_SYSTEM_LIB)
                message(STATUS "Found system libpq library: ${LIBPQ_SYSTEM_LIB}")
                target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBPQ_SYSTEM_LIB})
            else()
                message(STATUS "Linking system libpq")
                target_link_libraries(${PROJECT_NAME} PRIVATE pq)
            endif()
        endif()
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

# Link OpenSSL libraries (needed by tgbotxx/CPR)
# tgbotxx uses OpenSSL from Conan, but we need to ensure crypto library is linked
if(OPENSSL_SSL_LIBRARY AND OPENSSL_CRYPTO_LIBRARY)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
else()
    # Fallback to system OpenSSL
    find_library(OPENSSL_SSL_LIB ssl PATHS /usr/lib /usr/lib/x86_64-linux-gnu)
    find_library(OPENSSL_CRYPTO_LIB crypto PATHS /usr/lib /usr/lib/x86_64-linux-gnu)
    if(OPENSSL_SSL_LIB AND OPENSSL_CRYPTO_LIB)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENSSL_SSL_LIB} ${OPENSSL_CRYPTO_LIB})
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE ssl crypto)
    endif()
endif()

# Link zlib (needed by curl which is used by tgbotxx/CPR)
if(ZLIB_Z_LIBRARY)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${ZLIB_Z_LIBRARY})
else()
    find_library(ZLIB_LIB z PATHS /usr/lib /usr/lib/x86_64-linux-gnu)
    if(ZLIB_LIB)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${ZLIB_LIB})
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE z)
    endif()
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

# Enable testing
enable_testing()

# Test source files
file(GLOB_RECURSE TEST_SOURCES
    "tests/**/*.cpp"
)

if(TEST_SOURCES)
    # Create test executable
    # Include source files needed for tests (exclude config as tests don't need it)
    file(GLOB_RECURSE TEST_LIB_SOURCES
        "src/database/*.cpp"
        "src/repositories/*.cpp"
        "src/utils/*.cpp"
        "src/observability/*.cpp"
    )
    
    add_executable(${PROJECT_NAME}_tests
        ${TEST_SOURCES}
        ${TEST_LIB_SOURCES}
    )
    
    # Include directories
    target_include_directories(${PROJECT_NAME}_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    
    # Get nlohmann_json include directories from Conan
    if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/nlohmann_json-release-x86_64-data.cmake")
        include("${CMAKE_CURRENT_BINARY_DIR}/nlohmann_json-release-x86_64-data.cmake")
        if(nlohmann_json_INCLUDE_DIRS_RELEASE)
            target_include_directories(${PROJECT_NAME}_tests PRIVATE ${nlohmann_json_INCLUDE_DIRS_RELEASE})
        endif()
    endif()
    
    # Link test executable with same libraries as main executable
    # nlohmann_json is already found above for main target
    target_link_libraries(${PROJECT_NAME}_tests PRIVATE
        nlohmann_json::nlohmann_json
        libpqxx::pqxx
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
    )
    
    # Link same libraries as main executable
    if(TARGET libpqxx::pqxx)
        if(libpqxx_INCLUDE_DIRS_RELEASE)
            target_include_directories(${PROJECT_NAME}_tests PRIVATE ${libpqxx_INCLUDE_DIRS_RELEASE})
        endif()
        if(LIBPQXX_LIB)
            target_link_libraries(${PROJECT_NAME}_tests PRIVATE ${LIBPQXX_LIB})
        endif()
        if(LIBPQ_LIB)
            target_link_libraries(${PROJECT_NAME}_tests PRIVATE ${LIBPQ_LIB})
        endif()
        if(LIBPQ_COMMON_LIB)
            target_link_libraries(${PROJECT_NAME}_tests PRIVATE ${LIBPQ_COMMON_LIB})
        endif()
        if(LIBPQ_PORT_LIB)
            target_link_libraries(${PROJECT_NAME}_tests PRIVATE ${LIBPQ_PORT_LIB})
        endif()
    endif()
    
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
    
    if(OPENSSL_SSL_LIBRARY AND OPENSSL_CRYPTO_LIBRARY)
        target_link_libraries(${PROJECT_NAME}_tests PRIVATE ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
    endif()
    
    if(ZLIB_Z_LIBRARY)
        target_link_libraries(${PROJECT_NAME}_tests PRIVATE ${ZLIB_Z_LIBRARY})
    endif()
    
    # Compiler options for tests
    target_compile_options(${PROJECT_NAME}_tests PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
    
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(${PROJECT_NAME}_tests PRIVATE -g)
    endif()
    
    # Add test to CTest
    add_test(NAME ${PROJECT_NAME}_tests COMMAND ${PROJECT_NAME}_tests)
    
    message(STATUS "Tests enabled: ${PROJECT_NAME}_tests")
endif()
