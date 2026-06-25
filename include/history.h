#ifndef AETHER_HISTORY_H
#define AETHER_HISTORY_H

#include "common.h"

void free_entry(ClipboardEntry *entry);
void add_to_history(const char *text, gboolean is_image, GdkPixbuf *preview);
void clear_history(void);
void on_clipboard_changed(GtkClipboard *clip, gpointer data);

#endif // AETHER_HISTORY_H
