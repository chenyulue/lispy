#ifndef _LISPY_EVAL
#define _LISPY_EVAL

#include <string.h>
#include "mpc.h"

#define STR_EQ(X, Y) (strcmp((X), (Y)) == 0)
#define STR_CONTAIN(X, Y) strstr((X), (Y))
#define LASSERT(args, cond, err)  \
    do                            \
    {                             \
        if (!(cond))              \
        {                         \
            lval_del(args);       \
            return lval_err(err); \
        }                         \
    } while (0)

/* Crate Enumeration of possible lval types. */
enum lval_type
{
    LVAL_ERR,
    LVAL_NUM,
    LVAL_SYM,
    LVAL_SEXPR,
    LVAL_QEXPR,
};

/* Declare new lval struct, which uses a nested union in the struct to
    represent the evaluated result.
*/
typedef struct lval
{
    enum lval_type type;
    union
    {
        long num;

        /* Use string characters to store the error info and symbols. */
        char *err;
        char *sym;

        /* Count and pointer to a list of lval*/
        struct
        {
            int count;
            struct lval **cell;
        };
    };
} lval;

/************ Evaluate the AST ******************/
lval *lval_eval_sexpr(lval *v);
lval *lval_eval(lval *v);
lval *lval_pop(lval *v, int i);
lval *lval_take(lval *v, int i);
lval *builtin_op(lval *a, char *op);
lval *builtin_head(lval *a);
lval *builtin_tail(lval *a);
lval *builtin_list(lval *a);
lval *builtin_eval(lval *a);
lval *builtin_join(lval *a);
lval *lval_join(lval *x, lval *y);
lval *builtin(lval *a, char *func);

/**************** Construct new lval *********************/
/* Create a pointer to a new Number lval */
lval *lval_num(long x);
/* Create a pointer to a new Error lval  */
lval *lval_err(char *err);
/* Create a pointer to a new Symbol lval*/
lval *lval_sym(char *sym);
/* Create a pointer to a new S-expression lval */
lval *lval_sexpr(void);
/* Crate a pointer to a new Q-expression lval */
lval *lval_qexpr(void);
/* Delete a lval to free the memory. */
void lval_del(lval *v);

/* Read the input and contruct the lval. */
lval *lval_read_num(mpc_ast_t *t);
lval *lval_read(mpc_ast_t *t);
lval *lval_add(lval *v, lval *x);

/* Print the expression*/
void lval_expr_print(lval *v, char open, char close);
/* Print an lval value. */
void lval_print(lval *v);
/* Print an lval value followed by a newline. */
void lval_println(lval *v);

#endif