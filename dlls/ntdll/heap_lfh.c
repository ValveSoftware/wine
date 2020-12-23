/*
 * Wine Low Fragmentation Heap
 *
 * Copyright 2020 Remi Bernon for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "ntdll_misc.h"
#include "wine/list.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(heap);

typedef struct LFH_ptr LFH_ptr;
typedef struct LFH_block LFH_block;
typedef enum LFH_block_type LFH_block_type;
typedef struct LFH_arena LFH_arena;
typedef struct LFH_class LFH_class;
typedef struct LFH_heap LFH_heap;
typedef struct LFH_slist LFH_slist;

#define ARENA_HEADER_SIZE (sizeof(LFH_arena))

#define LARGE_ARENA_SIZE 0x400000 /* 4MiB */
#define LARGE_ARENA_MASK (LARGE_ARENA_SIZE - 1)

#define BLOCK_ARENA_SIZE 0x10000 /* 64kiB */
#define BLOCK_ARENA_MASK (BLOCK_ARENA_SIZE - 1)

#define SMALL_CLASS_STEP     0x20
#define SMALL_CLASS_MASK     (SMALL_CLASS_STEP - 1)
#define SMALL_CLASS_MIN_SIZE SMALL_CLASS_STEP
#define SMALL_CLASS_MAX_SIZE 0x800
#define SMALL_CLASS_COUNT    ((SMALL_CLASS_MAX_SIZE - SMALL_CLASS_MIN_SIZE) / SMALL_CLASS_STEP + 1)
#define SMALL_CLASS_FIRST    0
#define SMALL_CLASS_LAST     (SMALL_CLASS_FIRST + SMALL_CLASS_COUNT - 1)

#define MEDIUM_CLASS_STEP     (16 * SMALL_CLASS_STEP)
#define MEDIUM_CLASS_MASK     (MEDIUM_CLASS_STEP - 1)
#define MEDIUM_CLASS_MIN_SIZE SMALL_CLASS_MAX_SIZE
#define MEDIUM_CLASS_MAX_SIZE ((BLOCK_ARENA_SIZE - ARENA_HEADER_SIZE - ARENA_HEADER_SIZE) / 2)
#define MEDIUM_CLASS_COUNT    ((MEDIUM_CLASS_MAX_SIZE - MEDIUM_CLASS_MIN_SIZE + MEDIUM_CLASS_MASK) / MEDIUM_CLASS_STEP + 1)
#define MEDIUM_CLASS_FIRST    (SMALL_CLASS_LAST + 1)
#define MEDIUM_CLASS_LAST     (MEDIUM_CLASS_FIRST + MEDIUM_CLASS_COUNT - 1)

#define LARGE_CLASS_STEP      BLOCK_ARENA_SIZE
#define LARGE_CLASS_MASK      (LARGE_CLASS_STEP - 1)
#define LARGE_CLASS_MIN_SIZE  (BLOCK_ARENA_SIZE - ARENA_HEADER_SIZE)
#define LARGE_CLASS_MAX_SIZE  (LARGE_ARENA_SIZE / 2 - ARENA_HEADER_SIZE) /* we need an arena header for every large block */
#define LARGE_CLASS_COUNT     ((LARGE_CLASS_MAX_SIZE - LARGE_CLASS_MIN_SIZE) / LARGE_CLASS_STEP + 1)
#define LARGE_CLASS_FIRST     0
#define LARGE_CLASS_LAST      (LARGE_CLASS_FIRST + LARGE_CLASS_COUNT - 1)

#define TOTAL_BLOCK_CLASS_COUNT (MEDIUM_CLASS_LAST + 1)
#define TOTAL_LARGE_CLASS_COUNT (LARGE_CLASS_LAST + 1)

struct LFH_slist
{
    LFH_slist *next;
};

static void LFH_slist_push(LFH_slist **list, LFH_slist *entry)
{
    /* There will be no ABA issue here, other threads can only replace
     * list->next with a different entry, or NULL. */
    entry->next = __atomic_load_n(list, __ATOMIC_RELAXED);
    while (!__atomic_compare_exchange_n(list, &entry->next, entry, 0, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE));
}

static LFH_slist *LFH_slist_flush(LFH_slist **list)
{
    if (!__atomic_load_n(list, __ATOMIC_RELAXED)) return NULL;
    return __atomic_exchange_n(list, NULL, __ATOMIC_ACQUIRE);
}

/* be sure to keep these different from ARENA_INUSE magic */
enum LFH_block_type
{
    LFH_block_type_used = 0xa55a5aa5a55a5aa5ul,
    LFH_block_type_free = 0xc33c3cc3c33c3cc3ul,
};

struct DECLSPEC_ALIGN(16) LFH_block
{
    union
    {
        ssize_t next_free;
        LFH_slist entry_defer;
        size_t alloc_size;
    };

    LFH_block_type type;
};

