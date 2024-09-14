#ifndef AST_H_
#define AST_H_

#include <stdlib.h>
#include <string.h>
#include "lexer.h"

typedef enum
{
    AST_SECTION,
    AST_BAR,
    AST_NOTE,
    AST_CHORD,
    AST_BRANCH,
    AST_ASSIGN,
    AST_BINARY,
    AST_INT,
    AST_STRING,
    AST_IDENT,
} ASTNodeType;

typedef enum
{
    OP_CHORD,
    OP_AND,

} OperatorType;

typedef enum
{
    C_Major,
    A_Major,
    B_Major,
} KeyType;

typedef int BPM;

typedef struct
{
    int pitch;
    int length;
} Note;

typedef struct
{
    Note *notes;

    BPM bpm;
    KeyType key;
} Section;

typedef struct AstNode AstNode;

typedef struct AstNode
{
    ASTNodeType type;
    union
    {
        ssize_t int_value;
        char *identifier_name;
        struct
        {
            OperatorType op;
            AstNode *left;
            AstNode *right;
        } binary_expr;
        struct
        {
            OperatorType op;
            AstNode *operand;
        } unary_expr;
    } data;
} AstNode;

AstNode *create_int_literal(ssize_t value);
AstNode *create_identifier(const char *name);
AstNode *create_binary_expr(OperatorType op, AstNode *left, AstNode *right);


void destroy_ast(AstNode *node);

// TODO
void insert_ast_node(AstNode **root, AstNode *node);
void print_ast(AstNode *node); // debugging purposes

#endif // AST_H_
