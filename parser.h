enum redirect_type
{
    REDIRECT_IN,
    REDIRECT_OUT,
    REDIRECT_OUT_APPEND,
    REDIRECT_ERR
};

enum token_type
{
    PIPE,
    REDIRECT,
    WORD
};

struct token
{
    enum token_type type;
    union
    {
        char *value;
        char token;
    } data;
};

int tokenize(char *arr[], int n, struct token **tokens);