#include "common.h"
#include "chunk.h"
#include "lib/debug.h"

int main(int argc, const char* argv[]) {
    printf("Starting lox ...\n");

    chunk_t chunk;
    l_init_chunk(&chunk);

    int constant = l_add_constant(&chunk, 1.2);
    l_write_chunk(&chunk, OP_CONSTANT, 1);
    l_write_chunk(&chunk, constant, 1);
    l_write_chunk(&chunk, OP_RETURN, 1);
    l_dissassemble_chunk(&chunk, "test chunk");

    l_free_chunk(&chunk);

    printf("Exiting lox ...\n");
    return 0;
}