#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "debug.h"
#include "memory.h"
#include "vm.h"
#include "compiler.h"
#include "table.h"


#define GC_CONSTANT 2

void collectGarbage(VM* vm);

void* reallocate(VM* vm, void* ptr, size_t oldSize, size_t newSize) {
    vm->bytesAllocated += newSize - oldSize;

    if (vm->isActive) {
        if (newSize > oldSize) {
            #ifdef DEBUG_STRESS_GC
            #ifdef DEBUG_LOG_GC
            printf("With %p: %04d -> %04d\n", ptr, oldSize, newSize);
            printf("About to collect: at %zu\n", vm->bytesAllocated);
            #endif
            collectGarbage(vm);
            #else // !DEBUG_STRESS_GC
            if (vm->bytesAllocated > vm->nextGC) {
                #ifdef DEBUG_LOG_GC
                printf("With %p: %04d -> %04d\n", ptr, oldSize, newSize);
                printf("About to collect: at %zu\n", vm->bytesAllocated);
                #endif
                collectGarbage(vm);
            }
            #endif
        }
    }

    if (newSize == 0) {
        free(ptr);

        #ifdef DEBUG_LOG_MEMORY
        printf("Freeing %p: %04d -> %04d\n", ptr, oldSize, newSize);
        #endif

        return NULL;
    }

    #ifdef DEBUG_LOG_MEMORY
    printf("Allocating %p: %04d -> %04d\n", ptr, oldSize, newSize);
    #endif

    void* result = realloc(ptr, newSize);
    if (result == NULL) exit(64);
    return result;
}

static void freeObject(VM* vm, Obj* object) {
    #ifdef DEBUG_LOG_GC
    printf("Freeing %s\n", getObjName(object->type));
    #endif
    switch (object->type) {
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            FREE(vm, string, ObjString);
            break;
        }
        case OBJ_CELL: {
            ObjCell* cell = (ObjCell*)object;
            FREE(vm, cell, ObjCell);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* func = (ObjFunction*)object;
            freeChunk(vm, &func->body);
            FREE(vm, func, ObjFunction);
            break;
        }
        case OBJ_NATIVE: {
            ObjNative* native = (ObjNative*)object;
            FREE(vm, native, ObjNative);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            FREE_ARRAY(vm, closure->upvalues, closure->upvalueCount, Value);
            FREE_ARRAY(vm, closure->depths, closure->upvalueCount, uint8_t);
            FREE(vm, closure, ObjClosure);
            break;
        }
        case OBJ_LIST: {
            ObjList* list = (ObjList*)object;
            freeValueArray(vm, &list->array);
            FREE(vm, list, ObjList);
            break;
        }
        case OBJ_MAP: {
            ObjMap* map = (ObjMap*)object;
            freeTable(vm, &map->table);
            FREE(vm, map, ObjMap);
            break;
        }
    }
}

void freeObjects(VM* vm) {
    Obj* object = vm->objects;
    while (object != NULL) {
        Obj* next = object->next;
        freeObject(vm, object);
        object = next;
    }
}


void markObject(VM* vm, Obj* object) {
    if (object == NULL) return;
    if (object->colour == MEM_BLACK) return;

    object->colour = MEM_BLACK;
    object->line = NULL;

    if (vm->greyStart == NULL) {
        #ifdef DEBUG_LOG_GC
        printf("Starting greys with %p : %s\n", (void*)object, getObjName(object->type));
        #endif
        vm->greyStart = object;
    }
    else {
        #ifdef DEBUG_LOG_GC
        printf("Appending %p : %s\n", (void*)object, getObjName(object->type));
        #endif
        vm->greyEnd->line = object;
    }

    vm->greyEnd = object;
}

void markValue(VM* vm, Value value) {
    if (IS_OBJ(value)) markObject(vm, AS_OBJ(value));
}

static void markArray(VM* vm, ValueArray* array) {
    for (int i = 0; i < array->count; i++) {
        markValue(vm, array->values[i]);
    }
}

