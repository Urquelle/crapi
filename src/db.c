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
} Db_Stmt;

typedef struct {
    MYSQL_BIND *elems;
    size_t num_elems;
} Db_Param;

typedef struct {
    int32_t success;
    char *data;
} Db_Result;

Db_Stmt *
db_stmt_new(void *stmt) {
    Db_Stmt *result = (Db_Stmt *)xmalloc(sizeof(Db_Stmt));

    result->stmt = stmt;

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
    result->stmts.vals = 0;
    result->stmts.keys = 0;
    result->stmts.len = 0;

    return result;
}

Db_Result
db_query(Db *db, char *query) {
    Db_Result result = {0};

    if ( mysql_real_query(db->handle, query, string_len(query)) ) {
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

Db_Param
db_params(size_t num_elems) {
    Db_Param result = {0};

    result.elems = (MYSQL_BIND *)xcalloc(num_elems, sizeof(MYSQL_BIND));
    result.num_elems = num_elems;

    return result;
}

void
db_param_set(Db_Param *params, size_t idx, int type, void *data) {
    params->elems[idx].buffer_type = type;
    params->elems[idx].buffer = (void *)data;
    params->elems[idx].is_null = 0;
    params->elems[idx].length = 0;
}

bool
db_stmt_create(Db *db, char *key, char *query) {
    MYSQL_STMT *stmt = mysql_stmt_init(db->handle);

    if ( mysql_stmt_prepare(stmt, query, string_len(query)) ) {
        return false;
    }

    Db_Stmt *result = db_stmt_new(stmt);
    map_push(&db->stmts, str_intern(key), result);

    return true;
}

Db_Stmt *
db_stmt_get(Db *db, char *key) {
    Db_Stmt *result = map_get(&db->stmts, str_intern(key));

    return result;
}

char global_buf[1000];
char *
db_bind_to_string(MYSQL_BIND *bind) {
    switch ( bind->buffer_type ) {
        case MYSQL_TYPE_LONG: {
            sprintf(global_buf, "%d", *(int *)bind->buffer);

            return global_buf;
        } break;

        case MYSQL_TYPE_VAR_STRING: {
            return (char *)bind->buffer;
        }

        default: {
            assert(0);
            return "";
        } break;
    }
}

Db_Result
db_stmt_exec(Db_Stmt *stmt, Db_Param *params) {
    Db_Result result = {0};

    mysql_stmt_bind_param(stmt->stmt, params->elems);

    if ( mysql_stmt_execute(stmt->stmt) ) {
        printf("Db Fehler: %s", mysql_stmt_error(stmt->stmt));
        return result;
    }

    MYSQL_BIND *bind = 0;
    MYSQL_RES *mysql_result = mysql_stmt_result_metadata(stmt->stmt);
    uint32_t num_fields = mysql_num_fields(mysql_result);
    MYSQL_FIELD **fields = (MYSQL_FIELD **)xmalloc(sizeof(MYSQL_FIELD *)*num_fields);

    if ( mysql_result ) {
        bind = (MYSQL_BIND *)xcalloc(num_fields, sizeof(MYSQL_BIND));

        for ( uint32_t i = 0; i < num_fields; ++i ) {
            MYSQL_FIELD *field = mysql_fetch_field(mysql_result);
            fields[i] = field;

            bind[i].buffer_type = field->type;
            bind[i].buffer_length = field->length;
            bind[i].buffer = xmalloc(field->length);
        }
    }

    mysql_stmt_bind_result(stmt->stmt, bind);
    mysql_stmt_store_result(stmt->stmt);

    char buf[1000];
    sprintf(buf, "[");
    bool first = true;
    while ( !mysql_stmt_fetch(stmt->stmt) ) {
        sprintf(buf, "%s%s{", buf, (first) ? "": ",");
        for ( uint32_t i = 0; i < num_fields; ++i ) {
            mysql_stmt_fetch_column(stmt->stmt, bind, i, 0);
            sprintf(buf, "%s%s\"%s\": \"%s\"", buf, (i == 0) ? "" : ", ", fields[i]->name, db_bind_to_string(&bind[i]));
        }
        sprintf(buf, "%s}", buf);
        first = false;
    }
    sprintf(buf, "%s]", buf);

    size_t buf_len = string_len(buf);

    result.data = (char *)xmalloc(buf_len);
    string_copy(buf, result.data, buf_len);

    return result;
}
