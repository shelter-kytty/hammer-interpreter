#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

#include "compiler.h"

#include "common.h"
#include "vm.h"
#include "value.h"
#include "object.h"
#include "debug.h"
#include "memory.h"


static void compilerError(Compiler* compiler, const char* format, ...);


simple Chunk* currentChunk(Compiler* compiler) {
    return &compiler->function->body;
}

static Token getToken(Compiler* compiler, Expr* expression) {
    if (expression == NULL) {
        compilerError(compiler, "Token was NULL");
        return (Token){ TOKEN_ERROR, NULL, 0, 0 };
    }

    switch (expression->type) {
        case EXPR_LITERAL:  {
            LiteralExpr* literal = (LiteralExpr*)expression;
            return literal->token;
        }
        case EXPR_UNARY:    {
            UnaryExpr* unary = (UnaryExpr*)expression;
            return unary->token;
        }
        case EXPR_BINARY:   {
            BinaryExpr* binary = (BinaryExpr*)expression;
            return binary->token;
        }
        case EXPR_TERNARY:  {
            TernaryExpr* ternary = (TernaryExpr*)expression;
            return ternary->token;
        }
        case EXPR_BLOCK:    {
            BlockExpr* block = (BlockExpr*)expression;
            return block->token;
        }
        default: {
            compilerError(compiler, "Did not recognise expression type");
            return (Token){ TOKEN_ERROR, NULL, 0, 0 };
        }
    }
}

static bool isTType(Compiler* compiler, Expr* expression, TokenType type) {
    return getToken(compiler, expression).type == type;
}

static const char* getName(Compiler* compiler) {
    switch (compiler->type) {
    case FUN_SCRIPT: return "<script>";
    case FUN_LAMBDA: return "<lmbd>";
    case FUN_FUNCTION: return compiler->function->name->chars;
    default: return "UNKNOWN";
    }
}

static int getLastLine(Compiler* compiler) {
    if (compiler->function->body.count == 0) {
        return 0;
    }

    return compiler->function->body.lines[compiler->function->body.count - 1];
}

static void compilerError(Compiler* compiler, const char* format, ...) {
    Compiler* traced = compiler;

    while (traced != NULL) {
        fprintf(stderr, "[ line %d ] in %s\n", getLastLine(traced), getName(traced));
        traced = traced->enclosing;
    }

    fprintf(stderr, "Error: ");

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    compiler->tree->hadError = true;
}

static void emitByte(Compiler* compiler, uint8_t byte, int line) {
    writeChunk(compiler->vm, currentChunk(compiler), byte, line);
}

static void emitBytes(Compiler* compiler, uint8_t byte1, uint8_t byte2, int line) {
    emitByte(compiler, byte1, line);
    emitByte(compiler, byte2, line);
}

static void emitShort(Compiler* compiler, uint8_t op, uint16_t params, int line) {
    emitByte(compiler, op, line);
    emitBytes(compiler, (uint8_t)((params & 0xFF00) >> 8), (uint8_t)(params & 0x00FF), line);
}

static uint8_t makeConstant(Compiler* compiler, Value value) {
    int constant = addConstant(compiler->vm, currentChunk(compiler), value);

    if (constant > UINT8_MAX) {
        compilerError(compiler, "Too many constants in %s; limit is %d, had %d", getName(compiler), UINT8_MAX, constant);
        return 0;
    }

    return (uint8_t)constant;
}

static void emitConstant(Compiler* compiler, Value value, int line) {
    emitBytes(compiler, OP_LOADV, makeConstant(compiler, value), line);
}

static int emitJump(Compiler* compiler, OpCode op, int line) {
    int spot = currentChunk(compiler)->count;
    emitByte(compiler, (uint8_t)op, line);
    emitBytes(compiler, 0, 0, line);
    return spot;
}

static int patchJump(Compiler* compiler, int original) {
    uint16_t distance = currentChunk(compiler)->count - original - 3;   //compensate for bytes read by run()

    if (distance > UINT16_MAX) {
        compilerError(compiler, "Jump covers too many ops; limit is %d, had %d", UINT16_MAX, distance);
        return 0;
    }

    currentChunk(compiler)->code[original + 1] = (uint8_t)((distance & 0xFF00) >> 8);
    currentChunk(compiler)->code[original + 2] = (uint8_t)(distance & 0x00FF);
    return distance;
}

simple int beginScope(Compiler* compiler) {
    return ++compiler->scopeDepth;
}

simple void endScope(Compiler* compiler, int endline) {
    compiler->scopeDepth--;
    int i = 0;

    while (compiler->localCount > 0 && compiler->locals[compiler->localCount - 1].depth > compiler->scopeDepth) {
        i++;
        compiler->localCount--;
    }

    if (i > 0) {
        emitBytes(compiler, OP_RETURN_SCOPE, (uint8_t)i, endline);
    }
}

static void addLocal(Compiler* compiler, ObjString* name) {
    if (compiler->localCount > UINT8_MAX) {
        compilerError(compiler, "Too many bindings in scope; limit is %d, had %d", UINT8_MAX, compiler->localCount);
        return;
    }

    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (local->depth != -1 && local->depth < compiler->scopeDepth) {
            break;
        }

        if (name == local->name) {
            compilerError(compiler, "'%s' is already bound to this scope", name->chars);
            return;
        }
    }

    Local* local = &compiler->locals[compiler->localCount++];
    local->name = name;
    local->depth = -1;
    local->isCaptured = false;
}

static void fixLocal(Compiler* compiler, ObjString* name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (local->depth == -1 && local->name == name) {
            local->depth = compiler->scopeDepth;
            return;
        }

        if (name == local->name) {
            compilerError(compiler, "'%s' is already bound to this scope", name->chars);
            return;
        }
    }
}

static int resolveLocal(Compiler* compiler, Token token) {
    ObjString* name = copyString(compiler->vm, token.start, token.length);

    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (name == local->name) {
            if (local->depth == -1) {
                compilerError(compiler, "Local '%s' used-in-assignment", name->chars);
                return -1;
            }

            return i;
        }

    }

    return -1;
}

