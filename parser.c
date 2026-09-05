
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
#include "lexer.h"

// char * tokens[] = {"echo",  "\"Hello World\"", ">", "output.txt", "<", "cat", "|", "echo", "yo"};

// struct token *tokens[10];

int i = 0;
// extern tokens;
// extern tokens_num;

static void get_redirects(Expr *cmd, struct token **tokens, int tokens_num);
static void get_args(Expr *cmd, struct token **tokens, int tokens_num);
static void print_ast(Expr *ast, int level);
static Expr *cmd(struct token **tokens, int token_num);

static void get_args(Expr *cmd, struct token **tokens, int tokens_num)
{
    struct Cmd *cmd_data = &cmd->data.Cmd;
    while (i < tokens_num && tokens[i]->type == WORD)
    {
        cmd_data->args[cmd_data->argc++] = tokens[i]->data.value;
        i++;
    };
}

void get_redirects(Expr *cmd, struct token **tokens, int tokens_num)
{
    struct Cmd *cmd_data = &cmd->data.Cmd;

    while (i < tokens_num && tokens[i]->type == REDIRECT)
    {
        enum redirect_type op = tokens[i++]->data.token;
        char *file = tokens[i++]->data.value;

        cmd_data->redirects[cmd_data->redirectc].op = op;
        cmd_data->redirects[cmd_data->redirectc++].file = file;
    }
}

Expr *cmd(struct token **tokens, int token_num)
{
    Expr *cmd;
    cmd = malloc(sizeof(struct Expr));
    cmd->tag = CMD;
    struct Cmd *cmd_data = &cmd->data.Cmd;
    cmd_data->argc = 0;
    cmd_data->redirectc = 0;
    get_args(cmd, tokens, token_num);
    get_redirects(cmd, tokens, token_num);
    return cmd;
}

Expr *get_pipeline(struct token **tokens, int tokens_num)
{
    Expr *expr;

    expr = cmd(tokens, tokens_num);

    while (i < tokens_num && tokens[i]->type == PIPE)
    {
        i++;
        Expr *pipeline = malloc(sizeof(struct Expr));
        pipeline->tag = PIPE_CMD;
        struct Pipeline *pipe_data = &pipeline->data.Pipeline;
        pipe_data->right = cmd(tokens, tokens_num);
        pipe_data->left = expr;
        expr = pipeline;
    };
    i = 0;
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
            default:
                op = "?";
            }

            printf("op: %s\n", op);
            printf("file: %s\n", cmd_data->redirects[i].file);
        }
    }
}

int main(void)
{
    char *tokens_arr[] = {"echo", "\"Hello World\"", "|", "./output.txt", "2>", "cat", ">>", "yo", "|", "print"};
    int tokens_num = sizeof(tokens_arr) / sizeof(tokens_arr[0]);
    struct token **tokens = malloc(tokens_num * sizeof(struct token *));
    if (tokenize(tokens_arr, tokens_num, tokens) < 0)
    {
        exit(0);
    }

    Expr *ast = get_pipeline(tokens, tokens_num);
    print_ast(ast, 0);
    return 0;
}
