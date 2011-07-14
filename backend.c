#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "environment.h"
#include "exception.h"
#include "backend.h"
#include "unify.h"
#include "gc.h"

#include <llvm-c/Core.h>  
#include <llvm-c/Analysis.h>  
#include <llvm-c/ExecutionEngine.h>  
#include <llvm-c/Target.h>  
#include <llvm-c/Transforms/Scalar.h> 

/* 
   Tell LLVM about some external library functions so we can call them 
   and about some constants we want to use from jit'd code
*/
void llvm_functions(jit_t * jit)
{
   LLVMTypeRef args[2];
   LLVMTypeRef fntype; 
   LLVMTypeRef ret;
   LLVMValueRef fn;

   /* patch in the printf function */
   args[0] = LLVMPointerType(LLVMInt8Type(), 0);
   ret = LLVMWordType();
   fntype = LLVMFunctionType(ret, args, 1, 1);
   fn = LLVMAddFunction(jit->module, "printf", fntype);

   /* patch in the exit function */
   args[0] = LLVMWordType();
   ret = LLVMVoidType();
   fntype = LLVMFunctionType(ret, args, 1, 0);
   fn = LLVMAddFunction(jit->module, "exit", fntype);
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
void llvm_printf(jit_t * jit, const char * fmt, ...)
{
   int i, count = count_params(fmt);
   va_list ap;
   
   /* get printf function */
   LLVMValueRef fn = LLVMGetNamedFunction(jit->module, "printf");
   LLVMSetFunctionCallConv(fn, LLVMCCallConv);
   
   /* Add a global variable for format string */
   LLVMValueRef str = LLVMConstString(fmt, strlen(fmt), 0);
   jit->fmt_str = LLVMAddGlobal(jit->module, LLVMTypeOf(str), "fmt");
   LLVMSetInitializer(jit->fmt_str, str);
   LLVMSetGlobalConstant(jit->fmt_str, 1);
   LLVMSetLinkage(jit->fmt_str, LLVMInternalLinkage);
   
   /* build variadic parameter list for printf */
   LLVMValueRef indices[2] = { LLVMConstInt(LLVMWordType(), 0, 0), LLVMConstInt(LLVMWordType(), 0, 0) };
   LLVMValueRef GEP = LLVMBuildGEP(jit->builder, jit->fmt_str, indices, 2, "str");
   LLVMValueRef args[count + 1];
   args[0] = GEP;
   
   va_start(ap, fmt);

   for (i = 0; i < count; i++)
      args[i + 1] = va_arg(ap, LLVMValueRef);

   va_end(ap);

   /* build call to printf */
   LLVMValueRef call_printf = LLVMBuildCall(jit->builder, fn, args, count + 1, "printf");
   LLVMSetTailCall(call_printf, 1);
   LLVMAddInstrAttribute(call_printf, 0, LLVMNoUnwindAttribute);
   LLVMAddInstrAttribute(call_printf, 1, LLVMNoAliasAttribute);
}

/*
   Printing booleans requires special attention. We want to print
   the strings "true" and "false". To do this from the jit side we
   need this function.
*/
void llvm_printbool(jit_t * jit, LLVMValueRef obj)
{
    /* jit an if statement which checks for true/false */
    LLVMBasicBlockRef i;
    LLVMBasicBlockRef b1;
    LLVMBasicBlockRef b2;
    LLVMBasicBlockRef e;

    i = LLVMAppendBasicBlock(jit->function, "if");
    b1 = LLVMAppendBasicBlock(jit->function, "ifbody");
    b2 = LLVMAppendBasicBlock(jit->function, "ebody");
    e = LLVMAppendBasicBlock(jit->function, "ifend");
    LLVMBuildBr(jit->builder, i);
    LLVMPositionBuilderAtEnd(jit->builder, i); 

    LLVMBuildCondBr(jit->builder, obj, b1, b2);
    LLVMPositionBuilderAtEnd(jit->builder, b1);

    /* print "true" */
    llvm_printf(jit, "%s", jit->true_str);

    LLVMBuildBr(jit->builder, e);
    LLVMPositionBuilderAtEnd(jit->builder, b2);  

    /* print "false" */
    llvm_printf(jit, "%s", jit->false_str);

    LLVMBuildBr(jit->builder, e);
    LLVMPositionBuilderAtEnd(jit->builder, e);
}

/*
   This jits a printf for various cesium types. We use it to print
   the result of expressions that are evaluated, before returning from
   a jit'd expression.
*/
void print_obj(jit_t * jit, typ_t typ, LLVMValueRef obj)
{
   switch (typ)
   {
      case INT:
         llvm_printf(jit, "%ld", obj);
         break;
      case CHAR:
         llvm_printf(jit, "%c", obj);
         break;
      case DOUBLE:
         llvm_printf(jit, "%.5g", obj);
         break;
      case STRING:
         llvm_printf(jit, "\"%s\"", obj);
         break;
      case BOOL:
         llvm_printbool(jit, obj);
         break;
      case NIL:
         llvm_printf(jit, "%s", jit->nil_str);
         break;
   }
}

/*
   Initialise the LLVM JIT
*/
jit_t * llvm_init(void)
{
    char * error = NULL;
    
    /* create jit struct */
    jit_t * jit = (jit_t *) GC_MALLOC(sizeof(jit_t));

    /* Jit setup */
    LLVMLinkInJIT();
    LLVMInitializeNativeTarget();

    /* Create module */
    jit->module = LLVMModuleCreateWithName("cesium");

    /* Create JIT engine */
    if (LLVMCreateJITCompilerForModule(&(jit->engine), jit->module, 2, &error) != 0) 
    {   
       fprintf(stderr, "%s\n", error);  
       LLVMDisposeMessage(error);  
       abort();  
    } 
   
    /* Create optimisation pass pipeline */
    jit->pass = LLVMCreateFunctionPassManagerForModule(jit->module);  
    LLVMAddTargetData(LLVMGetExecutionEngineTargetData(jit->engine), jit->pass);  
    LLVMAddConstantPropagationPass(jit->pass);  
    LLVMAddInstructionCombiningPass(jit->pass);  
    LLVMAddPromoteMemoryToRegisterPass(jit->pass);  
    LLVMAddGVNPass(jit->pass);  
    LLVMAddCFGSimplificationPass(jit->pass);

    /* link in external functions callable from jit'd code */
    llvm_functions(jit);

    /* initialise some strings */
    START_EXEC;
    jit->fmt_str = NULL;
    jit->true_str = LLVMBuildGlobalStringPtr(jit->builder, "true", "true");
    jit->false_str = LLVMBuildGlobalStringPtr(jit->builder, "false", "false");
    jit->nil_str = LLVMBuildGlobalStringPtr(jit->builder, "nil", "nil");
    END_EXEC;

    return jit;
}

/*
   If something goes wrong after partially jit'ing something we need
   to clean up.
*/
void llvm_reset(jit_t * jit)
{
    LLVMDeleteFunction(jit->function);
    LLVMDisposeBuilder(jit->builder);
    jit->function = NULL;
    jit->builder = NULL;
}

/*
   Clean up LLVM on exit from Cesium
*/
void llvm_cleanup(jit_t * jit)
{
    /* Clean up */
    LLVMDisposePassManager(jit->pass);  
    LLVMDisposeExecutionEngine(jit->engine); 
    jit->pass = NULL;
    jit->engine = NULL;
    jit->module = NULL;
    jit->fmt_str = NULL;
    jit->nil_str = NULL;
    jit->true_str = NULL;
    jit->false_str = NULL;
}

/* Convert a typ to an LLVMTypeRef */
LLVMTypeRef typ_to_llvm(typ_t typ)
{
   switch (typ)
   {
      case DOUBLE:
         return LLVMDoubleType();
      case INT:
         return LLVMWordType();
      case BOOL:
         return LLVMInt1Type();
      case STRING:
         return LLVMPointerType(LLVMInt8Type(), 0);
      case CHAR:
         return LLVMInt8Type();
      case NIL:
         return LLVMVoidType();
   }
}

/*
   Jit an int literal
*/
int exec_int(jit_t * jit, ast_t * ast)
{
    long num = atol(ast->sym->name);
    
    ast->val = LLVMConstInt(LLVMWordType(), num, 0);

    return 0;
}

/*
   Jit a double literal
*/
int exec_double(jit_t * jit, ast_t * ast)
{
    double num = atof(ast->sym->name);
    
    ast->val = LLVMConstReal(LLVMDoubleType(), num);

    return 0;
}

/*
   Jit a bool
*/
int exec_bool(jit_t * jit, ast_t * ast)
{
    if (strcmp(ast->sym->name, "true") == 0)
        ast->val = LLVMConstInt(LLVMInt1Type(), 1, 0);
    else
        ast->val = LLVMConstInt(LLVMInt1Type(), 0, 0);

    return 0;
}

/*
   Jit a string literali, being careful to replace special 
   characters with their ascii equivalent
*/
int exec_string(jit_t * jit, ast_t * ast)
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
            
        ast->sym->val = LLVMBuildGlobalStringPtr(jit->builder, str, "string");
    }

    ast->val = ast->sym->val;

    return 0;
}

