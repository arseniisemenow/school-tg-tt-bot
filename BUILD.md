# Build Instructions

## Prerequisites

- C++20 compiler (GCC 10+, Clang 12+, or MSVC 2019+)
- CMake 4.0 or higher
- Conan 2.x
- PostgreSQL development libraries (for libpqxx)

## Building with Conan

### Step 1: Install Dependencies

Run Conan to install dependencies:

```bash
conan install . --output-folder=build --build=missing
```

This will:
- Install all dependencies specified in `conanfile.txt`
- Generate CMake toolchain files in the `build` directory

### Step 2: Configure CMake

If using Conan toolchain (recommended):

```bash
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
```

Or from the project root:

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake
```

### Step 3: Build

```bash
cmake --build build
```

Or using the build system directly:

```bash
cd build
make  # or ninja, depending on generator
```

## Building without Conan (Manual Dependencies)

If you prefer to install dependencies manually:

1. Install system packages:
   - `libpqxx-dev` (PostgreSQL C++ client)
   - `libcurl4-openssl-dev` (HTTP client)
   - `nlohmann-json3-dev` (JSON library)

2. Configure CMake:
   ```bash
   cmake -S . -B build
   ```

3. Build:
   ```bash
   cmake --build build
   ```

## Dependencies

### Required

- **nlohmann_json**: JSON parsing for configuration
- **libcurl**: HTTP client for School21 API
- **libpqxx**: PostgreSQL C++ client library

### Optional (for full functionality)

- **tgbotxx**: Telegram Bot API library (may need manual installation)
- **opentelemetry-cpp**: OpenTelemetry C++ SDK (for observability)

## Troubleshooting

### "nlohmann_json not found"

Make sure you've run `conan install` first, or install the system package:
- Ubuntu/Debian: `sudo apt-get install nlohmann-json3-dev`
- Or use Conan: `conan install . --output-folder=build --build=missing`

### "libcurl not found"

Install libcurl development package:
- Ubuntu/Debian: `sudo apt-get install libcurl4-openssl-dev`
- Or use Conan: `conan install . --output-folder=build --build=missing`

### "libpqxx not found"

Install PostgreSQL development libraries:
- Ubuntu/Debian: `sudo apt-get install libpqxx-dev`
- Or use Conan: `conan install . --output-folder=build --build=missing`

### Conan package not found

Some packages may not be available in the default Conan remote. Try:

```bash
conan remote add conancenter https://center.conan.io
conan install . --output-folder=build --build=missing
```

## IDE Integration

### CLion

1. Open the project in CLion
2. CLion will detect CMakeLists.txt
3. If using Conan, configure CMake options:
   - Go to Settings → Build, Execution, Deployment → CMake
   - Add CMake options: `-DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake`
   - Set build directory to include Conan toolchain path

### VS Code

1. Install CMake Tools extension
2. Run `conan install` first
3. Configure CMake with toolchain file
4. Build using CMake Tools

## Running the Application

After building:

```bash
./build/school_tg_tt_bot
```

Make sure to:
1. Set up `.env` file with required environment variables
2. Configure database connection
3. Run database migrations

See `README.md` for runtime configuration.

