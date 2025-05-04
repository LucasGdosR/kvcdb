#include "alloc.c"
#include "avl.c"

int main() {
    init_kv_alloc();
    init_avl();

#ifdef DEBUG
    test_avl();
#endif

    return 0;
}