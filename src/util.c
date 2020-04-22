typedef struct {
    size_t cap;
    size_t len;

    void **keys;
    void **vals;
} Map;

enum Param_Type {
    Param_Type_Char,
    Param_Type_Utf,
    Param_Type_Bool,
    Param_Type_Int,
    Param_Type_Custom,
    Param_Type_Array
};

typedef struct {
    uint32_t type;
    char*    key;
    char*    type_name;

    union {
        char*    char_value;
        bool     bool_value;
        uint32_t int32_value;
        void*    custom_value;
    };
} Param_Pair;

void *
xmalloc(size_t size) {
    void *result = malloc(size);

    if ( !result ) {
        assert(!"speicher konnte nicht reserviert werden");
    }

    return result;
}

void *
xcalloc(size_t size, size_t num) {
    void *result = calloc(size, num);

    if ( !result ) {
        assert(!"speicher konnte nicht reserviert werden");
    }

    return result;
}

static uint64_t
util_bytes_hash(void *ptr, size_t len) {
    uint64_t x = 0xcbf29ce484222325;
    char *buf = (char *)ptr;

    for (size_t i = 0; i < len; i++) {
        x ^= buf[i];
        x *= 0x100000001b3;
        x ^= x >> 32;
    }

    return x;
}

uint64_t util_ptr_hash(void *ptr) {
    uint64_t x = (uintptr_t)ptr;

    x *= 0xff51afd7ed558ccd;
    x ^= x >> 32;

    return x;
}

Map *map_create() {
    Map *result = (Map *)xmalloc(sizeof(Map));

    result->cap = 0;
    result->len = 0;
    result->keys = 0;
    result->vals = 0;

    return result;
}

void map_push(Map *map, void *key, void *val);
void map_grow(Map *map) {
    size_t cap = ((map->cap * 2) < 16) ? 16 : map->cap * 2;
    void *mem = xmalloc(map->cap);

    Map new_map = {0};
    new_map.keys = xcalloc(cap, sizeof(void *));
    new_map.vals = xmalloc(cap*sizeof(void *));

    for ( uint32_t i = 0; i < map->cap; ++i ) {
        if ( map->keys[i] ) {
            map_push(&new_map, map->keys[i], map->vals[i]);
        }
    }

    free(map->keys);
    free(map->vals);

    map->cap = cap;
    map->len = map->len;
    map->keys = new_map.keys;
    map->vals = new_map.vals;
}

void map_push(Map *map, void *key, void *val) {
    if ( !map ) return;

    if ( (map->cap / 2) < (map->len + 1) ) {
        map_grow(map);
    }

    size_t i = (size_t)util_ptr_hash(key);
    for (;;) {
        i &= map->cap - 1;

        if ( !map->keys[i] ) {
            map->keys[i] = key;
            map->vals[i] = val;
            map->len++;

            break;
        } else if ( map->keys[i] == key ) {
            map->vals[i] = val;
        }

        i++;
    }
}

void *map_get(Map *map, void *key) {
    if (map->len == 0) {
        return NULL;
    }

    size_t i = (size_t)util_ptr_hash(key);

    for (;;) {
        i &= map->cap - 1;

        if ( map->keys[i] == key ) {
            return map->vals[i];
        } else if ( !map->keys[i] ) {
            return NULL;
        }
        i++;
    }

    return NULL;
}

inline bool
is_equal(char *a, char *b, size_t len) {
    bool result = strncmp(a, b, len) == 0;

    return result;
}

uint32_t
string_len( char *str ) {
    uint32_t result = 0;

    if ( !str ) return result;

    while ( *str ) {
        result++;
        str++;
    }

    return result;
}

void
string_concat( char *first, char *second, char *dest ) {
    uint32_t dest_pos   = 0;
    uint32_t first_len  = string_len(first);
    uint32_t second_len = string_len(second);

    for ( uint32_t i = 0; i < first_len; ++dest_pos, ++i ) {
        dest[dest_pos] = first[i];
    }

    for ( uint32_t i = 0; i < second_len; ++dest_pos, ++i ) {
        dest[dest_pos] = second[i];
    }
}

void
string_copy( char *source, char *dest, uint64_t len ) {
    uint32_t source_len = string_len( source );

    for ( uint32_t i = 0; i < len; ++i ) {
        dest[i] = source[i];
    }

    dest[len] = '\0';
}

int
string_to_int(char *str) {
    int result = 0;

    while ( *str ) {
        result *= 10;
        result += *str - '0';
        str++;
    }

    return result;
}

typedef struct Str_Intern Str_Intern;
struct Str_Intern {
    size_t   length;
    Str_Intern*  next;
    char     str[1];
};

static Map global_str_interns;

static char *
str_intern_range(char *start, char *end) {
    size_t len = end - start;
    uint64_t hash = util_bytes_hash(start, len);
    void *key = (void *)(uintptr_t)(hash ? hash : 1);

    Str_Intern *intern = (Str_Intern *)map_get(&global_str_interns, key);
    for (Str_Intern *it = intern; it; it = it->next) {
        if (it->length == len && strncmp(it->str, start, len) == 0) {
            return it->str;
        }
    }

    Str_Intern *new_intern = (Str_Intern *)xmalloc(offsetof(Str_Intern, str) + len + 1);

    new_intern->length = len;
    new_intern->next   = intern;
    memcpy(new_intern->str, start, len);
    new_intern->str[len] = 0;
    map_push(&global_str_interns, key, new_intern);

    return new_intern->str;
}

static char *
str_intern(char *str) {
    return str_intern_range(str, str + string_len(str));
}

char *
strf(Mem_Arena *arena, char *fmt, ...) {
    va_list args = NULL;
    va_start(args, fmt);
    int size = 1 + vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    char *str = MEM_SIZE(arena, size);

    va_start(args, fmt);
    vsnprintf(str, size, fmt, args);
    va_end(args);

    return str;
}
