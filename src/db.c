#include "mysql.h"

enum {
    DB_INT,
    DB_LONG,
    DB_VARCHAR,
};

typedef struct {
    void *handle;
    Map stmts;
} Db;

typedef struct {
    char *name;
    size_t size;
    uint32_t type;
    void *data;
} Db_Field;

typedef struct {
    Db_Field **fields;
    size_t num_fields;
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
    Db_Row **rows;
    size_t num_rows;
} Db_Result;

Db_Field *
db_field(char *name, size_t size, uint32_t type, void *data, Mem_Arena *arena) {
    Db_Field *result = MEM_STRUCT(arena, Db_Field);

    result->name = name;
    result->size = size;
    result->type = type;

    result->data = ( type == MYSQL_TYPE_VAR_STRING || type == MYSQL_TYPE_DATETIME )
        ? MEM_SIZE(arena, size+1)
        : MEM_SIZE(arena, size);

    memcpy(result->data, data, size);

    if ( type == MYSQL_TYPE_VAR_STRING || type == MYSQL_TYPE_DATETIME ) {
        ((char *)result->data)[size] = 0;
    }

    return result;
}

Db_Stmt *
db_stmt_new(void *stmt, Mem_Arena *arena) {
    Db_Stmt *result = MEM_STRUCT(arena, Db_Stmt);

    result->stmt = stmt;

    return result;
}

unsigned long
db_len(uint32_t type, unsigned long len, void *data) {
    unsigned long result = len;

    switch ( type ) {
        case MYSQL_TYPE_DATETIME:
        case MYSQL_TYPE_VAR_STRING: {
            result = string_len((char *)data);
        } break;
    }

    return result;
}

void
db_init(Db *db, char *host, char *user, char *passwd, char *db_name) {
    MYSQL *mysql = mysql_init(NULL);
    if ( !mysql ) {
        return;
    }

    mysql = mysql_real_connect(mysql, host, user, passwd, db_name, 0, NULL, 0);
    if ( !mysql ) {
        return;
    }

    db->handle = mysql;

    db->stmts.cap = 0;
    db->stmts.vals = 0;
    db->stmts.keys = 0;
    db->stmts.len = 0;
}

