#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpc.h"
#include "eval.h"

/************* Functions to manipulate the environment. ****************/
lenv *lenv_new(void)
{
    lenv *e = malloc(sizeof(lenv));
    e->par = NULL;
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

    /* If no symbol check in parent otherwise error. */
    if (e->par)
    {
        return lenv_get(e->par, k);
    }

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

lenv *lenv_copy(lenv *e)
{
    lenv *x = lenv_new();
    x->par = e->par;
    x->count = e->count;
    x->dicts = malloc(sizeof(struct env_map) * x->count);

    for (int i = 0; i < e->count; i++)
    {
        x->dicts[i].sym = malloc(strlen(e->dicts[i].sym) + 1);
        strcpy(x->dicts[i].sym, e->dicts[i].sym);

        x->dicts[i].val = lval_copy(e->dicts[i].val);
    }

    return x;
}

void lenv_def(lenv *e, lval *k, lval *v)
{
    /* Iterate till e has no parent */
    while (e->par) { e = e->par; }
    
    /* Put value in e */
    lenv_put(e, k, v);
}

char *lenv_find_fun(lenv *e, lbuiltin fun)
{
    for (int i = 0; i < e->count; i++)
    {
        if (e->dicts[i].val->type != LVAL_FUN)
            continue;
        if (e->dicts[i].val->builtin == fun)
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
        lval *err = lval_err("S-Expression starts with incorrect type."
            "Got %s, Expected %s.", ltype_name(f->type), ltype_name(LVAL_FUN));
        lval_del(f);
        lval_del(v);
        return err;
    }

    /* If so call function to get result. */
    lval *result = lval_call(e, f, v);
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
    return builtin_var(e, a, "def");
}

lval *builtin_put(lenv *e, lval *a)
{
    return builtin_var(e, a, "=");
}

lval *builtin_var(lenv *e, lval *a, char *func)
{
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function %s passed incorrect type for variable list. "
            "Got %s, Expected %s.",
            func, ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));

    /* First argument is symbol list */
    lval *syms = a->cell[0];

    /* Ensure all elements of first list are symbols. */
    for (int i = 0; i < syms->count; i++)
    {
        LASSERT(a, syms->cell[i]->type == LVAL_SYM,
                "Variable at %d is not a symbol. Got %s, Expected %s", 
                i, ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
    }

    /* Check correct number of symbols and values. */
    LASSERT(a, syms->count == a->count - 1,
            "The numbers of variables and values passed to function <%s> not match, "
            "Got %d variables, but %d values.",
            func, syms->count, a->count - 1);

    /* Assign copies of values to symbols. */
    for (int i = 0; i < syms->count; i++)
    {
        /* If `def` define in globally. If 'put' define in locally. */
        if (STR_EQ(func, "def"))
        {
            lenv_def(e, syms->cell[i], a->cell[i + 1]);
        }
        if (STR_EQ(func, "="))
        {
            lenv_put(e, syms->cell[i], a->cell[i + 1]);
        }
    }

    lval_del(a);
    return lval_sexpr();
}

lval *builtin_lambda(lenv *e, lval *a)
{
    /* Check Two arguments, each of which is a Q-Expression*/
    LASSERT(a, a->count == 2, 
        "Lambda operator <\\> passed wrong number of arguments. "
        "Got %d, Expected %d", a->count, 2);
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
        "Lambda operator <\\> passed invalid type of argument %d. "
        "Got %s, Expected %s.", 0, ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));
    LASSERT(a, a->cell[1]->type == LVAL_QEXPR,
        "Lambda operator <\\> passed invalid type of argument %d. "
        "Got %s, Expected %s.", 1, ltype_name(a->cell[1]->type), ltype_name(LVAL_QEXPR));

    /* Check first Q-Expression contains only Symbols. */
    for (int i = 0; i < a->cell[0]->count; i++)
    {
        LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
            "Cannot define non-symbol argument at pos %d. Got %s, Expected %s.",
            i, ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
    }

    /* Pop first two arguments and pass them to lval_lambda. */
    lval *formals = lval_pop(a, 0);
    lval *body = lval_pop(a, 0);
    lval_del(a);

    return lval_lambda(formals, body);
}

lval *builtin_fun(lenv *e, lval *a)
{
    LASSERT(a, a->count == 2, "Function 'fun' passed too many arguments. "
        "Got %d, Expected %d.", a->count, 2);
    
    lval *decl = lval_pop(a, 0);
    lval *fun_name = lval_pop(decl, 0);
    lval *lambda = lval_lambda(decl, a);
    lenv_def(e, fun_name, lambda);
    return lval_sexpr();
}

lval *builtin_gt(lenv *e, lval *a)
{
    return builtin_ord(e, a, ">");
}

