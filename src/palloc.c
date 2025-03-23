#include "palloc/palloc.h"

#if defined(PALLOC_CONFIG)
#   include PALLOC_CONFIG
#else
#   include <stdlib.h>
#   include <memory.h>
#   define PALLOC_STD_MALLOC(S) malloc(S)
#   define PALLOC_STD_FREE(P) free(P)
#   define PALLOC_STD_REALLOC(P, S) realloc(P, S)
#   define PALLOC_STD_MEMCPY(D, S, N) memcpy(D, S, N)

#   if defined(PALLOC_THREAD) && defined(PALLOC_LOCKFREE)
#       if defined(_MSC_VER)
#           include <intrin.h>

static void PALLOC_ATOMIC_STORE( void * volatile * p, void * v )
{
    _InterlockedExchangePointer( p, v );
}

static void * PALLOC_STD_ATOMIC_LOAD( void * volatile * p )
{
    void * o = _InterlockedCompareExchangePointer( p, NULL, NULL );

    return o;
}

static int PALLOC_STD_ATOMIC_COMPARE_EXCHANGE_WEAK( void * volatile * p, void ** e, void * d )
{
    void * o = _InterlockedCompareExchangePointer( p, d, *e );

    if( o == *e )
    {
        return 1;
    } 

    *e = o;

    return 0;
}

#       endif
#   elif defined(PALLOC_THREAD) && defined(PALLOC_MUTEX)
#       if defined(_MSC_VER)
#           include <Windows.h>

typedef CRITICAL_SECTION PALLOC_STD_MUTEX_T;

static void PALLOC_STD_MUTEX_INIT( PALLOC_STD_MUTEX_T * l )
{
    InitializeCriticalSection( l );
}

static void PALLOC_STD_MUTEX_FINI( PALLOC_STD_MUTEX_T * l )
{
    DeleteCriticalSection( l );
}

static void PALLOC_STD_MUTEX_LOCK( PALLOC_STD_MUTEX_T * l )
{
    EnterCriticalSection( l );
}

static void PALLOC_STD_MUTEX_UNLOCK( PALLOC_STD_MUTEX_T * l )
{
    LeaveCriticalSection( l );
}

#      endif
#   endif
#endif

#if defined(PALLOC_THREAD)
#   if !defined(PALLOC_LOCKFREE) && !defined(PALLOC_MUTEX)
#       error "PALLOC_THREAD requires PALLOC_LOCKFREE or PALLOC_MUTEX"
#   elif defined(PALLOC_LOCKFREE) && defined(PALLOC_MUTEX)
#       error "PALLOC_LOCKFREE and PALLOC_MUTEX are mutually exclusive"
#   endif
#endif

#if defined(PALLOC_THREAD) && defined(PALLOC_LOCKFREE)
#   define PALLOC_THREAD_SENTINEL ((void *)(1))
#endif

#define PALLOC_BUFFSIZEOFFSET 2

#define PALLOC_TYPE_BUFF_T(N) palloc_buff_##N##_t

#define PALLOC_TYPE_BLOCK_T(N) palloc_block_##N##_t

#define PALLOC_DECL_BLOCK(N) \
    typedef struct PALLOC_TYPE_BLOCK_T(N) { \
        unsigned char m[PALLOC_BUFFSIZEOFFSET + N]; \
        struct PALLOC_TYPE_BLOCK_T(N) * n; \
    } PALLOC_TYPE_BLOCK_T(N)

#define PALLOC_TYPE_CHUNK_T(N) palloc_chunk_##N##_t

#define PALLOC_DECL_CHUNK(N, K) \
    typedef struct PALLOC_TYPE_CHUNK_T(N) { \
        PALLOC_TYPE_BLOCK_T(N) s[K]; \
    } PALLOC_TYPE_CHUNK_T(N)

#define PALLOC_NAME_GLOBAL_BLOCK(N) g_palloc_block_##N

#if defined(PALLOC_THREAD) && defined(PALLOC_LOCKFREE)
#   define PALLOC_DECL_GLOBAL_BLOCK(N) \
    static PALLOC_TYPE_BLOCK_T( N ) * volatile PALLOC_NAME_GLOBAL_BLOCK( N ) = NULL
