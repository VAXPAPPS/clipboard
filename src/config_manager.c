#include "config_manager.h"
#include "common.h"
#include "ui.h"
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GtkCssProvider *css_provider = NULL;
static GFileMonitor *file_monitor = NULL;
static char config_path[512];

static const char *default_ini = 
"[Settings]\n"
"GhostMode=false\n\n"
"[Colors]\n"
"WindowBoxBg=rgba(0, 0, 0, 0.3)\n"
"WindowBoxBorder=rgba(255,255,255,0.09)\n"
"GlassPanelBg=rgba(0, 0, 0, 0)\n"
"HeaderTitle=rgba(255,255,255,1.0)\n"
"GhostPillBg=rgba(255,255,255,0.05)\n"
"GhostPillText=rgba(155,148,184,1.0)\n"
"GhostPillBorder=rgba(255,255,255,0.09)\n"
"GhostActiveText=rgba(251,113,133,1.0)\n"
"GhostActiveBg=rgba(251,113,133,0.14)\n"
"GhostActiveBorder=rgba(251,113,133,0.4)\n"
"SearchBoxBg=rgba(255,255,255,0.05)\n"
"SearchBoxBorder=rgba(255,255,255,0.09)\n"
"SearchBoxText=rgba(255,255,255,1.0)\n"
"SearchBoxFocusBorder=rgba(0, 0, 0, 0.44)\n"
"TabBtnBg=rgba(255,255,255,0.05)\n"
"TabBtnText=rgba(155,148,184,1.0)\n"
"TabBtnCheckedBg=rgba(192,132,252,0.16)\n"
"TabBtnCheckedText=rgba(192,132,252,1.0)\n"
"TabBtnCheckedBorder=rgba(192,132,252,0.3)\n"
"CardBg=rgba(255,255,255,0.05)\n"
"CardBorder=rgba(255,255,255,0.09)\n"
"CardHoverBorder=rgba(192,132,252,1.0)\n"
"CardText=rgba(255,255,255,1.0)\n"
"CardCodeText=rgba(96,217,201,1.0)\n"
"MetaText=rgba(155,148,184,1.0)\n"
"PinFlagBg=rgba(251,191,103,0.18)\n"
"PinFlagText=rgba(251,191,103,1.0)\n"
"MenuBg=rgba(0, 0, 0, 0.4)\n"
"MenuBorder=rgba(0, 0, 0, 0.5)\n"
"MenuItemText=rgba(255,255,255,1.0)\n"
"MenuItemHoverBg=rgba(0, 0, 0, 0.72)\n"
;

static char* get_color(GKeyFile *kf, const char *key, const char *fallback) {
    char *val = g_key_file_get_string(kf, "Colors", key, NULL);
    if (val) return val;
    return g_strdup(fallback);
}

