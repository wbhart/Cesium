#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "symbol.h"
#include "types.h"

#ifdef __cplusplus
 extern "C" {
#endif

typedef struct bind_t
{
   sym_t * sym;
   type_t * type;
   LLVMValueRef val;
   struct bind_t * next;
} bind_t;

typedef struct env_t
{
   bind_t * scope;
   struct env_t * next;
} env_t;

extern env_t * current_scope;

void scope_init(void);

int scope_is_global(void);

void scope_mark(void);

void rewind_scope(void);

void bind_symbol(sym_t * sym, type_t * type, LLVMValueRef val);

bind_t * find_symbol(sym_t * sym);

bind_t * find_symbol_in_scope(sym_t * sym);

#ifdef __cplusplus
}
#endif

#endif

