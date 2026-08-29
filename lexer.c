#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include "parser.h"

int tokenize(char *arr[], int n, struct token **tokens)
{
    int i;
    for (i = 0; i < n; i++)
    {

        tokens[i] = malloc(sizeof(struct token));
        if (!strcmp(arr[i], "|"))
        {
            if (i == 0 || tokens[i - 1]->type != WORD)
            {
                printf("error: pipe must come after word\n");
                return -1;
            }
            tokens[i]->type = PIPE;
        }
        else if (!strcmp(arr[i], ">"))
        {
            if (i == 0 || tokens[i - 1]->type != WORD)
            {
                printf("error: redirect must come after word\n");
                return -1;
            }
            tokens[i]->type = REDIRECT;
            tokens[i]->data.token = REDIRECT_OUT;
        }
        else if (!strcmp(arr[i], "<"))
        {
            if (i == 0 || tokens[i - 1]->type != WORD)
            {
                printf("error: redirect must come after word\n");
                return -1;
            }
            tokens[i]->type = REDIRECT;
            tokens[i]->data.token = REDIRECT_IN;
        }
        else if (!strcmp(arr[i], ">>"))
        {
            if (i == 0 || tokens[i - 1]->type != WORD)
            {
                printf("error: redirect must come after word\n");
                return -1;
            }
            tokens[i]->type = REDIRECT;
            tokens[i]->data.token = REDIRECT_OUT_APPEND;
        }
        else if (!strcmp(arr[i], "2>"))
        {
            if (i == 0 || tokens[i - 1]->type != WORD)
            {
                printf("error: redirect must come after word\n");
                return -1;
            }
            tokens[i]->type = REDIRECT;
            tokens[i]->data.token = REDIRECT_ERR;
        }
        else
        {
            tokens[i]->type = WORD;
            char *str_ptr = malloc(strlen(arr[i]) + 1);
            strcpy(str_ptr, arr[i]);
            tokens[i]->data.value = str_ptr;
        }
    }

    if (i > 0 && tokens[i - 1]->type != WORD)
    {
        printf("error: command must not end with operator\n");
        return -1;
    }
    return 0;
}

// int main(void)
// {
//     char *arr[] = {"echo", "\"Hello World\"", "|", "./output.txt", "2>", "cat", ">>", "yo"};
//     struct token **tokens = malloc(8 * sizeof(struct token));
//     tokenize(arr, 8, tokens);
//     // for (int i = 0; i < 8; i++)
//     // {
//     //     printf("%d %s %p", tokens[i]->type, tokens[i]->data.value, &tokens[i]);
//     // }
// }