lval *builtin_lt(lenv *e, lval *a)
{
    return builtin_ord(e, a, "<");
}

lval *builtin_ge(lenv *e, lval *a)
{
    return builtin_ord(e, a, ">=");
}

lval *builtin_le(lenv *e, lval *a)
{
    return builtin_ord(e, a, "<=");
}

lval *builtin_ord(lenv *e, lval *a, char *op)
{
    LASSERT(a, a->count == 2, "Operator %s passed too many arguments. "
        "Got %d, Expected %d.", op, a->count, 2);
    LASSERT(a, a->cell[0]->type == LVAL_NUM, "Operator %s passed invalid type at pos %d. "
        "Got %d, Expected %d.", op, 0, ltype_name(a->cell[0]->type), ltype_name(LVAL_NUM));
    LASSERT(a, a->cell[1]->type == LVAL_NUM, "Operator %s passed invalid type at pos %d. "
        "Got %d, Expected %d.", op, 1, ltype_name(a->cell[1]->type), ltype_name(LVAL_NUM));

    int r;
    if (STR_EQ(op, ">")) r = a->cell[0]->num > a->cell[1]->num;
    if (STR_EQ(op, "<")) r = a->cell[0]->num < a->cell[1]->num;
    if (STR_EQ(op, ">=")) r = a->cell[0]->num >= a->cell[1]->num;
    if (STR_EQ(op, "<=")) r = a->cell[0]->num <= a->cell[1]->num;

    lval_del(a);
    return lval_num(r);
}

int lval_eq(lval *x, lval *y)
{
    /* Different types are always unequal. */
    if (x->type != y->type) return 0;

    /* Compare based upon type */
    switch (x->type)
    {
        /* Compare Number value*/
        case LVAL_NUM: return (x->num == y->num);

        /* Compare String Values */
        case LVAL_ERR: return STR_EQ(x->err, y->err);
        case LVAL_SYM: return STR_EQ(x->sym, y->sym);

        /* If builtin compare, otherwise compare formals and body */
        case LVAL_FUN:
            if (x->builtin || y->builtin)
            {
                return x->builtin == y->builtin;
            }
            else
            {
                return lval_eq(x->formals, y->formals) && lval_eq(x->body, y->body);
            }
        /* If list compare every individual element */
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            if (x->count != y->count) return 0;
            for (int i = 0; i < x->count; i++)
            {
                /* If any element not equal then whole list not equal. */
                if (!lval_eq(x->cell[i], y->cell[i])) return 0;
            }
            /* Otherwise lists must be equal. */
            return 1;
            break;
    }
    return 0;
}

lval *builtin_cmp(lenv *e, lval *a, char *func)
{
    LASSERT(a, a->count == 2, "Operator  %s passed wrong number of arguments. "
        "Got %d, Expected %d", func, a->count, 2);

    int r;
    if (STR_EQ(func, "==")) r = lval_eq(a->cell[0], a->cell[1]);
    if (STR_EQ(func, "!=")) r = !lval_eq(a->cell[0], a->cell[1]);
    lval_del(a);
    return lval_num(r);
}

lval *builtin_eq(lenv *e, lval *a)
{
    return builtin_cmp(e, a, "==");
}

lval *builtin_ne(lenv *e, lval *a)
{
    return builtin_cmp(e, a, "!=");
}

lval *builtin_if(lenv *e, lval *a)
{
    LASSERT(a, a->count == 3, "Function %s passed wrong number of arguments. "
        "Got %d, Expected %d.", "if", a->count, 3);
    LASSERT(a, a->cell[0]->type == LVAL_NUM, "Function %s passed invalid type at pos %d. "
        "Got %d, Expected %d.", "if", 0, ltype_name(a->cell[0]->type), ltype_name(LVAL_NUM));
    LASSERT(a, a->cell[1]->type == LVAL_QEXPR, "Function %s passed invalid type at pos %d. "
        "Got %d, Expected %d.", "if", 1, ltype_name(a->cell[1]->type), ltype_name(LVAL_QEXPR));
    LASSERT(a, a->cell[2]->type == LVAL_QEXPR, "Function %s passed invalid type at pos %d. "
        "Got %d, Expected %d.", "if", 2, ltype_name(a->cell[2]->type), ltype_name(LVAL_QEXPR));

    /* Mark both expressions as evaluable */
    lval *x;
    a->cell[1]->type = LVAL_SEXPR;
    a->cell[2]->type = LVAL_SEXPR;

    if (a->cell[0]->num)
    {
        /* If condition is true, evaluate first expression. */
        x = lval_eval(e, lval_pop(a, 1));
    }
    else
    {
        /* Otherwise, evaluate second expression */
        x = lval_eval(e, lval_pop(a, 2));
    }

    /* Delete argument list and return. */
    lval_del(a);
    return x;
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

    /* Lambda function */
    lenv_add_builtin(e, "\\", builtin_lambda);

    lenv_add_builtin(e, "def", builtin_def);
    lenv_add_builtin(e, "=", builtin_put);

    lenv_add_builtin(e, "fun", builtin_fun);

    /* Comparison Functions */
    lenv_add_builtin(e, "if", builtin_if);
    lenv_add_builtin(e, "==", builtin_eq);
    lenv_add_builtin(e, "!=", builtin_ne);
    lenv_add_builtin(e, ">",  builtin_gt);
    lenv_add_builtin(e, "<",  builtin_lt);
    lenv_add_builtin(e, ">=", builtin_ge);
    lenv_add_builtin(e, "<=", builtin_le);

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
    v->builtin = func;
    return v;
}

