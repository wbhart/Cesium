#ifndef BACKEND_H
#define BACKEND_H

#include <llvm-c/Core.h>  
#include <llvm-c/Analysis.h>  
#include <llvm-c/ExecutionEngine.h>  
#include <llvm-c/Target.h>  
#include <llvm-c/Transforms/Scalar.h> 

#include "ast.h"

#ifdef __cplusplus
 extern "C" {
#endif

#define TRACE 0 /* prints lots of ast and llvm trace info */

/* Are we on a 32 or 64 bit machine */
#if ULONG_MAX == 4294967295U
#define LLVMWordType() LLVMInt32Type()
#else
#define LLVMWordType() LLVMInt64Type()
#endif

typedef struct jit_t
{
    LLVMBuilderRef builder;
    LLVMValueRef function;
    LLVMExecutionEngineRef engine;  
    LLVMPassManagerRef pass;
    LLVMModuleRef module;
    LLVMValueRef fmt_str;
    LLVMValueRef nil_str;
    LLVMValueRef true_str;
    LLVMValueRef false_str;
} jit_t;

jit_t * llvm_init(void);

void llvm_reset(jit_t * jit);

void llvm_cleanup(jit_t * jit);

int exec_ast(jit_t * jit, ast_t * ast);

void exec_root(jit_t * jit, ast_t * ast);

void llvm_functions(jit_t * jit);

/* Set things up so we can begin jit'ing */
#define START_EXEC \
   LLVMBuilderRef __builder_save; \
   LLVMValueRef __function_save; \
   do { \
   __builder_save = jit->builder; \
   jit->builder = LLVMCreateBuilder(); \
   __function_save = jit->function; \
   LLVMTypeRef __args[] = { }; \
   LLVMTypeRef __retval = LLVMVoidType(); \
   LLVMTypeRef __fn_type = LLVMFunctionType(__retval, __args, 0, 0); \
   jit->function = LLVMAddFunction(jit->module, "exec", __fn_type); \
   LLVMBasicBlockRef __entry = LLVMAppendBasicBlock(jit->function, "entry"); \
   LLVMPositionBuilderAtEnd(jit->builder, __entry); \
   } while (0)
   
/* Run the jit'd code */
#define END_EXEC \
   do { \
   LLVMBuildRetVoid(jit->builder); \
   LLVMRunFunctionPassManager(jit->pass, jit->function); \
   if (TRACE) \
      LLVMDumpModule(jit->module); \
   LLVMGenericValueRef __exec_args[] = {}; \
   LLVMRunFunction(jit->engine, jit->function, 0, __exec_args); \
   LLVMDeleteFunction(jit->function); \
   LLVMDisposeBuilder(jit->builder); \
   jit->function = __function_save; \
   jit->builder = __builder_save; \
   } while (0)

#ifdef __cplusplus
}
#endif

#endif

