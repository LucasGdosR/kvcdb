#include "../headers/alloc.h"
#include "../headers/avl.h"
#include "../headers/utils.h"

#include <stdlib.h>
#include <sys/mman.h>

avl avl_tree;

// _____________________________________________________________________________
// HELPER FUNCTIONS
// _____________________________________________________________________________
inline byte_slice fetch_blob(blob b) {
    byte_slice result = {
        .size=b.size,
        .capacity=b.size,
        .data=global_kv_alloc.base+b.offset
    };
    return result;
}

inline blob slice2blob(byte_slice s) {
    blob results = {s.data - global_kv_alloc.base, s.size};
    return results;
}

inline i8 node_balance_factor(node *n) {
    return avl_tree.nodes[n->left].height - avl_tree.nodes[n->right].height;
}

inline void update_height(node *n) {
    i8 lh = avl_tree.nodes[n->left].height;
    i8 rh = avl_tree.nodes[n->right].height;
    n->height = 1 + (lh > rh ? lh : rh);
}

// TODO: SIMD
i32 slice_memcmp(byte_slice a, byte_slice b) {
    i32 result = 0;

    u64* restrict ptr_a = (u64*)a.data;
    u64* restrict ptr_b = (u64*)b.data;
    
    for
    (
        u32 bound = (a.size < b.size ? a.size : b.size) / 8,
        i = 0; i < bound; i++
    )
    {
        u64 val_a = *ptr_a++;
        u64 val_b = *ptr_b++;
        if (val_a != val_b)
        {
            result = val_a < val_b ? -1 : 1;
            break;
        }
    }
 
    if (!result)
    {
        u8* restrict ptr_a8 = (u8*)ptr_a;
        u8* restrict ptr_b8 = (u8*)ptr_b;
        for
        (
            i32 bound = (a.size < b.size ? a.size : b.size) % 8,
            i = 0; i < bound; i++
        )
        {
            u8 val_a = *ptr_a8++;
            u8 val_b = *ptr_b8++;
            if (val_a != val_b)
            { 
                result = val_a < val_b ? -1 : 1;
                break;
            }
        }

        if (!result)
        {
            result = a.size < b.size
                ? -1
                : b.size < a.size
                ? 1
                : 0;
        }
    }
    
    return result;
}

// If the key exists (even if deleted), returns its index in curr.
// Else, returns the index of the last valid node visited (for insertion).
avl_ret_ get_(byte_slice k) {
    u16 curr = avl_tree.root, parent = 0;
    
    node *n = avl_tree.nodes;
    
    while (curr)
    {
        i32 cmp = slice_memcmp(k, fetch_blob(n[curr].key));
        if (cmp)
        {
            parent = curr;
            curr = cmp < 0
                 ? n[curr].left
                 : n[curr].right;
        }
        else
        {
            break;
        }
    }
    
    avl_ret_ result = {
        .i = curr ? curr : parent,
        .state = curr
                ? avl_tree.nodes[curr].deleted
                    ? DELETED
                    : EXISTS
                : NOT_EXISTS
    };
    return result;
}

// 1)
// Parent's right becomes child's left.
// Child's left's parent becomes parent.
// 2)
// Child's left becomes parent.
// Parent's parent becomes child.
// 3)
// Child's parent becomes parent's parent.
// Some child of parent's parent becomes child.
void rotate_left(node* restrict parent, node* restrict child, u16 pi, u16 ci) {
    u16 ppi = parent->parent;
    node* restrict gp = &avl_tree.nodes[ppi];
    // 1)
    parent->right = child->left;
    if (child->left)
    {
        avl_tree.nodes[child->left].parent = pi;
    }
    // 2)
    child->left = pi;
    parent->parent = ci;
    // 3)
    child->parent = ppi;
    if (gp->left == pi)
    {
        gp->left = ci;
    }
    else if (gp->right == pi)
    {
        gp->right = ci;
    }
    else 
    {
        avl_tree.root = ci;
    }
    update_height(parent);
    update_height(child);
}

// 1)
// Parent's left becomes child's right.
// Child's right's parent becomes parent.
// 2)
// Child's right becomes parent.
// Parent's parent becomes child.
// 3)
// Child's parent becomes parent's parent.
// Some child of parent's parent becomes child.
void rotate_right(node* restrict parent, node* restrict child, u16 pi, u16 ci) {
    u16 ppi = parent->parent;
    node* restrict gp = &avl_tree.nodes[ppi];
    // 1)
    parent->left = child->right;
    if (child->right)
    {
        avl_tree.nodes[child->right].parent = pi;
    }
    // 2)
    child->right = pi;
    parent->parent = ci;
    // 3)
    child->parent = ppi;
    if (gp->left == pi)
    {
        gp->left = ci;
    }
    else if (gp->right == pi)
    {
        gp->right = ci;
    }
    else 
    {
        avl_tree.root = ci;
    }
    update_height(parent);
    update_height(child);

}

void balance_tree(u16 entry) {
    u16 i = entry;
    while (i)
    {
        node *n = &avl_tree.nodes[i];
        i8 old_height = n->height;
        update_height(n);
        i8 balance_factor = node_balance_factor(n);
        if (balance_factor == 2)
        {
            if (node_balance_factor(&avl_tree.nodes[n->left]) < 0)
            {
                node *left = &avl_tree.nodes[n->left];
                rotate_left(left, &avl_tree.nodes[left->right], n->left, left->right);
            }
            rotate_right(n, &avl_tree.nodes[n->left], i, n->left);
        }
        else if (balance_factor == -2)
        {
            if (node_balance_factor(&avl_tree.nodes[n->right]) > 0)
            {
                node *right = &avl_tree.nodes[n->right];
                rotate_right(right, &avl_tree.nodes[right->left], n->right, right->left);
            }
            rotate_left(n, &avl_tree.nodes[n->right], i, n->right);
        }
        if (n->height == old_height)
        {
            break;
        }
        i = n->parent;
    }
}

