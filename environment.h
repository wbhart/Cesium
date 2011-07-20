#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <llvm-c/Core.h>  
#include <llvm-c/Analysis.h>  
#include <llvm-c/ExecutionEngine.h>  
#include <llvm-c/Target.h>  
#include <llvm-c/Transforms/Scalar.h> 

#include "symbol.h"
#include "types.h"
#include "ast.h"

#ifdef __cplusplus
 extern "C" {
#endif

typedef struct bind_t
{
   struct ast_t * ast;
   type_t * type;
   sym_t * sym;
   LLVMValueRef val;
   int initialised;
   struct bind_t * next;
} bind_t;

typedef struct env_t
{
   bind_t * scope;
   struct env_t * next;
} env_t;

extern env_t * current_scope;

void scope_init(void);

int scope_is_global(bind_t * bind);

void scope_mark(void);

void rewind_scope(void);

void scope_up(void);

void scope_down(void);

void scope_print(void);

bind_t * bind_symbol(sym_t * sym, type_t * type, LLVMValueRef val);

bind_t * bind_lambda(sym_t * sym, type_t * type, ast_t * ast);

bind_t * find_symbol(sym_t * sym);

bind_t * find_symbol_in_scope(sym_t * sym);

#ifdef __cplusplus
}
#endif

#endif

