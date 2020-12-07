/*
 * mm.c using segregated lists
 * equivalence classes are 2^k
 * It is based on textbook's segregated fits scheme
 * In addition, one uses flags on reallocated blocks
 * to avoid fragmentation
 * Useful resource: https://www.cs.cmu.edu/afs/cs/academic/class/15213-s16/www/recitations/recitation11-malloc.pdf
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "SAGSANG",
    /* First member's full name */
    "SANGOUARD Ronan",
    /* First member's email address */
    "ronan.sangouard@polytechnique.edu",
    /* Second member's full name (leave blank if none) */
    "SAGBO Mahoutin Ralph Philippe",
    /* Second member's email address (leave blank if none) */
    "mahoutin-ralph-philippe.sagbo@polytechnique.edu"};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define WSIZE 4 // word and header/footer size (bytes)
#define DSIZE 8 // double word size (bytes)
#define INITCHUNKSIZE (1 << 6)
#define CHUNKSIZE (1 << 12)

#define REALLOC_BUFFER (1 << 7)

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* copy a pointer value */
#define COPY_PTR(p, ptr) (*(unsigned int *)(p) = (unsigned int)(ptr))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of bits header and footer */
#define HDRP(ptr) ((char *)(ptr)-WSIZE)
#define FTRP(ptr) ((char *)(ptr) + GET_SIZE(HDRP(ptr)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(ptr) ((char *)(ptr) + GET_SIZE((char *)(ptr)-WSIZE))
#define PREV_BLKP(ptr) ((char *)(ptr)-GET_SIZE((char *)(ptr)-DSIZE))

/* Given block ptr bp, compute address of predecessor and successor entries */
#define PRED_PTR(ptr) ((char *)(ptr))
#define SUCC_PTR(ptr) ((char *)(ptr) + WSIZE)

/* Given block ptr bp, compute address of predecessor and successor in the segregated list */
#define PRED(ptr) (*(char **)(ptr))
#define SUCC(ptr) (*(char **)(SUCC_PTR(ptr)))

/* Use flag for reallocation */
#define IS_FLAGGED(p) (GET(p) & 0x2)
#define FLAG(p) (GET(p) |= 0x2)
#define UNFLAG(p) (GET(p) &= ~0x2)
#define PUT_USING_FLAG(p, val) (*(unsigned int *)(p) = (val) | IS_FLAGGED(p))

/* segregated free lists */
#define NUM_LISTS 20
void *segregated_free_lists[NUM_LISTS];

// Functions

int mm_check(void *bottom, char useExplicitFreeList, char verbose)
{
    char status = 1;
    void *bp;

    printf("\nmm check\n\n==========\n\n");

    /*Is every block in the free list marked as free?*/
    if (useExplicitFreeList)
    {
    }

    /*Are there any contiguous free blocks that somehow escaped coalescing?*/
    char previousBlockWasFree = 0;
    printf("Are there any contiguous free blocks that somehow escaped coalescing?\n\n");
    for (bp = bottom; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (verbose)
        {
            printf("Block address: %p ; Free: %d\n", bp, !GET_ALLOC(HDRP(bp)));
        }

        if (!GET_ALLOC(HDRP(bp)) && previousBlockWasFree && GET_SIZE(HDRP(NEXT_BLKP(bp))) > 0)
        {
            // Notice that bp is not the footer
            printf("Two contiguous free blocks escaped coalescing\n");
            status = 0;
        }

        previousBlockWasFree = !GET_ALLOC(HDRP(bp));
    }
    printf("\n==========\n\n");

    /*Is every free block actually in the free list?*/
    if (useExplicitFreeList)
    {
    }

    /*Do the pointers in the free list point to valid free blocks?*/
    if (useExplicitFreeList)
    {
    }

    /*Do any allocated blocks overlap?*/
    printf("Do any allocated blocks overlap?\n\n");
    for (bp = bottom; GET_SIZE(HDRP(NEXT_BLKP(bp))) > 0; bp = NEXT_BLKP(bp))
    {
        if (verbose)
        {
            printf("Block address: %p ; Block size: %d ; Next block: %p\n",
                   bp, GET_SIZE(HDRP(bp)), NEXT_BLKP(bp));
        }
        if (bp + GET_SIZE(HDRP(bp)) != NEXT_BLKP(bp))
        {
            printf("Overlap found\n");
            status = 0;
        }
    }
    printf("\n==========\n\n");

    /*Do the pointers in a heap block point to valid heap addresses?*/

    return status;
}

static int max(int a, int b)
{
    return (a < b) ? b : a;
}

static int min(int a, int b)
{
    return (a < b) ? a : b;
}

static int find_list(size_t size)
{
    int list = 0;
    while ((list < NUM_LISTS - 1) && (size > 1))
    {
        size >>= 1;
        list++;
    }
    return list;
}

