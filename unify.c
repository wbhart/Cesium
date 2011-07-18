#include <stdlib.h>
#include <stdio.h>
#include "ast.h"
#include "types.h"
#include "unify.h"
#include "environment.h"
#include "gc.h"
#include "exception.h"

#include <llvm-c/Core.h>  
#include <llvm-c/Analysis.h>  
#include <llvm-c/ExecutionEngine.h>  
#include <llvm-c/Target.h>  
#include <llvm-c/Transforms/Scalar.h> 

type_rel_t * rel_stack;
type_rel_t * rel_assign;

void rel_stack_init(void)
{
   rel_stack = NULL;
   rel_assign = NULL;
}

void push_type_rel(type_t * t1, type_t * t2)
{
   type_rel_t * t = (type_rel_t *) GC_MALLOC(sizeof(type_rel_t));
   t->t1 = t1;
   t->t2 = t2;
   t->next = rel_stack;
   rel_stack = t;
}

type_rel_t * pop_type_rel(void)
{
   if (rel_stack == NULL)
      exception("Attempt to pop empty rel_stack!\n");

   type_rel_t * t = rel_stack;
   rel_stack = t->next;
   t->next = NULL;
   return t;
}

void type_subst_type(type_t ** tin, type_rel_t * rel)
{
    type_t * t = *tin;
    int i;

    if (t == NULL)
        return;
    
    if (t->typ == TYPEVAR)
    {
        if (*tin == rel->t1)
            *tin = rel->t2;
    }
    else if (t->typ == FN)
    {
        for (i = 0; i < t->arity; i++)
            type_subst_type(t->param + i, rel);
        if (t->ret != NULL)
            type_subst_type(&(t->ret), rel);
    }
}

void subst_type(type_t ** tin)
{
    type_rel_t * rel = rel_assign;
   
    while (rel != NULL)
    {
        type_subst_type(tin, rel);
        rel = rel->next;
    }
}

void rels_subst(type_rel_t * rels, type_rel_t * rel)
{
    while (rels != NULL)
    {
        type_subst_type(&(rels->t1), rel);
        type_subst_type(&(rels->t2), rel);
        rels = rels->next;
    }
}

void ass_subst(type_rel_t * rels, type_rel_t * rel)
{
    while (rels != NULL)
    {
        type_subst_type(&(rels->t2), rel);
        rels = rels->next;
    }
}

void unify(type_rel_t * rels, type_rel_t * ass)
{
    int i;
    
    while (rel_stack != NULL)
    {
        type_rel_t * rel = pop_type_rel();
        if (rel->t1 == rel->t2)
            continue;
        else if (rel->t1->typ == TYPEVAR)
        {
            rels_subst(rel_stack, rel);
            ass_subst(rel_assign, rel);
            rel->next = rel_assign;
            rel_assign = rel;
        } else if (rel->t2->typ == TYPEVAR)
            push_type_rel(rel->t2, rel->t1);
        else if (rel->t1->typ == FN)
        {
            if (rel->t2->typ != FN || rel->t1->arity != rel->t2->arity)
                exception("Type mismatch: function type not matched!\n");
            push_type_rel(rel->t1->ret, rel->t2->ret);
            for (i = 0; i < rel->t1->arity; i++)
                push_type_rel(rel->t1->param[i], rel->t2->param[i]);
        }
        else if (rel->t1->typ != rel->t2->typ)
           exception("Type mismatch!\n");
    }
}