C_ASSERT(sizeof(LFH_block) == 0x10);
C_ASSERT(offsetof(LFH_block, entry_defer) == 0);

struct DECLSPEC_ALIGN(16) LFH_arena
{
    ssize_t next_free;
    LFH_arena *class_entry;

    union
    {
        LFH_arena *parent;
        LFH_class *class;
    };

    union
    {
        size_t huge_size;
        size_t used_count;
    };
};

#ifdef _WIN64
C_ASSERT(sizeof(LFH_arena) == 0x20);
#else
C_ASSERT(sizeof(LFH_arena) == 0x10);
#endif

struct LFH_class
{
    LFH_arena *next;
    size_t     size;
};

struct LFH_heap
{
    LFH_slist *list_defer;

    LFH_class block_class[TOTAL_BLOCK_CLASS_COUNT];
    LFH_class large_class[TOTAL_LARGE_CLASS_COUNT];

    SLIST_ENTRY entry_orphan;
};

C_ASSERT(TOTAL_BLOCK_CLASS_COUNT == 0x7d);
C_ASSERT(TOTAL_LARGE_CLASS_COUNT == 0x20);

/* arena->class/arena->parent pointer low bits are used to discriminate between the two */
C_ASSERT(offsetof(LFH_heap, block_class[0]) > 0);
C_ASSERT(offsetof(LFH_heap, large_class[TOTAL_LARGE_CLASS_COUNT]) < BLOCK_ARENA_SIZE);

/* helpers to retrieve parent arena from a child, or class pointer from a large or block arena */
#define LFH_parent_from_arena(arena) (((arena)->parent && !((UINT_PTR)(arena)->parent & BLOCK_ARENA_MASK)) \
                                      ? (arena)->parent : (arena))
#define LFH_class_from_arena(arena) (((UINT_PTR)LFH_parent_from_arena(arena)->class & BLOCK_ARENA_MASK) \
                                     ? LFH_parent_from_arena(arena)->class : NULL)

/* helper to retrieve the heap from an arena, using its class pointer */
#define LFH_heap_from_arena(arena) ((LFH_heap *)((UINT_PTR)LFH_class_from_arena(arena) & ~BLOCK_ARENA_MASK))

/* helpers to retrieve block pointers to the containing block or large (maybe child) arena */
#define LFH_large_arena_from_block(block) ((LFH_arena *)((UINT_PTR)(block) & ~BLOCK_ARENA_MASK))
#define LFH_block_arena_from_block(block) (LFH_large_arena_from_block(block) + 1)
#define LFH_arena_from_block(block) (LFH_block_arena_from_block(block) == ((LFH_arena *)(block)) \
                                     ? LFH_large_arena_from_block(block) : LFH_block_arena_from_block(block))

/* helpers to translate between data pointer and LFH_block header */
#define LFH_block_from_ptr(ptr) ((LFH_block *)(ptr) - 1)
#define LFH_ptr_from_block(block) ((LFH_ptr *)((block) + 1))

static size_t LFH_block_get_class_size(const LFH_block *block)
{
    const LFH_arena *arena = LFH_arena_from_block(block);
    const LFH_class *class = LFH_class_from_arena(arena);
    return class ? class->size : arena->huge_size;
}

static size_t LFH_block_get_alloc_size(const LFH_block *block, ULONG flags)
{
    return block->alloc_size;
}

static size_t LFH_get_class_size(ULONG flags, size_t size)
{
    size_t extra = sizeof(LFH_block) + ((flags & HEAP_TAIL_CHECKING_ENABLED) ? 16 : 0);
    if (size + extra < size) return ~(size_t)0;
    return size + extra;
}

static void *LFH_memory_allocate(size_t size)
{
    void *addr = NULL;
    SIZE_T alloc_size = size;

    if (NtAllocateVirtualMemory(NtCurrentProcess(), (void **)&addr, 0, &alloc_size,
                                MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE))
        return NULL;

    return addr;
}

static BOOLEAN LFH_memory_deallocate(void *addr, size_t size)
{
    SIZE_T release_size = 0;

    if (NtFreeVirtualMemory(NtCurrentProcess(), &addr, &release_size, MEM_RELEASE))
        return FALSE;

    return TRUE;
}

static LFH_block *LFH_arena_get_block(const LFH_arena *arena, size_t offset)
{
    return (LFH_block *)((UINT_PTR)arena + offset);
}

static void LFH_arena_push_block(LFH_arena *arena, LFH_block *block)
{
    block->type = LFH_block_type_free;
    block->next_free = arena->next_free;
    arena->next_free = (UINT_PTR)block - (UINT_PTR)arena;
    arena->used_count--;
}

