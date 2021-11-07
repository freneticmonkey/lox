#include <stdio.h>

#include "lib/memory.h"
#include "value.h"

void l_init_value_array(value_array_t* array) {
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void l_write_value_array(value_array_t* array, value_t value) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(value_t, array->values, oldCapacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

void l_free_value_array(value_array_t* array) {
    FREE_ARRAY(value_t, array->values, array->capacity);
    l_init_value_array(array);
}

void l_print_value(value_t value)
{
    switch(value.type) {
        case VAL_NUMBER:
            printf("%g", AS_NUMBER(value));
            break;
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NIL:
            printf("nil");
            break;
        default:
            printf("unknown value type");
    }
}