static int addUpvalue(Compiler* compiler, uint8_t index, bool isLocal) {
    int upvalueCount = compiler->upvalueCount;

    // Check if the upvalue has already been tagged (indeces and isLocal are equal)
    // If so return its position
    for (int i = 0; i < upvalueCount; i++) {
        Upvalue upvalue = compiler->upvalues[i];
        if (upvalue.index == index && upvalue.isLocal == isLocal) {
            #ifdef DEBUG_UPVALUE_INFO
            printf("Found upvalue at index %d, islocal = %d\n", upvalue.index, upvalue.isLocal);
            #endif
            return i;
        }
    }

    // exit if too many upvalues
    if (upvalueCount == UINT8_COUNT) {
        compilerError(compiler, "Too many upvalues in function; limit is %d, had %d", UINT8_COUNT, upvalueCount);
        return -1;
    }

    #ifdef DEBUG_UPVALUE_INFO
    printf("New upvalue at index %d, islocal = %d\n", index, isLocal);
    #endif

    // Add upvalue if it doesnt already exist
    // Return the new upvalue position
    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    return compiler->upvalueCount++;
}

static int resolveUpvalue(Compiler* compiler, Token name) {
    if (compiler->enclosing == NULL) return -1;

    int immediate = resolveLocal(compiler->enclosing, name);
    if (immediate != -1) {
        #ifdef DEBUG_UPVALUE_INFO
        printf("Upvalue in immediate scope at %d\n", immediate);
        #endif
        compiler->enclosing->locals[immediate].isCaptured = true;
        return addUpvalue(compiler, (uint8_t)immediate, true);
    }

    int greater = resolveUpvalue(compiler->enclosing, name);
    if (greater != -1) {
        int added = addUpvalue(compiler, (uint8_t)greater, false);
        #ifdef DEBUG_UPVALUE_INFO
        printf("Upvalue in greater scope at %d\n", greater);
        #endif
        return added;
    }

    return -1;
}


void initCompiler(Compiler* compiler, VM* vm, FunctionType type, ObjString* name) {
    compiler->scopeDepth = 0;
    compiler->localCount = 0;
    compiler->upvalueCount = 0;
    compiler->tree = NULL;

    compiler->enclosing = vm->compiler;
    compiler->vm = vm;
    vm->compiler = compiler;

    compiler->type = type;
    compiler->function = newFunction(vm, name);
}

ObjFunction* endCompiler(Compiler* compiler) {
    if (compiler->type == FUN_SCRIPT) {
        emitByte(compiler, OP_RETURN, getLastLine(compiler));
    }

    ObjFunction* func = compiler->function;

    #ifdef DEBUG_DISPLAY_PROGRAM
    disassembleChunk(&func->body, getName(compiler));
    #endif

    compiler->vm->compiler = compiler->enclosing;

    return func;
}


static void compileExpr(Compiler* compiler, Expr* expression);


static void integer(Compiler* compiler, Token token) {
    long long value = strtoll(token.start, NULL, 0);
    if (value <= UINT16_MAX) {
        emitShort(compiler, OP_INT_P, value, token.line);
    }
    else {
        emitConstant(compiler, INT_VAL(value), token.line);
    }
}

static void floating(Compiler* compiler, Token token) {
    long double value = strtold(token.start, NULL);
    size_t integral = (size_t)value;
    long double fractional = value - integral;

    if (fractional == 0 && integral <= UINT16_MAX) {
        emitShort(compiler, OP_FLOAT_P, integral, token.line);
        return;
    }

    emitConstant(compiler, FLOAT_VAL(value), token.line);
}

static void character(Compiler* compiler, Token token) {
    if (token.length == 3) {
        emitBytes(compiler, OP_CHAR, *(token.start + 1), token.line);
    }
    else {
        char value = ' ';

        switch (*(token.start + 2)) {
        case 'n':   value = '\n'; break;
        case 't':   value = '\t'; break;
        case 'b':   value = '\b'; break;
        case 'f':   value = '\f'; break;
        case '\\':  value = '\\'; break;
        case '\'':  value = '\''; break;
        default:
            compilerError(compiler, "Invalid escape sequence '%c%c'", *(token.start + 1), *(token.start + 2));
            break;
        }

        emitBytes(compiler, OP_CHAR, value, token.line);
    }
}

static void string(Compiler* compiler, Token token) {
    emitConstant(compiler, OBJ_VAL(copyString(compiler->vm, token.start + 1, token.length - 2)), token.line);
}

static void fString(Compiler* compiler, Token oldStr) {
    char* newStr = calloc(oldStr.length - 3, sizeof(char));
    size_t newLen = 0;

    // Cut off the leading 'f"' and the trailing '"'
    for (const char* ptr = oldStr.start + 2; ptr - oldStr.start < oldStr.length - 1; ptr++, newLen++) {
        char tba = *ptr;

        if (tba == '\\') {
            switch (ptr[1]) {
            case '\\': ptr++; newStr[newLen] = '\\'; break;
            case '"':  ptr++; newStr[newLen] = '"';  break;
            case 'n':  ptr++; newStr[newLen] = '\n'; break;
            case 't':  ptr++; newStr[newLen] = '\t'; break;
            case 'b':  ptr++; newStr[newLen] = '\b'; break;
            case 'f':  ptr++; newStr[newLen] = '\f'; break;
            case '\n': ptr++; newLen--; break;
            default: break;
            } continue;
        }

        newStr[newLen] = tba;
    }

    emitConstant(compiler, OBJ_VAL(copyString(compiler->vm, newStr, newLen)), oldStr.line);
    free(newStr);
}

