/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
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
    "ateam",
    /* First member's full name */
    "Kim Keon woo",
    /* First member's email address */
    "orcasuit@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
/* 
 * mm_init - initialize the malloc package.
 */

/*기본 상수들*/
#define WSIZE   4   /* 워드와 헤더/풋터 사이즈 (bytes) */
#define DSIZE   8   /* 더블 워드 사이즈 (bytes) */
#define CHUNKSIZE (1<<12) /* 힙을 이만큼 확장 (bytes) */

#define MAX(x, y)  ((x) > (y)? (x) : (y))

/* 크기와 할당된 비트를 한 단어에 담습니다. */
#define PACK(size, alloc) ((size) | (alloc))

/* 주소 p에서 워드 읽고 쓰기 */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int*)(p) = (val))

/* 주소 p에서 크기와 할당된 필드를 읽는다. */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* 블록 ptr bp가 주어지면 헤더와 풋터의 주소를 계산 */
#define HDRP(bp)    ((char*)(bp) - WSIZE)
#define FTRP(bp)    ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* 블록 ptr bp가 주어지면, 다음 블록과 이전 블록의 주소를 계산 */
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(((char*)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE)))

/* 새 가용 블록으로 힙 확장하기 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* 정렬을 유지하기 위해 짝수 개의 단어를 할당 */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    /* 프리 블록 머리글/바닥글 및 에필로그 머리글 초기화하기 */
    PUT(HDRP(bp), PACK(size, 0)); /* Free block header */
    PUT(FTRP(bp), PACK(size, 0)); /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

    /* 이전 블록이 비어있는 경우 합치기 */
    return coalesce(bp);
}


/* 최초 가용 블록으로 힙 생성하기 */
static char* heap_listp = 0;
int mm_init(void)
{
    /* 초기 빈 힙 생성 */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));
    heap_listp += (2*WSIZE);

    /* 빈 힙을 CHUNKSIZE 바이트의 여유 블록으로 확장합니다.*/
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}


/*주어진 크기 `asize` 에 맞는 가용블록을 찾아 탐색 */
static void *find_fit(size_t asize){
    void* bp;

    /* 가용 리스트의 시작부터 끝까지 순차적으로 탐색 */
    for (bp = heap_listp; GET_SIZE(HDRP(bp))> 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    return NULL; /* 적합한 블록을 찾지 못함 */
}

/*할당하려는 메모리 크기에 맞게 가용 블록을 조정하고 할당.*/
static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp)); // 현재 블록의 크기를 얻음

    // 현재 블록 크기가 요청 크기 + 최소 블록 크기보다 크거나 같으면 분할
    if ((csize - asize) >= (2*DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1)); // 현재 블록에 asize 크기로 할당
        bp = NEXT_BLKP(bp); // 새로운 가용 블록의 시작 위치로 이동
        PUT(HDRP(bp), PACK(csize-asize, 0)); // 새로운 가용 블록 생성
        PUT(FTRP(bp), PACK(csize-asize, 0)); // 새로운 가용 블록의 풋터 설정
    }
    // 분할할 필요가 없는 경우
    else {
        PUT(HDRP(bp), PACK(csize, 1)); // 전체 블록을 할당 상태로 변경
        PUT(FTRP(bp), PACK(csize, 1)); // 풋터도 할당 상태로 변경
    }
}



/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */

/*가용 리스트에서 블록할당*/
void *mm_malloc(size_t size)
{
    size_t asize; /* 블록 크기 조정 */
    size_t extendsize; /* 적합하지 않은 경우 힙을 확장하는 양 */
    char *bp;

    /* 허위 요청 무시 */
    if(size == 0)
        return NULL;

    /* 오버헤드 및 정렬 요구 사항을 포함하도록 블록 크기를 조정 */
    if(size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE) + (DSIZE-1)) / DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* 적합한 항목을 찾을 수 없음, 메모리를 더 확보하고 블록 배치 */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;

    // int newsize = ALIGN(size + SIZE_T_SIZE);
    // void *p = mem_sbrk(newsize);
    // if (p == (void *)-1)
	// return NULL;
    // else {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }
}

/*
 * mm_free - Freeing a block does nothing.
 */
/* 블록을 반환하고 경계 태그 연결을 사용해서 상수 시간에 인접 가용 블록들과 통합*/
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

static void *coalesce(void* bp)
{
    size_t prev_alloc = GET_ALLOC(PTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {     /* Case 1 */
        return bp;
    }   

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc) { /* CASAE 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else {                                /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))); 
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
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
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














