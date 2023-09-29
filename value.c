//
// Created by Fabian Simon on 29.09.23.
//

#include <stdio.h>

#include "value.h"
#include "memory.h"

void init_value_array(ValueArray* arr) {
    arr->values = NULL;
    arr->capacity = 0;
    arr->count = 0;
}

void write_value_array(ValueArray* arr, Value val) {
    if (arr->capacity < arr->count+1) {
        int old_capacity = arr->capacity;
        arr->capacity = GROW_CAPACITY(old_capacity);
        arr->values = GROW_ARRAY(Value, arr->values, old_capacity, arr->capacity);
    }

    arr->values[arr->count] = val;
    arr->count++;
}

void free_value_array(ValueArray* arr) {
    FREE_ARRAY(Value, arr->values, arr->capacity);
    init_value_array(arr);
}

void print_value(Value value) {
    printf("%g", value);
}