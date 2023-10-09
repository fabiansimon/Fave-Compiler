//
// Created by Fabian Simon on 29.09.23.
//

#ifndef FAVE_CUH_VM_H
#define FAVE_CUH_VM_H

#include "chunk.h"
#include "value.h"
#include "table.h"

#define STACK_MAX 256

typedef struct {
    Chunk* chunk;
    uint8_t* ip;
    Value stack[STACK_MAX];
    Value* stack_top;
    Table strings;
    Obj* objects;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void init_VM();
void free_VM();

InterpretResult interpret(const char* src);
void push(Value val);
Value pop();

#endif //FAVE_CUH_VM_H
