#pragma once

#include "types.h"

#define KV_RESERVE_SIZE (4LL * GB)
#define KV_COMMIT_SIZE (2 * MB)

typedef struct kv_allocator {
    u8* base;
    u32 size;
    u32 committed;
} kv_allocator;

kv_allocator global_kv_alloc;
byte_slice alloc_kv(u32 size); 