#ifndef TYPES_H
#define TYPES_H

#ifdef __cplusplus
 extern "C" {
#endif

typedef enum
{
   NIL, BOOL, INT, DOUBLE, STRING, CHAR, FN, ARRAY, TYPEVAR 
} typ_t;

typedef struct type_t
{
   typ_t typ;
   int arity;
   struct type_t ** param;
   struct type_t * ret;
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

type_t * typ_to_type(typ_t);

type_t * fn_type(type_t * ret, int num, type_t ** param);

type_t * new_typevar(void);

void print_type(type_t * t);

#ifdef __cplusplus
}
#endif

#endif