/*
   We have a number of binary ops we want to jit and they
   all look the same, so define macros for them.
*/
#define exec_binary(__name, __fop, __iop, __str)        \
__name(jit_t * jit, ast_t * ast)                        \
{                                                       \
    ast_t * expr1 = ast->child;                         \
    ast_t * expr2 = expr1->next;                        \
                                                        \
    exec_ast(jit, expr1);                               \
    exec_ast(jit, expr2);                               \
                                                        \
    LLVMValueRef v1 = expr1->val, v2 = expr2->val;      \
                                                        \
    if (expr1->type == t_double)                        \
       ast->val = __fop(jit->builder, v1, v2, __str);   \
    else                                                \
       ast->val = __iop(jit->builder, v1, v2, __str);   \
                                                        \
    ast->type = expr1->type;                            \
                                                        \
    return 0;                                           \
}

#define exec_binary_rel(__name, __fop, __frel, __iop, __irel, __str)  \
__name(jit_t * jit, ast_t * ast)                                      \
{                                                                     \
    ast_t * expr1 = ast->child;                                       \
    ast_t * expr2 = expr1->next;                                      \
                                                                      \
    exec_ast(jit, expr1);                                             \
    exec_ast(jit, expr2);                                             \
                                                                      \
    LLVMValueRef v1 = expr1->val, v2 = expr2->val;                    \
                                                                      \
    if (expr1->type == t_double)                                      \
       ast->val = __fop(jit->builder, __frel, v1, v2, __str);         \
    else                                                              \
       ast->val = __iop(jit->builder, __irel, v1, v2, __str);         \
                                                                      \
    return 0;                                                         \
}

