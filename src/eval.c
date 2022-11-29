#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpc.h"
#include "eval.h"

/* Evaluate the AST */
lval *lval_eval_sexpr(lval *v)
{
    /* Evaluate the children. */
    for (int i = 0; i < v->count; i++)
    {
        v->cell[i] = lval_eval(v->cell[i]);

        /* If Error happens, return this error. */
        if (v->cell[i]->type == LVAL_ERR)
            return lval_take(v, i);
    }

    /* Empty Expreesion */
    if (v->count == 0) return v;

    /* Single Expression */
    if (v->count == 1) return lval_take(v, 0);

    /* Ensure First Element is Symbol */
    lval *f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        lval_del(f);
        lval_del(v);
        return lval_err("S-expression doesn't start with symbol.");
    }

    /* Call builtin with operator */
    lval *result = builtin_op(v, f->sym);
    lval_del(f);
    return result;
}

lval *lval_eval(lval *v)
{
    /* Evaluation Sexpressions. */
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }

    /* All other lval types remain the same. */
    return v;
}

lval* lval_pop(lval* v, int i)
{
    /* Find the item at "i" */
    lval *x = v->cell[i];

    /* Shift memory after the item at "i" over the top. */
    memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count - i - 1));

    /* Decrease the count of items in the lsit. */
    v->count--;

    /** Reallocate the memory used. */
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    
    return x;
}

lval* lval_take(lval* v, int i)
{
    lval *x = lval_pop(v, i);
    lval_del(v);
    return x;
}

lval* builtin_op(lval* a, char* op)
{
    /* Ensure all arguments are numbers */
    for (int i = 0; i < a->count; i++)
    {
        if (a->cell[i]->type != LVAL_NUM)
        {
            lval_del(a);
            return lval_err("Cannot operate on non-number.");
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

        if (STR_EQ(op, "+")) x->num += y->num;
        if (STR_EQ(op, "-")) x->num -= y->num;
        if (STR_EQ(op, "*")) x->num *= y->num;
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
lval *lval_err(char *err)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(err) + 1);
    strcpy(v->err, err);
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

/* Delete a lval to free the memory. */
void lval_del(lval *v)
{
    switch (v->type)
    {
    /* Do nothing special for the number type. */
    case LVAL_NUM:
        break;

    /* For Err or Sym, free the string data. */
    case LVAL_ERR:
        free(v->err);
        break;
    case LVAL_SYM:
        free(v->sym);
        break;

    /* If Sexpr then delete all elements inside. */
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
    return errno != ERANGE ? lval_num(x) : lval_err("Invalid Number.");
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

    /* Fill this list with any valid expression contained within. */
    for (int i = 0; i < t->children_num; i++)
    {
        if (STR_EQ(t->children[i]->contents, "(") ||
            STR_EQ(t->children[i]->contents, ")") ||
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

/****************** Print the expressions *****************/
void lval_expr_print(lval *v, char open, char close)
{
    putchar(open);
    for (int i = 0; i < v->count; i++)
    {
        /* Print the value contained within. */
        lval_print(v->cell[i]);

        /* Don't print the trailing space for the last element. */
        if (i != v->count - 1)
            putchar(' ');
    }
    putchar(close);
}

/* Print an lval value. */
void lval_print(lval *v)
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
        lval_expr_print(v, '(', ')');
        break;
    }
}

/* Print an lval value followed by a newline. */
void lval_println(lval *v)
{
    lval_print(v);
    putchar('\n');
}