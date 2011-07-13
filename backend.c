#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "backend.h"
#include "gc.h"

#include <llvm-c/Core.h>  
#include <llvm-c/Analysis.h>  
#include <llvm-c/ExecutionEngine.h>  
#include <llvm-c/Target.h>  
#include <llvm-c/Transforms/Scalar.h> 

LLVMExecutionEngineRef engine;  
LLVMPassManagerRef pass;
LLVMModuleRef module;
LLVMBuilderRef builder;
LLVMValueRef function;

LLVMValueRef fmtstr;
LLVMValueRef nil_str;

void llvm_functions(void)
{
   LLVMTypeRef args[2];
   LLVMTypeRef fntype; 
   LLVMTypeRef ret;
   LLVMValueRef fn;

   char * str = (char *) GC_MALLOC(4);
   str[0] = 'n';
   str[1] = 'i';
   str[2] = 'l';
   str[3] = '\0';
   START_EXEC;
   nil_str = LLVMBuildGlobalStringPtr(builder, str, "nil");
   END_EXEC;

   args[0] = LLVMPointerType(LLVMInt8Type(), 0);
   ret = LLVMWordType();
   fntype = LLVMFunctionType(ret, args, 1, 1);
   fn = LLVMAddFunction(module, "printf", fntype);

   args[0] = LLVMWordType();
   ret = LLVMVoidType();
   fntype = LLVMFunctionType(ret, args, 1, 0);
   fn = LLVMAddFunction(module, "exit", fntype);
}

/* count parameters as represented by %'s in format string */
int count_params(const char * fmt)
{
   int len = strlen(fmt);
   int i, count = 0;

   for (i = 0; i < len - 1; i++)
      if (fmt[i] == '%')
         if (fmt[i + 1] == '%')
               i++;
         else
            count++;

   return count;
}

/* runtime printf for LLVMValRef's */
void llvm_printf(const char * fmt, ...)
{
   int i, count = count_params(fmt);
   va_list ap;
   
   /* get printf function */
   LLVMValueRef fn = LLVMGetNamedFunction(module, "printf");
   LLVMSetFunctionCallConv(fn, LLVMCCallConv);
   
   /* Add a global variable for format string */
   LLVMValueRef str = LLVMConstString(fmt, strlen(fmt), 0);
   LLVMValueRef str_val = LLVMAddGlobal(module, LLVMTypeOf(str), "fmt");
   LLVMSetInitializer(str_val, str);
   LLVMSetGlobalConstant(str_val, 1);
   LLVMSetLinkage(str_val, LLVMInternalLinkage);
   fmtstr = str_val;
   
   /* build variadic parameter list for printf */
   LLVMValueRef indices[2] = { LLVMConstInt(LLVMWordType(), 0, 0), LLVMConstInt(LLVMWordType(), 0, 0) };
   LLVMValueRef GEP = LLVMBuildGEP(builder, str_val, indices, 2, "str");
   LLVMValueRef args[count + 1];
   args[0] = GEP;
   
   va_start(ap, fmt);

   for (i = 0; i < count; i++)
      args[i + 1] = va_arg(ap, LLVMValueRef);

   va_end(ap);

   /* build call to printf */
   LLVMValueRef call_printf = LLVMBuildCall(builder, fn, args, count + 1, "printf");
   LLVMSetTailCall(call_printf, 1);
   LLVMAddInstrAttribute(call_printf, 0, LLVMNoUnwindAttribute);
   LLVMAddInstrAttribute(call_printf, 1, LLVMNoAliasAttribute);
}

void llvm_printbool(LLVMValueRef obj)
{
    static int inited = 0;
    
    static LLVMValueRef true_str;
    static LLVMValueRef false_str;
    
    if (!inited)
    {
        true_str = LLVMBuildGlobalStringPtr(builder, "true", "true");
        false_str = LLVMBuildGlobalStringPtr(builder, "false", "false");
       inited = 1;
    }

    LLVMBasicBlockRef i;
    LLVMBasicBlockRef b1;
    LLVMBasicBlockRef b2;
    LLVMBasicBlockRef e;

    i = LLVMAppendBasicBlock(function, "if");
    b1 = LLVMAppendBasicBlock(function, "ifbody");
    b2 = LLVMAppendBasicBlock(function, "ebody");
    e = LLVMAppendBasicBlock(function, "ifend");
    LLVMBuildBr(builder, i);
    LLVMPositionBuilderAtEnd(builder, i); 

    LLVMBuildCondBr(builder, obj, b1, b2);
    LLVMPositionBuilderAtEnd(builder, b1);

    llvm_printf("%s", true_str);

    LLVMBuildBr(builder, e);
    LLVMPositionBuilderAtEnd(builder, b2);  

    llvm_printf("%s", false_str);

    LLVMBuildBr(builder, e);
    LLVMPositionBuilderAtEnd(builder, e);
}

