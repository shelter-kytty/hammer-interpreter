#include <complex.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "memory.h"
#include "ast.h"
#include "compiler.h"
#include "scanner.h"
#include "vm.h"
#include "serialise.h"

/*
+=====================+
| Expressions   vvvv  |
+---------------------+
*/

// AST is not under the GC's jurisdiction, it comes and goes as it pleases
#define ALLOCATE_EXPR(tr, type, exprType) \
    (type*)allocateExpression(tr, sizeof(type), exprType)

#define FREE_EXPR(ptr, type) \
        exprAlloc(ptr, sizeof(type), 0)

#define GROW_EXPR_ARRAY(ptr, osize, nsize, type)  \
        (type*)exprAlloc(ptr, sizeof(type) * osize, sizeof(type) * nsize)

#define FREE_EXPR_ARRAY(ptr, osize, type) \
        exprAlloc(ptr, sizeof(type) * osize, 0)

static void* exprAlloc(void* ptr, size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        #ifdef DEBUG_LOG_MEMORY
        printf("Freeing %p: %04d -> 0\n", ptr, oldSize);
        #endif
        free(ptr);
        return NULL;
    }

    #ifdef DEBUG_LOG_MEMORY
    printf("Allocating %p: %04d -> %04d\n", ptr, oldSize, newSize);
    #endif

    void* result = realloc(ptr, newSize);
    if (result == NULL) exit(64);
    return result;
}

const char* getExprName(ExprType type) {
    switch (type) {
        case EXPR_BLOCK:
            return "EXPR_BLOCK";
        case EXPR_UNARY:
            return "EXPR_UNARY";
        case EXPR_BINARY:
            return "EXPR_BINARY";
        case EXPR_LITERAL:
            return "EXPR_LITERAL";
        case EXPR_TERNARY:
            return "EXPR_TERNARY";
        default:
            return "UNKNOWN EXPRESSION";
    }
}

static Expr* allocateExpression(ProgramTree* tree, size_t size, ExprType type) {
    Expr* expression = (Expr*)exprAlloc(NULL, 0, size);
    expression->type = type;

    expression->next = tree->expressions;
    tree->expressions = expression;

    #ifdef DEBUG_LOG_MEMORY
    printf("%p allocate %zu for %s\n", (void*)expression, size, getExprName(type));
    #endif

    return expression;
}

static void freeExpr(Expr* expr) {
    switch (expr->type) {
        case EXPR_LITERAL: {
            LiteralExpr* literal = (LiteralExpr*)expr;
            FREE_EXPR(literal, LiteralExpr);
            break;
        }
        case EXPR_UNARY: {
            UnaryExpr* unary = (UnaryExpr*)expr;
            FREE_EXPR(unary, UnaryExpr);
            break;
        }
        case EXPR_BINARY: {
            BinaryExpr* binary = (BinaryExpr*)expr;
            FREE_EXPR(binary, BinaryExpr);
            break;
        }
        case EXPR_TERNARY: {
            TernaryExpr* ternary = (TernaryExpr*)expr;
            FREE_EXPR(ternary, TernaryExpr);
            break;
        }
        case EXPR_BLOCK: {
            BlockExpr* block = (BlockExpr*)expr;
            FREE_EXPR_ARRAY(block->subexprs, block->capacity, Expr*);
            FREE_EXPR(block, BlockExpr);
            break;
        }
    }
}

static LiteralExpr* Literal(ProgramTree* tree, Token token) {
    LiteralExpr* literal = ALLOCATE_EXPR(tree, LiteralExpr, EXPR_LITERAL);
    literal->token = token;

    return literal;
}

static UnaryExpr* Unary(ProgramTree* tree, Token token) {
    UnaryExpr* unary = ALLOCATE_EXPR(tree, UnaryExpr, EXPR_UNARY);
    unary->operand = NULL;
    unary->token = token;

    return unary;
}

static BinaryExpr* Binary(ProgramTree* tree, Token token) {
    BinaryExpr* binary = ALLOCATE_EXPR(tree, BinaryExpr, EXPR_BINARY);
    binary->left = NULL;
    binary->right = NULL;
    binary->token = token;

    return binary;
}

static TernaryExpr* Ternary(ProgramTree* tree, Token token) {
    TernaryExpr* ternary = ALLOCATE_EXPR(tree, TernaryExpr, EXPR_TERNARY);
    ternary->left = NULL;
    ternary->right = NULL;
    ternary->pivot = NULL;
    ternary->token = token;

    return ternary;
}

static void writeExpr(BlockExpr* block, Expr* expr) {
    if (block->capacity < block->count + 1) {
        int oldCapacity = block->capacity;
        block->capacity = GROW_CAP(oldCapacity);
        block->subexprs = GROW_EXPR_ARRAY(block->subexprs,
            oldCapacity, block->capacity, Expr*);
    }

    block->subexprs[block->count] = expr;
    block->count++;
}

static BlockExpr* Block(ProgramTree* tree, Token token) {
    BlockExpr* block = ALLOCATE_EXPR(tree, BlockExpr, EXPR_BLOCK);
    block->token = token;
    block->subexprs = NULL;
    block->count = 0;
    block->capacity = 0;

    return block;
}

/*
+---------------------+
| Expressions   ^^^^  |
+=====================+
| Program Tree  vvvv  |
+---------------------+
*/