static void compileLiteral(Compiler* compiler, LiteralExpr* literal) {
    Token token = getToken(compiler, (Expr*)literal);
    if (token.type == TOKEN_WILDCARD) {
        return;
    }
    else if (token.type == TOKEN_IDENTIFIER) {
        int spot;

        if (compiler->localCount > 0 && (spot = resolveLocal(compiler, token)) != -1) {
            emitBytes(compiler, OP_GET_LOCAL, (uint8_t)spot, token.line);
        }
        else if ((spot = resolveUpvalue(compiler, token)) != -1) {
            emitBytes(compiler, OP_UPVALUE, (uint8_t)spot, token.line);
        }
        else {
            ObjString* name = copyString(compiler->vm, token.start, token.length);
            emitBytes(compiler, OP_GET_GLOBAL, makeConstant(compiler, OBJ_VAL(name)), token.line);
        }

        return;
    }
    else if (token.type == TOKEN_GLYPH) {
        Token glyph = (Token){token.type, token.start + 1, token.length - 1, token.line};
        int spot;

        if (compiler->localCount > 0 && (spot = resolveLocal(compiler, glyph)) != -1) {
            emitBytes(compiler, OP_GET_LOCAL, (uint8_t)spot, glyph.line);
        }
        else if ((spot = resolveUpvalue(compiler, glyph)) != -1) {
            emitBytes(compiler, OP_UPVALUE, (uint8_t)spot, glyph.line);
        }
        else {
            ObjString* name = copyString(compiler->vm, glyph.start, glyph.length);
            emitBytes(compiler, OP_GET_GLOBAL, makeConstant(compiler, OBJ_VAL(name)), glyph.line);
        }

        return;
    }
    else {
        switch (literal->token.type) {
            case TOKEN_INTEGER:        integer  (compiler, token);                  return;
            case TOKEN_FLOAT:          floating (compiler, token);                  return;
            case TOKEN_STRING:         string   (compiler, token);                  return;
            case TOKEN_FORMAT_STRING:  fString  (compiler, token);                  return;
            case TOKEN_CHAR:           character(compiler, token);                  return;
            case TOKEN_TRUE:           emitByte (compiler, OP_TRUE, token.line);    return;
            case TOKEN_FALSE:          emitByte (compiler, OP_FALSE, token.line);   return;
            case TOKEN_UNIT:           emitByte (compiler, OP_UNIT, token.line);    return;
            default: break;
        }
    }

    compilerError(compiler, "Invalid expression at %.*s", token.length, token.start);
}

static void optimiseReturn(Compiler* compiler, UnaryExpr* unary) {
    Token token = getToken(compiler, (Expr*)unary);
    Expr* operand = unary->operand;

    if (operand->type == EXPR_BINARY && (getToken(compiler, operand).type == TOKEN_LEFT_PAREN || getToken(compiler, operand).type == TOKEN_DOLLAR)) {
        compileExpr(compiler, unary->operand);
        currentChunk(compiler)->code[currentChunk(compiler)->count - 2] = (uint8_t)OP_TAIL_CALL;
    }
    else {
        compileExpr(compiler, unary->operand);
        emitByte(compiler, OP_RETURN, token.line);
    }
}

static void optimiseNegation(Compiler* compiler, UnaryExpr* unary) {
    Token arg = getToken(compiler, unary->operand);
    if (arg.type == TOKEN_INTEGER) {
        long long value = strtoll(arg.start, NULL, 0);

        if (value <= UINT16_MAX) {
            emitShort(compiler, OP_INT_N, value, arg.line);
        }
        else {
            emitConstant(compiler, INT_VAL(-value), arg.line);
        }
    }
    else if (arg.type == TOKEN_FLOAT) {
        double value = strtod(arg.start, NULL);
        size_t integral = (size_t)value;
        long double fractional = value - integral;

        if (fractional == 0 && integral <= UINT16_MAX) {
            emitShort(compiler, OP_FLOAT_N, integral, arg.line);
        }
        else {
            emitConstant(compiler, FLOAT_VAL(-value), arg.line);
        }
    }
    else {
        compileExpr(compiler, unary->operand);
        emitByte(compiler, OP_NEGATE, arg.line);
    }
}

static void plainUnary(Compiler* compiler, UnaryExpr* unary, OpCode op) {
    compileExpr(compiler, unary->operand);
    emitByte(compiler, op, getToken(compiler, (Expr*)unary).line);
}

static void compileUnary(Compiler* compiler, UnaryExpr* unary) {
    Token token = getToken(compiler, (Expr*)unary);
    switch (token.type) {
    case TOKEN_RETURN:      optimiseReturn(compiler, unary);            return;
    case TOKEN_BANG:        plainUnary(compiler, unary, OP_NOT);        return;
    case TOKEN_MINUS:       optimiseNegation(compiler, unary);          return;
    case TOKEN_EOF:         plainUnary(compiler, unary, OP_RETURN);     return;
    case TOKEN_CAR:         plainUnary(compiler, unary, OP_CAR);        return;
    case TOKEN_CDR:         plainUnary(compiler, unary, OP_CDR);        return;
    case TOKEN_QUESTION:    plainUnary(compiler, unary, OP_TRUTHY);     return;
    default: break;
    }

    compilerError(compiler, "Invalid expression at %.*s", token.length, token.start);
}


static void plainBinary(Compiler* compiler, BinaryExpr* binary, OpCode op) {
    compileExpr(compiler, binary->left);
    compileExpr(compiler, binary->right);
    emitByte(compiler, op, binary->token.line);
}

static void reverseBinary(Compiler* compiler, BinaryExpr* binary, OpCode op) {
    compileExpr(compiler, binary->right);
    compileExpr(compiler, binary->left);
    emitByte(compiler, op, binary->token.line);
}

static void _and(Compiler* compiler, BinaryExpr* binary) {
    compileExpr(compiler, binary->left);
    int falseyJmp = emitJump(compiler, OP_JUMP_IF_FALSE, getToken(compiler, binary->left).line);
    emitByte(compiler, OP_POP, getToken(compiler, binary->left).line); // remove condition

    compileExpr(compiler, binary->right);
    patchJump(compiler, falseyJmp);
}

