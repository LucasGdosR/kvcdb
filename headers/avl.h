#pragma once

#include "types.h"

#define CRITICAL_MASS 0xFFFF

// Offsets are multiples of 8. Blob storage starts 8-byte aligned.
typedef struct blob {
    u32 offset;
    u32 size;
} blob;

typedef struct node {
    blob key;
    blob val;
    u16 parent;
    u16 left;
    u16 right;
    i8 height;
    i8 deleted;
} node;

typedef struct avl {
    u16 root;
    u16 size;
    // u16 capacity = CRITICAL_MASS
    node *nodes;
} avl;

typedef struct {
    byte_slice data;
    enum Node_State state;
} avl_ret;

typedef struct {
    i32 i;
    enum Node_State state;
} avl_ret_;

void put(byte_slice k, byte_slice v);

void del(byte_slice k);

avl_ret get(byte_slice k);

byte_slice fetch_blob(blob b);
i8 node_balance_factor(node *n);
void update_height(node *n);
blob slice2blob(byte_slice s);