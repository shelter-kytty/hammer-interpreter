#ifndef chunk_h_hammer
#define chunk_h_hammer

#include "common.h"
#include "value.h"


typedef enum {
    OP_RETURN,          // 0x00
    OP_TAIL_CALL,       // 0x01
    OP_POP,             // 0x02
    OP_RETURN_SCOPE,    // 0x03
    OP_DUPE_TOP,        // 0x04
    OP_LOADV,           // 0x05
    OP_TRUE,            // 0x06
    OP_FALSE,           // 0x07
    OP_UNIT,            // 0x08
    OP_NOT,             // 0x09
    OP_TRUTHY,          // 0x0A
    OP_NEGATE,          // 0x0B
    OP_ADD,             // 0x0C
    OP_SUBTRACT,        // 0x0D
    OP_MULTIPLY,        // 0x0E
    OP_DIVIDE,          // 0x0F
    OP_MODULO,          // 0x10
    OP_EXPONENT,        // 0x11
    OP_DIFF,            // 0x12
    OP_DIFFEQ,          // 0x13
    OP_EQUALS,          // 0x14
    OP_CONSTRUCT,       // 0x15
    OP_CAR,             // 0x16
    OP_CDR,             // 0x17
    OP_CONCAT,          // 0x18
    OP_MAKE_GLOBAL,     // 0x19
    OP_GET_GLOBAL,      // 0x1A
    OP_GET_LOCAL,       // 0x1B
    OP_JUMP_IF_TRUE,    // 0x1C
    OP_JUMP_IF_FALSE,   // 0x1D
    OP_JUMP,            // 0x1E
    OP_CALL,            // 0x1F
    OP_UPVALUE,         // 0x20
    OP_CLOSURE,         // 0x21
    OP_DECONS,          // 0x22
    OP_TREE_COMP,       // 0x23
    OP_LIST,            // 0x24
    OP_MAP,             // 0x25
    OP_SUBSCRIPT,       // 0x26
    OP_RECEIVE,         // 0x27
    OP_TEST_CASE,       // 0x28
    OP_INT_P,           // 0x29
    OP_INT_N,           // 0x2A
    OP_FLOAT_P,         // 0x2B
    OP_FLOAT_N,         // 0x2C
    OP_CHAR,            // 0x2D
    OP_COMPOSE,         // 0x2E
    OP_SWAP_TOP,        // 0x2F
    OP_SLICE,           // 0x30
    OP_IN,              // 0x31
} OpCode; 

typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    int* lines;
    ValueArray constants;
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(VM* vm, Chunk* chunk);
void writeChunk(VM* vm, Chunk* chunk, uint8_t op, int line);
int addConstant(VM* vm, Chunk* chunk, Value value);

#endif