#define exec_binary1(__name, __iop, __str)              \
__name(jit_t * jit, ast_t * ast)                        \
{                                                       \
    ast_t * expr1 = ast->child;                         \
    ast_t * expr2 = expr1->next;                        \
                                                        \
    exec_ast(jit, expr1);                               \
    exec_ast(jit, expr2);                               \
                                                        \
    LLVMValueRef v1 = expr1->val, v2 = expr2->val;      \
                                                        \
    ast->val = __iop(jit->builder, v1, v2, __str);      \
                                                        \
    ast->type = expr1->type;                            \
                                                        \
    return 0;                                           \
}

/* Jit add, sub, .... ops */
int exec_binary(exec_plus, LLVMBuildFAdd, LLVMBuildAdd, "add")

int exec_binary(exec_minus, LLVMBuildFSub, LLVMBuildSub, "sub")

int exec_binary(exec_times, LLVMBuildFMul, LLVMBuildMul, "times")

int exec_binary(exec_div, LLVMBuildFDiv, LLVMBuildSDiv, "div")

int exec_binary(exec_mod, LLVMBuildFRem, LLVMBuildSRem, "mod")

int exec_binary1(exec_lsh, LLVMBuildShl, "lsh")

int exec_binary1(exec_rsh, LLVMBuildAShr, "rsh")

int exec_binary1(exec_bitor, LLVMBuildOr, "bitor")

int exec_binary1(exec_bitand, LLVMBuildAnd, "bitand")

int exec_binary1(exec_bitxor, LLVMBuildXor, "bitxor")

int exec_binary1(exec_logand, LLVMBuildAnd, "logand")

int exec_binary1(exec_logor, LLVMBuildOr, "logor")

int exec_binary_rel(exec_le, LLVMBuildFCmp, LLVMRealOLE, LLVMBuildICmp, LLVMIntSLE, "le")

int exec_binary_rel(exec_ge, LLVMBuildFCmp, LLVMRealOGE, LLVMBuildICmp, LLVMIntSGE, "ge")

int exec_binary_rel(exec_lt, LLVMBuildFCmp, LLVMRealOLT, LLVMBuildICmp, LLVMIntSLT, "lt")

int exec_binary_rel(exec_gt, LLVMBuildFCmp, LLVMRealOGT, LLVMBuildICmp, LLVMIntSGT, "gt")

int exec_binary_rel(exec_eq, LLVMBuildFCmp, LLVMRealOEQ, LLVMBuildICmp, LLVMIntEQ, "eq")

int exec_binary_rel(exec_ne, LLVMBuildFCmp, LLVMRealONE, LLVMBuildICmp, LLVMIntNE, "ne")

