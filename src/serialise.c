#include "serialise.h"

#include <stdio.h>
#include "ast.h"

void serialiseToken(Token* token) {
    if (token->start != NULL) {
        printf("\"token\": \"%.*s\"", token->length, token->start);
    } else {
        printf("\"token\": \"null\"");
    }
}

void serialiseLiteral(LiteralExpr* literal) {
    printf("{ \"type\": \"Literal\", ");
    serialiseToken(&literal->token);
    printf(" }");
}

void serialiseUnary(UnaryExpr* unary) {
    printf("{ \"type\": \"Unary\", ");
    serialiseToken(&unary->token);

    printf(", \"operand\": ");
    serialiseExpr(unary->operand);
    printf(" }");
}

void serialiseBinary(BinaryExpr* binary) {
    printf("{ \"type\": \"Binary\", ");
    serialiseToken(&binary->token);

    printf(", \"left\": ");
    serialiseExpr(binary->left);
    printf(", ");

    printf("\"right\": ");
    serialiseExpr(binary->right);
    printf(" }");
}

void serialiseTernary(TernaryExpr* ternary) {
    printf("{ \"type\": \"Ternary\", ");
    serialiseToken(&ternary->token);

    printf(", \"pivot\": ");
    serialiseExpr(ternary->pivot);
    printf(", ");

    printf("\"left\": ");
    serialiseExpr(ternary->left);
    printf(", ");

    printf("\"right\": ");
    serialiseExpr(ternary->right);
    printf(" }");
}

void serialiseBlock(BlockExpr* block) {
    printf("{ \"type\": \"Block\", ");
    serialiseToken(&block->token);

    printf(", \"subexprs\": [ ");
    for (int i = 0; i < block->count; i++) {
        serialiseExpr(block->subexprs[i]);
        if (i > 0 && i < block->count - 1)
            printf(", ");
    }
    printf(" ] }");
}

void serialiseExpr(Expr* expression) {
    switch (expression->type) {
        case EXPR_LITERAL: serialiseLiteral((LiteralExpr*)expression); break;
        case EXPR_UNARY: serialiseUnary((UnaryExpr*)expression); break;
        case EXPR_BINARY: serialiseBinary((BinaryExpr*)expression); break;
        case EXPR_TERNARY: serialiseTernary((TernaryExpr*)expression); break;
        case EXPR_BLOCK: serialiseBlock((BlockExpr*)expression); break;
        default: break;
    }
}
