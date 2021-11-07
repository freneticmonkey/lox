#ifndef LOX_COMPILER_H
#define LOX_COMPILER_H

#include "object.h"
#include "vm.h"

#include "chunk.h"

bool l_compile(const char* source, chunk_t *chunk);

#endif