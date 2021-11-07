#ifndef LOX_VALUE_H
#define LOX_VALUE_H

#include "common.h"

typedef enum {
    VAL_BOOL,
    VAL_NIL, 
    VAL_NUMBER,
} ValueType;

typedef struct {
    ValueType type;
    union {
        double number;
        bool boolean;
    } as;
} value_t;

#define BOOL_VAL(value)   ((value_t){VAL_BOOL, {.boolean = value}})
#define NIL_VAL           ((value_t){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((value_t){VAL_NUMBER, {.number = value}})

#define AS_BOOL(value)    ((value).as.boolean)
#define AS_NUMBER(value)  ((value).as.number)

#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NIL(value)     ((value).type == VAL_NIL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)

typedef struct {
    int capacity;
    int count;
    value_t* values;
} value_array_t;

void l_init_value_array(value_array_t* array);
void l_write_value_array(value_array_t* array, value_t value);
void l_free_value_array(value_array_t* array);

void l_print_value(value_t value);

#endif