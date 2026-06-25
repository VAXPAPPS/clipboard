#include "dbus_service.h"
#include "ui.h"
#include "history.h"

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

void on_bus_acquired(GDBusConnection *c, const gchar *n, gpointer d) {
    (void)n; (void)d;
    GDBusNodeInfo *info = g_dbus_node_info_new_for_xml(introspection_xml, NULL);
    g_dbus_connection_register_object(c, DBUS_PATH, info->interfaces[0], &vtable, NULL, NULL, NULL);
    dbus_conn = c;
    g_dbus_node_info_unref(info);
    printf("📡 D-Bus ready\n");
}
