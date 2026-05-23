#!/bin/bash

set -e

# المتغيرات الأساسية
APP_NAME="aether-clipboard"
VERSION="0.1.0"
ARCH="amd64"
DEB_DIR="${APP_NAME}_${VERSION}_${ARCH}"

# ألوان للطباعة
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║   📦 Building Debian Package for Aether Clipboard Daemon   ║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════════════════════╝${NC}"

# ترجمة البرنامج
echo -e "${YELLOW}🔧 Compiling project...${NC}"
make clean
make

# إنشاء هيكل الحزمة
echo -e "${YELLOW}📁 Creating package structure...${NC}"
mkdir -p "$DEB_DIR/usr/bin"
mkdir -p "$DEB_DIR/usr/lib/systemd/user"
mkdir -p "$DEB_DIR/usr/share/icons/hicolor/scalable/apps"
mkdir -p "$DEB_DIR/DEBIAN"

# نسخ الملفات
echo -e "${YELLOW}📋 Copying files...${NC}"
cp aether_clipboard "$DEB_DIR/usr/bin/"
cp aether-clipboard.service "$DEB_DIR/usr/lib/systemd/user/"
cp clipboard.svg "$DEB_DIR/usr/share/icons/hicolor/scalable/apps/aether-clipboard.svg"

# إنشاء ملف control
echo -e "${YELLOW}📄 Generating control file...${NC}"
cat <<EOF > "$DEB_DIR/DEBIAN/control"
Package: $APP_NAME
Version: $VERSION
Architecture: $ARCH
Maintainer: Vaxp Developer <developer@vaxp.com>
Depends: libgtk-3-0, libsqlite3-0, libglib2.0-0
Description: Aether Clipboard Daemon
 A fast, lightweight clipboard daemon for the Aether desktop environment,
 written in C and utilizing GTK3 and SQLite3.
EOF

# إنشاء الحزمة
echo -e "${YELLOW}📦 Building .deb package...${NC}"
dpkg-deb --build "$DEB_DIR"

# تنظيف الملفات المؤقتة
echo -e "${YELLOW}🧹 Cleaning up build directory...${NC}"
rm -rf "$DEB_DIR"

echo -e "${GREEN}✅ Package ${DEB_DIR}.deb has been created successfully!${NC}"
