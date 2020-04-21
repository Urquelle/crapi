#include "mysql.h"

typedef struct {
    void *handle;
    Map stmts;
} Db;

typedef struct {
    char *name;
    size_t size;
    void *data;
} Db_Row;

typedef struct {
    void *stmt;
    void *params;
    size_t num_params;
} Db_Stmt;

typedef struct {
    int32_t success;
    char *data;
} Db_Result;

Db_Stmt *
db_stmt_new(void *stmt, void *params, size_t num_params) {
    Db_Stmt *result = (Db_Stmt *)xmalloc(sizeof(Db_Stmt));

    result->stmt = stmt;
    result->params = params;
    result->num_params = num_params;

    return result;
}

Db *
db_init(char *host, char *user, char *passwd, char *db) {
    MYSQL *mysql = mysql_init(NULL);
    if ( !mysql ) {
        return NULL;
    }

    mysql = mysql_real_connect(mysql, host, user, passwd, db, 0, NULL, 0);
    if ( !mysql ) {
        return NULL;
    }

    Db *result = (Db *)xmalloc(sizeof(Db));

    result->handle = mysql;
    result->stmts.cap = 0;

    return result;
}

Db_Result
db_query(Db *db, char *query, size_t len) {
    Db_Result result = {0};

    if ( mysql_real_query(db->handle, query, (unsigned long)len) ) {
        return result;
    }

    MYSQL_RES *mysql_result = mysql_store_result(db->handle);
    if ( mysql_result ) {
        uint32_t num_fields = mysql_num_fields(mysql_result);

        MYSQL_ROW row;
        char buf[1000];

        /* @TODO: umwandlung zu json auslagern. stattdessen im Db_Result
         *        entsprechende zeilen als datenstruktur zurückliefern
         */
        sprintf(buf, "[");
        while ( (row = mysql_fetch_row(mysql_result)) ) {
            sprintf(buf, "%s{", buf);
            for ( uint32_t i = 0; i < num_fields; ++i ) {
                MYSQL_FIELD field = mysql_result->fields[i];
                sprintf(buf, "%s%s\"%s\": \"%s\"", buf, (i == 0) ? "" : ",", field.name, row[i]);
            }
            sprintf(buf, "%s},", buf);
        }

        sprintf(buf, "%s ]\0", buf);
        size_t buf_len = string_len(buf);

        result.data = (char *)xmalloc(buf_len);
        string_copy(buf, result.data, buf_len);
    } else {
        if ( mysql_errno(db->handle) ) {
           fprintf(stderr, "Db Fehler: %s\n", mysql_error(db->handle));
        } else if ( mysql_field_count(db->handle) == 0 ) {
            // falls es kein select war
            uint64_t num_rows = mysql_affected_rows(db->handle);
        }
    }

    return result;
}

bool
db_stmt_create(Db *db, char *key, char *query, int num_params) {
    MYSQL_STMT *stmt = mysql_stmt_init(db->handle);
    MYSQL_BIND *params = (MYSQL_BIND *)xcalloc(num_params, sizeof(MYSQL_BIND));

    if ( mysql_stmt_prepare(stmt, query, string_len(query)) ) {
        return false;
    }

    Db_Stmt *result = db_stmt_new(stmt, params, num_params);
    map_push(&db->stmts, key, result);

    return true;
}

Db_Stmt *
db_stmt_get(Db *db, char *key) {
    Db_Stmt *result = map_get(&db->stmts, key);

    return result;
}

void
db_stmt_param_set(Db_Stmt *stmt, int idx, uint32_t type, void *value) {
    MYSQL_BIND *param = (MYSQL_BIND *)stmt->params + idx;
    param->buffer_type = type;
    param->buffer = value;
    param->is_null = 0;
    param->length = 0;
}

bool
db_stmt_exec(Db_Stmt *stmt) {
    if ( mysql_stmt_execute(stmt->stmt) ) {
        return false;
    }

    return true;
}