static void writeToken(ProgramTree* tree, Token token) {
    if (tree->tokenCapacity < tree->tokenCount + 1) {
        int oldCapacity = tree->tokenCapacity;
        tree->tokenCapacity = GROW_CAP(oldCapacity);
        tree->tokens = GROW_EXPR_ARRAY(tree->tokens,
            oldCapacity, tree->tokenCapacity, Token);
    }

    tree->tokens[tree->tokenCount] = token;
    tree->tokenCount++;
}

void initTree(ProgramTree* tree, Compiler* compiler, const char* source) {
    tree->tokenCount = 0;
    tree->tokenCapacity = 0;
    tree->tokens = NULL;
    tree->current = NULL;
    tree->expressions = NULL;
    tree->program = NULL;
    tree->hadError = false;
    tree->panicMode = false;
    tree->compiler = compiler;
    initScanner(&tree->scanner, source);
}

void freeTree(ProgramTree* tree) {
    FREE_EXPR_ARRAY(tree->tokens, tree->tokenCapacity, Token);

    Expr* expr = tree->expressions;
    while (expr != NULL) {
        Expr* next = expr->next;
        freeExpr(expr);
        expr = next;
    }

    initTree(tree, NULL, NULL);
}

/*
+---------------------+
| Program Tree  ^^^^  |
+=====================+
| Token Parsing vvvv  |
+---------------------+
*/

typedef Expr* (*ParseFn)(ProgramTree* tree, Expr* last);

typedef enum {
    PREC_NONE,          // {}
    PREC_ASSIGNMENT,    // = : <<
    PREC_GENERIC_LOW,   // custom
    PREC_CONSTRUCT,     // , .
    PREC_CONDITIONAL,   // if
    PREC_OR,            // or
    PREC_AND,           // and
    PREC_EQUALITY,      // == !=
    PREC_COMPARISON,    // < > <= >= in
    PREC_TERM,          // + -
    PREC_FACTOR,        // * / %
    PREC_EXPO,          // ^
    PREC_UNARY,         // ! -
    PREC_GENERIC_HIGH,  // |> custom
    PREC_CALL,          // () [] $
    PREC_PRIMARY,
} Precedence;

typedef struct {
    ParseFn head;
    ParseFn tail;
    Precedence prec;
} ParseRule;


void errorAt(ProgramTree* tree, Token* token, const char* message) {
    if (tree->panicMode) return;
    tree->panicMode = true;
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
    tree->hadError = true;
}

void errorAtPrev(ProgramTree* tree, const char* msg) {
    errorAt(tree, &tree->current[-1], msg);
}

void errorAtCrnt(ProgramTree* tree, const char* msg) {
    errorAt(tree, tree->current, msg);
}

simple Token getToken(ProgramTree* tree) {
    return *tree->current;
}

simple Token advance(ProgramTree* tree) {
    Token next = *tree->current++;

    if (next.type == TOKEN_ERROR) {
        errorAtPrev(tree, next.start);
    }

    return next;
}

simple bool check(ProgramTree* tree, TokenType expected) {
    return tree->current->type == expected;
}

static bool match(ProgramTree* tree, TokenType expected) {
    if (check(tree, expected)) {
        advance(tree);
        return true;
    }

    return false;
}

static void consume(ProgramTree* tree, TokenType expected, const char* msg) {
    if (check(tree, expected)) {
        advance(tree);
        return;
    }

    errorAtCrnt(tree, msg);
}

static void glare(ProgramTree* tree, TokenType expected, const char* msg) {
    if (check(tree, expected)) return;

    errorAtCrnt(tree, msg);
}

static void crossLine(ProgramTree* tree) {
    match(tree, TOKEN_SEMICOLON);
}

simple bool atEnd(ProgramTree* tree) {
    return tree->current->type == TOKEN_EOF;
}

static const ParseRule* getRule(TokenType type);

static void panic(ProgramTree* tree) {
    tree->panicMode = false;

    for (;;) {

        if ((getRule(tree->current->type))->head != NULL || tree->current->type == TOKEN_EOF) {
            return;
        }

        advance(tree);
    }
}

static bool nextIsTailExpr(ProgramTree* tree) {
    return getRule(getToken(tree).type)->tail != NULL;
}

static bool precIsLower(ProgramTree* tree, Precedence prec, Expr* last) {
    if (getToken(tree).type == TOKEN_COLON && last->type == EXPR_LITERAL && ((LiteralExpr*)last)->token.type == TOKEN_WILDCARD) {
        return prec <= PREC_CONSTRUCT;
    }

    return prec <= getRule(getToken(tree).type)->prec;
}

static Expr* expression(ProgramTree* tree, Precedence prec) {
    Token token = getToken(tree);

    // do a similar thing to the parsing function list in CLox
    // typedef a parsing function ptr and make a list of parse rules
    // to get some stuff working
    const ParseRule* rule = getRule(token.type);

    if (rule->head == NULL) {
        // Oh beans, that won't work at all!
        errorAtCrnt(tree, "Expected expression");
        return NULL;
    }

    Expr* last = rule->head(tree, NULL);

    while (nextIsTailExpr(tree) && precIsLower(tree, prec, last)) {
        #ifdef DEBUG_PARSER_PROGRESS
        printf("Parsed %s\n", getExprName(last->type));
        #endif
        rule = getRule(getToken(tree).type);
        last = rule->tail(tree, last);
    }


    #ifdef DEBUG_PARSER_PROGRESS
    printf("Parsed %s\n", getExprName(last->type));
    #endif

    return last;
}

