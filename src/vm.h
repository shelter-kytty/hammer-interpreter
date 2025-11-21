#ifndef vm_h_hammer
#define vm_h_hammer

#include "common.h"
#include "value.h"
#include "table.h"
#include "object.h"


typedef struct {
    ObjFunction* function;
    ObjClosure* closure;
    uint8_t* ip;
    Value* slots;
    bool isCHOF;
} CallFrame;

struct VM {
    // Static
    CallFrame frames[FRAME_MAX];
    int frameCount;
    Value stack[STACK_SIZE];
    Value* stackTop;

    // Rigid
    Compiler* compiler;

    // Dynamic
    Obj* objects;
    Obj* greyStart;
    Obj* greyEnd;
    Table strings;
    Table globals;

//  -+ Garbage Collection +-
    // Should the gc consider the heap size?
    // Remains 'false' until the vm enters the
    // runtime environment
    bool isActive;

    // How many bytes have been allocated (not including the AST)
    // Decreases when runtime memory is free'd
    size_t bytesAllocated;

    // How many bytes until the next gc occurs
    // Calculated based on how much memory remains
    // after a sweep, multiplied by a constant factor
    size_t nextGC;
};

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILATION_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;


void initVM(VM* vm);
void freeVM(VM* vm);
InterpretResult interpret(VM* vm, const char* source);
InterpretResult interpretTEST(const char* source);
InterpretResult interpretPrecompiled(VM* vm, const char* source);
InterpretResult repl();


#endif
