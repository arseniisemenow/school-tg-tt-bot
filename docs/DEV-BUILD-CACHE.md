# Development Build Cache

## Overview

The development setup uses Docker volumes to cache the build directory between deployments. This significantly speeds up rebuilds by reusing:

- **Conan dependencies** (libraries, packages)
- **CMake cache** (configuration, includes)
- **Object files** (compiled source files)

## How It Works

### First Deployment

1. Build directory is empty
2. Conan installs all dependencies (~2-5 minutes)
3. CMake configures the project
4. Full build compiles all source files (~3-5 minutes)
5. Build artifacts are stored in Docker volume

### Subsequent Deployments

1. Build directory is loaded from cached volume
2. Conan dependencies are reused (skipped)
3. CMake cache is reused (quick check)
4. **Incremental build** - only changed files are recompiled (~10-30 seconds)

## Usage

### Build with Cache (Default)

```bash
# Build using Dockerfile.dev (with cache support)
docker build -t school-tg-tt-bot:dev -f Dockerfile.dev .

# Deploy (build directory is cached automatically)
./scripts/deploy-dev.sh
```

### Clear Build Cache

If you need to do a clean build:

```bash
# Remove the cached volumes
docker volume rm school-tg-bot_build_cache school-tg-bot_conan_cache

# Or remove the entire stack (which removes volumes)
docker stack rm school-tg-bot
```

### Check Cache Status

```bash
# List volumes
docker volume ls | grep school-tg-bot

# Inspect volume
docker volume inspect school-tg-bot_build_cache
```

## What Gets Cached

### `build_cache` Volume (`/build/build`)

- CMake cache files (`CMakeCache.txt`, `CMakeFiles/`)
- Object files (`.o` files)
- Compiled binaries
- Conan toolchain files
- FetchContent downloads (tgbotxx, googletest)

### `conan_cache` Volume (`/build/.conan2`)

- Conan package cache
- Installed dependencies (libpqxx, nlohmann_json, etc.)
- Conan profiles and configuration

## Performance

### First Build
- Conan install: ~2-5 minutes
- CMake configure: ~10-30 seconds
- Full compile: ~3-5 minutes
- **Total: ~5-10 minutes**

### Cached Build (no source changes)
- Conan install: **skipped** (0 seconds)
- CMake configure: **quick check** (~5 seconds)
- Compile: **skipped** (0 seconds)
- **Total: ~5-10 seconds**

### Incremental Build (source changed)
- Conan install: **skipped** (0 seconds)
- CMake configure: **quick check** (~5 seconds)
- Compile: **only changed files** (~10-30 seconds)
- **Total: ~15-40 seconds**

## Troubleshooting

### Build Cache Not Working

**Problem:** Build takes full time every deployment

**Solutions:**
1. Check volumes exist: `docker volume ls | grep school-tg-bot`
2. Verify volumes are mounted: Check `docker-stack.dev.yml`
3. Check container logs: `docker service logs school-tg-bot_bot`

### Stale Build Cache

**Problem:** Changes not reflected after rebuild

**Solutions:**
1. Clear build cache: `docker volume rm school-tg-bot_build_cache`
2. Force clean build: Remove stack and redeploy
3. Check source code is copied: Verify files in container

### Volume Permission Issues

**Problem:** Permission denied errors

**Solutions:**
1. Check volume ownership: `docker volume inspect school-tg-bot_build_cache`
2. Ensure appuser has write access (handled by Dockerfile.dev)
3. Clear volume and recreate

## Best Practices

1. **Don't commit build directory** - Already in `.gitignore`
2. **Clear cache when dependencies change** - After modifying `conanfile.txt`
3. **Clear cache for major changes** - After major refactoring
4. **Keep cache for daily development** - Speeds up iteration

## Production vs Development

- **Production** (`Dockerfile`): Builds everything in image, no cache
- **Development** (`Dockerfile.dev`): Uses volumes for build cache

For production deployments, use the regular `Dockerfile` which creates a minimal runtime image.

