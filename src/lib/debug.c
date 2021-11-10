#include <stdio.h>

#include "lib/debug.h"
#include "value.h"

void l_dissassemble_chunk(chunk_t *chunk, const char *name) {
    printf("== %s ==\n", name);

    for (int offset = 0; offset < chunk->count;) {
        offset = l_disassemble_instruction(chunk, offset);
    }
}

static int _constant_instruction(const char* name, chunk_t* chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    l_print_value(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 2;
}

static int _simple_instruction(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int _byte_instruction(const char* name, chunk_t* chunk, int offset) {
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2; 
}

static int _jump_instruction(const char* name, int sign, chunk_t* chunk, int offset) {
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, (offset + 3 + sign * jump) );
    return offset + 3;
}

int l_disassemble_instruction(chunk_t* chunk, int offset) {
    printf("%04d ", offset);

    if (offset > 0 &&
        chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        
        case OP_CONSTANT:
            return _constant_instruction("OP_CONSTANT", chunk, offset);
        case OP_NIL:
            return _simple_instruction("OP_NIL", offset);
        case OP_TRUE:
            return _simple_instruction("OP_TRUE", offset);
        case OP_FALSE:
            return _simple_instruction("OP_FALSE", offset);
        case OP_POP:
            return _simple_instruction("OP_POP", offset);
        case OP_GET_LOCAL:
            return _byte_instruction("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL:
            return _byte_instruction("OP_SET_LOCAL", chunk, offset);
        case OP_GET_GLOBAL:
            return _constant_instruction("OP_GET_GLOBAL", chunk, offset);
        case OP_DEFINE_GLOBAL:
            return _constant_instruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_SET_GLOBAL:
            return _constant_instruction("OP_SET_GLOBAL", chunk, offset);
        case OP_EQUAL:
            return _simple_instruction("OP_EQUAL", offset);
        case OP_GREATER:
            return _simple_instruction("OP_GREATER", offset);
        case OP_LESS:
            return _simple_instruction("OP_LESS", offset);
        case OP_ADD:
            return _simple_instruction("OP_ADD", offset);
        case OP_SUBTRACT:
            return _simple_instruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY:
            return _simple_instruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:
            return _simple_instruction("OP_DIVIDE", offset);
        case OP_NOT:
            return _simple_instruction("OP_NOT", offset);
        case OP_NEGATE:
            return _simple_instruction("OP_NEGATE", offset);
        case OP_PRINT:
            return _simple_instruction("OP_PRINT", offset);
        case OP_JUMP:
            return _jump_instruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE:
            return _jump_instruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_LOOP:
            return _jump_instruction("OP_LOOP", -1, chunk, offset);
        case OP_CALL:
            return _byte_instruction("OP_CALL", chunk, offset);
        case OP_RETURN:
            return _simple_instruction("OP_RETURN", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}