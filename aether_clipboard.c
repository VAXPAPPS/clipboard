/*
 * ═══════════════════════════════════════════════════════════════════════════
 * 🐍 aether Clipboard Manager v3.0
 * ═══════════════════════════════════════════════════════════════════════════
 * مدير حافظة مع واجهة رسومية مدمجة
 * - يتتبع CLIPBOARD فقط (Ctrl+C)
 * - واجهة بحث في السجل
 * - تثبيت النصوص (محفوظة في SQLite)
 * - تصميم شفاف أنيق
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "theme_manager.h"

// ═══════════════════════════════════════════════════════════════════════════
// الإعدادات
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
// هيكل البيانات
// ═══════════════════════════════════════════════════════════════════════════

typedef struct {
    int id;
    char *content;
    gint64 timestamp;
    gboolean pinned;
} ClipboardEntry;

// المتغيرات العامة
static ClipboardEntry *history[MAX_HISTORY_SIZE];
static int history_count = 0;
static GtkClipboard *clipboard = NULL;
static GDBusConnection *dbus_conn = NULL;
static guint dbus_owner_id = 0;
static char *last_clipboard_text = NULL;
static sqlite3 *db = NULL;
static int next_id = 1;

// واجهة المستخدم
static GtkWidget *main_window = NULL;
static GtkWidget *search_entry = NULL;
static GtkWidget *history_list = NULL;
static GtkListStore *list_store = NULL;
static gboolean window_visible = FALSE;

// Ghost Mode
static gboolean ghost_mode = FALSE;
static GtkWidget *ghost_btn = NULL;

// ═══════════════════════════════════════════════════════════════════════════
// SQLite
// ═══════════════════════════════════════════════════════════════════════════

static char* get_db_path() {
    const char *home = g_get_home_dir();
    return g_strdup_printf("%s%s", home, DB_PATH);
}

static void init_database() {
    char *db_path = get_db_path();
    
    // إنشاء المجلد إذا لم يكن موجوداً
    char *dir = g_path_get_dirname(db_path);
    g_mkdir_with_parents(dir, 0755);
    g_free(dir);
    
    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        printf("❌ SQLite error: %s\n", sqlite3_errmsg(db));
        g_free(db_path);
        return;
    }
    
    // إنشاء الجدول
    const char *sql = 
        "CREATE TABLE IF NOT EXISTS pinned ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  content TEXT NOT NULL UNIQUE,"
        "  timestamp INTEGER NOT NULL,"
        "  created_at INTEGER DEFAULT (strftime('%s', 'now'))"
        ");";
    
    char *err = NULL;
    sqlite3_exec(db, sql, NULL, NULL, &err);
    if (err) {
        printf("❌ SQL error: %s\n", err);
        sqlite3_free(err);
    }
    
    printf("📦 Database: %s\n", db_path);
    g_free(db_path);
}

static void load_pinned_entries() {
    if (!db) return;
    
    const char *sql = "SELECT id, content, timestamp FROM pinned ORDER BY created_at DESC;";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            if (history_count >= MAX_HISTORY_SIZE) break;
            
            ClipboardEntry *entry = g_new0(ClipboardEntry, 1);
            entry->id = sqlite3_column_int(stmt, 0);
            entry->content = g_strdup((const char*)sqlite3_column_text(stmt, 1));
            entry->timestamp = sqlite3_column_int64(stmt, 2);
            entry->pinned = TRUE;
            
            history[history_count++] = entry;
            if (entry->id >= next_id) next_id = entry->id + 1;
        }
        sqlite3_finalize(stmt);
    }
    
    printf("📌 Loaded %d pinned entries\n", history_count);
}

static gboolean pin_entry(const char *content, gint64 timestamp) {
    if (!db || !content) return FALSE;
    
    const char *sql = "INSERT OR IGNORE INTO pinned (content, timestamp) VALUES (?, ?);";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, content, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, timestamp);
        
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            sqlite3_finalize(stmt);
            printf("📌 Pinned: %.50s...\n", content);
            return TRUE;
        }
        sqlite3_finalize(stmt);
    }
    return FALSE;
}

static gboolean unpin_entry(int id) {
    if (!db) return FALSE;
    
    const char *sql = "DELETE FROM pinned WHERE id = ?;";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            sqlite3_finalize(stmt);
            printf("📌 Unpinned ID: %d\n", id);
            return TRUE;
        }
        sqlite3_finalize(stmt);
    }
    return FALSE;
}

// ═══════════════════════════════════════════════════════════════════════════
// إدارة السجل
// ═══════════════════════════════════════════════════════════════════════════

static void free_entry(ClipboardEntry *entry) {
    if (entry) {
        g_free(entry->content);
        g_free(entry);
    }
}

static void add_to_history(const char *text) {
    if (!text || strlen(text) == 0) return;
    if (strlen(text) > MAX_ENTRY_LENGTH) return;
    
    // تجنب التكرار
    for (int i = 0; i < history_count; i++) {
        if (history[i] && g_strcmp0(history[i]->content, text) == 0) {
            return;
        }
    }
    
    // إيجاد أول عنصر غير مثبت
    int insert_pos = 0;
    while (insert_pos < history_count && history[insert_pos] && history[insert_pos]->pinned) {
        insert_pos++;
    }
    
    ClipboardEntry *entry = g_new0(ClipboardEntry, 1);
    entry->id = next_id++;
    entry->content = g_strdup(text);
    entry->timestamp = g_get_real_time();
    entry->pinned = FALSE;
    
    // إزالة أقدم عنصر غير مثبت إذا امتلأ السجل
    if (history_count >= MAX_HISTORY_SIZE) {
        for (int i = MAX_HISTORY_SIZE - 1; i >= 0; i--) {
            if (history[i] && !history[i]->pinned) {
                free_entry(history[i]);
                for (int j = i; j < history_count - 1; j++) {
                    history[j] = history[j + 1];
                }
                history_count--;
                break;
            }
        }
    }
    
    // إزاحة وإدخال
    for (int i = history_count; i > insert_pos; i--) {
        history[i] = history[i - 1];
    }
    
    history[insert_pos] = entry;
    history_count++;
    
    printf("📋 Added: %.50s%s\n", text, strlen(text) > 50 ? "..." : "");
}

static void clear_history() {
    // حذف العناصر غير المثبتة فقط
    int new_count = 0;
    for (int i = 0; i < history_count; i++) {
        if (history[i]) {
            if (history[i]->pinned) {
                history[new_count++] = history[i];
            } else {
                free_entry(history[i]);
            }
        }
    }
    for (int i = new_count; i < history_count; i++) {
        history[i] = NULL;
    }
    history_count = new_count;
    printf("🗑️ Cleared non-pinned entries\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// مراقبة الحافظة
// ═══════════════════════════════════════════════════════════════════════════

static void update_ui_list(const char *filter);

static void on_clipboard_changed(GtkClipboard *clip, gpointer data) {
    (void)data;
    
    // Ghost Mode - لا تسجل أي شيء
    if (ghost_mode) {
        return;
    }
    
    gchar *text = gtk_clipboard_wait_for_text(clip);
    if (!text) return;
    
    if (last_clipboard_text && g_strcmp0(last_clipboard_text, text) == 0) {
        g_free(text);
        return;
    }
    
    g_free(last_clipboard_text);
    last_clipboard_text = g_strdup(text);
    
    add_to_history(text);
    
    if (window_visible && search_entry) {
        const char *filter = gtk_entry_get_text(GTK_ENTRY(search_entry));
        update_ui_list(filter);
    }
    
    if (dbus_conn) {
        g_dbus_connection_emit_signal(dbus_conn, NULL, DBUS_PATH, DBUS_INTERFACE,
            "ClipboardChanged", g_variant_new("(s)", text), NULL);
    }
    
    g_free(text);
}

// ═══════════════════════════════════════════════════════════════════════════
// واجهة المستخدم
// ═══════════════════════════════════════════════════════════════════════════

enum {
    COL_ICON,
    COL_TIME,
    COL_CONTENT,
    COL_INDEX,
    COL_PINNED,
    NUM_COLS
};

static void update_ui_list(const char *filter) {
    if (!list_store) return;
    
    gtk_list_store_clear(list_store);
    
    for (int i = 0; i < history_count; i++) {
        if (!history[i]) continue;
        
        if (filter && strlen(filter) > 0) {
            if (!strcasestr(history[i]->content, filter)) {
                continue;
            }
        }
        
        GtkTreeIter iter;
        gtk_list_store_append(list_store, &iter);
        
        char time_str[32];
        time_t t = history[i]->timestamp / 1000000;
        strftime(time_str, sizeof(time_str), "%H:%M", localtime(&t));
        
        gchar *preview = g_strndup(history[i]->content, 70);
        for (int j = 0; preview[j]; j++) {
            if (preview[j] == '\n' || preview[j] == '\r') preview[j] = ' ';
        }
        if (strlen(history[i]->content) > 70) {
            gchar *full = g_strdup_printf("%s...", preview);
            g_free(preview);
            preview = full;
        }
        
        gtk_list_store_set(list_store, &iter,
            COL_ICON, history[i]->pinned ? "📌" : "",
            COL_TIME, time_str,
            COL_CONTENT, preview,
            COL_INDEX, i,
            COL_PINNED, history[i]->pinned,
            -1);
        
        g_free(preview);
    }
}

static void on_search_changed(GtkEntry *entry, gpointer data) {
    (void)data;
    const char *filter = gtk_entry_get_text(entry);
    update_ui_list(filter);
}

static void on_row_activated(GtkTreeView *view, GtkTreePath *path,
                              GtkTreeViewColumn *col, gpointer data) {
    (void)col; (void)data;
    
    GtkTreeModel *model = gtk_tree_view_get_model(view);
    GtkTreeIter iter;
    
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        gint index;
        gtk_tree_model_get(model, &iter, COL_INDEX, &index, -1);
        
        if (index >= 0 && index < history_count && history[index]) {
            gtk_clipboard_set_text(clipboard, history[index]->content, -1);
            printf("📋 Copied: %.50s...\n", history[index]->content);
            gtk_widget_hide(main_window);
            window_visible = FALSE;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Right-click Menu
// ═══════════════════════════════════════════════════════════════════════════

static int context_menu_index = -1;

static void on_menu_pin(GtkMenuItem *item, gpointer data) {
    (void)item; (void)data;
    
    if (context_menu_index >= 0 && context_menu_index < history_count && history[context_menu_index]) {
        ClipboardEntry *entry = history[context_menu_index];
        
        if (!entry->pinned) {
            if (pin_entry(entry->content, entry->timestamp)) {
                entry->pinned = TRUE;
                
                // نقله لأعلى (مع المثبتات)
                for (int i = context_menu_index; i > 0; i--) {
                    if (history[i-1] && !history[i-1]->pinned) {
                        ClipboardEntry *temp = history[i];
                        history[i] = history[i-1];
                        history[i-1] = temp;
                    } else {
                        break;
                    }
                }
                
                update_ui_list(NULL);
            }
        }
    }
}

static void on_menu_unpin(GtkMenuItem *item, gpointer data) {
    (void)item; (void)data;
    
    if (context_menu_index >= 0 && context_menu_index < history_count && history[context_menu_index]) {
        ClipboardEntry *entry = history[context_menu_index];
        
        if (entry->pinned) {
            if (unpin_entry(entry->id)) {
                entry->pinned = FALSE;
                update_ui_list(NULL);
            }
        }
    }
}

static void on_menu_delete(GtkMenuItem *item, gpointer data) {
    (void)item; (void)data;
    
    if (context_menu_index >= 0 && context_menu_index < history_count && history[context_menu_index]) {
        ClipboardEntry *entry = history[context_menu_index];
        
        if (entry->pinned) {
            unpin_entry(entry->id);
        }
        
        free_entry(entry);
        for (int i = context_menu_index; i < history_count - 1; i++) {
            history[i] = history[i + 1];
        }
        history[--history_count] = NULL;
        
        update_ui_list(NULL);
    }
}

static void on_menu_copy(GtkMenuItem *item, gpointer data) {
    (void)item; (void)data;
    
    if (context_menu_index >= 0 && context_menu_index < history_count && history[context_menu_index]) {
        gtk_clipboard_set_text(clipboard, history[context_menu_index]->content, -1);
        printf("📋 Copied: %.50s...\n", history[context_menu_index]->content);
    }
}

static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    (void)data;
    
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
        GtkTreeView *view = GTK_TREE_VIEW(widget);
        GtkTreePath *path;
        
        if (gtk_tree_view_get_path_at_pos(view, event->x, event->y, &path, NULL, NULL, NULL)) {
            GtkTreeModel *model = gtk_tree_view_get_model(view);
            GtkTreeIter iter;
            
            if (gtk_tree_model_get_iter(model, &iter, path)) {
                gint index;
                gboolean pinned;
                gtk_tree_model_get(model, &iter, COL_INDEX, &index, COL_PINNED, &pinned, -1);
                context_menu_index = index;
                
                GtkWidget *menu = gtk_menu_new();
                
                GtkWidget *copy_item = gtk_menu_item_new_with_label("📋 Copy");
                g_signal_connect(copy_item, "activate", G_CALLBACK(on_menu_copy), NULL);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), copy_item);
                
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
                
                if (pinned) {
                    GtkWidget *unpin_item = gtk_menu_item_new_with_label("📌 Unpin");
                    g_signal_connect(unpin_item, "activate", G_CALLBACK(on_menu_unpin), NULL);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), unpin_item);
                } else {
                    GtkWidget *pin_item = gtk_menu_item_new_with_label("📌 Pin");
                    g_signal_connect(pin_item, "activate", G_CALLBACK(on_menu_pin), NULL);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), pin_item);
                }
                
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
                
                GtkWidget *delete_item = gtk_menu_item_new_with_label("🗑️ Delete");
                g_signal_connect(delete_item, "activate", G_CALLBACK(on_menu_delete), NULL);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), delete_item);
                
                gtk_widget_show_all(menu);
                gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*)event);
            }
            
            gtk_tree_path_free(path);
            return TRUE;
        }
    }
    
    return FALSE;
}

// ═══════════════════════════════════════════════════════════════════════════
// أزرار وأحداث
// ═══════════════════════════════════════════════════════════════════════════

static void update_ghost_button() {
    if (ghost_btn) {
        if (ghost_mode) {
            gtk_button_set_label(GTK_BUTTON(ghost_btn), "👻 Ghost ON");
            gtk_style_context_add_class(gtk_widget_get_style_context(ghost_btn), "ghost-active");
        } else {
            gtk_button_set_label(GTK_BUTTON(ghost_btn), "👻 Ghost");
            gtk_style_context_remove_class(gtk_widget_get_style_context(ghost_btn), "ghost-active");
        }
    }
}

static void toggle_ghost_mode() {
    ghost_mode = !ghost_mode;
    update_ghost_button();
    printf("👻 Ghost Mode: %s\n", ghost_mode ? "ON" : "OFF");
    
    // إشارة D-Bus
    if (dbus_conn) {
        g_dbus_connection_emit_signal(dbus_conn, NULL, DBUS_PATH, DBUS_INTERFACE,
            "GhostModeChanged", g_variant_new("(b)", ghost_mode), NULL);
    }
}

static void on_ghost_clicked(GtkButton *btn, gpointer data) {
    (void)btn; (void)data;
    toggle_ghost_mode();
}

static void on_clear_clicked(GtkButton *btn, gpointer data) {
    (void)btn; (void)data;
    clear_history();
    update_ui_list(NULL);
    gtk_entry_set_text(GTK_ENTRY(search_entry), "");
}

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    (void)widget; (void)data;
    
    if (event->keyval == GDK_KEY_Escape) {
        gtk_widget_hide(main_window);
        window_visible = FALSE;
        return TRUE;
    }
    
    return FALSE;
}

static gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer data) {
    (void)widget; (void)event; (void)data;
    gtk_widget_hide(main_window);
    window_visible = FALSE;
    return TRUE;
}

// ═══════════════════════════════════════════════════════════════════════════
// CSS
// ═══════════════════════════════════════════════════════════════════════════

static void apply_css() {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "window, .background { background-color: rgba(0, 0, 0, 0); }"
        ".main-box { background-color: rgba(0, 0, 0, 0.34); border-radius: 16px; padding: 16px; }"
        ".title { font-size: 20px; font-weight: bold; color: #00d4ff; margin-bottom: 8px; }"
        ".search-entry { background: rgba(255, 255, 255, 0.08); color: #fff; "
        "  border: 1px solid rgba(0, 212, 255, 0.3); border-radius: 8px; padding: 10px 14px; font-size: 14px; }"
        ".search-entry:focus { border-color: #00d4ff; }"
        "treeview { background: transparent; color: #eee; font-size: 13px; }"
        "treeview:selected { background: rgba(0, 212, 255, 0.3); border-radius: 4px; }"
        "treeview header button { background: transparent; color: #888; border: none; font-size: 11px; padding: 4px 8px; }"
        ".clear-btn { background: rgba(255, 100, 100, 0.2); color: #ff6464; "
        "  border: 1px solid rgba(255, 100, 100, 0.3); border-radius: 6px; padding: 6px 12px; }"
        ".clear-btn:hover { background: rgba(255, 100, 100, 0.4); }"
        ".ghost-btn { background: rgba(150, 100, 255, 0.2); color: #b388ff; "
        "  border: 1px solid rgba(150, 100, 255, 0.3); border-radius: 6px; padding: 6px 12px; }"
        ".ghost-btn:hover { background: rgba(150, 100, 255, 0.4); }"
        ".ghost-active { background: rgba(150, 100, 255, 0.5); color: #fff; "
        "  border: 1px solid #b388ff; font-weight: bold; }"
        ".status { font-size: 11px; color: #666; }"
        "scrolledwindow { background: transparent; }"
        "menu { background: rgba(30, 30, 40, 0.95); border-radius: 8px; padding: 4px; }"
        "menuitem { color: #eee; padding: 6px 12px; border-radius: 4px; }"
        "menuitem:hover { background: rgba(0, 212, 255, 0.3); }"
        , -1, NULL);
    
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

// ═══════════════════════════════════════════════════════════════════════════
// إنشاء النافذة
// ═══════════════════════════════════════════════════════════════════════════

static void build_window() {
    main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(main_window), "aether Clipboard");
    gtk_window_set_default_size(GTK_WINDOW(main_window), WINDOW_WIDTH, WINDOW_HEIGHT);
    gtk_window_set_position(GTK_WINDOW(main_window), GTK_WIN_POS_CENTER);
    gtk_window_set_decorated(GTK_WINDOW(main_window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(main_window), TRUE);
    gtk_widget_set_app_paintable(main_window, TRUE);
    
    GdkScreen *screen = gtk_widget_get_screen(main_window);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (visual) gtk_widget_set_visual(main_window, visual);
    
    g_signal_connect(main_window, "delete-event", G_CALLBACK(on_window_delete), NULL);
    g_signal_connect(main_window, "key-press-event", G_CALLBACK(on_key_press), NULL);
    
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_style_context_add_class(gtk_widget_get_style_context(main_box), "main-box");
    gtk_container_set_border_width(GTK_CONTAINER(main_box), 8);
    gtk_container_add(GTK_CONTAINER(main_window), main_box);
    
    GtkWidget *title = gtk_label_new("📋 Clipboard History");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "title");
    gtk_box_pack_start(GTK_BOX(main_box), title, FALSE, FALSE, 0);
    
    search_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry), "🔍 Search...");
    gtk_style_context_add_class(gtk_widget_get_style_context(search_entry), "search-entry");
    g_signal_connect(search_entry, "changed", G_CALLBACK(on_search_changed), NULL);
    gtk_box_pack_start(GTK_BOX(main_box), search_entry, FALSE, FALSE, 0);
    
    // القائمة
    list_store = gtk_list_store_new(NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_BOOLEAN);
    history_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(history_list), FALSE);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(history_list), FALSE);
    
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    
    GtkTreeViewColumn *icon_col = gtk_tree_view_column_new_with_attributes("", renderer, "text", COL_ICON, NULL);
    gtk_tree_view_column_set_fixed_width(icon_col, 24);
    gtk_tree_view_append_column(GTK_TREE_VIEW(history_list), icon_col);
    
    GtkTreeViewColumn *time_col = gtk_tree_view_column_new_with_attributes("", renderer, "text", COL_TIME, NULL);
    gtk_tree_view_column_set_fixed_width(time_col, 45);
    gtk_tree_view_append_column(GTK_TREE_VIEW(history_list), time_col);
    
    GtkTreeViewColumn *content_col = gtk_tree_view_column_new_with_attributes("", renderer, "text", COL_CONTENT, NULL);
    gtk_tree_view_column_set_expand(content_col, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(history_list), content_col);
    
    g_signal_connect(history_list, "row-activated", G_CALLBACK(on_row_activated), NULL);
    g_signal_connect(history_list, "button-press-event", G_CALLBACK(on_button_press), NULL);
    
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), history_list);
    gtk_box_pack_start(GTK_BOX(main_box), scroll, TRUE, TRUE, 0);
    
    GtkWidget *bottom_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    
    // Ghost Mode Button
    ghost_btn = gtk_button_new_with_label("👻 Ghost");
    gtk_style_context_add_class(gtk_widget_get_style_context(ghost_btn), "ghost-btn");
    g_signal_connect(ghost_btn, "clicked", G_CALLBACK(on_ghost_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(bottom_box), ghost_btn, FALSE, FALSE, 0);
    
    GtkWidget *status = gtk_label_new("📌 Pin • 👻 Ghost");
    gtk_style_context_add_class(gtk_widget_get_style_context(status), "status");
    gtk_box_pack_start(GTK_BOX(bottom_box), status, TRUE, TRUE, 0);
    
    GtkWidget *clear_btn = gtk_button_new_with_label("🗑️ Clear");
    gtk_style_context_add_class(gtk_widget_get_style_context(clear_btn), "clear-btn");
    g_signal_connect(clear_btn, "clicked", G_CALLBACK(on_clear_clicked), NULL);
    gtk_box_pack_end(GTK_BOX(bottom_box), clear_btn, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(main_box), bottom_box, FALSE, FALSE, 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// D-Bus (مختصر)
// ═══════════════════════════════════════════════════════════════════════════

static const gchar introspection_xml[] =
    "<node><interface name='org.aether.Clipboard'>"
    "<method name='Show'/><method name='Hide'/><method name='Toggle'/>"
    "<method name='GetHistory'><arg type='i' direction='in'/><arg type='a(sxb)' direction='out'/></method>"
    "<method name='ClearHistory'/>"
    "<method name='SetGhostMode'><arg type='b' direction='in'/></method>"
    "<method name='GetGhostMode'><arg type='b' direction='out'/></method>"
    "<method name='ToggleGhostMode'><arg type='b' direction='out'/></method>"
    "<signal name='ClipboardChanged'><arg type='s'/></signal>"
    "<signal name='GhostModeChanged'><arg type='b'/></signal>"
    "</interface></node>";

static void handle_method_call(GDBusConnection *c, const gchar *s, const gchar *o, const gchar *i,
    const gchar *m, GVariant *p, GDBusMethodInvocation *inv, gpointer u) {
    (void)c; (void)s; (void)o; (void)i; (void)u;
    
    if (g_strcmp0(m, "Show") == 0 || (g_strcmp0(m, "Toggle") == 0 && !window_visible)) {
        update_ui_list(NULL);
        gtk_widget_show_all(main_window);
        gtk_window_present(GTK_WINDOW(main_window));
        gtk_widget_grab_focus(search_entry);
        window_visible = TRUE;
    } else if (g_strcmp0(m, "Hide") == 0 || (g_strcmp0(m, "Toggle") == 0 && window_visible)) {
        gtk_widget_hide(main_window);
        window_visible = FALSE;
    } else if (g_strcmp0(m, "GetHistory") == 0) {
        gint count; g_variant_get(p, "(i)", &count);
        if (count <= 0 || count > history_count) count = history_count;
        GVariantBuilder b; g_variant_builder_init(&b, G_VARIANT_TYPE("a(sxb)"));
        for (int i = 0; i < count; i++) {
            if (history[i]) g_variant_builder_add(&b, "(sxb)", history[i]->content, history[i]->timestamp, history[i]->pinned);
        }
        g_dbus_method_invocation_return_value(inv, g_variant_new("(a(sxb))", &b));
        return;
    } else if (g_strcmp0(m, "ClearHistory") == 0) {
        clear_history(); update_ui_list(NULL);
    } else if (g_strcmp0(m, "SetGhostMode") == 0) {
        gboolean enabled;
        g_variant_get(p, "(b)", &enabled);
        ghost_mode = enabled;
        update_ghost_button();
        printf("👻 Ghost Mode set to: %s\n", ghost_mode ? "ON" : "OFF");
    } else if (g_strcmp0(m, "GetGhostMode") == 0) {
        g_dbus_method_invocation_return_value(inv, g_variant_new("(b)", ghost_mode));
        return;
    } else if (g_strcmp0(m, "ToggleGhostMode") == 0) {
        toggle_ghost_mode();
        g_dbus_method_invocation_return_value(inv, g_variant_new("(b)", ghost_mode));
        return;
    }
    g_dbus_method_invocation_return_value(inv, NULL);
}

static const GDBusInterfaceVTable vtable = { handle_method_call, NULL, NULL, {0} };

static void on_bus_acquired(GDBusConnection *c, const gchar *n, gpointer d) {
    (void)n; (void)d;
    GDBusNodeInfo *info = g_dbus_node_info_new_for_xml(introspection_xml, NULL);
    g_dbus_connection_register_object(c, DBUS_PATH, info->interfaces[0], &vtable, NULL, NULL, NULL);
    dbus_conn = c;
    g_dbus_node_info_unref(info);
    printf("📡 D-Bus ready\n");
}

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
    
    apply_css();
    theme_manager_init();
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
