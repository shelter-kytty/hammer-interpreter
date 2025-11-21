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

void serialiseToken(FILE* file, Token* token) {
    fprintf(file, "\"token\": { ");

    fprintf(file, "\"type\": \"%s\"", tokenName(token->type));

    fprintf(file, ", \"line\": %d", token->line);

    if (token->start == NULL) {
        fprintf(file, ", \"content\": null");
    } else if (token->type == TOKEN_STRING) {
        fprintf(file, ", \"content\": %.*s", token->length, token->start);
    } else if (token->type == TOKEN_FORMAT_STRING) {
        fprintf(file, ", \"content\": %.*s", token->length-1, token->start+1);
    } else {
        fprintf(file, ", \"content\": \"%.*s\"", token->length, token->start);
    }

    fprintf(file, " }");
}

void serialiseLiteral(FILE* file, LiteralExpr* literal) {
    fprintf(file, "{ \"type\": \"LITERAL\", ");
    serialiseToken(file, &literal->token);
    fprintf(file, " }");
}

void serialiseUnary(FILE* file, UnaryExpr* unary) {
    fprintf(file, "{ \"type\": \"UNARY\", ");
    serialiseToken(file, &unary->token);

    fprintf(file, ", \"operand\": ");
    serialiseExpr(file, unary->operand);
    fprintf(file, " }");
}

void serialiseBinary(FILE* file, BinaryExpr* binary) {
    fprintf(file, "{ \"type\": \"BINARY\", ");
    serialiseToken(file, &binary->token);

    fprintf(file, ", \"left\": ");
    serialiseExpr(file, binary->left);
    fprintf(file, ", ");

    fprintf(file, "\"right\": ");
    serialiseExpr(file, binary->right);
    fprintf(file, " }");
}

void serialiseTernary(FILE* file, TernaryExpr* ternary) {
    fprintf(file, "{ \"type\": \"TERNARY\", ");
    serialiseToken(file, &ternary->token);

    fprintf(file, ", \"pivot\": ");
    serialiseExpr(file, ternary->pivot);
    fprintf(file, ", ");

    fprintf(file, "\"left\": ");
    serialiseExpr(file, ternary->left);
    fprintf(file, ", ");

    fprintf(file, "\"right\": ");
    serialiseExpr(file, ternary->right);
    fprintf(file, " }");
}

void serialiseBlock(FILE* file, BlockExpr* block) {
    fprintf(file, "{ \"type\": \"BLOCK\", ");
    serialiseToken(file, &block->token);

    fprintf(file, ", \"subexprs\": [ ");
    for (int i = 0; i < block->count; i++) {
        if (i > 0)
            fprintf(file, ", ");
        serialiseExpr(file, block->subexprs[i]);
    }
    fprintf(file, " ] }");
}

void serialiseExpr(FILE* file, Expr* expression) {
    switch (expression->type) {
        case EXPR_LITERAL: serialiseLiteral(file, (LiteralExpr*)expression); break;
        case EXPR_UNARY: serialiseUnary(file, (UnaryExpr*)expression); break;
        case EXPR_BINARY: serialiseBinary(file, (BinaryExpr*)expression); break;
        case EXPR_TERNARY: serialiseTernary(file, (TernaryExpr*)expression); break;
        case EXPR_BLOCK: serialiseBlock(file, (BlockExpr*)expression); break;
        default: break;
    }
}
