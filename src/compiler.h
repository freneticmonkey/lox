#ifndef LOX_COMPILER_H
#define LOX_COMPILER_H

#include "object.h"
#include "vm.h"

#include "chunk.h"

obj_function_t* l_compile(const char* source);

#endif