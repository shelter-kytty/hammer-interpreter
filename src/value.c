#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "value.h"
#include "common.h"
#include "object.h"

void initValueArray(ValueArray* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void writeValueArray(VM* vm, ValueArray* array, Value value) {
    if (array->capacity < array->count + 1) {
        int oldCap = array->capacity;
        array->capacity = GROW_CAP(oldCap);
        array->values = GROW_ARRAY(vm, array->values, oldCap, array->capacity, Value);
    }

    array->values[array->count] = value;
    array->count++;
}

void freeValueArray(VM* vm, ValueArray* array) {
    FREE_ARRAY(vm, array->values, array->capacity, Value);
    initValueArray(array);
}

bool valuesEqual(Value a, Value b) {
    if (a.type != b.type) {
        if (IS_ARITH(a) && IS_ARITH(b)) {
            if (IS_FLOAT(a))
                return AS_FLOAT(a) == AS_INT(b);
            else
                return AS_INT(a) == AS_FLOAT(b);
        } else
            return false;
    }

    switch (a.type) {
        case VAL_UNIT:          return true;
        case VAL_BOOL:          return AS_BOOL(a) == AS_BOOL(b);
        case VAL_INT:           return AS_INT(a) == AS_INT(b);
        case VAL_FLOAT:         return AS_FLOAT(a) == AS_FLOAT(b);
        case VAL_CHAR:          return AS_CHAR(a) == AS_CHAR(b);
        case VAL_OBJ:           {
            if (OBJ_TYPE(a) != OBJ_TYPE(b)) {
                return false;
            }
            
            switch (OBJ_TYPE(a)) {
            case OBJ_STRING:    return AS_STRING(a) == AS_STRING(b);
            case OBJ_CELL:      return valuesEqual(CAR(a), CAR(b)) && valuesEqual(CDR(a), CDR(b));
            default: return false;
            }
        }
        default:    return false;
    }
}

void printValue(Value value) {
    switch (value.type) {
        case VAL_UNIT:          printf("UNIT"); break;
        case VAL_BOOL:          printf(AS_BOOL(value) ? "true" : "false"); break;
        case VAL_INT:           printf("%lli", AS_INT(value)); break;
        case VAL_FLOAT:         printf("%lg", AS_FLOAT(value)); break;
        case VAL_CHAR:          printf("%c", AS_CHAR(value)); break;
        case VAL_OBJ:           printObject(value); break;
    }
}