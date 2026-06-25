#include "ui.h"
#include "history.h"
#include "db.h"

static int context_menu_index = -1;
static char current_tab_filter[32] = "all"; // all, pinned, image, text

static void rebuild_flowbox(void);



// ═══════════════════════════════════════════════════════════════════════════
// Right-Click Context Menu Logic
// ═══════════════════════════════════════════════════════════════════════════

static void on_menu_pin(GtkMenuItem *item, gpointer data) {
    (void)item; (void)data;
    if (context_menu_index >= 0 && context_menu_index < history_count && history[context_menu_index]) {
        ClipboardEntry *entry = history[context_menu_index];
        if (!entry->pinned) {
            if (pin_entry(entry->content, entry->timestamp, entry->is_image)) {
                entry->pinned = TRUE;
                for (int i = context_menu_index; i > 0; i--) {
                    if (history[i-1] && !history[i-1]->pinned) {
                        ClipboardEntry *temp = history[i];
                        history[i] = history[i-1];
                        history[i-1] = temp;
                    } else break;
                }
                rebuild_flowbox();
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
                rebuild_flowbox();
            }
        }
    }
}

static void on_menu_delete(GtkMenuItem *item, gpointer data) {
    (void)item; (void)data;
    if (context_menu_index >= 0 && context_menu_index < history_count && history[context_menu_index]) {
        ClipboardEntry *entry = history[context_menu_index];
        if (entry->pinned) { unpin_entry(entry->id); entry->pinned = FALSE; }
        free_entry(entry);
        for (int i = context_menu_index; i < history_count - 1; i++) {
            history[i] = history[i + 1];
        }
        history[--history_count] = NULL;
        rebuild_flowbox();
    }
}

static void on_menu_copy(GtkMenuItem *item, gpointer data) {
    (void)item; (void)data;
    if (context_menu_index >= 0 && context_menu_index < history_count && history[context_menu_index]) {
        if (history[context_menu_index]->is_image) {
            GError *err = NULL;
            GdkPixbuf *img = gdk_pixbuf_new_from_file(history[context_menu_index]->content, &err);
            if (img) {
                gtk_clipboard_set_image(clipboard, img);
                g_object_unref(img);
                printf("📋 Copied Image\n");
            }
        } else {
            gtk_clipboard_set_text(clipboard, history[context_menu_index]->content, -1);
            printf("📋 Copied: %.50s...\n", history[context_menu_index]->content);
        }
    }
}

static gboolean on_child_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    (void)data;
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
        GtkWidget *child = gtk_widget_get_ancestor(widget, GTK_TYPE_FLOW_BOX_CHILD);
        if (child) {
            int index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(child), "index"));
            context_menu_index = index;
            ClipboardEntry *entry = history[index];
            
            GtkWidget *menu = gtk_menu_new();
            
            GtkWidget *copy_item = gtk_menu_item_new_with_label("📋 Copy");
            g_signal_connect(copy_item, "activate", G_CALLBACK(on_menu_copy), NULL);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), copy_item);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
            
            if (entry->pinned) {
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
            return TRUE;
        }
    }
    return FALSE;
}