/* Create a user defined function. */
lval *lval_lambda(lval *formals, lval *body)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_FUN;

    /* Set Builtin to NULL */
    v->builtin = NULL;

    /* Build new environment */
    v->env = lenv_new();

    /* Set Formals and Body */
    v->formals = formals;
    v->body = body;
    return v;
}

/* Call a function */
lval *lval_call(lenv *e, lval *f, lval *a)
{
    /* If Builtin then simply apply that */
    if (f->builtin) { return f->builtin(e, a); }

    /* Record Argument Counts */
    int given = a->count;
    int total = f->formals->count;

    /* While arguments still remain to be processed */
    while (a->count)
    {
        /* If we've ran out of formal arguments to bind.*/
        if (f->formals->count == 0)
        {
            lval_del(a);
            return lval_err("Function passed too many arguments. "
                "Got %d, Expected %d.", given, total);
        }

        /* Pop the first symbol from the formals */
        lval *sym = lval_pop(f->formals, 0);

        /* Special case to deal with '&' */
        if (STR_EQ(sym->sym, "&"))
        {
            /* Ensure '&' is followed by another symbol */
            if (f->formals->count != 1)
            {
                lval_del(a);
                return lval_err("Function format invalid. Symbol '&' not followed by single symbol.");
            }

            /* Next formal should be bound to remaining arguments */
            lval *nsym = lval_pop(f->formals, 0);
            lenv_put(f->env, nsym, builtin_list(e, a));
            lval_del(sym);
            lval_del(nsym);
            break;
        }

        /* Pop the next argument from the list */
        lval *val = lval_pop(a, 0);

        /* Bind a copy into the function's environment */
        lenv_put(f->env, sym, val);

        /* Delete symbol and value */
        lval_del(sym);
        lval_del(val);
    }

    /* Argument list is now bound so can be cleaned up */
    lval_del(a);

    /* If '&' remains in formal list, bind to empty list */
    if (f->formals->count > 0 && STR_EQ(f->formals->cell[0]->sym, "&"))
    {
        /* Check to ensure that & is not passed invalidly. */
        if (f->formals->count != 2)
        {
            return lval_err("function format invalid. Symbol '&' not followed by single symbol.");
        }

        /* Pop and delete '&' symbol */
        lval_del(lval_pop(f->formals, 0));

        /* Pop next symbol and create empty list */
        lval *sym = lval_pop(f->formals, 0);
        lval *val = lval_qexpr();

        /* Bind to environment and delete */
        lenv_put(f->env, sym, val);
        lval_del(sym);
        lval_del(val);
    }

    /* If all formals have been bound evaluate it */
    if (f->formals->count == 0)
    {
        /* Set environment parent to evaluation environment */
        f->env->par = e;

        /* Evaluate and return the result */
        return builtin_eval(f->env, lval_add(lval_qexpr(), lval_copy(f->body)));
    }
    else
    {
        /* Otherwise return partially evaluated function */
        return lval_copy(f);
    }
}

/* Delete a lval to free the memory. */
void lval_del(lval *v)
{
    switch (v->type)
    {
    /* Do nothing special for the number and function types. */
    case LVAL_NUM:
        break;
    case LVAL_FUN:
        if (!v->builtin)
        {
            lenv_del(v->env);
            lval_del(v->formals);
            lval_del(v->body);
        }
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
        if (v->builtin)
        { 
            x->builtin = v->builtin;
        }
        else 
        {
            x->builtin = NULL;
            x->env = lenv_copy(v->env);
            x->formals = lval_copy(v->formals);
            x->body = lval_copy(v->body);
        }
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
        if (v->builtin)
        {
            printf("Builtin `%s` at 0X%p", 
                lenv_find_fun(e, v->builtin), (void *)v->builtin);
        }
        else
        {
            printf("(\\ ");
            lval_print(e, v->formals);
            putchar(' ');
            lval_print(e, v->body);
            putchar(')');
        }
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