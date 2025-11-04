#ifndef ast_h_hammer
#define ast_h_hammer

#include "common.h"
#include "scanner.h"


typedef struct Expr Expr;


typedef enum {
    EXPR_LITERAL,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_TERNARY,
    EXPR_BLOCK,
} ExprType;

struct Expr {
    ExprType type;
    Expr* next;
};

typedef struct {
    Expr expr;
    Token token;
} LiteralExpr;

typedef struct {
    Expr expr;
    Token token;
    Expr* operand;
} UnaryExpr;

typedef struct {
    Expr expr;
    Token token;
    Expr* left;
    Expr* right;
} BinaryExpr;

typedef struct {
    Expr expr;
    Token token;
    Expr* pivot;
    Expr* left;
    Expr* right;
} TernaryExpr;

typedef struct {
    Expr expr;
    Token token;
    Expr** subexprs;
    int count;
    int capacity;
} BlockExpr;


typedef struct {
    // Static
    Scanner scanner;
    int tokenCount;
    int tokenCapacity;
    bool panicMode;
    bool hadError;

    // Dynamic
    Token* current;      // the current (yet-to-be-consumed) token
    BlockExpr* program;  // the actual Abstract Syntax Tree
    Expr* expressions;   // linked list head for memory collection
    Token* tokens;       // the token stack
    Compiler* compiler;  // the associated compiler
} ProgramTree;

void printExpression(Expr* expression);
const char* getExprName(ExprType type);
void debugAST(const char* source);
void debugOptimisation(const char* source);
void createTree(Compiler* compiler, ProgramTree* tree, const char* source);
void initTree(ProgramTree* tree, const char* source);
void freeTree(ProgramTree* tree);


#endif
