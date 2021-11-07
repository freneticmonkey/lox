#ifndef LOX_VM_H
#define LOX_VM_H

//#include "common.h"
#include "chunk.h"
#include "value.h"

#define STACK_MAX 256

typedef struct {
    chunk_t* chunk;
    uint8_t* ip;
    value_t  stack[STACK_MAX];
    value_t* stack_top;
} vm_t;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

void l_init_vm();
void l_free_vm();

InterpretResult l_interpret(const char * source);

// VM Stack
// void  l_push(Value value);
// Value l_pop();
#endif