// Overwrites existing keys and returns their index.
// Inserts new keys (and balances) and returns their index.
i32 put_(byte_slice k, blob v) {
    i32 result = 0;
    blob key = slice2blob(k);
    // Empty tree: add the root.
    if (avl_tree.size == 0)
    {
        avl_tree.size = 1, avl_tree.root = 1;
        avl_tree.nodes[1] = (node){.key=key, .val=v, .height=1};    
        result = 1;
    }
    // Non-empty tree
    else
    {
        avl_ret_ prev = get_(k);
        node *n = &avl_tree.nodes[prev.i];
        // Overwrite
        if (prev.state != NOT_EXISTS)
        {
            n->deleted = 0;
            n->val = v;
            result = prev.i;
        }
        // Insert
        else
        {
            result = ++avl_tree.size;
            avl_tree.nodes[avl_tree.size] = (node){.key=key, .val=v, .height=1, .parent=prev.i};
            if (slice_memcmp(k, fetch_blob(n->key)) < 0)
            {
                n->left = avl_tree.size;
            }
            else
            {
                n->right = avl_tree.size;
            }
            balance_tree(prev.i);
        }
    }
    return result;
}

// _____________________________________________________________________________
// PUBLIC API
// _____________________________________________________________________________

// Overwrites existing keys. Flips flags for deleted keys.
// Inserts new keys and rebalances.
inline void put(byte_slice k, byte_slice v) {
    put_(k, slice2blob(v));
}

// Adds deleted flag to existing keys.
// Inserts node for non-existent keys and rebalances.
inline void del(byte_slice k) {
    i32 i = put_(k, (blob){});
    avl_tree.nodes[i].deleted = 1;
}

// If the state returned is NOT_EXISTS, the query must go to disk.
inline avl_ret get(byte_slice k) {
    avl_ret_ ret = get_(k);
    
    avl_ret result;
    result.state = ret.state;
    ret.i = 0; 
    if (ret.state == EXISTS)
    {
        result.data = fetch_blob(avl_tree.nodes[ret.i].val);
    }
    return result;
}

// The tree must be initialized before operations.
void init_avl() {
    if
    // TODO: alloc in platform layer.
    (!(avl_tree.nodes = mmap(
        0,
        sizeof(node) * CRITICAL_MASS,
        PROT_READ | PROT_WRITE,
        MAP_ANON | MAP_PRIVATE, // TODO: HUGE PAGES
        -1,
        0))
    )
    {
        exit(INIT_AVL);
    }
}

#ifdef DEBUG

u16 count_nodes(u16 node);
b32 ordering_is_valid(u16 node);
b32 height_property_is_valid(u16 node);

i32 pseudorand(i32 seed) {
    seed = (seed ^ (seed << 6));
    seed = (seed ^ (seed >> 5));
    return (seed ^ (seed << 11));
}

void test_avl() {
    i64 count = 20000, seed = 42;
    while (count--)
    {
        seed = pseudorand(seed);
        i64 k = seed, v = count;
        byte_slice key = alloc_kv(sizeof(i64));
        byte_slice val = alloc_kv(sizeof(i64));
        *((i64*)key.data) = k;
        *((i64*)val.data) = v;
        put(key, val); 
    }

    assert(avl_tree.size == count_nodes(avl_tree.root), "Walking the tree doesn't find all nodes or has cycles.\n");
    assert(ordering_is_valid(avl_tree.root), "The tree isn't sorted.\n");
    assert(height_property_is_valid(avl_tree.root), "AVL height property violated.\n");
}

u16 count_nodes(u16 node) {
    return node ? 1 + count_nodes(avl_tree.nodes[node].left) + count_nodes(avl_tree.nodes[node].right) : 0;
}

b32 height_property_is_valid(u16 node) {
    return node == 0 ? 1 :
        abs(node_balance_factor(&avl_tree.nodes[node])) > 1 ? 0 :
        height_property_is_valid(avl_tree.nodes[node].left) && height_property_is_valid(avl_tree.nodes[node].right);
}

blob* in_order_traversal(blob *keys, u16 n) {
    blob *result = keys;
    if (n)
    {
        node *node = &avl_tree.nodes[n];
        result = in_order_traversal(keys, node->left);
        *result++ = node->key;
        result = in_order_traversal(result, node->right);
    }
    return result;
}

b32 ordering_is_valid(u16 node) {
    i32 size = avl_tree.size;
    blob *keys = mmap(0, sizeof(blob) * size--, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    assert(keys, "Couldn't allocate auxiliary buffer in `ordering_is_valid()` test.\n");
    in_order_traversal(keys, avl_tree.root);
    
    while (size--)
    {
        assert(slice_memcmp(fetch_blob(keys[size]), fetch_blob(keys[size+1])) < 0, "AVL didn't maintain order.\n");
    }
    
    assert(!munmap(keys, sizeof(blob) * avl_tree.size), "Failed unmapping auxiliary buffer in `ordering_is_valid()` test.\n");

    return 1;
}
#endif