static Expr* topLevel(ProgramTree* tree) {
    Expr* expr = expression(tree, PREC_NONE);

    match(tree, TOKEN_SEMICOLON);

    if (tree->panicMode) {
        panic(tree);
    }

    return expr;
}

static void mapArgs(ProgramTree* tree, BlockExpr* map) {
    while (!check(tree, TOKEN_RIGHT_BRACKET) && !atEnd(tree))
    {
        Expr* key = expression(tree, PREC_GENERIC_LOW);

        glare(tree, TOKEN_ROCKET, "Expected '=>' between map args");
        Token bisector = advance(tree);

        Expr* value = expression(tree, PREC_GENERIC_LOW);

        BinaryExpr* pair = Binary(tree, bisector);
        pair->left = key;
        pair->right = value;

        writeExpr(map, (Expr*)pair);

        crossLine(tree);
    }
}

static void listArgs(ProgramTree* tree, BlockExpr* list) {
    while (!check(tree, TOKEN_RIGHT_BRACKET) && !atEnd(tree)) {
        writeExpr(list, expression(tree, PREC_GENERIC_LOW));
        crossLine(tree);
    }
}

static void container(ProgramTree* tree, BlockExpr* container) {
    Expr* first = expression(tree, PREC_GENERIC_LOW);

    if (check(tree, TOKEN_ROCKET)) {
        Token bisector = advance(tree);
        Expr* value = expression(tree, PREC_GENERIC_LOW);

        BinaryExpr* pair = Binary(tree, bisector);
        pair->left = first;
        pair->right = value;
        writeExpr(container, (Expr*)pair);

        crossLine(tree);

        mapArgs(tree, container);
    }
    else {
        writeExpr(container, first);
        crossLine(tree);

        container->token.type = TOKEN_LEFT_PAREN;

        listArgs(tree, container);
    }
}

// Compile Maps without '=>' as integer-indexed lists
static Expr* map(ProgramTree* tree, Expr* last) {
    Token open = advance(tree);

    if (match(tree, TOKEN_RIGHT_BRACKET)) {
        BlockExpr* list = Block(tree, (Token){TOKEN_LEFT_PAREN, open.start, open.length, open.line});
        return (Expr*)list;
    }

    if (match(tree, TOKEN_ROCKET)) {
        BlockExpr* map = Block(tree, open);
        consume(tree, TOKEN_RIGHT_BRACKET, "Expected ']' after empty map macro");
        return (Expr*)map;
    }

    BlockExpr* array = Block(tree, open);
    container(tree, array);

    consume(tree, TOKEN_RIGHT_BRACKET, "Expected ']' after args");

    return (Expr*)array;
}

static Expr* subscript(ProgramTree* tree, Expr* last) {
    Token open = advance(tree);
    BinaryExpr* binary = Binary(tree, open);

    binary->left = last;

    if (check(tree, TOKEN_COLON)) {
        Token bisector = advance(tree);
        BinaryExpr* slice = Binary(tree, bisector);
        slice->left = slice->right = (Expr*)Literal(tree, (Token){TOKEN_UNIT, bisector.start, bisector.length, bisector.line});
        if(!check(tree, TOKEN_RIGHT_BRACKET)) {
            slice->right = expression(tree, PREC_GENERIC_LOW);
        }
        else {
            slice->right = (Expr*)Literal(tree, (Token){TOKEN_UNIT, bisector.start, bisector.length, bisector.line});
        }
        binary->right = (Expr*)slice;
    }
    else {
        binary->right = expression(tree, PREC_GENERIC_LOW); // Gonna do some stuff with subscripting

        if (check(tree, TOKEN_COLON)) {
            Token bisector = advance(tree);
            BinaryExpr* slice = Binary(tree, bisector);
            slice->left = binary->right;
            if(!check(tree, TOKEN_RIGHT_BRACKET)) {
                slice->right = expression(tree, PREC_GENERIC_LOW);
            }
            else {
                slice->right = (Expr*)Literal(tree, (Token){TOKEN_UNIT, bisector.start, bisector.length, bisector.line});
            }
            binary->right = (Expr*)slice;
        }
    }

    consume(tree, TOKEN_RIGHT_BRACKET, "Expected ']' after subscript");

    return (Expr*)binary;
}

static Expr* literal(ProgramTree* tree, Expr* last) {
    return (Expr*)Literal(tree, advance(tree));
}

static Expr* unary(ProgramTree* tree, Expr* last) {
    Token operator = advance(tree);
    UnaryExpr* un = Unary(tree, operator);
    un->operand = expression(tree, (Precedence)((getRule(operator.type))->prec + 1));
    return (Expr*)un;
}

static Expr* postUnary(ProgramTree* tree, Expr* last) {
    Token operator = advance(tree);
    UnaryExpr* un = Unary(tree, operator);
    un->operand = last;
    return (Expr*)un;
}

static Expr* negation(ProgramTree* tree, Expr* last) {
    Token operator = advance(tree);
    UnaryExpr* un = Unary(tree, operator);
    un->operand = expression(tree, PREC_UNARY + 1);
    return (Expr*)un;
}

