#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "chunk.h"
#include "version.h"
#include "vm.h"
#include "lib/debug.h"


static void _repl() {
    char line[1024];
    for (;;) {
        printf("> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        l_interpret(line);
    }
}

static char* _read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }
    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}

static void _run_file(const char* path) {
    char* source = _read_file(path);
    InterpretResult result = l_interpret(source);
    free(source); 

    if (result == INTERPRET_COMPILE_ERROR) 
        exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) 
        exit(70);
}

int main(int argc, const char* argv[]) {
    printf("Starting lox %s ...\ncommit: %s\nbranch: %s\n", 
        VERSION, 
        COMMIT, 
        BRANCH
    );

    l_init_vm();

    if (argc == 1) {
        _repl();
    } else if (argc == 2) {
        _run_file(argv[1]);
    } else {
        fprintf(stderr, "Usage: lox [path]\n");
        exit(64);
    }

    l_free_vm();
    
    printf("Exiting lox ...\n");
    return 0;
}