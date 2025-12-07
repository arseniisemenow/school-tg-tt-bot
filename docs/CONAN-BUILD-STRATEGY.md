# Conan Build Strategy

## How Conan Downloads vs Builds

### Default Behavior: `--build=missing`

When you use `--build=missing`, Conan follows this strategy:

1. **First**: Try to download pre-built binary from ConanCenter
2. **If binary found**: Use it (fast, seconds)
3. **If binary NOT found**: Build from source (slow, minutes/hours)

**This is the optimal strategy** - Conan always tries to download first!

### Why Packages Get Built

A package is built from source only when:
- No pre-built binary exists for your configuration (OS, compiler, version, arch)
- Binary exists but doesn't match your profile settings
- Package version doesn't have binaries available

### Build Flags Explained

| Flag | Behavior |
|------|----------|
| `--build=never` | Only download binaries, fail if unavailable |
| `--build=missing` | Download if available, build if not (default, recommended) |
| `--build=always` | Always build from source (slow, not recommended) |
| `--build=outdated` | Download if available and up-to-date, build if outdated |

### Our Strategy

We use `--build=missing` which:
- ✅ Downloads pre-built binaries when available (fast)
- ✅ Builds from source only when binaries unavailable (slow but necessary)
- ✅ Skips libcurl (we use system libcurl instead)

## Why libcurl Was Being Built

libcurl/8.0.1 was being built because:
1. No pre-built binary available for GCC 12 + Ubuntu 22.04 + specific options
2. Building libcurl includes all documentation (man pages) which is very slow

**Solution**: We removed libcurl from conanfile.txt and use system libcurl instead.

## Verifying Download vs Build

Check Conan output:
```
libpqxx/7.9.0: Downloaded package revision...  # ✅ Downloaded
libcurl/8.0.1: Building from source...        # ❌ Building (slow)
```

## Ensuring Downloads Work

1. **Conan remotes configured**:
   ```bash
   conan remote list
   # Should show: conancenter https://center.conan.io
   ```

2. **Profile matches available binaries**:
   ```bash
   conan profile show default
   # Check compiler, version, OS match available binaries
   ```

3. **Check if binary exists**:
   ```bash
   conan search libpqxx/7.9.0@ --remote=conancenter
   # Shows available binaries for your configuration
   ```

## Performance

- **Download**: ~5-30 seconds per package
- **Build from source**: ~2-60 minutes per package (depends on package)

For our dependencies:
- `nlohmann_json`: Usually downloads (header-only, fast)
- `libpqxx`: Usually downloads (pre-built available)
- `libcurl`: We skip (use system version)

## Troubleshooting

### Package Always Building

**Check if binary exists:**
```bash
conan search <package>/<version>@ --remote=conancenter
```

**Check profile matches:**
```bash
conan profile show default
```

**Try different version** that has pre-built binaries:
```bash
# Check what versions have binaries
conan search <package>/*@ --remote=conancenter
```

### Force Download Only

If you want to fail fast if binaries unavailable:
```bash
conan install . --build=never
```

### Skip Specific Package

Skip building a specific package (use system version):
```bash
conan install . --build=missing --build=libcurl/*:never
```

