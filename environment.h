#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "symbol.h"
#include "types.h"

typedef struct bind_t
{
   sym_t * sym;
   type_t * type;
   struct bind_t * next;
} bind_t;

typedef struct env_t
{
   bind_t * scope;
   struct env_t * next;
} env_t;

env_t * current_scope;

void scope_init(void);

void bind_symbol(sym_t * sym, type_t * type);

bind_t * find_symbol(sym_t * sym);

#ifdef __cplusplus
}
#endif

#endif