static Expr* binary(ProgramTree* tree, Expr* last) {
    Token operator = advance(tree);
    BinaryExpr* bin = Binary(tree, operator);

    // + 1 makes operator left associative; 1 • 2 • 3 == ( ( 1 • 2 ) • 3 )
    Expr* next = expression(tree, (Precedence)((getRule(operator.type))->prec + 1));

    bin->left = last;
    bin->right = next;

    return (Expr*)bin;
}

static Expr* rBinary(ProgramTree* tree, Expr* last) {
    Token operator = advance(tree);
    BinaryExpr* bin = Binary(tree, operator);

    // lack of + 1 makes operator right associative; 1 • 2 • 3 == ( 1 • ( 2 • 3 ) )
    Expr* next = expression(tree, (Precedence)((getRule(operator.type))->prec));

    bin->left = last;
    bin->right = next;

    return (Expr*)bin;
}

static Expr* preBinary(ProgramTree* tree, Expr* last) {
    Token operator = advance(tree);
    BinaryExpr* bin = Binary(tree, operator);

    bin->left = expression(tree, PREC_GENERIC_LOW);

    bin->right = expression(tree, PREC_GENERIC_LOW);

    return (Expr*)bin;
}

static Expr* _if(ProgramTree* tree, Expr* last) {
    Token operator = advance(tree);
    TernaryExpr* ifExpr = Ternary(tree, operator);

    ifExpr->pivot = expression(tree, PREC_GENERIC_LOW);

    consume(tree, TOKEN_THEN, "Expected then branch");
    ifExpr->left = expression(tree, PREC_GENERIC_LOW);

    consume(tree, TOKEN_ELSE, "Expected else branch");
    ifExpr->right = expression(tree, PREC_GENERIC_LOW);

    return (Expr*)ifExpr;
}

static Expr* block(ProgramTree* tree, Expr* last) {
    Token open = advance(tree);

    crossLine(tree);

    if (match(tree, TOKEN_RIGHT_BRACE)) {
        LiteralExpr* unit = Literal(tree, (Token){TOKEN_UNIT, open.start, open.length, open.line});
        return (Expr*)unit;
    }

    BlockExpr* blck = Block(tree, open);

    while (!check(tree, TOKEN_RIGHT_BRACE) && !check(tree, TOKEN_EOF)) {
        writeExpr(blck, topLevel(tree));
    }

    consume(tree, TOKEN_RIGHT_BRACE, "Expected closing '}' after block");

    return (Expr*)blck;
}

static Expr* _match(ProgramTree* tree, Expr* last) {
    Token operator = advance(tree);
    BinaryExpr* _switch = Binary(tree, operator);

    _switch->left = expression(tree, PREC_GENERIC_LOW);

    BlockExpr* cases = Block(tree, operator);

    crossLine(tree);

    while (match(tree, TOKEN_PIPE)) {
        Expr* l = expression(tree, PREC_GENERIC_LOW);

        glare(tree, TOKEN_ROCKET, "Expected '=>' between case and operation");
        Token delimiter = advance(tree);
        BinaryExpr* _case = Binary(tree, delimiter);

        Expr* r = expression(tree, PREC_GENERIC_LOW);
        crossLine(tree);

        _case->left = l;
        _case->right = r;
        writeExpr(cases, (Expr*)_case);
    }

    _switch->right = (Expr*)cases;

    return (Expr*)_switch;
}

static Expr* function(ProgramTree* tree, Expr* last) {
    Token operator = advance(tree);
    TernaryExpr* fn = Ternary(tree, operator);

    BlockExpr* operands = Block(tree, operator);
    while (!check(tree, TOKEN_EQUALS) && !atEnd(tree)) {
        glare(tree, TOKEN_IDENTIFIER, "Expected identifier in fn declaration");
        writeExpr(operands, literal(tree, NULL));
    }

    consume(tree, TOKEN_EQUALS, "Expected '=' after function operands");

    Expr* body = expression(tree, PREC_ASSIGNMENT);

    fn->left = last;
    fn->pivot = (Expr*)operands;
    fn->right = body;

    return (Expr*)fn;
}

static ObjString* genID(ProgramTree* tree, int i) {
    char hex_lookup[16] = {
        '0', '1', '2', '3',
        '4', '5', '6', '7',
        '8', '9', 'A', 'B',
        'C', 'D', 'E', 'F'
    };

    int id_len = 3;
    char* id_base = ALLOCATE(tree->compiler->vm, id_len + 1, char);

    id_base[id_len] = '\0';
    id_base[0] = '0';
    id_base[1] = hex_lookup[i / 16];
    id_base[2] = hex_lookup[i % 16];

    // No need to manually free afterwards ; takeString frees or uses it
    ObjString* str = takeString(tree->compiler->vm, id_base, id_len);

    return str;
}

