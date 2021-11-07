#include "common.h"
#include "chunk.h"
#include "version.h"
#include "vm.h"
#include "lib/debug.h"

int main(int argc, const char* argv[]) {
    printf("Starting lox %s ...\ncommit: %s\nbranch: %s\n", 
        VERSION, 
        COMMIT, 
        BRANCH
    );

    l_init_vm();

    chunk_t chunk;
    l_init_chunk(&chunk);

    int constant = l_add_constant(&chunk, 1.2);
    l_write_chunk(&chunk, OP_CONSTANT, 1);
    l_write_chunk(&chunk, constant, 1);

    constant = l_add_constant(&chunk, 3.4);
    l_write_chunk(&chunk, OP_CONSTANT, 1);
    l_write_chunk(&chunk, constant, 1);

    l_write_chunk(&chunk, OP_ADD, 1);

    constant = l_add_constant(&chunk, 5.6);
    l_write_chunk(&chunk, OP_CONSTANT, 1);
    l_write_chunk(&chunk, constant, 1);

    l_write_chunk(&chunk, OP_DIVIDE, 1);

    l_write_chunk(&chunk, OP_NEGATE, 1);
    l_write_chunk(&chunk, OP_RETURN, 1);
    l_dissassemble_chunk(&chunk, "test chunk");

    l_interpret(&chunk);
    l_free_vm();
    
    l_free_chunk(&chunk);

    printf("Exiting lox ...\n");
    return 0;
}