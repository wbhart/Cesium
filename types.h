#ifndef TYPES_H
#define TYPES_H

#include "symbol.h"

#ifdef __cplusplus
 extern "C" {
#endif

typedef enum
{
   NIL, UNKNOWN, BOOL, INT, DOUBLE, STRING, CHAR, 
   FN, LAMBDA, ARRAY, TUPLE, DATATYPE, TYPEVAR
} typ_t;

typedef struct type_t
{
   typ_t typ;
   int arity;
   struct type_t ** param;
   struct type_t * ret;
   struct sym_t ** slot;
   struct sym_t * sym;
} type_t;

extern type_t * t_nil;
extern type_t * t_int;
extern type_t * t_bool;
extern type_t * t_double;
extern type_t * t_string;
extern type_t * t_char;

void type_init(void);

type_t * new_type(typ_t typ);

int type_equal(type_t * t1, type_t * t2);

type_t * fn_type(type_t * ret, int num, type_t ** param);

type_t * tuple_type(int num, type_t ** param);

type_t * array_type(type_t * param);

type_t * data_type(int num, type_t ** param, sym_t * sym, sym_t ** slots);

type_t * fn_to_lambda_type(type_t * type);

type_t * new_typevar(void);

void print_type(type_t * t);

#ifdef __cplusplus
}
#endif

#endif
