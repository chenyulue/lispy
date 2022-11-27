#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <readline.h>

#include "mpc.h"

static void run(char const *input, mpc_parser_t *parser);

int main(int argc, char **argv)
{
    /* Create parsers */
    mpc_parser_t *Number = mpc_new("number");
    mpc_parser_t *Operator = mpc_new("operator");
    mpc_parser_t *Expr = mpc_new("expr");
    mpc_parser_t *Lispy = mpc_new("lispy");

    /* Define parsers with the following DSL. */
    mpca_lang(MPCA_LANG_DEFAULT,
        "number: /-?[0-9]+/;"
        "operator: '+' | '-' | '*' | '/';"
        "expr: <number> | '(' <operator> <expr>+ ')';"
        "lispy: /^/ <operator> <expr>+ /$/;",
        Number, Operator, Expr, Lispy
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
    mpc_cleanup(4, Number, Operator, Expr, Lispy);

    return EXIT_SUCCESS;
}

static void run(char const *input, mpc_parser_t *parser)
{
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, parser, &r))
    {
        /* On Success Print the AST. */
        mpc_ast_print(r.output);
        mpc_ast_delete(r.output);
    }
    else
    {
        /* Otherwise print the error. */
        mpc_err_print(r.error);
        mpc_err_delete(r.error);
    }
}
