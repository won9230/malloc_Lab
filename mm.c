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
#define DSIZE 8     //더블 워드 크기 
#define CHUNKSIZE (1 << 12) //확장을 위한 기본 크기

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
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));     //직전에 가용 블럭이 있는지 확인
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));     //직후에 가용 블럭이 있는지 확인
    size_t size = GET_SIZE(HDRP(bp));   

    if(prev_alloc && next_alloc)    //가용블록이 없으면
    {
        last_bp = bp;
        return bp;  //지금 메모리 영역을 반환
    }
    else if(prev_alloc && !next_alloc)      //직후에 가용 블럭이 있으면
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));  //직후 사이즈를 늘린다.
        PUT(HDRP(bp),PACK(size,0));     //이미 여기서 footer가 직후 블록의 footer로 변경된다
        PUT(FTRP(bp),PACK(size,0));     //직후 블록 footer 변경
    }
    else if(!prev_alloc && next_alloc)      //직전에 가용 블록이 있으면
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));  //사이즈를 늘린다
        PUT(FTRP(bp),PACK(size,0));     //해당블럭 footer
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));  //직후 블록 header
        bp = PREV_BLKP(bp); //블록 포인터를 직전 블록으로 옮긴다.
    }
    else    //직전,직후에 가용블록이 있으면
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));  //양쪽으로 사이즈를 늘린다.
        PUT(HDRP(PREV_BLKP(bp)),PACK(size , 0));    //직전 블럭 header
        PUT(FTRP(NEXT_BLKP(bp)),PACK(size , 0));    //직후 블럭 footer
        bp = PREV_BLKP(bp); //블록 포인터를 직전 블록으로 옮긴다.
    }
    last_bp = bp;
    //최종 가용 블록의 주소를 리턴
    return bp;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    //짝수 word 할당으로 정렬 유지
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;   //size를 짝수 word && byte 형태로 만든다
    if((long)(bp = mem_sbrk(size)) == -1)   // 새 메모리의 첫 부분을 bp로 둔다. 주소값은 int로는 못 받아서 long으로 casting함
        return NULL;
    
    //새 가용 블록의 header와 footer를 정해주고 epilogue block을 가용 블록 맨 끝으로 옮긴다.
    PUT(HDRP(bp),PACK(size,0));     //헤더. 할당 안 해줬으므로 0으로.
    PUT(FTRP(bp),PACK(size,0));     //풋터
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0,1));     //새 에필로그 헤더

    //이전 블록이 사용 가능한 경우 병합
    return coalesce(bp);
}

static void *find_fit(size_t asize)     //first fit 방식
{

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
    char *bp = last_bp; //마지막에 저장한 블록 포인터를 가져온다

    for (bp = NEXT_BLKP(bp);  GET_SIZE(HDRP(bp)) != 0; bp = NEXT_BLKP(bp))
    {
        if(!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= aszie) //가용블럭의 크기가 필요한 크기보다 크면
        {
            last_bp = bp;   
            return bp;  //그 블럭을 할당해준다
        }
    }
    bp = heap_listp;    //만약 중간 부터 찾았는데 없으면 처음부터 다시 했는다
    while(bp < last_bp)
    {
        bp = NEXT_BLKP(bp);
        if(!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= aszie)
        {
            last_bp = bp;   //조건에 맞는 가용블록이 있으면 리턴
            return bp;
        }
    }
    return NULL;    //조건에 맞는 가용블록이 없으면 NULL를 리턴한다.
}

static void *place(void *bp,size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));  //현재 할당할 수 있는 후보 가용 블록의 주소
    if((csize - asize) >= (2*DSIZE))    //분할이 가능한경우
    {
        PUT(HDRP(bp),PACK(asize,1));    //앞의 블록은 할당 블록으로
        PUT(FTRP(bp),PACK(asize,1));
        //뒤의 블록은 가용 블록으로 분할한다.
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp),PACK(csize-asize,0));
        PUT(FTRP(bp),PACK(csize-asize,0));
    }
    else    //분할 못하면 남은 부분은 padding
    {
        PUT(HDRP(bp),PACK(csize,1));
        PUT(FTRP(bp),PACK(csize,1));
    }
    return 0;
}

int mm_init(void)
{
    //초기 빈 힙을 생성합니다.
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *) - 1) //유효하지 않은 메모리 값을 반환하면 return -1
        return -1;
    PUT(heap_listp,0);  //미사용 패딩(8의 배수로 정렬하기위해 사용)
    PUT(heap_listp + (1 * WSIZE),PACK(DSIZE, 1));   //프롤로그 헤더
    PUT(heap_listp + (2 * WSIZE),PACK(DSIZE, 1));   //프롤로그 풋터
    PUT(heap_listp + (3 * WSIZE),PACK(0, 1));   //에필로그 헤더
    heap_listp += (2*WSIZE);    //프롤로그해더에 heap_listp를 맞춘다

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
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE); //8의 배수를 맞춰서 asize를 조정

    //무료 목록을 검색하여 적합성 확인
    // if((bp = find_fit(asize)) != NULL) //first_fit 사용
    // {
    //     place(bp,asize);
    //     return bp;
    // }

    //next_fit 사용
    if((bp = next_fit(asize)) != NULL)  //알맞은 가용 블록의 주소를 리턴
    {
        place(bp,asize);    //필요하면 분할해서 할당한다.
        last_bp = bp;
        return bp;
    }

    //크기가 없는 가용 블록이 없다면 새로 더 많은 메모리를 확보하고 블록 배치
    extendsize = MAX(asize,CHUNKSIZE);  //둘 중 더 큰 값으로 사이즈를 정한다.
    if((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    // 새 힙에 메모리를 할당한다.
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
    //해당 블록의 size를 알아내 header와 footer의 정보를 수정한다.
    size_t size = GET_SIZE(HDRP(ptr));

    //header와 footer를 설정
    PUT(HDRP(ptr),PACK(size,0));
    PUT(FTRP(ptr),PACK(size,0));
    //만약 앞뒤의 블록이 가용 상태라면 연결한다.
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr; //크기를 조절하고 싶은 힙의 시작 포인터
    void *newptr;   //크기 조절 뒤의 새 힙의 시작 포인터
    size_t copySize;    //복사할 힙의 크기
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    //copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    copySize = GET_SIZE(HDRP(ptr)) - DSIZE;

    //원래 메모리 크기보다 적은 크기를 realloc하면
    //크기에 맞는 메모리만 할당되고 나머지는 안 된다.
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}