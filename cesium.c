#include <stdio.h>
#include <setjmp.h>

#include "gc.h"
#include "symbol.h"
#include "ast.h"
#include "types.h"
#include "unify.h"
#include "environment.h"

#include "parser.c"

#include <llvm-c/Core.h>  
#include <llvm-c/Analysis.h>  
#include <llvm-c/ExecutionEngine.h>  
#include <llvm-c/Target.h>  
#include <llvm-c/Transforms/Scalar.h> 

#include "backend.h"
#include "exception.h"

extern jmp_buf exc;

int main(void) {
    GC_INIT();
    GREG g;
 
    int jval, jval2;
    char c;

    sym_tab_init();
    ast_init();
    type_init();
    scope_init();
    rel_assign_init();

    jit_t * jit = llvm_init();

    yyinit(&g);

    printf("Welcome to Cesium v 0.2\n");
    printf("To exit press CTRL-D\n\n");

    printf("> ");

    while (1)
    {
       if (!(jval = setjmp(exc)))
       {
          if (!yyparse(&g))
          {
             printf("Error parsing\n");
             abort();
          } else if (root != NULL)
          {
             rel_stack_init();
             rel_assign_mark();
             scope_mark();
             annotate_ast(root);
             if (TRACE) 
                ast_print(root, 0);
             unify(rel_stack, rel_assign);
             if (TRACE)
                print_assigns(rel_assign);
             exec_root(jit, root);
             if (root->tag != AST_FNDEC)
                 rel_assign_rewind();
           }
        } else if (jval == 1)
              root = NULL;
        else if (jval == 2)
              break;
        printf("\n> ");
    }
  
    yydeinit(&g);
    llvm_cleanup(jit);

    return 0;
}

