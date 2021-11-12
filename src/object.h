#ifndef LOX_OBJECT_H
#define LOX_OBJECT_H

#include "common.h"
#include "chunk.h"
#include "value.h"

#define OBJ_TYPE(value)   (AS_OBJ(value)->type)
#define IS_CLOSURE(value)l_is_obj_type(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)l_is_obj_type(value, OBJ_FUNCTION)
#define IS_NATIVE(value)  l_is_obj_type(value, OBJ_NATIVE)
#define IS_STRING(value)  l_is_obj_type(value, OBJ_STRING)

#define AS_CLOSURE(value) ((obj_closure_t*)AS_OBJ(value))
#define AS_FUNCTION(value)((obj_function_t*)AS_OBJ(value))
#define AS_NATIVE(value)  (((obj_native_t*)AS_OBJ(value))->function)
#define AS_STRING(value)  ((obj_string_t*)AS_OBJ(value))
#define AS_CSTRING(value) (((obj_string_t*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_STRING,
} ObjType;

struct obj_t{
    ObjType       type;
    struct obj_t* next;
};

typedef struct {
    obj_t obj;
    int   arity;
    chunk_t chunk;
    obj_string_t* name;
} obj_function_t;

typedef value_t (*native_func_t)(int argCount, value_t *args);

typedef struct {
    obj_t         obj;
    native_func_t function;
} obj_native_t;

struct obj_string_t {
    obj_t    obj;
    int      length;
    char*    chars;
    uint32_t hash;
};

typedef struct
{
    obj_t           obj;
    obj_function_t* function;

} obj_closure_t;

obj_closure_t*  l_new_closure(obj_function_t* function);
obj_function_t* l_new_function();
obj_native_t*   l_new_native(native_func_t function);
obj_string_t*   l_take_string(char* chars, int length);
obj_string_t*   l_copy_string(const char* chars, int length);

void l_print_object(value_t value);

static inline bool l_is_obj_type(value_t value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif