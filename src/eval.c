#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpc.h"
#include "eval.h"

/************* Functions to manipulate the environment. ****************/
lenv *lenv_new(void)
{
    lenv *e = malloc(sizeof(lenv));
    e->count = 0;
    e->dicts = NULL;
    return e;
}

void lenv_del(lenv *e)
{
    for (int i = 0; i < e->count; i++)
    {
        free(e->dicts[i].sym);
        lval_del(e->dicts[i].val);
    }
    free(e->dicts);
    free(e);
}

lval *lenv_get(lenv *e, lval *k)
{
    /* Iterate over all items in environment */
    for (int i = 0; i < e->count; i++)
    {
        /* Check if the stored string mathces the symbol string.
            If it does, return a copy of the value.
        */
        if (STR_EQ(e->dicts[i].sym, k->sym))
        {
            return lval_copy(e->dicts[i].val);
        }
    }
    /* if no symbol found return error. */
    return lval_err("Unbound symbol '%s'", k->sym);
}

void lenv_put(lenv *e, lval *k, lval *v)
{
    /* Iterate over all items in environment */
    /* This is to see if variable already exists. */
    for (int i = 0; i < e->count; i++)
    {
        /* If variable is found delete the old item at that position.
            And replace with variable supplied by user.
        */
        if (STR_EQ(e->dicts[i].sym, k->sym))
        {
            lval_del(e->dicts[i].val);
            e->dicts[i].val = lval_copy(v);
            return;
        }
    }

    /* If no existing entry found, allocate space for new entry. */
    e->count++;
    e->dicts = realloc(e->dicts, e->count * sizeof(struct env_map));

    /* Copy contents of lval and symbol string into new location. */
    e->dicts[e->count - 1].val = lval_copy(v);
    e->dicts[e->count - 1].sym = malloc(strlen(k->sym) + 1);
    strcpy(e->dicts[e->count - 1].sym, k->sym);
}

char *lenv_find_fun(lenv *e, lbuiltin fun)
{
    for (int i = 0; i < e->count; i++)
    {
        if (e->dicts[i].val->type != LVAL_FUN)
            continue;
        if (e->dicts[i].val->fun == fun)
            return e->dicts[i].sym;
    }
    return "";
}

/* Print out all the named values in an environment. */
void builtin_print_env(lenv *e)
{
    puts("\n******* All the Variables in the Environment *******");
    for (int i = 0; i < e->count; i++)
    {   
        printf("%s = ", e->dicts[i].sym);
        lval_println(e, e->dicts[i].val);
    }
    puts("");
}

/************************ Evaluate the AST ********************/
lval *lval_eval_sexpr(lenv *e, lval *v)
{
    /* Evaluate the children. */
    for (int i = 0; i < v->count; i++)
    {
        v->cell[i] = lval_eval(e, v->cell[i]);

        /* If Error happens, return this error. */
        if (v->cell[i]->type == LVAL_ERR)
            return lval_take(v, i);
    }

    /* Empty Expreesion */
    if (v->count == 0)
        return v;

    /* Single Expression */
    if (v->count == 1)
        return lval_take(v, 0);

    /* Ensure First Element is Symbol */
    lval *f = lval_pop(v, 0);
    if (f->type != LVAL_FUN)
    {
        lval_del(f);
        lval_del(v);
        return lval_err("First element got %s, expected %s.",
                        ltype_name(f->type), ltype_name(LVAL_FUN));
    }

    /* If so call function to get result. */
    lval *result = f->fun(e, v);
    lval_del(f);
    return result;
}

lval *lval_eval(lenv *e, lval *v)
{
    if (v->type == LVAL_SYM)
    {
        lval *x = lenv_get(e, v);
        lval_del(v);
        return x;
    }

    /* Evaluation Sexpressions. */
    if (v->type == LVAL_SEXPR)
    {
        return lval_eval_sexpr(e, v);
    }

    /* All other lval types remain the same. */
    return v;
}

