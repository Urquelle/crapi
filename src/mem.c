#define MEM_SIZE(Arena, Size) mem_alloc(Arena, Size)
#define MEM_STRUCT(Arena, Struct) (Struct *)mem_alloc(Arena, sizeof(Struct))

#define ZERO_STRUCT(inst) zero_size(sizeof(inst), &(inst))
#define ZERO_ARRAY(count, pointer) zero_size(count*sizeof((pointer)[0]), pointer)

inline void
zero_size(size_t size, void *ptr) {
    uint8_t *byte = (uint8_t *)ptr;

    while ( size-- ) {
        *byte++ = 0;
    }
}

typedef struct Mem_Page Mem_Page;
struct Mem_Page {
    size_t size;
    size_t used;
    Mem_Page *next;
    void *mem;
};

Mem_Page *
mem_page(size_t size) {
    Mem_Page *result = (Mem_Page *)malloc(sizeof(Mem_Page));

    result->size = size;
    result->used = 0;
    result->next = 0;
    result->mem  = malloc(size);

    return result;
}

void mem_page_free(Mem_Page *page) {
    free(page->mem);
    free(page);
}

enum { MIN_PAGE_SIZE = 2048 };
typedef struct {
    Mem_Page *curr_page;
    size_t num_pages;
    Mem_Page **pages;
} Mem_Arena;

Mem_Arena *
mem_arena_new() {
    Mem_Arena *result = (Mem_Arena *)malloc(sizeof(Mem_Arena));

    result->num_pages = 1;
    result->curr_page = (Mem_Page *)mem_page(MIN_PAGE_SIZE);
    result->pages     = &result->curr_page;

    return result;
}

void *
mem_alloc(Mem_Arena *arena, size_t size) {
    Mem_Page *page = arena->curr_page;

    if ( (page->used + size) > page->size ) {
        if ( !page->next ) {
            arena->num_pages += 1;
            size_t new_size = (MIN_PAGE_SIZE < size) ? size : MIN_PAGE_SIZE;
            page->next = mem_page(new_size);
        }

        arena->curr_page = page->next;
        page = page->next;
    }

    void *result = (void *)((char *)page->mem + page->used);
    page->used = page->used + size;

    return result;
}

void
mem_reset(Mem_Arena *arena) {
    Mem_Page *page = arena->curr_page;

    while ( page ) {
        page->used = 0;
        page = page->next;
    }

    arena->curr_page = *arena->pages;
}

