#include <stdlib.h>

#include "lib/memory.h"
#include "vm.h"

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, newSize);
    if (result == NULL) 
        exit(1);
    return result;
}

static void _free_object(obj_t* object) {
    switch (object->type) {
        case OBJ_FUNCTION: {
            obj_function_t* function = (obj_function_t*)object;
            l_free_chunk(&function->chunk);
            FREE(obj_function_t, object);
            break;
        }
        case OBJ_NATIVE: {
            FREE(obj_native_t, object);
            break;
        }
        case OBJ_STRING: {
            obj_string_t* string = (obj_string_t*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(obj_string_t, object);
            break;
        }
    }
}

void l_free_objects() {
    obj_t* object = vm.objects;
    while (object != NULL) {
        obj_t* next = object->next;
        _free_object(object);
        object = next;
    }
}