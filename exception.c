#include <stdio.h>
#include "environment.h"
#include "exception.h"
#include "unify.h"
#include "backend.h"

jmp_buf exc;

void exception(const char * msg)
{
   rewind_scope();
   rel_assign_rewind();

   fprintf(stderr, msg);
   
   longjmp(exc, 1);
}

void jit_exception(jit_t * jit, const char * msg)
{
   llvm_reset(jit);
   rewind_scope();
   rel_assign_rewind();
   jit->bind_num = 0;

   fprintf(stderr, msg);

   longjmp(exc, 1);
}

