# ═══════════════════════════════════════════════════════════════════════════
# aether Clipboard Daemon - Makefile
# ═══════════════════════════════════════════════════════════════════════════

CC = gcc
CFLAGS = -Wall -Wextra -O2 $(shell pkg-config --cflags gtk+-3.0 sqlite3)
LDFLAGS = $(shell pkg-config --libs gtk+-3.0 sqlite3)
LDFLAGS_GIO = $(shell pkg-config --libs gio-2.0)

TARGET = aether_clipboard
SRC = aether_clipboard.c theme_manager.c

.PHONY: all clean install

all: $(TARGET)
	@echo "✅ Build complete: $(TARGET)"

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS) $(LDFLAGS_GIO)

clean:
	rm -f $(TARGET)
	@echo "🧹 Cleaned."

install: $(TARGET)
	install -Dm755 $(TARGET) /usr/bin/$(TARGET)
	@echo "✅ Installed to /usr/bin/$(TARGET)"
