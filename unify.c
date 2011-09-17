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
type_rel_t * rel_assign_save;

void rel_stack_init(void)
{
    rel_stack = NULL;
}

void rel_assign_init(void)
{
    rel_stack = NULL;
    rel_assign_save = NULL;
}

void rel_assign_mark(void)
{
    rel_assign_save = rel_assign;
}

void rel_assign_rewind(void)
{
    while (rel_assign != rel_assign_save)
        rel_assign = rel_assign->next;
}

void merge_datatypes(type_t ** pt1, type_t ** pt2)
{
    type_t * t1 = *pt1;
    type_t * t2 = *pt2;
    int a1 = t1->arity;
    int a2 = t2->arity;
    int i, j;

    t1->param = (type_t **) GC_REALLOC(t1->param, (a1+a2)*sizeof(type_t *));
    t1->slot = (sym_t **) GC_REALLOC(t1->slot, (a1+a2)*sizeof(sym_t *));
                   
    for (i = 0; i < a2; i++)
    {
        for (j = 0; j < a1; j++)
            if (t1->slot[j] == t2->slot[i]) /* we have it */
                break;
        if (j == a1) /* didn't find it */
        {
            t1->param[a1] = t2->param[i]; /* so add it in */
            t1->slot[a1] = t2->slot[i];
            a1++;
        }
    }
    t1->arity = a1;
                    
    type_rel_t * rel = (type_rel_t *) GC_MALLOC(sizeof(type_rel_t));
    rel->t1 = t1;
    rel->t2 = t2;
    rels_subst(rel_stack, rel);
    ass_subst(rel_assign, rel);

    *pt2 = *pt1;
}

void push_type_rel(type_t ** t1, type_t ** t2)
{
   if ((*t1)->typ == DATATYPE && (*t2)->typ == DATATYPE
       && (*t1)->sym == NULL && (*t2)->sym == NULL)
       merge_datatypes(t1, t2);
   else {
       type_rel_t * t = (type_rel_t *) GC_MALLOC(sizeof(type_rel_t));
       t->t1 = *t1;
       t->t2 = *t2;
       t->next = rel_stack;
       rel_stack = t;
   }
}