static Expr* partialApply(ProgramTree* tree, BinaryExpr* application, BlockExpr* args, Token operator, int partial) {
    TernaryExpr* lmbd = Ternary(tree, (Token){TOKEN_COLON, NULL, 1, operator.line});

    lmbd->left = (Expr*)Literal(tree, (Token){TOKEN_WILDCARD, NULL, 1, operator.line});

    BlockExpr* lmbd_params = Block(tree, (Token){TOKEN_COLON, NULL, 1, operator.line});

    for (int i = 0; i < partial; i++) {
        ObjString* str = genID(tree, i);
        writeExpr(lmbd_params, (Expr*)Literal(tree, (Token){TOKEN_IDENTIFIER, str->chars, 3, operator.line}));
    }

    lmbd->pivot = (Expr*)lmbd_params;

    // for tail-calling
    UnaryExpr* ret = Unary(tree, (Token){TOKEN_RETURN, "<-", 2, operator.line});
    ret->operand = (Expr*)application;
    lmbd->right = (Expr*)ret;

    // replace '_' args with generated ids
    int j = 0;
    for (int i = 0; i < args->count; i++) {
        Expr* param = args->subexprs[i];
        if (param->type == EXPR_LITERAL && ((LiteralExpr*)param)->token.type == TOKEN_WILDCARD) {
            args->subexprs[i] = lmbd_params->subexprs[j++];
        }
    }

    return (Expr*)lmbd;
}

static Expr* apply(ProgramTree* tree, Expr* last) {
    Token operator = advance(tree);
    BinaryExpr* application = Binary(tree, operator);

    BlockExpr* args = Block(tree, operator);

    int partial = 0;

    if (operator.type == TOKEN_LEFT_PAREN) {
        crossLine(tree);
        while (!check(tree, TOKEN_RIGHT_PAREN) && !atEnd(tree)) {
            Expr* param = expression(tree, PREC_GENERIC_LOW);

            if (param->type == EXPR_LITERAL && ((LiteralExpr*)param)->token.type == TOKEN_WILDCARD) {
                partial++;
            }

            writeExpr(args, param);
            crossLine(tree);
        }
        consume(tree, TOKEN_RIGHT_PAREN, "Expected ')' after params");
    } else {
        int _line_n = operator.line;
        while (getRule(getToken(tree).type)->head != NULL && getToken(tree).line == _line_n) {
            Expr* param = expression(tree, PREC_GENERIC_LOW);

            if (param->type == EXPR_LITERAL && ((LiteralExpr*)param)->token.type == TOKEN_WILDCARD) {
                partial++;
            }

            writeExpr(args, param);
        }
    }

    application->left = last;
    application->right = (Expr*)args;

    if (partial > 0) {
        return partialApply(tree, application, args, operator, partial);
    }

    return (Expr*)application;
}