static void add_free_block(void *ptr, size_t size)
{
    void *search_ptr = ptr;
    void *insert_ptr = NULL;

    // find list index
    int list = find_list(size);

    // Keep size ascending order and search
    search_ptr = segregated_free_lists[list];
    while ((search_ptr != NULL) && (size > GET_SIZE(HDRP(search_ptr))))
    {
        insert_ptr = search_ptr;
        search_ptr = PRED(search_ptr);
    }

    // Set predecessor and successor
    // four possible configurations
    if (search_ptr != NULL)
    {
        if (insert_ptr != NULL)
        {
            COPY_PTR(PRED_PTR(ptr), search_ptr);
            COPY_PTR(SUCC_PTR(search_ptr), ptr);
            COPY_PTR(SUCC_PTR(ptr), insert_ptr);
            COPY_PTR(PRED_PTR(insert_ptr), ptr);
        }
        else
        {
            COPY_PTR(PRED_PTR(ptr), search_ptr);
            COPY_PTR(SUCC_PTR(search_ptr), ptr);
            COPY_PTR(SUCC_PTR(ptr), NULL);
            segregated_free_lists[list] = ptr;
        }
    }
    else
    {
        if (insert_ptr != NULL)
        {
            COPY_PTR(PRED_PTR(ptr), NULL);
            COPY_PTR(SUCC_PTR(ptr), insert_ptr);
            COPY_PTR(PRED_PTR(insert_ptr), ptr);
        }
        else
        {
            COPY_PTR(PRED_PTR(ptr), NULL);
            COPY_PTR(SUCC_PTR(ptr), NULL);
            segregated_free_lists[list] = ptr;
        }
    }

    return;
}

static void rmv_free_block(void *ptr)
{
    // find the good list index
    int list = find_list(GET_SIZE(HDRP(ptr)));

    // four possible configurations
    if (PRED(ptr) != NULL)
    {
        if (SUCC(ptr) != NULL)
        {
            COPY_PTR(SUCC_PTR(PRED(ptr)), SUCC(ptr));
            COPY_PTR(PRED_PTR(SUCC(ptr)), PRED(ptr));
        }
        else
        {
            COPY_PTR(SUCC_PTR(PRED(ptr)), NULL);
            segregated_free_lists[list] = PRED(ptr);
        }
    }
    else
    {
        if (SUCC(ptr) != NULL)
        {
            COPY_PTR(PRED_PTR(SUCC(ptr)), NULL);
        }
        else
        {
            segregated_free_lists[list] = NULL;
        }
    }

    return;
}