static void _or(Compiler* compiler, BinaryExpr* binary) {
    compileExpr(compiler, binary->left);
    int truthyJmp = emitJump(compiler, OP_JUMP_IF_TRUE, getToken(compiler, binary->left).line);
    emitByte(compiler, OP_POP, getToken(compiler, binary->left).line); // remove condition

    compileExpr(compiler, binary->right);
    patchJump(compiler, truthyJmp);
}

// Refactor to only copy string once
// Leave 'ObjString* name = NULL;' in the fn scope so the other parts can
// access it
static void bindVal(Compiler* compiler, BinaryExpr* binary) {
    int spot = 0;
    Token nomme = getToken(compiler, binary->left);
    ObjString* name = NULL;

    // Scan identifier;
    if (nomme.type == TOKEN_IDENTIFIER)
        name = copyString(compiler->vm, nomme.start, nomme.length);
    else if (nomme.type == TOKEN_GLYPH)
        name = copyString(compiler->vm, nomme.start + 1, nomme.length - 1);
    else {
        compilerError(compiler, "Expected lvalue, got %.*s", nomme.length, nomme.start);
        return;
    }

    // Create binding
    if (compiler->scopeDepth == 0)
        spot = makeConstant(compiler, OBJ_VAL(name));
    else
        addLocal(compiler, name);

    // Handle rvalue
    if (getToken(compiler, binary->right).type == TOKEN_LEFT_BRACE && compiler->scopeDepth > 0) {
        emitByte(compiler, OP_UNIT, nomme.line);
        compileExpr(compiler, binary->right);
        emitBytes(compiler, OP_SWAP_TOP, OP_POP, getLastLine(compiler));
    }
    else {
        compileExpr(compiler, binary->right);
    }

    // Complete assignment
    if (compiler->scopeDepth == 0) {
        emitBytes(compiler, OP_MAKE_GLOBAL, (uint8_t)spot, binary->token.line);
    }
    else {
        fixLocal(compiler, name);

        if (getToken(compiler, binary->right).type == TOKEN_EQUALS) {
            emitByte(compiler, OP_DUPE_TOP, getLastLine(compiler));
        }
    }
}

// TODO: this can be cleaned up/improved; either making the decision to only allow list-like cells to be deconstructed, or adding functionality to deconstruct more complex arrangements
static void recurse(Compiler* compiler, BinaryExpr* binary) {

    if (getToken(compiler, binary->right).type == TOKEN_COMMA) {
        emitByte(compiler, OP_DECONS, getToken(compiler, binary->right).line);
        recurse(compiler, (BinaryExpr*)binary->right);
        emitByte(compiler, OP_POP, getToken(compiler, binary->left).line);
    }
    else {
        emitByte(compiler, OP_DECONS, getToken(compiler, binary->right).line);
        if (getToken(compiler, binary->right).type == TOKEN_IDENTIFIER) {
            Token id = getToken(compiler, binary->right);
            ObjString* name = copyString(compiler->vm,  id.start, id.length);
            uint8_t spot = makeConstant(compiler, OBJ_VAL(name));
            emitBytes(compiler, OP_MAKE_GLOBAL, spot, id.line);
            emitByte(compiler, OP_POP, id.line);
        }
        else if (getToken(compiler, binary->right).type == TOKEN_WILDCARD){
            emitByte(compiler, OP_POP, getToken(compiler, binary->right).line);
        }
        else {
            compilerError(compiler, "Expected lvalue, got %.*s", getToken(compiler, binary->right).length, getToken(compiler, binary->right).start);
            return;
        }
    }

    if (getToken(compiler, binary->left).type == TOKEN_COMMA) {
        recurse(compiler, (BinaryExpr*)binary->left);
    }
    else if (getToken(compiler, binary->left).type == TOKEN_IDENTIFIER) {
        Token id = getToken(compiler, binary->left);
        ObjString* name = copyString(compiler->vm,  id.start, id.length);
        uint8_t spot = makeConstant(compiler, OBJ_VAL(name));
        emitBytes(compiler, OP_MAKE_GLOBAL, spot, id.line);
    }
    else if (getToken(compiler, binary->left).type != TOKEN_WILDCARD) {
        compilerError(compiler, "Expected lvalue, got %.*s", getToken(compiler, binary->left).length, getToken(compiler, binary->left).start);
        return;
    }
}

static ObjCell* maskTree(Compiler* compiler, BinaryExpr* binary) {
    ObjCell* cell = newCell(compiler->vm);

    TokenType lType = getToken(compiler, binary->left).type;
    if (lType == TOKEN_IDENTIFIER) {
        Token id = getToken(compiler, binary->left);
        ObjString* name = copyString(compiler->vm, id.start, id.length);
        addLocal(compiler, name);
        fixLocal(compiler, name);
        cell->car = BOOL_VAL(true);
        // make local and add 'true' branch
    }
    else if (lType == TOKEN_WILDCARD) {
        cell->car = BOOL_VAL(false);
        // add 'false' branch
    }
    else if (lType == TOKEN_COMMA) {
        cell->car = OBJ_VAL(maskTree(compiler, (BinaryExpr*)binary->left));
        // recurse to add cell branch
    }
    else {
        compilerError(compiler, "Expected lvalue, got %.*s", getToken(compiler, binary->left).length, getToken(compiler, binary->left).start);
        // ignore branch and throw an error
    }

    TokenType rType = getToken(compiler, binary->right).type;
    if (rType == TOKEN_IDENTIFIER) {
        Token id = getToken(compiler, binary->right);
        ObjString* name = copyString(compiler->vm, id.start, id.length);
        addLocal(compiler, name);
        fixLocal(compiler, name);
        cell->cdr = BOOL_VAL(true);
        // make local and add 'true' branch
    }
    else if (rType == TOKEN_WILDCARD) {
        cell->cdr = BOOL_VAL(false);
        // add 'false' branch
    }
    else if (rType == TOKEN_COMMA) {
        cell->cdr = OBJ_VAL(maskTree(compiler, (BinaryExpr*)binary->right));
        // recurse to add cell branch
    }
    else {
        compilerError(compiler, "Expected lvalue, got %.*s", getToken(compiler, binary->right).length, getToken(compiler, binary->right).start);
        // ignore branch and throw an error
    }

    return cell;
}