static LFH_block *LFH_arena_pop_block(LFH_arena *arena)
{
    if (arena->next_free > 0)
    {
        LFH_block *block = LFH_arena_get_block(arena, arena->next_free);
        arena->next_free = block->next_free;
        arena->used_count++;
        return block;
    }
    else
    {
        LFH_arena *large_arena = LFH_large_arena_from_block(arena);
        LFH_class *large_class = LFH_class_from_arena(large_arena);
        LFH_class *class = LFH_class_from_arena(arena);
        LFH_block *block = LFH_arena_get_block(arena, -arena->next_free);
        LFH_arena *child = LFH_large_arena_from_block(block);

        ssize_t extra = (arena == large_arena ? ARENA_HEADER_SIZE : 0);
        ssize_t limit = (arena == large_arena ? LARGE_ARENA_SIZE : large_class->size);

        if (arena == large_arena && arena != child)
            child->parent = arena;

        arena->next_free -= class->size + extra;
        if (-arena->next_free > limit - class->size)
            arena->next_free = 0;

        arena->used_count++;
        return block;
    }
}

static int LFH_arena_is_empty(LFH_arena *arena)
{
    return arena->next_free == 0;
}

static int LFH_arena_is_used(LFH_arena *arena)
{
    return arena->used_count > 0;
}

static int LFH_class_is_block(LFH_heap *heap, LFH_class *class)
{
    return class >= heap->block_class && class < (heap->block_class + TOTAL_BLOCK_CLASS_COUNT);
}

static void LFH_class_initialize(LFH_heap *heap, LFH_class *class, size_t index)
{
    class->next = NULL;

    if (LFH_class_is_block(heap, class))
    {
        if (index <= SMALL_CLASS_LAST)
            class->size = min(SMALL_CLASS_MIN_SIZE + SMALL_CLASS_STEP * (index - SMALL_CLASS_FIRST), SMALL_CLASS_MAX_SIZE);
        else
            class->size = min(MEDIUM_CLASS_MIN_SIZE + MEDIUM_CLASS_STEP * (index - MEDIUM_CLASS_FIRST), MEDIUM_CLASS_MAX_SIZE);
    }
    else
    {
        class->size = min(LARGE_CLASS_MIN_SIZE + LARGE_CLASS_STEP * (index - LARGE_CLASS_FIRST), LARGE_CLASS_MAX_SIZE);
    }
}

static LFH_arena *LFH_class_pop_arena(LFH_class *class)
{
    LFH_arena *arena = class->next;
    if (!arena) return NULL;
    class->next = arena->class_entry;
    return arena;
}

static void LFH_class_remove_arena(LFH_class *class, LFH_arena *arena)
{
    LFH_arena **next = &class->next;
    while (*next != arena) next = &(*next)->class_entry;
    *next = arena->class_entry;
}

static LFH_arena *LFH_class_peek_arena(LFH_class *class)
{
    return class->next;
}

static void LFH_class_push_arena(LFH_class *class, LFH_arena *arena)
{
    arena->class_entry = class->next;
    class->next = arena;
}

static LFH_class *LFH_heap_get_class(LFH_heap *heap, size_t size)
{
    if (size == 0)
        return &heap->block_class[0];
    else if (size <= SMALL_CLASS_MAX_SIZE)
        return &heap->block_class[SMALL_CLASS_FIRST + (size + SMALL_CLASS_MASK - SMALL_CLASS_MIN_SIZE) / SMALL_CLASS_STEP];
    else if (size <= MEDIUM_CLASS_MAX_SIZE)
        return &heap->block_class[MEDIUM_CLASS_FIRST + (size + MEDIUM_CLASS_MASK - MEDIUM_CLASS_MIN_SIZE) / MEDIUM_CLASS_STEP];
    else if (size <= LARGE_CLASS_MAX_SIZE)
        return &heap->large_class[LARGE_CLASS_FIRST + (size + LARGE_CLASS_MASK - LARGE_CLASS_MIN_SIZE) / LARGE_CLASS_STEP];
    else
        return NULL;
}

static void LFH_arena_initialize(LFH_heap *heap, LFH_class *class, LFH_arena *arena, size_t huge_size)
{
    arena->class = class;
    arena->next_free = -ARENA_HEADER_SIZE;

    if (class == NULL)
        arena->huge_size = huge_size;
    else
        arena->used_count = 0;
}

static LFH_arena *LFH_acquire_arena(LFH_heap *heap, LFH_class *class);
static BOOLEAN LFH_release_arena(LFH_heap *heap, LFH_arena *arena);

static LFH_block *LFH_allocate_block(LFH_heap *heap, LFH_class *class, LFH_arena *arena);
static BOOLEAN LFH_deallocate_block(LFH_heap *heap, LFH_arena *arena, LFH_block *block);

static BOOLEAN LFH_deallocate_deferred_blocks(LFH_heap *heap)
{
    LFH_slist *entry = LFH_slist_flush(&heap->list_defer);

    while (entry)
    {
        LFH_block *block = LIST_ENTRY(entry, LFH_block, entry_defer);
        entry = entry->next;

        if (!LFH_deallocate_block(heap, LFH_arena_from_block(block), block))
            return FALSE;
    }

    return TRUE;
}

static size_t LFH_huge_alloc_size(size_t size)
{
    return (ARENA_HEADER_SIZE + size + BLOCK_ARENA_MASK) & ~BLOCK_ARENA_MASK;
}

static LFH_arena *LFH_allocate_huge_arena(LFH_heap *heap, size_t size)
{
    LFH_arena *arena;
    size_t alloc_size = LFH_huge_alloc_size(size);
    if (alloc_size < size) return NULL;

    if ((arena = LFH_memory_allocate(alloc_size)))
        LFH_arena_initialize(heap, NULL, arena, size);

    return arena;
}

static LFH_arena *LFH_allocate_large_arena(LFH_heap *heap, LFH_class *class)
{
    LFH_arena *arena;

    if ((arena = LFH_memory_allocate(LARGE_ARENA_SIZE)))
    {
        LFH_arena_initialize(heap, class, arena, 0);
        LFH_class_push_arena(class, arena);
    }

    return arena;
}

static LFH_arena *LFH_allocate_block_arena(LFH_heap *heap, LFH_class *large_class, LFH_class *block_class)
{
    LFH_arena *large_arena;
    LFH_arena *arena = NULL;

    if ((large_arena = LFH_acquire_arena(heap, large_class)))
    {
        arena = (LFH_arena *)LFH_allocate_block(heap, large_class, large_arena);
        LFH_arena_initialize(heap, block_class, arena, 0);
        LFH_class_push_arena(block_class, arena);
    }

    return arena;
}

static LFH_arena *LFH_acquire_arena(LFH_heap *heap, LFH_class *class)
{
    LFH_arena *arena;

    if (!(arena = LFH_class_peek_arena(class)))
    {
        if (LFH_class_is_block(heap, class))
            arena = LFH_allocate_block_arena(heap, &heap->large_class[0], class);
        else
            arena = LFH_allocate_large_arena(heap, class);
    }

    return arena;
}

static BOOLEAN LFH_release_arena(LFH_heap *heap, LFH_arena *arena)
{
    LFH_arena *large_arena = LFH_large_arena_from_block(arena);
    if (arena == large_arena)
        return LFH_memory_deallocate(arena, LARGE_ARENA_SIZE);
    else
        return LFH_deallocate_block(heap, large_arena, (LFH_block *)arena);
};

static LFH_block *LFH_allocate_block(LFH_heap *heap, LFH_class *class, LFH_arena *arena)
{
    LFH_block *block = LFH_arena_pop_block(arena);
    if (LFH_arena_is_empty(arena))
        LFH_class_pop_arena(class);
    return block;
}

static BOOLEAN LFH_deallocate_block(LFH_heap *heap, LFH_arena *arena, LFH_block *block)
{
    LFH_class *class = LFH_class_from_arena(arena);

    arena = LFH_parent_from_arena(arena);
    if (LFH_arena_is_empty(arena))
        LFH_class_push_arena(class, arena);

    LFH_arena_push_block(arena, block);
    if (LFH_arena_is_used(arena))
        return TRUE;

    LFH_class_remove_arena(class, arena);
    return LFH_release_arena(heap, arena);
}

static SLIST_HEADER *LFH_orphan_list(void)
{
    static SLIST_HEADER *header;
    SLIST_HEADER *ptr, *expected = NULL;

    if ((ptr = __atomic_load_n(&header, __ATOMIC_RELAXED)))
        return ptr;

    if (!(ptr = LFH_memory_allocate(sizeof(*header))))
        return NULL;

    RtlInitializeSListHead(ptr);
    if (__atomic_compare_exchange_n(&header, &expected, ptr, 0, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE))
        return ptr;

    LFH_memory_deallocate(ptr, sizeof(*header));
    return expected;
}

static void LFH_heap_initialize(LFH_heap *heap)
{
    size_t i;

    for (i = 0; i < TOTAL_LARGE_CLASS_COUNT; ++i)
        LFH_class_initialize(heap, &heap->large_class[i], i);
    for (i = 0; i < TOTAL_BLOCK_CLASS_COUNT; ++i)
        LFH_class_initialize(heap, &heap->block_class[i], i);

    heap->list_defer = NULL;
}

static void LFH_heap_finalize(LFH_heap *heap)
{
    LFH_arena *arena;

    LFH_deallocate_deferred_blocks(heap);

    for (size_t i = 0; i < TOTAL_BLOCK_CLASS_COUNT; ++i)
    {
        while ((arena = LFH_class_pop_arena(&heap->block_class[i])))
        {
            WARN("block arena %p still has used blocks\n", arena);
            LFH_release_arena(heap, arena);
        }
    }

    for (size_t i = 0; i < TOTAL_LARGE_CLASS_COUNT; ++i)
    {
        while ((arena = LFH_class_pop_arena(&heap->large_class[i])))
        {
            WARN("large arena %p still has used blocks\n", arena);
            LFH_memory_deallocate(arena, LARGE_ARENA_SIZE);
        }
    }
}