void print_obj(typ_t typ, LLVMValueRef obj)
{
   switch (typ)
   {
      case INT:
         llvm_printf("%ld", obj);
         break;
      case CHAR:
         llvm_printf("%c", obj);
         break;
      case DOUBLE:
         llvm_printf("%.5g", obj);
         break;
      case STRING:
         llvm_printf("\"%s\"", obj);
         break;
      case BOOL:
         llvm_printbool(obj);
         break;
      case NIL:
         llvm_printf("%s", nil_str);
         break;
   }
}


void llvm_init(void)
{
    char * error = NULL;
      
    /* Jit setup */
    LLVMLinkInJIT();
    LLVMInitializeNativeTarget();

    /* Create module */
    module = LLVMModuleCreateWithName("cesium");

    /* Create JIT engine */
    if (LLVMCreateJITCompilerForModule(&engine, module, 2, &error) != 0) 
    {   
       fprintf(stderr, "%s\n", error);  
       LLVMDisposeMessage(error);  
       abort();  
    } 
   
    /* Create optimisation pass pipeline */
    pass = LLVMCreateFunctionPassManagerForModule(module);  
    LLVMAddTargetData(LLVMGetExecutionEngineTargetData(engine), pass);  
    LLVMAddConstantPropagationPass(pass);  
    LLVMAddInstructionCombiningPass(pass);  
    LLVMAddPromoteMemoryToRegisterPass(pass);  
    LLVMAddGVNPass(pass);  
    LLVMAddCFGSimplificationPass(pass);

    /* link in external functions callable from jit'd code */
    llvm_functions();
}

void llvm_reset(void)
{
    LLVMDeleteFunction(function);
    LLVMDisposeBuilder(builder);
}

void llvm_cleanup(void)
{
    /* Clean up */
    LLVMDisposePassManager(pass);  
    LLVMDisposeExecutionEngine(engine); 
}

void exec_int(ast_t * ast)
{
    long num = atol(ast->sym->name);
    
    ast->val = LLVMConstInt(LLVMWordType(), num, 0);
}

void exec_double(ast_t * ast)
{
    double num = atof(ast->sym->name);
    
    ast->val = LLVMConstReal(LLVMDoubleType(), num);
}

void exec_string(ast_t * ast)
{
    char * name = ast->sym->name;
         
    if (ast->sym->val == NULL)
    {
        int length = strlen(name) - 2;
        int i, j, bs = 0;
            
        for (i = 0; i < length; i++)
        {
            if (name[i + 1] == '\\')
            { 
                bs++;
                i++;
            }
        }

        char * str = (char *) GC_MALLOC(length + 1 - bs);
            
        for (i = 0, j = 0; i < length; i++, j++)
        {
            if (name[i + 1] == '\\')
            {
                switch (name[i + 2])
                {
                case '0':
                    str[j] = '\0';
                    break;
                case '\\':
                    str[j] = '\\';
                    break;
                case 'n':
                    str[j] = '\n';
                    break;
                case 'r':
                    str[j] = '\r';
                    break;
                case 't':
                    str[j] = '\t';
                    break;
                default:
                    str[j] = name[i + 2];
                }
                i++;
            } else
                str[j] = name[i + 1];
        }

        length = length - bs;
        str[length] = '\0';
            
        ast->sym->val = LLVMBuildGlobalStringPtr(builder, str, "string");
    }

    ast->val = ast->sym->val;
}

#define exec_binary(__name, __fop, __iop, __str)   \
__name(ast_t * ast)                                \
{                                                  \
    ast_t * expr1 = ast->child;                    \
    ast_t * expr2 = expr1->next;                   \
                                                   \
    exec_ast(expr1);                               \
    exec_ast(expr2);                               \
                                                   \
    LLVMValueRef v1 = expr1->val, v2 = expr2->val; \
                                                   \
    if (ast->type == t_double)                     \
       ast->val = __fop(builder, v1, v2, __str);   \
    else                                           \
       ast->val = __iop(builder, v1, v2, __str);   \
}

void exec_binary(exec_plus, LLVMBuildFAdd, LLVMBuildAdd, "add")

void exec_binary(exec_minus, LLVMBuildFSub, LLVMBuildSub, "sub")

void exec_ast(ast_t * ast)
{
    switch (ast->tag)
    {
    case AST_INT:
        exec_int(ast);
        break;
    case AST_DOUBLE:
        exec_double(ast);
        break;
    case AST_STRING:
        exec_string(ast);
        break;
    case AST_PLUS:
        exec_plus(ast);
        break;
    case AST_MINUS:
        exec_minus(ast);
        break;
    }
}

void exec_root(ast_t * ast)
{
    START_EXEC;
         
    exec_ast(ast);

    print_obj(ast->type->typ, ast->val);
    
    END_EXEC;
         
    if (fmtstr)
    {
        LLVMDeleteGlobal(fmtstr);
        fmtstr = NULL;
    }
}

