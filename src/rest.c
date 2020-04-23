typedef struct Rest_Api Rest_Api;
#define REST_CALLBACK(name) void name(Rest_Api *api, Http_Request *req, Http_Response *res)
typedef REST_CALLBACK(Rest_Callback);

typedef struct {
    uint32_t method;
    char *pattern;
    Rest_Callback *func;
} Rest_Route;

Rest_Route *
rest_route(uint32_t method, char *pattern, Rest_Callback func, Mem_Arena *arena) {
    Rest_Route *result = MEM_STRUCT(arena, Rest_Route);

    result->method = method;
    result->pattern = pattern;
    result->func = func;

    return result;
}

enum { REST_MAX_ROUTES = 50 };
struct Rest_Api {
    Server *server;
    Mem_Arena *temp_arena;
    Mem_Arena *perm_arena;
    Db *db;

    Rest_Route *routes[REST_MAX_ROUTES];
    size_t num_routes;

    Rest_Route *uses[REST_MAX_ROUTES];
    size_t num_uses;
};

void
rest_init(Rest_Api *api, Mem_Arena *perm_arena, Mem_Arena *temp_arena, Db *db) {
    api->perm_arena = perm_arena;
    api->temp_arena = temp_arena;
    api->db = db;
}

bool rest_pattern_match(Rest_Route *route, char *url) {
    bool result = true;

    char *pattern = route->pattern;
    for ( ;; ) {
        if ( pattern[0] == 0 || url[0] == 0 ) {
            break;
        }

        if ( pattern[0] == ':' ) {
            pattern++;

            while ( pattern[0] && pattern[0] != '/' && pattern[0] != '-' ) {
                pattern++;
            }

            while ( url[0] && url[0] != '/' && url[0] != '-' ) {
                url++;
            }
        } else if ( pattern[0] != url[0] ) {
            result = false;
            break;
        }

        if ( pattern[0] ) {
            pattern++;
        }

        if ( url[0] ) {
            url++;
        }
    }

    if ( pattern[0] != url[0] ) {
        result = false;
    }

    return result;
}

bool
rest_pattern_startswith(Rest_Route *route, char *url) {
    bool result = false;

    char *pattern = route->pattern;
    while ( *pattern && *url ) {
        if ( *pattern != *url ) {
            result = false;
            break;
        }

        pattern++;
        url++;
    }

    if ( *pattern != '\0' ) {
        result = false;
    }

    return result;
}

void rest_parse_params(Rest_Route *route, Http_Request *req, Mem_Arena *arena) {
    char *pattern = route->pattern;
    char *url = req->url;

    for ( ;; ) {
        if ( pattern[0] == 0 || url[0] == 0 ) {
            break;
        }

        if ( pattern[0] == ':' ) {
            pattern++;

            char *key_ptr = pattern;
            while ( pattern[0] && pattern[0] != '/' && pattern[0] != '-' ) {
                pattern++;
            }

            size_t key_len = pattern - key_ptr;

            char *val_ptr = url;
            while ( url[0] && url[0] != '/' && url[0] != '-' ) {
                url++;
            }

            size_t val_len = url - val_ptr;

            char *key = (char *)MEM_SIZE(arena, key_len + 1);
            string_copy(key_ptr, key, key_len);

            char *val = (char *)MEM_SIZE(arena, val_len + 1);
            string_copy(val_ptr, val, val_len);

            map_push(&req->params, str_intern(key), val);
        }

        if ( pattern[0] ) {
            pattern++;
        }

        if ( url[0] ) {
            url++;
        }
    }
}

void rest_use(Rest_Api *api, char *pattern, Rest_Callback *func) {
    assert(api->num_routes < REST_MAX_ROUTES);

    api->uses[api->num_uses++] = rest_route(Request_Any, pattern, func, api->perm_arena);
}

void rest_any(Rest_Api *api, char *pattern, Rest_Callback *func) {
    assert(api->num_routes < REST_MAX_ROUTES);

    api->routes[api->num_routes++] = rest_route(Request_Any, pattern, func, api->perm_arena);
}

void rest_get(Rest_Api *api, char *pattern, Rest_Callback *func) {
    assert(api->num_routes < REST_MAX_ROUTES);

    api->routes[api->num_routes++] = rest_route(Request_Get, pattern, func, api->perm_arena);
}

void rest_post(Rest_Api *api, char *pattern, Rest_Callback *func) {
    assert(api->num_routes < REST_MAX_ROUTES);

    api->routes[api->num_routes++] = rest_route(Request_Post, pattern, func, api->perm_arena);
}

void rest_start(Rest_Api *api, char *ip, uint16_t port) {
    api->server = server_create(SERVER_TCP);

    Server_Response result = server_start(api->server, ip, port);

    if ( !result.success ) {
        assert(!"server konnte nicht gestartet werden");
    }

    Http_Request *request = &(Http_Request){0};
    Http_Response *response = &(Http_Response){0};

    while ( api->server->is_running ) {
        http_request_accept( request, api->server->socket );

        for ( int i = 0; i < api->num_uses; ++i ) {
            Rest_Route *use_route = api->uses[i];

            if ( rest_pattern_startswith(use_route, request->url) ) {
                rest_parse_params(use_route, request, api->perm_arena);
                use_route->func(api, request, response);
            }
        }

        Rest_Route *route = NULL;
        for ( uint32_t i = 0; i < api->num_routes; ++i ) {
            Rest_Route *r = api->routes[i];

            if ( r->method != request->method && r->method != Request_Any ) {
                continue;
            }

            if ( rest_pattern_match(r, request->url) ) {
                route = r;
                break;
            }
        }

        if ( route ) {
            rest_parse_params(route, request, api->perm_arena);
            route->func(api, request, response);
        } else {
            response->content = "404 - Seite nicht gefunden";
        }

        http_server_send_response( request, response, response->content, api->temp_arena );
        mem_reset(api->temp_arena);
    }
}
