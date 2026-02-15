#!/bin/bash
echo "================================================================"
echo "PulseOne Containerized Setup v1.0 (Linux/macOS)"
echo "================================================================"

if ! command -v docker &> /dev/null; then
    echo "❌ ERROR: Docker is not installed. Please install Docker first."
    exit 1
fi

echo "📦 Loading Docker images from local assets..."
for img in images/*.tar; do
    echo "   Loading $img..."
    docker load < "$img"
done

echo "🚀 Starting PulseOne services..."
docker-compose up -d

echo "✅ PulseOne is running!"
echo "➡️  Web UI: http://localhost:3000"
