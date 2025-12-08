# ═══════════════════════════════════════════════════════════════════════════
# Venom Clipboard Daemon - Makefile
# ═══════════════════════════════════════════════════════════════════════════

CC = gcc
CFLAGS = -Wall -Wextra -O2 $(shell pkg-config --cflags gtk+-3.0 sqlite3)
LDFLAGS = $(shell pkg-config --libs gtk+-3.0 sqlite3)
LDFLAGS_GIO = $(shell pkg-config --libs gio-2.0)

TARGET = venom_clipboard
TEST_TARGET = clipboard_test
SRC = venom_clipboard.c
TEST_SRC = clipboard_test.c

.PHONY: all clean install test

all: $(TARGET) $(TEST_TARGET)
	@echo "✅ Build complete: $(TARGET), $(TEST_TARGET)"

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

$(TEST_TARGET): $(TEST_SRC)
	$(CC) $(CFLAGS) -o $(TEST_TARGET) $(TEST_SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(TEST_TARGET)
	@echo "🧹 Cleaned."

install: $(TARGET)
	install -Dm755 $(TARGET) /usr/bin/$(TARGET)
	@echo "✅ Installed to /usr/bin/$(TARGET)"
