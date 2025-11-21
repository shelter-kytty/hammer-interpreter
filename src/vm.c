#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
#include <time.h>

#include "vm.h"

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"


/*
+=================+
| Control   vvvv  |
+-----------------+
*/

void runtimeError(VM* vm, const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm->frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm->frames[i];
        ObjFunction* function = frame->function;
        size_t instruction = frame->ip - function->body.code - 1;
        fprintf(stderr, "[ line %d ] in ", function->body.lines[instruction]);
        if (function->name == NULL) {
            fprintf(stderr, "script\n");
        }
        else {
            fprintf(stderr, "%s$\n", function->name->chars);
        }
    }

    // fprintf(stderr, "In syscall to '%s'\n", getInstructionName(*(currentFrame(vm)->ip - 1)));
    #ifdef DEBUG_DISPLAY_STACK
    for (Value* ptr = vm->stack; ptr < vm->stackTop; ptr++) {
        printValue(*ptr);
        printf(" | ");
    }
    printf("\n");
    #endif

    #ifdef DEBUG_DISPLAY_STRINGS
    printf("\nstrings:\n");
    printTable(&vm->strings);
    #endif

    #ifdef DEBUG_DISPLAY_TABLES
    printf("\nglobals:\n");
    printTable(&vm->globals);
    #endif
}

simple CallFrame* currentFrame(VM* vm) {
    return &vm->frames[vm->frameCount - 1];
}

simple void push(VM* vm, Value value) {
    *vm->stackTop = value;
    vm->stackTop++;
}

simple Value pop(VM* vm) {
    return *(--vm->stackTop);
}

simple Value peek(VM* vm, int distance) {
    return vm->stackTop[(-1) - distance];
}


/*
+---------------------+
| Control       ^^^^  |
+=====================+
| NativeFns     vvvv  |
+---------------------+
*/

static bool callValue(VM* vm, Value caller, uint8_t argCount);
static bool isTruthy(Value value);
InterpretResult run(VM* vm);

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

void returnNative(VM* vm, int argCount, Value result) {
    vm->stackTop -= argCount + 1;
    push(vm, result);
}

void defineNative(VM* vm, const char* name, NativeFn function, int arity) {
    Value key = OBJ_VAL(copyString(vm, name, (int)strlen(name)));
    Value value = OBJ_VAL(newNative(vm, function, arity));
    tableAddEntry(vm, &vm->globals, AS_STRING(key), value);
}


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