static void decons(Compiler* compiler, BinaryExpr* binary) {
    compileExpr(compiler, binary->right);

    if (compiler->scopeDepth == 0) {
        recurse(compiler, (BinaryExpr*)binary->left);
    }
    else {
        ObjCell* mask = maskTree(compiler, (BinaryExpr*)binary->left);
        uint8_t spot = makeConstant(compiler, OBJ_VAL(mask));
        emitBytes(compiler, OP_TREE_COMP, spot, getToken(compiler, binary->left).line);
    }
}

static void assignment(Compiler* compiler, BinaryExpr* binary) {
    if (binary->left->type == EXPR_BINARY && getToken(compiler, binary->left).type == TOKEN_COMMA) {
        decons(compiler, binary);
    }
    else {
        bindVal(compiler, binary);
    }
}

static void apply(Compiler* compiler, BinaryExpr* binary) {
    compileExpr(compiler, binary->left);

    BlockExpr* args = (BlockExpr*)binary->right;

    for (int i = 0; i < args->count; i++) {
        compileExpr(compiler, args->subexprs[i]);
    }

    emitBytes(compiler, OP_CALL, (uint8_t)args->count, getToken(compiler, (Expr*)binary).line);
}

static void compileMatch(Compiler* compiler, BinaryExpr* binary) {
    int endings[UINT8_MAX];
    int endCount = 0;

    compileExpr(compiler, binary->left);

    BlockExpr* cases = (BlockExpr*)binary->right;

    if (cases->count > UINT16_MAX) {
        compilerError(compiler, "Too many cases in match; limit is %d, had %d", UINT16_MAX, cases->count);
        return;
    }

    for (int i = 0; i < cases->count; i++) {
        BinaryExpr* _case = (BinaryExpr*)cases->subexprs[i];

        if (getToken(compiler, _case->left).type == TOKEN_WILDCARD) {
            emitByte(compiler, OP_DUPE_TOP, getToken(compiler, _case->left).line);
        }
        else {
            compileExpr(compiler, _case->left);
        }

        int skipCase = emitJump(compiler, OP_TEST_CASE, getToken(compiler, _case->left).line);

        compileExpr(compiler, _case->right);
        endings[endCount++] = emitJump(compiler, OP_JUMP, getToken(compiler, _case->right).line);
        patchJump(compiler, skipCase);
    }

    for (int i = 0; i < endCount; i++) {
        patchJump(compiler, endings[i]);
    }
}

static void bothInts(Compiler* compiler, BinaryExpr* binary, OpCode op, int offset) {
    long long a = strtoll(getToken(compiler, binary->left).start + offset, NULL, 0);
    long long b = strtoll(getToken(compiler, binary->right).start + offset, NULL, 0);
    long long c = 0;

    switch (op) {
        case OP_ADD:        c = a + b; break;
        case OP_SUBTRACT:   c = a - b; break;
        case OP_MULTIPLY:   c = a * b; break;
        case OP_DIVIDE:     c = a / b; break;
        case OP_MODULO:     c = a % b; break;
        case OP_EXPONENT:   c = (long long)pow((double)a, (double)b); break;
        default: break;
    }

    if (c <= UINT16_MAX && c >= 0) {
        emitShort(compiler, OP_INT_P, c, getToken(compiler, (Expr*)binary).line);
    }
    else if (c < 0 && c >= (-UINT16_MAX)) {
        emitShort(compiler, OP_INT_N, -c, getToken(compiler, (Expr*)binary).line);
    }
    else {
        emitConstant(compiler, INT_VAL(c), getToken(compiler, (Expr*)binary).line);
    }
}

static void bothFloats(Compiler* compiler, BinaryExpr* binary, OpCode op) {
    double a = strtod(getToken(compiler, binary->left).start, NULL);
    double b = strtod(getToken(compiler, binary->right).start, NULL);
    double c = 0;

    switch (op) {
        case OP_ADD:        c = a + b; break;
        case OP_SUBTRACT:   c = a - b; break;
        case OP_MULTIPLY:   c = a * b; break;
        case OP_DIVIDE:     c = a / b; break;
        case OP_MODULO:     c = fmod(a, b); break;
        case OP_EXPONENT:   c = pow(a, b); break;
        default: break;
    }

    size_t integral = (size_t)c;
    double fractional = c - integral;

    if (fractional == 0 && c <= UINT16_MAX && c >= 0) {
        emitShort(compiler, OP_FLOAT_P, c, getToken(compiler, (Expr*)binary).line);
    }
    else if (fractional == 0 && c < 0 && c >= (-UINT16_MAX)) {
        emitShort(compiler, OP_FLOAT_N, -c, getToken(compiler, (Expr*)binary).line);
    }
    else {
        emitConstant(compiler, FLOAT_VAL(c), getToken(compiler, (Expr*)binary).line);
    }
}

static void optimiseArithmetic(Compiler* compiler, BinaryExpr* binary, OpCode op) {
    if (getToken(compiler, binary->left).type == TOKEN_INTEGER && getToken(compiler, binary->right).type == TOKEN_INTEGER) {
        bothInts(compiler, binary, op, 0);
    }
    else if (getToken(compiler, binary->left).type == TOKEN_FLOAT && getToken(compiler, binary->right).type == TOKEN_FLOAT) {
        bothFloats(compiler, binary, op);
    }
    else {
        plainBinary(compiler, binary, op);
    }
}

