
#include "lexer.h"

#define MAX_ARGS 10
#define MAX_REDIRECTS 10
#define MAX_TASKS 10

#ifndef PARSER_H
#define PARSER_H

struct Io_redirect
{
    enum redirect_type op;
    char *file;
};

typedef struct Cmd
{
    char *args[MAX_ARGS];
    int argc;
    struct Io_redirect redirects[MAX_REDIRECTS];
    int redirectc;
} Cmd;

// typedef struct Expr
// {
//     enum
//     {
//         PIPE_CMD,
//         CMD,
//     } tag;
//     union
//     {
//         // struct Pipeline
//         // {
//         //     struct Expr *left;
//         //     struct Expr *right;
//         // } Pipeline;

//         struct Cmd
//         {
//             char *args[MAX_ARGS];
//             int argc;
//             struct Io_redirect redirects[MAX_REDIRECTS];
//             int redirectc;
//         } Cmd;

//     } data;
// } Expr;

int get_pipeline(struct token **tokens, int tokens_num, Cmd *tasks[]);
void print_ast(Cmd *ast, int level);

#endif