#include <stdio.h>

#include "debug.h"

#include "value.h"
#include "object.h"
#include "chunk.h"

void disassembleChunk(Chunk* chunk, const char* name)
{
    printf("== %s ==\n", name);
    printf("%d : %d\n", chunk->count, chunk->capacity);
    for (int offset = 0; offset < chunk->count;) {
        offset = disassembleInstruction(chunk, offset);
    }

    printf("\n");
}

const char* getObjName(int type) {
    switch (type) {
    case OBJ_STRING:
        return "OBJ_STRING";
    case OBJ_CELL:
        return "OBJ_CELL";
    case OBJ_FUNCTION:
        return "OBJ_FUNCTION";
    case OBJ_NATIVE:
        return "OBJ_NATIVE";
    case OBJ_CLOSURE:
        return "OBJ_CLOSURE";
    case OBJ_LIST:
        return "OBJ_LIST";
    case OBJ_MAP:
        return "OBJ_MAP";
    default:
        return "UNKNOWN_OBJ";
    }
}

const char* getValName(Value val) {
    switch (val.type) {
    case VAL_UNIT:
        return "VAL_UNIT";
    case VAL_BOOL:
        return "VAL_BOOL";
    case VAL_INT:
        return "VAL_INT";
    case VAL_FLOAT:
        return "VAL_FLOAT";
    case VAL_CHAR:
        return "VAL_CHAR";
    case VAL_OBJ:
        return getObjName(OBJ_TYPE(val));
    default:
        return "UNKNOWN_VAL";
    }
}

const char* getInstructionName(int type) {
    switch (type) {
        case OP_RETURN:         return "OP_RETURN";
        case OP_TAIL_CALL:      return "OP_TAIL_CALL";
        case OP_POP:            return "OP_POP";
        case OP_RETURN_SCOPE:   return "OP_RETURN_SCOPE";
        case OP_DUPE_TOP:       return "OP_DUPE_TOP";
        case OP_LOADV:          return "OP_LOADV";
        case OP_UNIT:           return "OP_UNIT";
        case OP_NOT:            return "OP_NOT";
        case OP_TRUTHY:         return "OP_TRUTHY";
        case OP_ADD:            return "OP_ADD";
        case OP_SUBTRACT:       return "OP_SUBTRACT";
        case OP_MULTIPLY:       return "OP_MULTIPLY";
        case OP_DIVIDE:         return "OP_DIVIDE";
        case OP_NEGATE:         return "OP_NEGATE";
        case OP_MODULO:         return "OP_MODULO";
        case OP_EXPONENT:       return "OP_EXPONENTIAL";
        case OP_DIFF:           return "OP_DIFF";
        case OP_DIFFEQ:         return "OP_DIFFEQ";
        case OP_EQUALS:         return "OP_EQUALS";
        case OP_CONSTRUCT:      return "OP_CONSTRUCT";
        case OP_CAR:            return "OP_CAR";
        case OP_CDR:            return "OP_CDR";
        case OP_CONCAT:         return "OP_CONCAT";
        case OP_MAKE_GLOBAL:    return "OP_MAKE_GLOBAL";
        case OP_GET_GLOBAL:     return "OP_GET_GLOBAL";
        case OP_GET_LOCAL:      return "OP_GET_LOCAL";
        case OP_JUMP_IF_TRUE:   return "OP_JUMP_IF_TRUE";
        case OP_JUMP_IF_FALSE:  return "OP_JUMP_IF_FALSE";
        case OP_JUMP:           return "OP_JUMP";
        case OP_CALL:           return "OP_CALL";
        case OP_UPVALUE:        return "OP_UPVALUE";
        case OP_CLOSURE:        return "OP_CLOSURE";
        case OP_DECONS:         return "OP_DECONS";
        case OP_TREE_COMP:      return "OP_TREE_COMP";
        case OP_LIST:           return "OP_LIST";
        case OP_MAP:            return "OP_MAP";
        case OP_SUBSCRIPT:      return "OP_SUBSCRIPT";
        case OP_RECEIVE:        return "OP_RECEIVE";
        case OP_TEST_CASE:      return "OP_TEST_CASE";
        case OP_INT_P:          return "OP_INT_P";
        case OP_INT_N:          return "OP_INT_N";
        case OP_FLOAT_P:        return "OP_FLOAT_P";
        case OP_CHAR:           return "OP_CHAR";
        case OP_COMPOSE:        return "OP_COMPOSE";
        case OP_SLICE:          return "OP_SLICE";
        default:                return "UNKNOWN_OP";
    }
}

static int constantInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset];
    uint8_t position = chunk->code[offset + 1];
    printf("%-16s %02d '", name, constant);
    printValue(chunk->constants.values[position]);
    printf("'\n");
    return offset + 2;
}

static int doubleInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset];
    uint8_t value = chunk->code[offset + 1];
    printf("%-16s %02d %d\n", name, constant, value);
    return offset + 2;
}

static int simpleInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset];
    printf("%-16s %02d\n", name, constant);
    return offset + 1;
}

static int variadicInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset];
    uint8_t count = chunk->code[offset + 1] * 2;
    printf("%-16s %02d %d  ", name, constant, count);
    if (count > 0) {
        printf("[ ");
        printf("%d", chunk->code[offset + 2]);
        for (int i = 1; i < count; i++){
            printf(" ; %d", chunk->code[offset + 2 + i]);
        }
        printf(" ]");
    }
    printf("\n");
    return offset + count + 2;
}

