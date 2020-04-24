#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#include "common.c"
#include "mem.c"
#include "util.c"
#include "db.c"
#include "server.c"
#include "rest.c"

#include ".credentials.ini"

static Mem_Arena *temp_arena;
static Mem_Arena *perm_arena;

REST_CALLBACK(get_orga_structs) {
    assert(api->db);

    Db_Param params = db_params(1, temp_arena);
    db_param_set(&params, 0, DB_INT, &(int){ http_param_int(req, "id") });

    Db_Stmt *stmt = db_stmt_get(api->db, "orga/structs");
    Db_Result result = db_stmt_exec(stmt, &params, temp_arena);

    res->content = db_json_array(&result, temp_arena);
    res->mime_type = MIME_APPLICATION_JSON;
}

REST_CALLBACK(get_orgas) {
    assert(api->db);

    char *query = "SELECT * FROM orga";
    Db_Result result = db_query(api->db, query, temp_arena);

    res->content = db_json_array(&result, temp_arena);
    res->mime_type = MIME_APPLICATION_JSON;
}

REST_CALLBACK(get_orga) {
    assert(api->db);

    Db_Param params = db_params(1, temp_arena);
    db_param_set(&params, 0, DB_INT, &(int){ http_param_int(req, "id") });

    Db_Stmt *stmt = db_stmt_get(api->db, "orga");
    Db_Result result = db_stmt_exec(stmt, &params, temp_arena);

    res->content = db_json_obj(result.rows[0], temp_arena);
    res->mime_type = MIME_APPLICATION_JSON;
}

REST_CALLBACK(post_authenticate) {
    assert(api->db);

    Db_Param params = db_params(2, temp_arena);
    db_param_set(&params, 0, DB_STRING, http_param(req, "username"));
    db_param_set(&params, 1, DB_STRING, http_param(req, "password"));

    Db_Stmt *stmt = db_stmt_get(api->db, "auth/authenticate");
    Db_Result result = db_stmt_exec(stmt, &params, temp_arena);

    if ( result.num_rows > 0 ) {
        res->content = "erfolgreich angemeldet";
    } else {
        res->content = "benutzer konnte nicht angemeldet werden";
    }
}

int main(int argc, char **argv) {
    if ( argc < 5 ) {
        fprintf(stderr, "-port und -ip müssen als parameter übergeben werden.\n");
        exit(1);
    }

    char *ip = NULL;
    uint16_t port = 0;
    for ( int i = 1; i < argc; ++i ) {
        if ( is_equal(argv[i], "-ip", 3) ) {
            ip = argv[++i];
            continue;
        } else if ( is_equal(argv[i], "-port", 5) ) {
            char *p = argv[++i];
            while (*p) {
                port *= 10;
                port += *p - '0';
                p++;
            }
            continue;
        }
    }

    temp_arena = mem_arena_new();
    perm_arena = mem_arena_new();

    Rest_Api *api = &(Rest_Api){0};
    rest_init(api, perm_arena, temp_arena, MEM_STRUCT(perm_arena, Db));
    db_init(api->db, NULL, DB_USER, DB_PASSWD, DB_NAME);

    db_stmt_create(api->db, "orga", "SELECT * FROM orga WHERE id = ?", perm_arena);
    db_stmt_create(api->db, "orga/structs", "SELECT id, name FROM structure WHERE orga_id = ?", perm_arena);
    db_stmt_create(api->db, "auth/authenticate", "SELECT * FROM user WHERE (username = ? OR email = ?) AND password = ?", perm_arena);

    rest_get(api, "/orgas", get_orgas);
    rest_get(api, "/orga/:id/structures", get_orga_structs);
    rest_get(api, "/orga/:id", get_orga);
    rest_post(api, "/auth/authenticate", post_authenticate);

    rest_start(api, ip, port);

    return 0;
}
