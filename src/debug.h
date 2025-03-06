#ifndef debug_h_hammer
#define debug_h_hammer

#include "chunk.h"

const char* getObjName(int type);
const char* getValName(Value val);
const char* getInstructionName(int type);
void disassembleChunk(Chunk* chunk, const char* name);
int disassembleInstruction(Chunk* chunk, int offset);

#endif