
#include "builtins.h"
#include "../vm.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
#include <time.h>

#include "../common.h"
#include "../compiler.h"
#include "../debug.h"
#include "../memory.h"


extern void runtimeError(VM *vm, const char *format, ...);

extern bool callValue(VM* vm, Value caller, uint8_t argCount);
extern bool isTruthy(Value value);
extern InterpretResult run(VM* vm);

extern CallFrame* currentFrame(VM *vm);
extern void push(VM *vm, Value value);
extern Value pop(VM *vm);
extern Value peek(VM *vm, int distance);


static bool callFromC(VM* vm, Value caller, uint8_t argCount) {
    if (!callValue(vm, caller, argCount)) {
        return false;
    }

    if (!IS_NATIVE(caller)) {
        currentFrame(vm)->isCHOF = true;

        if (run(vm) == INTERPRET_RUNTIME_ERROR) {
            return false;
        }

        return true;
    }

    #ifdef DEBUG_DISPLAY_STACK
    printf("????   | %-16s %02d %02d\n", "OP_CALL", OP_CALL, argCount);
    for (Value* ptr = vm->stack; ptr < vm->stackTop; ptr++) {
        printValue(*ptr);
        printf(" | ");
    }
    printf("\n");
    #endif

    return true;
}

static void returnNative(VM* vm, int argCount, Value result) {
    vm->stackTop -= argCount + 1;
    push(vm, result);
}

static void defineNative(VM* vm, const char* name, NativeFn function, int arity) {
    Value key = OBJ_VAL(copyString(vm, name, (int)strlen(name)));
    Value value = OBJ_VAL(newNative(vm, function, arity));
    tableAddEntry(vm, &vm->globals, AS_STRING(key), value);
}

// Change format for printf/n? currently '{n}' is the format but this means you cant just, put a number
// between braces. I think something like the C format with a single, simple preceding character could
// work, could use '%n' or similar. Would allow for escaping the format more easily.
static inline bool isIntChar(char ch) {
    return ch >= '0' && ch <= '9';
}

static ObjList* reverseList(VM* vm, ObjList* in) {
    ObjList* out = newList(vm);

    // Garbage Collector...
    push(vm, OBJ_VAL(out));

    for (size_t i = 0; i < in->array.count; ++i) {
        writeValueArray(vm, &out->array, in->array.values[(in->array.count - 1) - i]);
    }

    pop(vm);

    return out;
}

static ObjString* reverseString(VM* vm, ObjString* in) {
    int length = in->length;
    char* heapChars = ALLOCATE(vm, length + 1, char);

    for (size_t i = 0; i < length; ++i) {
        heapChars[i] = in->chars[(length - 1) - i];
    }

    heapChars[length] = '\0';

    return takeString(vm, heapChars, length);
}


/*
+--------------------+
| IMPLEMENTATION ^^^ |
+--------------------+
| LIBRARY        vvv |
+--------------------+
*/

bool clockNative(VM* vm, int argc, Value* argv) {
    returnNative(vm, argc, FLOAT_VAL((double)clock() / CLOCKS_PER_SEC));
    return true;
}

bool exitNative(VM* vm, int argc, Value* argv) {
    if (!IS_INT(argv[0])) {
        runtimeError(vm, "exit$ : Expected int, got %s", getValName(argv[0]));
        return false;
    }
    runtimeError(vm, "Exited with code %lli", AS_INT(argv[0]));
    return false;
}

bool printfNative(VM* vm, int argc, Value* argv) {
    if (!IS_STRING(argv[0])) {
        runtimeError(vm, "printf$ : Expected string, got %s", getValName(argv[0]));
        return false;
    }

    const char* format = AS_CSTRING(argv[0]);
    char* end;

    while (*format != '\0') {
        char next = *format;

        if (next == '{' && isIntChar(format[1])) {
            format++;

            long slot = strtol(format, &end, 10) + 1; // 0-indexed

            if (slot > argc - 1) {
                runtimeError(vm, "printf$ : Attempted to index out of args; got %d with %d args", slot, argc-1);
                return false;
            }

            printValue(argv[slot]);

            format = end;

            if (*format != '}') {
                runtimeError(vm, "printf$ : Expected '}' in format");
                return false;
            }

            format++;
        }
        else {
            putchar(next);
            format++;
        }
    }

    returnNative(vm, argc, UNIT_VAL);
    return true;
}

bool printfnNative(VM* vm, int argc, Value* argv) {
    if (!IS_STRING(argv[0])) {
        runtimeError(vm, "printfn$ : Expected string, got %s", getValName(argv[0]));
        return false;
    }

    const char* format = AS_CSTRING(argv[0]);
    char* end;

    while (*format != '\0') {
        char next = *format;

        if (next == '{' && isIntChar(format[1])) {
            format++;

            long slot = strtol(format, &end, 10) + 1; //0-indexed

            if (slot > argc - 1) {
                runtimeError(vm, "printfn$ : Attempted to index out of args; got %d with %d args", slot, argc-1);
                return false;
            }

            printValue(argv[slot]);

            format = end;

            if (*format != '}') {
                runtimeError(vm, "printfn$ : Expected '}' in format");
                return false;
            }

            format++;
        }
        else {
            putchar(next);
            format++;
        }
    }

    putchar('\n');

    returnNative(vm, argc, UNIT_VAL);
    return true;
}