void push_type_ass(type_rel_t * rel)
{
    check_recursion(rel);
    if (rel->t1->typ == DATATYPE && rel->t2->typ == DATATYPE
       && rel->t1->sym == NULL && rel->t2->sym == NULL)
       merge_datatypes(&rel->t1, &rel->t2);
    else 
    {
        rel->next = rel_assign;
        rel_assign = rel;
    }
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

int is_recursive(type_t * t, type_t * rec)
{
    int i;

    if (t == rec) return 1;
    
    if (t == NULL || t->typ == TYPEVAR || t->recursive != 0)
        return 0;

    if (t->typ == FN || t->typ == LAMBDA)
    {
        for (i = 0; i < t->arity; i++)
            if (is_recursive(t->param[i], rec))
                return 1;
        return is_recursive(t->ret, rec);
    } else if (t->typ == TUPLE || t->typ == DATATYPE)
    {
        for (i = 0; i < t->arity; i++)
        {
            if (is_recursive(t->param[i], rec))
                return 1;
        }
    } else if (t->typ == ARRAY)
        return is_recursive(t->ret, rec);
}

type_t * make_recursive(type_t * t, type_t * rec)
{
    int i;

    if (t == NULL || t->typ == TYPEVAR || t->recursive != 0)
        return t;
 
    if (t->typ == FN || t->typ == LAMBDA)
    {
        int count = t->arity;
        type_t ** param = (type_t **) GC_MALLOC(count*sizeof(type_t *));
        for (i = 0; i < count; i++)
            param[i] = make_recursive(t->param[i], rec);
        type_t * fn = fn_type(make_recursive(t->ret, rec), count, param);
        if (t->typ == LAMBDA)
            fn->typ = LAMBDA;
        return fn;
    } else if (t->typ == TUPLE || t->typ == DATATYPE)
        t->recursive = is_recursive(t, rec);
        
    return t;
}

/* 
   search for recursion: find occurrences of var (which should be a typevar) 
   within t and replace with rec and set rec to recursive
*/
void subst_recursive(type_t * t, type_t * var, type_t * rec)
{
    int i;
    
    if (t == NULL || t->typ == TYPEVAR || t->recursive != 0)
        return;
    
    if (t->typ == FN || t->typ == LAMBDA)
    {
        for (i = 0; i < t->arity; i++)
        {
            if (t->param[i] == var)
                t->param[i] = make_recursive(rec, var);
            else
                subst_recursive(t->param[i], var, rec);
        }
        if (t->ret != NULL)
        {
            if (t->ret == var)
                t->ret = make_recursive(rec, var);
            else
                subst_recursive(t->ret, var, rec);
        }
    } else if (t->typ == TUPLE || t->typ == DATATYPE)
    {
        for (i = 0; i < t->arity; i++)
        {
            if (t->param[i] == var)
                t->param[i] = make_recursive(rec, var);
            else
                subst_recursive(t->param[i], var, rec);
        }
    } else if (t->typ == ARRAY)
    {
        if (t->ret == var)
            t->ret = make_recursive(rec, var);
        else
            subst_recursive(t->ret, var, rec);
    }
}

void check_recursion(type_rel_t * rel)
{
    /* if we have something like T1 = datatype(.... T1 ....) make recursive */
    if (rel->t1->typ == TYPEVAR && rel->t2->recursive == 0)
        subst_recursive(rel->t2, rel->t1, rel->t2);
}

void type_subst_type(type_t ** tin, type_rel_t * rel)
{
    type_t * t = *tin;
    int i;

    if (t == NULL)
        return;

    if (t->recursive == 2)
    {
        t->recursive = 1;
        return;
    }
    if (t->recursive == 1)
        t->recursive = 2;
    
    if (t->typ == TYPEVAR)
    {
        if (*tin == rel->t1)
            *tin = rel->t2;
    }
    else if (t->typ == FN || t->typ == LAMBDA)
    {
        for (i = 0; i < t->arity; i++)
            type_subst_type(t->param + i, rel);
        if (t->ret != NULL)
            type_subst_type(&(t->ret), rel);
    } else if (t->typ == TUPLE || t->typ == DATATYPE)
    {
        for (i = 0; i < t->arity; i++)
            type_subst_type(t->param + i, rel);
    } else if (t->typ == ARRAY)
    {
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
        check_recursion(rels);
        type_subst_type(&(rels->t2), rel);
         
        rels = rels->next;
    }
}

void unify(type_rel_t * rels, type_rel_t * ass)
{
    int i, j;
    
    while (rel_stack != NULL)
    {
        type_rel_t * rel = pop_type_rel();
        check_recursion(rel);
   
        if (rel->t1 == rel->t2)
            continue;

        if (rel->t1->recursive && rel->t2->recursive)
        {
            if (rel->t2->recursive == 2)
            {
                rel->t2->recursive = 1;
                continue;
            } 
            if (rel->t2->recursive == 1)
                rel->t2->recursive = 2;
        }
            
        if (rel->t1->typ == TYPEVAR)
        {
            rels_subst(rel_stack, rel);
            ass_subst(rel_assign, rel);
            push_type_ass(rel);
        } else if (rel->t2->typ == TYPEVAR)
            push_type_rel(&rel->t2, &rel->t1);
        else if (rel->t1->typ == FN || rel->t1->typ == LAMBDA)
        {
            if ((rel->t2->typ != FN && rel->t2->typ != LAMBDA) || rel->t1->arity != rel->t2->arity)
                exception("Type mismatch: function type not matched!\n");
            push_type_rel(&rel->t1->ret, &rel->t2->ret);
            for (i = 0; i < rel->t1->arity; i++)
                push_type_rel(&rel->t1->param[i], &rel->t2->param[i]);
        }
        else if (rel->t1->typ == TUPLE)
        {
            if (rel->t2->typ != TUPLE || rel->t1->arity != rel->t2->arity)
                exception("Type mismatch: tuple type not matched!\n");
            for (i = 0; i < rel->t1->arity; i++)
                push_type_rel(&rel->t1->param[i], &rel->t2->param[i]);
        }
        else if (rel->t1->typ == ARRAY)
        {
            if (rel->t2->typ != ARRAY)
                exception("Type mismatch: array type not matched!\n");
            push_type_rel(&rel->t1->ret, &rel->t2->ret);
        }
        else if (rel->t1->typ == DATATYPE)
        {
            if (rel->t2->typ != DATATYPE)
                exception("Type mismatch: data type not matched!\n");
            if (rel->t1->sym != NULL && rel->t2->sym != NULL) /* comparing two full datatypes */
            {
                if (rel->t1->arity != rel->t2->arity || rel->t1->sym != rel->t2->sym)
                    exception("Type mismatch: data type not matched!\n");
                for (i = 0; i < rel->t1->arity; i++)
                    push_type_rel(&rel->t1->param[i], &rel->t2->param[i]);
            } else /* one of the data types is not full */
            {
                if (rel->t1->sym == NULL) /* switch order */
                    push_type_rel(&rel->t2, &rel->t1);
                else /* compare partial datatype with full */
                {
                    for (j = 0; j < rel->t2->arity; j++)
                    {
                        for (i = 0; i < rel->t1->arity; i++)
                        {
                            if (rel->t1->slot[i] == rel->t2->slot[j])
                                break;
                        }
                        if (i == rel->t1->arity) /* slot was not found */
                            exception("Nonexistent slot!\n");
                        /* check type of slot is right */
                        push_type_rel(&rel->t1->param[i], &rel->t2->param[j]);
                    }

                    /* update partial type */
                    type_t ** pp = (type_t **) GC_MALLOC(rel->t1->arity*sizeof(type_t *));
                    sym_t ** ss = (sym_t **) GC_MALLOC(rel->t1->arity*sizeof(sym_t *));
                    for (j = 0; j < rel->t1->arity; j++)
                    {
                        pp[j] = rel->t1->param[j]; 
                        ss[j] = rel->t1->slot[j]; 
                    }
                    
                    /* restore partial types for slots */
                    for (j = 0; j < rel->t1->arity; j++)
                    {
                       for (i = 0; i < rel->t2->arity; i++)
                       {
                           if (rel->t1->slot[j] == rel->t2->slot[i])
                           {
                               pp[j] = rel->t2->param[i]; 
                               break;
                           }
                       }
                    }
                    rel->t2->param = pp;
                    rel->t2->slot = ss;

                    rel->t2->sym = rel->t1->sym;
                    rel->t2->arity = rel->t1->arity;
                }
            }
        }
        else if (rel->t1->typ != rel->t2->typ)
            exception("Type mismatch!\n");
    }
}

/* bind a new identifier in the symbol table */
bind_t * bind_id(ast_t * id)
{
    id->type = new_typevar(); /* type is to be inferred */            
    
    /* check we're not redefining a local symbol in the current scope */
    bind_t * bind = find_symbol_in_scope(id->sym);
    if (bind != NULL && !scope_is_global(bind))
        exception("Attempt to redefine local symbol\n");
                          
    /* make new binding in the current scope*/
    return bind_symbol(id->sym, id->type, NULL);
}

bind_t * bind_tuple(ast_t * id)
{
    ast_t * p = id->child;
    bind_t * bind;
    int count = ast_list_length(p);
    int i;

    type_t ** param = (type_t **) GC_MALLOC(count*sizeof(type_t *));
    for (i = 0; i < count; i++)
    {
        if (p->tag == AST_LTUPLE) /* we have a tuple */
            bind = bind_tuple(p);
        else /* we have an identifier */
        {
            bind = bind_id(p); 
            bind->initialised = 1; /* tuples vars are always initialised */
        }
        
        param[i] = p->type; /* get type */

        p = p->next;
    }

    id->type = tuple_type(count, param);

    return bind;
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
    type_t * retty, * ty;
    type_t ** param;
    bind_t * bind;
    sym_t * sym;
    sym_t ** slot;
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
        {
           printf("%s ", a->sym->name);
           exception("Unbound symbol\n");
        }
        break;
    case AST_LVALUE:
        b = find_symbol(a->sym);
        if (b != NULL) /* ensure it exists */
        {
            a->type = b->type;
            a->bind = b; /* make sure we use the right binding */
            b->initialised = 1; 
        } else
        {
           printf("%s ", a->sym->name);
           exception("Unbound symbol\n");
        }
        break;
    case AST_LTUPLE:
        p = a->child;
        count = ast_list_length(p); /* count parameters */
        param = (type_t **) GC_MALLOC(count*sizeof(type_t *));
        for (i = 0; i < count; i++)
        {
           annotate_ast(p);
           param[i] = p->type;
           p = p->next;
        }
        a->type = tuple_type(count, param);
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
        push_type_rel(&a->type, &a->child->next->type);
        break;
    case AST_ASSIGNMENT:
        id = a->child;
        expr = id->next;
        annotate_ast(expr);
        annotate_ast(id);
        if (id->type->typ == FN)
            exception("Attempt to assign to function\n");
        a->type = expr->type;
        push_type_rel(&id->type, &expr->type);
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
        push_type_rel(&a->child->type, &a->child->next->type);
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
            } else /* AST_IDENT or AST_TUPLE*/
                id = p;
                        
            if (p->tag == AST_ASSIGNMENT)
               annotate_ast(expr); /* get expr before anything is redefined */

            if (id->tag == AST_LTUPLE)
                bind = bind_tuple(id);
            else
                bind = bind_id(id);
                       
            /* check if it is a combined variable declaration and assignment */
            if (p->tag == AST_ASSIGNMENT)
            {
                bind->initialised = 1;
                annotate_ast(id);
                p->type = expr->type;
                push_type_rel(&id->type, &expr->type);
            } else
            {
                bind->initialised = 0;
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
        push_type_rel(&a->child->type, &t_bool);
        break;
    case AST_IFELSE:
        annotate_ast(a->child);
        annotate_ast(a->child->next);
        annotate_ast(a->child->next->next);
        a->type = t_nil;
        push_type_rel(&a->child->type, &t_bool);
        break;
    case AST_IFEXPR:
        annotate_ast(a->child);
        annotate_ast(a->child->next);
        annotate_ast(a->child->next->next);
        a->type = a->child->next->type;
        push_type_rel(&a->child->type, &t_bool);
        push_type_rel(&a->child->next->type, &a->child->next->next->type);
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
            push_type_rel(&b->type, &t_nil);
        else
        {
            annotate_ast(a->child);
            if (a->child->type->typ == FN) /* need to make return type a lambda */
            {
                type_t * temp = fn_to_lambda_type(a->child->type);
                push_type_rel(&b->type, &temp);
            } else
                push_type_rel(&b->type, &a->child->type);
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
    case AST_LAMBDA:
        p = a->child->child; 
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
        a->type = fn_type(retty, count, param);
        a->type->typ = LAMBDA;
        sym = sym_lookup("lambda");
        b = bind_lambda(sym, a->type, a);
        b->initialised = 1;

        current_scope = a->env;
        
        /* process function body */
        t = a->child->next;
        if (t->tag == AST_EXPRBLOCK)
        {
            expr = t->child;
            while (expr->next != NULL)
            {
                annotate_ast(expr);
                expr = expr->next;
            }
            annotate_ast(expr);
        } else
        {
            expr = t;
            annotate_ast(expr);
        }
        t->type = expr->type;
        push_type_rel(&bind->type, &t->type);
        
        scope_down();
        break;
    case AST_APPL:
        id = a->child;

        /* find function and get particulars */
        if (id->tag == AST_IDENT)
        {
            bind = find_symbol(id->sym);
            if (bind != NULL)
            {
                id->type = bind->type;
                id->bind = bind;
            } else
                exception("Unknown function or datatype\n");
        } else /* we may not have an identifier giving the function */
            annotate_ast(id);
        
        /* count arguments */
        p = id->next;
        count = ast_list_length(p);
        
        /* the function may be passed via a parameter whose type is a typevar */
        if (id->type->typ != FN && id->type->typ != LAMBDA && id->type->typ != DATATYPE) 
        {
            /* build appropriate function type */
            param = (type_t **) GC_MALLOC(count*sizeof(type_t *));
            for (i = 0; i < count; i++)
                param[i] = new_typevar();
            type_t * fn_ty = fn_type(new_typevar(), count, param);
            fn_ty->typ = LAMBDA;
            
            /* use it instead and infer the typevar */
            push_type_rel(&id->type, &fn_ty);
            id->type = fn_ty;
            
            /* update binding */
            if (id->sym != NULL)
            {
                bind = find_symbol(id->sym);
                bind->type = id->type;
                id->bind = bind;
            }
        } else if (count != id->type->arity) /* check number of parameters */
        {
            if (id->type->typ == FN || id->type->typ == LAMBDA)
                exception("Wrong number of parameters in function\n");
            else /* DATATYPE */
                exception("Wrong number of parameters in type constructor\n");
        }
        
        /* get parameter types */
        for (i = 0; i < count; i++)
        {
            annotate_ast(p);
            if (p->type->typ == FN) /* need to make type a lambda */
            {
                type_t * temp = fn_to_lambda_type(p->type);
                push_type_rel(&id->type->param[i], &temp);
            } else
                push_type_rel(&id->type->param[i], &p->type);
            p = p->next;
        }

        /* get return types */
        if (id->type->typ == DATATYPE)
            a->type = id->type;
        else /* FN or LAMBDA */
            a->type = id->type->ret;
        break;
    case AST_TUPLE:
        /* count parameters */
        p = a->child;
        count = ast_list_length(p);
        
        /* build appropriate tuple type */
        param = (type_t **) GC_MALLOC(count*sizeof(type_t *));
        
        /* get parameter types */
        for (i = 0; i < count; i++)
        {
            annotate_ast(p);
            if (p->type->typ == FN) /* need to make type a lambda */
                param[i] = fn_to_lambda_type(p->type);
            else
                param[i] = p->type;
            p = p->next;
        }
            
        a->type = tuple_type(count, param);
        break;
    case AST_DATATYPE:
        /* count parameters */
        id = a->child;
        p = id->next;
        count = ast_list_length(p);
        
        current_scope = a->env;
        
        /* build appropriate data type */
        param = (type_t **) GC_MALLOC(count*sizeof(type_t *));
        slot = (sym_t **) GC_MALLOC(count*sizeof(sym_t *));
        
        /* get parameter types */
        for (i = 0; i < count; i++)
        {
            p->type = new_typevar();
            param[i] = p->type;
            slot[i] = p->sym;
            p = p->next;
        }
            
        /* make binding */
        id->type = data_type(count, param, id->sym, slot);
        bind = bind_datatype(id->sym, id->type, a);
        bind->typeval = NULL;

        a->type = id->type;
        break;
    case AST_SLOT:
        id = a->child;
        p = id->next;
        if (a->type == NULL) /* we haven't got a type so add typevar */
            a->type = new_typevar();

        /* make data type */
        param = (type_t **) GC_MALLOC(sizeof(type_t *));
        slot = (sym_t **) GC_MALLOC(sizeof(sym_t *));
        param[0] = a->type;
        slot[0] = p->sym;
        ty = data_type(1, param, NULL, slot);
        id->type = ty;

        /* recurse if we have a slot of a slot */
        if (id->tag == AST_IDENT) 
        {
            b = find_symbol(id->sym);
            if (b != NULL && b->initialised)
                push_type_rel(&b->type, &id->type);
        }
        annotate_ast(id);

        /* join types up */
        if (ty != id->type)
            push_type_rel(&ty, &id->type);
        break;
    case AST_LOCATION:
        id = a->child;
        p = id->next;
        if (a->type == NULL) /* we haven't got a type so add typevar */
            a->type = new_typevar();

        /* make data type */
        ty = array_type(a->type);
        id->type = ty;

        /* recurse if we have a location of a location */
        if (id->tag == AST_IDENT) 
        {
            b = find_symbol(id->sym);
            if (b != NULL && b->initialised)
                push_type_rel(&b->type, &id->type);
        }
        annotate_ast(id);

        /* join types up */
        if (ty != id->type)
            push_type_rel(&ty, &id->type);
        
        /* ensure index is an integer */
        annotate_ast(p);
        push_type_rel(&p->type, &t_int);
        break;
    case AST_ARRAY:
        p = a->child;
        annotate_ast(p);
        /* the parameter is an integer */
        push_type_rel(&p->type, &t_int);
        a->type = array_type(new_typevar());
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
