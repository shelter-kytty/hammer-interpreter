#ifndef object_h_hammer
#define object_h_hammer

#include "common.h"
#include "value.h"
#include "chunk.h"
#include "table.h"

#define OBJ_TYPE(val)       (AS_OBJ(val)->type)
#define IS_CALLABLE(val)    (isCallable(val))

#define IS_STRING(val)      (isObjType(val, OBJ_STRING))
#define IS_CELL(val)        (isObjType(val, OBJ_CELL))
#define IS_FUNC(val)        (isObjType(val, OBJ_FUNCTION))
#define IS_NATIVE(val)      (isObjType(val, OBJ_NATIVE))
#define IS_CLOSURE(val)     (isObjType(val, OBJ_CLOSURE))
#define IS_LIST(val)        (isObjType(val, OBJ_LIST))
#define IS_MAP(val)         (isObjType(val, OBJ_MAP))

#define AS_STRING(val)      ((ObjString*)AS_OBJ(val))
#define AS_CELL(val)        ((ObjCell*)AS_OBJ(val))
#define AS_FUNC(val)        ((ObjFunction*)AS_OBJ(val))
#define AS_NATIVE(val)      ((ObjNative*)AS_OBJ(val))
#define AS_CLOSURE(val)     ((ObjClosure*)AS_OBJ(val))
#define AS_LIST(val)        ((ObjList*)AS_OBJ(val))
#define AS_MAP(val)         ((ObjMap*)AS_OBJ(val))

#define AS_CSTRING(val)     (((ObjString*)AS_OBJ(val))->chars)

#define CAR(val)            (((ObjCell*)AS_OBJ(val))->car)
#define CDR(val)            (((ObjCell*)AS_OBJ(val))->cdr)

#define NATIVE_FN(val)      (((ObjNative*)AS_OBJ(val))->function)
#define CLOSED_FN(val)      (((ObjClosure*)AS_OBJ(val))->function)

#define ARRAY(val)          (((ObjList*)AS_OBJ(val))->array)

#define TABLE(val)          (((ObjMap*)AS_OBJ(val))->table)

typedef enum {
    OBJ_STRING,
    OBJ_CELL,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_CLOSURE,
    OBJ_LIST,
    OBJ_MAP,
} ObjType;

struct Obj {
    ObjType type;
    Colour colour;
    struct Obj* next;
    struct Obj* line;
};

struct ObjString {
    Obj obj;
    int length;
    uint32_t hash;
    char* chars;
};

typedef struct {
    Obj obj;
    Value car;
    Value cdr;
} ObjCell;

typedef bool (*NativeFn)(VM* vm, int argc, Value* argv);

typedef struct {
    Obj obj;
    NativeFn function;
    int arity;
} ObjNative;

typedef struct {
    Obj obj;
    Chunk body;
    ObjString* name;
    uint8_t arity;
} ObjFunction;

typedef struct {
    Obj obj;
    ObjFunction* function;
    uint8_t upvalueCount;
    Value* upvalues;
    uint8_t* depths;
} ObjClosure;

typedef struct {
    Obj obj;
    ValueArray array;
} ObjList;

typedef struct {
    Obj obj;
    Table table;
} ObjMap;


void printObject(Value value);
ObjString* copyString(VM* vm, const char* chars, size_t length);
ObjString* takeString(VM* vm, char* chars, size_t length);
ObjCell* newCell(VM* vm);
ObjNative* newNative(VM* vm, NativeFn function, int arity);
ObjFunction* newFunction(VM* vm, ObjString* name);
ObjClosure* newClosure(VM* vm, ObjFunction* function, uint8_t upvalueCount);
ObjList* newList(VM* vm);
ObjMap* newMap(VM* vm);

static inline bool isCallable(Value value) {
    return IS_OBJ(value) && (
        OBJ_TYPE(value) == OBJ_FUNCTION ||
        OBJ_TYPE(value) == OBJ_CLOSURE  ||
        OBJ_TYPE(value) == OBJ_NATIVE
    );
}

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && OBJ_TYPE(value) == type;
}

#endif