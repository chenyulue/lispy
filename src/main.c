#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <readline.h>

int main(int argc, char **argv)
{
    /* Print Version and Exit Information */
    char const welcome_info[] = (
        "Lispy Version 0.0.1 (C) Copyright 2022, Chenyu Lue\n"
        "Press Ctrl + C or :q to Exit\n"
    );
    puts(welcome_info);

    /* In a forever looping */
    while (1)
    {
        /* Output the prompt and get input */
        char *input = readline("lispy> ");

        /* Add input to history */
        add_history(input);

        if (!strcmp(input, ":q")) break;

        /* Echo input back to user */
        printf("You entered: %s\n", input);

        /* Free retrieved input */
        free(input);
    }

    return EXIT_SUCCESS;
}
