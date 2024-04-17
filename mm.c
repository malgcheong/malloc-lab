/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.ㄷ
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
    "12jo",
    /* First member's full name */
    "NamCheongWoo",
    /* First member's email address */
    "skacjddn7@gmail.com",
    /* Second member's full name (leave blank if none) */
    "BangJiWon",
    /* Second member's email address (leave blank if none) */
    "haha71119@gmail.com"
};

#define WSIZE 4 /* Word and header/footer size (bytes) */
#define DSIZE 8 /* Double word size (bytes) */
#define CHUNKSIZE (1<<12) /* Extend heap by this amount (bytes) */
#define LISTLIMIT 50

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size,alloc) ((size) | (alloc)) 

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p,val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Given free block ptr bp, compute address of pre succesor and succesor blocks */
#define PRES_FREEP(bp) (*(void**)(bp))
#define SUCC_FREEP(bp) (*(void**)(bp + WSIZE))

/* Internal Function Prototype */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void add_freeblock(void *bp, size_t size);
static void delete_freeblock(void *bp);

/* Points to first byte of heap */
static char *heap_listp;
static char *segregation_list[LISTLIMIT];

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{

    for (int i = 0; i < LISTLIMIT; i++)
    {
        segregation_list[i] = NULL;
    }                               

    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                                 /* Aligment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));        /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));        /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));            /* Epilogue header */
    heap_listp += (2*WSIZE);

    if (extend_heap(4) == NULL) return -1;

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL){
        return -1;
    }
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *             Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;       /* Adjusted block size */
    size_t extendsize;  /* Amount to extend heap if no fit */
    char *bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp,asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

/*
 * extend_heap - Expand heap with new available block.
 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1){
        return NULL;
    }

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/*
 * coalesce - Integrate with adjacent available blocks
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc){                  /* Case 1 */
        add_freeblock(bp, size);
        return bp;
    }
    else if (prev_alloc && !next_alloc){            /* Case 2 */
        delete_freeblock(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) {           /* Case 3 */
        delete_freeblock(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else {                                          /* Case 4 */
        delete_freeblock(PREV_BLKP(bp));
        delete_freeblock(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
                GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    add_freeblock(bp, size);
    return bp;
}

/*
 * find_fit - Search the available list from scratch and select the first available block of size.
 */
static void *find_fit(size_t asize) {
    /* First-fit search */
    void *bp;
    int list = 0;
    size_t searchsize = asize;

    while (list < LISTLIMIT)
    {
        if ((list == LISTLIMIT - 1) || (searchsize <= 1) && (segregation_list[list] != NULL))
        {
            bp = segregation_list[list];

            while ((bp != NULL) && (asize > GET_SIZE(HDRP(bp))))
            {
                bp = SUCC_FREEP(bp);
            }
            if (bp != NULL)
            {
                return bp;
            }
        }
        searchsize >>= 1;
        list++;
    }

    return NULL;
}

/*
 * place - If there is enough space, the available space should be divided into two blocks and data should be allocated to only one side.
 *         If not, allocate all data to all available space.
 */
static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));

    delete_freeblock(bp);
    if ((csize - asize) >= (2*DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        coalesce(bp);
    } else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
 * add_freeblock - Add a free block to the free block list and set the last block to heap_listp
 */
static void add_freeblock(void *bp, size_t size)
{
    int list = 0;
    void *search_ptr;
    void *insert_ptr = NULL; 

    while ((list < LISTLIMIT - 1) && (size > 1))
    {
        size >>= 1;
        list++;
    }

    search_ptr = segregation_list[list];
    while ((search_ptr != NULL) && (size > GET_SIZE(HDRP(search_ptr))))
    {
        insert_ptr = search_ptr;
        search_ptr = SUCC_FREEP(search_ptr); 
    }

    if (search_ptr != NULL)
    { 
        if (insert_ptr != NULL)
        {                                
            SUCC_FREEP(bp) = search_ptr; 
            PRES_FREEP(bp) = insert_ptr;
            PRES_FREEP(search_ptr) = bp;
            SUCC_FREEP(insert_ptr) = bp;
        }
        else
        { 
            SUCC_FREEP(bp) = search_ptr;
            PRES_FREEP(bp) = NULL;
            PRES_FREEP(search_ptr) = bp;
            segregation_list[list] = bp; 
        }
    }
    else
    {
        if (insert_ptr != NULL)
        {
            SUCC_FREEP(bp) = NULL;       
            PRES_FREEP(bp) = insert_ptr; 
            SUCC_FREEP(insert_ptr) = bp;
        }
        else
        {
            SUCC_FREEP(bp) = NULL;
            PRES_FREEP(bp) = NULL;
            segregation_list[list] = bp;
        }
    }

    return;
}

/*
 * delete_freeblock - Delete one block from the free block list and link before and after the deleted block.
 */
static void delete_freeblock(void *bp)
{
    int list = 0;
    size_t size = GET_SIZE(HDRP(bp));

    while ((list < LISTLIMIT - 1) && (size > 1))
    { 
        size >>= 1;
        list++;
    }

    if (SUCC_FREEP(bp) != NULL)
    {
        if (PRES_FREEP(bp) != NULL)
        {
            PRES_FREEP(SUCC_FREEP(bp)) = PRES_FREEP(bp);
            SUCC_FREEP(PRES_FREEP(bp)) = SUCC_FREEP(bp);
        }
        else
        {
            PRES_FREEP(SUCC_FREEP(bp)) = NULL;
            segregation_list[list] = SUCC_FREEP(bp);
        }
    }
    else
    {
        if (PRES_FREEP(bp) != NULL)
        {
            SUCC_FREEP(PRES_FREEP(bp)) = NULL;
        }
        else
        {
            segregation_list[list] = NULL;
        }
    }
    return;
}