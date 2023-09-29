//
// Created by Fabian Simon on 29.09.23.
//

#ifndef FAVE_CUH_VALUE_H
#define FAVE_CUH_VALUE_H

typedef double Value;

typedef struct {
    int capacity;
    int count;
    Value* values;
} ValueArray;

void init_value_array(ValueArray* arr);
void write_value_array(ValueArray* arr, Value val);
void free_value_array(ValueArray* arr);
void print_value(Value value);


#endif //FAVE_CUH_VALUE_H
