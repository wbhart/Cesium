#include <stdio.h>
#include "types.h"
#include "gc.h"

type_t * t_nil;
type_t * t_int;
type_t * t_bool;
type_t * t_double;
type_t * t_string;
type_t * t_char;

type_t * new_type(typ_t typ)
{
   type_t * t = GC_MALLOC(sizeof(type_t));
   t->typ = typ;
   t->arity = 0;
   return t;
}

void type_init(void)
{
   t_nil = new_type(NIL);
   t_int = new_type(INT);
   t_bool = new_type(BOOL);
   t_double = new_type(DOUBLE);
   t_string = new_type(STRING);
   t_char = new_type(CHAR);
}

int type_equal(type_t * t1, type_t * t2)
{
   if (t1 == t2)
      return 1;
   return 0;
}

type_t * typ_to_type(typ_t typ)
{
   switch (typ)
   {
   case NIL:
      return t_nil;
      break;
   case BOOL:
      return t_bool;
      break;
   case INT:
      return t_int;
      break;
   case DOUBLE:
      return t_double;
      break;
   case STRING:
      return t_string;
      break;
   case CHAR:
      return t_char;
      break;
   default:
      return NULL;
   }
}

type_t * fn_type(type_t * ret, int num, type_t ** param, int variadic)
{
   type_t * t = GC_MALLOC(sizeof(type_t));
   t->typ = FN;
   t->param = GC_MALLOC(sizeof(type_t *)*num);
   t->ret = ret;
   t->arity = num;

   int i;
   for (i = 0; i < num; i++)
      t->param[i] = param[i];

   return t;
}

type_t * new_typevar(void)
{
    static long typevarnum = 0;
    type_t * t = new_type(TYPEVAR);
    t->arity = typevarnum++;
    return t;
}

void print_type(type_t * t)
{
    if (t == NULL)
    {
        printf("NULL");
        return;
    }
    
    switch (t->typ)
    {
    case NIL:
        printf("nil");
        break;
    case BOOL:
        printf("bool");
        break;
    case INT:
        printf("int");
        break;
    case DOUBLE:
        printf("double");
        break;
    case STRING:
        printf("string");
        break;
    case CHAR:
        printf("char");
        break;
    case FN:
        printf("fn");
        break;
    case TYPEVAR:
        printf("T%ld", t->arity);
        break;
    } 
}
