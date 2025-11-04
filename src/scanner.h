#ifndef scanner_h_hammer
#define scanner_h_hammer

// Type :: num_of_first - num_of_last (inclusive)
typedef enum {
    // Symbols :: 0 - 6
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACKET, TOKEN_RIGHT_BRACKET,
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_SEMICOLON,

    // Operators :: 7 - 29
    TOKEN_DOT, TOKEN_DOT_DOT, TOKEN_COMMA,
    TOKEN_PLUS, TOKEN_MINUS,
    TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT,
    TOKEN_UCARET,

    TOKEN_EQUALS, TOKEN_RECEIVE,
    TOKEN_COLON, TOKEN_ROCKET,

    TOKEN_GREATER, TOKEN_LESS,
    TOKEN_GREATER_EQUALS, TOKEN_LESS_EQUALS,
    TOKEN_BANG_EQUALS, TOKEN_EQUALS_EQUALS,
    TOKEN_DOLLAR,

    TOKEN_QUESTION, TOKEN_BANG, TOKEN_PIPE, TOKEN_SPIGOT,

    TOKEN_CUSTOM,

    // Literals :: 30 - 40
    TOKEN_IDENTIFIER, TOKEN_INTEGER, TOKEN_FLOAT, TOKEN_STRING,
    TOKEN_FORMAT_STRING, TOKEN_CHAR, TOKEN_TRUE, TOKEN_FALSE, TOKEN_UNIT,
    TOKEN_WILDCARD, TOKEN_GLYPH,

    // Keywords :: 41 - 56
    TOKEN_IF, TOKEN_THEN, TOKEN_ELSE, TOKEN_MATCH,
    TOKEN_CONS, TOKEN_CAR, TOKEN_CDR,
    TOKEN_AND, TOKEN_OR, TOKEN_IN,
    TOKEN_RETURN,

    // Control :: 57 - 60
    TOKEN_BREAK, TOKEN_SOF, TOKEN_EOF, TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;

typedef struct {
    const char* trueBeginning;
    const char* start;
    const char* current;
    int line;
} Scanner;

void debugScanner(const char* source);

Token scanToken(Scanner* scanner);
void printToken(const Token* token);
void initScanner(Scanner* scanner, const char* source);

#endif
