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
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
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

int mm_init(void) {
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)               
        return -1;                                              
        
    PUT(heap_listp, 0);                                                     
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));                          
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));                          
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));                          
    
    heap_listp += (2 * WSIZE);                         
    
    #ifdef NEXT_FIT
        last_freep = heap_listp;                             
    #endif
    
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {                     
        return -1;
    }
    
    return 0;
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    size_t asize;                                           
    size_t extendsize;           
    char *bp;
    
    if (size == 0) {
        return NULL;                            
    }
    
    asize = ALIGN(size + SIZE_T_SIZE);                      
    
    // 적절한 공간을 가진 블록을 찾으면 할당(혹은 분할까지) 진행한다.
    // bp는 계속 free 블록을 가리킬 수 있도록 한다.
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }
    
    // 적절한 공간을 찾지 못했다면 힙을 늘려주고, 그 늘어난 공간에 할당시켜야 한다.
    extendsize = MAX(asize, CHUNKSIZE);                                     // 둘 중 더 큰 값을 선택한다.
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) {                   // 실패 시 bp로는 NULL을 반환한다.
        return NULL;
    }
    
    // 힙을 늘리는 데에 성공했다면, 그 늘어난 공간에 할당시킨다.
    place(bp, asize);
    return bp;
}
/*
 *  extend_heap - word 단위의 메모리를 인자로 받아 힙을 늘려준다.  
 */
static void* extend_heap(size_t words) {
    char* bp;
    size_t size;
    
    size = (words % 2 == 1) ? (words + 1) * WSIZE : (words) * WSIZE;        // words가 홀수로 들어왔다면 짝수로 바꿔준다. 짝수로 들어왔다면 그대로 WSIZE를 곱해준다. ex. 5만큼(5개의 워드 만큼) 확장하라고 하면, 6으로 만들고 24바이트로 만든다. 
                                                                            // 8바이트(2개 워드, 짝수) 정렬을 위해 짝수로 만들어줘야 한다.
    
    if ((long)(bp = mem_sbrk(size)) == -1) {                                // 변환한 사이즈만큼 메모리 확보에 실패하면 NULL이라는 주소값을 반환해 실패했음을 알린다. bp 자체의 값, 즉 주소값이 32bit이므로 long으로 캐스팅한다.
        return NULL;                                                        // 그리고 mem_sbrk 함수가 실행되므로 bp는 새로운 메모리의 첫 주소값을 가르키게 된다.
    }              
    
    // 새 free 블록의 header와 footer를 정해준다. 자연스럽게 전 epilogue 자리에는 새로운 header가 자리 잡게 된다. 그리고 epilogue는 맨 뒤로 보내지게 된다.
    PUT(HDRP(bp), PACK(size, 0));                                           // 새 free 블록의 header로, free 이므로 0을 부여
    PUT(FTRP(bp), PACK(size, 0));                                           // 새 free 블록의 footer로, free 이므로 0을 부여
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));                                   // 앞에서 현재 bp(새롭게 늘어난 메모리의 첫 주소 값으로 역시 payload이다)의 header에 값을 부여해주었다. 따라서 이 header의 사이즈 값을 참조해 다음 블록의 payload를 가르킬 수 있고, 이 payload의 직전인 header는 epilogue가 된다.
    
    return coalesce(bp);                                                    // 앞 뒤 블록이 free 블록이라면 연결하고 bp를 반환한다.
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp) {
    size_t size = GET_SIZE(HDRP(bp));                                       // bp가 가리키는 블록의 사이즈만 들고 온다.
    
    // header, footer 둘 다 flag를 0으로 바꿔주면 된다.
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));                      
    
    coalesce(bp);                                                           // 앞 뒤 블록이 free 블록이라면 연결한다.                   
}


/*
 * coalesce - 앞 혹은 뒤 블록이 free 블록이고, 현재 블록도 free 블록이라면 연결시키고 연결된 free 블록의 주소를 반환한다.
 */ 
static void* coalesce(void* bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));                     // 이전 블록의 free 여부
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));                     // 다음 블록의 free 여부
    size_t size = GET_SIZE(HDRP(bp));                                       // 현재 블록의 사이즈
    
    // 경우 1. 이전 블록 할당, 다음 블록 할당 - 연결시킬 수 없으니 그대로 bp를 반환한다.
    if (prev_alloc && next_alloc) {
        return bp;
    }
    
    // 경우 2. 이전 블록 할당, 다음 블록 free - 다음 블록과 연결시키고 현재 bp를 반환하면 된다.
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));                              // 다음 블록의 header에 저장된 다음 블록의 사이즈를 더해주면 연결된 만큼의 사이즈가 나온다.
        PUT(HDRP(bp), PACK(size, 0));                                       // 현재 블록의 header에 새로운 header가 부여된다.
        PUT(FTRP(bp), PACK(size, 0));                                       // 다음 블록의 footer에 새로운 footer가 부여된다.
    }
    
    // 경우 3. 이전 블록 free, 다음 블록 할당 - 이전 블록과 연결시키고 이전 블록을 가리키도록 bp를 바꾼다.
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));                                       // 현재 블록의 footer에 새로운 footer를 부여한다.
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));                            // 이전 블록의 header에 새로운 header를 부여한다.
        bp = PREV_BLKP(bp);                                                 // bp가 이전 블록을 가리키도록 한다.
    }

        // 경우 4. 이전 블록 free, 다음 블록 free - 모두 연결한 후 이전 블록을 가리키도록 bp를 바꾼다.
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));  // 이전 블록의 header에서 사이즈를, 다음 블록의 footer에서 사이즈를 읽어와 size를 더한다.
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));                            // 이전 블록의 header에 새로운 header를 부여한다.
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));                            // 다음 블록의 footer에 새로운 footer를 부여한다.
        bp = PREV_BLKP(bp);
    }
    
    #ifdef NEXT_FIT
        last_freep = bp;
    #endif
    
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














