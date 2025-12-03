#!/bin/bash
# Initial setup script for new developers
# Run from host machine

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=== School TG Bot - Initial Setup ==="
echo ""

# Check prerequisites
echo "Checking prerequisites..."

# Check Docker
if ! command -v docker &> /dev/null; then
    echo "ERROR: Docker is not installed. Please install Docker first."
    exit 1
fi
echo "✓ Docker found: $(docker --version)"

# Check Docker Compose
if ! command -v docker-compose &> /dev/null && ! docker compose version &> /dev/null; then
    echo "ERROR: Docker Compose is not installed. Please install Docker Compose first."
    exit 1
fi
if command -v docker-compose &> /dev/null; then
    echo "✓ Docker Compose found: $(docker-compose --version)"
else
    echo "✓ Docker Compose found: $(docker compose version)"
fi

# Check Git
if ! command -v git &> /dev/null; then
    echo "WARNING: Git is not installed. Version control features may not work."
else
    echo "✓ Git found: $(git --version)"
fi

echo ""
echo "Prerequisites check passed!"
echo ""

# Setup .env file
cd "$PROJECT_ROOT"
if [ ! -f .env ]; then
    echo "Creating .env file from template..."
    if [ -f .env.example ]; then
        cp .env.example .env
        echo "✓ Created .env file from .env.example"
        echo "  Please edit .env and fill in required values:"
        echo "    - TELEGRAM_BOT_TOKEN"
        echo "    - SCHOOL21_API_USERNAME"
        echo "    - SCHOOL21_API_PASSWORD"
    else
        echo "Creating .env file with default template..."
        cat > .env << 'EOF'
# Telegram Bot Configuration
TELEGRAM_BOT_TOKEN=

# School21 API Configuration
SCHOOL21_API_USERNAME=
SCHOOL21_API_PASSWORD=

# Environment
ENVIRONMENT=development
EOF
        echo "✓ Created .env file with default template"
        echo "  Please edit .env and fill in required values"
    fi
else
    echo "✓ .env file already exists"
fi

echo ""
echo "=== Setup Complete ==="
echo ""
echo "Next steps:"
echo "1. Edit .env file and fill in required values"
echo "2. Start services: docker compose up -d"
echo "3. Enter bot container: docker compose exec bot bash"
echo "4. Setup environment: ./scripts/setup-env.sh"
echo "5. Build application: ./scripts/build.sh"
echo ""

