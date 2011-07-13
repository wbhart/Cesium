#ifndef BACKEND_H
#define BACKEND_H

#include <llvm-c/Core.h>  
#include <llvm-c/Analysis.h>  
#include <llvm-c/ExecutionEngine.h>  
#include <llvm-c/Target.h>  
#include <llvm-c/Transforms/Scalar.h> 

#include "ast.h"

#define TRACE 0 /* prints lots of ast and llvm trace info */

/* Are we on a 32 or 64 bit machine */
#if ULONG_MAX == 4294967295U
#define LLVMWordType() LLVMInt32Type()
#else
#define LLVMWordType() LLVMInt64Type()
#endif

void llvm_init(void);

void llvm_reset(void);

void llvm_cleanup(void);

void exec_ast(ast_t * ast);

void exec_root(ast_t * ast);

void llvm_functions(void);

/* Set things up so we can begin jit'ing */
#define START_EXEC \
   LLVMBuilderRef __builder_save; \
   LLVMValueRef __function_save; \
   do { \
   __builder_save = builder; \
   builder = LLVMCreateBuilder(); \
   __function_save = function; \
   LLVMTypeRef __args[] = { }; \
   LLVMTypeRef __retval = LLVMVoidType(); \
   LLVMTypeRef __fn_type = LLVMFunctionType(__retval, __args, 0, 0); \
   function = LLVMAddFunction(module, "exec", __fn_type); \
   LLVMBasicBlockRef __entry = LLVMAppendBasicBlock(function, "entry"); \
   LLVMPositionBuilderAtEnd(builder, __entry); \
   } while (0)
   
/* Run the jit'd code */
#define END_EXEC \
   do { \
   LLVMBuildRetVoid(builder); \
   LLVMRunFunctionPassManager(pass, function); \
   if (TRACE) \
      LLVMDumpModule(module); \
   LLVMGenericValueRef __exec_args[] = {}; \
   LLVMRunFunction(engine, function, 0, __exec_args); \
   LLVMDeleteFunction(function); \
   LLVMDisposeBuilder(builder); \
   function = __function_save; \
   builder = __builder_save; \
   } while (0)

#endif

