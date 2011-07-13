#include <stdio.h>
#include "exception.h"

jmp_buf exc;

void exception(char * msg)
{
   fprintf(stderr, msg);
   
   longjmp(exc, 1);
}

