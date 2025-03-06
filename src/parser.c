#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "parser.h"

/*
void initParser(Parser* parser, Scanner* scanner) {
    parser->scanner = scanner;
    parser->hadError = false;
    parser->panicMode = false;
}

void errorAt(Parser* parser, Token* token, const char* message) {
    if (parser->panicMode) return;
    parser->panicMode = true;
    fprintf(stderr, "[ line %d ] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    }
    else if (token->type == TOKEN_ERROR) {
        // nothing
    }
    else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser->hadError = true;
}

void errorAtPrev(Parser* parser, const char* msg) {
    errorAt(parser, &parser->previous, msg);
}

void errorAtCrnt(Parser* parser, const char* msg) {
    errorAt(parser, &parser->current, msg);
}

void advance(Parser* parser) {
    parser->previous = parser->current;
    parser->current = parser->next;

    for (;;) {
        parser->next = scanToken(parser->scanner);
        if (parser->next.type != TOKEN_ERROR) break;

        errorAtCrnt(parser, parser->next.start);
    }
}

bool checkNext(Parser* parser, TokenType type) {
    return parser->next.type == type;
}

bool check(Parser* parser, TokenType type) {
    return parser->current.type == type;
}

bool match(Parser* parser, TokenType type) {
    if (!check(parser, type)) return false;
    
    advance(parser);
    return true;
}

void consume(Parser* parser, TokenType type, const char* msg) {
    if (check(parser, type)) {
        advance(parser);
        return;
    }

    errorAtCrnt(parser, msg);
}

void end(Parser* parser) {
    if (check(parser, TOKEN_BREAK) || check(parser, TOKEN_SEMICOLON) || check(parser, TOKEN_EOF)) {
        advance(parser);
        return;
    }

    errorAtCrnt(parser, "Expected newline or ';'");
}

bool isAtBreak(Parser* parser) {
    return check(parser, TOKEN_BREAK) || check(parser, TOKEN_SEMICOLON) || check(parser, TOKEN_EOF);
}

void synchronise(Parser* parser) {
    parser->panicMode = false;

    while (parser->current.type != TOKEN_EOF) {
        if (parser->previous.type == TOKEN_BREAK || parser->previous.type == TOKEN_SEMICOLON) return;

        switch (parser->current.type) {
        case TOKEN_PRINT:
        case TOKEN_RETURN:
            return;

        default:
            // do nothing
        }

        advance(parser);
    }
}
*/