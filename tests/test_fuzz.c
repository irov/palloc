#include "palloc/palloc.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <stdlib.h>
#include <time.h>
#include <string.h>

#define NUM_PROBE 1000000
#define NUM_THREADS 64
#define MAX_PTRS 16

typedef struct
{
    int thread_id;
} thread_arg_t;

static int check_mem( void * p, size_t sz, int thread_id )
{
    if( p == NULL )
    {
        return 0;
    }

    for( size_t i = 0; i != sz; ++i )
    {
        unsigned char * pv = (unsigned char *)p;

        unsigned char v = pv[i];

        if( v != thread_id )
        {
            return 1;
        }
    }

    return 0;
}

static DWORD WINAPI thread_func( LPVOID lpParam )
{
    thread_arg_t * myarg = (thread_arg_t *)lpParam;
    int thread_id = myarg->thread_id;

    srand( (unsigned)time( NULL ) + thread_id * 12345 );

    void * ptrs[MAX_PTRS] = {0};
    size_t sizes[MAX_PTRS] = {0};

    for( int i = 0; i != NUM_PROBE; ++i )
    {
        int idx = rand() % MAX_PTRS;
        int action = rand() % 3;

        size_t sz = sizes[idx];
        void * p = ptrs[idx];

        if( check_mem( ptrs[idx], sizes[idx], thread_id ) != 0 )
        {
            ExitThread( EXIT_FAILURE );

            return 0;
        }

        switch( action )
        {
        case 0:
            {
                if( p != NULL )
                {
                    pfree( p );
                }

                size_t new_sz = 1 + rand() % 4096;

                ptrs[idx] = palloc( new_sz );
                sizes[idx] = new_sz;

                memset( ptrs[idx], thread_id, new_sz );
            } break;
        case 1:
            {
                size_t new_sz = 1 + rand() % 4096;

                ptrs[idx] = prealloc( p, new_sz );
                sizes[idx] = new_sz;

                memset( ptrs[idx], thread_id, new_sz );
            } break;
        case 2:
            {
                memset( ptrs[idx], 0xFF, sizes[idx] );

                pfree( p );
                ptrs[idx] = NULL;
                sizes[idx] = 0;
            } break;
        }
    }

    for( int i = 0; i != MAX_PTRS; ++i )
    {
        pfree( ptrs[i] );
        ptrs[i] = NULL;
        sizes[i] = 0;
    }

    ExitThread( EXIT_SUCCESS );

    return 0;
}

int main( void )
{
    PALLOC_INIT();

    HANDLE threads[NUM_THREADS];
    thread_arg_t thread_args[NUM_THREADS];

    for( int i = 0; i != NUM_THREADS; ++i )
    {
        thread_args[i].thread_id = i;
        threads[i] = CreateThread( NULL, 0, &thread_func, thread_args + i, 0, NULL );
    }

    WaitForMultipleObjects( NUM_THREADS, threads, TRUE, INFINITE );

    for( int i = 0; i != NUM_THREADS; ++i )
    {
        DWORD exit_code;
        GetExitCodeThread( threads[i], &exit_code );

        if( exit_code != EXIT_SUCCESS )
        {
            return EXIT_FAILURE;
        }
    }

    for( int i = 0; i != NUM_THREADS; ++i )
    {
        CloseHandle( threads[i] );
    }

    PALLOC_FINI();

    return EXIT_SUCCESS;
}