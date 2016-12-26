/*
 * CS 2110 Fall 2016
 * Author: Dong Son Trinh
 */

/* we need this for uintptr_t */
#include <stdint.h>
/* we need this for memcpy/memset */
#include <string.h>
/* we need this for my_sbrk */
#include "my_sbrk.h"
/* we need this for the metadata_t struct definition */
#include "my_malloc.h"

/* You *MUST* use this macro when calling my_sbrk to allocate the
 * appropriate size. Failure to do so may result in an incorrect
 * grading!
 */
#define SBRK_SIZE 2048


/*
 * canary calculator
 */
#define CANARY_CALC(block, request, ptr) (((block) << 16) | (request)) ^ (ptr)

/* If you want to use debugging printouts, it is HIGHLY recommended
 * to use this macro or something similar. If you produce output from
 * your code then you may receive a 20 point deduction. You have been
 * warned.
 */
#ifdef DEBUG
#define DEBUG_PRINT(x) printf(x)
#else
#define DEBUG_PRINT(x)
#endif

/* Our freelist structure - this is where the curr freelist of
 * blocks will be maintained. failure to maintain the list inside
 * of this structure will result in no credit, as the grader will
 * expect it to be maintained here.
 * DO NOT CHANGE the way this structure is declared
 * or it will break the autograder.
 */
metadata_t* freelist;

/* my memcpy
 *
 * my version of memcpy that copies data over from a destination pointer to source
 * for length of size
 */
static void my_memcpy(void *src, void *dst, size_t size) {
    char *src_char = (char *) src;
    char *dst_char = (char *) dst;
    while (size) {
        *dst_char = *src_char;
        dst_char++;
        src_char++;
        size--;
    }
}

/*
 * update canary of the block in the metadata and at the end of the block
 */
static void update_canary(metadata_t *block) {
    block->canary = CANARY_CALC(block->block_size, block->request_size, (unsigned int) (uintptr_t) block);
    *(unsigned int *) ((char *) block + block->request_size + sizeof(metadata_t)) = block->canary;
}

/*
 * set value in memory to 0 starting from the address by ptr for length of size
 */
static void zero_out(void *ptr, size_t size) {
    char * ptr_char = (char *) ptr;
    while (size) {
        *ptr_char = 0;
        ptr_char++;
        size--;
    } 
}

void* my_malloc(size_t size) {

    /* set to no error */
    ERRNO = NO_ERROR;
    
    /* if input size is zero, return NULL */
    if (!size) {
        return NULL;
    }

    /* calculate full size of new block */
    size_t new_size = size + sizeof(metadata_t) + sizeof(int);

    /* if full size > 2KB */
    if (new_size > SBRK_SIZE) {
        ERRNO = SINGLE_REQUEST_TOO_LARGE;
        return NULL;
    }

    /* if the freelist is pointing to null, expand */
    if (!freelist) {
        freelist = (metadata_t *) my_sbrk(SBRK_SIZE);
        /* if expands are more than 8KB, error
         * otherwise expand
         */
        if (!freelist) {
            ERRNO = OUT_OF_MEMORY;
            return NULL;
        } else {
            freelist->next = NULL;
            freelist->block_size = SBRK_SIZE;
            freelist->request_size = 0;
        }
    }


    /* block pointers used for tracking position in freelist */
    metadata_t *curr = freelist;
    metadata_t *prev = NULL;
    metadata_t *pprev = NULL;

    /* iterate until found a block that could fit in the new block
     * or stop if no such block is found
     */
    while (curr && curr->block_size < new_size) {
        pprev = prev;
        prev = curr;
        curr = curr->next;
    }

    /* if a fitting block cannot be found, expand the list */
    if (!curr) {
        curr = (metadata_t *) my_sbrk(SBRK_SIZE);
        if (!curr) {
            ERRNO = OUT_OF_MEMORY; 
            return NULL;
        } else {
            curr->next = NULL;
            curr->block_size = SBRK_SIZE;
            curr->request_size = 0;
            /* if the new block is located right after the last non-NULL block
             * in the freelist, combine them into 1
             */
            if ((uintptr_t) ((char *) prev + sizeof(metadata_t)) == (uintptr_t) curr) {
                prev->next = NULL;
                prev->block_size += curr->block_size;
                prev->request_size = 0;
                curr = prev;
                prev = pprev;
            } else {
                prev->next = curr; 
            }
        }
    }

    /* if the block fits exatly, remove that block from the freelist and return it */
    if (curr->block_size == new_size) {
        if (!prev) {
            freelist = curr->next;
        } else {
            prev->next = curr->next;
        }
        curr->next = NULL;
        curr->block_size = new_size;
        curr->request_size = size;
        update_canary(curr);
        return (metadata_t *) ((char *) curr + sizeof(metadata_t));
    }

    /* if the block, once cut off the size for a new block, does not have enough
     * space to fit 1 more block, return the whole block
     */
    if (curr->block_size - new_size <= sizeof(metadata_t) + sizeof(int)) {
        if (curr->block_size - new_size < 0) {
            ERRNO = SINGLE_REQUEST_TOO_LARGE;
            return NULL;
        } else {
            if (!prev) {
                freelist = curr->next;
            } else {
                prev->next = curr->next;
            }
            curr->next = NULL;
            curr->block_size = new_size;
            curr->request_size = size;
            update_canary(curr);
            return (metadata_t *) ((char *) curr + sizeof(metadata_t));
        }
    }


    /* make new block to return */
    metadata_t *next_block = curr->next;
    size_t old_size = curr->block_size;

    metadata_t *new_block = curr;

    /* move the pointer up by the size of the new block */
    curr = (metadata_t *) ((char *) curr + new_size);
    curr->block_size = old_size - new_size;
    curr->next = next_block;
    curr->request_size = 0;

    if (!prev) {
        freelist = curr;
    } else {
        prev->next = curr;
    }

    /* initialize the metadata of the new block, including the canary */
    new_block->block_size = new_size;
    new_block->next = NULL;
    new_block->request_size = size;
    update_canary(new_block);
    
    /* return the pointer to data right after the metadata */
    return (metadata_t *) ((char *) new_block + sizeof(metadata_t));
}

