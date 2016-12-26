#include "my_malloc.h"

/* These have been provided for convenience */
void print_metadata(metadata_t* block, int with_canary) {
    printf("%p - { next = %p, block_size = %d, request_size = %d",
        (void*)block,
        (void*)block->next,
        block->block_size,
        block->request_size);

    if(with_canary) {
        int* end_canary = (int*)((char*)block + sizeof(metadata_t) + block->request_size);
        printf(", begin_canary = %d, end_canary = %d", block->canary, *end_canary);
    }

    printf(" }\n");
}

/* These have been provided for convenience */
void print_freelist() {
    printf("\n--------FREELIST--------\n");
    
    extern metadata_t* freelist;
    for(metadata_t* curr = freelist; curr; curr = curr->next) {
        print_metadata(curr, 0);
    }

    char* name = NULL;
    switch(ERRNO) {
        case NO_ERROR:
            name = "NO_ERROR";
            break;
        case OUT_OF_MEMORY:
            name = "OUT_OF_MEMORY";
            break;
        case SINGLE_REQUEST_TOO_LARGE:
            name = "SINGLE_REQUEST_TOO_LARGE";
            break;
        case CANARY_CORRUPTED:
            name = "CANARY_CORRUPTED";
            break;
    }

    printf("ERRNO = %s\n", name);
}

int main() {
    /* Put tests here! */

    return 0;
}