static void on_child_activated(GtkFlowBox *box, GtkFlowBoxChild *child, gpointer data) {
    (void)box; (void)data;
    int index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(child), "index"));
    if (index >= 0 && index < history_count && history[index]) {
        if (history[index]->is_image) {
            GError *err = NULL;
            GdkPixbuf *img = gdk_pixbuf_new_from_file(history[index]->content, &err);
            if (img) {
                gtk_clipboard_set_image(clipboard, img);
                g_object_unref(img);
                printf("📋 Copied Image\n");
            }
        } else {
            gtk_clipboard_set_text(clipboard, history[index]->content, -1);
            printf("📋 Copied: %.50s...\n", history[index]->content);
        }
        gtk_widget_hide(main_window);
        window_visible = FALSE;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// UI Construction
// ═══════════════════════════════════════════════════════════════════════════

static GtkWidget* create_card(int index, ClipboardEntry *entry) {
    (void)index;
    GtkWidget *event_box = gtk_event_box_new();
    g_signal_connect(event_box, "button-press-event", G_CALLBACK(on_child_button_press), NULL);
    
    gtk_widget_set_halign(event_box, GTK_ALIGN_START);
    gtk_widget_set_valign(event_box, GTK_ALIGN_START);

    
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(card), "card");
    gtk_widget_set_size_request(card, 205, -1);
    
    if (entry->pinned) {
        GtkWidget *pin_lbl = gtk_label_new("★");
        gtk_style_context_add_class(gtk_widget_get_style_context(pin_lbl), "pin-flag");
        gtk_widget_set_halign(pin_lbl, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(card), pin_lbl, FALSE, FALSE, 0);
    }
    
    if (entry->is_image && entry->preview) {
        GtkWidget *img = gtk_image_new_from_pixbuf(entry->preview);
        GtkWidget *img_wrap = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_style_context_add_class(gtk_widget_get_style_context(img_wrap), "img-wrap");
        gtk_box_pack_start(GTK_BOX(img_wrap), img, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(card), img_wrap, FALSE, FALSE, 0);
    } else {
        gchar *preview = NULL;
        if (g_utf8_validate(entry->content, -1, NULL)) {
            glong chars = g_utf8_strlen(entry->content, -1);
            preview = g_utf8_substring(entry->content, 0, chars > 200 ? 200 : chars);
        } else {
            preview = g_strdup("Invalid UTF-8 Text");
        }
        for (int j = 0; preview[j]; j++) { if (preview[j] == '\r') preview[j] = ' '; }
        GtkWidget *lbl = gtk_label_new(preview);
        gtk_label_set_line_wrap(GTK_LABEL(lbl), TRUE);
        gtk_label_set_line_wrap_mode(GTK_LABEL(lbl), PANGO_WRAP_WORD_CHAR);
        gtk_label_set_max_width_chars(GTK_LABEL(lbl), 1);
        gtk_widget_set_size_request(lbl, 185, -1);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0);
        gtk_label_set_yalign(GTK_LABEL(lbl), 0);
        gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "card-text");
        if (g_str_has_prefix(entry->content, "http") || strstr(entry->content, "()")) {
            gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "card-code");
        }
        gtk_box_pack_start(GTK_BOX(card), lbl, TRUE, TRUE, 0);
        g_free(preview);
    }
    
    GtkWidget *meta_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(meta_box), "meta");
    
    char time_str[32];
    time_t t = entry->timestamp / 1000000;
    strftime(time_str, sizeof(time_str), "%H:%M", localtime(&t));
    
    GtkWidget *type_lbl = gtk_label_new(entry->is_image ? "Image" : "Text");
    GtkWidget *time_lbl = gtk_label_new(time_str);
    
    gtk_box_pack_start(GTK_BOX(meta_box), type_lbl, FALSE, FALSE, 0);
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(meta_box), spacer, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(meta_box), time_lbl, FALSE, FALSE, 0);
    
    gtk_box_pack_end(GTK_BOX(card), meta_box, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(event_box), card);
    
    return event_box;
}

static gint flowbox_sort_func(GtkFlowBoxChild *child1, GtkFlowBoxChild *child2, gpointer user_data) {
    (void)user_data;
    int idx1 = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(child1), "index"));
    int idx2 = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(child2), "index"));
    
    if (idx1 < 0 || idx1 >= history_count || !history[idx1]) return 0;
    if (idx2 < 0 || idx2 >= history_count || !history[idx2]) return 0;
    
    ClipboardEntry *e1 = history[idx1];
    ClipboardEntry *e2 = history[idx2];
    
    // 1. Pinned
    if (e1->pinned != e2->pinned) return e1->pinned ? -1 : 1;
    // 2. Images
    if (e1->is_image != e2->is_image) return e1->is_image ? -1 : 1;
    
    // 3. Short texts before long texts
    if (!e1->is_image && !e2->is_image) {
        int e1_short = (e1->content && strlen(e1->content) <= 200) ? 1 : 0;
        int e2_short = (e2->content && strlen(e2->content) <= 200) ? 1 : 0;
        if (e1_short != e2_short) return e1_short ? -1 : 1;
    }
    
    // 4. Chronological
    return idx1 - idx2;
}

