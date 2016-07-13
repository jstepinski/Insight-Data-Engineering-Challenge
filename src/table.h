#ifndef _table_h
#define _table_h

typedef void (*dataCleanFn)(void* addr);

typedef struct tablePrototype table;

// create the table by specifying the size of the data (List*), 
// the initial capacity (set in venmoGraphParams), and the cleaner function (our list remover)
table* table_create(int dataSize, int initCapacity, dataCleanFn fn);

// destroy the table
void table_destroy(table* T);

// recover the number of cells (nodes), i.e. the number of unique entries in the table (not hash cells)
int table_count(table* T);

// check the load of the table and rehash if necessary
int table_checkLoad(table* T);

// add an entry to the table
// the key must be a string
// as a void*, the addr can be the address of any data type, but in our project, we use List**
void* table_put(table* T, char* key, void* addr);

// get the address of a cell of memory from the table
// then use that address to get the key
// since the key is a string whose length varies, we need 
// it and the original address to get the actual datum 
void* table_getCell(table* T, char* key);
void* table_getKey(void* cell);
void* table_getDatum(table* T, void* cell, char* key);

// remove an entry from the table by specifying the key
void table_remove(table* T, char* key);

// iterate through the entries in the table
// begin by setting cell = firstCell and key = getKey(cell)
// then in a loop update cell = nextCell(T,cell,key) and key = getKey(cell).
void* table_firstCell(table* T);
void* table_nextCell(table* T, void* prevCell, char* key);

#endif