static LFH_heap *LFH_heap_allocate(void)
{
    void *addr;
    addr = LFH_memory_allocate(sizeof(LFH_heap));
    if (!addr)
        return NULL;

    LFH_heap_initialize(addr);
    return addr;
}

static void LFH_heap_deallocate(LFH_heap *heap)
{
    LFH_heap_finalize(heap);
    LFH_memory_deallocate(heap, sizeof(*heap));
}

static LFH_heap *LFH_thread_heap(BOOL create)
{
    SLIST_ENTRY *entry;

    LFH_heap *heap = (LFH_heap *)NtCurrentTeb()->Reserved5[2];
    if (!heap && create)
    {
        if ((entry = RtlInterlockedPopEntrySList(LFH_orphan_list())))
            heap = LIST_ENTRY(entry, LFH_heap, entry_orphan);
        else
            heap = LFH_heap_allocate();

        NtCurrentTeb()->Reserved5[2] = heap;
    }

    return heap;
}

static void LFH_dump_arena(LFH_heap *heap, LFH_class *class, LFH_arena *arena)
{
    LFH_arena *large_arena = LFH_large_arena_from_block(arena);
    LFH_arena *block_arena = LFH_block_arena_from_block(arena);

    if (arena == block_arena)
        WARN("    block arena: %p-%p", arena, (void *)((UINT_PTR)large_arena + BLOCK_ARENA_SIZE - 1));
    else if (arena == large_arena)
        WARN("    large arena: %p-%p", arena, (void *)((UINT_PTR)large_arena + LARGE_ARENA_SIZE - 1));

    WARN(" heap: %p class: %p parent: %p free: %Id used: %Id\n",
          LFH_heap_from_arena(arena), LFH_class_from_arena(arena), LFH_parent_from_arena(arena), arena->next_free, arena->used_count);
}

static void LFH_dump_class(LFH_heap *heap, LFH_class *class)
{
    LFH_arena *arena = class->next;
    if (!arena) return;

    if (LFH_class_is_block(heap, class))
        WARN("  block class: %p size: %Ix\n", class, class->size);
    else
        WARN("  large class: %p size: %Ix\n", class, class->size);

    while (arena)
    {
        LFH_dump_arena(heap, class, arena);
        arena = arena->class_entry;
    }
}

static void LFH_dump_heap(LFH_heap *heap)
{
    size_t i;

    WARN("heap: %p\n", heap);

    for (i = 0; i < TOTAL_BLOCK_CLASS_COUNT; ++i)
        LFH_dump_class(heap, &heap->block_class[i]);

    for (i = 0; i < TOTAL_LARGE_CLASS_COUNT; ++i)
        LFH_dump_class(heap, &heap->large_class[i]);
}

static BOOLEAN LFH_validate_block(ULONG flags, const LFH_block *block);
static BOOLEAN LFH_validate_arena(ULONG flags, const LFH_arena *arena);
static BOOLEAN LFH_validate_heap(ULONG flags, const LFH_heap *heap);

static BOOLEAN LFH_validate_block(ULONG flags, const LFH_block *block)
{
    const LFH_arena *arena = LFH_arena_from_block(block);
    const LFH_arena *large_arena = LFH_large_arena_from_block(block);
    const LFH_arena *block_arena = LFH_block_arena_from_block(block);
    const char *err = NULL;

    if (flags & HEAP_VALIDATE)
        return LFH_validate_arena(flags, arena);

    if (!arena)
        err = "invalid arena";
    else if (arena != LFH_large_arena_from_block(arena) &&
             arena != (LFH_large_arena_from_block(arena) + 1))
        err = "invalid arena alignment";
    else if (arena == block_arena)
    {
        if ((UINT_PTR)block < (UINT_PTR)block_arena + ARENA_HEADER_SIZE ||
            ((UINT_PTR)block & (sizeof(*block) - 1)) != 0)
            err = "invalid block alignment";
    }
    else
    {
        if (arena != large_arena)
            err = "large/huge arena mismatch";
        else if ((UINT_PTR)block != (UINT_PTR)block_arena)
            err = "invalid block for large/huge arena";
    }

    if (err) WARN("%08x %p: %s\n", flags, block, err);
    return err == NULL;
}

static BOOLEAN LFH_validate_free_block(ULONG flags, const LFH_block *block)
{
    const char *err = NULL;

    if (!LFH_validate_block(flags, block))
        return FALSE;
    if (block->type != LFH_block_type_free)
        err = "invalid free block type";

    if (err) WARN("%08x %p: %s\n", flags, block, err);
    return err == NULL;
}

