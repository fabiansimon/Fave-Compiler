//
// Created by Fabian Simon on 29.09.23.
//

#ifndef FAVE_CUH_DEBUG_H
#define FAVE_CUH_DEBUG_H

#include "chunk.h"

void disassemble_chunk(Chunk* chunk, const char* name);
int disassemble_instruction(Chunk* chunk, int offset);

#endif //FAVE_CUH_DEBUG_H
