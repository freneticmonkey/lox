#include <stdarg.h>
#include <stdio.h>

#include "lib/debug.h"
#include "common.h"
#include "compiler.h"
#include "vm.h"

vm_t _vm;

void           _push(value_t value);
value_t        _pop();
static value_t _peek(int distance);
static bool    _is_falsey(value_t value);
bool           _values_equal(value_t a, value_t b);

static void _reset_stack() {
    _vm.stack_top = _vm.stack;
}

static void _runtime_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = _vm.ip - _vm.chunk->code - 1;
    int line = _vm.chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    _reset_stack();
}


void l_init_vm() {
    _reset_stack();
}

void l_free_vm() {

}

static InterpretResult _run() {
#define READ_BYTE() (*_vm.ip++)
#define READ_CONSTANT() (_vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(valueType, op) \
    do { \
        if (!IS_NUMBER(_peek(0)) || !IS_NUMBER(_peek(1))) { \
            _runtime_error("Operands must be numbers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        double b = AS_NUMBER(_pop()); \
        double a = AS_NUMBER(_pop()); \
        _push(valueType(a op b)); \
    } while (false)

    for (;;) {

#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (value_t* slot = _vm.stack; slot < _vm.stack_top; slot++) {
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
                value_t constant = READ_CONSTANT();
                _push(constant);
                break;
            }
            case OP_NIL:      _push(NIL_VAL); break;
            case OP_TRUE:     _push(BOOL_VAL(true)); break;
            case OP_FALSE:    _push(BOOL_VAL(false)); break;
            case OP_EQUAL: {
                value_t b = _pop();
                value_t a = _pop();
                _push(BOOL_VAL(_values_equal(a, b)));
                break;
            }
            case OP_GREATER:  BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS:     BINARY_OP(BOOL_VAL, <); break;
            case OP_ADD:      BINARY_OP(NUMBER_VAL, +); break;
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); break;
            case OP_NOT:      _push(BOOL_VAL(_is_falsey(_pop()))); break;
            case OP_NEGATE: {
                if (!IS_NUMBER(_peek(0))) {
                    _runtime_error("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                _push(NUMBER_VAL(-AS_NUMBER(_pop())));
                break;
            }
            case OP_RETURN: {
                l_print_value(_pop());
                printf("\n");
                return INTERPRET_OK;
            }
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult l_interpret(const char* source) {
    chunk_t chunk;
    l_init_chunk(&chunk);

    if (!l_compile(source, &chunk)) {
        l_free_chunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    _vm.chunk = &chunk;
    _vm.ip = _vm.chunk->code;

    InterpretResult result = _run();

    l_free_chunk(&chunk);
    return result;
}

void _push(value_t value) {
    *_vm.stack_top = value;
    _vm.stack_top++;
}

value_t _pop() {
    _vm.stack_top--;
    return *_vm.stack_top;
}

static value_t _peek(int distance) {
    return _vm.stack_top[-1 - distance];
}

static bool _is_falsey(value_t value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

bool _values_equal(value_t a, value_t b) {
    if (a.type != b.type) 
        return false;

    switch (a.type) {
        case VAL_BOOL:   return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL:    return true;
        case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
        default:         return false; // Unreachable.
    }
}