/*
   Load an identifier
*/
int exec_load(jit_t * jit, ast_t * ast)
{
    bind_t * bind = find_symbol(ast->sym);
    
    if (ast->val == NULL)
    {
        ast->type = bind->type;
        ast->val = LLVMBuildLoad(jit->builder, bind->val, bind->sym->name);
    } else
    {
        subst_type(&ast->type);
        ast->val = LLVMBuildLoad(jit->builder, ast->val, ast->sym->name);
    }
    
    return 0;
}

/*
   Find an identifier
*/
int exec_ident(jit_t * jit, ast_t * ast)
{
    bind_t * bind = find_symbol(ast->sym);
        
    if (bind->type->typ == TYPEVAR) /* we don't know what type it is */
    {
        subst_type(&bind->type); /* fill in the type */

        if (bind->type->typ != TYPEVAR) /* if we now know what it is */
        {
            LLVMTypeRef type = typ_to_llvm(bind->type->typ);
                
            if (scope_is_global()) /* variable is global */
                bind->val = LLVMAddGlobal(jit->module, type, bind->sym->name);             
            else
                bind->val = LLVMBuildAlloca(jit->builder, type, bind->sym->name);

            LLVMSetInitializer(bind->val, LLVMGetUndef(type));
        }   
    }
        
    ast->type = bind->type;
    ast->val = bind->val;
   
    return 0;
}

/*
   Jit a variable assignment
*/
int exec_assignment(jit_t * jit, ast_t * ast)
{
    ast_t * id = ast->child;
    ast_t * exp = ast->child->next;
    
    exec_ast(jit, exp);
    exec_ident(jit, id);
    
    LLVMBuildStore(jit->builder, exp->val, id->val);

    ast->val = exp->val;
    ast->type = exp->type;

    return 0;
}

/*
   Declare local/global variables
*/
int exec_varassign(jit_t * jit, ast_t * ast)
{
    ast_t * c = ast->child;

    while (c != NULL)
    {
        if (c->tag == AST_IDENT)
            exec_ident(jit, c);
        else /* AST_ASSIGNMENT */
            exec_assignment(jit, c);
                    
        c = c->next;
    }

    ast->type = t_nil;

    return 0;
}

/*
   As we traverse the ast we dispatch on ast tag to various jit 
   functions defined above
*/
int exec_ast(jit_t * jit, ast_t * ast)
{
    switch (ast->tag)
    {
    case AST_INT:
        return exec_int(jit, ast);
    case AST_DOUBLE:
        return exec_double(jit, ast);
    case AST_STRING:
        return exec_string(jit, ast);
    case AST_BOOL:
        return exec_bool(jit, ast);
    case AST_PLUS:
        return exec_plus(jit, ast);
    case AST_MINUS:
        return exec_minus(jit, ast);
    case AST_TIMES:
        return exec_times(jit, ast);
    case AST_DIV:
        return exec_div(jit, ast);
    case AST_MOD:
        return exec_mod(jit, ast);
    case AST_LSH:
        return exec_lsh(jit, ast);
    case AST_RSH:
        return exec_rsh(jit, ast);
    case AST_BITOR:
        return exec_bitor(jit, ast);
    case AST_BITAND:
        return exec_bitand(jit, ast);
    case AST_BITXOR:
        return exec_bitxor(jit, ast);
    case AST_VARASSIGN:
        return exec_varassign(jit, ast);
    case AST_ASSIGNMENT:
        return exec_assignment(jit, ast);
    case AST_IDENT:
        return exec_load(jit, ast);
    case AST_LE:
        return exec_le(jit, ast);
    case AST_GE:
        return exec_ge(jit, ast);
    case AST_LT:
        return exec_lt(jit, ast);
    case AST_GT:
        return exec_gt(jit, ast);
    case AST_EQ:
        return exec_eq(jit, ast);
    case AST_NE:
        return exec_ne(jit, ast);
    case AST_LOGAND:
        return exec_logand(jit, ast);
    case AST_LOGOR:
        return exec_logor(jit, ast);
    default:
        ast->type = t_nil;
        return 0;
    }
}

/* We start traversing the ast to do jit'ing here */
void exec_root(jit_t * jit, ast_t * ast)
{
    /* Traverse the ast jit'ing everything, then run the jit'd code */
    START_EXEC;
         
    exec_ast(jit, ast);

    print_obj(jit, ast->type->typ, ast->val);
    
    END_EXEC;
         
    /* 
       If our jit side print_obj had to create a format string
       clean it up now that we've executed the jit'd code.
    */
    if (jit->fmt_str)
    {
        LLVMDeleteGlobal(jit->fmt_str);
        jit->fmt_str = NULL;
    }
}

