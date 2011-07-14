#ifndef AST_H
#define AST_H

#include <llvm-c/Core.h>  
#include <llvm-c/Analysis.h>  
#include <llvm-c/ExecutionEngine.h>  
#include <llvm-c/Target.h>  
#include <llvm-c/Transforms/Scalar.h> 

#ifdef __cplusplus
 extern "C" {
#endif

#include "environment.h"
#include "symbol.h"
#include "types.h"

typedef enum {
   AST_IDENT, AST_LVALUE, 
   AST_DOUBLE, AST_INT, AST_BOOL, AST_STRING,
   AST_PLUS, AST_MINUS, AST_TIMES, AST_DIV, AST_MOD,
   AST_LSH, AST_RSH, 
   AST_BITOR, AST_BITAND, AST_BITXOR,
   AST_LE, AST_GE, AST_LT, AST_GT, AST_EQ, AST_NE,
   AST_LOGAND, AST_LOGOR,
   AST_POST_INC, AST_POST_DEC,
   AST_LOGNOT, AST_BITNOT, AST_UNMINUS,
   AST_ASSIGNMENT, AST_VARASSIGN, 
   AST_FNDEC
} tag_t;

typedef struct ast_t {
   struct ast_t * next;
   struct ast_t * child;
   tag_t tag;
   sym_t * sym;
   type_t * type;
   env_t * env;
   LLVMValueRef val;
} ast_t;

extern ast_t * root;

extern ast_t * op_plus;
extern ast_t * op_minus;
extern ast_t * op_times;
extern ast_t * op_div;
extern ast_t * op_mod;
extern ast_t * op_lsh;
extern ast_t * op_rsh;
extern ast_t * op_bitor;
extern ast_t * op_bitand;
extern ast_t * op_bitxor;
extern ast_t * op_le;
extern ast_t * op_ge;
extern ast_t * op_lt;
extern ast_t * op_gt;
extern ast_t * op_eq;
extern ast_t * op_ne;
extern ast_t * op_logand;
extern ast_t * op_logor;
extern ast_t * const_true;
extern ast_t * const_false;

void ast_init(void);

ast_t * new_ast(void);

ast_t * ast_symbol(sym_t * sym, tag_t tag);

ast_t * ast_unary(ast_t * a, tag_t tag);

ast_t * ast_binary(ast_t * a1, ast_t * a2, ast_t * op);

ast_t * ast_op(tag_t tag);

ast_t * ast_reverse(ast_t * a);

void ast_print(ast_t * a, int indent);

#ifdef __cplusplus
}
#endif

#endif