static BOOLEAN LFH_validate_defer_block(ULONG flags, const LFH_block *block)
{
    const char *err = NULL;

    if (!LFH_validate_block(flags, block))
        return FALSE;
    if (block->type != LFH_block_type_free)
        err = "invalid defer block type";
    else if (flags & HEAP_FREE_CHECKING_ENABLED)
    {
        const unsigned int *data = (const unsigned int *)LFH_ptr_from_block(block);
        size_t class_size = LFH_block_get_class_size(block);
        for (size_t i = 0; i < class_size / 4 - (data - (const unsigned int *)block) && !err; ++i)
            if (data[i] != 0xfeeefeee) err = "invalid free filler";
    }

    if (err) WARN("%08x %p: %s\n", flags, block, err);
    return err == NULL;
}

static BOOLEAN LFH_validate_used_block(ULONG flags, const LFH_block *block)
{
    const char *err = NULL;

    if (!LFH_validate_block(flags, block))
        return FALSE;
    if (block->type != LFH_block_type_used)
        err = "invalid used block type";
    else if (0 && (flags & HEAP_TAIL_CHECKING_ENABLED))
    {
        const unsigned char *data = (const unsigned char *)LFH_ptr_from_block(block);
        size_t alloc_size = LFH_block_get_alloc_size(block, flags);
        size_t class_size = LFH_block_get_class_size(block);
        for (size_t i = alloc_size; i < class_size - (data - (const unsigned char *)block) && !err; ++i)
            if (data[i] != 0xab) err = "invalid tail filler";
    }

    if (err) WARN("%08x %p: %s\n", flags, block, err);
    return err == NULL;
}

static BOOLEAN LFH_validate_arena_free_blocks(ULONG flags, const LFH_arena *arena)
{
    ssize_t offset = arena->next_free;
    while (offset > 0)
    {
        LFH_block *block = LFH_arena_get_block(arena, offset);
        if (!LFH_validate_free_block(flags, block))
            return FALSE;
        offset = block->next_free;
    }

    return TRUE;
}

static BOOLEAN LFH_validate_arena(ULONG flags, const LFH_arena *arena)
{
    const char *err = NULL;
    const LFH_arena *parent;

    if (flags & HEAP_VALIDATE)
        return LFH_validate_heap(flags, LFH_heap_from_arena(arena));

    if (arena != LFH_large_arena_from_block(arena) &&
        arena != (LFH_large_arena_from_block(arena) + 1))
        err = "invalid arena alignment";
    else if (arena == LFH_block_arena_from_block(arena))
    {
        if (!LFH_validate_block(flags, (LFH_block *)arena))
            err = "invalid block arena";
        else if (!LFH_validate_arena_free_blocks(flags, arena))
            err = "invalid block arena free list";
    }
    else if (arena == LFH_large_arena_from_block(arena) && !LFH_class_from_arena(arena))
    {
        if (arena->huge_size <= LARGE_CLASS_MAX_SIZE)
            err = "invalid huge arena size";
    }
    else if (arena == LFH_large_arena_from_block(arena) &&
             (parent = LFH_parent_from_arena(arena)) != arena)
    {
        if (arena > parent || LFH_large_arena_from_block(parent) != parent)
            err = "invalid child arena parent";
    }
    else
    {
        if (!LFH_validate_arena_free_blocks(flags, arena))
            err = "invalid large arena free list";
    }

    if (err) WARN("%08x %p: %s\n", flags, arena, err);
    return err == NULL;
}

static BOOLEAN LFH_validate_class_arenas(ULONG flags, const LFH_class *class)
{
    LFH_arena *arena = class->next;
    while (arena)
    {
        if (!LFH_validate_arena(flags, arena))
            return FALSE;

        arena = arena->class_entry;
    }

    return TRUE;
}

static BOOLEAN LFH_validate_heap_defer_blocks(ULONG flags, const LFH_heap *heap)
{
    const LFH_slist *entry = heap->list_defer;

    while (entry)
    {
        const LFH_block *block = LIST_ENTRY(entry, LFH_block, entry_defer);
        if (!LFH_validate_defer_block(flags, block))
            return FALSE;
        entry = entry->next;
    }

    return TRUE;
}

static BOOLEAN LFH_validate_heap(ULONG flags, const LFH_heap *heap)
{
    const char *err = NULL;
    UINT i;

    flags &= ~HEAP_VALIDATE;

    if (heap != LFH_thread_heap(FALSE))
        err = "unable to validate foreign heap";
    else if (!LFH_validate_heap_defer_blocks(flags, heap))
        err = "invalid heap defer blocks";
    else
    {
        for (i = 0; err == NULL && i < TOTAL_BLOCK_CLASS_COUNT; ++i)
        {
            if (!LFH_validate_class_arenas(flags, &heap->block_class[i]))
                return FALSE;
        }

        for (i = 0; err == NULL && i < TOTAL_LARGE_CLASS_COUNT; ++i)
        {
            if (!LFH_validate_class_arenas(flags, &heap->large_class[i]))
                return FALSE;
        }
    }

    if (err) WARN("%08x %p: %s\n", flags, heap, err);
    return err == NULL;
}

