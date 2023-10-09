//
// Created by Fabian Simon on 06.10.23.
//

#include <stdlib.h>
#include <string.h>

#include "table.h"
#include "memory.h"
#include "value.h"
#include "memory.h"

#define TABLE_MAX_LOAD 0.75

void init_table(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void free_table(Table* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    init_table(table);
}

static Entry* find_entry(Entry* entries, int capacity, ObjString* key) {
    uint32_t idx = key->hash % capacity;
    Entry* tombstone = NULL;

    for (;;) {
        Entry* entry = &entries[idx];
        if (entry->key == NULL) {
            if (IS_NIL(entry->val)) {
                return tombstone != NULL ? tombstone : entry; // Empty entry
            } else {
                if (tombstone == NULL) tombstone = entry;
            }
        } else if (entry->key == key) return entry;

        idx = (idx+1) % capacity;
    }
}

bool table_get(Table* table, ObjString* key, Value* val) {
    if (table->count == 0) return false;

    Entry* entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    *val = entry->val;
    return true;
}

static void adjust_capacity(Table* table, int capacity) {
    Entry* entries = ALLOCATE(Entry, capacity);

    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].val = NIL_VAL;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;

        Entry* dest = find_entry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->val = entry->val;
        table->count++;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity);

    table->entries = entries;
    table->capacity = capacity;
}

bool table_set(Table* table, ObjString* key, Value val) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjust_capacity(table, capacity);
    }

    Entry* entry = find_entry(table->entries, table->capacity, key);
    bool is_new_key = entry->key == NULL;
    if (is_new_key && IS_NIL(entry->val)) table->count++;

    entry->key = key;
    entry->val = val;
    return is_new_key;
}

bool table_delete(Table* table, ObjString* key) {
    if (table->count == 0) return false;

    Entry* entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    entry->key = NULL;
    entry->val = BOOL_VAL(true);
    return true;
}

void table_add_all(Table* from , Table* to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];
        if (entry->key != NULL) table_set(to, entry->key, entry->val);
    }
}

ObjString* table_find_string(Table* table, const char* chars, int length, uint32_t hash) {
    if (table->count == 0) return NULL;

    uint32_t idx = hash % table->capacity;

    for (;;) {
        Entry* entry = &table->entries[idx];
        if (entry->key == NULL) {
            if (IS_NIL(entry->val)) return NULL; // Empty non-tombstone entry
        } else if (entry->key->length == length
                    && entry->key->hash == hash
                   && memcmp(entry->key->chars, chars, length) == 0) return entry->key;

        idx = (idx+1) % table->capacity;
    }

}