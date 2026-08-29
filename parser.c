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

#define MAX_ARGS 10
#define MAX_REDIRECTS 10

// char * tokens[] = {"echo",  "\"Hello World\"", ">", "output.txt", "<", "cat", "|", "echo", "yo"};

char *tokens_arr[] = {"echo", "\"Hello World\"", "|", "./output.txt", "2>", "cat", ">>", "yo"};
int tokens_num = sizeof(tokens_arr) / sizeof(tokens_arr[0]);
struct token *tokens[8];

int i;

struct Io_redirect
{
    enum redirect_type op;
    char *file;
} Io_redirect;

typedef struct Expr
{
    enum
    {
        PIPE_CMD,
        CMD,
    } tag;
    union
    {
        struct Pipeline
        {
            struct Expr *left;
            struct Expr *right;
        } Pipeline;

        struct Cmd
        {
            char *word;
            char *args[MAX_ARGS];
            int argc;
            struct Io_redirect redirects[MAX_REDIRECTS];
            int redirectc;
        } Cmd;

    } data;

} Expr;

char *get_word();
void get_args(struct Cmd *cmd);
void get_redirects(Expr *cmd);
Expr *cmd();
Expr *get_pipeline();

char *get_word()
{

    return tokens[i++]->data.value;
}

void get_args(struct Cmd *cmd)
{
    while (i < tokens_num && tokens[i]->type == WORD)
    {
        cmd->args[cmd->argc++] = tokens[i]->data.value;
        i++;
    };
}

void get_redirects(Expr *cmd)
{
    struct Cmd *cmd_data = &cmd->data.Cmd;

    while (i < tokens_num && tokens[i]->type == REDIRECT)
    {

        char *file = tokens[i++]->data.value;
        cmd_data->redirects[cmd_data->redirectc].op = tokens[i]->data.token;
        cmd_data->redirects[cmd_data->redirectc++].file = file;
    }
}

Expr *cmd()
{
    Expr *cmd;
    cmd = malloc(sizeof(struct Expr));
    cmd->tag = CMD;
    struct Cmd *cmd_data = &cmd->data.Cmd;
    cmd_data->argc = 0;
    cmd_data->redirectc = 0;
    cmd_data->word = get_word();
    get_args(cmd_data);
    get_redirects(cmd);
    return cmd;
}

Expr *get_pipeline()
{
    Expr *expr;

    expr = cmd();

    while (tokens[i]->type == PIPE)
    {
        i++;
        Expr *pipeline = malloc(sizeof(struct Expr));
        pipeline->tag = PIPE_CMD;
        struct Pipeline *pipe_data = &pipeline->data.Pipeline;
        pipe_data->right = cmd();
        pipe_data->left = expr;
        expr = pipeline;
    };
    return expr;
}

void print_ast(Expr *ast, int level)
{
    if (ast->tag == PIPE_CMD)
    {
        printf("PIPE\n");
        printf("left:\n");
        print_ast(ast->data.Pipeline.left, level + 1);
        printf("right:\n");

        print_ast(ast->data.Pipeline.right, level + 1);
        for (int i = 0; i < level; i++)
        {
            printf("\t");
        }
    }
    else
    {
        printf("CMD\n");
        struct Cmd *cmd_data = &ast->data.Cmd;
        printf("%s\n", cmd_data->word);
        // get size of args
        printf("args\n");
        for (int i = 0; i < cmd_data->argc; i++)
        {
            printf("%s\n", cmd_data->args[i]);
        }

        printf("redirects\n");
        for (int i = 0; i < cmd_data->redirectc; i++)
        {
            char *op;
            switch (cmd_data->redirects[i].op)
            {
            case REDIRECT_IN:
                op = "<";
                break;
            case REDIRECT_OUT:
                op = ">";
                break;
            case REDIRECT_OUT_APPEND:
                op = ">>";
                break;
            case REDIRECT_ERR:
                op = "2>";
                break;
            }
            printf("op: %s\n", op);
            printf("file: %s\n", cmd_data->redirects[i].file);
        }
    }
}

int main(void)
{
    if (tokenize(tokens_arr, tokens_num, tokens) < 0)
    {
        exit(0);
    }

    Expr *ast = get_pipeline();
    print_ast(ast, 0);
    return 0;
}
