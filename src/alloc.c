#include "../headers/alloc.h"
#include "../headers/utils.h"
#include "../headers/types.h"

#include <stdlib.h>
#include <sys/mman.h>

void init_kv_alloc() {
    if
    // TODO: alloc in platform layer.
    (!(global_kv_alloc.base = mmap(
        0,
        KV_RESERVE_SIZE,
        PROT_NONE,
        MAP_PRIVATE | MAP_ANON,
        -1,
        0))
    )
    {
        exit(INIT_KV);
    }
}

// Returns an 8-byte aligned slice with a capacity that's a multiple of 8.
byte_slice alloc_kv(u32 size) {
    assert((i64)global_kv_alloc.size + (i64)size < KV_RESERVE_SIZE, "KV Arena out of capacity.\n");
    
    u32 capacity = align(size, 8);

    byte_slice result = { global_kv_alloc.base + global_kv_alloc.size, size, capacity };
    
    global_kv_alloc.size += capacity;
    
    if (global_kv_alloc.committed < global_kv_alloc.size)
    {
        i64 commit_multiple = 1 + 
        // Strength reduction should turn this division into a right-shift
        // TODO: check compiler outpu    t
            (global_kv_alloc.size - global_kv_alloc.committed - 1) / KV_COMMIT_SIZE;
        i64 commit_size = commit_multiple * KV_COMMIT_SIZE;
        // TODO: commit in platform layer
        if (mprotect(
            global_kv_alloc.base + global_kv_alloc.committed,
            commit_size,
            PROT_READ | PROT_WRITE))
        {
            exit(ALLOC_KV);
        }
        global_kv_alloc.committed += commit_size;
    }

    return result;
}