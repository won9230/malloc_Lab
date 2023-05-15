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

//=====================가용 리스트 조작을 위한 기본 상수 및 매크로 정의======================
//기본 상수 및 매크로
#define WSIZE 4     //워드 크기
#define DSIZE 8     //더블 크기 
#define CHUNKSIZE (1 << 12) //호가장을 위한 기본 크기

#define MAX(x, y) ((x) > (y) ? (x) : (y))

//단어에 크기와 할당된 비트를 입력합니다.
#define PACK(size, alloc) ((size) | (alloc))    //크기와 할당 비트를 통합해서 헤더와 풋터에 저장할 수 있는 값을 리턴

//주소p에서 크기 및 할당된 파일 필드 읽기
#define GET(p) (*(unsigned int *)(p))   //p(포인터)가 참조하는 워드를 읽어서 리턴
#define PUT(p, val) (*(unsigned int *)(p) = (val))  //p가 가르키는 워드를 val에 저장

//주소 p에서 크기 및 할당된 필드 읽기
#define GET_SIZE(p) (GET(p) & ~0x7)     //헤더 or 풋터의 size를 리턴한다.
#define GET_ALLOC(p) (GET(p) & 0x1)     //헤더 or 풋터의 할당 비트를 리턴한다

//주어진 블록 포인터 bp로부터 해당 블록의 헤더와 푸터의 주소를 계산하세요.
#define HDRP(bp) ((char *)(bp) - WSIZE)     //헤더를 가르키는 포인터를 리턴
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)    //풋터를 가르키는 포인터를 리턴

//주어진 블록 포인터(bp)를 이용하여, 다음과 이전 블록의 주소를 계산하세요.
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))     //다음 블록의 포인터를 리턴
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))     //이전 블록의 포인터를 리턴

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "team 1",
    /* First member's full name */
    "chel won",
    /* First member's email address */
    "won9230@naver.com",
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
static void *heap_listp;
static char *last_bp;
/* 
 * mm_init - initialize the malloc package.
 */

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if(prev_alloc && next_alloc)
    {
        last_bp = bp;
        return bp;
    }
    else if(prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp),PACK(size,0));
        PUT(FTRP(bp),PACK(size,0));
    }
    else if(!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp),PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
        bp = PREV_BLKP(bp);
    }
    else
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size , 0));
        PUT(FTRP(NEXT_BLKP(bp)),PACK(size , 0));
        bp = PREV_BLKP(bp);
    }
    last_bp = bp;
    return bp;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    //짝수 단어 할당으로 정렬 유지
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    //비어있는 블록의 헤더와 footer 그리고 에필로그 헤더를 초기화합니다.
    PUT(HDRP(bp),PACK(size,0));
    PUT(FTRP(bp),PACK(size,0));
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0,1));

    //이전 블록이 사용 가능한 경우 병합
    return coalesce(bp);
}

static void *find_fit(size_t asize)
{
    // void *bp= mem_heap_lo() + 2 * WSIZE;
    // while (GET_SIZE(HDRP(bp))>0)
    // {
    //     if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
    //         return bp;
    //     bp = NEXT_BLKP(bp);
    // }
    // return NULL;
    void *bp;

    for(bp = heap_listp;GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            return bp;
        }
    }
    return NULL;
    //#endif
}

static void *next_fit(size_t aszie)
{
    char *bp = last_bp;

    for (bp = NEXT_BLKP(bp);  GET_SIZE(HDRP(bp)) != 0; bp = NEXT_BLKP(bp))
    {
        if(!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= aszie)
        {
            last_bp = bp;
            return bp;
        }
    }
    bp = heap_listp;
    while(bp < last_bp)
    {
        bp = NEXT_BLKP(bp);
        if(!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= aszie)
        {
            last_bp = bp;
            return bp;
        }
    }
    return NULL;
}

static void *place(void *bp,size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    if((csize - asize) >= (2*DSIZE))
    {
        PUT(HDRP(bp),PACK(asize,1));
        PUT(FTRP(bp),PACK(asize,1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp),PACK(csize-asize,0));
        PUT(FTRP(bp),PACK(csize-asize,0));
    }
    else
    {
        PUT(HDRP(bp),PACK(csize,1));
        PUT(FTRP(bp),PACK(csize,1));
    }
    return 0;
}

int mm_init(void)
{
    //초기 빈 힙을 생성합니다.
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *) - 1)
        return -1;
    PUT(heap_listp,0);
    PUT(heap_listp + (1 * WSIZE),PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE),PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE),PACK(0, 1));
    heap_listp += (2*WSIZE);

    //CHUNKSIZE 바이트의 빈 블록으로 빈힙을 확장하세요
    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    last_bp = (char *)heap_listp; //next_fit 추가
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;   //수정된 블록 크기
    size_t extendsize;  //적합하지 않은 경우 힙을 확장할 양
    char *bp;

    //가상 요청 무시
    if(size == 0)
        return NULL;
    
    //오버헤드 및 정렬 요청을 포함하도록 블록 크기 조정
    if(size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    //무료 목록을 검색하여 적합성 확인
    // if((bp = find_fit(asize)) != NULL) //first_fit 사용
    // {
    //     place(bp,asize);
    //     return bp;
    // }

    //next_fit 사용
    if((bp = next_fit(asize)) != NULL)
    {
        place(bp,asize);
        last_bp = bp;
        return bp;
    }

    //적합치를 찾을 수 없습니다. 더 많은 메모리를 확보하고 블록 배치
    extendsize = MAX(asize,CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp,asize);
    last_bp = bp;
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
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr),PACK(size,0));
    PUT(FTRP(ptr),PACK(size,0));
    coalesce(ptr);
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
    //copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    copySize = GET_SIZE(HDRP(ptr)) - DSIZE;
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}