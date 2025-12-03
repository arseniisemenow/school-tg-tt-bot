# Production Dockerfile
# Based on ADR-008: Deployment and CI/CD Strategy
# Multi-stage build for optimized production image

# Stage 1: Build
FROM ubuntu:22.04 AS builder

WORKDIR /build

# Install build tools and dependencies
RUN apt-get update && \
    apt-get install -y \
    build-essential \
    gcc-12 \
    g++-12 \
    cmake \
    ninja-build \
    git \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Set GCC 12 as default
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 100 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 100

# Install Conan 2.x
RUN pip3 install --no-cache-dir conan==2.*

# Copy Conan configuration
COPY conanfile.txt ./

# Install Conan dependencies
# Set C++20 standard during install
RUN conan profile detect --force && \
    conan install . --output-folder=build --build=missing -s compiler.cppstd=20

# Copy source code
COPY . .

# Build the project
RUN cmake -S . -B build \
    -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --target school_tg_tt_bot

# Stage 2: Runtime
FROM ubuntu:22.04

# Install runtime dependencies
RUN apt-get update && \
    apt-get install -y \
    libpq5 \
    libcurl4 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN useradd -m -u 1000 appuser && \
    mkdir -p /app /app/config /app/migrations && \
    chown -R appuser:appuser /app

# Copy built binary
COPY --from=builder --chown=appuser:appuser /build/build/school_tg_tt_bot /usr/local/bin/school_tg_tt_bot

# Copy configuration files
COPY --chown=appuser:appuser config/ /app/config/
COPY --chown=appuser:appuser migrations/ /app/migrations/

# Switch to non-root user
USER appuser

WORKDIR /app

# Health check
HEALTHCHECK --interval=30s --timeout=10s --retries=3 \
    CMD pg_isready -h ${POSTGRES_HOST:-localhost} -p ${POSTGRES_PORT:-5432} || exit 1

# Entry point
ENTRYPOINT ["/usr/local/bin/school_tg_tt_bot"]

