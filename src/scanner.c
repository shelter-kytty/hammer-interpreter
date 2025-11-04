#include <string.h>
#include <stdio.h>

#include "common.h"

#include "scanner.h"


void initScanner(Scanner* scanner, const char* source) {
    scanner->trueBeginning = source;
    scanner->start = source;
    scanner->current = source;
    scanner->line = 1;
}

void printToken(const Token* token) {
    switch (token->type) {
        // Symbols :: 0 - 4
        case TOKEN_LEFT_PAREN:      printf("(\n"); break;
        case TOKEN_RIGHT_PAREN:     printf(")\n"); break;
        case TOKEN_LEFT_BRACKET:    printf("[\n"); break;
        case TOKEN_RIGHT_BRACKET:   printf("]\n"); break;
        case TOKEN_LEFT_BRACE:      printf("{\n"); break;
        case TOKEN_RIGHT_BRACE:     printf("}\n"); break;
        case TOKEN_SEMICOLON:       printf(";\n"); break;

        // Operators :: 5 - 22
        case TOKEN_DOT:             printf(".\n"); break;
        case TOKEN_DOT_DOT:         printf("..\n"); break;
        case TOKEN_COMMA:           printf(",\n"); break;
        case TOKEN_PLUS:            printf("+\n"); break;
        case TOKEN_MINUS:           printf("-\n"); break;
        case TOKEN_STAR:            printf("*\n"); break;
        case TOKEN_SLASH:           printf("/\n"); break;
        case TOKEN_PERCENT:         printf("%%\n"); break;
        case TOKEN_UCARET:          printf("^\n"); break;

        case TOKEN_EQUALS:          printf("=\n"); break;
        case TOKEN_RECEIVE:         printf("<<\n"); break;
        case TOKEN_COLON:           printf(":\n"); break;
        case TOKEN_ROCKET:          printf("=>\n"); break;

        case TOKEN_GREATER:         printf(">\n"); break;
        case TOKEN_LESS:            printf("<\n"); break;
        case TOKEN_GREATER_EQUALS:  printf(">=\n"); break;
        case TOKEN_LESS_EQUALS:     printf("<=\n"); break;
        case TOKEN_BANG_EQUALS:     printf("!=\n"); break;
        case TOKEN_EQUALS_EQUALS:   printf("==\n"); break;
        case TOKEN_DOLLAR:          printf("$\n"); break;

        case TOKEN_QUESTION:        printf("?\n"); break;
        case TOKEN_BANG:            printf("!\n"); break;
        case TOKEN_PIPE:            printf("|\n"); break;
        case TOKEN_SPIGOT:          printf("|>\n"); break;

        case TOKEN_CUSTOM:          printf("cstm: %.*s\n", token->length, token->start); break;


        // Literals :: 23 - 32
        case TOKEN_IDENTIFIER:      printf("idf: %.*s\n", token->length, token->start); break;
        case TOKEN_INTEGER:         printf("int: %.*s\n", token->length, token->start); break;
        case TOKEN_FLOAT:           printf("flt: %.*s\n", token->length, token->start); break;
        case TOKEN_STRING:          printf("str: %.*s\n", token->length - 2, token->start + 1); break;
        case TOKEN_FORMAT_STRING:   printf("str: %.*s\n", token->length - 3, token->start + 2); break;
        case TOKEN_CHAR:            printf("chr: %c\n", token->start[1]); break;
        case TOKEN_TRUE:            printf("true\n"); break;
        case TOKEN_FALSE:           printf("false\n"); break;
        case TOKEN_UNIT:            printf("unit\n"); break;
        case TOKEN_WILDCARD:        printf("_\n"); break;
        case TOKEN_GLYPH:           printf("glph: %.*s\n", token->length, token->start); break;

        // Keywords :: 33 - 47
        case TOKEN_IF:              printf("if\n"); break;
        case TOKEN_THEN:            printf("then\n"); break;
        case TOKEN_ELSE:            printf("else\n"); break;
        case TOKEN_MATCH:           printf("match\n"); break;
        case TOKEN_CONS:            printf("cons\n"); break;
        case TOKEN_CAR:             printf("car\n"); break;
        case TOKEN_CDR:             printf("cdr\n"); break;
        case TOKEN_AND:             printf("and\n"); break;
        case TOKEN_OR:              printf("or\n"); break;
        case TOKEN_IN:              printf("in\n"); break;
        case TOKEN_RETURN:          printf("return\n"); break;

        // Control :: 48 - 51
        case TOKEN_SOF:             printf("SOF\n"); break;
        case TOKEN_EOF:             printf("EOF\n"); break;
        case TOKEN_ERROR:           printf("[ Error at line %d ]: %s\n", token->line, token->start); break;


        default:                    printf("Print rule does not exist for token %d [ %.*s ]\n",
            token->type, token->length, token->start); break;
    }
}