static void optimiseConcatenation(Compiler* compiler, BinaryExpr* binary) {
    if (getToken(compiler, binary->left).type == TOKEN_INTEGER && getToken(compiler, binary->right).type == TOKEN_INTEGER) {
        long long a = strtoll(getToken(compiler, binary->left).start, NULL, 0);
        long long b = strtoll(getToken(compiler, binary->right).start, NULL, 0);
        ObjList* list = newList(compiler->vm);

        long long c = a;
        int dir = a < b ? 1 : -1;
        while (c != b + dir) {
            writeValueArray(compiler->vm, &list->array, INT_VAL(c));
            c += dir;
        }

        emitConstant(compiler, OBJ_VAL(list), getToken(compiler, (Expr*)binary).line);
    }
    else if (getToken(compiler, binary->left).type == TOKEN_STRING && getToken(compiler, binary->right).type == TOKEN_STRING) {
        Token a = getToken(compiler, binary->left);
        Token b = getToken(compiler, binary->right);

        int new_length = (a.length - 2) + (b.length - 2);
        char* heapChars = ALLOCATE(compiler->vm, new_length + 1, char);
        memcpy(heapChars, a.start + 1, a.length - 2);
        memcpy(heapChars + (a.length - 2), b.start + 1, b.length - 2);
        heapChars[new_length] = '\0';

        ObjString* c = takeString(compiler->vm, heapChars, new_length);

        emitConstant(compiler, OBJ_VAL(c), getToken(compiler, (Expr*)binary).line);
    }
    else {
        plainBinary(compiler, binary, OP_CONCAT);
    }
}

static void slice(Compiler* compiler, BinaryExpr* binary) {
    if (getToken(compiler, binary->left).type == TOKEN_UNIT) {
        if (getToken(compiler, binary->right).type == TOKEN_UNIT) {
            // start to end
            emitBytes(compiler, OP_SLICE, 0, getLastLine(compiler));
        }
        else {
            // start to y
            compileExpr(compiler, binary->right);
            emitBytes(compiler, OP_SLICE, 1, getLastLine(compiler));
        }
    }
    else {
        compileExpr(compiler, binary->left);
        if (getToken(compiler, binary->right).type == TOKEN_UNIT) {
            // x to end
            emitBytes(compiler, OP_SLICE, 2, getLastLine(compiler));
        }
        else {
            // x to y
            compileExpr(compiler, binary->right);
            emitBytes(compiler, OP_SLICE, 3, getLastLine(compiler));
        }
    }
}

static void subscripting(Compiler* compiler, BinaryExpr* binary) {
    if (getToken(compiler, binary->right).type == TOKEN_COLON && binary->right->type == EXPR_BINARY) {
        compileExpr(compiler, binary->left);
        slice(compiler, (BinaryExpr*)binary->right);
    }
    else {
        plainBinary(compiler, binary, OP_SUBSCRIPT);
    }
}

static void getCustom(Compiler* compiler, Expr* expr) {
    Token toLiteral = getToken(compiler, expr);
    toLiteral.type = TOKEN_IDENTIFIER;
    LiteralExpr operator = (LiteralExpr){(Expr){EXPR_LITERAL, NULL}, toLiteral};
    compileLiteral(compiler, &operator);
}

static void functionOperator(Compiler* compiler, BinaryExpr* binary) {
    getCustom(compiler, (Expr*)binary);
    compileExpr(compiler, binary->left);
    compileExpr(compiler, binary->right);
    emitBytes(compiler, OP_CALL, 2, getLastLine(compiler));
}

static void compileBinary(Compiler* compiler, BinaryExpr* binary) {
    Token token = getToken(compiler, (Expr*)binary);
    switch (token.type) {
        // call
        case TOKEN_LEFT_PAREN:      apply(compiler, binary); return;
        case TOKEN_LEFT_BRACKET:    subscripting(compiler, binary); return;

        case TOKEN_DOT:             plainBinary(compiler, binary, OP_COMPOSE); return;
        case TOKEN_DOT_DOT:         optimiseConcatenation(compiler, binary); return;
        case TOKEN_COMMA:           plainBinary(compiler, binary, OP_CONSTRUCT); return;

        case TOKEN_PLUS:            optimiseArithmetic(compiler, binary, OP_ADD); return;
        case TOKEN_MINUS:           optimiseArithmetic(compiler, binary, OP_SUBTRACT); return;
        case TOKEN_STAR:            optimiseArithmetic(compiler, binary, OP_MULTIPLY); return;
        case TOKEN_SLASH:           optimiseArithmetic(compiler, binary, OP_DIVIDE); return;
        case TOKEN_PERCENT:         optimiseArithmetic(compiler, binary, OP_MODULO); return;
        case TOKEN_UCARET:          optimiseArithmetic(compiler, binary, OP_EXPONENT); return;

        case TOKEN_EQUALS:          assignment(compiler, binary); return;
        case TOKEN_RECEIVE:         plainBinary(compiler, binary, OP_RECEIVE); return;

        case TOKEN_GREATER:         plainBinary(compiler, binary, OP_DIFF); return;
        case TOKEN_LESS:            reverseBinary(compiler, binary, OP_DIFF); return;
        case TOKEN_GREATER_EQUALS:  plainBinary(compiler, binary, OP_DIFFEQ); return;
        case TOKEN_LESS_EQUALS:     reverseBinary(compiler, binary, OP_DIFFEQ); return;
        case TOKEN_BANG_EQUALS:     plainBinary(compiler, binary, OP_EQUALS); emitByte(compiler, OP_NOT, getLastLine(compiler)); return;
        case TOKEN_EQUALS_EQUALS:   plainBinary(compiler, binary, OP_EQUALS); return;

        // call
        case TOKEN_DOLLAR:          apply(compiler, binary); return;
        case TOKEN_SPIGOT:          reverseBinary(compiler, binary, OP_CALL); emitByte(compiler, 1, getLastLine(compiler)); return;

        case TOKEN_CUSTOM:          functionOperator(compiler, binary); return;

        case TOKEN_CONS:            plainBinary(compiler, binary, OP_CONSTRUCT); return;

        // short-circuiting
        case TOKEN_AND:             _and(compiler, binary); return;
        case TOKEN_OR:              _or(compiler, binary); return;
        case TOKEN_MATCH:           compileMatch(compiler, binary); return;

        case TOKEN_IN:              plainBinary(compiler, binary, OP_IN); return;
        default: break;
    }

    compilerError(compiler, "Invalid expression at %.*s", token.length, token.start);
}

