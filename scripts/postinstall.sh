#!/bin/sh
set -e

# 1. 创建日志目录（可选，但建议保留）
LOG_DIR="/var/log/lanft"
if [ ! -d "$LOG_DIR" ]; then
    mkdir -p "$LOG_DIR"
    chmod 0755 "$LOG_DIR"
fi


echo "  lanft installed successfully!"
echo ""
echo "  This package provides a user-level systemd service."
echo "  To enable and start it for your current user, run:"
echo ""
echo "    systemctl --user enable --now lanft"
echo ""
echo "  To check service status:"
echo "    systemctl --user status lanft"
echo ""
echo "  The service will automatically start when you log in."