static bool isSign(char ch) {
    return ch == '+' || ch == '-';
}

static bool isAlpha(char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch == '_');
}

static bool isDigit(char ch) {
    return ch >= '0' && ch <= '9';
}

static bool isAtEnd(Scanner* scanner) {
    return *scanner->current == '\0';
}

static char advance(Scanner* scanner) {
    return *scanner->current++;
}

static char peek(Scanner* scanner) {
    return *scanner->current;
}

static char peekNext(Scanner* scanner) {
    if (isAtEnd(scanner)) return '\0';
    return scanner->current[1];
}

static bool match(Scanner* scanner, char expected) {
    if (isAtEnd(scanner)) return false;
    if (*scanner->current != expected) return false;
    scanner->current++;
    return true;
}

static void endlineBreak(Scanner* scanner) {
    for (;;) {
        char c = peek(scanner);

        switch (c) {
        case ' ':
        case '\r':
        case '\t':
            advance(scanner);
            break;
        case '\n':
            scanner->line++;
            advance(scanner);
            break;
        case '/': {
            if (peekNext(scanner) == '/') {

                while (peek(scanner) != '\n' && !isAtEnd(scanner))
                    advance(scanner);

                if (!isAtEnd(scanner)) {
                    advance(scanner);
                    scanner->line++;
                }

                break;
            }
            return;
        }
        default:
            return;
        }
    }
}

static Token makeToken(Scanner* scanner, TokenType type) {
    Token token;
    token.line = scanner->line;
    token.start = scanner->start;
    token.length = (int)(scanner->current - scanner->start);
    token.type = type;

#ifdef DEBUG_DISPLAY_TOKENS
    printToken(&token);
#endif

    return token;
}

static Token errorToken(Scanner* scanner, const char* msg) {
    Token err;
    err.line = scanner->line;
    err.start = msg;
    err.length = strlen(msg);
    err.type = TOKEN_ERROR;

#ifdef DEBUG_DISPLAY_TOKENS
    printToken(&err);
#endif

    return err;
}

static Token scanNums(Scanner* scanner) {
    TokenType type = TOKEN_INTEGER;

    while (isDigit(peek(scanner))) {
        advance(scanner);
    }

    if (peek(scanner) == '.' && isDigit(peekNext(scanner))) {
        advance(scanner);

        while (isDigit(peek(scanner))) {
            advance(scanner);
        }

        type = TOKEN_FLOAT;
    }

    if (match(scanner, 'e') || match(scanner, 'E')) {
        if (isSign(peek(scanner))) {
            advance(scanner);
        }

        if (!isDigit(peek(scanner))) {
            return errorToken(scanner, "Exponent must have a power");
        }

        while (isDigit(peek(scanner))) {
            advance(scanner);
        }

        type = TOKEN_FLOAT;
    }


    return makeToken(scanner, type);
}

static TokenType idToken(Scanner* scanner, const char* key, int start, int length, TokenType type) {
    if (scanner->current - scanner->start == start + length &&
        memcmp(scanner->start + start, key, length) == 0) return type;
    return TOKEN_IDENTIFIER;
}

static TokenType identifierType(Scanner* scanner) {
    switch (scanner->start[0]) {
    case '_': return idToken(scanner, "", 1, 0, TOKEN_WILDCARD);

    case 'a': return idToken(scanner, "nd", 1, 2, TOKEN_AND);
    case 'c': {
        switch (scanner->start[1]) {
        case 'a': return idToken(scanner, "r", 2, 1, TOKEN_CAR);
        case 'd': return idToken(scanner, "r", 2, 1, TOKEN_CDR);
        case 'o': return idToken(scanner, "ns", 2, 2, TOKEN_CONS);
        }
        break; // C3>c(• -• ) wielding my icecream at you
    }
    case 'e': return idToken(scanner, "lse", 1, 3, TOKEN_ELSE);
    case 'f': {
        switch (scanner->start[1]) {
        case 'a': return idToken(scanner, "lse", 2, 3, TOKEN_FALSE);
        case 'r': return idToken(scanner, "st", 2, 2, TOKEN_CAR);
        }
        break;
    }
    case 'i': {
        switch (scanner->start[1]) {
        case 'f': return idToken(scanner, "", 2, 0, TOKEN_IF);
        case 'n': return idToken(scanner, "", 2, 0, TOKEN_IN);
        }
        break;
    }
    case 'm': return idToken(scanner, "atch", 1, 4, TOKEN_MATCH);
    case 'o': return idToken(scanner, "r", 1, 1, TOKEN_OR);
    case 'r': return idToken(scanner, "eturn", 1, 5, TOKEN_RETURN);
    case 's': return idToken(scanner, "cnd", 1, 3, TOKEN_CDR);
    case 't': {
        switch (scanner->start[1]) {
        case 'h': return idToken(scanner, "en", 2, 2, TOKEN_THEN);
        case 'r': return idToken(scanner, "ue", 2, 2, TOKEN_TRUE);
        }
        break;
    }
    case 'u': return idToken(scanner, "nit", 1, 3, TOKEN_UNIT);
    default: break;
    }

    return TOKEN_IDENTIFIER;
}

