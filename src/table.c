#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <assert.h>
#include "table.h"
#include "venmoGraphParams.h"

#define MAX_LOAD 0.75

struct tablePrototype {

    void** cells;

	int count_cells;
	int count_elems;
	int dataSize;
	
    float load;

	int (*hashFunc)(char* key, int n); // pointer to the hash function

    dataCleanFn dataDeleter;
};

static int hash(char* s, int nCells) {
	unsigned long MULTIPLIER = 2630849305L;
	unsigned long hashcode = 0;
	for (int i=0; s[i] != '\0'; i++)
		hashcode = hashcode * MULTIPLIER + s[i];
	return hashcode % nCells;
}

table* table_create(int dataSize, int initCapacity,  dataCleanFn fn) {

	table* T = malloc(sizeof(table));

    // Check that memory for the map is available
    assert(T != NULL);

	T->count_cells = initCapacity;
	
	T->count_elems = 0;
	T->dataSize = dataSize;
    T->load = 0;

    T->cells = calloc(T->count_cells, sizeof(void*));

    // Check that memory for the cells could be allocated
    assert(T->cells != NULL);
        
	// set the hash function and any value cleanup function
	T->hashFunc = hash;
    T->dataDeleter = fn;
	
	return T;
}

// deallocate the cmap
void table_destroy(table* T) {

    int i;
    void* aCell;
    void* bCell;
    char* tempKey;
    void* datum;

	// Since each cell potentially contains an entire list of other cells,
	// each of which had to be allocated on the heap, we must iterate 
	// through all the entries and free the keys. if the value was also 
	// allocated on the heap by the user, the cleanup function is called
    for (i=0; i<T->count_cells; ++i) {
        aCell = T->cells[i];
        bCell = aCell;
        while (bCell != NULL) {
            aCell = *(void**)bCell;
            if (T->dataDeleter != NULL) {
                tempKey = (char*)bCell + sizeof(void*);
                datum = (char*)bCell + sizeof(void*) + strlen(tempKey) + 1;
                T->dataDeleter(datum);
            }
            free(bCell);
            bCell = aCell;
        }
    }

	// Every malloc needs a free
    free(T->cells);
    free(T);
}

// Return the number of elements
int table_count(table* T) {
    return T->count_elems;
}

// Rehash gets called from cmap_put if the load of the CMap exceeds the MAX_LOAD
void table_rehash(table* T) {

    int old_count = T->count_cells;
    T->count_cells = T->count_cells*2 + 1;

    void** new_cell_array = realloc(T->cells, (T->count_cells)*sizeof(void*));

    // If the reallocation cannot be performed, return, and the rehash is not performed. 
    if (new_cell_array == NULL) {
        printf("\n\n Table rehashing failed\n\n");
		return;
	}

    void* aCell;
    void* bCell;
    void* nCell;

    int h, i;
    char* key;

    // Initialize the new cells
    for (i=old_count; i<T->count_cells; ++i)
        new_cell_array[i] = NULL;

    // Rehash the old cells
    for (i=0; i<old_count; ++i) {
        aCell = &new_cell_array[i];
        nCell =  new_cell_array[i];
        while (nCell != NULL) {

            key = (char*)nCell + sizeof(void*);
            h = T->hashFunc(key, T->count_cells);

            // If the hash (h) is not the correct root cell (i), we have to move
            // the cell.
            if (h != i) {

                bCell = *(void**)nCell;     // retain in b whatever n points to

                // link nCell into its new slot in the array, and make sure that
                // it points to whatever is already in the array
                *(void**)nCell = new_cell_array[h];
                new_cell_array[h] = nCell;

                // reset nCell to the next element in the list
                nCell = bCell;

                // change the actual address of the previous cell to reflect the
                // fact that its contents have been moved
                *(void**)aCell = bCell;
            }
            else {
                nCell = *(void**)nCell; // just move right down the list
                aCell = *(void**)aCell;
            }
        }
    }
    T->cells = new_cell_array;
}

int table_checkLoad(table* T) {
	T->load = ((float)T->count_elems)/((float)T->count_cells);
	if ((float)(T->load) > MAX_LOAD) {
		table_rehash(T);
		return 1;
	}
	return 0;
}

