#ifndef GC_H
#define GC_H

#include <stdlib.h>
#define CS_MALLOC_NAME "malloc"
#define CS_MALLOC_NAME2 "malloc"
#define GC_INIT() 
#define GC_MALLOC(N) calloc(N, 1)
#define GC_REALLOC realloc

#endif
