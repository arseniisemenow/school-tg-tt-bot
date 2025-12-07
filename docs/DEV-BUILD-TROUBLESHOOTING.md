# Development Build Troubleshooting

## Issue: Build Takes Forever (Hours)

### Symptom
Conan is building libcurl from source, installing hundreds of man pages and documentation files, taking hours instead of minutes.

### Root Cause
Conan is trying to build `libcurl/8.0.1` from source because:
1. No pre-built binary available for your platform
2. Building libcurl includes all documentation (man pages, etc.) which is very slow

### Solution
**We removed libcurl from conanfile.txt** - the project uses system libcurl instead (already installed in Dockerfile.dev).

If you still see this issue:
1. Clear build cache: `./scripts/clear-build-cache.sh`
2. Rebuild images (artifact-free): `./scripts/build-images.sh`
3. Rebuild artifacts: `./scripts/build-docker.sh`
4. Redeploy (bind mounts binary/config/migrations): `./scripts/deploy-dev.sh`

## Issue: Permission Denied Errors

### Symptom
```
PermissionError: [Errno 13] Permission denied: 'cmakedeps_macros.cmake'
```

### Root Cause
Docker volumes are created with root ownership, but the container runs as `appuser` (UID 1000).

### Solution
The Dockerfile.dev now includes an entrypoint script that fixes permissions automatically. If you still see issues:

1. **Clear and recreate volumes:**
   ```bash
   ./scripts/clear-build-cache.sh
   ```

2. **Manually fix permissions** (if needed):
   ```bash
   docker volume inspect school-tg-bot_build_cache
   # Check volume location, then fix permissions on host if needed
   ```

3. **Rebuild and redeploy:**
   ```bash
   ./scripts/build-images.sh
   ./scripts/deploy-dev.sh
   ```

## Issue: Conan Cache Not Working

### Symptom
Conan reinstalls dependencies every time, even though volume is mounted.

### Root Cause
Conan cache location mismatch - cache is at `/home/appuser/.conan2` but volume mounts `/build/.conan2`.

### Solution
The build script now sets `CONAN_USER_HOME=/build/.conan2` to use the mounted volume. Verify:

```bash
# Check if Conan cache volume exists
docker volume ls | grep conan_cache

# Check container logs
docker service logs school-tg-bot_bot | grep -i conan
```

## Issue: Build Directory Not Cached

### Symptom
CMake reconfigures every time, full rebuild happens.

### Root Cause
Build directory volume not mounted or permissions issue.

### Solution
1. Verify volumes in docker-stack.dev.yml:
   ```yaml
   volumes:
     - build_cache:/build/build
     - conan_cache:/build/.conan2
   ```

2. Check volumes exist:
   ```bash
   docker volume ls | grep school-tg-bot
   ```

3. Clear and recreate if needed:
   ```bash
   ./scripts/clear-build-cache.sh
   ```

## Performance Expectations

### First Build (No Cache)
- Conan install: ~2-5 minutes (without libcurl)
- CMake configure: ~10-30 seconds
- Full compile: ~2-3 minutes
- **Total: ~5-8 minutes**

### Cached Build (No Changes)
- Conan install: **skipped** (0 seconds)
- CMake configure: **quick check** (~5 seconds)
- Compile: **skipped** (0 seconds)
- **Total: ~5-10 seconds**

### Incremental Build (Source Changed)
- Conan install: **skipped** (0 seconds)
- CMake configure: **quick check** (~5 seconds)
- Compile: **only changed files** (~10-30 seconds)
- **Total: ~15-40 seconds**

## Quick Fixes

### Complete Reset
```bash
# Remove everything and start fresh
docker stack rm school-tg-bot
./scripts/clear-build-cache.sh
./scripts/build-images.sh
./scripts/deploy-dev.sh
```

### Check What's Happening
```bash
# Watch build logs in real-time
docker service logs -f school-tg-bot_bot

# Check volume usage
docker system df -v | grep school-tg-bot

# Check container status
docker service ps school-tg-bot_bot
```

### Force Clean Build
```bash
# Remove volumes and rebuild
./scripts/clear-build-cache.sh
./scripts/build-images.sh
./scripts/deploy-dev.sh
```