static void *coalesce(void *ptr)
{
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(ptr)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t size = GET_SIZE(HDRP(ptr));

    if (IS_FLAGGED(HDRP(PREV_BLKP(ptr))))
        // if flagged, considered as allocated
        prev_alloc = 1;

    // alloc -- ptr -- alloc
    if (prev_alloc && next_alloc)
    {
        return ptr;
    }
    // alloc -- ptr -- no alloc
    else if (prev_alloc && !next_alloc)
    {
        rmv_free_block(ptr);
        rmv_free_block(NEXT_BLKP(ptr));
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        PUT_USING_FLAG(HDRP(ptr), PACK(size, 0));
        PUT_USING_FLAG(FTRP(ptr), PACK(size, 0));
    }
    // no alloc -- ptr -- alloc
    else if (!prev_alloc && next_alloc)
    {
        rmv_free_block(ptr);
        rmv_free_block(PREV_BLKP(ptr));
        size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
        PUT_USING_FLAG(FTRP(ptr), PACK(size, 0));
        PUT_USING_FLAG(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        ptr = PREV_BLKP(ptr);
    }
    // no alloc -- ptr -- no alloc
    else
    {
        rmv_free_block(ptr);
        rmv_free_block(PREV_BLKP(ptr));
        rmv_free_block(NEXT_BLKP(ptr));
        size += GET_SIZE(HDRP(PREV_BLKP(ptr))) + GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        PUT_USING_FLAG(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        PUT_USING_FLAG(FTRP(NEXT_BLKP(ptr)), PACK(size, 0));
        ptr = PREV_BLKP(ptr);
    }

    add_free_block(ptr, size);

    return ptr;
}

static void *extend_heap(size_t size)
{
    void *ptr;
    size_t aligned_size = ALIGN(size);

    if ((ptr = mem_sbrk(aligned_size)) == (void *)-1)
        return NULL;

    // Set headers and footer
    PUT(HDRP(ptr), PACK(aligned_size, 0));
    PUT(FTRP(ptr), PACK(aligned_size, 0));
    PUT(HDRP(NEXT_BLKP(ptr)), PACK(0, 1));
    add_free_block(ptr, aligned_size);

    return coalesce(ptr);
}

static void *place(void *ptr, size_t aligned_size)
{
    size_t ptr_size = GET_SIZE(HDRP(ptr));

    rmv_free_block(ptr);

    size_t slack = ptr_size - aligned_size;
    if (slack <= DSIZE * 2)
    {
        PUT_USING_FLAG(HDRP(ptr), PACK(ptr_size, 1));
        PUT_USING_FLAG(FTRP(ptr), PACK(ptr_size, 1));
    }

    else if (aligned_size >= 100)
    {
        PUT_USING_FLAG(HDRP(ptr), PACK(slack, 0));
        PUT_USING_FLAG(FTRP(ptr), PACK(slack, 0));
        PUT(HDRP(NEXT_BLKP(ptr)), PACK(aligned_size, 1));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(aligned_size, 1));
        add_free_block(ptr, slack);
        return NEXT_BLKP(ptr);
    }

    else
    {
        PUT_USING_FLAG(HDRP(ptr), PACK(aligned_size, 1));
        PUT_USING_FLAG(FTRP(ptr), PACK(aligned_size, 1));
        PUT(HDRP(NEXT_BLKP(ptr)), PACK(slack, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(slack, 0));
        add_free_block(NEXT_BLKP(ptr), slack);
    }
    return ptr;
}

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty segragated list */
    int list;
    for (list = 0; list < NUM_LISTS; list++)
    {
        segregated_free_lists[list] = NULL;
    }

    /* Create the initial empty heap */
    char *heap_listp;
    if ((long)(heap_listp = mem_sbrk(4 * WSIZE)) == -1)
        return -1;

    PUT(heap_listp, 0);                            /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     /* Epilogue header */

    if (extend_heap(INITCHUNKSIZE) == NULL)
        return -1;

    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    if (size == 0)
        return NULL;

    void *ptr = NULL;

    // aligned block size
    size_t aligned_size;
    if (size <= DSIZE)
    {
        aligned_size = 2 * DSIZE;
    }
    else
    {
        aligned_size = ALIGN(size + DSIZE);
    }

    int index = 0;
    size_t search_size = aligned_size;
    // Search the good free list
    while (index < NUM_LISTS)
    {
        if ((index == NUM_LISTS - 1) || ((search_size <= 1) && (segregated_free_lists[index] != NULL)))
        {
            ptr = segregated_free_lists[index];
            while ((ptr != NULL) && ((aligned_size > GET_SIZE(HDRP(ptr))) || (IS_FLAGGED(HDRP(ptr)))))
            {
                // too small or flagged
                ptr = PRED(ptr);
            }
            if (ptr != NULL)
                break;
        }

        search_size >>= 1;
        index++;
    }

    if (ptr == NULL)
    {
        // must extend heap
        size_t extend_size = max(aligned_size, CHUNKSIZE);

        if ((ptr = extend_heap(extend_size)) == NULL)
            return NULL;
    }

    ptr = place(ptr, aligned_size);

    return ptr;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    UNFLAG(HDRP(NEXT_BLKP(ptr)));
    PUT_USING_FLAG(HDRP(ptr), PACK(size, 0));
    PUT_USING_FLAG(FTRP(ptr), PACK(size, 0));

    add_free_block(ptr, size);
    coalesce(ptr);

    return;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (size == 0)
        return NULL;

    // Pointer that will be returned
    void *new_ptr = ptr;

    // Align block size
    size_t aligned_size = size;
    if (aligned_size <= DSIZE)
    {
        aligned_size = 2 * DSIZE;
    }
    else
    {
        aligned_size = ALIGN(size + DSIZE);
    }

    // Add margin
    aligned_size += REALLOC_BUFFER;

    // Calculate overhead
    int overhead_size = GET_SIZE(HDRP(ptr)) - aligned_size;

    if (overhead_size < 0)
    {
        // must allocate more space
        if (!GET_ALLOC(HDRP(NEXT_BLKP(ptr))) || !GET_SIZE(HDRP(NEXT_BLKP(ptr))))
        {
            // next block is a free block or the epilogue block
            int slack = GET_SIZE(HDRP(ptr)) + GET_SIZE(HDRP(NEXT_BLKP(ptr))) - aligned_size;
            if (slack < 0)
            {
                int extend_size = max(-slack, CHUNKSIZE);
                if (extend_heap(extend_size) == NULL)
                    return NULL;
                slack += extend_size;
            }

            rmv_free_block(NEXT_BLKP(ptr));

            // Do not split block
            PUT(HDRP(ptr), PACK(aligned_size + slack, 1));
            PUT(FTRP(ptr), PACK(aligned_size + slack, 1));
        }
        else
        {
            new_ptr = mm_malloc(aligned_size - DSIZE);
            memcpy(new_ptr, ptr, min(size, aligned_size));
            mm_free(ptr);
        }
        overhead_size = GET_SIZE(HDRP(new_ptr)) - aligned_size;
    }

    // Tag the next block if overhead size < twice the margin
    if (overhead_size < 2 * REALLOC_BUFFER)
        FLAG(HDRP(NEXT_BLKP(new_ptr)));

    return new_ptr;
}
