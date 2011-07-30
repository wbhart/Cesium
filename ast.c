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
ast_t * op_lsh;
ast_t * op_rsh;
ast_t * op_bitor;
ast_t * op_bitand;
ast_t * op_bitxor;
ast_t * op_le;
ast_t * op_ge;
ast_t * op_lt;
ast_t * op_gt;
ast_t * op_eq;
ast_t * op_ne;
ast_t * op_logand;
ast_t * op_logor;
ast_t * op_pluseq;
ast_t * op_minuseq;
ast_t * op_timeseq;
ast_t * op_diveq;
ast_t * op_modeq;
ast_t * op_andeq;
ast_t * op_oreq;
ast_t * op_xoreq;
ast_t * op_lsheq;
ast_t * op_rsheq;

void ast_init(void)
{
    op_plus = ast_op(AST_PLUS);
    op_minus = ast_op(AST_MINUS);
    op_times = ast_op(AST_TIMES);
    op_div = ast_op(AST_DIV);
    op_mod = ast_op(AST_MOD);
    op_lsh = ast_op(AST_LSH);
    op_rsh = ast_op(AST_RSH);
    op_bitor = ast_op(AST_BITOR);
    op_bitand = ast_op(AST_BITAND);
    op_bitxor = ast_op(AST_BITXOR);
    op_le = ast_op(AST_LE);
    op_ge = ast_op(AST_GE);
    op_lt = ast_op(AST_LT);
    op_gt = ast_op(AST_GT);
    op_eq = ast_op(AST_EQ);
    op_ne = ast_op(AST_NE);
    op_logand = ast_op(AST_LOGAND);
    op_logor = ast_op(AST_LOGOR);
    op_pluseq = ast_op(AST_PLUSEQ);
    op_minuseq = ast_op(AST_MINUSEQ);
    op_timeseq = ast_op(AST_TIMESEQ);
    op_diveq = ast_op(AST_DIVEQ);
    op_modeq = ast_op(AST_MODEQ);
    op_andeq = ast_op(AST_ANDEQ);
    op_oreq = ast_op(AST_OREQ);
    op_xoreq = ast_op(AST_XOREQ);
    op_lsheq = ast_op(AST_LSHEQ);
    op_rsheq = ast_op(AST_RSHEQ);
}

