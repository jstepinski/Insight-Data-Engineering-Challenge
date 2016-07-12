#ifndef _table_h
#define _table_h

typedef void (*dataCleanFn)(void* addr);

typedef struct tablePrototype table;

table* table_create(int dataSize, int initCapacity, dataCleanFn fn);

void table_destroy(table* T);

int table_count(table* T);

int table_checkLoad(table* T);

void* table_put(table* T, char* key, void* addr);

void* table_getCell(table* T, char* key);
void* table_getKey(void* cell);
void* table_getDatum(table* T, void* cell, char* key);

void table_remove(table* T, char* key);

void* table_firstCell(table* T);
void* table_nextCell(table* T, void* prevCell, char* key);

#endif
