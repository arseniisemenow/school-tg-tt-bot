# ADR-001: Technology Stack Selection

## Status
Accepted

## Context
We need to build a Telegram bot for managing group behavior, specifically for ELO-based ranking and match tracking. The bot must handle multiple groups, concurrent requests, database persistence, observability, and integration with School21 API. We require a robust, maintainable C++ stack that supports modern development practices.

Key requirements:
- Telegram Bot API integration
- PostgreSQL database connectivity
- Observability (metrics, logging, tracing)
- Dependency management
- Build system
- Schema migration management

## Decision
We will use the following technology stack:

### Telegram Bot Library: tgbotxx
- **Library**: [tgbotxx](https://github.com/egorpugin/tgbotxx)
- **Rationale**: 
  - Modern C++20 library with async support
  - Active maintenance and community
  - Comprehensive Telegram Bot API coverage
  - Type-safe API wrappers
  - Supports both polling and webhook modes
  - Good documentation and examples

### Observability: OpenTelemetry C++
- **Library**: OpenTelemetry C++ SDK
- **Rationale**:
  - Unified observability standard (metrics, logging, tracing)
  - Vendor-agnostic (can switch backends without code changes)
  - Industry standard, well-supported
  - Supports OTEL Collector for production
  - Allows simpler local development setup
  - Rich instrumentation capabilities

### Build System: CMake
- **Version**: CMake 4.0+
- **C++ Standard**: C++20
- **Rationale**:
  - Industry standard for C++ projects
  - Excellent cross-platform support
  - Good IDE integration (CLion, VS Code, etc.)
  - Strong ecosystem and community
  - Supports modern C++ features
  - C++20 provides needed features (coroutines, concepts, ranges)

### Dependency Management: Conan
- **Package Manager**: Conan 2.x
- **Rationale**:
  - Modern C++ package manager
  - Good integration with CMake
  - Supports transitive dependencies
  - Can manage complex dependency graphs
  - Active development and community
  - Works well with libpqxx, OpenTelemetry, and other C++ libraries

### Database Client: libpqxx
- **Library**: libpqxx (PostgreSQL C++ client library)
- **Rationale**:
  - Official PostgreSQL C++ client library
  - Thread-safe connection pooling support
  - Parameterized queries (SQL injection prevention)
  - Good performance and reliability
  - Active maintenance
  - Well-documented

### Migration Tool: Flyway
- **Tool**: Flyway (via command-line or Java API)
- **Rationale**:
  - Industry-standard database migration tool
  - Version-controlled schema changes
  - Supports PostgreSQL natively
  - Can be integrated into CI/CD pipeline
  - Rollback capabilities
  - Clear migration history tracking

## Consequences

### Positive
- **Modern Stack**: All technologies are actively maintained and support modern C++ practices
- **Observability**: OpenTelemetry provides comprehensive observability out of the box
- **Type Safety**: tgbotxx and libpqxx provide type-safe APIs
- **Maintainability**: Standard tools make onboarding easier
- **Flexibility**: OpenTelemetry allows switching observability backends without code changes
- **Database Safety**: Flyway ensures schema changes are version-controlled and reversible

### Negative
- **Learning Curve**: Team needs to learn tgbotxx and OpenTelemetry C++ APIs
- **Conan Setup**: Initial Conan configuration can be complex
- **Flyway Integration**: Need to integrate Flyway (Java-based) with C++ build process
- **OpenTelemetry C++**: Less mature than Go/Java implementations, fewer examples

### Neutral
- **Dependencies**: Multiple dependencies increase build time
- **Complexity**: More moving parts than minimal solutions

## Alternatives Considered

### Telegram Bot Libraries
1. **tgbot-cpp**: Older library, less modern C++ features
2. **Custom Telegram API wrapper**: Too much development overhead
3. **Python/Node.js**: Rejected - requirement is C++

### Observability
1. **Prometheus + Grafana + Jaeger**: Separate tools, more complex setup
2. **Custom logging/metrics**: Not standardized, harder to maintain
3. **Boost.Log + custom metrics**: Less comprehensive, more work

### Build System
1. **Bazel**: More complex, steeper learning curve
2. **Make**: Too low-level, harder to maintain
3. **Meson**: Less common, smaller ecosystem

### Dependency Management
1. **vcpkg**: Good alternative, but Conan has better CMake integration
2. **Hunter**: Less active, smaller package repository
3. **Manual dependency management**: Too error-prone and time-consuming

### Database Client
1. **libpq (C API)**: Lower-level, more error-prone
2. **ODBC**: Less efficient, more abstraction overhead
3. **Custom wrapper**: Too much development overhead

### Migration Tool
1. **Liquibase**: Java-based alternative, similar to Flyway
2. **Manual SQL scripts**: No versioning, error-prone
3. **C++ migration code**: Harder to maintain, no rollback support