#elif defined(PALLOC_THREAD) && defined(PALLOC_MUTEX)
#   define PALLOC_NAME_GLOBAL_MUTEX(N) g_palloc_mutex_##N

#   define PALLOC_DECL_GLOBAL_BLOCK(N) \
        static PALLOC_STD_MUTEX_T PALLOC_NAME_GLOBAL_MUTEX(N); \
        static PALLOC_TYPE_BLOCK_T(N) * volatile PALLOC_NAME_GLOBAL_BLOCK(N) = NULL
#else
#   define PALLOC_DECL_GLOBAL_BLOCK(N) \
        static PALLOC_TYPE_BLOCK_T(N) * PALLOC_NAME_GLOBAL_BLOCK(N) = NULL
#endif

#define PALLOC_INIT_CHUNK(N) _palloc_init_chunk_##N

#define PALLOC_DECL_INIT_CHUNK(N, K) \
    static PALLOC_TYPE_BLOCK_T(N) * PALLOC_INIT_CHUNK(N)( PALLOC_TYPE_CHUNK_T(N) * c ) { \
        PALLOC_TYPE_BLOCK_T(N) * f = NULL; \
        for( PALLOC_TYPE_BLOCK_T(N) * it = c->s + 0, \
            *it_end = c->s + K; \
            it != it_end; \
            ++it ) { \
            it->n = f; \
            f = it; \
        } \
        return f; \
    }

#define PALLOC_GET_GLOBAL_BLOCK(N) _palloc_get_global_block_##N

#if defined(PALLOC_THREAD) && defined(PALLOC_LOCKFREE)
#   define PALLOC_DECL_GET_GLOBAL_BLOCK(N) \
        static PALLOC_TYPE_BLOCK_T(N) * PALLOC_GET_GLOBAL_BLOCK(N)() { \
            void * g = PALLOC_STD_ATOMIC_LOAD(&PALLOC_NAME_GLOBAL_BLOCK(N)); \
            if( g != NULL && g != PALLOC_THREAD_SENTINEL ) { \
                return g; \
            } \
            void * e = NULL; \
            if( PALLOC_STD_ATOMIC_COMPARE_EXCHANGE_WEAK(&PALLOC_NAME_GLOBAL_BLOCK(N), &e, PALLOC_THREAD_SENTINEL ) == 1) { \
                PALLOC_TYPE_CHUNK_T(N) * c = (PALLOC_TYPE_CHUNK_T(N) *)PALLOC_STD_MALLOC(sizeof(PALLOC_TYPE_CHUNK_T(N))); \
                PALLOC_TYPE_BLOCK_T(N) * b = PALLOC_INIT_CHUNK(N)(c); \
                PALLOC_ATOMIC_STORE(&PALLOC_NAME_GLOBAL_BLOCK(N), b); \
                return b; \
            } \
            do { \
                g = PALLOC_STD_ATOMIC_LOAD(&PALLOC_NAME_GLOBAL_BLOCK(N)); \
            } while( g == PALLOC_THREAD_SENTINEL ); \
            return g; \
        }
#else
#   define PALLOC_DECL_GET_GLOBAL_BLOCK(N) \
        static PALLOC_TYPE_BLOCK_T(N) * PALLOC_GET_GLOBAL_BLOCK(N)() { \
            if( PALLOC_NAME_GLOBAL_BLOCK(N) != NULL ) { \
                return PALLOC_NAME_GLOBAL_BLOCK(N); \
            } \
            PALLOC_TYPE_CHUNK_T(N) * c = (PALLOC_TYPE_CHUNK_T(N) *)PALLOC_STD_MALLOC(sizeof(PALLOC_TYPE_CHUNK_T(N))); \
            PALLOC_TYPE_BLOCK_T(N) * b = PALLOC_INIT_CHUNK(N)(c); \
            PALLOC_NAME_GLOBAL_BLOCK(N) = b; \
            return b; \
        }
#endif

#define PALLOC_ALLOC_BLOCK(N) _palloc_alloc_block_##N