static void LFH_block_initialize(LFH_block *block, ULONG flags, size_t old_size, size_t new_size, size_t class_size)
{
    char *ptr = (char *)LFH_ptr_from_block(block);

    TRACE("block %p, flags %x, old_size %Ix, new_size %Ix, class_size %Ix, ptr %p\n", block, flags, old_size, new_size, class_size, ptr);

    if ((flags & HEAP_ZERO_MEMORY) && new_size > old_size)
        memset(ptr + old_size, 0, new_size - old_size);
    else if ((flags & HEAP_FREE_CHECKING_ENABLED) && new_size > old_size && class_size < BLOCK_ARENA_SIZE)
        memset(ptr + old_size, 0x55, new_size - old_size);

    if ((flags & HEAP_TAIL_CHECKING_ENABLED))
        memset(ptr + new_size, 0xab, class_size - new_size - (ptr - (char *)block));

    block->type = LFH_block_type_used;
    block->alloc_size = new_size;
}

static LFH_ptr *LFH_allocate(ULONG flags, size_t size)
{
    LFH_block *block = NULL;
    LFH_class *class;
    LFH_arena *arena;
    LFH_heap *heap = LFH_thread_heap(TRUE);
    size_t class_size = LFH_get_class_size(flags, size);

    if (!LFH_deallocate_deferred_blocks(heap))
        return NULL;

    if (class_size == ~(size_t)0)
        return NULL;

    if ((class = LFH_heap_get_class(heap, class_size)))
    {
        arena = LFH_acquire_arena(heap, class);
        if (arena) block = LFH_allocate_block(heap, class, arena);
        if (block) LFH_block_initialize(block, flags, 0, size, LFH_block_get_class_size(block));
    }
    else
    {
        arena = LFH_allocate_huge_arena(heap, class_size);
        if (arena) block = LFH_arena_get_block(arena, ARENA_HEADER_SIZE);
        if (block) LFH_block_initialize(block, flags, 0, size, LFH_block_get_class_size(block));
    }

    if (!block) return NULL;
    return LFH_ptr_from_block(block);
}

static BOOLEAN LFH_free(ULONG flags, LFH_ptr *ptr)
{
    LFH_block *block = LFH_block_from_ptr(ptr);
    LFH_arena *arena = LFH_arena_from_block(block);
    LFH_heap *heap = LFH_heap_from_arena(arena);

    if (!LFH_class_from_arena(arena))
        return LFH_memory_deallocate(arena, LFH_block_get_class_size(block));

    if (flags & HEAP_FREE_CHECKING_ENABLED)
    {
        unsigned int *data = (unsigned int *)LFH_ptr_from_block(block);
        size_t class_size = LFH_block_get_class_size(block);
        for (size_t i = 0; i < class_size / 4 - (data - (const unsigned int *)block); ++i)
            data[i] = 0xfeeefeee;
    }

    block->type = LFH_block_type_free;

    if (heap == LFH_thread_heap(FALSE))
        LFH_deallocate_block(heap, LFH_arena_from_block(block), block);
    else
        LFH_slist_push(&heap->list_defer, &block->entry_defer);

    return TRUE;
}

static LFH_ptr *LFH_reallocate(ULONG flags, LFH_ptr *old_ptr, size_t new_size)
{
    LFH_block *block = LFH_block_from_ptr(old_ptr);
    LFH_arena *arena = LFH_arena_from_block(block);
    LFH_heap *heap = LFH_heap_from_arena(arena);
    size_t old_size = LFH_block_get_alloc_size(block, flags);
    size_t old_class_size = LFH_block_get_class_size(block);
    size_t new_class_size = LFH_get_class_size(flags, new_size);
    LFH_class *new_class, *old_class = LFH_class_from_arena(arena);
    LFH_ptr *new_ptr = NULL;

    if (new_class_size == ~(size_t)0)
        return NULL;

    if (new_class_size <= old_class_size)
        goto in_place;

    if ((new_class = LFH_heap_get_class(heap, new_class_size)) && new_class == old_class)
        goto in_place;

    if (new_class == old_class && LFH_huge_alloc_size(old_class_size) == LFH_huge_alloc_size(new_class_size))
        goto in_place;

    if (flags & HEAP_REALLOC_IN_PLACE_ONLY)
        return NULL;

    if (!(new_ptr = LFH_allocate(flags, new_size)))
        return NULL;

    memcpy(new_ptr, old_ptr, old_size);

    if (LFH_free(flags, old_ptr))
        return new_ptr;

    LFH_free(flags, new_ptr);
    return NULL;

in_place:
    LFH_block_initialize(block, flags, old_size, new_size, old_class_size);
    return old_ptr;
}

static size_t LFH_get_allocated_size(ULONG flags, const LFH_ptr *ptr)
{
    const LFH_block *block = LFH_block_from_ptr(ptr);
    return LFH_block_get_alloc_size(block, flags);
}

