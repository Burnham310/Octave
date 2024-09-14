#include "ast.h"
#include <stdio.h>

AstNode *create_int_literal(const ssize_t value)
{
    AstNode *node = (AstNode *)malloc(sizeof(AstNode));
    node->type = AST_INT;
    node->data.int_value = value;
    return node;
}

AstNode *create_identifier(const char *name)
{
    AstNode *node = (AstNode *)malloc(sizeof(AstNode));
    node->type = AST_IDENT;
    node->data.identifier_name = strdup(name);
    return node;
}

AstNode *create_binary_expr(const OperatorType op, AstNode *left, AstNode *right)
{
    AstNode *node = (AstNode *)malloc(sizeof(AstNode));
    node->type = AST_BINARY;
    node->data.binary_expr.op = op;
    node->data.binary_expr.left = left;
    node->data.binary_expr.right = right;
    return node;
}

void destroy_ast(AstNode *node)
{
    if (node == NULL)
        return;

    switch (node->type)
    {
    case AST_IDENT:
        free(node->data.identifier_name);
        break;
    case AST_BINARY:
        destroy_ast(node->data.binary_expr.left);
        destroy_ast(node->data.binary_expr.right);
        break;
    default:
        break;
    }
    free(node);
}