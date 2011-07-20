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

/* 
   Go through each node of the AST and do the following things:
   1) Generate type variables for each node where the type is not known
   2) Push relations onto the stack of type relations that get passed to unify
   3) Add a type to the ast node corresponding to either the known type of the
      node or the generated type variable for the node
   4) Bind symbols and set ast->bind to the correct binding
*/
void annotate_ast(ast_t * a)
{
    ast_t * t, * t2, * p, * id, * expr;
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
        if (b != NULL && b->initialised) /* ensure initialised and exists */
        {
            a->type = b->type;
            a->bind = b; /* make sure we use the right binding */
        } else
           exception("Unbound symbol\n");
        break;
    case AST_LVALUE:
        b = find_symbol(a->sym);
        if (b != NULL) /* ensure it exists */
        {
            a->type = b->type;
            a->bind = b; /* make sure we use the right binding */
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
        annotate_ast(a->child);
        a->type = a->child->type;
        push_type_rel(a->type, a->child->next->type);
        break;
    case AST_ASSIGNMENT:
        id = a->child;
        expr = id->next;
        annotate_ast(expr);
        annotate_ast(id);
        a->type = expr->type;
        push_type_rel(id->type, expr->type);
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
        p = a->child;
        scope_mark();
        while (p != NULL)
        {
            if (p->tag == AST_ASSIGNMENT)
            {
                id = p->child;
                expr = id->next;
            } else /* AST_IDENT */
                id = p;
            
            id->type = new_typevar(); /* type is to be inferred */            
            
            /* check we're not redefining a local symbol in the current scope */
            bind = find_symbol_in_scope(id->sym);
            if (bind != NULL && !scope_is_global(bind))
                exception("Attempt to redefine local symbol\n");
              
            if (p->tag == AST_ASSIGNMENT)
               annotate_ast(expr); /* get expr before anything is redefined */
            
            /* make new binding in the current scope*/
            bind = bind_symbol(id->sym, id->type, NULL);
            
            /* check if it is a combined variable declaration and assignment */
            if (p->tag == AST_ASSIGNMENT)
            {
                bind->initialised = 1; /* mark it initialised */
                annotate_ast(id);
                p->type = expr->type;
                push_type_rel(id->type, expr->type);
            } else
            {
                bind->initialised = 0; /* mark it uninitialised */
                p->bind = bind;
            }

            p = p->next;
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
            push_type_rel(b->type, t_nil);
        else
        {
            annotate_ast(a->child);
            push_type_rel(b->type, a->child->type);
        }
        a->type = t_nil;
        break;
    case AST_FNDEC:
        id = a->child;
        p = id->next->child; 
        count = ast_list_length(p); /* count parameters */
        
        current_scope = a->env;
        
        /* add parameters into scope */
        param = (type_t **) GC_MALLOC(count*sizeof(type_t *));
        for (i = 0; i < count; i++)
        {
           param[i] = p->type = new_typevar(); /* set types */
           
           if (find_symbol_in_scope(p->sym)) /* check parameter name */
               exception("Parameter name already in use\n");

           bind = bind_symbol(p->sym, p->type, NULL); /* bind symbol */
           bind->initialised = 1;
           
           p = p->next;
        }

        /* put return into scope */
        sym = sym_lookup("return");
        if (find_symbol_in_scope(sym))
            exception("Use of reserved word \"return\" as a parameter name\n");
        retty = new_typevar();
        bind = bind_symbol(sym, retty, NULL);

        scope_down();
        
        /* Add function to global scope */
        id->type = fn_type(retty, count, param);
        bind = bind_lambda(id->sym, id->type, a);
        bind->initialised = 1;

        current_scope = a->env;
        
        /* process function body */
        expr = id->next->next;
        expr->type = t_nil;
        expr = expr->child;
        while (expr != NULL)
        {
            annotate_ast(expr);
            expr = expr->next;
        }
        
        scope_down();

        a->type = id->type;
        break;
    case AST_APPL:
        id = a->child;

        /* find function and get particulars */
        bind = find_symbol(id->sym);
        if (bind != NULL)
        {
            id->type = bind->type;
            id->bind = bind;
        } else
           exception("Unknown function\n");

        /* count arguments */
        p = id->next;
        count = ast_list_length(p);
        
        /* the function may be passed via a parameter whose type is a typevar */
        if (id->type->typ != FN) 
        {
            /* build appropriate function type */
            param = (type_t **) GC_MALLOC(count*sizeof(type_t *));
            for (i = 0; i < count; i++)
                param[i] = new_typevar();
            type_t * fn_ty = fn_type(new_typevar(), count, param);

            /* use it instead and infer the typevar */
            push_type_rel(id->type, fn_ty);
            id->type = fn_ty;
        } else if (count != id->type->arity) /* check number of parameters */
            exception("Wrong number of parameters in function\n"); 

        /* get parameter types */
        for (i = 0; i < count; i++)
        {
            annotate_ast(p);
            push_type_rel(id->type->param[i], p->type);
            p = p->next;
        }

        /* get return types */
        a->type = id->type->ret;
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
