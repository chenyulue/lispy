#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpc.h"
#include "eval.h"

lval eval(mpc_ast_t *t)
{
    /* If tagged as number, return it directly. */
    if (STR_CONTAIN(t->tag, "number"))
    {
        /* Check if there is some error in conversion. */
        errno = 0;
        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
    }

    /* The operator is always the second child. */
    char *op = t->children[1]->contents;

    /* Store the third child in `x` */
    lval x = eval(t->children[2]);

    /* Iterate the remaining children and combinding. */
    for (int i = 3; STR_CONTAIN(t->children[i]->tag, "expr"); i++)
    {
        x = eval_op(x, op, eval(t->children[i]));
    }

    return x;
}

lval eval_op(lval x, char *op, lval y)
{
    /* If either value is an error return it. */
    if (x.type == LVAL_ERR)
    {
        return x;
    }
    if (y.type == LVAL_ERR)
    {
        return y;
    }

    /* Otherwise do maths on the number values. */
    if (STR_EQ(op, "+"))
    {
        return lval_num(x.num + y.num);
    }
    if (STR_EQ(op, "-"))
    {
        return lval_num(x.num - y.num);
    }
    if (STR_EQ(op, "*"))
    {
        return lval_num(x.num * y.num);
    }
    if (STR_EQ(op, "/"))
    {
        /* If second operand is zero return error. */
        return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num);
    }

    return lval_err(LERR_BAD_OP);
}

/* Create a new number type lval */
lval lval_num(long x)
{
    return (lval){
        .type = LVAL_NUM,
        .num = x,
    };
}
/* Create a new error type lval */
lval lval_err(enum lval_err err)
{
    return (lval){
        .type = LVAL_ERR,
        .err = err,
    };
}

/* Print an lval value. */
void lval_print(lval v)
{
    switch (v.type)
    {
        /* In the case the type is a number, print it.
            Then 'break' out of the switch.
        */
    case LVAL_NUM:
        printf("%ld", v.num);
        break;

    /* In the case the type is an error */
    case LVAL_ERR:
        switch (v.err)
        {
        case LERR_DIV_ZERO:
            printf("Error: Division by Zero.");
            break;
        case LERR_BAD_OP:
            printf("Error: Invalid operator.");
            break;
        case LERR_BAD_NUM:
            printf("Error: Invalid number.");
            break;
        }
        break;
    }
}

/* Print an lval value followed by a newline. */
void lval_println(lval v)
{
    lval_print(v);
    putchar('\n');
}