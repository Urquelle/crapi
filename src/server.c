enum Http_Response_Code {
    /* Information */
    Response_Continue                        = 100,
    Response_Switching_Protocols             = 101,
    Response_Processing                      = 102,

    /* Success */
    Response_Ok                              = 200,
    Response_Created                         = 201,
    Response_Accepted                        = 202,
    Response_Non_Authoritative_Information   = 203,
    Response_No_Content                      = 204,
    Response_Reset_Content                   = 205,
    Response_Partial_Content                 = 206,
    Response_Multi_Status                    = 207,
    Response_Already_Reported                = 208,
    Response_Im_Used                         = 226,

    /* Redirection */
    Response_Multiple_Choices                = 300,
    Response_Moved_Permanently               = 301,
    Response_Found                           = 302,
    Response_See_Other                       = 303,
    Response_Not_Modified                    = 304,
    Response_Use_Proxy                       = 305,
    Response_Switch_Proxy                    = 306,
    Response_Temporary_Redirect              = 307,
    Response_Permanent_Redirect              = 308,
    Response_Resume_Incomplete               = 308,

    /* Client Error */
    Response_Bad_Request                     = 400,
    Response_Unauthorized                    = 401,
    Response_Payment_Required                = 402,
    Response_Forbidden                       = 403,
    Response_Not_Found                       = 404,
    Response_Method_Not_Allowed              = 405,
    Response_Not_Acceptable                  = 406,
    Response_Proxy_Authentication_Required   = 407,
    Response_Request_Timeout                 = 408,
    Response_Conflict                        = 409,
    Response_Gone                            = 410,
    Response_Length_Required                 = 411,
    Response_Precondition_Failed             = 412,
    Response_Payload_Too_Large               = 413,
    Response_Request_Uri_Too_Long            = 414,
    Response_Unsupported_Media_Type          = 415,
    Response_Requested_Range_Not_Satisfiable = 416,
    Response_Expectation_Failed              = 417,
    Response_Im_A_Teapot                     = 418,
};

enum {
    MAX_HTTP_VERSION_SIZE = 15,
    MAX_URL_LENGTH = 2000,
    MAX_HTTP_HOST_SIZE = 20,
    MAX_HTTP_USER_AGENT_SIZE = 500,
    MAX_HTTP_ACCEPT_SIZE = 300,
    MAX_HTTP_ACCEPT_ENCODING_SIZE = 200,
    MAX_HTTP_ACCEPT_LANGUAGE_SIZE = 200,
};

enum Server_Kind {
    SERVER_TCP,
    SERVER_UDP,
};

typedef struct {
    SOCKET      socket;
    uint32_t    kind;
    char*       ip;
    uint32_t    port;
    bool        is_running;
} Server;

typedef struct {
    SOCKET   socket;
    char*    ip;
    uint16_t port;
} Client;

typedef struct {
    bool success;
    char *message;
} Server_Response;

enum Mime_Type {
    MIME_UNKNOWN,
    MIME_APPLICATION_JSON,
    MIME_TEXT_HTML,
    MIME_TEXT_CSS,
    MIME_TEXT_JAVASCRIPT,
};
typedef struct {
    char *header;
    char* content;
    uint32_t code;
    uint32_t mime_type;
} Http_Response;

enum Http_Method {
    Request_Unknown,
    Request_Get,
    Request_Head,
    Request_Post,
    Request_Put,
    Request_Delete,
    Request_Connect,
    Request_Options,
    Request_Trace,
    Request_Patch,
    Request_Any,
};

typedef struct {
    char     http_version[MAX_HTTP_VERSION_SIZE];
    char     host[MAX_HTTP_HOST_SIZE];
    char     user_agent[MAX_HTTP_USER_AGENT_SIZE];
    char     accept[MAX_HTTP_ACCEPT_SIZE];
    char     accept_encoding[MAX_HTTP_ACCEPT_ENCODING_SIZE];
    char     accept_language[MAX_HTTP_ACCEPT_LANGUAGE_SIZE];
} Http_Header;

typedef struct {
    uint32_t method;
    SOCKET   client_socket;
    char     url[MAX_URL_LENGTH];
    Http_Header header;

    Map params;
} Http_Request;

void
server_init(Server *server, uint32_t kind) {
    server->kind = kind;
}

Server_Response
server_start(Server *server, char *ip, uint16_t port) {
    WSADATA wsa_data = {0};
    uint32_t result = 0;
    struct sockaddr_in service = {0};

    server->ip = ip;
    server->port = port;

    result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != NO_ERROR) {
        return (Server_Response){ false, "" };
    }

    if ( server->kind == SERVER_TCP ) {
        server->socket = socket(AF_INET, SOCK_STREAM, 0);
    } else if ( server->kind == SERVER_UDP ) {
        server->socket = socket(AF_INET, SOCK_DGRAM, 0);
    }

    if (server->socket == INVALID_SOCKET) {
        WSACleanup();

        return (Server_Response){ false, "fehler" };
    }

    service.sin_family      = AF_INET;
    service.sin_addr.s_addr = inet_addr(server->ip);
    service.sin_port        = htons((u_short)server->port);

    result = bind(server->socket, (SOCKADDR *) &service, sizeof (service));
    if (result == SOCKET_ERROR) {
        closesocket(server->socket);
        WSACleanup();

        return (Server_Response){ false, "" };
    }

    if ( server->kind == SERVER_TCP ) {
        result = listen(server->socket, SOMAXCONN);
        if ( result == SOCKET_ERROR) {
            return (Server_Response){ false, "" };
        }
    }

    server->is_running = true;

    return (Server_Response){ true, "" };
}