// add a new element to the map. if the key already exists, the old value
// is cleared by the cleanup function, if it exists. 
// Note that rehashing does not raise an assert if it fails; the map is simply
// used in its old form. HOWEVER, an assert is raised if this "put" function
// fails to allocate memory for the new cell.
void* table_put(table* T, char* key, void* addr) {

	int hashCode = T->hashFunc(key, T->count_cells);

	void* rootCell;
	void* newCell;
	void* nextCell;

	// Check if the key exists in the map already
	void* keyCell = table_getCell(T, key);

	if (keyCell != NULL) {
		rootCell = &keyCell; 
		newCell  = keyCell;
		nextCell = *(void**)keyCell;
	}
	else {
		T->count_elems = T->count_elems + 1;

		rootCell = &(T->cells[hashCode]);
		newCell  = malloc(sizeof(void**) + strlen(key) + 1 + T->dataSize);
		nextCell =  T->cells[hashCode];

        char initializer = '\0';
        memcpy((char*)newCell + sizeof(void*), &initializer, sizeof(char));

		assert(newCell != NULL);
	}

	// Ensure that the address in the new cell references the location of the
    // next cell. Also set the root pointer to point to the new cell
    *(void**)newCell = nextCell; 
	*(void**)rootCell = newCell;
    
    // Copy the key and the value, but first check if the key already exists
    char* keyDest = (char*)newCell + sizeof(void*);
    void* datDest = (char*)newCell + sizeof(void*) + strlen(key) + 1;

    if (strcmp(keyDest, key) == 0) {
        if (T->dataDeleter != NULL)
            T->dataDeleter(datDest);
    }
    else
        strcpy(keyDest, key);

    memcpy(datDest, addr, T->dataSize);
	
	return (void*)keyDest;
}

void* table_getCell(table* T, char* key) {

	int hashCode = T->hashFunc(key, T->count_cells);

    void* aCell = T->cells[hashCode];

    // Continue through the list. If we find the key, return the position of the
    // value. If we run through the whole list and get to the NULL at the end,
    // we return NULL

    while (aCell != NULL) {
        if ( strcmp((char*)aCell + sizeof(void*), key) == 0 )
            return (char*)aCell;
        
        aCell = *(void**)aCell;
    }
    return NULL;
}

void* table_getKey(void* cell) {
	return (char*)cell + sizeof(void*);
}

void* table_getDatum(table* T, void* cell, char* key) {
	return (char*)cell + sizeof(void*) + strlen(key) + 1;
}

// Remove an element from the map. if the key is not found, the map is unchanged
void table_remove(table* T, char* key) {
    
    int hashCode = T->hashFunc(key, T->count_cells);
    void* aCell = &T->cells[hashCode];
    void* pCell = &T->cells[hashCode];

	int keyFound = 0;

	// Find the element AND the address of the previous cell. We need
	// to be able to link the previous cell to the one following the 
	// one we are deleting
    while (aCell != NULL) {
        if ( strcmp((char*)aCell + sizeof(void*), key) == 0 ) {
			keyFound = 1;
            break;
		}

        pCell = aCell;
        aCell = *(void**)aCell;
    }
 
	// End the function if the key could not be found
	if (keyFound == 0)
		return;

    void* datum;
    char* tempKey;
    if (T->dataDeleter != NULL) {
        tempKey = (char*)aCell + sizeof(void*);
        datum = (char*)aCell + sizeof(void*) + strlen(tempKey) + 1;
        T->dataDeleter(datum);
    }

    // Make sure to fully remove the key by copying a terminator into its
    // address
    strcpy((char*)aCell + sizeof(void*), "\0");

    // Link the previous cell to the next one ...
    *(void**)pCell = *(void**)aCell;

    // ... and free the cell in between them
    free(aCell);

    // Decrement the count; don't need to compute load here
    T->count_elems = T->count_elems - 1;
}


void* table_firstCell(table* T) {

	if (T->count_elems == 0)
        return NULL;

    int i = 0;
    while(T->cells[i] == NULL)
        i = i + 1;

    return (void*)T->cells[i];
}

void* table_nextCell(table* T, void* prevCell, char* key) {
	
    int h = T->hashFunc(key, T->count_cells);
	void* cell = *(void**)prevCell;

	if (cell == NULL) {
        h = h + 1;
		if (h >= T->count_cells)
			return NULL;
        while (T->cells[h] == NULL) {
            h = h + 1;
			if (h >= T->count_cells)
				return NULL;
		}
        return (void*)T->cells[h];
    }
    else {
        return cell;
    }
}