lval *lval_pop(lval *v, int i)
{
    /* Find the item at "i" */
    lval *x = v->cell[i];

    /* Shift memory after the item at "i" over the top. */
    memmove(&v->cell[i], &v->cell[i + 1], sizeof(lval *) * (v->count - i - 1));

    /* Decrease the count of items in the lsit. */
    v->count--;

    /** Reallocate the memory used. */
    v->cell = realloc(v->cell, sizeof(lval *) * v->count);

    return x;
}

lval *lval_take(lval *v, int i)
{
    lval *x = lval_pop(v, i);
    lval_del(v);
    return x;
}

lval *builtin_op(lenv *e, lval *a, char *op)
{
    /* Ensure all arguments are numbers */
    for (int i = 0; i < a->count; i++)
    {
        a->cell[i] = lval_eval(e, a->cell[i]);
        if (a->cell[i]->type != LVAL_NUM)
        {
            lval_del(a);
            return lval_err("Operator '%s' expects %s as arguments at pos %d, but got %s.",
                            op, ltype_name(LVAL_NUM), i, ltype_name(a->cell[i]->type));
        }
    }

    /* Pop the first element */
    lval *x = lval_pop(a, 0);

    /* If no arguments and sub then perform unary negation. */
    if (STR_EQ(op, "-") && a->count == 0)
    {
        x->num = -x->num;
    }

    /* While there are still elements remaining */
    while (a->count > 0)
    {
        /* Pop the next element */
        lval *y = lval_pop(a, 0);

        if (STR_EQ(op, "+"))
            x->num += y->num;
        if (STR_EQ(op, "-"))
            x->num -= y->num;
        if (STR_EQ(op, "*"))
            x->num *= y->num;
        if (STR_EQ(op, "/"))
        {
            if (y->num == 0)
            {
                lval_del(x);
                lval_del(y);
                x = lval_err("Division by Zero.");
                break;
            }
            x->num /= y->num;
        }

        lval_del(y);
    }
    lval_del(a);
    return x;
}

lval *builtin_head(lenv *e, lval *a)
{
    LASSERT(a, a->count == 1,
            "Function 'head' passed too many arguments. "
            "Got %d, Expected %d.",
            a->count, 1);
    a->cell[0] = lval_eval(e, a->cell[0]);
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'head' passed incorect type for argument 0. "
            "Got %s, Expected %s.",
            ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));
    LASSERT(a, a->cell[0]->count, "Function 'head' got {}, expected %s.",
            ltype_name(LVAL_QEXPR));

    lval *v = lval_take(a, 0);
    while (v->count > 1)
    {
        lval_del(lval_pop(v, 1));
    }
    return v;
}
lval *builtin_tail(lenv *e, lval *a)
{
    LASSERT(a, a->count == 1,
            "Function 'tail' passed too many arguments. "
            "Got %d, Expected %d",
            a->count, 1);
    a->cell[0] = lval_eval(e, a->cell[0]);
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'tail' passed incorrect type for argument 0. "
            "Got %s, Expected %s",
            ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));
    LASSERT(a, a->cell[0]->count, "Function 'tail' passed {}.");

    lval *v = lval_take(a, 0);
    lval_del(lval_pop(v, 0));
    return v;
}

lval *builtin_list(lenv *e, lval *a)
{
    // lval *x = lval_eval(e, a);
    a->type = LVAL_QEXPR;
    return a;
}

lval *builtin_eval(lenv *e, lval *a)
{
    LASSERT(a, a->count == 1,
            "Function 'eval' passed too many arguments. "
            "Got %d, Expected %d",
            a->count, 1);
    a->cell[0] = lval_eval(e, a->cell[0]);
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'eval' passed incorrect type for argument 0. "
            "Got %s, Expected %s",
            ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));

    lval *x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

lval *builtin_join(lenv *e, lval *a)
{
    for (int i = 0; i < a->count; i++)
    {
        a->cell[i] = lval_eval(e, a->cell[i]);
        LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
                "Function 'join' passed incorrect type for argument %d. "
                "Got %s, Expected %s",
                i, ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));
    }

    lval *x = lval_pop(a, 0);
    while (a->count)
    {
        x = lval_join(x, lval_pop(a, 0));
    }
    lval_del(a);
    return x;
}

