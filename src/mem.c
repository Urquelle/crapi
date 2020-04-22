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

typedef struct {
    size_t size;
    size_t used;
    void *mem;
} Mem_Page;

Mem_Page *mem_page_new(size_t size) {
    Mem_Page *result = (Mem_Page *)malloc(sizeof(Mem_Page));

    result->size = size;
    result->used = 0;
    result->mem  = malloc(size);

    return result;
}

void mem_page_free(Mem_Page *page) {
    free(page->mem);
    free(page);
}

typedef struct {
    size_t min_page_size;
    size_t curr_page_num;

    size_t num_pages;
    size_t max_pages;
    Mem_Page **pages;
} Mem_Arena;

Mem_Arena *mem_arena_new(size_t min_page_size) {
    Mem_Arena *result = (Mem_Arena *)malloc(sizeof(Mem_Arena));

    result->min_page_size = min_page_size;
    result->curr_page_num = 0;

    result->num_pages     = 1;
    result->max_pages     = 10;
    result->pages         = (Mem_Page **)malloc(sizeof(Mem_Page *)*result->max_pages);
    result->pages[0]      = mem_page_new(min_page_size);

    return result;
}

void *mem_alloc(Mem_Arena *arena, size_t size) {
    Mem_Page *page = arena->pages[arena->curr_page_num];

    if ( (page->used + size) > page->size ) {
        arena->curr_page_num += 1;
        size_t new_size = ((arena->min_page_size < size) ? size : arena->min_page_size)*2;

        if ( arena->num_pages >= arena->max_pages ) {
            arena->max_pages = arena->max_pages*2;
            arena->pages = (Mem_Page **)realloc(arena->pages, arena->max_pages);
        }

        arena->pages[arena->curr_page_num] = mem_page_new(new_size);
        page = arena->pages[arena->curr_page_num];
    }

    void *result = (void *)((char *)page->mem + page->used);
    page->used = page->used + size;

    return result;
}

void mem_reset(Mem_Arena *arena) {
    for ( int i = 0; i < arena->num_pages; ++i ) {
        Mem_Page *page = arena->pages[i];
        page->used = 0;
    }

    arena->curr_page_num = 0;
}

