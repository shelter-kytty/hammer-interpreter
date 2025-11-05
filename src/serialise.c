#include "serialise.h"

#include <stdio.h>
#include "ast.h"
#include "scanner.h"


const char* tokenName(TokenType type) {
   switch (type) {
        case TOKEN_LEFT_PAREN:      return "LEFT_PAREN";
        case TOKEN_RIGHT_PAREN:     return "RIGHT_PAREN";
        case TOKEN_LEFT_BRACKET:    return "LEFT_BRACKET";
        case TOKEN_RIGHT_BRACKET:   return "RIGHT_BRACKET";
        case TOKEN_LEFT_BRACE:      return "LEFT_BRACE";
        case TOKEN_RIGHT_BRACE:     return "RIGHT_BRACE";
        case TOKEN_SEMICOLON:       return "SEMICOLON";
        case TOKEN_DOT:             return "DOT";
        case TOKEN_DOT_DOT:         return "DOT_DOT";
        case TOKEN_COMMA:           return "COMMA";
        case TOKEN_PLUS:            return "PLUS";
        case TOKEN_MINUS:           return "MINUS";
        case TOKEN_STAR:            return "STAR";
        case TOKEN_SLASH:           return "SLASH";
        case TOKEN_PERCENT:         return "PERCENT";
        case TOKEN_UCARET:          return "UCARET";
        case TOKEN_EQUALS:          return "EQUALS";
        case TOKEN_RECEIVE:         return "RECEIVE";
        case TOKEN_COLON:           return "COLON";
        case TOKEN_ROCKET:          return "ROCKET";
        case TOKEN_GREATER:         return "GREATER";
        case TOKEN_LESS:            return "LESS";
        case TOKEN_GREATER_EQUALS:  return "GREATER_EQUALS";
        case TOKEN_LESS_EQUALS:     return "LESS_EQUALS";
        case TOKEN_BANG_EQUALS:     return "BANG_EQUALS";
        case TOKEN_EQUALS_EQUALS:   return "EQUALS_EQUALS";
        case TOKEN_DOLLAR:          return "DOLLAR";
        case TOKEN_QUESTION:        return "QUESTION";
        case TOKEN_BANG:            return "BANG";
        case TOKEN_PIPE:            return "PIPE";
        case TOKEN_SPIGOT:          return "SPIGOT";
        case TOKEN_CUSTOM:          return "CUSTOM";
        case TOKEN_IDENTIFIER:      return "IDENTIFIER";
        case TOKEN_INTEGER:         return "INTEGER";
        case TOKEN_FLOAT:           return "FLOAT";
        case TOKEN_STRING:          return "STRING";
        case TOKEN_FORMAT_STRING:   return "FORMAT_STRING";
        case TOKEN_CHAR:            return "CHAR";
        case TOKEN_TRUE:            return "TRUE";
        case TOKEN_FALSE:           return "FALSE";
        case TOKEN_UNIT:            return "UNIT";
        case TOKEN_WILDCARD:        return "WILDCARD";
        case TOKEN_GLYPH:           return "GLYPH";
        case TOKEN_IF:              return "IF";
        case TOKEN_THEN:            return "THEN";
        case TOKEN_ELSE:            return "ELSE";
        case TOKEN_MATCH:           return "MATCH";
        case TOKEN_CONS:            return "CONS";
        case TOKEN_CAR:             return "CAR";
        case TOKEN_CDR:             return "CDR";
        case TOKEN_AND:             return "AND";
        case TOKEN_OR:              return "OR";
        case TOKEN_IN:              return "IN";
        case TOKEN_RETURN:          return "RETURN";
        case TOKEN_SOF:             return "SOF";
        case TOKEN_EOF:             return "EOF";
        case TOKEN_ERROR:           return "ERROR";
        default: return "UNKNOWN";
    }
}

void serialiseToken(Token* token) {
    printf("\"token\": { ");
    if (token->start == NULL) {
        printf("\"content\": \"null\"");
    } else if (token->type == TOKEN_STRING || token->type == TOKEN_FORMAT_STRING) {
        printf("\"content\": %.*s", token->length, token->start);
    } else {
        printf("\"content\": \"%.*s\"", token->length, token->start);
    }

    printf(", \"type\": \"%s\"", tokenName(token->type));

    printf(", \"line\": %d", token->line);

    printf(" }");
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
        if (i > 0)
            printf(", ");
        serialiseExpr(block->subexprs[i]);
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