static const ParseRule rules[] = {
    // Symbols :: 0 - 4     := Mostly for parsing stuff
    [TOKEN_LEFT_PAREN]          = { NULL,       apply,      PREC_CALL },
    [TOKEN_RIGHT_PAREN]         = { NULL,       NULL,       PREC_NONE },
    [TOKEN_LEFT_BRACKET]        = { map,        subscript,  PREC_CALL },
    [TOKEN_RIGHT_BRACKET]       = { NULL,       NULL,       PREC_NONE },
    [TOKEN_LEFT_BRACE]          = { block,      NULL,       PREC_NONE },
    [TOKEN_RIGHT_BRACE]         = { NULL,       NULL,       PREC_NONE },
    [TOKEN_SEMICOLON]           = { NULL,       NULL,       PREC_NONE },

    // Operators :: 5 - 22  := Mostly infix ops, - and ! are prefix ops
    [TOKEN_DOT]                 = { NULL,       rBinary,    PREC_CONSTRUCT },
    [TOKEN_DOT_DOT]             = { NULL,       binary,     PREC_CONSTRUCT }, // should be PREC_CONSTRUCT????
    [TOKEN_COMMA]               = { NULL,       rBinary,    PREC_CONSTRUCT },
    [TOKEN_PLUS]                = { NULL,       binary,     PREC_TERM },
    [TOKEN_MINUS]               = { negation,   binary,     PREC_TERM },
    [TOKEN_STAR]                = { NULL,       binary,     PREC_FACTOR },
    [TOKEN_SLASH]               = { NULL,       binary,     PREC_FACTOR },
    [TOKEN_PERCENT]             = { NULL,       binary,     PREC_FACTOR },
    [TOKEN_UCARET]              = { NULL,       binary,     PREC_EXPO },

    [TOKEN_COLON]               = { NULL,       function,   PREC_ASSIGNMENT },
    [TOKEN_EQUALS]              = { NULL,       rBinary,    PREC_ASSIGNMENT },
    [TOKEN_RECEIVE]             = { NULL,       rBinary,    PREC_ASSIGNMENT},
    [TOKEN_ROCKET]              = { NULL,       NULL,       PREC_NONE },

    [TOKEN_GREATER]             = { NULL,       binary,     PREC_COMPARISON },
    [TOKEN_LESS]                = { NULL,       binary,     PREC_COMPARISON },
    [TOKEN_GREATER_EQUALS]      = { NULL,       binary,     PREC_COMPARISON },
    [TOKEN_LESS_EQUALS]         = { NULL,       binary,     PREC_COMPARISON },
    [TOKEN_BANG_EQUALS]         = { NULL,       binary,     PREC_EQUALITY },
    [TOKEN_EQUALS_EQUALS]       = { NULL,       binary,     PREC_EQUALITY },

    [TOKEN_DOLLAR]              = { NULL,       apply,      PREC_CALL },

    [TOKEN_QUESTION]            = { NULL,       postUnary,  PREC_UNARY },
    [TOKEN_BANG]                = { unary,      NULL,       PREC_UNARY },
    [TOKEN_PIPE]                = { NULL,       NULL,       PREC_NONE },

    [TOKEN_SPIGOT]              = { NULL,       binary,     PREC_GENERIC_HIGH },

    // Make special head+tail functions that analyse the operator to figure out the
    // precedence and associativity and don't throw errors for missing operands and such
    [TOKEN_CUSTOM]              = { NULL,       binary,     PREC_GENERIC_LOW },

    // Literals :: 23 - 32  := Capture literal tokens, don't take any params
    [TOKEN_IDENTIFIER]          = { literal,    NULL,       PREC_NONE }, // Literals found as infix expressions could be considered "virtual" apply ops
    [TOKEN_INTEGER]             = { literal,    NULL,       PREC_NONE },
    [TOKEN_FLOAT]               = { literal,    NULL,       PREC_NONE },
    [TOKEN_STRING]              = { literal,    NULL,       PREC_NONE },
    [TOKEN_FORMAT_STRING]       = { literal,    NULL,       PREC_NONE },
    [TOKEN_CHAR]                = { literal,    NULL,       PREC_NONE },
    [TOKEN_TRUE]                = { literal,    NULL,       PREC_NONE },
    [TOKEN_FALSE]               = { literal,    NULL,       PREC_NONE },
    [TOKEN_UNIT]                = { literal,    NULL,       PREC_NONE },
    [TOKEN_WILDCARD]            = { literal,    NULL,       PREC_NONE },
    [TOKEN_GLYPH]               = { literal,    NULL,       PREC_NONE },

    // Keywords :: 33 - 47  := Some are infix operators, but most will be prefix or mixfix
    [TOKEN_IF]                  = { _if,        NULL,       PREC_CONDITIONAL },
    [TOKEN_THEN]                = { NULL,       NULL,       PREC_NONE },
    [TOKEN_ELSE]                = { NULL,       NULL,       PREC_NONE },
    [TOKEN_MATCH]               = { _match,     NULL,       PREC_NONE },
    [TOKEN_CONS]                = { preBinary,  NULL,       PREC_NONE },
    [TOKEN_CAR]                 = { unary,      NULL,       PREC_NONE },
    [TOKEN_CDR]                 = { unary,      NULL,       PREC_NONE },
    [TOKEN_AND]                 = { NULL,       binary,     PREC_AND },
    [TOKEN_OR]                  = { NULL,       binary,     PREC_OR },
    [TOKEN_IN]                  = { NULL,       binary,     PREC_COMPARISON },
    [TOKEN_RETURN]              = { unary,      NULL,       PREC_NONE },

    // Control :: 48 - 51   := Nothing, should not be parsed
    [TOKEN_SOF]                 = { NULL,       NULL,       PREC_NONE },
    [TOKEN_EOF]                 = { NULL,       NULL,       PREC_NONE },
    [TOKEN_ERROR]               = { NULL,       NULL,       PREC_NONE },
};

static const ParseRule* getRule(TokenType type) {
    return &rules[type];
}

void printExpression(Expr* expression) {
    if (expression == NULL) {
        printf("[ Error ] expression is none. Segfaulting\n");
        return;
    }

    switch(expression->type) {
        case EXPR_LITERAL:  {
            LiteralExpr* literal = (LiteralExpr*)expression;
            printToken(&literal->token);
            break;
        }

        case EXPR_UNARY:    {
            UnaryExpr* unary = (UnaryExpr*)expression;
            printToken(&unary->token);
            printf("O: ");
            printExpression(unary->operand);
            break;
        }

        case EXPR_BINARY:   {
            BinaryExpr* binary = (BinaryExpr*)expression;
            printToken(&binary->token);
            printf("L: ");
            printExpression(binary->left);
            printf("R: ");
            printExpression(binary->right);
            break;
        }

        case EXPR_TERNARY:  {
            TernaryExpr* ternary = (TernaryExpr*)expression;
            printToken(&ternary->token);
            printf("L: ");
            printExpression(ternary->left);
            printf("P: ");
            printExpression(ternary->pivot);
            printf("R: ");
            printExpression(ternary->right);
            break;
        }

        case EXPR_BLOCK:    {
            BlockExpr* block = (BlockExpr*)expression;
            printToken(&block->token);
            TokenType type = block->token.type;
            for (size_t i = 0; i < block->count; ++i) {
                if (type == TOKEN_SOF) {
                    printf("Expr  %04lu: ", i);
                } else if (type == TOKEN_LEFT_BRACE) {
                    printf("SExpr %04lu: ", i);
                } else {
                    printf("Arg   %04lu: ", i);
                }
                printExpression(block->subexprs[i]);
            }
            break;
        }
    }
}

