#ifndef _LISPY_EVAL
#define _LISPY_EVAL

#include <string.h>
#include "mpc.h"

#define STR_EQ(X, Y) (strcmp((X), (Y)) == 0)
#define STR_CONTAIN(X, Y) strstr((X), (Y))
#define LASSERT(args, cond, fmt, ...)                 \
    do                                                \
    {                                                 \
        if (!(cond))                                  \
        {                                             \
            lval *err = lval_err(fmt, ##__VA_ARGS__); \
            lval_del(args);                           \
            return err;                               \
        }                                             \
    } while (0)

/* Forward Declarations */
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

/* Crate Enumeration of possible lval types. */
enum lval_type
{
    LVAL_ERR,
    LVAL_NUM,
    LVAL_SYM,
    LVAL_FUN,
    LVAL_SEXPR,
    LVAL_QEXPR,
};

typedef lval *(*lbuiltin)(lenv *, lval *);

/* Declare new lval struct, which uses a nested union in the struct to
    represent the evaluated result.
*/
struct lval
{
    enum lval_type type;
    union
    {
        /* Basic types. */
        long num;

        /* Use string characters to store the error info and symbols. */
        char *err;
        char *sym;

        /* Functions */
        struct 
        {
            lbuiltin builtin;
            lenv *env;
            lval *formals;
            lval *body;
        };
        

        /* Count and pointer to a list of lval*/
        struct
        {
            int count;
            struct lval **cell;
        };
    };
};

/* We change the `lenv` struct slightly, using a sym-to-lval dictionary to record
    the environment.
*/
struct env_map
{
    char *sym;
    lval *val;
};
struct lenv
{
    lenv *par;
    int count;
    struct env_map *dicts;
};

/************* Functions to manipulate the environment. ****************/
lenv *lenv_new(void);
void lenv_del(lenv *e);
lval *lenv_get(lenv *e, lval *k);
void lenv_put(lenv *e, lval *k, lval *v);
lenv *lenv_copy(lenv *e);
void lenv_def(lenv *e, lval *k, lval *v);
char *lenv_find_fun(lenv *e, lbuiltin fun);
/* Print out all the named values in an environment. */
void builtin_print_env(lenv *e);

/************ Evaluate the AST ******************/
lval *lval_eval_sexpr(lenv *e, lval *v);
lval *lval_eval(lenv *e, lval *v);
lval *lval_pop(lval *v, int i);
lval *lval_take(lval *v, int i);
lval *builtin_op(lenv *e, lval *a, char *op);
lval *builtin_head(lenv *e, lval *a);
lval *builtin_tail(lenv *e, lval *a);
lval *builtin_list(lenv *e, lval *a);
lval *builtin_eval(lenv *e, lval *a);
lval *builtin_join(lenv *e, lval *a);
lval *lval_join(lval *x, lval *y);
// lval *builtin(lenv *e, lval *a, char *func);
lval *builtin_add(lenv *e, lval *a);
lval *builtin_sub(lenv *e, lval *a);
lval *builtin_mul(lenv *e, lval *a);
lval *builtin_div(lenv *e, lval *a);
lval *builtin_def(lenv *e, lval *a);
lval *builtin_put(lenv *e, lval *a);
lval *builtin_var(lenv *e, lval *a, char *func);
lval *builtin_lambda(lenv *e, lval *a);
lval *builtin_fun(lenv *e, lval *a);
void lenv_add_builtin(lenv *e, char *name, lbuiltin fun);
void lenv_add_builtins(lenv *e);

/**************** Construct new lval *********************/
/* Create a pointer to a new Number lval */
lval *lval_num(long x);
/* Create a pointer to a new Error lval  */
lval *lval_err(char *fmt, ...);
/* Create a pointer to a new Symbol lval*/
lval *lval_sym(char *sym);
/* Create a pointer to a new S-expression lval */
lval *lval_sexpr(void);
/* Create a pointer to a new Q-expression lval */
lval *lval_qexpr(void);
/* Create a pointer to a list function */
lval *lval_fun(lbuiltin func);
/* Create a user defined function. */
lval *lval_lambda(lval *formals, lval *body);
/* Call a function */
lval *lval_call(lenv *e, lval *f, lval *a);
/* Delete a lval to free the memory. */
void lval_del(lval *v);

/* Read the input and contruct the lval. */
lval *lval_read_num(mpc_ast_t *t);
lval *lval_read(mpc_ast_t *t);
lval *lval_add(lval *v, lval *x);
lval *lval_copy(lval *v);

/* Print the expression*/
void lval_expr_print(lenv *e, lval *v, char open, char close);
/* Print an lval value. */
void lval_print(lenv *e, lval *v);
/* Print an lval value followed by a newline. */
void lval_println(lenv *e, lval *v);

/*********** Utilities *******************/
char *ltype_name(enum lval_type t);

void lispy_exit(int *flag);

#endif