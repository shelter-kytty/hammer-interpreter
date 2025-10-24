#ifndef memory_h_hammer
#define memory_h_hammer

#include "common.h"
#include "object.h"

#define ALLOCATE(v, count, type)    \
        (type*)reallocate(v, NULL, 0, sizeof(type) * count)

#define FREE(v, ptr, type) \
        reallocate(v, ptr, sizeof(type), 0)

#define GROW_CAP(cap) \
        ((cap) < 4 ? 4 : (cap) * 2)

#define GROW_ARRAY(v, ptr, osize, nsize, type)  \
        (type*)reallocate(v, ptr, sizeof(type) * osize, sizeof(type) * nsize)

#define FREE_ARRAY(v, ptr, osize, type) \
        reallocate(v, ptr, sizeof(type) * osize, 0)

#define ALLOCATE_FAM(v, type, elementType, size) \
    (type*)reallocate(v, NULL, 0, sizeof(type) + (size) * sizeof(elementType));

void* reallocate(VM* vm, void* ptr, size_t oldSize, size_t newSize);
void freeObjects(VM* vm);

void markValue(VM* vm, Value value);
void markObject(VM* vm, Obj* object);

#endif