void debugAST(const char* source) {
    // vvvv Setup Program Tree
    VM vm;
    initVM(&vm);

    Compiler compiler;
    initCompiler(&compiler, &vm, 0, NULL);

    ProgramTree tree;
    initTree(&tree, &compiler, source);

    writeToken(&tree, (Token){ TOKEN_SOF, source, 0, 0 });
    tree.program = Block(&tree, *tree.tokens);

    for (;;) {
        Token token = scanToken(&tree.scanner);

        writeToken(&tree, token);

        if (token.type == TOKEN_EOF) break;
    }

    tree.current = tree.tokens;
    // ^^^^ Setup Program Tree

    #ifdef DEBUG_DISPLAY_TOKENS
    for (int i = 0; i < tree.tokenCount; ++i) {
        printf("%04d : ", i);
        printToken(&tree.tokens[i]);
    }
    #endif

    advance(&tree);

    match(&tree, TOKEN_SEMICOLON);

    while (!check(&tree, TOKEN_EOF)) {
        writeExpr(tree.program, topLevel(&tree));
    }

    UnaryExpr* end = Unary(&tree, *tree.current);
    end->operand = (Expr*)Literal(&tree, (Token){TOKEN_UNIT, tree.current->start, 0, tree.current->line});
    writeExpr(tree.program, (Expr*)end);

    printExpression((Expr*)tree.program);

    freeTree(&tree);

    //endCompiler(&compiler);

    freeVM(&vm);
}

void createTree(Compiler* compiler, ProgramTree* tree, const char* source) {
    // vvvv Setup Program Tree
    initTree(tree, compiler, source);

    writeToken(tree, (Token){ TOKEN_SOF, source, 0, 0 });
    tree->program = Block(tree, *tree->tokens);

    for (;;) {
        Token token = scanToken(&tree->scanner);

        writeToken(tree, token);

        if (token.type == TOKEN_EOF) break;
    }

    tree->current = tree->tokens;
    // ^^^^ Setup Program Tree

    advance(tree);

    match(tree, TOKEN_SEMICOLON);

    while (!check(tree, TOKEN_EOF)) {
        writeExpr(tree->program, topLevel(tree));
    }

    writeExpr(tree->program, (Expr*)Literal(tree, (Token){TOKEN_UNIT, tree->current->start, 0, tree->current->line}));


    #ifdef DEBUG_DISPLAY_AST
    printf("AST:\n");
    printExpression((Expr*)tree->program);
    #endif
}


/*
+---------------------------+
| Token Parsing        ^^^^ |
+---------------------------+
| Serialising          vvvv |
+---------------------------+
*/

void serialiseAST(FILE* file, const char* source) {
    // SETUP ------------------------------- vvvv
    VM vm;
    initVM(&vm);

    Compiler compiler;
    initCompiler(&compiler, &vm, 0, NULL);

    ProgramTree tree;
    initTree(&tree, &compiler, source);

    writeToken(&tree, (Token){ TOKEN_SOF, source, 0, 0 });
    tree.program = Block(&tree, *tree.tokens);

    for (;;) {
        Token token = scanToken(&tree.scanner);

        writeToken(&tree, token);

        if (token.type == TOKEN_EOF) break;
    }

    tree.current = tree.tokens;

    #ifdef DEBUG_DISPLAY_TOKENS
    for (int i = 0; i < tree.tokenCount; ++i) {
        printf("%04d : ", i);
        printToken(&tree.tokens[i]);
    }
    #endif
    // SETUP ------------------------------- ^^^^


    advance(&tree);
    match(&tree, TOKEN_SEMICOLON);

    while (!check(&tree, TOKEN_EOF)) {
        writeExpr(tree.program, topLevel(&tree));
    }

    writeExpr(tree.program, (Expr*)Literal(&tree, (Token){TOKEN_UNIT, tree.current->start, 0, tree.current->line}));

    serialiseExpr(file, (Expr*)tree.program);

    freeTree(&tree);

    //endCompiler(&compiler);

    freeVM(&vm);
}

size_t countUntil(const char **src, char end) {
    for (size_t i = 0; *src[i] != '\0'; i++) {
        if (*src[i] == end) return i;
    }

    return 0;
}

TokenType retType(TokenType type, const char **start, size_t i) {
    (*start) += i;
    return type;
}