static int integerInstruction(const char* name, int sign, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset];
    uint16_t value = (uint16_t)((chunk->code[offset + 1] << 8) | chunk->code[offset + 2]);
    printf("%-16s %02d %+d\n", name, constant, value * sign);
    return offset + 3;
}

static int jumpInstruction(const char* name, int sign, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset];
    uint16_t jump = (uint16_t)((chunk->code[offset + 1] << 8) | chunk->code[offset + 2]);
    printf("%-16s %02d %d -> %d\n", name, constant, offset, offset + 3 + jump * sign);
    return offset + 3;
}

int disassembleInstruction(Chunk* chunk, int offset) {
    printf("%04d", offset);
    if (offset > 0 &&
        chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", chunk, offset);
        case OP_TAIL_CALL:
            return doubleInstruction("OP_TAIL_CALL", chunk, offset);
        case OP_POP:
            return simpleInstruction("OP_POP", chunk, offset);
        case OP_RETURN_SCOPE:
            return doubleInstruction("OP_RETURN_SCOPE", chunk, offset);
        case OP_DUPE_TOP:
            return simpleInstruction("OP_DUPE_TOP", chunk, offset);
        case OP_LOADV:
            return constantInstruction("OP_LOADV", chunk, offset);
        case OP_TRUE: 
            return simpleInstruction("OP_TRUE", chunk, offset);
        case OP_FALSE:
            return simpleInstruction("OP_FALSE", chunk, offset);
        case OP_UNIT:
            return simpleInstruction("OP_UNIT", chunk, offset);
        case OP_NOT:
            return simpleInstruction("OP_NOT", chunk, offset);
        case OP_TRUTHY:
            return simpleInstruction("OP_TRUTHY", chunk, offset);
        case OP_NEGATE:
            return simpleInstruction("OP_NEGATE", chunk, offset);
        case OP_ADD:
            return simpleInstruction("OP_ADD", chunk, offset);
        case OP_SUBTRACT:
            return simpleInstruction("OP_SUBTRACT", chunk, offset);
        case OP_MULTIPLY:
            return simpleInstruction("OP_MULTIPLY", chunk, offset);
        case OP_DIVIDE:
            return simpleInstruction("OP_DIVIDE", chunk, offset);
        case OP_MODULO:
            return simpleInstruction("OP_MODULO", chunk, offset);
        case OP_EXPONENT:
            return simpleInstruction("OP_EXPONENTIAL", chunk, offset);
        case OP_DIFF:
            return simpleInstruction("OP_DIFF", chunk, offset);
        case OP_DIFFEQ:
            return simpleInstruction("OP_DIFFEQ", chunk, offset);
        case OP_EQUALS:
            return simpleInstruction("OP_EQUALS", chunk, offset);
        case OP_CONSTRUCT:
            return simpleInstruction("OP_CONSTRUCT", chunk, offset);
        case OP_CAR:
            return simpleInstruction("OP_CAR", chunk, offset);
        case OP_CDR:
            return simpleInstruction("OP_CDR", chunk, offset);
        case OP_CONCAT:
            return simpleInstruction("OP_CONCAT", chunk, offset);
        case OP_MAKE_GLOBAL:
            return constantInstruction("OP_MAKE_GLOBAL", chunk, offset);
        case OP_GET_GLOBAL:
            return constantInstruction("OP_GET_GLOBAL", chunk, offset);
        case OP_GET_LOCAL:
            return doubleInstruction("OP_GET_LOCAL", chunk, offset);
        case OP_JUMP_IF_TRUE:
            return jumpInstruction("OP_JUMP_IF_TRUE", 1, chunk, offset);
        case OP_JUMP_IF_FALSE:
            return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_JUMP:
            return jumpInstruction("OP_JUMP", 1, chunk, offset);
        case OP_CALL:
            return doubleInstruction("OP_CALL", chunk, offset);
        case OP_UPVALUE:
            return doubleInstruction("OP_UPVALUE", chunk, offset);
        case OP_CLOSURE:
            return variadicInstruction("OP_CLOSURE", chunk, offset);
        case OP_DECONS:
            return simpleInstruction("OP_DECONS", chunk, offset);
        case OP_TREE_COMP:
            return constantInstruction("OP_TREE_COMP", chunk, offset);
        case OP_LIST:
            return doubleInstruction("OP_LIST", chunk, offset);
        case OP_MAP:
            return doubleInstruction("OP_MAP", chunk, offset);
        case OP_SUBSCRIPT:
            return simpleInstruction("OP_SUBSCRIPT", chunk, offset);
        case OP_RECEIVE:
            return simpleInstruction("OP_RECEIVE", chunk, offset);
        case OP_TEST_CASE:
            return jumpInstruction("OP_TEST_CASE", 1, chunk, offset);
        case OP_INT_P:
            return integerInstruction("OP_INT_P", 1, chunk, offset);
        case OP_INT_N:
            return integerInstruction("OP_INT_N", -1, chunk, offset);
        case OP_FLOAT_P:
            return integerInstruction("OP_FLOAT_P", 1, chunk, offset);
        case OP_FLOAT_N:
            return integerInstruction("OP_FLOAT_N", -1, chunk, offset);
        case OP_CHAR:
            return doubleInstruction("OP_CHAR", chunk, offset);
        case OP_COMPOSE:
            return simpleInstruction("OP_COMPOSE", chunk, offset);
        case OP_SLICE:
            return doubleInstruction("OP_SLICE", chunk, offset);
        default:
            return simpleInstruction("UNKNOWN_OP", chunk, offset);
    }
}