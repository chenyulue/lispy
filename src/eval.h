#ifndef _LISPY_EVAL
#define _LISPY_EVAL

#include <string.h>
#include "mpc.h"

#define STR_EQ(X, Y) (strcmp((X), (Y)) == 0)
#define STR_CONTAIN(X, Y) strstr((X), (Y))

long eval(mpc_ast_t *t);
long eval_op(long x, char *op, long y);

#endif