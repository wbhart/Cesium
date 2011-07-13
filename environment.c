#include "gc.h"
#include "symbol.h"
#include "environment.h"

env_t * current_scope;

void scope_init(void)
{
   current_scope = GC_MALLOC(sizeof(env_t));
   current_scope->next = NULL;
   current_scope->scope = NULL;
}

void bind_symbol(sym_t * sym, type_t * type)
{
   bind_t * scope = current_scope->scope;
   bind_t * b = GC_MALLOC(sizeof(bind_t));
   b->sym = sym;
   b->type = type;
   b->next = scope;
   current_scope->scope = b;
}

bind_t * find_symbol(sym_t * sym)
{
   bind_t * b = current_scope->scope;

   while (b != NULL)
   {
      if (b->sym == sym)
         return b;
      b = b->next;
   }

   return NULL;
}