static void compileIf(Compiler* compiler, TernaryExpr* ternary) {
    compileExpr(compiler, ternary->pivot);
    int skipThen = emitJump(compiler, OP_JUMP_IF_FALSE, ternary->token.line);
    emitByte(compiler, OP_POP, ternary->token.line);

    // then branch
    compileExpr(compiler, ternary->left);

    int skipElse = emitJump(compiler, OP_JUMP, getToken(compiler, ternary->right).line);
    patchJump(compiler, skipThen);
    emitByte(compiler, OP_POP, getToken(compiler, ternary->right).line);

    // else branch
    compileExpr(compiler, ternary->right);

    patchJump(compiler, skipElse);
}

static bool isNamedFn(Compiler* compiler, Expr* expr) {
    return getToken(compiler, expr).type == TOKEN_COLON && expr->type == EXPR_TERNARY && getToken(compiler, ((TernaryExpr*)expr)->left).type != TOKEN_WILDCARD;
}

static void openBlock(Compiler* compiler, BlockExpr* block)  {
    for (size_t i = 0; i < block->count - 1; ++i) {
        Expr* next = block->subexprs[i];

        compileExpr(compiler, next);

        if (getToken(compiler, next).type != TOKEN_EQUALS && !isNamedFn(compiler, next)) {
            //printf("Emitting pop\n");
            emitByte(compiler, OP_POP, getLastLine(compiler));
        }
    }


    if (block->count != 0) {
        Expr* last = block->subexprs[block->count - 1];

        compileExpr(compiler, last);

        if (getToken(compiler, last).type == TOKEN_EQUALS || isNamedFn(compiler, last)) {
            emitByte(compiler, OP_DUPE_TOP, getLastLine(compiler));
        }
    }
}

static bool isFnName(Compiler* compiler, Expr* expr) {
    return  isTType(compiler, expr, TOKEN_IDENTIFIER) ||
            isTType(compiler, expr, TOKEN_WILDCARD)   ||
            isTType(compiler, expr, TOKEN_GLYPH);
}

static void compileFunction(Compiler* enclosing, TernaryExpr* ternary) {
    #ifdef DEBUG_COMPILER_PROGRESS
    printf("Starting fn compilation\n");
    printf("fn name is ");
    Token fgjho = getToken(enclosing, ternary->left);
    printToken(&fgjho);
    #endif

    if (!isFnName(enclosing, ternary->left)) {
        compilerError(enclosing, "Expected function identifier or wildcard, got %.*s", getToken(enclosing, ternary->left).length, getToken(enclosing, ternary->left).start);
        return;
    }

    if (((BlockExpr*)ternary->pivot)->count > UINT8_MAX) {
        compilerError(enclosing, "Too many function args; limit is %d, had %d", UINT8_MAX, ((BlockExpr*)ternary->pivot)->count);
        return;
    }

    Compiler compiler;
    LiteralExpr* leftHand = (LiteralExpr*)ternary->left;
    ObjString* name = NULL;
    FunctionType type = FUN_LAMBDA;

    if (leftHand->token.type != TOKEN_WILDCARD) {
        // Identifier or glyph?
        name = leftHand->token.type == TOKEN_IDENTIFIER
                ? copyString(enclosing->vm, leftHand->token.start, leftHand->token.length)
                : copyString(enclosing->vm, leftHand->token.start + 1, leftHand->token.length - 1);
        type = FUN_FUNCTION;
        // necessary for recursive functions
        if (enclosing->scopeDepth > 0) {
            addLocal(enclosing, name);
            fixLocal(enclosing, name);
        }
    }

    initCompiler(&compiler, enclosing->vm, type, name);
    compiler.tree = enclosing->tree;
    beginScope(&compiler);

    #ifdef DEBUG_COMPILER_PROGRESS
    printf("Initialised compiler & fn scope\n");
    #endif

    BlockExpr* args = (BlockExpr*)ternary->pivot;

    for (int i = 0; i < args->count; i++) {
        LiteralExpr* literal = (LiteralExpr*)args->subexprs[i];
        Token token = literal->token;
        compiler.locals[compiler.localCount++] = (Local){copyString(compiler.vm, token.start, token.length), compiler.scopeDepth, false};
    }

    #ifdef DEBUG_COMPILER_PROGRESS
    printf("Compiled fn args\n");
    #endif

    // implicit returns
    if (getToken(enclosing, ternary->right).type != TOKEN_LEFT_BRACE) {
        compileExpr(&compiler, ternary->right);

        if (getToken(enclosing, ternary->right).type == TOKEN_DOLLAR || getToken(enclosing, ternary->right).type == TOKEN_LEFT_PAREN) {
            currentChunk(&compiler)->code[currentChunk(&compiler)->count - 2] = (uint8_t)OP_TAIL_CALL;
        }
        else if (getToken(enclosing, ternary->right).type != TOKEN_RETURN) {
            emitByte(&compiler, OP_RETURN, getLastLine(&compiler));
        }
    }
    else {
        BlockExpr* body = (BlockExpr*)ternary->right;
        openBlock(&compiler, body);
        if (getToken(enclosing, body->subexprs[body->count - 1]).type != TOKEN_RETURN) {
            emitByte(&compiler, OP_RETURN, getLastLine(&compiler));
        }
    }

    #ifdef DEBUG_COMPILER_PROGRESS
    printf("Compiled fn body\n");
    #endif

    ObjFunction* func = endCompiler(&compiler);
    func->arity = args->count;
    emitConstant(enclosing, OBJ_VAL(func), getToken(enclosing, (Expr*)ternary).line);

    if (compiler.upvalueCount > 0) {
        int line = getToken(enclosing, (Expr*)ternary).line;

        emitBytes(enclosing, OP_CLOSURE, compiler.upvalueCount, line);

        for (int i = 0; i < compiler.upvalueCount; i++) {
            emitByte(enclosing, compiler.upvalues[i].isLocal ? 1 : 0, line);
            emitByte(enclosing, compiler.upvalues[i].index, line);
        }
    }

    #ifdef DEBUG_UPVALUE_INFO
    printf("Emitted %d upvalues\n", compiler.upvalueCount);
    #endif

    if (leftHand->token.type != TOKEN_WILDCARD && enclosing->scopeDepth == 0) {
        uint8_t spot = makeConstant(enclosing, OBJ_VAL(name));
        emitBytes(enclosing, OP_MAKE_GLOBAL, spot, leftHand->token.line);
    }
}

