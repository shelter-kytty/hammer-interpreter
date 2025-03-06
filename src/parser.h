#ifndef parser_h_hammer
#define parser_h_hammer

#include "common.h"
#include "scanner.h"

/*
typedef struct {
    Scanner* scanner;
    Token previous;
    Token current;
    Token next;
    bool hadError;
    bool panicMode;
} Parser;

void initParser(Parser* parser, Scanner* scanner);
void errorAtCrnt(Parser* parser, const char* msg);
void errorAtPrev(Parser* parser, const char* msg);
void advance(Parser* parser);
bool checkNext(Parser* parser, TokenType type);
bool check(Parser* parser, TokenType type);
bool match(Parser* parser, TokenType type);
void consume(Parser* parser, TokenType type, const char* msg);
void end(Parser* parser);
bool isAtBreak(Parser* parser);
void synchronise(Parser* parser);
*/

#endif