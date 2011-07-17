#ifndef SYMBOL_H
#define SYMBOL_H

#include <llvm-c/Core.h>  
#include <llvm-c/Analysis.h>  
#include <llvm-c/ExecutionEngine.h>  
#include <llvm-c/Target.h>  
#include <llvm-c/Transforms/Scalar.h> 

#ifdef __cplusplus
 extern "C" {
#endif

#include <string.h>

#define SYM_TAB_SIZE 10000

typedef struct sym_t {
   char * name;
   LLVMValueRef val;
} sym_t;

void sym_tab_init(void);

void print_sym_tab(void);

sym_t * sym_lookup(const char * name);

#ifdef __cplusplus
}
#endif

#endif
