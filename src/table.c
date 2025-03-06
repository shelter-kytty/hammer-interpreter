#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "table.h"
#include "object.h"
#include "value.h"
#include "memory.h"

#define TABLE_MAX_LOAD 0.7

void initTable(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeTable(VM* vm, Table* table) {
    FREE_ARRAY(vm, table->entries, table->capacity, Entry);
    initTable(table);
}

static Entry* tableFindEntry(Entry* entries, int capacity, ObjString* key) {
    Entry* steppingStone = NULL;

    for (uint32_t i = 1, index = (key->hash + 1) % capacity; ; ++i, index = (key->hash + i*i + index) % capacity) {
        Entry* entry = &entries[index];

        if (entry->key == NULL) {
            if (IS_UNIT(entry->value)) {
                return steppingStone != NULL ? steppingStone : entry;
            } else {
                steppingStone = (steppingStone != NULL) ? steppingStone : entry;
            }
        } else if (entry->key == key) {
            return entry;
        }
    }
}

static void growTable(VM* vm, Table* table, int new_capacity) {
    Entry* entries = ALLOCATE(vm, new_capacity, Entry);
    for (Entry* e = entries; e < entries + new_capacity; e++) {
        e->key = NULL;
        e->value = UNIT_VAL;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;

        Entry* dest = tableFindEntry(entries, new_capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(vm, table->entries, table->capacity, Entry);
    table->entries = entries;
    table->capacity = new_capacity;
}

bool pingTable(Table* table, const char* str, size_t length) {
    if (table->count == 0) return false;

    uint32_t h = 0, high;
    const char* s = str;
    while (s < str + length) {
        h = (h << 4) + *s++;
        if (high = h & 0xF0000000)
            h ^= high >> 24;
        h &= ~high;
    }

    for (uint32_t i = 1, index = (h + 1) % table->capacity; ; ++i, index = (h + i*i + index) % table->capacity) {
        Entry* entry = &table->entries[index];

        if (entry->key == NULL) {
            if (IS_UNIT(entry->value)) return false;
        } else if (
            (entry->key->length == length) &&
            (entry->key->hash   == h)      &&
            (*entry->key->chars == *str)   &&
            (memcmp(entry->key->chars, str, length) == 0) 
            ) {
            return true;
        }
    }
}

ObjString* tableFindString(Table* table, const char* chars, size_t length, uint32_t hash) {
    if (table->count == 0) return NULL;

    #ifdef DEBUG_STRING_DETAILS
    printf("Hash is : %d :: Capacity is : %d \n", hash, table->capacity);
    #endif

    for (uint32_t i = 1, index = (hash + 1) % table->capacity; ; ++i, index = (hash + i*i + index) % table->capacity) {
        Entry* entry = &table->entries[index];

        #ifdef DEBUG_STRING_DETAILS
        printf("Trying entry '%d'\n", index);
        #endif

        if (entry->key == NULL) {
            if (IS_UNIT(entry->value)) {
                #ifdef DEBUG_STRING_DETAILS
                printf("Entry '%d' was empty\n", index);
                #endif
                return NULL;
            }
            #ifdef DEBUG_STRING_DETAILS
            printf("Entry '%d' was a tombstone\n", index);
            #endif
        } else if (
            (entry->key->length == length) &&
            (entry->key->hash   == hash)   &&
            (*entry->key->chars == *chars) &&
            (memcmp(entry->key->chars, chars, length) == 0) 
            ) {
            #ifdef DEBUG_STRING_DETAILS
            printf("Entry '%d' was a match\n", index);
            #endif
            return entry->key;
        }
    }
}

bool tableAddEntry(VM* vm, Table* table, ObjString* key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAP(table->capacity);
        growTable(vm, table, capacity);
    }

    Entry* entry = tableFindEntry(table->entries, table->capacity, key);
    bool isNewEntry = (entry->key == NULL);
    if (isNewEntry && IS_UNIT(entry->value)) table->count++;

    entry->key = key;
    entry->value = value;

    return isNewEntry;
}

bool tableDeleteEntry(Table* table, ObjString* key) {
    if (table->count == 0) return false;

    Entry* entry = tableFindEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}

Entry* tableGetEntry(Table* table, ObjString* key) {
    if (table->count == 0) return NULL;

    Entry* entry = tableFindEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return NULL;

    return entry;
}

void printTable(Table* table) {
    printf("%d : %d\n", table->count, table->capacity);
    for (int i = 0; i < table->capacity; i++) {
        Entry entry = table->entries[i];
        if (entry.key == NULL) {
            printf("NULL : N\\A  |  ");
        }
        else {
            printf("%p  %s : %d  |  ", entry.key, entry.key->chars, entry.key->hash);
        }
        printValue(entry.value);
        printf("\n");
    }
}

void printEntry(Entry* entry) {
    printf("%s => ", entry->key->chars);
    printValue(entry->value);
}

void tableRemoveWhite(Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key != NULL && entry->key->obj.colour == MEM_WHITE) {
            #ifdef DEBUG_LOG_GC
            printf("Removing interned %p : %s\n", (void*)entry->key, entry->key->chars);
            #endif
            tableDeleteEntry(table, entry->key);
        }
    }
}

void markTable(VM* vm, Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        markObject(vm, (Obj*)entry->key);
        markValue(vm, entry->value);
    }
}

void doToAllEntries(Table* table, void (*algo)(Entry* entry)) {
    for (int i = 0; i < table->count; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key != NULL) {
            algo(entry);
        }
    }
}