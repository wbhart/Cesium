#include <stdlib.h>
#include <stdio.h>
#include "gc.h"
#include "types.h"
#include "ast.h"

ast_t * root;

ast_t * op_plus;
ast_t * op_minus;
ast_t * op_times;
ast_t * op_div;
ast_t * op_mod;

void ast_init(void)
{
    op_plus = ast_op(AST_PLUS);
    op_minus = ast_op(AST_MINUS);
    op_times = ast_op(AST_TIMES);
    op_div = ast_op(AST_DIV);
    op_mod = ast_op(AST_MOD);
}

ast_t * new_ast(void)
{
   return GC_MALLOC(sizeof(ast_t));
}

ast_t * ast_symbol(sym_t * sym, tag_t tag)
{
   ast_t * ast = new_ast();
   ast->tag = tag;
   ast->sym = sym;
   return ast;
}

ast_t * ast_unary(ast_t * a, tag_t tag)
{
   ast_t * ast = new_ast();
   ast->tag = tag;
   ast->child = a;
   return ast;
}

ast_t * ast_binary(ast_t * a1, ast_t * a2, ast_t * op)
{
   ast_t * ast = new_ast();
   ast->tag = op->tag;
   ast->child = a1;
   a1->next = a2;
   return ast;
}

ast_t * ast_op(tag_t tag)
{
   ast_t * ast = new_ast();
   ast->tag = tag;
   return ast;
}

ast_t * ast_reverse(ast_t * a)
{
    ast_t * t = a, * t2;

    if (a == NULL)
        return NULL;

    a = a->next;
    t->next = NULL;
    
    while (a != NULL)
    {
        t2 = a->next;
        a->next = t;
        t = a;
        a = t2;
    }

    return t;
}

void ast_print_op(ast_t * a)
{
    switch (a->tag)
    {
    case AST_PLUS:
        printf("plus");
        break;
    case AST_MINUS:
        printf("minus");
        break;
    case AST_TIMES:
        printf("times");
        break;
    case AST_DIV:
        printf("div");
        break;
    case AST_MOD:
        printf("mod");
        break;
    }
}

void ast_print_type(ast_t * a)
{
    printf("<");
    print_type(a->type);
    printf(">");
}

void ast_print(ast_t * a, int indent)
{
    int i;
    ast_t * t;
    
    if (a == NULL)
        return;

    for (i = 0; i < indent; i++)
        printf(" ");
    printf("+ ");

    switch (a->tag)
    {
    case AST_IDENT:
        printf("ident(%s)", a->sym->name);
        ast_print_type(a);
        printf("\n");
        break;
    case AST_INT:
        printf("int(%s)", a->sym->name);
        ast_print_type(a);
        printf("\n");
        break;
    case AST_DOUBLE:
        printf("double(%s)", a->sym->name);
        ast_print_type(a);
        printf("\n");
        break;
    case AST_STRING:
        printf("string(%s)", a->sym->name);
        ast_print_type(a);
        printf("\n");
        break;
    case AST_POST_INC:
        printf("postinc");
        ast_print_type(a);
        printf("\n");
        ast_print(a->child, indent + 3);
        break;
    case AST_POST_DEC:
        printf("postdec");
        ast_print_type(a);
        printf("\n");
        ast_print(a->child, indent + 3);
        break;
    case AST_PLUS:
    case AST_MINUS:
    case AST_TIMES:
    case AST_DIV:
    case AST_MOD:
        ast_print_op(a);
        ast_print_type(a);
        printf("\n");
        ast_print(a->child, indent + 3);
        ast_print(a->child->next, indent + 3);
        break;
    case AST_ASSIGNMENT:
        printf("assignment");
        ast_print_type(a);
        printf("\n");
        ast_print(a->child, indent + 3);
        ast_print(a->child->next, indent + 3);
        break;
    case AST_VARASSIGN:
        printf("varassign");
        printf("\n");
        t = a->child;
        while (t != NULL)
        {
            ast_print(t, indent + 3);
            t = t->next;
        }
        break;
    }
}