static void compileTernary(Compiler* compiler, TernaryExpr* ternary) {
    Token token = ternary->token;

    switch (token.type) {
        case TOKEN_IF:      compileIf(compiler, ternary); return;
        case TOKEN_COLON:   compileFunction(compiler, ternary); return;
        default: break;
    }

    printToken(&token);

    compilerError(compiler, "Invalid expression at %.*s", token.length, token.start);
}


static void codeBlock(Compiler* compiler, BlockExpr* block)  {
    beginScope(compiler);

    for (size_t i = 0; i < block->count - 1; ++i) {
        Expr* next = block->subexprs[i];

        compileExpr(compiler, next);

        if (getToken(compiler, next).type != TOKEN_EQUALS &&
            (getToken(compiler, next).type != TOKEN_COLON && getToken(compiler, ((TernaryExpr*)next)->left).type != TOKEN_WILDCARD)) {
            emitByte(compiler, OP_POP, getLastLine(compiler));
        }
    }

    Expr* last = block->subexprs[block->count - 1];

    if (block->count != 0) {
        compileExpr(compiler, last);

        if (getToken(compiler, last).type == TOKEN_EQUALS ||
            (getToken(compiler, last).type == TOKEN_COLON && getToken(compiler, ((TernaryExpr*)last)->left).type != TOKEN_WILDCARD)) {
            emitByte(compiler, OP_DUPE_TOP, getLastLine(compiler));
        }
    }

    endScope(compiler, getLastLine(compiler));
}

static void list(Compiler* compiler, BlockExpr* block) {
    for (size_t i = 0; i < block->count; ++i) {
        compileExpr(compiler, block->subexprs[i]);
    }

    emitBytes(compiler, OP_LIST, (uint8_t)block->count, getToken(compiler, (Expr*)block).line);
}

static void map(Compiler* compiler, BlockExpr* block) {
    for (size_t i = 0; i < block->count; ++i) {
        BinaryExpr* mapping = (BinaryExpr*)(block->subexprs[i]);
        compileExpr(compiler, mapping->left);
        compileExpr(compiler, mapping->right);
    }

    emitBytes(compiler, OP_MAP, (uint8_t)block->count, getToken(compiler, (Expr*)block).line);
}

static void compileBlock(Compiler* compiler, BlockExpr* block) {
    Token token = getToken(compiler, (Expr*)block);
    switch (token.type) {
    case TOKEN_LEFT_PAREN:      list(compiler, block); return;
    case TOKEN_LEFT_BRACE:      codeBlock(compiler, block); return;
    case TOKEN_LEFT_BRACKET:    map(compiler, block); return;
    default: break;
    }

    compilerError(compiler, "Invalid expression at %.*s", token.length, token.start);
}

static void compileExpr(Compiler* compiler, Expr* expression) {
    #ifdef DEBUG_COMPILER_PROGRESS
    printf("%s: About to compile %s\n", getName(compiler), getExprName(expression->type));
    // Token tok = getToken(compiler, expression);
    // printToken(&tok);
    #endif
    switch (expression->type) {
        case EXPR_LITERAL:  {
            LiteralExpr* literal = (LiteralExpr*)expression;
            compileLiteral(compiler, literal);
            break;
        }

        case EXPR_UNARY:    {
            UnaryExpr* unary = (UnaryExpr*)expression;
            compileUnary(compiler, unary);
            break;
        }

        case EXPR_BINARY:   {
            BinaryExpr* binary = (BinaryExpr*)expression;
            compileBinary(compiler, binary);
            break;
        }

        case EXPR_TERNARY:  {
            TernaryExpr* ternary = (TernaryExpr*)expression;
            compileTernary(compiler, ternary);
            break;
        }

        case EXPR_BLOCK:    {
            BlockExpr* block = (BlockExpr*)expression;
            compileBlock(compiler, block);
            break;
        }
        default:
            break;
    }
}

ObjFunction* compile(const char* source, VM* vm) {
    Compiler compiler;
    initCompiler(&compiler, vm, FUN_SCRIPT, NULL);

    ProgramTree tree;
    createTree(&compiler, &tree, source);

    if (tree.hadError) {
        fprintf(stderr, "Encountered error in parsing\n");
        freeTree(&tree);
        return NULL;
    }

    compiler.tree = &tree;

    #ifdef DEBUG_DISPLAY_TREE
    printf("Count is %d; capacity is %d\n", compiler.tree->program->count, compiler.tree->program->capacity);
    #endif

    for (size_t i = 0; i < compiler.tree->program->count - 1; ++i) {
        Expr* next = compiler.tree->program->subexprs[i];

        compileExpr(&compiler, next);

        emitByte(&compiler, OP_POP, getLastLine(&compiler));

        #ifdef DEBUG_COMPILER_PROGRESS
        printf(compiler.tree->hadError ? "Compiled with errors\n" : "Compiled successfully\n");
        #endif
    }

    compileExpr(&compiler, compiler.tree->program->subexprs[compiler.tree->program->count - 1]);

    if (tree.hadError) {
        fprintf(stderr, "Encountered error in compiling\n");
        freeTree(&tree);
        return NULL;
    }

    freeTree(&tree);
    return endCompiler(&compiler);
}
