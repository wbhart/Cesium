#include <stdio.h>
#include "gc.h"
#include "unify.h"
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
   scope_ptr = NULL;
}

int scope_is_global(bind_t * bind)
{
   env_t * s = current_scope;
   while (s->next != NULL)
      s = s->next;

   bind_t * b = s->scope;
   while (b != NULL)
   {
      if (b == bind)
         return 1;
      b = b->next;
   }
   return 0;
}

int scope_is_current(bind_t * bind)
{
   bind_t * b = current_scope->scope;

   while (b != NULL)
   {
      if (b == bind)
         return 1;
      b = b->next;
   }
   return 0;
}

void scope_mark(void)
{
   scope_ptr = current_scope->scope;
}

void scope_up(void)
{
   env_t * env = (env_t *) GC_MALLOC(sizeof(env_t));
   env->next = current_scope;
   current_scope = env;
   scope_ptr = NULL;   
}

void scope_down(void)
{
   current_scope = current_scope->next;
}

void rewind_scope()
{
    if (current_scope->next != NULL)
    {
        while (current_scope->next != NULL)
            scope_down();
    }
    if (scope_is_global(scope_ptr))
    {
        while (current_scope->scope != scope_ptr)
            current_scope->scope = current_scope->scope->next;
    } else
        scope_ptr = NULL;
}

void scope_print(void)
{
   printf("Scope:\n");
   env_t * s = current_scope;
   
   while (s != NULL)
   {
      bind_t * bind = current_scope->scope;
  
      while (bind != NULL)
      {
         printf("%s\n", bind->sym->name);
         bind = bind->next;
      }
      printf("----\n");
 
      s = s->next;
  }
}

bind_t * bind_symbol(sym_t * sym, type_t * type, LLVMValueRef val)
{
   bind_t * scope = current_scope->scope;
   bind_t * b = (bind_t *) GC_MALLOC(sizeof(bind_t));
   b->sym = sym;
   b->type = type;
   b->val = val;
   b->next = scope;
   current_scope->scope = b;
   return b;
}

bind_t * bind_lambda(sym_t * sym, type_t * type, ast_t * ast)
{
   bind_t * scope = current_scope->scope;
   bind_t * b = (bind_t *) GC_MALLOC(sizeof(bind_t));
   b->sym = sym;
   b->type = type;
   b->ast = ast;
   b->next = scope;
   current_scope->scope = b;
   return b;
}

bind_t * find_symbol(sym_t * sym)
{
   env_t * s = current_scope;
   bind_t * b;

   while (s != NULL)
   {
      b = s->scope;
 
      while (b != NULL)
      {
         if (b->sym == sym)
            return b;
         b = b->next;
      }
      
      s = s->next;
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