bool typeOfNative(VM* vm, int argc, Value* argv) {
    returnNative(vm, argc, INT_VAL(
        IS_OBJ(argv[0])
        ? (long long)(OBJ_TYPE(argv[0]) + VAL_OBJ)
        : (long long)(argv[0].type)
        )
    );
    return true;
}

bool lenNative(VM* vm, int argc, Value* argv) {
    if (!IS_OBJ(argv[0])) {
        runtimeError(vm, "len$ : Expected string or list, got %s", getValName(argv[0]));
        return false;
    }

    switch (OBJ_TYPE(argv[0])) {
    case OBJ_STRING:    returnNative(vm, argc, INT_VAL(AS_STRING(argv[0])->length)); return true;
    case OBJ_LIST:      returnNative(vm, argc, INT_VAL(ARRAY(argv[0]).count)); return true;
    default: runtimeError(vm, "len$ : Expected string or list, got %s", getValName(argv[0])); return false;
    }
}

bool applyNative(VM* vm, int argc, Value* argv) {
    if (!IS_CALLABLE(argv[0])) {
        runtimeError(vm, "apply$ : Expected callable, got %s", getValName(argv[0]));
        return false;
    }

    #ifdef DEBUG_DISPLAY_STACK
    for (Value* ptr = vm->stack; ptr < vm->stackTop; ptr++) {
        printValue(*ptr);
        printf(" | ");
    }
    printf("\n");
    #endif

    if (!callFromC(vm, argv[0], argc-1)) {
        return false;
    }

    returnNative(vm, 0, pop(vm));
    return true;
}

bool mapNative(VM* vm, int argc, Value* argv) {
    if (!IS_CALLABLE(argv[0])) {
        runtimeError(vm, "map$ : Expected callable, got %s", getValName(argv[0]));
        return false;
    }
    if (!IS_LIST(argv[1])) {
        runtimeError(vm, "map$ : Expected list, got %s", getValName(argv[1]));
        return false;
    }

    Value f = argv[0];
    Value l = argv[1];
    Value m = OBJ_VAL(newList(vm));

    push(vm, m);

    #ifdef DEBUG_DISPLAY_STACK
    for (Value* ptr = vm->stack; ptr < vm->stackTop; ptr++) {
        printValue(*ptr);
        printf(" | ");
    }
    printf("\n");
    #endif

    for (int i = 0; i < ARRAY(l).count; ++i) {
        push(vm, f);

        Value x = ARRAY(l).values[i];
        push(vm, x);

        if (!callFromC(vm, f, 1)) {
            return false;
        }

        Value y = peek(vm, 0);

        writeValueArray(vm, &ARRAY(m), y);

        pop(vm);
    }

    returnNative(vm, argc, pop(vm));

    return true;
}

bool filterNative(VM* vm, int argc, Value* argv) {
    if (!IS_CALLABLE(argv[0])) {
        runtimeError(vm, "filter$ : Expected callable, got %s", getValName(argv[0]));
        return false;
    }
    if (!IS_LIST(argv[1])) {
        runtimeError(vm, "filter$ : Expected list, got %s", getValName(argv[1]));
        return false;
    }

    Value f = argv[0];
    Value l = argv[1];
    Value m = OBJ_VAL(newList(vm));

    push(vm, m);

    #ifdef DEBUG_DISPLAY_STACK
    for (Value* ptr = vm->stack; ptr < vm->stackTop; ptr++) {
        printValue(*ptr);
        printf(" | ");
    }
    printf("\n");
    #endif

    for (int i = 0; i < ARRAY(l).count; ++i) {
        push(vm, f);

        Value x = ARRAY(l).values[i];
        push(vm, x);

        if (!callFromC(vm, f, 1)) {
            return false;
        }

        if (isTruthy(pop(vm))) {
            writeValueArray(vm, &ARRAY(m), x);
        }
    }

    returnNative(vm, argc, pop(vm));

    return true;
}