ast_t * new_ast(void)
{
   return (ast_t *) GC_MALLOC(sizeof(ast_t));
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

ast_t * ast_stmt0(tag_t tag)
{
   ast_t * ast = new_ast();
   ast->tag = tag;
   return ast;
}

ast_t * ast_stmt2(ast_t * a1, ast_t * a2, tag_t tag)
{
   ast_t * ast = new_ast();
   ast->tag = tag;
   ast->child = a1;
   a1->next = a2;
   return ast;
}

ast_t * ast_stmt3(ast_t * a1, ast_t * a2, ast_t * a3, tag_t tag)
{
   ast_t * ast = new_ast();
   ast->tag = tag;
   ast->child = a1;
   a1->next = a2;
   a2->next = a3;
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

int ast_list_length(ast_t * p)
{
    int count = 0;
    
    if (p->tag != AST_NIL)
    {
        while (p != NULL)
        {
            count++;
            p = p->next;
        }
    }

    return count;
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
    case AST_LSH:
        printf("lsh");
        break;
    case AST_RSH:
        printf("rsh");
        break;
    case AST_BITOR:
        printf("bitor");
        break;
    case AST_BITAND:
        printf("bitand");
        break;
    case AST_BITXOR:
        printf("bitxor");
        break;
    case AST_LE:
        printf("le");
        break;
    case AST_GE:
        printf("ge");
        break;
    case AST_LT:
        printf("lt");
        break;
    case AST_GT:
        printf("gt");
        break;
    case AST_EQ:
        printf("eq");
        break;
    case AST_NE:
        printf("ne");
        break;
    case AST_LOGAND:
        printf("logand");
        break;
    case AST_LOGOR:
        printf("logor");
        break;
    case AST_LOGNOT:
        printf("lognot");
        break;
    case AST_BITNOT:
        printf("bitnot");
        break;
    case AST_UNMINUS:
        printf("unary-minus");
        break;
    case AST_POST_INC:
        printf("post-inc");
        break;
    case AST_POST_DEC:
        printf("post-dec");
        break;
    case AST_PRE_INC:
        printf("pre-inc");
        break;
    case AST_PRE_DEC:
        printf("pre-dec");
        break;
    case AST_PLUSEQ:
        printf("pluseq");
        break;
    case AST_MINUSEQ:
        printf("minuseq");
        break;
    case AST_TIMESEQ:
        printf("timeseq");
        break;
    case AST_DIVEQ:
        printf("diveq");
        break;
    case AST_MODEQ:
        printf("modeq");
        break;
    case AST_ANDEQ:
        printf("andeq");
        break;
    case AST_OREQ:
        printf("oreq");
        break;
    case AST_XOREQ:
        printf("xoreq");
        break;
    case AST_LSHEQ:
        printf("lsheq");
        break;
    case AST_RSHEQ:
        printf("rsheq");
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
    case AST_LVALUE:
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
    case AST_BOOL:
        printf("bool(%s)", a->sym->name);
        ast_print_type(a);
        printf("\n");
        break;
    case AST_POST_INC:
    case AST_POST_DEC:
    case AST_PRE_INC:
    case AST_PRE_DEC:
    case AST_LOGNOT:
        ast_print_op(a);
        ast_print_type(a);
        printf("\n");
        ast_print(a->child, indent + 3);
        break;
    case AST_PLUS:
    case AST_MINUS:
    case AST_TIMES:
    case AST_DIV:
    case AST_MOD:
    case AST_LSH:
    case AST_RSH:
    case AST_BITOR:
    case AST_BITAND:
    case AST_BITXOR:
    case AST_LE:
    case AST_GE:
    case AST_LT:
    case AST_GT:
    case AST_EQ:
    case AST_NE:
    case AST_LOGAND:
    case AST_LOGOR:
    case AST_PLUSEQ:
    case AST_MINUSEQ:
    case AST_TIMESEQ:
    case AST_DIVEQ:
    case AST_MODEQ:
    case AST_ANDEQ:
    case AST_OREQ:
    case AST_XOREQ:
    case AST_LSHEQ:
    case AST_RSHEQ:
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
    case AST_PARAMS:
        printf("params");
        printf("\n");
        t = a->child;
        while (t != NULL)
        {
            ast_print(t, indent + 3);
            t = t->next;
        }
        break;
    case AST_IF:
        printf("if");
        printf("\n");
        ast_print(a->child, indent + 3);
        ast_print(a->child->next, indent + 3);
        break;
    case AST_IFELSE:
        printf("if_else");
        printf("\n");
        ast_print(a->child, indent + 3);
        ast_print(a->child->next, indent + 3);
        ast_print(a->child->next->next, indent + 3);
        break;
    case AST_IFEXPR:
        printf("if_expr");
        printf("\n");
        ast_print(a->child, indent + 3);
        ast_print(a->child->next, indent + 3);
        ast_print(a->child->next->next, indent + 3);
        break;
    case AST_BLOCK:
    case AST_FNBLOCK:
    case AST_EXPRBLOCK:
        printf("block");
        printf("\n");
        t = a->child;
        while (t != NULL)
        {
            ast_print(t, indent + 3);
            t = t->next;
        }
        break;
    case AST_WHILE:
        printf("while");
        printf("\n");
        ast_print(a->child, indent + 3);
        ast_print(a->child->next, indent + 3);
        break;
    case AST_BREAK:
        printf("break");
        printf("\n");
        break;
    case AST_RETURN:
        printf("return");
        printf("\n");
        t = a->child;
        if (t != NULL)
            ast_print(t, indent + 3);
        break;
    case AST_FNDEC:
        printf("fn");
        ast_print_type(a);
        printf("\n");
        ast_print(a->child, indent + 3);
        ast_print(a->child->next, indent + 3);
        ast_print(a->child->next->next, indent + 3);
        printf("\n");
        break;
    case AST_LAMBDA:
        printf("lambda");
        ast_print_type(a);
        printf("\n");
        ast_print(a->child, indent + 3);
        ast_print(a->child->next, indent + 3);
        printf("\n");
        break;
    case AST_APPL:
        printf("appl");
        ast_print_type(a);
        printf("\n");
        ast_print(a->child, indent + 3);
        t = a->child->next;
        while (t != NULL)
        {
            ast_print(t, indent + 3);
            t = t->next;
        }
        break;
    case AST_TUPLE:
    case AST_LTUPLE:
        printf("tuple");
        ast_print_type(a);
        printf("\n");
        ast_print(a->child, indent + 3);
        t = a->child->next;
        while (t != NULL)
        {
            ast_print(t, indent + 3);
            t = t->next;
        }
        break;
    default:
        printf("nil\n");
    }
}