static void rebuild_flowbox() {
    if (!flow_box) return;
    
    GList *children = gtk_container_get_children(GTK_CONTAINER(flow_box));
    for (GList *iter = children; iter != NULL; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);
    
    for (int i = 0; i < history_count; i++) {
        if (!history[i]) continue;
        GtkWidget *child = gtk_flow_box_child_new();
        g_object_set_data(G_OBJECT(child), "index", GINT_TO_POINTER(i));
        
        GtkWidget *card = create_card(i, history[i]);
        gtk_container_add(GTK_CONTAINER(child), card);
        gtk_container_add(GTK_CONTAINER(flow_box), child);
    }
    gtk_widget_show_all(flow_box);
    gtk_flow_box_invalidate_filter(GTK_FLOW_BOX(flow_box));
}

void update_ui_list(const char *filter) {
    (void)filter;
    rebuild_flowbox();
}

static gboolean flowbox_filter_func(GtkFlowBoxChild *child, gpointer user_data) {
    (void)user_data;
    int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(child), "index"));
    if (idx < 0 || idx >= history_count || !history[idx]) return FALSE;
    ClipboardEntry *entry = history[idx];
    
    if (g_strcmp0(current_tab_filter, "pinned") == 0 && !entry->pinned) return FALSE;
    if (g_strcmp0(current_tab_filter, "image") == 0 && !entry->is_image) return FALSE;
    if (g_strcmp0(current_tab_filter, "text") == 0 && entry->is_image) return FALSE;
    
    if (search_entry) {
        const char *search_text = gtk_entry_get_text(GTK_ENTRY(search_entry));
        if (search_text && strlen(search_text) > 0) {
            if (entry->is_image) return FALSE;
            if (!strcasestr(entry->content, search_text)) return FALSE;
        }
    }
    return TRUE;
}

static void on_search_changed(GtkEntry *entry, gpointer data) {
    (void)entry; (void)data;
    if (flow_box) gtk_flow_box_invalidate_filter(GTK_FLOW_BOX(flow_box));
}