TokenType tTypeFromName(const char **start) {
   switch (*start[0]) {
       //alphabetise and break down into tries
        case 'A': return retType(TOKEN_AND, start, 3);
        case 'B': {
            if (*start[4] == '_')
                return TOKEN_BANG_EQUALS;
            return TOKEN_BANG;
        };
        case 'C': {
            switch (*start[1]) {
                case 'A': return retType(TOKEN_CAR, start, 3);
                case 'D': return retType(TOKEN_CDR, start, 3);
                case 'H': return retType(TOKEN_CHAR, start, 4);
                case 'O': {
                    switch(*start[2]) {
                        case 'N': return retType(TOKEN_CONS, start, 4);
                        case 'M': return retType(TOKEN_COMMA, start, 5);
                        case 'L': return retType(TOKEN_COLON, start, 5);
                    }
                };
                case 'U': return retType(TOKEN_CUSTOM, start, 6);
            }
        };
        case 'D': {
            switch (*start[2]) {
                case 'T': {
                    if (*start[4] == '_')
                        return retType(TOKEN_DOT_DOT, start, 7);

                    return retType(TOKEN_DOT, start, 3);
                };
                case 'L': return retType(TOKEN_DOLLAR, start, 6);
            }
        };
        case 'E': {
            switch (*start[1]) {
                case 'O': return retType(TOKEN_EOF, start, 3);
                case 'R': return retType(TOKEN_ERROR, start, 5);
                case 'Q': {
                    if (*start[6] == '_')
                        return retType(TOKEN_EQUALS_EQUALS, start, 13);

                    return retType(TOKEN_EQUALS, start, 6);
                };
                case 'L': return retType(TOKEN_ELSE, start, 4);
            }
        };
        case 'F': {
            switch (*start[1]) {
                case 'A': return retType(TOKEN_FALSE, start, 5);
                case 'L': return retType(TOKEN_FLOAT, start, 5);
                case 'O': return retType(TOKEN_FORMAT_STRING, start, 13);
            }
        };
        case 'G': {
            switch (*start[1]) {
                case 'L': return retType(TOKEN_GLYPH, start, 5);
                case 'R': {
                    if (*start[7] == '_')
                        return retType(TOKEN_GREATER_EQUALS, start, 14);
                    return retType(TOKEN_GREATER, start, 7);
                };
            }
        };
        case 'I': {
            switch (*start[1]) {
                case 'D': return retType(TOKEN_IDENTIFIER, start, 10);
                case 'F': return retType(TOKEN_IF, start, 2);
                case 'N': {
                    if (*start[3] == 'T')
                        return retType(TOKEN_INTEGER, start, 7);
                    return retType(TOKEN_IN, start, 2);
                };
            }
        };
        case 'L': {
            switch (*start[1]) {
                case 'F': {
                    switch (*start[9]) {
                        case 'N': return retType(TOKEN_LEFT_PAREN, start, 10);
                        case 'K': return retType(TOKEN_LEFT_BRACKET, start, 12);
                        case 'E': return retType(TOKEN_LEFT_BRACE, start, 10);
                    }
                };
                case 'S': {
                    if (*start[4] == '_')
                        return retType(TOKEN_LESS_EQUALS, start, 11);
                    return retType(TOKEN_LESS, start, 4);
                };
            }
        };
        case 'M': {
            switch (*start[1]) {
                case 'A': return retType(TOKEN_MATCH, start, 5);
                case 'I': return retType(TOKEN_MINUS, start, 5);
            }
        };
        case 'O': return retType(TOKEN_OR, start, 2);
        case 'P': {
            switch (*start[1]) {
                case 'I': return retType(TOKEN_PIPE, start, 4);
                case 'L': return retType(TOKEN_PLUS, start, 4);
                case 'E': return retType(TOKEN_PERCENT, start, 7);
            }
        };
        case 'Q': return retType(TOKEN_QUESTION, start, 8);
        case 'R': {
            switch (*start[1]) {
                case 'E': {
                    if (*start[2] == 'C')
                        return retType(TOKEN_RECEIVE, start, 7);

                    return retType(TOKEN_RETURN, start, 6);
                };
                case 'I': {
                    switch (*start[10]) {
                        case 'N': return retType(TOKEN_RIGHT_PAREN, start, 11);
                        case 'K': return retType(TOKEN_RIGHT_BRACKET, start, 13);
                        case 'E': return retType(TOKEN_RIGHT_BRACE, start, 11);
                    }
                };
                case 'O': return retType(TOKEN_ROCKET, start, 6);
            }
        };
        case 'S': {
            switch (*start[1]) {
                case 'E': return retType(TOKEN_SEMICOLON, start, 9);
                case 'L': return retType(TOKEN_SLASH, start, 5);
                case 'O': return retType(TOKEN_SOF, start, 3);
                case 'P': return retType(TOKEN_SPIGOT, start, 6);
                case 'T': {
                    switch (*start[2]) {
                        case 'A': return retType(TOKEN_STAR, start, 4);
                        case 'R': return retType(TOKEN_STRING, start, 6);
                    }
                };
            }
        };
        case 'T': {
            switch (*start[1]) {
                case 'R': return retType(TOKEN_TRUE, start, 4);
                case 'H': return retType(TOKEN_THEN, start, 4);
            }
        };
        case 'U': {
            switch (*start[1]) {
                case 'N': return retType(TOKEN_UNIT, start, 4);
                case 'C': return retType(TOKEN_UCARET, start, 6);
            }
        };
        case 'W': return retType(TOKEN_WILDCARD, start, 8);
        default: return TOKEN_ERROR;
    }
}

void consumeWhitespace(const char **source) {
    for (;;) {
        switch (**source) {
            case ' ':
            case '\n':
            case '\t':
            case '\r':
                (*source)++;
                break;
            default: return;
        }
    }
}

void serialiseToken(FILE* file, Token* token) {
    fprintf(file, "\"token\": { ");
    if (token->start == NULL) {
        fprintf(file, "\"content\": \"null\"");
    } else if (token->type == TOKEN_STRING || token->type == TOKEN_FORMAT_STRING) {
        fprintf(file, "\"content\": %.*s", token->length, token->start);
    } else {
        fprintf(file, "\"content\": \"%.*s\"", token->length, token->start);
    }

    fprintf(file, ", \"type\": \"%s\"", tokenName(token->type));

    fprintf(file, ", \"line\": %d", token->line);

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

// Deserialise expr type and call corresponding function to deserialise the rest
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

void deserialiseJSON(ProgramTree *tree, Compiler* compiler, const char *source) {
    initTree(tree, compiler, source);

}

/*
+---------------------------+
| Serialising          ^^^^ |
+---------------------------+
*/