// Change format for printf/n? currently '{n}' is the format but this means you cant just, put a number
// between braces. I think something like the C format with a single, simple preceding character could
// work, could use '%n' or similar. Would allow for escaping the format more easily.
static inline bool isIntChar(char ch) {
    return ch >= '0' && ch <= '9';
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

bool addOperator(VM* vm, int argc, Value* argv) {
    Value a = argv[0];
    Value b = argv[1];
    Value c;

    if (!IS_ARITH(a) || !IS_ARITH(b)) {
        runtimeError(vm, "ADD : Cannot perform op on %s and %s", getValName(a), getValName(b));
        return false;
    }

    if (TYPES_EQUAL(a, b)) {
        if (IS_INT(a)) {
            c = INT_VAL(AS_INT(a) + AS_INT(b));
        }
        else {
            c = FLOAT_VAL(AS_FLOAT(a) + AS_FLOAT(b));
        }
    }
    else {
        if (IS_INT(a)) {
            c = FLOAT_VAL(AS_INT(a) + AS_FLOAT(b));
        }
        else {
            c = FLOAT_VAL(AS_FLOAT(a) + AS_INT(b));
        }
    }

    returnNative(vm, argc, c);
    return true;
}

bool subOperator(VM* vm, int argc, Value* argv) {
    Value a = argv[0];
    Value b = argv[1];
    Value c;

    if (!IS_ARITH(a) || !IS_ARITH(b)) {
        runtimeError(vm, "SUB : Cannot perform op on %s and %s", getValName(a), getValName(b));
        return false;
    }

    if (TYPES_EQUAL(a, b)) {
        if (IS_INT(a)) {
            c = INT_VAL(AS_INT(a) - AS_INT(b));
        }
        else {
            c = FLOAT_VAL(AS_FLOAT(a) - AS_FLOAT(b));
        }
    }
    else {
        if (IS_INT(a)) {
            c = FLOAT_VAL(AS_INT(a) - AS_FLOAT(b));
        }
        else {
            c = FLOAT_VAL(AS_FLOAT(a) - AS_INT(b));
        }
    }

    returnNative(vm, argc, c);
    return true;
}

bool mulOperator(VM* vm, int argc, Value* argv) {
    Value a = argv[0];
    Value b = argv[1];
    Value c;

    if (!IS_ARITH(a) || !IS_ARITH(b)) {
        runtimeError(vm, "MUL : Cannot perform op on %s and %s", getValName(a), getValName(b));
        return false;
    }

    if (TYPES_EQUAL(a, b)) {
        if (IS_INT(a)) {
            c = INT_VAL(AS_INT(a) * AS_INT(b));
        }
        else {
            c = FLOAT_VAL(AS_FLOAT(a) * AS_FLOAT(b));
        }
    }
    else {
        if (IS_INT(a)) {
            c = FLOAT_VAL(AS_INT(a) * AS_FLOAT(b));
        }
        else {
            c = FLOAT_VAL(AS_FLOAT(a) * AS_INT(b));
        }
    }

    returnNative(vm, argc, c);
    return true;
}

bool divOperator(VM* vm, int argc, Value* argv) {
    Value a = argv[0];
    Value b = argv[1];
    Value c;

    if (!IS_ARITH(a) || !IS_ARITH(b)) {
        runtimeError(vm, "DIV : Cannot perform op on %s and %s", getValName(a), getValName(b));
        return false;
    }

    if (TYPES_EQUAL(a, b)) {
        if (IS_INT(a)) {
            c = INT_VAL(AS_INT(a) / AS_INT(b));
        }
        else {
            c = FLOAT_VAL(AS_FLOAT(a) / AS_FLOAT(b));
        }
    }
    else {
        if (IS_INT(a)) {
            c = FLOAT_VAL(AS_INT(a) / AS_FLOAT(b));
        }
        else {
            c = FLOAT_VAL(AS_FLOAT(a) / AS_INT(b));
        }
    }

    returnNative(vm, argc, c);
    return true;
}

bool modOperator(VM* vm, int argc, Value* argv) {
    Value a = argv[0];
    Value b = argv[1];
    Value c;

    if (!IS_ARITH(a) || !IS_ARITH(b)) {
        runtimeError(vm, "POW : Cannot perform op on %s and %s", getValName(a), getValName(b));
        return false;
    }

    if (TYPES_EQUAL(a, b)) {
        if (IS_INT(a)) {
            c = INT_VAL(fmodl(AS_INT(a), AS_INT(b)));
        }
        else {
            c = FLOAT_VAL(fmod(AS_FLOAT(a), AS_FLOAT(b)));
        }
    }
    else {
        if (IS_INT(a)) {
            c = FLOAT_VAL(fmod((double)AS_INT(a), AS_FLOAT(b)));
        }
        else {
            c = FLOAT_VAL(fmod(AS_FLOAT(a), (double)AS_INT(b)));
        }
    }

    returnNative(vm, argc, c);
    return true;
}

bool powOperator(VM* vm, int argc, Value* argv) {
    Value b = argv[0];
    Value a = argv[1];
    Value c;

    if (!IS_ARITH(a) || !IS_ARITH(b)) {
        runtimeError(vm, "MOD : Cannot perform op on %s and %s", getValName(a), getValName(b));
        return false;
    }

    if (TYPES_EQUAL(a, b)) {
        if (IS_INT(a)) {
            c = INT_VAL(powl(AS_INT(a), AS_INT(b)));
        }
        else {
            c = FLOAT_VAL(powf(AS_FLOAT(a), AS_FLOAT(b)));
        }
    }
    else {
        if (IS_INT(a)) {
            c = FLOAT_VAL(powf((double)AS_INT(a), AS_FLOAT(b)));
        }
        else {
            c = FLOAT_VAL(powf(AS_FLOAT(a), (double)AS_INT(b)));
        }
    }

    returnNative(vm, argc, c);
    return true;
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

/*
+---------------------+
| NativeFns     ^^^^  |
+=====================+
| Vm            vvvv  |
+---------------------+
*/


void initVM(VM* vm) {
    vm->frameCount = 0;
    vm->compiler = NULL;
    vm->stackTop = vm->stack;

    vm->objects = NULL;
    vm->isActive = false;
    vm->greyStart = NULL;
    vm->greyEnd = NULL;
    vm->bytesAllocated = 0;
    vm->nextGC = 500000;

    initTable(&vm->strings);
    initTable(&vm->globals);

    // stdlib
    defineNative(vm, "clock", clockNative, 0);
    defineNative(vm, "exit", exitNative, 1);
    defineNative(vm, "printf", printfNative, -2);
    defineNative(vm, "printfn", printfnNative, -2);
    defineNative(vm, "typeOf", typeOfNative, 1);
    defineNative(vm, "len", lenNative, 1);
    defineNative(vm, "rev", revNative, 1);

    // hofs
    defineNative(vm, "map", mapNative, 2);
    defineNative(vm, "zip", zipNative, 3);
    defineNative(vm, "filter", filterNative, 2);
    defineNative(vm, "foldl", foldlNative, 2);
    defineNative(vm, "foldr", foldrNative, 2);
    defineNative(vm, "apply", applyNative, -2);

    defineNative(vm, "+", addOperator, 2);
    defineNative(vm, "-", subOperator, 2);
    defineNative(vm, "*", mulOperator, 2);
    defineNative(vm, "/", divOperator, 2);
    defineNative(vm, "%", modOperator, 2);
    defineNative(vm, "^", powOperator, 2);
    defineNative(vm, "$", applyNative, -2);
}

void freeVM(VM* vm) {
    #ifdef DEBUG_LOG_MEMORY
    printf("Ended with %zu bytes allocated with a threshold of %zu\n", vm->bytesAllocated, vm->nextGC);
    #endif
    freeObjects(vm);
    freeTable(vm, &vm->globals);
    freeTable(vm, &vm->strings);
    vm->compiler = NULL;
    vm->objects = NULL;
    vm->frameCount = 0;
    vm->isActive = false;
    vm->greyStart = NULL;
    vm->greyEnd = NULL;
    vm->bytesAllocated = 0;
}


/*
+---------------------+
| Vm            ^^^^  |
+=====================+
| Functionality vvvv  |
+---------------------+
*/


static bool isTruthy(Value value) {
    switch (value.type) {
        case VAL_UNIT:      return false;
        case VAL_BOOL:      return AS_BOOL(value);
        default: return true;
        /*
        case VAL_INT:       return AS_INT(value)    != 0;
        case VAL_FLOAT:     return AS_FLOAT(value)  != 0.0;
        case VAL_CHAR:      return AS_CHAR(value)   != '\0';
        case VAL_OBJ: {
            switch (OBJ_TYPE(value)) {
            case OBJ_STRING: return AS_STRING(value)->length > 0;
            case OBJ_CELL:
                #ifdef OPTION_RECURSIVE_TRUTHINESS
                return isTruthy(CAR(value)) || isTruthy(CDR(value));
                #endif
            case OBJ_FUNCTION:
            case OBJ_CLOSURE:
            case OBJ_NATIVE:
                return true;
            }
        }
        */
    }

    return false;
}

static ObjString* concatStrings(VM* vm, ObjString* a, ObjString* b) {
    int new_length = a->length + b->length;
    char* heapChars = ALLOCATE(vm, new_length + 1, char);
    memcpy(heapChars, a->chars, a->length);
    memcpy(heapChars + a->length, b->chars, b->length);
    heapChars[new_length] = '\0';

    return takeString(vm, heapChars, new_length);
}

static ObjList* concatLists(VM* vm, ValueArray* a, ValueArray* b) {
    ObjList* list = newList(vm);

    // Also GC
    push(vm, OBJ_VAL(list));

    for (int i = 0; i < a->count; ++i) {
        writeValueArray(vm, &list->array, a->values[i]);
    }

    for (int i = 0; i < b->count; ++i) {
        writeValueArray(vm, &list->array, b->values[i]);
    }

    pop(vm);

    return list;
}

static ObjList* fromRange(VM* vm, long long a, long long b) {
    ObjList* list = newList(vm);

    // GC
    push(vm, OBJ_VAL(list));

    long long C = a;

    if (a > b) {
        while (C >= b) {
            writeValueArray(vm, &list->array, INT_VAL(C--));
        }
    }
    else if (a < b) {
        while (C <= b) {
            writeValueArray(vm, &list->array, INT_VAL(C++)); //🤯
        }
    }
    else {
        writeValueArray(vm, &list->array, INT_VAL(C));
    }

    pop(vm);

    return list;
}

static bool callFunc(VM* vm, ObjFunction* func, uint8_t argCount) {
    if (argCount != func->arity) {
        runtimeError(vm, "CALL : %s takes %d args, but got %d", func->name != NULL ? func->name->chars : "<lmbd>", func->arity, argCount);
        return false;
    }

    if (vm->frameCount + 1 >= FRAME_MAX) {
        runtimeError(vm, "CALL : Encountered stack overflow");
        return false;
    }

    vm->frameCount++;
    CallFrame* frame = currentFrame(vm);

    frame->function = func;
    frame->ip       = func->body.code;
    frame->slots    = vm->stackTop - argCount;
    frame->closure  = NULL;
    frame->isCHOF   = false;

    return true;
}

static bool callNative(VM* vm, ObjNative* func, uint8_t argCount) {
    if (func->arity < 0) {
        if (argCount < abs(func->arity) - 1) {
            runtimeError(vm, "CALL : Expected at least %d args, but got %d", abs(func->arity) - 1, argCount);
            return false;
        }
    }
    else if (argCount != func->arity) {
        runtimeError(vm, "CALL : Expected %d args, but got %d", func->arity, argCount);
        return false;
    }

    NativeFn native = func->function;
    bool result = native(vm, argCount, vm->stackTop - argCount);
    return result;
}

static bool callClosure(VM* vm, ObjClosure* closure, uint8_t argCount) {
    if (argCount != closure->function->arity) {
        runtimeError(vm, "CALL : Expected %d args, but got %d", closure->function->arity, argCount);
        return false;
    }

    if (vm->frameCount + 1 >= FRAME_MAX) {
        runtimeError(vm, "CALL : Encountered stack overflow");
        return false;
    }

    vm->frameCount++;
    CallFrame* frame = currentFrame(vm);

    frame->function = closure->function;
    frame->ip       = closure->function->body.code;
    frame->slots    = vm->stackTop - argCount;
    frame->closure  = closure;
    frame->isCHOF   = false;

    return true;
}

static bool callValue(VM* vm, Value caller, uint8_t argCount) {
    if (IS_FUNC(caller)) {
        return callFunc(vm, AS_FUNC(caller), argCount);
    }
    else if (IS_NATIVE(caller)) {
        return callNative(vm, AS_NATIVE(caller), argCount);
    }
    else if (IS_CLOSURE(caller)) {
        return callClosure(vm, AS_CLOSURE(caller), argCount);
    }
    else {
        runtimeError(vm, "CALL : Expected function, got %s", getValName(caller));
        return false;
    }
}

static bool retrieveUpvalue(VM* vm, uint8_t depth, Value* returnal) {
    ObjClosure* closure = currentFrame(vm)->closure;

    if (closure == NULL) {
        const char* name = currentFrame(vm)->function->name->chars;
        runtimeError(vm, "GET : No upvalues; %s is not a closure", name != NULL ? name : "<lmbd>");
        return false;
    }

    for (int i = 0; i < closure->upvalueCount; i++) {
        if (closure->depths[i] == depth) {
            *returnal = closure->upvalues[i];
            return true;
        }
    }

    runtimeError(vm, "GET : Could not find upvalue");
    return false;
}

static bool compareTrees(VM* vm, Value a, Value b) {
    bool flagA = true;
    bool flagB = true;

    if (IS_BOOL(CAR(b)) && AS_BOOL(CAR(b)) == true) {
        push(vm, CAR(a));
    }
    else if (IS_CELL(CAR(b))) {
        if (IS_CELL(CAR(a))) {
            flagA = compareTrees(vm, CAR(a), CAR(b));
        }
        else {
            runtimeError(vm, "DECONS : trees do not align; expected pair");
            flagA = false;
        }
    }

    if (IS_BOOL(CDR(b)) && AS_BOOL(CDR(b)) == true) {
        push(vm, CDR(a));
    }
    else if (IS_CELL(CDR(b))) {
        if (IS_CELL(CDR(a))) {
            flagB = compareTrees(vm, CDR(a), CDR(b));
        }
        else {
            runtimeError(vm, "DECONS : trees do not align; expected pair");
            flagB = false;
        }
    }

    return flagA && flagB;
}

static void subscriptList(VM* vm, ObjList* list, long long index, uint8_t offset) {
    pop(vm);
    pop(vm);

    if (index > list->array.count - (1 - offset) || index < -(list->array.count - offset)) {
        push(vm, UNIT_VAL);
    }
    else if (index < offset && index > -(list->array.count - offset)) {
        push(vm, list->array.values[(list->array.count + index) - offset]);
    }
    else {
        push(vm, list->array.values[index - offset]);
    }
}

static void subscriptString(VM* vm, ObjString* string, long long index, uint8_t offset) {
    pop(vm);
    pop(vm);

    if (index > string->length - (1 - offset) || index < -(string->length - offset)) {
        push(vm, UNIT_VAL);
    }
    else if (index < offset && index > -(string->length - offset)) {
        push(vm, CHAR_VAL(string->chars[(string->length + index) - offset]));
    }
    else {
        push(vm, CHAR_VAL(string->chars[index - offset]));
    }
}

static void popAndPushInSequence(VM* vm, int count) {
    Value a = pop(vm);

    if (count > 0) {
        popAndPushInSequence(vm, count - 1);
    }
    else {
        while (vm->stackTop > (currentFrame(vm)->slots - 1)) {
            pop(vm);
        }
        vm->frameCount--;
    }

    push(vm, a);
}

static int getArity(VM* vm, Value fn) {
    switch (OBJ_TYPE(fn)) {
    case OBJ_FUNCTION: return AS_FUNC(fn)->arity;
    case OBJ_CLOSURE:  return CLOSED_FN(fn)->arity;
    case OBJ_NATIVE: {
        ObjNative* native = AS_NATIVE(fn);
        if (native->arity >= 0 && native->arity <= UINT8_MAX) {
            return native->arity;
        }
        else {
            runtimeError(vm, "COMPOSE : Cannot represent variadic natives with functions");
            return -1;
        }
    }
    default:
        runtimeError(vm,  "COMPOSE : Expected function, got %s", getValName(fn));
        return -1;
    }
}

static bool sliceList(VM* vm, ObjList* list, long long x, long long y) {
    if (x > list->array.count || y > list->array.count || x < 0 || y < 0) {
        runtimeError(vm, "SLICE : Index was outside of list; length was %d, got indeces %d , %d", list->array.count, x, y);
        return false;
    }

    ObjList* new = newList(vm);
    push(vm, OBJ_VAL(new));

    for (int i = x; i <= y; i++) {
        writeValueArray(vm, &new->array, list->array.values[i]);
    }

    return true;
}

static bool sliceString(VM* vm, ObjString* string, long long x, long long y) {
    if (x > string->length || y > string->length || x < 0 || y < 0) {
        runtimeError(vm, "SLICE : Index was outside of string; length was %d, got indeces %d , %d", string->length, x, y);
        return false;
    }

    push(vm, OBJ_VAL(copyString(vm, (string->chars + x), (y - x) + 1)));

    return true;
}

static bool sliceArray(VM* vm, Value array, long long x, long long y) {
    if (IS_LIST(array)) {
        return sliceList(vm, AS_LIST(array), x, y);
    }
    else {
        return sliceString(vm, AS_STRING(array), x, y);
    }
}

/*
+---------------------+
| Functionality ^^^^  |
+=====================+
| Runtime       vvvv  |
+---------------------+
*/

InterpretResult run(VM* vm) {
#define READ_BYTE(v)    (*currentFrame(vm)->ip++)
#define READ_SHORT(v) \
    (currentFrame(v)->ip += 2, (uint16_t)((currentFrame(v)->ip[-2] << 8) | currentFrame(v)->ip[-1]))
#define READ_CONST(v, i)    (currentFrame(vm)->function->body.constants.values[i])

#define BINARY_OP(v, op)                                                                                \
    do {                                                                                                \
        Value b = pop(v);                                                                               \
        Value a = pop(v);                                                                               \
        if (!IS_ARITH(a) || !IS_ARITH(b)) {                                                             \
            runtimeError(v, "ARITH : Cannot perform op on %s and %s", getValName(a), getValName(b));    \
            return INTERPRET_RUNTIME_ERROR;                                                             \
        }                                                                                               \
        if (TYPES_EQUAL(a, b)) {                                                                        \
            if (IS_INT(a)) {                                                                            \
                push(v, INT_VAL(AS_INT(a) op AS_INT(b)));                                               \
            }                                                                                           \
            else {                                                                                      \
                push(v, FLOAT_VAL(AS_FLOAT(a) op AS_FLOAT(b)));                                         \
            }                                                                                           \
        }                                                                                               \
        else {                                                                                          \
            if (IS_INT(a)) {                                                                            \
                push(v, FLOAT_VAL(AS_INT(a) op AS_FLOAT(b)));                                           \
            }                                                                                           \
            else {                                                                                      \
                push(v, FLOAT_VAL(AS_FLOAT(a) op AS_INT(b)));                                           \
            }                                                                                           \
        }                                                                                               \
    } while (0)

    for (;;) {
        #ifdef DEBUG_DISPLAY_INSTRUCTIONS
        disassembleInstruction(&currentFrame(vm)->function->body,
            currentFrame(vm)->ip - currentFrame(vm)->function->body.code);
        #endif

        uint8_t instr = READ_BYTE(vm);

        switch(instr) {
            case OP_RETURN: {

                // Add a check to see if the run() call is being controlled by a C-based HOF or
                // some other C function that isn't interpret(), and return if it is.
                if (vm->frameCount - 1 > 0) {
                    Value result = pop(vm);

                    while (vm->stackTop > (currentFrame(vm)->slots - 1)) {
                        pop(vm);
                    }

                    push(vm, result);

                    if (currentFrame(vm)->isCHOF) {
                        vm->frameCount--;
                        return INTERPRET_OK;
                    } // smthn like that

                    vm->frameCount--;

                    break;
                }


                #ifdef DEBUG_DISPLAY_INSTRUCTIONS
                printf("\n");
                #endif

                pop(vm); // the final UNIT, must be cleaned up for repl
                vm->frameCount--; // remove top-level function frame (oops <w>)
                return INTERPRET_OK;
            }
            case OP_TAIL_CALL: {
                uint8_t count = READ_BYTE(vm);
                bool isCHigherOrderFunction = currentFrame(vm)->isCHOF;
                bool isNative = IS_NATIVE(peek(vm, count));

                popAndPushInSequence(vm, count);

                if (!callValue(vm, peek(vm, count), count)) {
                    return INTERPRET_RUNTIME_ERROR;
                }

                // Native functions don't hit an OP_RETURN so
                // the eval loop needs to exit here.
                if (isNative && isCHigherOrderFunction) {
                    #ifdef DEBUG_DISPLAY_STACK
                    for (Value* ptr = vm->stack; ptr < vm->stackTop; ptr++) {
                        printf("[ ");
                        printValue(*ptr);
                        printf(" ]");
                    }
                    printf("\n");
                    #endif
                    return INTERPRET_OK;
                }

                // Tail call CallFrames inherit the isCHOF flag
                // otherwise they will not exit properly on their return.
                currentFrame(vm)->isCHOF = isCHigherOrderFunction;

                break;
            }
            case OP_POP: {
                pop(vm);
                break;
            }
            case OP_RETURN_SCOPE: {
                uint8_t count = READ_BYTE(vm);

                Value result = pop(vm);

                while (count > 0) {
                    pop(vm);
                    count--;
                }

                push(vm, result);

                break;
            }
            case OP_DUPE_TOP: {
                push(vm, peek(vm, 0));
                break;
            }
            case OP_LOADV: {
                uint8_t slot = READ_BYTE(vm);
                push(vm, READ_CONST(vm, slot));
                break;
            }
            case OP_TRUE: {
                push(vm, BOOL_VAL(true));
                break;
            }
            case OP_FALSE: {
                push(vm, BOOL_VAL(false));
                break;
            }
            case OP_UNIT: {
                push(vm, UNIT_VAL);
                break;
            }
            case OP_NOT: {
                push(vm, BOOL_VAL(!isTruthy(pop(vm))));
                break;
            }
            case OP_TRUTHY: {
                push(vm, BOOL_VAL(!valuesEqual(pop(vm), UNIT_VAL)));
                break;
            }
            case OP_NEGATE: {
                if (!IS_ARITH(peek(vm, 0))) {
                    runtimeError(vm, "MINUS : Cannot negate %s", getValName(peek(vm, 0)));
                    return INTERPRET_RUNTIME_ERROR;
                }

                Value value = pop(vm);

                if (IS_INT(value)) {
                    push(vm, INT_VAL(-(AS_INT(value))));
                }
                else {
                    push(vm, FLOAT_VAL(-(AS_FLOAT(value))));
                }

                break;
            }
            case OP_ADD:        BINARY_OP(vm, + ); break;
            case OP_SUBTRACT:   BINARY_OP(vm, - ); break;
            case OP_MULTIPLY:   BINARY_OP(vm, * ); break;
            case OP_DIVIDE:     BINARY_OP(vm, / ); break;
            case OP_MODULO: {
                Value b = pop(vm);
                Value a = pop(vm);

                if (!IS_ARITH(a) || !IS_ARITH(b)) {
                    runtimeError(vm, "MOD : Cannot perform op on %s and %s", getValName(a), getValName(b));
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (TYPES_EQUAL(a, b)) {
                    if (IS_INT(a)) {
                        push(vm, INT_VAL(fmodl(AS_INT(a), AS_INT(b))));
                    }
                    else {
                        push(vm, FLOAT_VAL(fmod(AS_FLOAT(a), AS_FLOAT(b))));
                    }
                }
                else {
                    if (IS_INT(a)) {
                        push(vm, FLOAT_VAL(fmod((double)AS_INT(a), AS_FLOAT(b))));
                    }
                    else {
                        push(vm, FLOAT_VAL(fmod(AS_FLOAT(a), (double)AS_INT(b))));
                    }
                }

                break;
            }
            case OP_EXPONENT: {
                Value b = pop(vm);
                Value a = pop(vm);

                if (!IS_ARITH(a) || !IS_ARITH(b)) {
                    runtimeError(vm, "POW : Cannot perform op on %s and %s", getValName(a), getValName(b));
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (TYPES_EQUAL(a, b)) {
                    if (IS_INT(a)) {
                        push(vm, INT_VAL(powl(AS_INT(a), AS_INT(b))));
                    }
                    else {
                        push(vm, FLOAT_VAL(powf(AS_FLOAT(a), AS_FLOAT(b))));
                    }
                }
                else {
                    if (IS_INT(a)) {
                        push(vm, FLOAT_VAL(powf((double)AS_INT(a), AS_FLOAT(b))));
                    }
                    else {
                        push(vm, FLOAT_VAL(powf(AS_FLOAT(a), (double)AS_INT(b))));
                    }
                }

                break;
            }
            case OP_DIFF: {
                Value b = pop(vm);
                Value a = pop(vm);

                if (!IS_ARITH(a) || !IS_ARITH(b)) {
                    runtimeError(vm, "DIFF : Cannot perform op on %s and %s", getValName(a), getValName(b));
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (TYPES_EQUAL(a, b)) {
                    if (IS_INT(a)) {
                        push(vm, BOOL_VAL(AS_INT(a) > AS_INT(b)));
                    }
                    else {
                        push(vm, BOOL_VAL(AS_FLOAT(a) > AS_FLOAT(b)));
                    }
                }
                else {
                    if (IS_INT(a)) {
                        push(vm, BOOL_VAL(AS_INT(a) > AS_FLOAT(b)));
                    }
                    else {
                        push(vm, BOOL_VAL(AS_FLOAT(a) > AS_INT(b)));
                    }
                }

                break;
            }
            case OP_DIFFEQ: {
                Value b = pop(vm);
                Value a = pop(vm);

                if (!IS_ARITH(a) || !IS_ARITH(b)) {
                    runtimeError(vm, "DIFFEQ : Cannot perform op on %s and %s", getValName(a), getValName(b));
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (TYPES_EQUAL(a, b)) {
                    if (IS_INT(a)) {
                        push(vm, BOOL_VAL(AS_INT(a) >= AS_INT(b)));
                    }
                    else {
                        push(vm, BOOL_VAL(AS_FLOAT(a) >= AS_FLOAT(b)));
                    }
                }
                else {
                    if (IS_INT(a)) {
                        push(vm, BOOL_VAL(AS_INT(a) >= AS_FLOAT(b)));
                    }
                    else {
                        push(vm, BOOL_VAL(AS_FLOAT(a) >= AS_INT(b)));
                    }
                }

                break;
            }
            case OP_EQUALS: {
                push(vm, BOOL_VAL(valuesEqual(pop(vm), pop(vm))));
                break;
            }
            case OP_CONSTRUCT: {
                Value b = peek(vm, 0);
                Value a = peek(vm, 1);

                ObjCell* cell = newCell(vm);

                cell->car = a;
                cell->cdr = b;

                pop(vm); // b
                pop(vm); // a

                push(vm, OBJ_VAL(cell));

                break;
            }
            case OP_CAR: {
                if (!IS_CELL(peek(vm, 0))) {
                    runtimeError(vm, "CAR : Cannot extract car from %s", getValName(peek(vm, 0)));
                    return INTERPRET_RUNTIME_ERROR;
                }

                Value value = pop(vm);

                push(vm, CAR(value));

                break;
            }
            case OP_CDR: {
                if (!IS_CELL(peek(vm, 0))) {
                    runtimeError(vm, "CDR : Cannot extract cdr from %s", getValName(peek(vm, 0)));
                    return INTERPRET_RUNTIME_ERROR;
                }

                Value value = pop(vm);

                push(vm, CDR(value));

                break;
            }
            case OP_CONCAT: {
                if (!TYPES_EQUAL(peek(vm, 0), peek(vm, 1))) {
                    runtimeError(vm, "CONCAT : Cannot concatenate %s and %s", getValName(peek(vm, 0)), getValName(peek(vm, 1)));
                    return INTERPRET_RUNTIME_ERROR;
                }

                Value b = peek(vm, 0);
                Value a = peek(vm, 1);

                if (IS_STRING(a)) {
                    ObjString* c = concatStrings(vm, AS_STRING(a), AS_STRING(b));

                    pop(vm); // b
                    pop(vm); // a

                    push(vm, OBJ_VAL(c));
                }
                else if (IS_LIST(a)) {
                    ObjList* c = concatLists(vm, &ARRAY(a), &ARRAY(b));

                    pop(vm); // b
                    pop(vm); // a

                    push(vm, OBJ_VAL(c));
                }
                else if (IS_INT(a)) {
                    ObjList* c = fromRange(vm, AS_INT(a), AS_INT(b));

                    pop(vm); // b
                    pop(vm); // a

                    push(vm, OBJ_VAL(c));
                }
                else {
                    runtimeError(vm, "CONCAT : Cannot concatenate %s and %s", getValName(a), getValName(b));
                    return INTERPRET_RUNTIME_ERROR;
                }

                break;
            }
            case OP_MAKE_GLOBAL: {
                Value value = peek(vm, 0);
                Value key = READ_CONST(vm, READ_BYTE(vm));

                if (!tableAddEntry(vm, &vm->globals, AS_STRING(key), value)) {
                    runtimeError(vm, "MAKE : Binding '%s' already exists", AS_CSTRING(key));
                    return INTERPRET_RUNTIME_ERROR;
                }

                break;
            }
            case OP_GET_GLOBAL: {
                Value key = READ_CONST(vm, READ_BYTE(vm));
                Entry* entry = tableGetEntry(&vm->globals, AS_STRING(key));

                if (entry == NULL) {
                    runtimeError(vm, "GET : Binding '%s' does not exist", AS_CSTRING(key));
                    return INTERPRET_RUNTIME_ERROR;
                }

                push(vm, entry->value);

                break;
            }
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE(vm);
                push(vm, currentFrame(vm)->slots[slot]);
                break;
            }
            case OP_JUMP_IF_TRUE: {
                uint16_t spot = READ_SHORT(vm);
                if (isTruthy(peek(vm, 0))) {
                    currentFrame(vm)->ip += spot;
                }
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t spot = READ_SHORT(vm);
                if (!isTruthy(peek(vm, 0))) {
                    currentFrame(vm)->ip += spot;
                }
                break;
            }
            case OP_JUMP: {
                uint16_t spot = READ_SHORT(vm);
                currentFrame(vm)->ip += spot;
                break;
            }
            case OP_CALL: {
                uint8_t depth = READ_BYTE(vm);
                if (!IS_OBJ(peek(vm, depth))) {
                    runtimeError(vm, "CALL : Expected function, got %s", getValName(peek(vm, depth)));
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (!callValue(vm, peek(vm, depth), depth)) {
                    return INTERPRET_RUNTIME_ERROR;
                }

                break;
            }
            case OP_UPVALUE: {
                uint8_t depth = READ_BYTE(vm);

                Value upvalue;

                if (!retrieveUpvalue(vm, depth, &upvalue)) {
                    return INTERPRET_RUNTIME_ERROR;
                }

                push(vm, upvalue);

                break;
            }
            case OP_CLOSURE: {
                uint8_t count = READ_BYTE(vm);
                if (!IS_FUNC(peek(vm, 0))) {
                    runtimeError(vm, "CLOSURE : Expected function, got %s", getValName(peek(vm, 0)));
                    return INTERPRET_RUNTIME_ERROR;
                }

                // recursive local functions close over themselves, so the closure
                // needs to be on the stack
                Value func = peek(vm, 0);
                ObjClosure* closure = newClosure(vm, AS_FUNC(func), count);
                pop(vm);
                push(vm, OBJ_VAL(closure));

                ObjClosure* current = currentFrame(vm)->closure;


                for (int i = 0; i < count; i++) {
                    bool isLocal = (bool)READ_BYTE(vm);
                    uint8_t depth = READ_BYTE(vm);

                    if (isLocal) {
                        closure->upvalues[i] = currentFrame(vm)->slots[depth];
                    } else {
                        for (int j = 0; j < current->upvalueCount; j++) {
                            if (current->depths[j] == depth){
                                closure->upvalues[i] = current->upvalues[j];
                            }
                        }
                    }

                    closure->depths[i] = (uint8_t)i;
                }


                break;
            }
            case OP_DECONS: {
                if (!IS_CELL(peek(vm, 0))) {
                    runtimeError(vm, "DECONS : Cannot decons %s", getValName(peek(vm, 0)));
                    return INTERPRET_RUNTIME_ERROR;
                }

                Value cell = pop(vm);

                push(vm, CAR(cell));
                push(vm, CDR(cell));

                break;
            }
            case OP_TREE_COMP: {
                if (!IS_CELL(peek(vm, 0))) {
                    runtimeError(vm, "DECONS : Cannot decons %s", getValName(peek(vm, 0)));
                    return INTERPRET_RUNTIME_ERROR;
                }

                Value b = READ_CONST(vm, READ_BYTE(vm));
                Value a = pop(vm);

                if (!compareTrees(vm, a, b)) {
                    return INTERPRET_RUNTIME_ERROR;
                }

                break;
            }
            case OP_LIST: {
                uint8_t count = READ_BYTE(vm);
                ObjList* list = newList(vm);

                push(vm, OBJ_VAL(list));

                uint8_t i = count + 1;
                while (i - 1) {
                    writeValueArray(vm, &list->array, peek(vm, --i));
                }

                i = count + 1;
                while (i) {
                    pop(vm);
                    --i;
                }

                push(vm, OBJ_VAL(list));

                break;
            }
            case OP_MAP: {
                uint8_t count = READ_BYTE(vm);
                ObjMap* map = newMap(vm);

                push(vm, OBJ_VAL(map));

                uint8_t i = (count*2) + 1;
                while (i - 1) {
                    Value key = peek(vm, --i);
                    Value val = peek(vm, --i);

                    if (!IS_STRING(key)) {
                        runtimeError(vm, "MAP : Expected string. got %s", getValName(key));
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    if (!tableAddEntry(vm, &map->table, AS_STRING(key), val)) {
                        runtimeError(vm, "MAP : Key %s is already in map", AS_CSTRING(key));
                        return INTERPRET_RUNTIME_ERROR;
                    }
                }

                i = (count*2) + 1;
                while (i) {
                    pop(vm);
                    --i;
                }

                push(vm, OBJ_VAL(map));

                break;
            }
            case OP_SUBSCRIPT: {
                Value thing = peek(vm, 1);
                Value index = peek(vm, 0);

                if (IS_MAP(thing)) {
                    if (!IS_STRING(index)) {
                        runtimeError(vm, "SUBSCRIPT : Expected string, got %s", getValName(index));
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    Entry* entry = tableGetEntry(&TABLE(thing), AS_STRING(index));

                    pop(vm);
                    pop(vm);

                    if (entry == NULL) {
                        push(vm, UNIT_VAL);
                    }
                    else {
                        push(vm, entry->value);
                    }

                    break;
                }

                uint8_t offset;
                #ifdef OPTION_ONE_INDEXED
                offset = 1;
                #else
                offset = 0;
                #endif

                if (!IS_INT(index)) {
                    runtimeError(vm, "SUBSCRIPT : Expected integer, got %s", getValName(index));
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (IS_LIST(thing)) {
                    subscriptList(vm, AS_LIST(thing), AS_INT(index), offset);
                }
                else if (IS_STRING(thing)) {
                    subscriptString(vm, AS_STRING(thing), AS_INT(index), offset);
                }
                else {
                    runtimeError(vm, "SUBSCRIPT : %s is not subscriptable", getValName(thing));
                    return INTERPRET_RUNTIME_ERROR;
                }

                break;
            }
            case OP_RECEIVE: {
                Value value = peek(vm, 0);
                Value array = peek(vm, 1);

                if (IS_LIST(array)) {

                    ObjList* list = AS_LIST(array);

                    writeValueArray(vm, &list->array, value);

                    pop(vm);
                }
                else if (IS_MAP(array)) {
                    if (!IS_CELL(value)) {
                        runtimeError(vm, "RECEIVE : Expected k, v pair, got %s", getValName(value));
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    if (!IS_STRING(CAR(value))) {
                        runtimeError(vm, "RECEIVE : Expected string, got %s", getValName(CAR(value)));
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    if (!tableAddEntry(vm, &TABLE(array), AS_STRING(CAR(value)), CDR(value))) {
                        runtimeError(vm, "RECEIVE : Key %s is already in map", AS_CSTRING(CAR(value)));
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    pop(vm);
                    pop(vm);

                    push(vm, value);
                }
                else {
                    runtimeError(vm, "RECEIVE : %s cannot receive values", getValName(array));
                    return INTERPRET_RUNTIME_ERROR;
                }

                break;
            }
            case OP_TEST_CASE: {
                uint16_t spot = READ_SHORT(vm);

                if (valuesEqual(peek(vm, 0), peek(vm, 1))) {
                    pop(vm);
                    pop(vm);
                    break;
                }

                pop(vm);
                currentFrame(vm)->ip += spot;
                break;
            }
            case OP_INT_P: {
                uint16_t val = READ_SHORT(vm);
                push(vm, INT_VAL(val));
                break;
            }
            case OP_INT_N: {
                uint16_t val = READ_SHORT(vm);
                push(vm, INT_VAL(-val));
                break;
            }
            case OP_FLOAT_P: {
                uint16_t val = READ_SHORT(vm);
                push(vm, FLOAT_VAL(val));
                break;
            }
            case OP_FLOAT_N: {
                uint16_t val = READ_SHORT(vm);
                push(vm, FLOAT_VAL(-val));
                break;
            }
            case OP_CHAR: {
                uint8_t val = READ_BYTE(vm);
                push(vm, CHAR_VAL(val));
                break;
            }
            case OP_COMPOSE: {
                if (!IS_CALLABLE(peek(vm, 1)) || !IS_CALLABLE(peek(vm, 0))) {
                    runtimeError(vm, "COMPOSE : Cannot compose %s with %s", getValName(peek(vm, 0)), getValName(peek(vm, 1)));
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjFunction* fn = newFunction(vm, NULL);

                push(vm, OBJ_VAL(fn)); // GC :: +1 to all the peek()'s

                size_t instruction_n = currentFrame(vm)->ip - currentFrame(vm)->function->body.code - 1;
                int line_n = currentFrame(vm)->function->body.lines[instruction_n];

                int arity = getArity(vm, peek(vm, 1));

                if (arity == -1) {
                    runtimeError(vm, "COMPOSE : Cannot compose %s with %s", getValName(peek(vm, 0)), getValName(peek(vm, 1)));
                    return INTERPRET_RUNTIME_ERROR;
                }

                fn->arity = arity;


                addConstant(vm, &fn->body, peek(vm, 2));
                writeChunk(vm, &fn->body, OP_LOADV, line_n);
                writeChunk(vm, &fn->body, 0, line_n);

                addConstant(vm, &fn->body, peek(vm, 1));
                writeChunk(vm, &fn->body, OP_LOADV, line_n);
                writeChunk(vm, &fn->body, 1, line_n);

                for (uint8_t i = 0; i < arity; i++) {
                    writeChunk(vm, &fn->body, OP_GET_LOCAL, line_n);
                    writeChunk(vm, &fn->body, i, line_n);
                }

                writeChunk(vm, &fn->body, OP_CALL, line_n);
                writeChunk(vm, &fn->body, arity, line_n);

                writeChunk(vm, &fn->body, OP_TAIL_CALL, line_n);
                writeChunk(vm, &fn->body, 1, line_n);

                pop(vm); // GC
                pop(vm);
                pop(vm);

                push(vm, OBJ_VAL(fn));

                #ifdef DEBUG_DISPLAY_PROGRAM
                disassembleChunk(&fn->body, "<lmbd>");
                #endif

                break;
            }
            case OP_SWAP_TOP: {
                Value b = pop(vm);
                Value a = pop(vm);

                push(vm, b);
                push(vm, a);

                break;
            }
            case OP_SLICE: {
                uint8_t mode = READ_BYTE(vm);

                uint8_t offset;
                #ifdef OPTION_ONE_INDEXED
                offset = 1;
                #else
                offset = 0;
                #endif

                switch (mode) {
                    case 0: {  // start to end
                        Value array = peek(vm, 0);
                        if (!IS_LIST(array) && !IS_STRING(array)) {
                            runtimeError(vm, "SLICE : Cannot slice %s", getValName(array));
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        long long length = 0;

                        if (IS_LIST(array)) {
                            length = ARRAY(array).count - 1;
                        }
                        else {
                            length = AS_STRING(array)->length - 1;
                        }

                        if (!sliceArray(vm, array, 0, length)) {
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        Value result = pop(vm);
                        pop(vm);
                        push(vm, result);
                        break;
                    }
                    case 1: { // start to y
                        Value array = peek(vm, 1);
                        Value index = peek(vm, 0);

                        if (!IS_LIST(array) && !IS_STRING(array)) {
                            runtimeError(vm, "SLICE : Cannot slice %s", getValName(array));
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        if (!IS_INT(index)) {
                            runtimeError(vm, "SLICE : Expected VAL_INT, got %s", getValName(index));
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        if (!sliceArray(vm, array, 0, AS_INT(index) - offset)) {
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        Value result = pop(vm);
                        pop(vm);
                        pop(vm);
                        push(vm, result);
                        break;
                    }
                    case 2: { // x to end
                        Value array = peek(vm, 1);
                        Value index = peek(vm, 0);

                        if (!IS_LIST(array) && !IS_STRING(array)) {
                            runtimeError(vm, "SLICE : Cannot slice %s", getValName(array));
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        if (!IS_INT(index)) {
                            runtimeError(vm, "SLICE : Expected VAL_INT, got %s", getValName(index));
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        long long length = 0;

                        if (IS_LIST(array)) {
                            length = ARRAY(array).count - 1;
                        }
                        else {
                            length = AS_STRING(array)->length - 1;
                        }

                        if (!sliceArray(vm, array, AS_INT(index) - offset, length)) {
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        Value result = pop(vm);
                        pop(vm);
                        pop(vm);
                        push(vm, result);
                        break;
                    }
                    case 3: { // x to y
                        Value array = peek(vm, 2);
                        Value x = peek(vm, 1);
                        Value y = peek(vm, 0);

                        if (!IS_LIST(array) && !IS_STRING(array)) {
                            runtimeError(vm, "SLICE : Cannot slice %s", getValName(array));
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        if (!IS_INT(x) || !IS_INT(y)) {
                            runtimeError(vm, "SLICE : Expected two VAL_INTs, got %s and %s", getValName(x), getValName(y));
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        if (!sliceArray(vm, array, AS_INT(x) - offset, AS_INT(y) - offset)) {
                            return INTERPRET_RUNTIME_ERROR;
                        }

                        Value result = pop(vm);
                        pop(vm);
                        pop(vm);
                        pop(vm);
                        push(vm, result);
                        break;
                    }
                    default:
                        runtimeError(vm, "SLICE : Unknown operating mode %d", mode);
                        return INTERPRET_RUNTIME_ERROR;
                }

                break;
            }
            case OP_IN: {
                Value list = peek(vm, 0);
                Value atom = peek(vm, 1);

                bool result = false;

                if (IS_LIST(list)) {
                    for (int i = 0; i < ARRAY(list).count; i++) {
                        if (valuesEqual(ARRAY(list).values[i], atom)) {
                            result = true;
                            break;
                        }
                    }
                }
                else if (IS_STRING(list) && IS_STRING(atom)) {
                    for (int i = 0; i <= AS_STRING(list)->length - AS_STRING(atom)->length; i++) {
                        if (memcmp(&AS_CSTRING(list)[i], AS_CSTRING(atom), AS_STRING(atom)->length) == 0) {
                            result = true;
                            break;
                        }
                    }
                }
                else if (IS_STRING(list) && IS_CHAR(atom)) {
                    for (int i = 0; i < AS_STRING(list)->length; i++) {
                        if (AS_CSTRING(list)[i] == AS_CHAR(atom)) {
                            result = true;
                            break;
                        }
                    }
                }
                else {
                    runtimeError(vm, "IN : Cannot search for %s in %s", getValName(atom), getValName(list));
                    return INTERPRET_RUNTIME_ERROR;
                }

                pop(vm);
                pop(vm);
                push(vm, BOOL_VAL(result));

                break;
            }
        }

        #ifdef DEBUG_DISPLAY_STACK
        for (Value* ptr = vm->stack; ptr < vm->stackTop; ptr++) {
            printValue(*ptr);
            printf(" | ");
        }
        printf("\n");
        #endif

        #ifdef DEBUG_DISPLAY_STRINGS
        printf("\nstrings:\n");
        printTable(&vm->strings);
        #endif

        #ifdef DEBUG_DISPLAY_TABLES
        printf("\nglobals:\n");
        printTable(&vm->globals);
        #endif
    }

#undef READ_CONST
#undef READ_BYTE
}

InterpretResult interpretTEST(const char* source) {
    VM vm;
    initVM(&vm);

    ObjFunction* script = compile(source, &vm);

    if (script == NULL) {
        freeVM(&vm);
        return INTERPRET_COMPILATION_ERROR;
    }

    vm.frames[vm.frameCount++] = (CallFrame){script, NULL, script->body.code, vm.stack, false};

    vm.isActive = true;

    InterpretResult result = run(&vm);

    freeVM(&vm);

    return result;
}

InterpretResult interpret(VM* vm, const char* source) {
    // no gc during compilation
    // shouldnt be needed normally but repl() makes multiple calls to interpret() with one VM so must be reset
    vm->isActive = false;
    ObjFunction* script = compile(source, vm);

    if (script == NULL) {
        return INTERPRET_COMPILATION_ERROR;
    }

    vm->frames[vm->frameCount++] = (CallFrame){script, NULL, script->body.code, vm->stack, false};
    vm->isActive = true;

    return run(vm);
}

InterpretResult interpretPrecompiled(VM* vm, const char* source) {
    vm->isActive = false;
    ObjFunction* script = recompile(source, vm);

    if (script == NULL) {
        return INTERPRET_COMPILATION_ERROR;
    }

    vm->frames[vm->frameCount++] = (CallFrame){script, NULL, script->body.code, vm->stack, false};
    vm->isActive = true;

    return run(vm);
}

InterpretResult repl() {
    VM vm;
    initVM(&vm);

    InterpretResult res = INTERPRET_RUNTIME_ERROR;

    for (;;) {
        char buf[2048];
        printf("\n>>> ");

        if (!fgets(buf, sizeof(buf), stdin)) {
            printf("Error receiving input\n");
            continue;
        }

        res = interpret(&vm, buf);

        if (res == INTERPRET_RUNTIME_ERROR) {
            // TODO: make cleanVM() if at all possible
            break;
        }
    }

    freeVM(&vm);
    return res;
}

/*
+---------------------+
| Runtime       ^^^^  |
+=====================+
*/
