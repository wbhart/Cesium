#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <setjmp.h>

jmp_buf exc;

void exception(char * msg);

#endif

