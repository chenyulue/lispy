#ifndef _LISPY_EVAL
#define _LISPY_EVAL

#include <string.h>
#include "mpc.h"

#define STR_EQ(X, Y) (strcmp((X), (Y)) == 0)
#define STR_CONTAIN(X, Y) strstr((X), (Y))

/* Crate Enumeration of possible lval types. */
enum lval_type {
    LVAL_NUM,
    LVAL_ERR,
};

/* Create enumeration of possible error types. */
enum lval_err {
    LERR_DIV_ZERO,
    LERR_BAD_OP,
    LERR_BAD_NUM,
};

/* Declare new lval struct, which uses a nested union in the struct to 
    represent the evaluated result.
*/
typedef struct {
    enum lval_type type;
    union {
        long num;
        enum lval_err err;
    };
} lval;

lval eval(mpc_ast_t *t);
lval eval_op(lval x, char *op, lval y);

/* Create a new number type lval */
lval lval_num(long x);
/* Create a new error type lval */
lval lval_err(enum lval_err err);

/* Print an lval value. */
void lval_print(lval v);
/* Print an lval value followed by a newline. */
void lval_println(lval v);

#endif