#if defined(PALLOC_THREAD) && defined(PALLOC_LOCKFREE)
#   define PALLOC_DECL_ALLOC_BLOCK(N) \
        static unsigned char * PALLOC_ALLOC_BLOCK(N)() { \
            void * expected; \
            PALLOC_TYPE_BLOCK_T(N) * b; \
            do { \
                do { \
                    b = (PALLOC_TYPE_BLOCK_T( N ) *)PALLOC_STD_ATOMIC_LOAD( &PALLOC_NAME_GLOBAL_BLOCK( N ) ); \
                } while( b == PALLOC_THREAD_SENTINEL ); \
                if( b == NULL ) { \
                    b = PALLOC_GET_GLOBAL_BLOCK(N)(); \
                } \
                expected = b; \
            } while( !PALLOC_STD_ATOMIC_COMPARE_EXCHANGE_WEAK(&PALLOC_NAME_GLOBAL_BLOCK(N), &expected, b->n)); \
            return b->m; \
        }
#elif defined(PALLOC_THREAD) && defined(PALLOC_MUTEX)
#   define PALLOC_DECL_ALLOC_BLOCK(N) \
        static unsigned char * PALLOC_ALLOC_BLOCK(N)() { \
            PALLOC_STD_MUTEX_LOCK(&PALLOC_NAME_GLOBAL_MUTEX(N)); \
            PALLOC_TYPE_BLOCK_T(N) * b = PALLOC_GET_GLOBAL_BLOCK(N)(); \
            PALLOC_NAME_GLOBAL_BLOCK(N) = b->n; \
            PALLOC_STD_MUTEX_UNLOCK(&PALLOC_NAME_GLOBAL_MUTEX(N)); \
            unsigned char * m = b->m; \
            return m; \
        }
#else
#   define PALLOC_DECL_ALLOC_BLOCK(N) \
        static unsigned char * PALLOC_ALLOC_BLOCK(N)() { \
            PALLOC_TYPE_BLOCK_T(N) * b = PALLOC_GET_GLOBAL_BLOCK(N)(); \
            PALLOC_NAME_GLOBAL_BLOCK(N) = b->n; \
            unsigned char * m = b->m; \
            return m; \
        }
#endif

#define PALLOC_FREE_BLOCK(N) _palloc_free_block_##N

#if defined(PALLOC_THREAD) && defined(PALLOC_LOCKFREE)
#   define PALLOC_DECL_FREE_BLOCK(N) \
        static void PALLOC_FREE_BLOCK(N)( void * p ) { \
            PALLOC_TYPE_BLOCK_T(N) * b = (PALLOC_TYPE_BLOCK_T(N) *)(p); \
            PALLOC_TYPE_BLOCK_T(N) * old_head; \
            do { \
                old_head = (PALLOC_TYPE_BLOCK_T(N)*)PALLOC_STD_ATOMIC_LOAD(&PALLOC_NAME_GLOBAL_BLOCK(N)); \
                b->n = old_head; \
            } while (!PALLOC_STD_ATOMIC_COMPARE_EXCHANGE_WEAK(&PALLOC_NAME_GLOBAL_BLOCK(N), (void**)&old_head, b)); \
        }
#elif defined(PALLOC_THREAD) && defined(PALLOC_MUTEX)
#   define PALLOC_DECL_FREE_BLOCK(N) \
        static void PALLOC_FREE_BLOCK(N)( void * p ) { \
            PALLOC_TYPE_BLOCK_T(N) * b = (PALLOC_TYPE_BLOCK_T(N) *)(p); \
            PALLOC_STD_MUTEX_LOCK(&PALLOC_NAME_GLOBAL_MUTEX(N)); \
            b->n = PALLOC_NAME_GLOBAL_BLOCK(N); \
            PALLOC_NAME_GLOBAL_BLOCK(N) = b; \
            PALLOC_STD_MUTEX_UNLOCK(&PALLOC_NAME_GLOBAL_MUTEX(N)); \
        }
#else
#   define PALLOC_DECL_FREE_BLOCK(N) \
        static void PALLOC_FREE_BLOCK(N)( void * p ) { \
            PALLOC_TYPE_BLOCK_T(N) * b = (PALLOC_TYPE_BLOCK_T(N) *)(p); \
            b->n = PALLOC_NAME_GLOBAL_BLOCK(N); \
            PALLOC_NAME_GLOBAL_BLOCK(N) = b; \
        }
#endif

