#ifndef AETHER_DB_H
#define AETHER_DB_H

#include "common.h"

void init_database(void);
void load_pinned_entries(void);
gboolean pin_entry(const char *content, gint64 timestamp, gboolean is_image);
gboolean unpin_entry(int id);

#endif // AETHER_DB_H
