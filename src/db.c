#include "db.h"

static char* get_db_path() {
    const char *home = g_get_home_dir();
    return g_strdup_printf("%s%s", home, DB_PATH);
}

void init_database() {
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
        "  created_at INTEGER DEFAULT (strftime('%s', 'now')),"
        "  is_image INTEGER DEFAULT 0"
        ");";
    
    char *err = NULL;
    sqlite3_exec(db, sql, NULL, NULL, &err);
    if (err) {
        printf("❌ SQL error: %s\n", err);
        sqlite3_free(err);
    }
    
    // الترقية التلقائية لقاعدة البيانات لدعم الصور
    sqlite3_exec(db, "ALTER TABLE pinned ADD COLUMN is_image INTEGER DEFAULT 0;", NULL, NULL, NULL);
    
    printf("📦 Database: %s\n", db_path);
    g_free(db_path);
}

void load_pinned_entries() {
    if (!db) return;
    
    const char *sql = "SELECT id, content, timestamp, is_image FROM pinned ORDER BY created_at DESC;";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            if (history_count >= MAX_HISTORY_SIZE) break;
            
            ClipboardEntry *entry = g_new0(ClipboardEntry, 1);
            entry->id = sqlite3_column_int(stmt, 0);
            entry->content = g_strdup((const char*)sqlite3_column_text(stmt, 1));
            entry->timestamp = sqlite3_column_int64(stmt, 2);
            entry->pinned = TRUE;
            entry->is_image = sqlite3_column_int(stmt, 3);
            if (entry->is_image) {
                GError *err = NULL;
                entry->preview = gdk_pixbuf_new_from_file_at_scale(entry->content, -1, 64, TRUE, &err);
                if (!entry->preview && err) {
                    g_error_free(err);
                }
            } else {
                entry->preview = NULL;
            }
            
            history[history_count++] = entry;
            if (entry->id >= next_id) next_id = entry->id + 1;
        }
        sqlite3_finalize(stmt);
    }
    
    printf("📌 Loaded %d pinned entries\n", history_count);
}

gboolean pin_entry(const char *content, gint64 timestamp, gboolean is_image) {
    if (!db || !content) return FALSE;
    
    const char *sql = "INSERT OR IGNORE INTO pinned (content, timestamp, is_image) VALUES (?, ?, ?);";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, content, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, timestamp);
        sqlite3_bind_int(stmt, 3, is_image ? 1 : 0);
        
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            sqlite3_finalize(stmt);
            printf("📌 Pinned: %.50s...\n", content);
            return TRUE;
        }
        sqlite3_finalize(stmt);
    }
    return FALSE;
}

gboolean unpin_entry(int id) {
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
