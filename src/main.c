#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <readline.h>

#include "mpc.h"
#include "eval.h"

static void run(char const *input, mpc_parser_t *parser);

int main(int argc, char **argv)
{
    /* Create parsers */
    mpc_parser_t *Number = mpc_new("number");
    mpc_parser_t *Symbol = mpc_new("symbol");
    mpc_parser_t *Sexpr = mpc_new("sexpr");
    mpc_parser_t *Qexpr = mpc_new("qexpr");
    mpc_parser_t *Expr = mpc_new("expr");
    mpc_parser_t *Lispy = mpc_new("lispy");

    /* Define parsers with the following DSL. */
    mpca_lang(MPCA_LANG_DEFAULT,
        "number: /-?[0-9]+/;"
        "symbol: '+' | '-' | '*' | '/' | \"list\" | \"head\" | \"tail\" | \"join\" | \"eval\";"
        "sexpr: '(' <expr>* ')';"
        "qexpr: '{' <expr>* '}';"
        "expr: <number> | <symbol> | <sexpr> | <qexpr>;"
        "lispy: /^/ <expr>* /$/;",
        Number, Symbol, Sexpr, Qexpr, Expr, Lispy
        );

    /* Print Version and Exit Information */
    char const welcome_info[] = ("Lispy Version 0.0.1 (C) Copyright 2022, Chenyu Lue\n"
                                 "Press Ctrl + C or :q to Exit\n");
    puts(welcome_info);

    /* In a forever looping */
    while (1)
    {
        /* Output the prompt and get input */
        char *input = readline("lispy> ");

        /* Add input to history */
        add_history(input);

        if (!strcmp(input, ":q"))
            break;

        /* Attemp to Parse and run the user input. */
        run(input, Lispy);

        /* Free retrieved input */
        free(input);
    }

    /* Undefine and delete our parsers. */
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    return EXIT_SUCCESS;
}

static void run(char const *input, mpc_parser_t *parser)
{
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, parser, &r))
    {
        /* On Success Print the AST. */
        lval *result = lval_eval(lval_read(r.output));
        lval_println(result);
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
