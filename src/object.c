#include <stdio.h>
#include <string.h>

#include "common.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include "table.h"
#include "debug.h"
#include "memory.h"


#define ALLOCATE_OBJ(v, type, objectType) \
    (type*)allocateObject(v, sizeof(type), objectType)

#define ALLOCATE_COBJ(v, type, elementType, size) \
    (type*)reallocate(v, NULL, 0, sizeof(type) + (size) * sizeof(elementType));

static Obj* allocateObject(VM* vm, size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(vm, NULL, 0, size);
    object->type = type;
    object->colour = MEM_WHITE;

    object->next = vm->objects;
    vm->objects = object;

    object->line = NULL;

#ifdef DEBUG_LOG_MEMORY
    printf("%p allocate %zu for %s\n", (void*)object, size, getObjName(type));
#endif

    return object;
}

// PJW hash - very cool 👍
static uint32_t hashString(const char* str, size_t length) {
    uint32_t h = 0, high;
    const char* s = str;
    while (s < str + length) {
        h = (h << 4) + *s++;
        if (high = h & 0xF0000000)
            h ^= high >> 24;
        h &= ~high;
    }
    return h;
}

ObjString* allocateString(VM* vm, char* chars, int length, uint32_t hash) {
    ObjString* string = ALLOCATE_OBJ(vm, ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    tableAddEntry(vm, &vm->strings, string, UNIT_VAL);

    return string;
}

ObjString* copyString(VM* vm, const char* chars, size_t length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm->strings, chars, length, hash);

    if (interned != NULL) {
        return interned;
    }

    char* heapChars = ALLOCATE(vm, length + 1, char);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';

    return allocateString(vm, heapChars, length, hash);
}

ObjString* takeString(VM* vm, char* chars, size_t length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm->strings, chars, length, hash);

    if (interned != NULL) {
        FREE_ARRAY(vm, chars, length + 1, char);
        return interned;
    }

    return allocateString(vm, chars, length, hash);
}


ObjCell* newCell(VM* vm) {
    ObjCell* cell = ALLOCATE_OBJ(vm, ObjCell, OBJ_CELL);
    cell->car = UNIT_VAL;
    cell->cdr = UNIT_VAL;
    return cell;
}

ObjNative* newNative(VM* vm, NativeFn function, int arity) {
    ObjNative* native = ALLOCATE_OBJ(vm, ObjNative, OBJ_NATIVE);
    native->function = function;
    native->arity = arity;
    return native;
}

ObjFunction* newFunction(VM* vm, ObjString* name) {
    ObjFunction* func = ALLOCATE_OBJ(vm, ObjFunction, OBJ_FUNCTION);
    initChunk(&func->body);
    func->name = name;
    func->arity = 0;
    return func;
}

ObjClosure* newClosure(VM* vm, ObjFunction* function, uint8_t upvalueCount) {
    Value* upvalues = ALLOCATE(vm, upvalueCount, Value);
    uint8_t* depths = ALLOCATE(vm, upvalueCount, uint8_t);

    ObjClosure* closure = ALLOCATE_OBJ(vm, ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalueCount = upvalueCount;
    closure->upvalues = upvalues;
    closure->depths = depths;
    
    return closure;
}

ObjList* newList(VM* vm) {
    ObjList* list = ALLOCATE_OBJ(vm, ObjList, OBJ_LIST);
    initValueArray(&list->array);
    return list;
}

ObjMap* newMap(VM* vm) {
    ObjMap* map = ALLOCATE_OBJ(vm, ObjMap, OBJ_MAP);
    initTable(&map->table);
    return map;
}


void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING: {
            #ifdef DEBUG_STRING_DETAILS
            printf("%s : %d : %p", AS_CSTRING(value), AS_STRING(value)->hash, AS_OBJ(value));
            #else
            printf("%s", AS_CSTRING(value));
            #endif
            break;
        }
        case OBJ_CELL: {
            #ifdef OPTION_RECURSIVE_PRINTING
            printf("(");
            printValue(CAR(value));
            printf(" , ");
            printValue(CDR(value));
            printf(")");
            #else
            printf("(");
            if (!IS_CELL(CAR(value)))
                printValue(CAR(value));
            else
                printf("(,)");
            printf(" , ");
            if (!IS_CELL(CDR(value)))
                printValue(CDR(value));
            else
                printf("(,)");
            printf(")");
            #endif
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* func = AS_FUNC(value);
            if (func->name != NULL) {
                printf("<fn %s : %d>", func->name->chars, func->arity);
            } else {
                printf("<lmbd : %d>", func->arity);
            }
            break;
        }
        case OBJ_NATIVE: {
            ObjNative* native = AS_NATIVE(value);
            printf("<ntv : %d>", native->arity);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = AS_CLOSURE(value);
            if (closure->function->name != NULL) {
                printf("<clsr %s : %d %d>", closure->function->name->chars, closure->function->arity, closure->upvalueCount);
            } else {
                printf("<clsr %d : %d>", closure->upvalueCount, closure->function->arity);
            }
            break;
        }
        case OBJ_LIST: {
            #ifdef OPTION_DETAILED_PRINTING
            ObjList* list = AS_LIST(value);
            printf("[ ");
            if (list->array.count > 0) {
                printValue(list->array.values[0]);

                for (size_t i = 1; i < list->array.count; ++i) {
                    printf(" ; ");
                    printValue(list->array.values[i]);
                }
            }
            else {
                printf(";");
            }
            printf(" ]");
            #else
            printf("[;]");
            #endif
            break;
        }
        case OBJ_MAP: {
            #ifdef OPTION_DETAILED_PRINTING
            ObjMap* map = AS_MAP(value);
            Table* table = &map->table;
            printf("[ ");
            if (table->count == 1) {
                for (int i = 0; i < table->capacity; i++) {
                    Entry* entry = &table->entries[i];
                    if (entry->key != NULL) {
                        printf("%s => ", entry->key->chars);
                        printValue(entry->value);
                    }
                }
            } 
            else if (table->count > 1) {
                int j = 0;

                for (int i = 0; i < table->capacity; i++) {
                    Entry* entry = &table->entries[i];
                    if (entry->key != NULL) {
                        j++;
                        printf("%s => ", entry->key->chars);
                        printValue(entry->value);
                        if (j < table->count) printf(" ; ");
                    }
                }
            }
            else {
                printf("=>");
            }
            printf(" ]");
            #else
            printf("[=>]");
            #endif
            break;
        }
    }
}