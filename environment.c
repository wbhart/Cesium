#include "gc.h"
#include "symbol.h"
#include "environment.h"

#include <llvm-c/Core.h>  
#include <llvm-c/Analysis.h>  
#include <llvm-c/ExecutionEngine.h>  
#include <llvm-c/Target.h>  
#include <llvm-c/Transforms/Scalar.h> 

env_t * current_scope;
bind_t * scope_ptr;

void scope_init(void)
{
   current_scope = (env_t *) GC_MALLOC(sizeof(env_t));
   current_scope->next = NULL;
   current_scope->scope = NULL;
   scope_ptr = NULL;
}

int scope_is_global(void)
{
   return current_scope->next == NULL;
}

void scope_mark(void)
{
   scope_ptr = current_scope->scope;
}

void rewind_scope()
{
    while (current_scope->scope != scope_ptr)
        current_scope->scope = current_scope->scope->next;
}

void bind_symbol(sym_t * sym, type_t * type, LLVMValueRef val)
{
   bind_t * scope = current_scope->scope;
   bind_t * b = (bind_t *) GC_MALLOC(sizeof(bind_t));
   b->sym = sym;
   b->type = type;
   b->val = val;
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

bind_t * find_symbol_in_scope(sym_t * sym)
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

