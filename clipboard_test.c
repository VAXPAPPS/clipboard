/*
 * ═══════════════════════════════════════════════════════════════════════════
 * 🧪 Venom Clipboard Test GUI
 * ═══════════════════════════════════════════════════════════════════════════
 * تطبيق اختبار بواجهة رسومية لعفريت الحافظة
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <string.h>

#define DBUS_NAME "org.venom.Clipboard"
#define DBUS_PATH "/org/venom/Clipboard"
#define DBUS_INTERFACE "org.venom.Clipboard"

// ═══════════════════════════════════════════════════════════════════════════
// المتغيرات العامة
// ═══════════════════════════════════════════════════════════════════════════

static GDBusConnection *dbus_conn = NULL;
static GtkWidget *window = NULL;
static GtkWidget *clipboard_label = NULL;
static GtkWidget *primary_label = NULL;
static GtkWidget *history_list = NULL;
static GtkWidget *status_label = NULL;
static GtkListStore *history_store = NULL;

// ═══════════════════════════════════════════════════════════════════════════
// D-Bus
// ═══════════════════════════════════════════════════════════════════════════

static GVariant* call_method(const char *method, GVariant *params) {
    GError *error = NULL;
    GVariant *result = g_dbus_connection_call_sync(
        dbus_conn, DBUS_NAME, DBUS_PATH, DBUS_INTERFACE,
        method, params, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error
    );
    
    if (error) {
        gtk_label_set_text(GTK_LABEL(status_label), error->message);
        g_error_free(error);
        return NULL;
    }
    
    gtk_label_set_text(GTK_LABEL(status_label), "✅ Connected");
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// تحديث الواجهة
// ═══════════════════════════════════════════════════════════════════════════

static void update_current() {
    // CLIPBOARD
    GVariant *result = call_method("GetCurrent", g_variant_new("(b)", FALSE));
    if (result) {
        const gchar *content;
        g_variant_get(result, "(&s)", &content);
        gchar *display = g_strdup_printf("📋 CLIPBOARD:\n%s", 
            strlen(content) > 100 ? g_strndup(content, 100) : content);
        gtk_label_set_text(GTK_LABEL(clipboard_label), display);
        g_free(display);
        g_variant_unref(result);
    }
    
    // PRIMARY
    result = call_method("GetCurrent", g_variant_new("(b)", TRUE));
    if (result) {
        const gchar *content;
        g_variant_get(result, "(&s)", &content);
        gchar *display = g_strdup_printf("🖱️ PRIMARY:\n%s",
            strlen(content) > 100 ? g_strndup(content, 100) : content);
        gtk_label_set_text(GTK_LABEL(primary_label), display);
        g_free(display);
        g_variant_unref(result);
    }
}

static void update_history() {
    gtk_list_store_clear(history_store);
    
    GVariant *result = call_method("GetHistory", g_variant_new("(i)", 20));
    if (result) {
        GVariantIter *iter;
        const gchar *content;
        gint64 timestamp;
        gboolean is_primary;
        
        g_variant_get(result, "(a(sxb))", &iter);
        
        while (g_variant_iter_next(iter, "(&sxb)", &content, &timestamp, &is_primary)) {
            GtkTreeIter tree_iter;
            gtk_list_store_append(history_store, &tree_iter);
            
            char time_str[32];
            time_t t = timestamp / 1000000;
            strftime(time_str, sizeof(time_str), "%H:%M:%S", localtime(&t));
            
            gchar *preview = g_strndup(content, 50);
            for (int i = 0; preview[i]; i++) {
                if (preview[i] == '\n') preview[i] = ' ';
            }
            
            gtk_list_store_set(history_store, &tree_iter,
                0, time_str,
                1, is_primary ? "P" : "C",
                2, preview,
                -1);
            
            g_free(preview);
        }
        
        g_variant_iter_free(iter);
        g_variant_unref(result);
    }
}

static void refresh_all(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data;
    update_current();
    update_history();
}

// ═══════════════════════════════════════════════════════════════════════════
// الأزرار
// ═══════════════════════════════════════════════════════════════════════════

static void on_clear_history(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data;
    GVariant *result = call_method("ClearHistory", NULL);
    if (result) {
        g_variant_unref(result);
        update_history();
    }
}

static void on_set_clipboard(GtkWidget *widget, gpointer entry) {
    (void)widget;
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(entry));
    if (strlen(text) > 0) {
        GVariant *result = call_method("SetClipboard", g_variant_new("(sb)", text, FALSE));
        if (result) {
            g_variant_unref(result);
            gtk_entry_set_text(GTK_ENTRY(entry), "");
            update_current();
            update_history();
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// إشارات D-Bus
// ═══════════════════════════════════════════════════════════════════════════

static void on_clipboard_changed(GDBusConnection *conn, const gchar *sender,
                                  const gchar *path, const gchar *iface,
                                  const gchar *signal, GVariant *params,
                                  gpointer data) {
    (void)conn; (void)sender; (void)path; (void)iface; (void)signal; (void)data;
    update_current();
    update_history();
}

// ═══════════════════════════════════════════════════════════════════════════
// تطبيق CSS
// ═══════════════════════════════════════════════════════════════════════════

static void apply_css() {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "window { background: #1a1a2e; }"
        "label { color: #eee; }"
        ".title { font-size: 18px; font-weight: bold; color: #00d4ff; }"
        ".content-box { background: #16213e; border-radius: 8px; padding: 12px; }"
        ".clipboard-text { font-family: monospace; color: #0ff; }"
        "button { background: #0f3460; color: #fff; border: none; border-radius: 4px; padding: 8px 16px; }"
        "button:hover { background: #00d4ff; color: #000; }"
        "entry { background: #16213e; color: #fff; border: 1px solid #0f3460; border-radius: 4px; padding: 8px; }"
        "treeview { background: #16213e; color: #eee; }"
        "treeview:selected { background: #0f3460; }"
        ".status { font-size: 11px; color: #888; }"
        , -1, NULL);
    
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);
}

// ═══════════════════════════════════════════════════════════════════════════
// إنشاء الواجهة
// ═══════════════════════════════════════════════════════════════════════════

static void build_ui() {
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "🧪 Venom Clipboard Test");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 600);
    gtk_container_set_border_width(GTK_CONTAINER(window), 16);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_add(GTK_CONTAINER(window), main_box);
    
    // العنوان
    GtkWidget *title = gtk_label_new("🧪 Venom Clipboard Test");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "title");
    gtk_box_pack_start(GTK_BOX(main_box), title, FALSE, FALSE, 0);
    
    // CLIPBOARD
    GtkWidget *clip_frame = gtk_frame_new(NULL);
    gtk_style_context_add_class(gtk_widget_get_style_context(clip_frame), "content-box");
    clipboard_label = gtk_label_new("📋 CLIPBOARD:\n(empty)");
    gtk_label_set_xalign(GTK_LABEL(clipboard_label), 0);
    gtk_label_set_line_wrap(GTK_LABEL(clipboard_label), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(clipboard_label), "clipboard-text");
    gtk_container_add(GTK_CONTAINER(clip_frame), clipboard_label);
    gtk_box_pack_start(GTK_BOX(main_box), clip_frame, FALSE, FALSE, 0);
    
    // PRIMARY
    GtkWidget *prim_frame = gtk_frame_new(NULL);
    gtk_style_context_add_class(gtk_widget_get_style_context(prim_frame), "content-box");
    primary_label = gtk_label_new("🖱️ PRIMARY:\n(empty)");
    gtk_label_set_xalign(GTK_LABEL(primary_label), 0);
    gtk_label_set_line_wrap(GTK_LABEL(primary_label), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(primary_label), "clipboard-text");
    gtk_container_add(GTK_CONTAINER(prim_frame), primary_label);
    gtk_box_pack_start(GTK_BOX(main_box), prim_frame, FALSE, FALSE, 0);
    
    // إدخال نص جديد
    GtkWidget *input_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "أدخل نص للحافظة...");
    GtkWidget *set_btn = gtk_button_new_with_label("📋 Set");
    g_signal_connect(set_btn, "clicked", G_CALLBACK(on_set_clipboard), entry);
    gtk_box_pack_start(GTK_BOX(input_box), entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(input_box), set_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), input_box, FALSE, FALSE, 0);
    
    // أزرار
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *refresh_btn = gtk_button_new_with_label("🔄 Refresh");
    GtkWidget *clear_btn = gtk_button_new_with_label("🗑️ Clear History");
    g_signal_connect(refresh_btn, "clicked", G_CALLBACK(refresh_all), NULL);
    g_signal_connect(clear_btn, "clicked", G_CALLBACK(on_clear_history), NULL);
    gtk_box_pack_start(GTK_BOX(btn_box), refresh_btn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(btn_box), clear_btn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), btn_box, FALSE, FALSE, 0);
    
    // السجل
    GtkWidget *history_label = gtk_label_new("📜 History:");
    gtk_label_set_xalign(GTK_LABEL(history_label), 0);
    gtk_box_pack_start(GTK_BOX(main_box), history_label, FALSE, FALSE, 0);
    
    history_store = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    history_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(history_store));
    
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_append_column(GTK_TREE_VIEW(history_list),
        gtk_tree_view_column_new_with_attributes("Time", renderer, "text", 0, NULL));
    gtk_tree_view_append_column(GTK_TREE_VIEW(history_list),
        gtk_tree_view_column_new_with_attributes("T", renderer, "text", 1, NULL));
    gtk_tree_view_append_column(GTK_TREE_VIEW(history_list),
        gtk_tree_view_column_new_with_attributes("Content", renderer, "text", 2, NULL));
    
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), history_list);
    gtk_box_pack_start(GTK_BOX(main_box), scroll, TRUE, TRUE, 0);
    
    // الحالة
    status_label = gtk_label_new("⏳ Connecting...");
    gtk_style_context_add_class(gtk_widget_get_style_context(status_label), "status");
    gtk_box_pack_start(GTK_BOX(main_box), status_label, FALSE, FALSE, 0);
    
    gtk_widget_show_all(window);
}

// ═══════════════════════════════════════════════════════════════════════════
// Main
// ═══════════════════════════════════════════════════════════════════════════

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    
    // الاتصال بـ D-Bus
    GError *error = NULL;
    dbus_conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (error) {
        g_printerr("D-Bus error: %s\n", error->message);
        g_error_free(error);
        return 1;
    }
    
    apply_css();
    build_ui();
    
    // الاستماع للتغييرات
    g_dbus_connection_signal_subscribe(
        dbus_conn, DBUS_NAME, DBUS_INTERFACE, "ClipboardChanged",
        DBUS_PATH, NULL, G_DBUS_SIGNAL_FLAGS_NONE,
        on_clipboard_changed, NULL, NULL
    );
    
    // تحديث أولي
    refresh_all(NULL, NULL);
    
    gtk_main();
    
    g_object_unref(dbus_conn);
    return 0;
}
