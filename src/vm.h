#ifndef LOX_VM_H
#define LOX_VM_H

#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    obj_closure_t* closure;
    uint8_t* ip;
    value_t* slots;
} callframe_t;

typedef struct {
    callframe_t frames[FRAMES_MAX];
    int frame_count;

    value_t  stack[STACK_MAX];
    value_t* stack_top;
    table_t  globals;
    table_t  strings;
    obj_t*   objects;
} vm_t;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

// exposing the vm instance
extern vm_t vm;

void l_init_vm();
void l_free_vm();

InterpretResult l_interpret(const char * source);

#endif