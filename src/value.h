#ifndef type_h_hammer
#define type_h_hammer

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

/*
{}          = 0
true        = 1
1           = 2
1.0         = 3
'a'         = 4
"a"         = 5
(1 , 1)     = 6
<f : 0>     = 7
<ntv : 1>   = 8
<clsr : 0>  = 9
[;]         = 10
[=>]        = 11
*/

typedef enum {
    VAL_UNIT,
    VAL_BOOL,
    VAL_INT,
    VAL_FLOAT,
    VAL_CHAR,
    VAL_OBJ
} ValType;

typedef struct {
    ValType type;
    union {
        bool boolean;
        long long integer;
        double floating;
        char character;
        Obj* obj;
    } as;
} Value;

typedef struct {
    int count;
    int capacity;
    Value* values;
} ValueArray;


#define TYPES_EQUAL(v1, v2) ((v1).type == (v2).type)
#define IS_ARITH(val)       (isArith(val))

#define IS_UNIT(val)        ((val).type == VAL_UNIT)
#define IS_BOOL(val)        ((val).type == VAL_BOOL)
#define IS_INT(val)         ((val).type == VAL_INT)
#define IS_FLOAT(val)       ((val).type == VAL_FLOAT)
#define IS_CHAR(val)        ((val).type == VAL_CHAR)
#define IS_OBJ(val)         ((val).type == VAL_OBJ)

#define AS_BOOL(val)        ((val).as.boolean)
#define AS_INT(val)         ((val).as.integer)
#define AS_FLOAT(val)       ((val).as.floating)
#define AS_CHAR(val)        ((val).as.character)
#define AS_OBJ(val)         ((val).as.obj)

#define UNIT_VAL            ((Value){VAL_UNIT,      {.boolean = false}})
#define BOOL_VAL(value)     ((Value){VAL_BOOL,      {.boolean = (value)}})
#define INT_VAL(value)      ((Value){VAL_INT,       {.integer = (value)}})
#define FLOAT_VAL(value)    ((Value){VAL_FLOAT,     {.floating = (value)}})
#define CHAR_VAL(value)     ((Value){VAL_CHAR,      {.character = (value)}})
#define OBJ_VAL(object)     ((Value){VAL_OBJ,       {.obj = (Obj*)object}})

void initValueArray(ValueArray* array);
void writeValueArray(VM* vm, ValueArray* array, Value value);
void freeValueArray(VM* vm, ValueArray* array);
bool valuesEqual(Value a, Value b);

void printValue(Value value);

static inline bool isArith(Value val) {
    return val.type == VAL_INT || val.type == VAL_FLOAT;
}

#endif