uint32_t
wait_for_request( Server *server, Client *client, char* buffer,
        uint32_t buffer_length, Mem_Arena *arena )
{
    if ( server->kind == SERVER_TCP ) {
        client->socket = INVALID_SOCKET;
        client->socket = accept(server->socket, NULL, NULL);

        if (client->socket == INVALID_SOCKET) {
            closesocket(server->socket);
            WSACleanup();
        }

        uint32_t byte_count;
        byte_count = recv(client->socket, buffer, buffer_length, 0);

        if (byte_count > 0) {
            uint32_t message_length = byte_count / sizeof(char);
            buffer[message_length] = 0;

            return message_length;
        } else if (byte_count == 0) {
            //
        } else {
            closesocket(client->socket);
            WSACleanup();
        }
    } else if ( server->kind == SERVER_UDP ) {
        struct sockaddr_in sender_addr = {0};
        socklen_t addr_size = sizeof(sender_addr);
        uint32_t byte_count;

        while ( 1 ) {
            byte_count = recvfrom(server->socket, buffer, 1024, 0, (SOCKADDR *)&sender_addr, &addr_size);

            if ( byte_count > 0 ) {
                uint32_t message_length = byte_count / sizeof(char);
                buffer[message_length] = 0;

                client->port = sender_addr.sin_port;
                client->ip = strf(arena, "%s", inet_ntoa(*(struct in_addr *) &sender_addr.sin_addr));

                return message_length;
            }
        }
    }

    return 0;
}

bool
server_send_tcp_response( SOCKET socket, char* message, uint32_t message_length ) {
    uint32_t status = send(socket, message, message_length, 0);

    if (status == SOCKET_ERROR) {
        closesocket(socket);
        WSACleanup();

        return false;
    }

    closesocket(socket);

    return true;
}

bool
server_send_udp_response( Server* server, Client* client, char* message, uint32_t message_length ) {
    struct sockaddr_in recv_addr;
    uint32_t addr_size = sizeof(recv_addr);
    uint32_t status;

    recv_addr.sin_family      = AF_INET;
    recv_addr.sin_port        = htons(client->port);
    recv_addr.sin_addr.s_addr = inet_addr(client->ip);

    client->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    status = sendto(client->socket, message, message_length, 0, (SOCKADDR *) &recv_addr, addr_size);

    if ( status == SOCKET_ERROR ) {
        return false;
    }

    return true;
}

char *mime_type[] = {
    [MIME_TEXT_HTML]        = "text/html",
    [MIME_TEXT_CSS]         = "text/css",
    [MIME_TEXT_JAVASCRIPT]  = "text/javascript",
    [MIME_APPLICATION_JSON] = "application/json",
};

void
parse_key_value( char **c, char *key, char *value ) {
    uint32_t index = 0;

    while ( **c != ':' ) {
        key[index++] = **c;
        (*c)++;
    }
    key[index] = '\0';

    // skip colon
    (*c)++;

    // skip blank
    (*c)++;

    index = 0;
    while ( **c != '\r' && **c != '\n' ) {
        value[index++] = **c;
        (*c)++;
    }
    value[index] = 0;
    (*c)++;
}

void
parse_post_data_key_value( char **c, char *key, char *value ) {
    uint32_t index = 0;

    while ( **c != '=' ) {
        key[index++] = **c;
        (*c)++;
    }
    key[index] = '\0';

    // skip "="
    (*c)++;

    index = 0;
    while ( **c != '\0' && **c != '&' && **c != '\r' && **c != '\n' ) {
        value[index++] = **c;
        (*c)++;
    }
    value[index] = 0;
    (*c)++;
}

