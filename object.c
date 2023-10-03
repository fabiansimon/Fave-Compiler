//
// Created by Fabian Simon on 03.10.23.
//

#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, object_type) \
    (type*)allocate_object(sizeof(type), object_type)


static Obj* allocate_object(size_t size, ObjType type) {
    Obj* object = (Obj*) reallocate(NULL, 0, size);
    object->type = type;
    return object;
}

static ObjString* allocate_string(char* chars, int length) {
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    return string;
}

ObjString* copy_string(const char* chars, int lenght) {
    char* heap_chars = ALLOCATE(char, lenght+1);
    memcpy(heap_chars, chars, lenght);
    heap_chars[lenght] = '\0';
    return allocate_string(heap_chars, lenght);
}

void print_object(Value val) {
    switch (OBJ_TYPE(val)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(val));
            break;
    }
}