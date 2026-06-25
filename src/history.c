#include "history.h"
#include "ui.h"

void free_entry(ClipboardEntry *entry) {
    if (entry) {
        if (entry->is_image && !entry->pinned) {
            remove(entry->content);
        }
        if (entry->preview) {
            g_object_unref(entry->preview);
        }
        g_free(entry->content);
        g_free(entry);
    }
}

void add_to_history(const char *text, gboolean is_image, GdkPixbuf *preview) {
    if (!text || strlen(text) == 0) return;
    if (!is_image && strlen(text) > MAX_ENTRY_LENGTH) return;
    
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
    entry->is_image = is_image;
    if (preview) {
        entry->preview = g_object_ref(preview);
    } else {
        entry->preview = NULL;
    }
    
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

void clear_history() {
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

void on_clipboard_changed(GtkClipboard *clip, gpointer data) {
    (void)data;
    
    // Ghost Mode - لا تسجل أي شيء
    if (ghost_mode) {
        return;
    }
    
    // Image Check (direct image data)
    if (gtk_clipboard_wait_is_image_available(clip)) {
        GdkPixbuf *img = gtk_clipboard_wait_for_image(clip);
        if (img) {
            char *dir = g_strdup_printf("%s/.local/share/aether/images", g_get_home_dir());
            g_mkdir_with_parents(dir, 0755);
            gint64 ts = g_get_real_time();
            char *filepath = g_strdup_printf("%s/img_%ld.png", dir, ts);
            
            GError *err = NULL;
            if (gdk_pixbuf_save(img, filepath, "png", &err, NULL)) {
                if (last_clipboard_text && g_strcmp0(last_clipboard_text, filepath) == 0) {
                    g_free(filepath);
                } else {
                    g_free(last_clipboard_text);
                    last_clipboard_text = g_strdup(filepath);
                    
                    int w = gdk_pixbuf_get_width(img);
                    int h = gdk_pixbuf_get_height(img);
                    GdkPixbuf *preview = NULL;
                    if (h > 0) preview = gdk_pixbuf_scale_simple(img, w * 64 / h, 64, GDK_INTERP_BILINEAR);
                    
                    add_to_history(filepath, TRUE, preview);
                    
                    if (preview) g_object_unref(preview);
                    
                    if (window_visible && search_entry) {
                        const char *filter = gtk_entry_get_text(GTK_ENTRY(search_entry));
                        update_ui_list(filter);
                    }
                    if (dbus_conn) {
                        g_dbus_connection_emit_signal(dbus_conn, NULL, DBUS_PATH, DBUS_INTERFACE,
                            "ClipboardChanged", g_variant_new("(s)", "[Image]"), NULL);
                    }
                }
            } else {
                if (err) g_error_free(err);
                g_free(filepath);
            }
            g_free(dir);
            g_object_unref(img);
            return;
        }
    }

    gchar *text = gtk_clipboard_wait_for_text(clip);
    if (!text) return;
    
    if (last_clipboard_text && g_strcmp0(last_clipboard_text, text) == 0) {
        g_free(text);
        return;
    }
    
    // Check if text is a file URI or path pointing to an image
    gchar *image_path = NULL;
    if (g_str_has_prefix(text, "file://")) {
        image_path = g_filename_from_uri(text, NULL, NULL);
    } else if (g_str_has_prefix(text, "/")) {
        image_path = g_strdup(text);
        g_strchomp(image_path); // remove newlines
    }
    
    if (image_path) {
        // Try to load as image
        GdkPixbuf *img = gdk_pixbuf_new_from_file(image_path, NULL);
        if (img) {
            char *dir = g_strdup_printf("%s/.local/share/aether/images", g_get_home_dir());
            g_mkdir_with_parents(dir, 0755);
            gint64 ts = g_get_real_time();
            char *filepath = g_strdup_printf("%s/img_%ld.png", dir, ts);
            
            GError *err = NULL;
            if (gdk_pixbuf_save(img, filepath, "png", &err, NULL)) {
                g_free(last_clipboard_text);
                last_clipboard_text = g_strdup(text); // prevent loop
                
                int w = gdk_pixbuf_get_width(img);
                int h = gdk_pixbuf_get_height(img);
                GdkPixbuf *preview = NULL;
                if (h > 0) preview = gdk_pixbuf_scale_simple(img, w * 64 / h, 64, GDK_INTERP_BILINEAR);
                
                add_to_history(filepath, TRUE, preview);
                
                if (preview) g_object_unref(preview);
                
                if (window_visible && search_entry) {
                    const char *filter = gtk_entry_get_text(GTK_ENTRY(search_entry));
                    update_ui_list(filter);
                }
                if (dbus_conn) {
                    g_dbus_connection_emit_signal(dbus_conn, NULL, DBUS_PATH, DBUS_INTERFACE,
                        "ClipboardChanged", g_variant_new("(s)", "[Image]"), NULL);
                }
                
                g_free(filepath);
                g_free(dir);
                g_object_unref(img);
                g_free(image_path);
                g_free(text);
                return;
            } else {
                if (err) g_error_free(err);
                g_free(filepath);
            }
            g_free(dir);
            g_object_unref(img);
        }
        g_free(image_path);
    }
    
    g_free(last_clipboard_text);
    last_clipboard_text = g_strdup(text);
    
    add_to_history(text, FALSE, NULL);
    
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
