#ifndef compiler_h_hammer
#define compiler_h_hammer

#include "common.h"
#include "ast.h"
#include "chunk.h"
#include "object.h"

// +-----------------------------------------------------------------+
// | The Hammer byte compiler uses a stack-based approach not too    |
// | dissimilar to the one used by CLox. This specification of       |
// | bytecode is dubbed 'Maul'                                       |
// +-----------------------------------------------------------------+

typedef struct {
    ObjString* name;
    int depth;
} Local;

typedef enum {
    FUN_SCRIPT,
    FUN_FUNCTION,
    FUN_LAMBDA,
} FunctionType;

struct Compiler {
    // Static
    int scopeDepth;
    int localCount;
    Local locals[STACK_SIZE];
    FunctionType type;
    int upvalueCount;
    uint8_t upvalues[STACK_SIZE];

    // Rigid
    ObjFunction* function;
    Compiler* enclosing;
    VM* vm;

    // Dynamic
    ProgramTree* tree;
};

void markCompiler(VM* vm);
void initCompiler(Compiler* compiler, VM* vm, FunctionType type, ObjString* name);
ObjFunction* endCompiler(Compiler* compiler);
ObjFunction* compile(const char* source, VM* vm);

#endif