void* my_realloc(void* ptr, size_t new_size) {
    /* if NULL pointer is given, malloc the size */
    if (!ptr) {
        ptr = my_malloc(new_size);
        return ptr;
    } else if (!new_size) {
        /* if given size is zero, free the pointer */
        my_free(ptr);
        return NULL;
    } else {
        /* pointer to the actual block and pointer to the new blcok */
        metadata_t *curr_block = (metadata_t *) ((char *) ptr - sizeof(metadata_t));
        void *new_ptr;
        size_t curr_size = curr_block->request_size;
        /* if the requested size is same as the size of block, do not do anything */
        if (new_size == curr_size) {
            return ptr;
        } else {
            /* allocate new block with the new size */
            new_ptr = my_malloc(new_size);
            if (!new_ptr) {
                return NULL;
            }
            /* get the minimum of the new size and the size of the actual block */
            size_t real_size = new_size > curr_size ? curr_size : new_size;

            /* copy the whole block for memory length of size */
            my_memcpy(ptr, new_ptr, real_size);

            /* free old block*/
            my_free(ptr);
            return new_ptr;
        }
    }
}


void* my_calloc(size_t nmemb, size_t size) {
    if (!nmemb || !size) {
        return NULL;
    }

    /* get whole size of new block, allocate it and set all data to 0 */
    size_t whole_size = nmemb * size;
    void *ptr = my_malloc(whole_size);
    zero_out(ptr, whole_size);
    return ptr;
}

void my_free(void* ptr) {
    ERRNO = NO_ERROR;
    if (!ptr) {
        return;
    }

    /* get pointer to the beginning of the block and calculate its real canary */
    metadata_t *block_pointer = (metadata_t *) ((char *) ptr - sizeof(metadata_t));
    unsigned int canary = CANARY_CALC(block_pointer->block_size, block_pointer->request_size, (unsigned int) (uintptr_t) block_pointer); 

    /* check if block's written 2 canary values match the real canary
     * if wrong, set the error type to corrupted canary
     */
    if (canary != block_pointer->canary || canary != *(unsigned int *) ((char *) block_pointer + block_pointer->request_size + sizeof(metadata_t))) {
        ERRNO = CANARY_CORRUPTED;
        return;
    } 

    /* block pointers to traverse the list */
    metadata_t *curr = freelist;
    metadata_t *prev = NULL;
    
    /* find location in the freelist to add the removed block */
    while (curr && (uintptr_t) block_pointer > (uintptr_t) curr) {
        prev = curr;
        curr = curr->next;
    }

    /* if the removed block is before the beginning of the freelist,
     * add to front of freelist
     */
    if (!prev) {
        block_pointer->next = freelist;
        freelist = block_pointer;
    } else if (((char *) prev + prev->block_size) == (char *) block_pointer) {
        /* if after the block to the left goes the removed block in memory,
         * combine them into one
         */
        prev->block_size += block_pointer->block_size;
        block_pointer = prev;
    } else {
        /* otherwise just set the next pointer of the left block to the removed one */
        prev->next = block_pointer;
    }

    /* 
     * if the block to the right of the removed block goes right after the removed block,
     * combine them into one
     */
    if (((char *) block_pointer + block_pointer->block_size) == (char *) curr) {
        block_pointer->next = curr->next;
        block_pointer->block_size += curr->block_size;
    } else {
        /* else just make the removed block point next to the right block */
        block_pointer->next = curr;
    }
}
