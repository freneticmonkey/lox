#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "lib/memory.h"
#include "lib/debug.h"
#include "common.h"
#include "compiler.h"
#include "vm.h"

vm_t vm;

void           _push(value_t value);
value_t        _pop();
static value_t _peek(int distance);
static bool    _is_falsey(value_t value);
static void    _concatenate();

static void _reset_stack() {
    vm.stack_top = vm.stack;
}

static void _runtime_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm.ip - vm.chunk->code - 1;
    int line = vm.chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    _reset_stack();
}


void l_init_vm() {
    _reset_stack();
    vm.objects = NULL;
    l_init_table(&vm.strings);
}

void l_free_vm() {
    l_free_table(&vm.strings);
    l_free_objects();
}

static InterpretResult _run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
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
        for (value_t* slot = vm.stack; slot < vm.stack_top; slot++) {
            printf("[ ");
            l_print_value(*slot);
            printf(" ]");
        }
        printf("\n");
        
        l_disassemble_instruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
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
                _push(BOOL_VAL(l_values_equal(a, b)));
                break;
            }
            case OP_GREATER:  BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS:     BINARY_OP(BOOL_VAL, <); break;
            case OP_ADD: {
                if (IS_STRING(_peek(0)) && IS_STRING(_peek(1))) {
                    _concatenate();
                } 
                else if (IS_NUMBER(_peek(0)) && IS_NUMBER(_peek(1))) {
                    double b = AS_NUMBER(_pop());
                    double a = AS_NUMBER(_pop());
                    _push(NUMBER_VAL(a + b));
                } 
                else {
                    _runtime_error(
                        "Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
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

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpretResult result = _run();

    l_free_chunk(&chunk);
    return result;
}

void _push(value_t value) {
    *vm.stack_top = value;
    vm.stack_top++;
}

value_t _pop() {
    vm.stack_top--;
    return *vm.stack_top;
}

static value_t _peek(int distance) {
    return vm.stack_top[-1 - distance];
}

static bool _is_falsey(value_t value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void _concatenate() {
    obj_string_t* b = AS_STRING(_pop());
    obj_string_t* a = AS_STRING(_pop());

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    obj_string_t* result = l_take_string(chars, length);
    _push(OBJ_VAL(result));
}