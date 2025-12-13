#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════════
# 🐍 Venom Clipboard Daemon - Installer
# ═══════════════════════════════════════════════════════════════════════════

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DAEMON_NAME="venom_clipboard"
SERVICE_NAME="venom-clipboard.service"

# ألوان
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}"
echo "╔════════════════════════════════════════════════════════════╗"
echo "║   🐍 Venom Clipboard Daemon Installer                      ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo -e "${NC}"

# التحقق من الصلاحيات
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}❌ Please run as root (sudo)${NC}"
    exit 1
fi

# إيقاف الخدمة إذا كانت تعمل
echo -e "${YELLOW}🛑 Stopping running instances...${NC}"
systemctl --user stop $SERVICE_NAME 2>/dev/null || true
pkill -9 $DAEMON_NAME 2>/dev/null || true

# ترجمة إذا لزم الأمر
if [ ! -f "$SCRIPT_DIR/$DAEMON_NAME" ]; then
    echo -e "${YELLOW}🔧 Compiling...${NC}"
    cd "$SCRIPT_DIR"
    if [ -f "Makefile" ]; then
        make
    else
        echo -e "${RED}❌ Makefile not found${NC}"
        exit 1
    fi
fi

# تثبيت البرنامج
echo -e "${YELLOW}📦 Installing binary...${NC}"
install -Dm755 "$SCRIPT_DIR/$DAEMON_NAME" "/usr/bin/$DAEMON_NAME"
echo -e "  ${GREEN}✓${NC} /usr/bin/$DAEMON_NAME"

# تثبيت ملف الخدمة
echo -e "${YELLOW}📄 Installing service file...${NC}"
if [ -f "$SCRIPT_DIR/$SERVICE_NAME" ]; then
    install -Dm644 "$SCRIPT_DIR/$SERVICE_NAME" "/usr/lib/systemd/user/$SERVICE_NAME"
    echo -e "  ${GREEN}✓${NC} /usr/lib/systemd/user/$SERVICE_NAME"
else
    echo -e "${YELLOW}⚠️ Service file not found. Generating...${NC}"
    cat > "/usr/lib/systemd/user/$SERVICE_NAME" << EOF
[Unit]
Description=Venom Clipboard Daemon
After=graphical-session.target

[Service]
Type=simple
ExecStart=/usr/bin/$DAEMON_NAME
Restart=on-failure
RestartSec=3

[Install]
WantedBy=default.target
EOF
    echo -e "  ${GREEN}✓${NC} Generated /usr/lib/systemd/user/$SERVICE_NAME"
fi

# تفعيل الخدمة للمستخدم
REAL_USER="${SUDO_USER:-$USER}"
USER_ID=$(id -u "$REAL_USER")
echo -e "${YELLOW}🔄 Enabling service for user: ${REAL_USER}${NC}"
export XDG_RUNTIME_DIR="/run/user/$USER_ID"
su - "$REAL_USER" -c "export XDG_RUNTIME_DIR=/run/user/$USER_ID; systemctl --user daemon-reload"
su - "$REAL_USER" -c "export XDG_RUNTIME_DIR=/run/user/$USER_ID; systemctl --user enable $SERVICE_NAME"
su - "$REAL_USER" -c "export XDG_RUNTIME_DIR=/run/user/$USER_ID; systemctl --user start $SERVICE_NAME"

# النتيجة
echo ""
echo -e "${GREEN}✅ Venom Clipboard Daemon installed successfully!${NC}"
echo ""
echo -e "   ${CYAN}Status:${NC} systemctl --user status $SERVICE_NAME"
echo ""
echo -e "${CYAN}📋 D-Bus Interface:${NC}"
echo "   - GetCurrent(is_primary) -> content"
echo "   - SetClipboard(content, is_primary) -> success"
echo "   - GetHistory(count) -> entries"
echo "   - ClearHistory() -> success"
echo "   - GetHistoryItem(index) -> content"
echo "   - Signal: ClipboardChanged(content, is_primary)"