bool zipNative(VM* vm, int argc, Value* argv) {
    if (!IS_CALLABLE(argv[0])) {
        runtimeError(vm, "zip$ : Expected callable, got %s", getValName(argv[0]));
        return false;
    }
    if (!IS_LIST(argv[1]) || !IS_LIST(argv[2])) {
        runtimeError(vm, "zip$ : Expected lists, got %s and %s", getValName(argv[1]), getValName(argv[2]));
        return false;
    }

    Value f = argv[0];
    Value l1 = argv[1];
    Value l2 = argv[2];
    Value z = OBJ_VAL(newList(vm));

    push(vm, z);

    #ifdef DEBUG_DISPLAY_STACK
    for (Value* ptr = vm->stack; ptr < vm->stackTop; ptr++) {
        printValue(*ptr);
        printf(" | ");
    }
    printf("\n");
    #endif

    int min = ARRAY(l1).count < ARRAY(l2).count ? ARRAY(l1).count : ARRAY(l2).count;

    for (int i = 0; i < min; ++i) {
        push(vm, f);

        Value x = ARRAY(l1).values[i];
        push(vm, x);

        Value y = ARRAY(l2).values[i];
        push(vm, y);

        if (!callFromC(vm, f, 2)) {
            return false;
        }

        Value a = peek(vm, 0);

        writeValueArray(vm, &ARRAY(z), a);

        pop(vm);
    }

    returnNative(vm, argc, pop(vm));

    return true;
}

bool revNative(VM* vm, int argc, Value* argv) {
    Value to_reverse = argv[0];

    if (IS_LIST(to_reverse)) {
        ObjList* reversed = reverseList(vm, AS_LIST(to_reverse));
        returnNative(vm, argc, OBJ_VAL(reversed));
        return true;
    }
    else if (IS_STRING(to_reverse)) {
        ObjString* reversed = reverseString(vm, AS_STRING(to_reverse));
        returnNative(vm, argc, OBJ_VAL(reversed));
        return true;
    }
    else {
        runtimeError(vm, "rev$ : Expected string or list, got %s", getValName(to_reverse));
        return false;
    }
}

// Need to fix :: replace the weird bits with callFromC()
bool foldlNative(VM* vm, int argc, Value* argv) {
    if (!IS_CALLABLE(argv[0])) {
        runtimeError(vm, "foldl$ : Expected callable, got %s", getValName(argv[0]));
    }
    if (!IS_LIST(argv[1])) {
        runtimeError(vm, "foldl$ : Expected list, got %s", getValName(argv[1]));
        return false;
    }

    Value f = argv[0];
    Value l = argv[1];

    // get this sh*t started
    push(vm, f);

    push(vm, ARRAY(l).values[0]);
    push(vm, ARRAY(l).values[1]);

    #ifdef DEBUG_DISPLAY_STACK
    for (Value* ptr = vm->stack; ptr < vm->stackTop; ptr++) {
        printValue(*ptr);
        printf(" | ");
    }
    printf("\n");
    #endif

    if (!callFromC(vm, f, 2)) {
        return false;
    }

    Value x = pop(vm);

    for (int i = 2; i < ARRAY(l).count; ++i) {
        push(vm, f);

        Value y = ARRAY(l).values[i];
        push(vm, x);
        push(vm, y);

        if (!callFromC(vm, f, 2)) {
            return false;
        }

        x = pop(vm);
    }

    returnNative(vm, argc, x);

    return true;
}

bool foldrNative(VM* vm, int argc, Value* argv) {
    if (!IS_CALLABLE(argv[0])) {
        runtimeError(vm, "foldr$ : Expected callable, got %s", getValName(argv[0]));
    }
    if (!IS_LIST(argv[1])) {
        runtimeError(vm, "foldr$ : Expected list, got %s", getValName(argv[1]));
        return false;
    }

    Value f = argv[0];
    Value l = argv[1];

    // get this sh*t started
    push(vm, f);
    push(vm, ARRAY(l).values[ARRAY(l).count - 2]);
    push(vm, ARRAY(l).values[ARRAY(l).count - 1]);

    #ifdef DEBUG_DISPLAY_STACK
    for (Value* ptr = vm->stack; ptr < vm->stackTop; ptr++) {
        printValue(*ptr);
        printf(" | ");
    }
    printf("\n");
    #endif

    if (!callFromC(vm, f, 2)) {
        return false;
    }

    Value x = pop(vm);

    for (int i = ARRAY(l).count - 3; i >= 0; --i) {
        push(vm, f);

        Value y = ARRAY(l).values[i];
        push(vm, y);
        push(vm, x);

        if (!callFromC(vm, f, 2)) {
            return false;
        }

        x = pop(vm);
    }

    returnNative(vm, argc, x);

    return true;
}

void defineBuiltins(VM *vm)
{
    defineNative(vm, "clock", clockNative, 0);
    defineNative(vm, "exit", exitNative, 1);
    defineNative(vm, "printf", printfNative, -2);
    defineNative(vm, "printfn", printfnNative, -2);
    defineNative(vm, "typeOf", typeOfNative, 1);
    defineNative(vm, "len", lenNative, 1);
    defineNative(vm, "rev", revNative, 1);

    defineNative(vm, "map", mapNative, 2);
    defineNative(vm, "zip", zipNative, 3);
    defineNative(vm, "filter", filterNative, 2);
    defineNative(vm, "foldl", foldlNative, 2);
    defineNative(vm, "foldr", foldrNative, 2);
    defineNative(vm, "apply", applyNative, -2);
    defineNative(vm, "$", applyNative, -2);
}
