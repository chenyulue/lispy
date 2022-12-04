#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <readline.h>

#include "mpc.h"
#include "eval.h"

/* Create parsers */
mpc_parser_t *Number;
mpc_parser_t *Symbol;
mpc_parser_t *String;
mpc_parser_t *Comment;
// mpc_parser_t *Bool = mpc_new("bool");
mpc_parser_t *Sexpr;
mpc_parser_t *Qexpr;
mpc_parser_t *Expr;
mpc_parser_t *Lispy;

static void run(lenv *e, char const *input, mpc_parser_t *parser, int *flag);

int main(int argc, char **argv)
{
    /* Create parsers */
    Number = mpc_new("number");
    Symbol = mpc_new("symbol");
    String = mpc_new("string");
    Comment = mpc_new("comment");
    // Bool = mpc_new("bool");
    Sexpr = mpc_new("sexpr");
    Qexpr = mpc_new("qexpr");
    Expr = mpc_new("expr");
    Lispy = mpc_new("lispy");

    /* Define parsers with the following DSL. */
    mpca_lang(MPCA_LANG_DEFAULT,
              "number: /-?[0-9]+/;"
              //   "bool: \"true\" | \"false\";"
              "symbol: /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&\\|]+/;"
              "string: /\"(\\\\.|[^\"])*\"/;"
              "comment: /;[^\\r\\n]*/;"
              "sexpr: '(' <expr>* ')';"
              "qexpr: '{' <expr>* '}';"
              "expr: <number> | <symbol> | <string> | <comment> | <sexpr> | <qexpr>;"
              "lispy: /^/ <expr>* /$/;",
              Number, Symbol, String, Comment, Sexpr, Qexpr, Expr, Lispy);

    /* Print Version and Exit Information */
    char const welcome_info[] = ("Lispy Version 0.0.1 (C) Copyright 2022, Chenyu Lue\n"
                                 "Press Ctrl + C or type 'exit' to Exit\n");

    lenv *e = lenv_new();
    lenv_add_builtins(e);
    /* Supplied with list of files */
    if (argc >= 2)
    {
        /* Loop over each supplied filename (starting from 1). */
        for (int i = 1; i < argc; i++)
        {
            /* Argument list with a single argument, the filename. */
            lval *args = lval_add(lval_sexpr(), lval_str(argv[i]));

            /* Pass to builtin load and get the result. */
            lval *x = builtin_load(e, args);

            /* If the result is an error, be sure to print it to stdout. */
            if (x->type == LVAL_ERR)
            {
                lval_println(e, x);
            }
            lval_del(x);
        }
    }
    else
    {
        puts(welcome_info);

        /* In a forever looping */
        int flag = 1;
        while (flag)
        {
            /* Output the prompt and get input */
            char *input = readline("lispy> ");

            /* Add input to history */
            add_history(input);

            /* Attemp to Parse and run the user input. */
            run(e, input, Lispy, &flag);

            /* Free retrieved input */
            free(input);
        }
    }

    /* Delete the environment. */
    lenv_del(e);

    /* Undefine and delete our parsers. */
    mpc_cleanup(8, Number, Symbol, String, Comment, Sexpr, Qexpr, Expr, Lispy);

    return EXIT_SUCCESS;
}

static void run(lenv *e, char const *input, mpc_parser_t *parser, int *flag)
{
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, parser, &r))
    {
        /* On Success Print the AST. */
        lval *result = lval_eval(e, lval_read(r.output));
        // if (result->type == LVAL_FUN && result->builtin == builtin_print_env)
        //     builtin_print_env(e);
        // if (result->type == LVAL_FUN && result->builtin == lispy_exit)
        //     lispy_exit(flag);
        // else
        if (result->type == LVAL_ERR && STR_EQ(result->err, "exit"))
        {
            *flag = 0;
        }
        else
        {
            lval_println(e, result);
        }
        lval_del(result);
        mpc_ast_delete(r.output);
    }
    else
    {
        /* Otherwise print the error. */
        mpc_err_print(r.error);
        mpc_err_delete(r.error);
    }
}
