#include <stdio.h>
#include "environment.h"
#include "exception.h"

jmp_buf exc;

void exception(const char * msg)
{
   rewind_scope();

   fprintf(stderr, msg);
   
   longjmp(exc, 1);
}

void jit_exception(jit_t * jit, const char * msg)
{
   llvm_reset(jit);
   rewind_scope();

   fprintf(stderr, msg);

   longjmp(exc, 1);
}

