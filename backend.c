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
LLVMValueRef nilstr;

/* 
   Tell LLVM about some external library functions so we can call them 
   and about some constants we want to use from jit'd code
*/
void llvm_functions(void)
{
   LLVMTypeRef args[2];
   LLVMTypeRef fntype; 
   LLVMTypeRef ret;
   LLVMValueRef fn;

   /* set up a nil string */
   char * str = (char *) GC_MALLOC(4);
   str[0] = 'n';
   str[1] = 'i';
   str[2] = 'l';
   str[3] = '\0';
   START_EXEC;
   nilstr = LLVMBuildGlobalStringPtr(builder, str, "nil");
   END_EXEC;

   /* patch in the printf function */
   args[0] = LLVMPointerType(LLVMInt8Type(), 0);
   ret = LLVMWordType();
   fntype = LLVMFunctionType(ret, args, 1, 1);
   fn = LLVMAddFunction(module, "printf", fntype);

   /* patch in the exit function */
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

/* 
   When an expression is evaluated it is a pain to pass it back out of
   jit'd code and print the value returned by the expression. So we 
   print it on the jit side. So this is our runtime printf for 
   printing LLVMValRef's from jit'd code.
*/
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

/*
   Printing booleans requires special attention. We want to print
   the strings "true" and "false". To do this from the jit side we
   need this function.
*/
void llvm_printbool(LLVMValueRef obj)
{
    static int inited = 0;
    
    static LLVMValueRef truestr;
    static LLVMValueRef falsestr;
    
    /* set up the "true" and "false" strings */
    if (!inited)
    {
        truestr = LLVMBuildGlobalStringPtr(builder, "true", "true");
        falsestr = LLVMBuildGlobalStringPtr(builder, "false", "false");
       inited = 1;
    }

    /* jit an if statement which checks for true/false */
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

    /* print "true" */
    llvm_printf("%s", truestr);

    LLVMBuildBr(builder, e);
    LLVMPositionBuilderAtEnd(builder, b2);  

    /* print "false" */
    llvm_printf("%s", falsestr);

    LLVMBuildBr(builder, e);
    LLVMPositionBuilderAtEnd(builder, e);
}

/*
   This jits a printf for various cesium typesi. We use it to print
   the result of expressions that are evaluated, before returning from
   a jit'd expression.
*/
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
         llvm_printf("%s", nilstr);
         break;
   }
}

/*
   Initialise the LLVM JIT
*/
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

    /* initialise some globals */
    fmtstr = NULL;
    nilstr = NULL;
}

/*
   If something goes wrong after partially jit'ing something we need
   to clean up.
*/
void llvm_reset(void)
{
    LLVMDeleteFunction(function);
    LLVMDisposeBuilder(builder);
}

/*
   Clean up LLVM on exit from Cesium
*/
void llvm_cleanup(void)
{
    /* Clean up */
    LLVMDisposePassManager(pass);  
    LLVMDisposeExecutionEngine(engine); 
}

/*
   Jit an int literal
*/
int exec_int(ast_t * ast)
{
    long num = atol(ast->sym->name);
    
    ast->val = LLVMConstInt(LLVMWordType(), num, 0);

    return 0;
}

/*
   Jit a double literal
*/
int exec_double(ast_t * ast)
{
    double num = atof(ast->sym->name);
    
    ast->val = LLVMConstReal(LLVMDoubleType(), num);

    return 0;
}

/*
   Jit a string literali, being careful to replace special 
   characters with their ascii equivalent
*/
int exec_string(ast_t * ast)
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

    return 0;
}

/*
   We have a number of binary ops we want to jit and they
   all look the same, so define a macro for them.
*/
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
                                                   \
    return 0;                                      \
}

/* Jit add, sub, .... ops */
int exec_binary(exec_plus, LLVMBuildFAdd, LLVMBuildAdd, "add")

int exec_binary(exec_minus, LLVMBuildFSub, LLVMBuildSub, "sub")

/*
   As we traverse the ast we dispatch on ast tag to various jit 
   functions defined above
*/
int exec_ast(ast_t * ast)
{
    switch (ast->tag)
    {
    case AST_INT:
        return exec_int(ast);
    case AST_DOUBLE:
        return exec_double(ast);
    case AST_STRING:
        return exec_string(ast);
    case AST_PLUS:
        return exec_plus(ast);
    case AST_MINUS:
        return exec_minus(ast);
    }
}

/* We start traversing the ast to do jit'ing here */
void exec_root(ast_t * ast)
{
    /* Traverse the ast jit'ing everything, then run the jit'd code */
    START_EXEC;
         
    exec_ast(ast);

    print_obj(ast->type->typ, ast->val);
    
    END_EXEC;
         
    /* 
       If our jit side print_obj had to create a format string
       clean it up now that we've executed the jit'd code.
    */
    if (fmtstr)
    {
        LLVMDeleteGlobal(fmtstr);
        fmtstr = NULL;
    }
}

