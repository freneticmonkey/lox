#ifndef CLOX_CHUNK_H
#define CLOX_CHUNK_H

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT,
    OP_NEGATE,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_RETURN,
} OpCode;

typedef struct {
    int      count;
    int      capacity;
    uint8_t* code;
    int*     lines;
    value_array_t constants;
} chunk_t;

void l_init_chunk(chunk_t* chunk);
void l_free_chunk(chunk_t* chunk);

void l_write_chunk(chunk_t* chunk, uint8_t byte, int line);
int  l_add_constant(chunk_t* chunk, Value value);

#endif