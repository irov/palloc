#include "palloc/palloc.h"

#include <stdlib.h>
#include <time.h>
#include <string.h>

#define MAX_PTRS 1024

int main( void )
{
    srand( (unsigned)time( NULL ) );
    void * ptrs[MAX_PTRS] = {0};
    size_t sizes[MAX_PTRS] = {0};

    for( int i = 0; i < 1000000; ++i )
    {
        int idx = rand() % MAX_PTRS;
        int action = rand() % 3;

        switch( action )
        {
        case 0:
            {
                size_t sz = 1 + rand() % 4096;

                void * p = ptrs[idx];

                if( p != NULL )
                {
                    pfree( p );
                }

                ptrs[idx] = palloc( sz );
                sizes[idx] = sz;

                memset( ptrs[idx], 0xAB, sz );
            } break;
        case 1:
            {
                size_t sz = 1 + rand() % 4096;
                
                void * p = ptrs[idx];

                p = prealloc( p, sz );
                ptrs[idx] = p;
                sizes[idx] = sz;

                memset( ptrs[idx], 0xAB, sz );
            } break;
        case 2:
            {
                void * p = ptrs[idx];

                pfree( p );
                ptrs[idx] = NULL;
                sizes[idx] = 0;
            } break;
        }
    }

    return EXIT_SUCCESS;
}