#ifndef UNIFY_H
#define UNIFY_H

#include "types.h"
#include "ast.h"

#ifdef __cplusplus
 extern "C" {
#endif

typedef struct type_rel_t
{
   type_t * t1;
   type_t * t2;
   struct type_rel_t * next;
} type_rel_t;

extern type_rel_t * rel_stack;
extern type_rel_t * rel_assign;

void rel_stack_init(void);

void rel_assign_init(void);

void rel_assign_mark(void);

void rel_assign_rewind(void);

void rels_subst(type_rel_t * rels, type_rel_t * rel);

void ass_subst(type_rel_t * rels, type_rel_t * rel);

void push_type_rel(type_t * t1, type_t * t2);

void push_type_ass(type_rel_t * rel);

type_rel_t * pop_type_rel(void);

void subst_type(type_t ** type);

void check_recursion(type_rel_t * rel);

void unify(type_rel_t * rels, type_rel_t * ass);

void annotate_ast(ast_t * a);

void print_assigns(type_rel_t * ass);

#ifdef __cplusplus
}
#endif

#endif
