#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <setjmp.h>
#include "backend.h"

#ifdef __cplusplus
 extern "C" {
#endif

extern jmp_buf exc;

void exception(const char * msg);

void jit_exception(jit_t * jit, const char * msg);

#ifdef __cplusplus
}
#endif

#endif