void config_manager_reload(void) {
    GKeyFile *kf = g_key_file_new();
    if (!g_key_file_load_from_file(kf, config_path, G_KEY_FILE_NONE, NULL)) {
        g_key_file_free(kf);
        return;
    }
    
    // Settings
    gboolean new_ghost = g_key_file_get_boolean(kf, "Settings", "GhostMode", NULL);
    if (ghost_mode != new_ghost) {
        ghost_mode = new_ghost;
        update_ghost_button();
    }
    
    // CSS
    GString *css = g_string_new("");
    g_string_append(css, "window, .background { background-color: transparent; }");
    
    char *c_window_bg = get_color(kf, "WindowBoxBg", "rgba(0, 0, 0, 0.3)");
    char *c_window_border = get_color(kf, "WindowBoxBorder", "rgba(255,255,255,0.09)");
    g_string_append_printf(css, ".window-box { background: %s; border-radius: 22px; border: 1px solid %s; box-shadow: 0 30px 80px rgba(0,0,0,0.55), inset 0 1px 0 rgba(255,255,255,0.06); }", c_window_bg, c_window_border);
    g_free(c_window_bg); g_free(c_window_border);
    
    char *c_glass = get_color(kf, "GlassPanelBg", "rgba(0, 0, 0, 0)");
    g_string_append_printf(css, ".glass-panel { background-color: %s; border-radius: 22px; }", c_glass);
    g_free(c_glass);
    
    char *c_title = get_color(kf, "HeaderTitle", "rgba(255,255,255,1.0)");
    g_string_append_printf(css, ".header-title { font-size: 15px; font-weight: bold; color: %s; }", c_title);
    g_free(c_title);
    
    char *c_gp_bg = get_color(kf, "GhostPillBg", "rgba(255,255,255,0.05)");
    char *c_gp_text = get_color(kf, "GhostPillText", "rgba(155,148,184,1.0)");
    char *c_gp_border = get_color(kf, "GhostPillBorder", "rgba(255,255,255,0.09)");
    g_string_append_printf(css, ".ghost-pill { font-size: 11px; padding: 7px 13px; border-radius: 999px; background: %s; color: %s; border: 1px solid %s; }", c_gp_bg, c_gp_text, c_gp_border);
    g_free(c_gp_bg); g_free(c_gp_text); g_free(c_gp_border);
    
    char *c_ga_bg = get_color(kf, "GhostActiveBg", "rgba(251,113,133,0.14)");
    char *c_ga_text = get_color(kf, "GhostActiveText", "rgba(251,113,133,1.0)");
    char *c_ga_border = get_color(kf, "GhostActiveBorder", "rgba(251,113,133,0.4)");
    g_string_append_printf(css, ".ghost-active { color: %s; background: %s; border-color: %s; }", c_ga_text, c_ga_bg, c_ga_border);
    g_free(c_ga_bg); g_free(c_ga_text); g_free(c_ga_border);
    
    char *c_sb_bg = get_color(kf, "SearchBoxBg", "rgba(255,255,255,0.05)");
    char *c_sb_border = get_color(kf, "SearchBoxBorder", "rgba(255,255,255,0.09)");
    char *c_sb_text = get_color(kf, "SearchBoxText", "rgba(255,255,255,1.0)");
    char *c_sb_focus = get_color(kf, "SearchBoxFocusBorder", "rgba(0, 0, 0, 0.44)");
    g_string_append_printf(css, ".search-box { background: %s; border: 1px solid %s; border-radius: 14px; padding: 4px 14px; color: %s; } .search-box:focus { border-color: %s; }", c_sb_bg, c_sb_border, c_sb_text, c_sb_focus);
    g_free(c_sb_bg); g_free(c_sb_border); g_free(c_sb_text); g_free(c_sb_focus);
    
    char *c_tb_bg = get_color(kf, "TabBtnBg", "rgba(255,255,255,0.05)");
    char *c_tb_text = get_color(kf, "TabBtnText", "rgba(155,148,184,1.0)");
    g_string_append_printf(css, ".tab-btn { font-size: 11.5px; padding: 6px 12px; border-radius: 10px; background: %s; color: %s; border: 1px solid transparent; min-height: 24px; }", c_tb_bg, c_tb_text);
    g_free(c_tb_bg); g_free(c_tb_text);
    
    char *c_tbc_bg = get_color(kf, "TabBtnCheckedBg", "rgba(192,132,252,0.16)");
    char *c_tbc_text = get_color(kf, "TabBtnCheckedText", "rgba(192,132,252,1.0)");
    char *c_tbc_border = get_color(kf, "TabBtnCheckedBorder", "rgba(192,132,252,0.3)");
    g_string_append_printf(css, ".tab-btn:checked { background: %s; color: %s; border-color: %s; }", c_tbc_bg, c_tbc_text, c_tbc_border);
    g_free(c_tbc_bg); g_free(c_tbc_text); g_free(c_tbc_border);
    
    g_string_append(css, "flowbox { background: transparent; padding: 0 16px 16px; }");
    g_string_append(css, "flowboxchild { background: transparent; padding: 0; outline: none; border: none; }");
    g_string_append(css, "flowboxchild:selected { background: transparent; }");
    
    char *c_card_bg = get_color(kf, "CardBg", "rgba(255,255,255,0.05)");
    char *c_card_border = get_color(kf, "CardBorder", "rgba(255,255,255,0.09)");
    char *c_card_hover = get_color(kf, "CardHoverBorder", "rgba(192,132,252,1.0)");
    g_string_append_printf(css, ".card { background: %s; border: 1px solid %s; border-radius: 14px; padding: 10px; transition: all 150ms; } flowboxchild:hover .card { border-color: %s; }", c_card_bg, c_card_border, c_card_hover);
    g_free(c_card_bg); g_free(c_card_border); g_free(c_card_hover);
    
    g_string_append(css, ".card.wide { min-width: 420px; }");
    
    char *c_card_text = get_color(kf, "CardText", "rgba(255,255,255,1.0)");
    char *c_card_code = get_color(kf, "CardCodeText", "rgba(96,217,201,1.0)");
    char *c_meta = get_color(kf, "MetaText", "rgba(155,148,184,1.0)");
    g_string_append_printf(css, ".card-text { font-size: 12px; color: %s; } .card-code { font-family: monospace; font-size: 11px; color: %s; } .meta { font-size: 10px; color: %s; margin-top: 6px; }", c_card_text, c_card_code, c_meta);
    g_free(c_card_text); g_free(c_card_code); g_free(c_meta);
    
    char *c_pin_bg = get_color(kf, "PinFlagBg", "rgba(251,191,103,0.18)");
    char *c_pin_text = get_color(kf, "PinFlagText", "rgba(251,191,103,1.0)");
    g_string_append_printf(css, ".pin-flag { background: %s; color: %s; border-radius: 50%%; font-size: 10px; padding: 2px 6px; margin-bottom: 4px; }", c_pin_bg, c_pin_text);
    g_free(c_pin_bg); g_free(c_pin_text);
    
    g_string_append(css, ".img-wrap { background: linear-gradient(135deg,#2c2740,#1b1828); border-radius: 10px; margin-bottom: 8px; }");
    
    char *c_menu_bg = get_color(kf, "MenuBg", "rgba(0, 0, 0, 0.4)");
    char *c_menu_border = get_color(kf, "MenuBorder", "rgba(0, 0, 0, 0.5)");
    char *c_mi_text = get_color(kf, "MenuItemText", "rgba(255,255,255,1.0)");
    char *c_mi_hover = get_color(kf, "MenuItemHoverBg", "rgba(0, 0, 0, 0.72)");
    g_string_append_printf(css, "menu { background: %s; border-radius: 8px; padding: 4px; border: 1px solid %s; } menuitem { color: %s; padding: 6px 12px; border-radius: 4px; } menuitem:hover { background: %s; }", c_menu_bg, c_menu_border, c_mi_text, c_mi_hover);
    g_free(c_menu_bg); g_free(c_menu_border); g_free(c_mi_text); g_free(c_mi_hover);
    
    g_key_file_free(kf);
    
    if (css_provider) {
        gtk_css_provider_load_from_data(css_provider, css->str, -1, NULL);
    }
    
    g_string_free(css, TRUE);
}

