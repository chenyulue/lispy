#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpc.h"
#include "eval.h"

long eval(mpc_ast_t *t)
{
    /* If tagged as number, return it directly. */
    if (STR_CONTAIN(t->tag, "number"))
    {
        return atol(t->contents);
    }

    /* The operator is always the second child. */
    char *op = t->children[1]->contents;

    /* Store the third child in `x` */
    long x = eval(t->children[2]);

    /* Iterate the remaining children and combinding. */
    for (int i = 3; STR_CONTAIN(t->children[i]->tag, "expr"); i++)
    {
        x = eval_op(x, op, eval(t->children[i]));
    }

    return x;
}

long eval_op(long x, char *op, long y)
{
    if (STR_EQ(op, "+")) 
    {
        return x + y;
    }
    if (STR_EQ(op, "-"))
    {
        return x - y;
    }
    if (STR_EQ(op, "*"))
    {
        return x * y;
    }
    if (STR_EQ(op, "/"))
    {
        return x / y;
    }
    return 0;
}