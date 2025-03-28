#ifndef PALLOC_H_
#define PALLOC_H_

#include <stddef.h>

#ifdef PALLOC_SUFFIX
#   define PCONCAT_I( x, y ) x##y
#   define PCONCAT(x, y) PCONCAT_I(x, y)

#   define PINIT PCONCAT(pinit, PALLOC_SUFFIX)
#   define PFINI PCONCAT(pfini, PALLOC_SUFFIX)
#   define PALLOC PCONCAT(palloc, PALLOC_SUFFIX)
#   define PFREE PCONCAT(pfree, PALLOC_SUFFIX)
#   define PREALLOC PCONCAT(prealloc, PALLOC_SUFFIX)
#else
#   define PINIT pinit
#   define PFINI pfini
#   define PALLOC palloc
#   define PFREE pfree
#   define PREALLOC prealloc
#endif

void PINIT();
void PFINI();

void * PALLOC( size_t nbytes );
void PFREE( void * p );
void * PREALLOC( void * p, size_t nbytes );

#endif // PALLOC_H_