lval *lval_join(lval *x, lval *y)
{
    /* For each cell in 'y' add it to 'x' */
    while (y->count)
    {
        x = lval_add(x, lval_pop(y, 0));
    }

    /* Delete the empty 'y' and return 'x' */
    lval_del(y);
    return x;
}

lval *builtin_add(lenv *e, lval *a)
{
    return builtin_op(e, a, "+");
}

lval *builtin_sub(lenv *e, lval *a)
{
    return builtin_op(e, a, "-");
}

lval *builtin_mul(lenv *e, lval *a)
{
    return builtin_op(e, a, "*");
}

lval *builtin_div(lenv *e, lval *a)
{
    return builtin_op(e, a, "/");
}

lval *builtin_def(lenv *e, lval *a)
{
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'def' passed incorrect type for variable list. "
            "Got %s, Expected %s.",
            ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));

    /* First argument is symbol list */
    lval *syms = a->cell[0];

    /* Ensure all elements of first list are symbols. */
    for (int i = 0; i < syms->count; i++)
    {
        LASSERT(a, syms->cell[i]->type == LVAL_SYM,
                "Variable at %d is not a symbol.", i);
    }

    /* Check correct number of symbols and values. */
    LASSERT(a, syms->count == a->count - 1,
            "The numbers of variables and values not match, "
            "Got %d variables, but %d values.",
            syms->count, a->count - 1);

    /* Assign copies of values to symbols. */
    for (int i = 0; i < syms->count; i++)
    {
        lenv_put(e, syms->cell[i], a->cell[i + 1]);
    }

    lval_del(a);
    return lval_sexpr();
}

void lenv_add_builtin(lenv *e, char *name, lbuiltin func)
{
    lval *k = lval_sym(name);
    lval *v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k);
    lval_del(v);
}

void lenv_add_builtins(lenv *e)
{
    /* List Functions */
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);

    /* Mathematical Functions */
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);

    /* Variable Functions */
    lenv_add_builtin(e, "def", builtin_def);

    /* Print environment function */
    lenv_add_builtin(e, "_Env", builtin_print_env);

    /* Exit function */
    lenv_add_builtin(e, "exit", lispy_exit);
}

/********** Construct new lvals ****************/
/* Create a pointer to a new Number lval */
lval *lval_num(long x)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

/* Create a pointer to a new Error lval  */
lval *lval_err(char *fmt, ...)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERR;

    /* Create a va list and initialize it */
    va_list va;
    va_start(va, fmt);

    /* Allocate 512 bytes of space */
    v->err = malloc(512);

    /* printf the error string with a maximum of 511 characters. */
    vsnprintf(v->err, 511, fmt, va);

    /* Reallocate to number ot bytes actually used. */
    v->err = realloc(v->err, strlen(v->err) + 1);

    /* Clean up our va list. */
    va_end(va);

    return v;
}

/* Create a pointer to a new Symbol lval*/
lval *lval_sym(char *sym)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(sym) + 1);
    strcpy(v->sym, sym);
    return v;
}

/* Create a pointer to a new S-expression lval */
lval *lval_sexpr(void)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

/* Create a pointer to a new Q-expression lval */
lval *lval_qexpr(void)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

/* Create a pointer to a list function */
lval *lval_fun(lbuiltin func)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->fun = func;
    return v;
}

/* Delete a lval to free the memory. */
void lval_del(lval *v)
{
    switch (v->type)
    {
    /* Do nothing special for the number and function types. */
    case LVAL_NUM:
    case LVAL_FUN:
        break;

    /* For Err or Sym, free the string data. */
    case LVAL_ERR:
        free(v->err);
        break;
    case LVAL_SYM:
        free(v->sym);
        break;

    /* If Sexpr or Qexpr then delete all elements inside. */
    case LVAL_QEXPR:
    case LVAL_SEXPR:
        for (int i = 0; i < v->count; i++)
        {
            lval_del(v->cell[i]);
        }
        /* Also free the memory allocated to the cell array itself. */
        free(v->cell);
        break;
    }

    /* Free the memory allocated to the `lval` struct itself. */
    free(v);
}

/************** Parse And Read the input. *******************/
lval *lval_read_num(mpc_ast_t *t)
{
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err("Invalid Number: Out of range.");
}

