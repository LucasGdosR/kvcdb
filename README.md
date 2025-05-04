# kvcdb
Exploration of some key-value database ideas in C. No libs (only syscalls).

### Plans
1. Add a REPL (learn about buffered I/O);
2. Serialize segments to disk once the tree reaches a threshold;
3. Add WAL to make disk writes safe;
4. Search through persisted segments when the query isn't in memory;
5. Use hashing to accelerate lookups;
6. Add a background thread compacting segments with duplicated keys;
7. Include compression for values;
8. ???

### Implemented

#### alloc.\[c, h\]
**Summary:** Allocates memory for blobs.

**Tricks:**
- Bump allocator that's reseted whenever the AVL is flushed to disk;
- Reserves a huge chunk of memory from the OS (4GB), but commits only as needed (2MB multiples);
- Returns 8-byte aligned slices to facilitate reading and writing.

**TODO:**
- Make it platform independent;
- Reset it when flushing to disk (clear to zero?);
- Make it 32-byte aligned for AVX2 SIMD reading / writing?

#### avl.\[c, h\]
**Summary:** AVL tree that stores writes in memory in sorted order before flushing to disk.

**Tricks:**
- Uses indexes instead of pointers to save memory (16B / node);
- Uses `blob`s instead of `byte_slice`s to store keys and values to save memory (16B / node);
- Receives `byte_slice`s from its public API so `blob` abstraction doesn't leak;
- Recent data is in memory, but older data is in disk, so deletion actually stores nodes in the tree;
- `get` is reused for all three operations;
- Enum for `get`'s return allows storing `null`;
- `slice_memcmp` compares 8-bytes at a time and finishes with a byte-by-byte comparison if unaligned.

**TODO**:
- When the tree is full, flush to disk and reset the allocator;
- Move `slice_memcmp` elsewhere (utils?);
- Make `slice_memcmp` use SIMD.

#### utils.h
**Summary:** Macros.
**Implemented**:
- `align`: ceiling function to a multiple of an alignment;
- `assert`: regular assert.