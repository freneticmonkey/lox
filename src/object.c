#include <stdio.h>
#include <string.h>

#include "lib/memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) \
    (type*)_allocate_object(sizeof(type), objectType)

static obj_t* _allocate_object(size_t size, ObjType type) {
    obj_t* object = (obj_t*)reallocate(NULL, 0, size);
    object->type = type;

    object->next = vm.objects;
    vm.objects = object;
    return object;
}

static obj_string_t* _allocate_string(char* chars, int length, uint32_t hash) {
    obj_string_t* string = ALLOCATE_OBJ(obj_string_t, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    l_table_set(&vm.strings, string, NIL_VAL);
    return string;
}

static uint32_t _hash_string(const char* key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
return hash;
}

obj_string_t* l_take_string(char* chars, int length) {
    uint32_t hash = _hash_string(chars, length);

    obj_string_t* interned = l_table_find_string(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }
    return _allocate_string(chars, length, hash);
}

obj_string_t* l_copy_string(const char* chars, int length) {
    uint32_t hash = _hash_string(chars, length);

    obj_string_t* interned = l_table_find_string(&vm.strings, chars, length, hash);
    if (interned != NULL) 
        return interned;

    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return _allocate_string(heapChars, length, hash);
}

void l_print_object(value_t value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
    }
}