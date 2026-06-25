#include "common.h"
#include "db.h"
#include "history.h"
#include "ui.h"
#include "dbus_service.h"
#include "config_manager.h"

// ═══════════════════════════════════════════════════════════════════════════
// Global State Definition
// ═══════════════════════════════════════════════════════════════════════════

ClipboardEntry *history[MAX_HISTORY_SIZE];
int history_count = 0;
GtkClipboard *clipboard = NULL;
GDBusConnection *dbus_conn = NULL;
guint dbus_owner_id = 0;
char *last_clipboard_text = NULL;
sqlite3 *db = NULL;
int next_id = 1;

GtkWidget *main_window = NULL;
GtkWidget *search_entry = NULL;
GtkWidget *flow_box = NULL;
gboolean window_visible = FALSE;

gboolean ghost_mode = FALSE;
GtkWidget *ghost_btn = NULL;

// ═══════════════════════════════════════════════════════════════════════════
// Main
// ═══════════════════════════════════════════════════════════════════════════

int main(int argc, char *argv[]) {
    printf("🚀 ════════════════════════════════════════════════════════════\n");
    printf("🚀 aether Clipboard Manager v3.0 (SQLite)\n");
    printf("🚀 ════════════════════════════════════════════════════════════\n");
    
    gtk_init(&argc, &argv);
    
    init_database();
    load_pinned_entries();
    
    config_manager_init();
    build_window();
    
    clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    g_signal_connect(clipboard, "owner-change", G_CALLBACK(on_clipboard_changed), NULL);
    
    printf("📋 Monitoring CLIPBOARD only\n");
    
    dbus_owner_id = g_bus_own_name(G_BUS_TYPE_SESSION, DBUS_NAME, G_BUS_NAME_OWNER_FLAGS_NONE,
        on_bus_acquired, NULL, NULL, NULL, NULL);
    
    printf("✅ Ready. Use D-Bus Toggle to show/hide.\n");
    
    gtk_main();
    
    if (dbus_owner_id > 0) g_bus_unown_name(dbus_owner_id);
    if (db) sqlite3_close(db);
    for (int i = 0; i < history_count; i++) free_entry(history[i]);
    g_free(last_clipboard_text);
    
    return 0;
}
