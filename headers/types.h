#pragma once

#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef i32 b32;

#define KB (1 << 10)
#define MB (1 << 20) 
#define GB (1 << 30)

#define private_global static
#define internal static

enum Node_State {
    NOT_EXISTS,
    EXISTS,
    DELETED,
};

// Data must be 8-byte aligned.
typedef struct byte_slice {
    u8* data;
    u32 size;
    u32 capacity;
} byte_slice;

enum Error_Code {
    NO_ERROR,
    INIT_AVL,
    INIT_KV,
    ALLOC_KV,
};