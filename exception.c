#include <stdio.h>
#include "exception.h"

jmp_buf exc;

void exception(const char * msg)
{
   fprintf(stderr, msg);
   
   longjmp(exc, 1);
}

void jit_exception(jit_t * jit, const char * msg)
{
   llvm_reset(jit);

   fprintf(stderr, msg);

   longjmp(exc, 1);
}