lval *lval_read(mpc_ast_t *t)
{
    /* If Symbol or Number return conversion to that type. */
    if (STR_CONTAIN(t->tag, "number"))
    {
        return lval_read_num(t);
    }
    if (STR_CONTAIN(t->tag, "symbol"))
    {
        return lval_sym(t->contents);
    }

    /* if root (>) or sexpr then create empty list. */
    lval *x = NULL;
    if (STR_CONTAIN(t->tag, "sexpr") || STR_EQ(t->tag, ">"))
    {
        x = lval_sexpr();
    }
    if (STR_CONTAIN(t->tag, "qexpr"))
    {
        x = lval_qexpr();
    }

    /* Fill this list with any valid expression contained within. */
    for (int i = 0; i < t->children_num; i++)
    {
        if (STR_EQ(t->children[i]->contents, "(") ||
            STR_EQ(t->children[i]->contents, ")") ||
            STR_EQ(t->children[i]->contents, "{") ||
            STR_EQ(t->children[i]->contents, "}") ||
            STR_EQ(t->children[i]->tag, "regex"))
            continue;

        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

lval *lval_add(lval *v, lval *x)
{
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval *) * v->count);
    v->cell[v->count - 1] = x;
    return v;
}

lval *lval_copy(lval *v)
{
    lval *x = malloc(sizeof(lval));
    x->type = v->type;

    switch (v->type)
    {
    /* Copy Functions and Numbers directly. */
    case LVAL_FUN:
        x->fun = v->fun;
        break;
    case LVAL_NUM:
        x->num = v->num;
        break;

    /* Copy Strings using malloc and strcpy. */
    case LVAL_ERR:
        x->err = malloc(strlen(v->err) + 1);
        strcpy(x->err, v->err);
        break;
    case LVAL_SYM:
        x->sym = malloc(strlen(v->sym) + 1);
        strcpy(x->sym, v->sym);
        break;

    /* Copy Lists by copying each sub-expression */
    case LVAL_SEXPR:
    case LVAL_QEXPR:
        x->count = v->count;
        x->cell = malloc(sizeof(lval *) * x->count);
        for (int i = 0; i < x->count; i++)
        {
            x->cell[i] = lval_copy(v->cell[i]);
        }
        break;
    }

    return x;
}

/****************** Print the expressions *****************/
void lval_expr_print(lenv *e, lval *v, char open, char close)
{
    putchar(open);
    for (int i = 0; i < v->count; i++)
    {
        /* Print the value contained within. */
        lval_print(e, v->cell[i]);

        /* Don't print the trailing space for the last element. */
        if (i != v->count - 1)
            putchar(' ');
    }
    putchar(close);
}

/* Print an lval value. */
void lval_print(lenv *e, lval *v)
{
    switch (v->type)
    {
        /* In the case the type is a number, print it.
            Then 'break' out of the switch.
        */
    case LVAL_NUM:
        printf("%ld", v->num);
        break;

    /* In the case the type is an error */
    case LVAL_ERR:
        printf("Error: %s", v->err);
        break;

    case LVAL_SYM:
        printf("%s", v->sym);
        break;

    case LVAL_SEXPR:
        lval_expr_print(e, v, '(', ')');
        break;

    case LVAL_QEXPR:
        lval_expr_print(e, v, '{', '}');
        break;

    case LVAL_FUN:
        printf("Function <%s> at 0X%p", lenv_find_fun(e, v->fun), (void *)v->fun);
        break;
    }
}

/* Print an lval value followed by a newline. */
void lval_println(lenv *e, lval *v)
{
    lval_print(e, v);
    putchar('\n');
}

/*********** Utilities *******************/
char *ltype_name(enum lval_type t)
{
    switch (t)
    {
    case LVAL_FUN:
        return "Function";
    case LVAL_NUM:
        return "Number";
    case LVAL_ERR:
        return "Error";
    case LVAL_SYM:
        return "Symbol";
    case LVAL_SEXPR:
        return "S-Expression";
    case LVAL_QEXPR:
        return "Q-Expression";
    default:
        return "Unknown";
    }
}

void lispy_exit(int *flag)
{
    *flag = 0;
}