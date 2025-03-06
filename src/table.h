#ifndef table_h_hammer
#define table_h_hammer

#include "common.h"
#include "value.h"

typedef struct {
    ObjString* key;
    Value value;
} Entry;

typedef struct {
    int count;
    int capacity;
    Entry* entries;
} Table;

void initTable(Table* table);
void freeTable(VM* vm, Table* table);
bool tableAddEntry(VM* vm, Table* table, ObjString* key, Value value);
bool tableDeleteEntry(Table* table, ObjString* key);
bool pingTable(Table* table, const char* str, size_t length);
ObjString* tableFindString(Table* table, const char* chars, size_t length, uint32_t hash);
Entry* tableGetEntry(Table* table, ObjString* key);

void printTable(Table* table);
void tableRemoveWhite(Table* table);
void markTable(VM* vm, Table* table);

void doToAllEntries(Table* table, void (*algo)(Entry* entry));

#endif