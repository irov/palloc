#ifndef PALLOC_H_
#define PALLOC_H_

#include <stddef.h>

void * palloc( size_t nbytes );
void pfree( void * p );
void * prealloc( void * p, size_t nbytes );

#endif // PALLOC_H_