void
http_request_parse( Http_Request *request, char *content, Mem_Arena *arena ) {
    char *c = content;
    char method[10] = {0};

    // parse method
    uint32_t index = 0;
    while ( *c != ' ' ) {
        method[index++] = *c++;
    }
    method[index] = '\0';

    if ( strncmp( method, "GET", 3 ) == 0 ) {
        request->method = Request_Get;
    } else if ( is_equal( method, "POST", 4 ) ) {
        request->method = Request_Post;
    } else if ( is_equal( method, "HEAD", 4 ) ) {
        request->method = Request_Head;
    } else if ( is_equal( method, "PUT", 3 ) ) {
        request->method = Request_Put;
    } else if ( is_equal( method, "DELETE", 6 ) ) {
        request->method = Request_Delete;
    } else if ( is_equal( method, "CONNECT", 7 ) ) {
        request->method = Request_Connect;
    } else if ( is_equal( method, "OPTIONS", 7 ) ) {
        request->method = Request_Options;
    } else if ( is_equal( method, "TRACE", 5 ) ) {
        request->method = Request_Trace;
    } else if ( is_equal( method, "PATCH", 5 ) ) {
        request->method = Request_Patch;
    } else {
        request->method = Request_Unknown;
    }

    // parse requested resource
    index = 0;
    char resource_name[256] = {0};
    c++;
    while ( *c != ' ' ) {
        resource_name[index++] = *c++;
    }
    resource_name[index] = '\0';
    string_copy(resource_name, request->url, string_len(resource_name));

    // parse http version
    index = 0;
    char http_version[10] = {0};
    c++;
    while ( *c != '\r' && *c != '\n' ) {
        http_version[index++] = *c++;
    }
    http_version[index] = '\0';
    string_copy(http_version, request->header.http_version, string_len(http_version));

    c++;
    if ( c[0] == '\n' ) c++;

    // parse key/value
    char key[100] = {0};
    char value[1000] = {0};
    bool end_of_stream = false;
    bool in_param_list = false;

    while ( *c ) {
        int newline_count = 0;
        while ( *c == ' ' || *c == '\r' || *c == '\n' || *c == '\0' ) {
            if ( *c == '\n' ) {
                newline_count++;
                if ( newline_count == 2 ) in_param_list = true;
            }

            if ( *c == '\0' ) {
                end_of_stream = true;
                break;
            } else {
                c++;
            }
        }

        if ( end_of_stream ) break;

        if ( !in_param_list ) {
            parse_key_value(&c, key, value);

            if ( is_equal( "Host", key, 4 ) ) {
                string_copy( value, request->header.host, string_len(value) );
            } else if ( is_equal( "User-Agent", key, 10 ) ) {
                string_copy( value, request->header.user_agent, string_len(value) );
            } else if ( is_equal( "Accept-Encoding", key, 15 ) ) {
                string_copy( value, request->header.accept_encoding, string_len(value) );
            } else if ( is_equal( "Accept-Language", key, 15 ) ) {
                string_copy( value, request->header.accept_language, string_len(value) );
            } else if ( is_equal( "Accept", key, 6 ) ) {
                string_copy( value, request->header.accept, string_len(value) );
            }
        } else {
            parse_post_data_key_value( &c, key, value );

            Param_Pair *param = MEM_STRUCT(arena, Param_Pair);
            param->type = Param_Type_Char;

            size_t key_length = strlen( key );
            param->key = MEM_SIZE(arena, sizeof( char ) * key_length + 1);
            string_copy( key, param->key, string_len(key) );

            size_t value_length = strlen( value );
            param->char_value = MEM_SIZE(arena, sizeof( char ) * value_length + 1);
            string_copy( value, param->char_value, string_len(value) );

            map_push( &request->params, str_intern(key, arena), param, arena );
        }
    }
}

void
http_request_accept( Http_Request *request, SOCKET listen_socket,
        Mem_Arena *arena )
{
    request->client_socket = INVALID_SOCKET;
    request->client_socket = accept(listen_socket, NULL, NULL);

    if (request->client_socket == INVALID_SOCKET) {
        closesocket(listen_socket);
        WSACleanup();
    }

#define DEFAULT_BUFLEN 1024
    char message[DEFAULT_BUFLEN];
    uint32_t result;
    char *content = "";

    do {
        result = recv(request->client_socket, message, DEFAULT_BUFLEN, 0);

        if (result > 0) {
            uint32_t message_length = result / sizeof(char);
            message[message_length] = 0;
            content = strf(arena, "%s%s", content, message);
        } else if (result == 0) {
            break;
        } else {
            closesocket(request->client_socket);
            WSACleanup();

            break;
        }
    } while ( result > 0 && result == DEFAULT_BUFLEN );

    http_request_parse( request, content, arena );
}

bool
http_server_send_response(Http_Request *request, Http_Response *response,
        char *content, Mem_Arena *arena)
{
    size_t content_len = string_len( content );

    response->header = strf(arena,
            "%s %d OK\nContent-length: %zd\nAccess-Control-Allow-Origin: *\nContent-type: %s\n\n",
            request->header.http_version,
            response->code,
            content_len,
            mime_type[response->mime_type]);

    size_t response_len = string_len(response->header) + content_len + 1;
    char *response_content = (char *)MEM_SIZE(arena, response_len);
    string_concat(response->header, content, response_content);

    bool result = server_send_tcp_response(request->client_socket, response_content, (int)response_len);

    return result;
}

char *
http_param(Http_Request *req, char *key, Mem_Arena *arena) {
    char *result = (char *)map_get(&req->params, str_intern(key, arena));

    return result;
}

int
http_param_int(Http_Request *req, char *key, Mem_Arena *arena) {
    char *value = http_param(req, key, arena);
    int result = 0;

    if ( value ) {
        result = string_to_int(value);
    }

    return result;
}

