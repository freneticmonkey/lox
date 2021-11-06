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
        case OP_RETURN:
            return _simple_instruction("OP_RETURN", offset);
        case OP_CONSTANT:
            return _constant_instruction("OP_CONSTANT", chunk, offset);
        case OP_ADD:
            return _simple_instruction("OP_ADD", offset);
        case OP_SUBTRACT:
            return _simple_instruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY:
            return _simple_instruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:
            return _simple_instruction("OP_DIVIDE", offset);
        case OP_NEGATE:
            return _simple_instruction("OP_NEGATE", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}