static Token scanText(Scanner* scanner) {
    while (isDigit(peek(scanner)) || isAlpha(peek(scanner))) advance(scanner);
    return makeToken(scanner, identifierType(scanner));
}

static Token character(Scanner* scanner) {
    if (peek(scanner) == '\'') {
        return errorToken(scanner, "Invalid character");
    }
    else if (peek(scanner) == '\n' || isAtEnd(scanner)) {
        return errorToken(scanner, "Unterminated character");
    }
    else if (peek(scanner) == '\\') {
        advance(scanner);
        advance(scanner);
    }
    else {
        advance(scanner);
    }

    if (!match(scanner, '\'')) return errorToken(scanner, "Unterminated character");

    return makeToken(scanner, TOKEN_CHAR);
}

static Token string(Scanner* scanner) {
    while (peek(scanner) != '"' && !isAtEnd(scanner)) {
        if (advance(scanner) == '\n') scanner->line++;
    }

    if (isAtEnd(scanner)) return errorToken(scanner, "Unterminated string");

    advance(scanner); // Eat the quote

    return makeToken(scanner, TOKEN_STRING);
}

static Token formattedString(Scanner* scanner) {
    while (peek(scanner) != '"' && !isAtEnd(scanner)) {
        char a = advance(scanner);
        if (a == '\n') {
            scanner->line++;
        }
        else if (a == '\\' && peek(scanner) == '\\') {
            advance(scanner);
        }
        else if (a == '\\' && peek(scanner) == '"') {
            advance(scanner);
        }
    }

    if (isAtEnd(scanner)) return errorToken(scanner, "Unterminated string");

    advance(scanner); // closing '"'

    return makeToken(scanner, TOKEN_FORMAT_STRING);
}

// Operator definition is '[:+\-*\\=^&|@<#$=!~?>]+' :: work this into code somehow (Maybe operators should be
// treated with maximal munch (I mean, they already are kinda??? Just gotta have it group any random set
// together lol))


// '[:+-*\=^&/|@<#$!~?>]+'
bool isGlyph(char c) {
    switch (c) {
        case '+': case '-': case '*':
        case '^': case '/': case '%':
        case ':': case '$': case '|':
        case '.': case '>': case '<':
        case '@': case '#': case '?':
        case '!': case '~': case '&':
        case '=': case '\\': return true;
        default: return false;
    }
}

static TokenType idOp(Scanner* scanner, const char* key, int start, int length, TokenType type) {
    if (scanner->current - scanner->start == start + length &&
        memcmp(scanner->start + start, key, length) == 0) return type;
    return TOKEN_CUSTOM;
}