#define PALLOC_DECLARE(N, K) \
    PALLOC_DECL_BLOCK(N); \
    PALLOC_DECL_CHUNK(N, K); \
    PALLOC_DECL_GLOBAL_BLOCK(N); \
    PALLOC_DECL_INIT_CHUNK(N, K); \
    PALLOC_DECL_GET_GLOBAL_BLOCK(N); \
    PALLOC_DECL_ALLOC_BLOCK(N); \
    PALLOC_DECL_FREE_BLOCK(N)

#define PALLOC_THRESHOLD 2048

PALLOC_DECLARE( 16, 4096 );
PALLOC_DECLARE( 32, 2048 );
PALLOC_DECLARE( 64, 1024 );
PALLOC_DECLARE( 128, 512 );
PALLOC_DECLARE( 256, 256 );
PALLOC_DECLARE( 512, 128 );
PALLOC_DECLARE( 1024, 64 );
PALLOC_DECLARE( 2048, 32 );

typedef unsigned char * (*palloc_alloc_func_t)();
typedef void (*palloc_free_func_t)(void *);

static palloc_alloc_func_t palloc_alloc_table[11] = {
    &PALLOC_ALLOC_BLOCK( 16 ),
    &PALLOC_ALLOC_BLOCK( 16 ),
    &PALLOC_ALLOC_BLOCK( 16 ),
    &PALLOC_ALLOC_BLOCK( 16 ),
    &PALLOC_ALLOC_BLOCK( 32 ),
    &PALLOC_ALLOC_BLOCK( 64 ),
    &PALLOC_ALLOC_BLOCK( 128 ),
    &PALLOC_ALLOC_BLOCK( 256 ),
    &PALLOC_ALLOC_BLOCK( 512 ),
    &PALLOC_ALLOC_BLOCK( 1024 ),
    &PALLOC_ALLOC_BLOCK( 2048 )
};

static palloc_free_func_t palloc_free_table[11] = {
    &PALLOC_FREE_BLOCK( 16 ),
    &PALLOC_FREE_BLOCK( 16 ),
    &PALLOC_FREE_BLOCK( 16 ),
    &PALLOC_FREE_BLOCK( 16 ),
    &PALLOC_FREE_BLOCK( 32 ),
    &PALLOC_FREE_BLOCK( 64 ),
    &PALLOC_FREE_BLOCK( 128 ),
    &PALLOC_FREE_BLOCK( 256 ),
    &PALLOC_FREE_BLOCK( 512 ),
    &PALLOC_FREE_BLOCK( 1024 ),
    &PALLOC_FREE_BLOCK( 2048 )
};

const int palloc_index_table[2048] = {
    -1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
};

#define PALLOC_INDEX(N) palloc_index_table[N]
#define PALLOC_ALLOC(I) (*palloc_alloc_table[I])();
#define PALLOC_FREE(I, Q) (*palloc_free_table[I])(Q);

void pinit()
{
#if defined(PALLOC_THREAD) && defined(PALLOC_MUTEX)
    PALLOC_STD_MUTEX_INIT( &PALLOC_NAME_GLOBAL_MUTEX( 16 ) );
    PALLOC_STD_MUTEX_INIT( &PALLOC_NAME_GLOBAL_MUTEX( 32 ) );
    PALLOC_STD_MUTEX_INIT( &PALLOC_NAME_GLOBAL_MUTEX( 64 ) );
    PALLOC_STD_MUTEX_INIT( &PALLOC_NAME_GLOBAL_MUTEX( 128 ) );
    PALLOC_STD_MUTEX_INIT( &PALLOC_NAME_GLOBAL_MUTEX( 256 ) );
    PALLOC_STD_MUTEX_INIT( &PALLOC_NAME_GLOBAL_MUTEX( 512 ) );
    PALLOC_STD_MUTEX_INIT( &PALLOC_NAME_GLOBAL_MUTEX( 1024 ) );
    PALLOC_STD_MUTEX_INIT( &PALLOC_NAME_GLOBAL_MUTEX( 2048 ) );
#endif    
}

