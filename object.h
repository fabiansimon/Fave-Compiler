//
// Created by Fabian Simon on 03.10.23.
//

#ifndef FAVE_CUH_OBJECT_H
#define FAVE_CUH_OBJECT_H

#include "common.h"
#include "value.h"

#define OBJ_TYPE(val)       (AS_OBJ(val)->type)

#define IS_STRING(val)      is_obj_type(value, OBJ_STRING)

#define AS_STRING(val)      ((ObjString*)AS_OBJ((val))
#define AS_CSTRING(val)      (((ObjString*)AS_OBJ(val))->chars)

typedef enum {
    OBJ_STRING,
} ObjType;

struct Obj {
    ObjType type;
};

struct ObjString {
    Obj obj;
    int length;
    char* chars;
};

ObjString* copy_string(const char* string, int length);

static inline bool is_obj_type(Value val, ObjType type) {
    return IS_OBJ(val) && AS_OBJ(val)->type == type;
}

#endif //FAVE_CUH_OBJECT_H
