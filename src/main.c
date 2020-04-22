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

REST_CALLBACK(get_orga_structs) {
    assert(api->db);

    Db_Param params = db_params(1);
    db_param_set(&params, 0, MYSQL_TYPE_LONG, &(int){ http_param_int(req, "id") });

    Db_Stmt *stmt = db_stmt_get(api->db, "orga/structs");
    Db_Result result = db_stmt_exec(stmt, &params);

    res->content = result.data;
    res->mime_type = MIME_APPLICATION_JSON;
}

REST_CALLBACK(get_orgas) {
    assert(api->db);

    char *query = "SELECT * FROM orga";
    Db_Result result = db_query(api->db, query);

    res->content = result.data;
    res->mime_type = MIME_APPLICATION_JSON;
}

REST_CALLBACK(get_orga) {
    assert(api->db);

    Db_Param params = db_params(1);
    db_param_set(&params, 0, MYSQL_TYPE_LONG, &(int){ http_param_int(req, "id") });

    Db_Stmt *stmt = db_stmt_get(api->db, "orga");
    Db_Result result = db_stmt_exec(stmt, &params);

    res->content = result.data;
    res->mime_type = MIME_APPLICATION_JSON;
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
    Rest_Api *api = &(Rest_Api){0};
    api->db = db_init(NULL, "root", DB_PASSWD, "grimoire");

    db_stmt_create(api->db, "orga", "SELECT * FROM orga WHERE id = ?");
    db_stmt_create(api->db, "orga/structs", "SELECT id, name FROM structure WHERE orga_id = ?");
    db_stmt_create(api->db, "auth/authenticate", "SELECT * FROM user WHERE (username = ? OR email = ?) AND password = ?");

    rest_get(api, "/orgas", get_orgas);
    rest_get(api, "/orga/:id/structures", get_orga_structs);
    rest_get(api, "/orga/:id", get_orga);
    rest_post(api, "/auth/authenticate", post_authenticate);

    rest_start(api, "127.0.0.1", 3300);

    return 0;
}
