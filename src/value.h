#ifndef LOX_VALUE_H
#define LOX_VALUE_H

#include "common.h"

typedef double Value;

typedef struct {
    int capacity;
    int count;
    Value* values;
} value_array_t;

void l_init_value_array(value_array_t* array);
void l_write_value_array(value_array_t* array, Value value);
void l_free_value_array(value_array_t* array);

void l_print_value(Value value);

#endif