// Operators that could be removed 100%
//  - `<< / Receive
//  - `|> / Pipline
// Operators that COULD go
//  - Most arithmetic operators (`+, `-, `*, `^, `/, `%)
//      - Would still require native fns to provide functionality, but
//      they are very rudimentary
//  - `$ / Apply
//      - The native fn apply() exists, so `$ can very easily be defined
//      in terms of that (could do that natively/automatically/whatever);
//      would the behaviour change somewhat? Yes, but it would be largely
//      identical in purpose and usefulness.
//  - `? / Interrogative
//      - Can be understood as two equality checks; would be worse, though.
static TokenType operatorType(Scanner* scanner) {
    switch (scanner->start[0]) {
        case '+': return idOp(scanner, "", 1, 0, TOKEN_PLUS);
        case '-': return idOp(scanner, "", 1, 0, TOKEN_MINUS);
        case '*': return idOp(scanner, "", 1, 0, TOKEN_STAR);
        case '^': return idOp(scanner, "", 1, 0, TOKEN_UCARET);
        case '/': return idOp(scanner, "", 1, 0, TOKEN_SLASH);
        case '%': return idOp(scanner, "", 1, 0, TOKEN_PERCENT);
        case ':': return idOp(scanner, "", 1, 0, TOKEN_COLON);
        case '$': return idOp(scanner, "", 1, 0, TOKEN_DOLLAR);
        case '?': return idOp(scanner, "", 1, 0, TOKEN_QUESTION);
        case '!': {
            switch (scanner->start[1]) {
            case '=': return idOp(scanner, "", 2, 0, TOKEN_BANG_EQUALS);
            default: return idOp(scanner, "", 1, 0, TOKEN_GREATER);
            }
        }
        case '|': {
            switch (scanner->start[1]) {
            case '>': return idOp(scanner, "", 2, 0, TOKEN_SPIGOT);
            default: return idOp(scanner, "", 1, 0, TOKEN_PIPE);
            }
        }
        case '.': {
            switch (scanner->start[1]) {
            case '.': return idOp(scanner, "", 2, 0, TOKEN_DOT_DOT);
            default: return idOp(scanner, "", 1, 0, TOKEN_DOT);
            }
        }
        case '>': {
            switch (scanner->start[1]) {
            case '=': return idOp(scanner, "", 2, 0, TOKEN_GREATER_EQUALS);
            default: return idOp(scanner, "", 1, 0, TOKEN_GREATER);
            }
        }
        case '<': {
            switch (scanner->start[1]) {
            case '<': return idOp(scanner, "", 2, 0, TOKEN_RECEIVE);
            case '=': return idOp(scanner, "", 2, 0, TOKEN_LESS_EQUALS);
            case '-': return idOp(scanner, "", 2, 0, TOKEN_RETURN);
            default: return idOp(scanner, "", 1, 0, TOKEN_LESS);
            }
        }
        case '=': {
            switch (scanner->start[1]) {
            case '=': return idOp(scanner, "", 2, 0, TOKEN_EQUALS_EQUALS);
            case '>': return idOp(scanner, "", 2, 0, TOKEN_ROCKET);
            default: return idOp(scanner, "", 1, 0, TOKEN_EQUALS);
            }
        }
        default: return TOKEN_CUSTOM;
    }
}

static Token scanOperator(Scanner* scanner) {
    while (isGlyph(peek(scanner))) advance(scanner);
    return makeToken(scanner, operatorType(scanner));
}

// Guard against 0-length operators
static Token literalOperator(Scanner* scanner) {
    while (isGlyph(peek(scanner))) advance(scanner);

    if (scanner->current == scanner->start + 1) {
        return errorToken(scanner, "Cannot have glyph with no characters");
    }

    return makeToken(scanner, TOKEN_GLYPH);
}

Token scanToken(Scanner* scanner) {
    endlineBreak(scanner);
    scanner->start = scanner->current;

    if (isAtEnd(scanner)) return makeToken(scanner, TOKEN_EOF);

    char c = advance(scanner);

    if (c == 'f' && match(scanner, '"')) return formattedString(scanner);

    if (isAlpha(c)) return scanText(scanner);
    if (isDigit(c)) return scanNums(scanner);
    if (isGlyph(c)) return scanOperator(scanner);

    if (c == '`') return literalOperator(scanner);

    switch (c) {
        // Purely Syntactic Symbols
        case '(': return makeToken(scanner, TOKEN_LEFT_PAREN);
        case ')': return makeToken(scanner, TOKEN_RIGHT_PAREN);
        case '[': return makeToken(scanner, TOKEN_LEFT_BRACKET);
        case ']': return makeToken(scanner, TOKEN_RIGHT_BRACKET);
        case '{': return makeToken(scanner, TOKEN_LEFT_BRACE);
        case '}': return makeToken(scanner, TOKEN_RIGHT_BRACE);
        case ',': return makeToken(scanner, TOKEN_COMMA);
        case ';': return makeToken(scanner, TOKEN_SEMICOLON);

        // Literals
        case '"': return string(scanner);
        case '\'': return character(scanner);

        default: break;
    }

    return errorToken(scanner, "Unrecognised token");
}

void debugScanner(const char* source) {
    Scanner scanner;
    scanner.start = source;
    scanner.current = source;
    scanner.line = 0;

    for (;;) {
        Token token = scanToken(&scanner);
        if (token.type == TOKEN_EOF) break;
    }
}