static BOOLEAN LFH_validate(ULONG flags, const LFH_ptr *ptr)
{
    const LFH_block *block = LFH_block_from_ptr(ptr);
    const LFH_heap *heap;

    /* clear HEAP_VALIDATE so we only validate block */
    if (ptr) return LFH_validate_used_block(flags & ~HEAP_VALIDATE, block);

    if (!(heap = LFH_thread_heap(FALSE)))
        return TRUE;

    return LFH_validate_heap(flags, heap);
}

static BOOLEAN LFH_try_validate_all(ULONG flags)
{
    if (!(flags & HEAP_VALIDATE_ALL))
        return TRUE;

    if (LFH_validate(flags, NULL))
        return TRUE;

    LFH_dump_heap(LFH_thread_heap(FALSE));
    return FALSE;
}

void *HEAP_lfh_allocate(struct tagHEAP *std_heap, ULONG flags, SIZE_T size)
{
    void *ptr;

    TRACE("%p %08x %lx\n", std_heap, flags, size);

    if (!LFH_try_validate_all(flags))
        goto error;

    if ((ptr = LFH_allocate(flags, size)))
        return ptr;

    if (flags & HEAP_GENERATE_EXCEPTIONS) RtlRaiseStatus(STATUS_NO_MEMORY);
    RtlSetLastWin32ErrorAndNtStatusFromNtStatus(STATUS_NO_MEMORY);
    return NULL;

error:
    RtlSetLastWin32ErrorAndNtStatusFromNtStatus(STATUS_INVALID_PARAMETER);
    return NULL;
}

BOOLEAN HEAP_lfh_free(struct tagHEAP *std_heap, ULONG flags, void *ptr)
{
    TRACE("%p %08x %p\n", std_heap, flags, ptr);

    if (!LFH_try_validate_all(flags))
        goto error;

    if (!LFH_validate(flags, ptr))
        goto error;

    return LFH_free(flags, ptr);

error:
    RtlSetLastWin32ErrorAndNtStatusFromNtStatus(STATUS_INVALID_PARAMETER);
    return FALSE;
}

void *HEAP_lfh_reallocate(struct tagHEAP *std_heap, ULONG flags, void *ptr, SIZE_T size)
{
    TRACE("%p %08x %p %lx\n", std_heap, flags, ptr, size);

    if (!LFH_try_validate_all(flags))
        goto error;

    if (!LFH_validate(flags, ptr))
        goto error;

    if ((ptr = LFH_reallocate(flags, ptr, size)))
        return ptr;

    if (flags & HEAP_GENERATE_EXCEPTIONS) RtlRaiseStatus(STATUS_NO_MEMORY);
    RtlSetLastWin32ErrorAndNtStatusFromNtStatus(STATUS_NO_MEMORY);
    return NULL;

error:
    RtlSetLastWin32ErrorAndNtStatusFromNtStatus(STATUS_INVALID_PARAMETER);
    return NULL;
}

SIZE_T HEAP_lfh_get_allocated_size(struct tagHEAP *std_heap, ULONG flags, const void *ptr)
{
    TRACE("%p %08x %p\n", std_heap, flags, ptr);

    if (!LFH_try_validate_all(flags))
        goto error;

    if (!LFH_validate(flags, ptr))
        goto error;

    return LFH_get_allocated_size(flags, ptr);

error:
    RtlSetLastWin32ErrorAndNtStatusFromNtStatus(STATUS_INVALID_PARAMETER);
    return ~(SIZE_T)0;
}

BOOLEAN HEAP_lfh_validate(struct tagHEAP *std_heap, ULONG flags, const void *ptr)
{
    TRACE("%p %08x %p\n", std_heap, flags, ptr);

    if (!LFH_try_validate_all(flags))
        return FALSE;

    return LFH_validate(flags, ptr);
}

void HEAP_lfh_notify_thread_destroy(BOOLEAN last)
{
    SLIST_HEADER *list_orphan = LFH_orphan_list();
    SLIST_ENTRY *entry_orphan = NULL;
    LFH_heap *heap;

    if (last)
    {
        while ((entry_orphan || (entry_orphan = RtlInterlockedFlushSList(list_orphan))))
        {
            LFH_heap *orphan = LIST_ENTRY(entry_orphan, LFH_heap, entry_orphan);
            entry_orphan = entry_orphan->Next;
            LFH_heap_deallocate(orphan);
        }
    }
    else if ((heap = LFH_thread_heap(FALSE)) && LFH_validate_heap(0, heap))
        RtlInterlockedPushEntrySList(list_orphan, &heap->entry_orphan);
}

void HEAP_lfh_set_debug_flags(ULONG flags)
{
    LFH_heap *heap = LFH_thread_heap(FALSE);
    if (!heap) return;

    LFH_deallocate_deferred_blocks(heap);
}
