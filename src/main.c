#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "util.c"
#include "db.c"
#include "server.c"
#include "rest.c"

#include ".credentials.ini"

REST_CALLBACK(get_structures_for_orga) {
    assert(api->db);

    char *id = http_param(req, "id");
    assert(id);
#if 1
    char query[250];
    sprintf(query, "SELECT * FROM structure WHERE orga_id = %s\0", id);
    Db_Result result = db_query(api->db, query, string_len(query));
#else
    Db_Stmt *stmt = db_stmt_get(api->db, "structs");
    db_stmt_param_set(stmt, 0, MYSQL_TYPE_LONG, id);
    db_stmt_exec(stmt);
#endif

    res->content = result.data;
}

REST_CALLBACK(get_orgas) {
    assert(api->db);

    char *query = "SELECT * FROM orga";
    Db_Result result = db_query(api->db, query, string_len(query));

    res->content = result.data;
}

REST_CALLBACK(get_orga) {
    assert(api->db);

    char query[150];
    char *id = http_param(req, "id");
    assert(id);
    sprintf(query, "SELECT * FROM orga WHERE id = %s\0", id);
    Db_Result result = db_query(api->db, query, string_len(query));

    res->mime_type = MT_APPLICATION_JSON;
    res->content = result.data;
}

REST_CALLBACK(post_authenticate) {
    assert(api->db);

    char *username = http_param(req, "username");
    char *password = http_param(req, "password");

    char query[250];
    sprintf(query, "SELECT * FROM user WHERE (username = '%s' OR email = '%s') AND password = '%s'",
            username, username, password);
}

int main(int argc, char **argv) {
    Db *db = db_init(NULL, "root", DB_PASSWD, "grimoire");

    db_stmt_create(db, "structs", "SELECT * FROM structure WHERE orga_id = ?", 1);
    db_stmt_create(db, "authenticate", "SELECT * FROM user WHERE (username = ? OR email = ?) AND password = ?", 3);

    Rest_Api *api = &(Rest_Api){0};
    api->db = db;

    rest_get(api, "/orgas", get_orgas);
    rest_get(api, "/orga/:id/structures", get_structures_for_orga);
    rest_get(api, "/orga/:id", get_orga);
    rest_post(api, "/auth/authenticate", post_authenticate);

    rest_start(api, "127.0.0.1", 3300);

    return 0;
}
