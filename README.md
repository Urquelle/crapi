C Rest Api
==========

small and insecure rest api server written in c.

```c++
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
    db_param_set(&params, 0, MYSQL_TYPE_LONG, &(int){ http_param_int(req, "id") });

    Db_Stmt *stmt = db_stmt_get(api->db, "orga/structs");
    Db_Result result = db_stmt_exec(stmt, &params, temp_arena);

    res->content = db_json(&result, temp_arena);
    res->mime_type = MIME_APPLICATION_JSON;
}

int main(int argc, char **argv) {
    temp_arena = mem_arena_new(1024);
    perm_arena = mem_arena_new(1024);

    Rest_Api *api = &(Rest_Api){0};
    api->db = MEM_STRUCT(perm_arena, Db);
    db_init(api->db, NULL, "root", DB_PASSWD, "grimoire");

    db_stmt_create(api->db, "orga", "SELECT * FROM orga WHERE id = ?", temp_arena);
    rest_get(api, "/orga/:id/structures", get_orga_structs);
    rest_start(api, "127.0.0.1", 3000);

    return 0;
}
```
