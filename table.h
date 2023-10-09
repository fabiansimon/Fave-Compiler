//
// Created by Fabian Simon on 06.10.23.
//

#ifndef FAVE_CUH_TABLE_H
#define FAVE_CUH_TABLE_H

#include "common.h"
#include "value.h"

typedef struct Entry {
    ObjString* key;
    Value val;
} Entry;

typedef struct {
    int count;
    int capacity;
    Entry* entries;
} Table;

void init_table(Table* table);
void free_table(Table* table);
bool table_get(Table* table, ObjString* key, Value* val);
bool table_set(Table* table, ObjString* key, Value val);
bool table_delete(Table* table, ObjString* key);
void table_add_all(Table* from , Table* to);
ObjString* table_find_string(Table* table, const char* chars, int length, uint32_t hash);


#endif //FAVE_CUH_TABLE_H