static void on_file_changed(GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, gpointer user_data) {
    (void)monitor; (void)file; (void)other_file; (void)user_data;
    if (event_type == G_FILE_MONITOR_EVENT_CHANGED || event_type == G_FILE_MONITOR_EVENT_CREATED) {
        config_manager_reload();
    }
}

void config_manager_init(void) {
    const char *home = g_get_home_dir();
    snprintf(config_path, sizeof(config_path), "%s/.config/vaxp/clipboard/clipboard.vaxp", home);
    
    char dir_path[512];
    snprintf(dir_path, sizeof(dir_path), "%s/.config/vaxp/clipboard", home);
    
    if (!g_file_test(dir_path, G_FILE_TEST_EXISTS)) {
        g_mkdir_with_parents(dir_path, 0755);
    }
    
    if (!g_file_test(config_path, G_FILE_TEST_EXISTS)) {
        FILE *f = fopen(config_path, "w");
        if (f) {
            fputs(default_ini, f);
            fclose(f);
        }
    }
    
    css_provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        
    config_manager_reload();
    
    GFile *gfile = g_file_new_for_path(config_path);
    file_monitor = g_file_monitor_file(gfile, G_FILE_MONITOR_NONE, NULL, NULL);
    g_signal_connect(file_monitor, "changed", G_CALLBACK(on_file_changed), NULL);
    g_object_unref(gfile);
}
