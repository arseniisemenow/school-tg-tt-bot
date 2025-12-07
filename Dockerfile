# Build stage
FROM ubuntu:22.04 AS builder

WORKDIR /build

# Install build tools and dependencies via apt-get
RUN apt-get update && \
    apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libpqxx-dev \
    libcurl4-openssl-dev \
    nlohmann-json3-dev \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy source code
COPY . .

# Build the project
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --parallel $(nproc)

# Runtime stage
FROM ubuntu:22.04

# Install runtime dependencies
RUN apt-get update && \
    apt-get install -y \
    libpq5 \
    libpqxx-6.4 \
    libcurl4 \
    ca-certificates \
    procps \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user for security
RUN useradd -m -u 1000 appuser && \
    mkdir -p /app/config /app/migrations && \
    chown -R appuser:appuser /app

# Copy binary and config files from builder
COPY --from=builder --chown=appuser:appuser /build/build/school_tg_tt_bot /usr/local/bin/school_tg_tt_bot
COPY --chown=appuser:appuser config/ /app/config/
COPY --chown=appuser:appuser migrations/ /app/migrations/

# Switch to non-root user
USER appuser

WORKDIR /app

# Health check - check if the bot process is running
HEALTHCHECK --interval=30s --timeout=10s --retries=3 \
    CMD pgrep -f school_tg_tt_bot > /dev/null || exit 1

# Run the application
ENTRYPOINT ["/usr/local/bin/school_tg_tt_bot"]