static void on_tab_toggled(GtkToggleButton *btn, gpointer data) {
    if (gtk_toggle_button_get_active(btn)) {
        strncpy(current_tab_filter, (char*)data, sizeof(current_tab_filter)-1);
        if (flow_box) gtk_flow_box_invalidate_filter(GTK_FLOW_BOX(flow_box));
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// UI Event Handlers
// ═══════════════════════════════════════════════════════════════════════════

void update_ghost_button() {
    if (ghost_btn) {
        if (ghost_mode) {
            gtk_button_set_label(GTK_BUTTON(ghost_btn), "👻 Ghost (ON)");
            gtk_style_context_add_class(gtk_widget_get_style_context(ghost_btn), "ghost-active");
        } else {
            gtk_button_set_label(GTK_BUTTON(ghost_btn), "👻 Ghost");
            gtk_style_context_remove_class(gtk_widget_get_style_context(ghost_btn), "ghost-active");
        }
    }
}

void toggle_ghost_mode() {
    ghost_mode = !ghost_mode;
    update_ghost_button();
    printf("👻 Ghost Mode: %s\n", ghost_mode ? "ON" : "OFF");
    if (dbus_conn) {
        g_dbus_connection_emit_signal(dbus_conn, NULL, DBUS_PATH, DBUS_INTERFACE,
            "GhostModeChanged", g_variant_new("(b)", ghost_mode), NULL);
    }
}

static void on_ghost_clicked(GtkButton *btn, gpointer data) {
    (void)btn; (void)data;
    toggle_ghost_mode();
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
// Main Window Builder
// ═══════════════════════════════════════════════════════════════════════════

void build_window() {
    main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(main_window), "aether Clipboard");
    gtk_window_set_default_size(GTK_WINDOW(main_window), 480, 620);
    gtk_window_set_position(GTK_WINDOW(main_window), GTK_WIN_POS_CENTER);
    gtk_window_set_decorated(GTK_WINDOW(main_window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(main_window), TRUE);
    gtk_widget_set_app_paintable(main_window, TRUE);
    
    GdkScreen *screen = gtk_widget_get_screen(main_window);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (visual) gtk_widget_set_visual(main_window, visual);
    
    g_signal_connect(main_window, "delete-event", G_CALLBACK(on_window_delete), NULL);
    g_signal_connect(main_window, "key-press-event", G_CALLBACK(on_key_press), NULL);
    
    GtkWidget *bg_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(bg_box), "window-box");
    gtk_container_add(GTK_CONTAINER(main_window), bg_box);
    
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(main_box), "glass-panel");
    gtk_box_pack_start(GTK_BOX(bg_box), main_box, TRUE, TRUE, 0);
    
    // Header
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_top(header, 18);
    gtk_widget_set_margin_bottom(header, 12);
    gtk_widget_set_margin_start(header, 20);
    gtk_widget_set_margin_end(header, 20);
    
    GtkWidget *title = gtk_label_new("Clipboard");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "header-title");
    
    ghost_btn = gtk_button_new_with_label("👻 Ghost");
    gtk_style_context_add_class(gtk_widget_get_style_context(ghost_btn), "ghost-pill");
    g_signal_connect(ghost_btn, "clicked", G_CALLBACK(on_ghost_clicked), NULL);
    
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    
    // LTR layout order for header
    gtk_box_pack_start(GTK_BOX(header), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header), spacer, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(header), ghost_btn, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(main_box), header, FALSE, FALSE, 0);
    
    // Search
    GtkWidget *search_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_bottom(search_wrap, 14);
    gtk_widget_set_margin_start(search_wrap, 20);
    gtk_widget_set_margin_end(search_wrap, 20);
    
    search_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry), "Search...");
    gtk_style_context_add_class(gtk_widget_get_style_context(search_entry), "search-box");
    g_signal_connect(search_entry, "changed", G_CALLBACK(on_search_changed), NULL);
    
    gtk_box_pack_start(GTK_BOX(search_wrap), search_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), search_wrap, FALSE, FALSE, 0);
    
    // Tabs
    GtkWidget *tabs_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_bottom(tabs_box, 14);
    gtk_widget_set_margin_start(tabs_box, 20);
    gtk_widget_set_margin_end(tabs_box, 20);
    
    GtkWidget *tab_all = gtk_radio_button_new_with_label(NULL, "All");
    GtkWidget *tab_pin = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(tab_all), "Pinned");
    GtkWidget *tab_img = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(tab_all), "Images");
    GtkWidget *tab_txt = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(tab_all), "Texts");
    
    // Remove default radio styling, make them look like buttons
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(tab_all), FALSE);
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(tab_pin), FALSE);
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(tab_img), FALSE);
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(tab_txt), FALSE);
    
    gtk_style_context_add_class(gtk_widget_get_style_context(tab_all), "tab-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(tab_pin), "tab-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(tab_img), "tab-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(tab_txt), "tab-btn");
    
    g_signal_connect(tab_all, "toggled", G_CALLBACK(on_tab_toggled), "all");
    g_signal_connect(tab_pin, "toggled", G_CALLBACK(on_tab_toggled), "pinned");
    g_signal_connect(tab_img, "toggled", G_CALLBACK(on_tab_toggled), "image");
    g_signal_connect(tab_txt, "toggled", G_CALLBACK(on_tab_toggled), "text");
    
    gtk_box_pack_start(GTK_BOX(tabs_box), tab_all, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tabs_box), tab_pin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tabs_box), tab_img, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tabs_box), tab_txt, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(main_box), tabs_box, FALSE, FALSE, 0);
    
    // Grid (FlowBox)
    flow_box = gtk_flow_box_new();
    gtk_widget_set_valign(GTK_WIDGET(flow_box), GTK_ALIGN_START);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow_box), 2);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flow_box), 1);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow_box), GTK_SELECTION_NONE);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flow_box), 10);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flow_box), 10);
    
    gtk_flow_box_set_filter_func(GTK_FLOW_BOX(flow_box), flowbox_filter_func, NULL, NULL);
    gtk_flow_box_set_sort_func(GTK_FLOW_BOX(flow_box), flowbox_sort_func, NULL, NULL);
    g_signal_connect(flow_box, "child-activated", G_CALLBACK(on_child_activated), NULL);
    
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), flow_box);
    
    gtk_box_pack_start(GTK_BOX(main_box), scroll, TRUE, TRUE, 0);
    
    // Set text direction to LTR globally
    gtk_widget_set_direction(main_window, GTK_TEXT_DIR_LTR);
}
