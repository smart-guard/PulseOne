#!/bin/bash

# PulseOne Collector - Linux Bare-metal Installer
# Optimized for Raspberry Pi / Debian / Ubuntu

set -e

echo "🚀 PulseOne Collector Native Service Installer"

# 1. Environment Check
if [ "$EUID" -ne 0 ]; then
  echo "❌ Please run as root (sudo)"
  exit 1
fi

# 2. Setup Directories
INSTALL_DIR="/opt/pulseone/collector"
LOG_DIR="/var/log/pulseone"

echo "📂 Creating directories..."
mkdir -p "$INSTALL_DIR/bin"
mkdir -p "$INSTALL_DIR/config"
mkdir -p "$LOG_DIR"

# 3. Create User
if ! id "pulseone" &>/dev/null; then
    echo "👤 Creating pulseone system user..."
    useradd -r -s /bin/false pulseone
fi

# 4. Copy Files (Assuming build is done)
if [ -f "../bin/pulseone-collector" ]; then
    echo "📦 Copying binary..."
    cp ../bin/pulseone-collector "$INSTALL_DIR/bin/"
    chmod +x "$INSTALL_DIR/bin/pulseone-collector"
else
    echo "⚠️ Warning: Binary not found in ../bin/. Please build first."
fi

# 5. Setup Service
echo "⚙️ Registering systemd service..."
cp pulseone-collector.service /etc/systemd/system/
chown pulseone:pulseone "$LOG_DIR"
chown -R pulseone:pulseone "$INSTALL_DIR"

systemctl daemon-reload
systemctl enable pulseone-collector

echo "================================================================"
echo "✅ Installation complete!"
echo "To start service: sudo systemctl start pulseone-collector"
echo "To check status: sudo systemctl status pulseone-collector"
echo "To view logs: tail -f $LOG_DIR/collector.log"
echo "================================================================"
