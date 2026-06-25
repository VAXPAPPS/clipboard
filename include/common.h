#ifndef AETHER_COMMON_H
#define AETHER_COMMON_H

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

// ═══════════════════════════════════════════════════════════════════════════
// Settings
// ═══════════════════════════════════════════════════════════════════════════

#define MAX_HISTORY_SIZE 100
#define MAX_ENTRY_LENGTH 1048576
#define DBUS_NAME "org.aether.Clipboard"
#define DBUS_PATH "/org/aether/Clipboard"
#define DBUS_INTERFACE "org.aether.Clipboard"

#define WINDOW_WIDTH 450
#define WINDOW_HEIGHT 550
#define DB_PATH "/.local/share/aether/clipboard.db"

// ═══════════════════════════════════════════════════════════════════════════
// Data Structures
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    int id;
    char *content;
    gint64 timestamp;
    gboolean pinned;
    gboolean is_image;
    GdkPixbuf *preview;
} ClipboardEntry;

// ═══════════════════════════════════════════════════════════════════════════
// Global State (externs)
// ═══════════════════════════════════════════════════════════════════════════

extern ClipboardEntry *history[MAX_HISTORY_SIZE];
extern int history_count;
extern GtkClipboard *clipboard;
extern GDBusConnection *dbus_conn;
extern guint dbus_owner_id;
extern char *last_clipboard_text;
extern sqlite3 *db;
extern int next_id;

// UI elements needed globally
extern GtkWidget *main_window;
extern GtkWidget *search_entry;
extern GtkWidget *flow_box;
extern gboolean window_visible;

// Ghost Mode
extern gboolean ghost_mode;
extern GtkWidget *ghost_btn;

#endif // AETHER_COMMON_H