static void blackenObject(VM* vm, Obj* object) {
    #ifdef DEBUG_LOG_GC
    printf("Blackening %p : %s\n", (void*)object, getObjName(object->type));
    #endif
    switch (object->type) {
        case OBJ_CELL: {
            ObjCell* cell = (ObjCell*)object;
            markValue(vm, cell->car);
            markValue(vm, cell->cdr);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            markObject(vm, (Obj*)function->name);
            markArray(vm, &function->body.constants);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            markObject(vm, (Obj*)closure->function);
            for (int i = 0; i < closure->upvalueCount; i++) {
                markValue(vm, closure->upvalues[i]);
            }
            break;
        }
        case OBJ_LIST: {
            ObjList* list = (ObjList*)object;
            markArray(vm, &list->array);
            break;
        }
        case OBJ_MAP: {
            ObjMap* map = (ObjMap*)object;
            markTable(vm, &map->table);
            break;
        }
        case OBJ_STRING:
        case OBJ_NATIVE: 
            break;
    }
}

static void markRoots(VM* vm) {
    #ifdef DEBUG_LOG_GC
    printf("Slots:\n");
    #endif
    for (Value* slot = vm->stack; slot < vm->stackTop; slot++) {
        markValue(vm, *slot);
        #ifdef DEBUG_LOG_GC
        printValue(*slot);
        printf("\n");
        #endif
    }
    #ifdef DEBUG_LOG_GC
    printf("Frames:\n");
    #endif
    for (int i = 0; i < vm->frameCount; i++) {
        markObject(vm, (Obj*)vm->frames[i].function);
        markObject(vm, (Obj*)vm->frames[i].closure);
        #ifdef DEBUG_LOG_GC
        printValue(OBJ_VAL((Obj*)vm->frames[i].function));
        printf("\n");
        #endif
    }

    markTable(vm, &vm->globals);
    // markCompiler(vm); // I don't think I need this??? Shouldn't have to tiptoe
    // around allocation during the compilation phase... just wait until after
}

static void walkLine(VM* vm) {
    // Old loop didn't reset VM.greyStart to NULL, may
    // have contributed to some objs getting lost
    while (vm->greyStart != NULL) {
        blackenObject(vm, vm->greyStart);
        vm->greyStart = vm->greyStart->line;
    }
}

static void sweep(VM* vm) {
    Obj* previous = NULL;
    Obj* object = vm->objects;
    while (object != NULL) {
        #ifdef DEBUG_LOG_GC
        printf("Looping: ");
        #endif
        if (object->colour == MEM_BLACK || object->colour == MEM_GREY) {
            #ifdef DEBUG_LOG_GC
            printf("%p was %d; whiting out\n", (void*)object, object->colour);
            #endif
            object->colour = MEM_WHITE;
            previous = object;
            object = object->next;
        } else {
            #ifdef DEBUG_LOG_GC
            printf("%p was %d; freeing\n", (void*)object, object->colour);
            #endif
            Obj* unreached = object;
            object = object->next;
            if (previous != NULL) {
                previous->next = object;
            } else {
                vm->objects = object;
            }

            freeObject(vm, unreached);
        }
    }
}

void collectGarbage(VM* vm) {
    #ifdef DEBUG_LOG_GC
    size_t before = vm->bytesAllocated;
    #endif 

    // get all the easy stuff: the stack, globals, interned strings, 
    // active callframes, etc.
    #ifdef DEBUG_LOG_GC
    printf("Marking roots\n");
    #endif
    markRoots(vm);
    
    // traverse linked list "VM->greyStart/Obj->line". Appending elements through "VM->greyEnd"
    // saves having to allocate memory for double pointers (which would still have to be dereferenced)
    #ifdef DEBUG_LOG_GC
    printf("Walking grey line\n");
    #endif
    walkLine(vm);

    // interned strings are only useful if they're being used, and clog the hashtable otherwise,
    // so get rid of unused ones
    #ifdef DEBUG_LOG_GC
    printf("Cleaning strings\n");
    #endif
    tableRemoveWhite(&vm->strings);

    // traverse "VM->objects", removing white-coloured objs; grey-coloured objs are demoted 
    // to white, this is to allow for 1 cycle of lenience, the idea being it COULD get "picked up" by
    // a reference between now and the next gc cycle.
    // This does mean the first GC cycle won't clean anything up, but I can always just fiddle with
    // it until I get something that works (e.g. the constants could be set lower)
    #ifdef DEBUG_LOG_GC
    printf("Sweeping\n");
    #endif
    sweep(vm);

    vm->nextGC = vm->bytesAllocated * GC_CONSTANT;
    
    #ifdef DEBUG_LOG_GC
    printf("Finished collecting: at %zu (collected %zu bytes) next at %zu\n", vm->bytesAllocated, before - vm->bytesAllocated, vm->nextGC);
    #endif
}