Db_Result
db_query(Db *db, char *query, Mem_Arena *arena) {
    Db_Result result = {0};

    if ( mysql_real_query(db->handle, query, string_len(query)) ) {
        return result;
    }

    MYSQL_RES *mysql_result = mysql_store_result(db->handle);
    if ( mysql_result ) {
        uint64_t num_fields = mysql_num_fields(mysql_result);
        uint64_t num_rows = mysql_num_rows(mysql_result);

        result.rows = (Db_Row **)MEM_SIZE(arena, sizeof(Db_Row *)*num_rows);
        result.num_rows = num_rows;

        MYSQL_ROW mysql_row;
        uint64_t row_idx = 0;
        while ( (mysql_row = mysql_fetch_row(mysql_result)) ) {
            Db_Row *row = MEM_STRUCT(arena, Db_Row);

            row->fields = (Db_Field **)MEM_SIZE(arena, sizeof(Db_Field *)*num_fields);
            row->num_fields = num_fields;

            for ( uint32_t i = 0; i < num_fields; ++i ) {
                MYSQL_FIELD field = mysql_result->fields[i];
                uint64_t len = db_len(field.type, field.length, mysql_row[i]);
                row->fields[i] = db_field(field.name, len, field.type, mysql_row[i], arena);
            }

            result.rows[row_idx++] = row;
        }
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
db_params(size_t num_elems, Mem_Arena *arena) {
    Db_Param result = {0};

    result.elems = (MYSQL_BIND *)MEM_SIZE(arena, sizeof(MYSQL_BIND)*num_elems);
    ZERO_ARRAY(num_elems, result.elems);
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
db_stmt_create(Db *db, char *key, char *query, Mem_Arena *arena) {
    MYSQL_STMT *stmt = mysql_stmt_init(db->handle);

    if ( mysql_stmt_prepare(stmt, query, string_len(query)) ) {
        return false;
    }

    Db_Stmt *result = db_stmt_new(stmt, arena);
    map_push(&db->stmts, str_intern(key), result);

    return true;
}

Db_Stmt *
db_stmt_get(Db *db, char *key) {
    Db_Stmt *result = map_get(&db->stmts, str_intern(key));

    return result;
}

char *
db_bind_to_string(MYSQL_BIND *bind, Mem_Arena *arena) {
    char *result = "";

    switch ( bind->buffer_type ) {
        case MYSQL_TYPE_LONG: {
            result = strf(arena, "%d", *(int *)bind->buffer);
        } break;

        case MYSQL_TYPE_VAR_STRING: {
            result = strf(arena, "%s", (char *)bind->buffer);
        } break;

        case MYSQL_TYPE_DATETIME: {
            MYSQL_TIME *t = (MYSQL_TIME *)bind->buffer;
            result = strf(arena, "%d.%d.%d %d:%d:%d", t->year, t->month, t->day,
                    t->hour, t->minute, t->second);
        } break;

        default: {
            assert(0);
        } break;
    }

    return result;
}

Db_Result
db_stmt_exec(Db_Stmt *stmt, Db_Param *params, Mem_Arena *arena) {
    Db_Result result = {0};

    mysql_stmt_bind_param(stmt->stmt, params->elems);

    if ( mysql_stmt_execute(stmt->stmt) ) {
        printf("Db Fehler: %s", mysql_stmt_error(stmt->stmt));
        return result;
    }

    MYSQL_BIND *bind = 0;
    MYSQL_RES *mysql_result = mysql_stmt_result_metadata(stmt->stmt);

    uint64_t num_fields = mysql_num_fields(mysql_result);
    MYSQL_FIELD **fields = (MYSQL_FIELD **)MEM_SIZE(arena, sizeof(MYSQL_FIELD *)*num_fields);
    unsigned long len = 0;

    if ( mysql_result ) {
        bind = (MYSQL_BIND *)MEM_SIZE(arena, num_fields*sizeof(MYSQL_BIND));
        ZERO_ARRAY(num_fields, bind);

        for ( uint32_t i = 0; i < num_fields; ++i ) {
            MYSQL_FIELD *field = mysql_fetch_field(mysql_result);
            fields[i] = field;

            bind[i].buffer_type = field->type;
            bind[i].buffer_length = field->length;
            bind[i].buffer = MEM_SIZE(arena, field->length);
            bind[i].length = &len;
        }
    }

    mysql_stmt_bind_result(stmt->stmt, bind);
    mysql_stmt_store_result(stmt->stmt);

    uint64_t num_rows = mysql_stmt_num_rows(stmt->stmt);
    result.rows = (Db_Row **)MEM_SIZE(arena, sizeof(Db_Row *)*num_rows);
    result.num_rows = num_rows;

    uint64_t row_idx = 0;
    while ( !mysql_stmt_fetch(stmt->stmt) ) {
        Db_Row *row = MEM_STRUCT(arena, Db_Row);
        row->fields = (Db_Field **)MEM_SIZE(arena, sizeof(Db_Field *)*num_fields);
        row->num_fields = num_fields;

        for ( uint32_t i = 0; i < num_fields; ++i ) {
            MYSQL_FIELD *field = fields[i];

            mysql_stmt_fetch_column(stmt->stmt, bind, i, 0);
            len = db_len(field->type, len, bind[i].buffer);
            row->fields[i] = db_field(field->name, len, field->type,
                    db_bind_to_string(&bind[i], arena), arena);
        }

        result.rows[row_idx] = row;
    }

    return result;
}

char *
db_json(Db_Result *res, Mem_Arena *arena) {
    char *result = "[";
    for ( int i = 0; i < res->num_rows; ++i ) {
        result = strf(arena, "%s%s{", result, (i == 0) ? "" : ", ");
        Db_Row *row = res->rows[i];

        for ( int j = 0; j < row->num_fields; ++j ) {
            Db_Field *field = row->fields[j];
            result = strf(arena, "%s%s\"%s\": \"%s\"", result, (j == 0) ? "" : ", ", field->name, (char *)field->data);
        }

        result = strf(arena, "%s}", result);
    }
    result = strf(arena, "%s]", result);

    return result;
}