void annotate_ast(ast_t * a)
{
    ast_t * t, *t2, * p;
    bind_t * b;
    type_t * retty;
    type_t ** param;
    bind_t * bind;
    sym_t * sym;
    int count, i;

    switch (a->tag)
    {
    case AST_IDENT:
        b = find_symbol(a->sym);
        if (b != NULL && b->type != t_nil)
        {
            a->type = b->type;
            a->val = b->val;
        } else
           exception("Unbound symbol\n");
        break;
    case AST_LVALUE:
        a->tag = AST_IDENT;
        b = find_symbol(a->sym);
        if (b != NULL)
        {
             if (b->type == t_nil)
             {
                 a->type = new_typevar();
                 b->type = a->type;
             }
             else
                 a->type = b->type;
             a->val = b->val;
        } else
           exception("Unbound symbol\n");
        break;
    case AST_DOUBLE:
        a->type = t_double;
        break;
    case AST_INT:
        a->type = t_int;
        break;
    case AST_BOOL:
        a->type = t_bool;
        break;
    case AST_STRING:
        a->type = t_string;
        break;
    case AST_POST_INC:
    case AST_POST_DEC:
    case AST_PRE_INC:
    case AST_PRE_DEC:
    case AST_LOGNOT:
    case AST_BITNOT:
    case AST_UNMINUS:
        annotate_ast(a->child);
        a->type = a->child->type;
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
    case AST_ASSIGNMENT:
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
        annotate_ast(a->child->next);
        if (a->tag == AST_ASSIGNMENT)
            a->child->tag = AST_LVALUE;
        annotate_ast(a->child);
        a->type = a->child->type;
        push_type_rel(a->type, a->child->next->type);
        break;
    case AST_LT:
    case AST_GT:
    case AST_LE:
    case AST_GE:
    case AST_EQ:
    case AST_NE:
        annotate_ast(a->child->next);
        annotate_ast(a->child);
        a->type = t_bool;
        push_type_rel(a->child->type, a->child->next->type);
        break;
    case AST_VARASSIGN:
        t = a->child;
        scope_mark();
        while (t != NULL)
        {
            bind_t * bind;
            sym_t * sym;
            if (t->tag == AST_ASSIGNMENT)
            {
                annotate_ast(t->child->next);
                t->type = new_typevar();
                t->child->type = t->type;
                sym = t->child->sym;
            } else
            {
                sym = t->sym;
                t->type = t_nil;
            }
            
            bind = find_symbol_in_scope(sym);
            if (bind != NULL)
            {
                if (!scope_is_global(bind))
                    exception("Attempt to redefine local symbol\n");

                ast_t * s = a->child;
                sym_t * sym2;
                while (s != t)
                {
                    if (s->tag == AST_ASSIGNMENT)
                        sym2 = s->child->sym;
                    else
                        sym2 = s->sym;
                    if (sym == sym2)
                        exception("Immediate redefinition of symbol\n");
                    s = s->next;
                }
            }
                
            bind_symbol(sym, t->type, NULL);
            if (t->tag == AST_ASSIGNMENT)
                push_type_rel(t->type, t->child->next->type);
            
            t = t->next;
        }
        a->type = t_nil;
        break;
    case AST_IF:
    case AST_WHILE:
        annotate_ast(a->child);
        annotate_ast(a->child->next);
        a->type = t_nil;
        push_type_rel(a->child->type, t_bool);
        break;
    case AST_IFELSE:
        annotate_ast(a->child);
        annotate_ast(a->child->next);
        annotate_ast(a->child->next->next);
        a->type = t_nil;
        push_type_rel(a->child->type, t_bool);
        break;
    case AST_BLOCK:
        t = a->child;
        current_scope = a->env;
        while (t != NULL)
        {
            annotate_ast(t);
            t = t->next;
        }
        scope_down();
        a->type = t_nil;
        break;
    case AST_BREAK:
        a->type = t_nil;
        break;
    case AST_RETURN:
        b = find_symbol(a->sym);
        if (b == NULL)
            exception("Returning outside a function\n");
        if (a->child == NULL)
            a->type = t_nil;
        else
        {
           annotate_ast(a->child);
           push_type_rel(b->type, a->child->type);
           a->type = t_nil;
        }
        break;
    case AST_FNDEC:
        count = 0;
        t = a->child->next;
        scope_mark();

        p = t->child; /* count params */
        if (p->tag != AST_NIL)
        {
            while (p != NULL)
            {
                count++;
                p = p->next;
            }
        }

        /* inject parameters into scope */
        current_scope = a->env;
        param = (type_t **) GC_MALLOC(count*sizeof(type_t *));
        p = t->child;
        i = 0;
        while (i < count)
        {
           p->type = new_typevar();
           sym = p->sym;
           bind = find_symbol_in_scope(sym);
           if (bind != NULL)
               exception("Parameter name already in use\n");
           bind_symbol(sym, p->type, NULL);
           param[i] = p->type;

           p = p->next;
           i++;
        }
        sym = sym_lookup("return");
        bind = find_symbol_in_scope(sym);
        if (bind != NULL)
            exception("Use of reserved word \"return\" as a parameter name\n");
        retty = new_typevar();
        bind_symbol(sym, retty, NULL);
        scope_down();
        
        /* Add function to global scope */
        t = a->child;
        t->type = fn_type(retty, count, param);
        sym = t->sym;
        bind_lambda(sym, t->type, a);

        /* process function block */
        t = t->next->next;
        current_scope = a->env;
        t->type = t_nil;
        t = t->child;
        while (t != NULL)
        {
            annotate_ast(t);
            t = t->next;
        }
        
        scope_down();
        a->type = a->child->type;
        break;
    case AST_APPL:
        t = a->child;
        b = find_symbol(t->sym);
        if (b != NULL)
            t->type = b->type;
        else
           exception("Unknown function\n");
        count = 0;
        p = t->next;
        if (p->tag != AST_NIL)
        {
            while (p != NULL) /* count params */
            {
                count++;
                p = p->next;
            }
        }
        if (t->type->typ != FN)
        {
            param = (type_t **) GC_MALLOC(count*sizeof(type_t *));
            for (i = 0; i < count; i++)
                param[i] = new_typevar();
            type_t * t2 = fn_type(new_typevar(), count, param);
            push_type_rel(t->type, t2);
            t->type = t2;
        }
        i = 0;
        p = t->next;
        while (i < count)
        {
            annotate_ast(p);
            push_type_rel(t->type->param[i], p->type);
            i++;
            p = p->next;
        }
        a->type = new_typevar();
        push_type_rel(a->type, t->type->ret);
        break;
    }
}

void print_assigns(type_rel_t * ass)
{
    while (ass != NULL)
    {
        print_type(ass->t1);
        printf(" = ");
        print_type(ass->t2);
        printf(";\n");
        ass = ass->next;
    }
}
