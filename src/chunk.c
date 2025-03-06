#include <stdio.h>

#include "memory.h"
#include "common.h"
#include "chunk.h"
#include "value.h"
#include "debug.h"

void initChunk(Chunk* chunk) 
{
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    initValueArray(&chunk->constants);
}

void freeChunk(VM* vm, Chunk* chunk)
{
    FREE_ARRAY(vm, chunk->code, chunk->capacity, uint8_t);
    FREE_ARRAY(vm, chunk->lines, chunk->capacity, int);
    freeValueArray(vm, &chunk->constants);
    initChunk(chunk);
}

void writeChunk(VM* vm, Chunk* chunk, uint8_t op, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAP(oldCapacity);
        chunk->code = GROW_ARRAY(vm, chunk->code, 
            oldCapacity, chunk->capacity, uint8_t);
        chunk->lines = GROW_ARRAY(vm, chunk->lines,
            oldCapacity, chunk->capacity, int);
    }

    chunk->code[chunk->count] = op;
    chunk->lines[chunk->count] = line;
    chunk->count++;

    #ifdef DEBUG_CHUNK_UPDATES
    disassembleChunk(chunk, "Chunk Update");
    #endif
}

int addConstant(VM* vm, Chunk* chunk, Value value) {
    writeValueArray(vm, &chunk->constants, value);
    
    #ifdef DEBUG_CHUNK_UPDATES
    printf("New Value\n > ");
    printValue(value);
    printf("\n");
    #endif
    
    return chunk->constants.count - 1;
}