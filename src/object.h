#ifndef LOX_OBJECT_H
#define LOX_OBJECT_H

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value)   (AS_OBJ(value)->type)
#define IS_STRING(value)  l_is_obj_type(value, OBJ_STRING)
#define AS_STRING(value)  ((obj_string_t*)AS_OBJ(value))
#define AS_CSTRING(value) (((obj_string_t*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_STRING,
} ObjType;

struct obj_t{
    ObjType type;
    struct obj_t* next;
};

struct obj_string_t {
  obj_t obj;
  int   length;
  char* chars;
};

obj_string_t* l_take_string(char* chars, int length);
obj_string_t* l_copy_string(const char* chars, int length);

void l_print_object(value_t value);

static inline bool l_is_obj_type(value_t value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif