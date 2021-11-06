#include <stdio.h>

#include "lib/debug.h"
#include "common.h"
#include "vm.h"

vm_t _vm;

static void _reset_stack() {
    _vm.stack_top = _vm.stack;
}


void l_init_vm() {
    _reset_stack();
}

void l_free_vm() {

}

static InterpretResult _run() {
#define READ_BYTE() (*_vm.ip++)
#define READ_CONSTANT() (_vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(op) \
    do { \
      double b = l_pop(); \
      double a = l_pop(); \
      l_push(a op b); \
    } while (false)

    for (;;) {

#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value* slot = _vm.stack; slot < _vm.stack_top; slot++) {
            printf("[ ");
            l_print_value(*slot);
            printf(" ]");
        }
        printf("\n");
        
        l_disassemble_instruction(_vm.chunk, (int)(_vm.ip - _vm.chunk->code));
#endif

        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                l_push(constant);
                break;
            }
            case OP_ADD:      BINARY_OP(+); break;
            case OP_SUBTRACT: BINARY_OP(-); break;
            case OP_MULTIPLY: BINARY_OP(*); break;
            case OP_DIVIDE:   BINARY_OP(/); break;
            case OP_NEGATE:   l_push(-l_pop()); break;
            case OP_RETURN: {
                l_print_value(l_pop());
                printf("\n");
                return INTERPRET_OK;
            }
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult l_interpret(chunk_t* chunk) {
    _vm.chunk = chunk;
    _vm.ip = _vm.chunk->code;
    return _run();
}

void l_push(Value value) {
    *_vm.stack_top = value;
    _vm.stack_top++;
}

Value l_pop() {
    _vm.stack_top--;
    return *_vm.stack_top;
}