void pfini()
{
#if defined(PALLOC_THREAD) && defined(PALLOC_MUTEX)
    PALLOC_STD_MUTEX_FINI( &PALLOC_NAME_GLOBAL_MUTEX( 16 ) );
    PALLOC_STD_MUTEX_FINI( &PALLOC_NAME_GLOBAL_MUTEX( 32 ) );
    PALLOC_STD_MUTEX_FINI( &PALLOC_NAME_GLOBAL_MUTEX( 64 ) );
    PALLOC_STD_MUTEX_FINI( &PALLOC_NAME_GLOBAL_MUTEX( 128 ) );
    PALLOC_STD_MUTEX_FINI( &PALLOC_NAME_GLOBAL_MUTEX( 256 ) );
    PALLOC_STD_MUTEX_FINI( &PALLOC_NAME_GLOBAL_MUTEX( 512 ) );
    PALLOC_STD_MUTEX_FINI( &PALLOC_NAME_GLOBAL_MUTEX( 1024 ) );
    PALLOC_STD_MUTEX_FINI( &PALLOC_NAME_GLOBAL_MUTEX( 2048 ) );
#endif
}

void * palloc( size_t nbytes )
{
    if( nbytes == 0 )
    {
        nbytes = 1;
    }

    if( nbytes >= PALLOC_THRESHOLD )
    {
        unsigned char * q = (unsigned char *)PALLOC_STD_MALLOC( nbytes + PALLOC_BUFFSIZEOFFSET );
        *q++ = 0xff;
        *q++ = 0xff;
        unsigned char * p = q;

        return (void *)p;
    }

    int index = PALLOC_INDEX( nbytes );

    unsigned char * q = PALLOC_ALLOC(index);

    *q++ = nbytes & 0xff;
    *q++ = (nbytes >> 8) & 0xff;
    unsigned char * p = q;

    return (void *)p;
}

void pfree( void * p )
{
    if( p == NULL )
    {
        return;
    }

    unsigned char * q = (unsigned char *)p;
    size_t hbytes = *(--q);
    size_t lbytes = *(--q);
    size_t nbytes = hbytes << 8 | lbytes;

    if( nbytes == 0xffff )
    {
        PALLOC_STD_FREE( q );

        return;
    }

    int index = PALLOC_INDEX( nbytes );

    PALLOC_FREE(index, q);
}

void * prealloc( void * p, size_t nbytes )
{
    if( p == NULL )
    {
        void * new_p = palloc( nbytes );

        return new_p;
    }

    if( nbytes == 0 )
    {
        nbytes = 1;
    }

    unsigned char * old_q = (unsigned char *)p;
    size_t old_hbytes = *(--old_q);
    size_t old_lbytes = *(--old_q);
    size_t old_nbytes = old_hbytes << 8 | old_lbytes;
    
    if( nbytes >= PALLOC_THRESHOLD )
    {
        if( old_nbytes == 0xffff )
        {
            unsigned char * new_q = (unsigned char *)PALLOC_STD_REALLOC( old_q, nbytes + PALLOC_BUFFSIZEOFFSET );
            new_q++;
            new_q++;
            unsigned char * new_p = new_q;

            return new_p;
        }

        unsigned char * new_q = (unsigned char *)PALLOC_STD_MALLOC( nbytes + PALLOC_BUFFSIZEOFFSET );
        *new_q++ = 0xff;
        *new_q++ = 0xff;
        unsigned char * new_p = new_q;

        PALLOC_STD_MEMCPY( new_p, p, old_nbytes );

        int old_index = PALLOC_INDEX( old_nbytes );

        PALLOC_FREE( old_index, old_q );

        return new_p;
    }

    int new_index = PALLOC_INDEX( nbytes );

    if( old_nbytes == 0xffff )
    {
        unsigned char * new_q = PALLOC_ALLOC( new_index );
        *new_q++ = nbytes & 0xff;
        *new_q++ = (nbytes >> 8) & 0xff;
        unsigned char * new_p = new_q;

        PALLOC_STD_MEMCPY( new_p, p, nbytes );

        PALLOC_STD_FREE( old_q );

        return new_p;
    }

    int old_index = PALLOC_INDEX( old_nbytes );

    if( old_index == new_index )
    {
        return p;
    }

    unsigned char * new_q = PALLOC_ALLOC( new_index );

    *new_q++ = nbytes & 0xff;
    *new_q++ = (nbytes >> 8) & 0xff;
    unsigned char * new_p = new_q;

    size_t min_nbytes = old_nbytes < nbytes ? old_nbytes : nbytes;
    PALLOC_STD_MEMCPY( new_p, p, min_nbytes );

    PALLOC_FREE( old_index, old_q );

    return new_p;
}