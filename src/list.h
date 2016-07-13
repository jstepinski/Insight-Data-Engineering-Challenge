#ifndef _list_h
#define _list_h

typedef void (*cleanListFn) (void *addr);

typedef struct ListPrototype List;

// create the linked list by specifying the size of the key (char*), 
// the size of the data (long int), and the cleaner function (something to free the data)
List* List_create(int keySize, int dataSize, cleanListFn fn);

// destroy the list
void List_destroy(List* L);

// initialize and free the global frequency array
void List_lenActFreq_initalize();
void List_lenActFreq_destroy();

// update the frequency array
// increase the frequency of list length "len" by "inc"
void LIST_UPDATE_FREQS(int len, int inc);

// recover the actual and recorded lengths of the list
int List_lenAct(List* L);
int List_lenRec(List* L);

// increment the actual and recorded lengths
void List_incLenAct(List* L, int inc);
void List_incLenRec(List* L, int inc);

// add an entry to the list
// as a void*, the keyAddr can be a string (char*), or in our case, a pointer to one (char**)
// add the datum by reference
void List_put(List* L, void* keyAddr, void* datum);

// get the address of a  block of memory from the list
// then use that address to get the name and datum in the block
void* List_getBlock(List* L, char* name);
void* List_getName(void* block);
void* List_getDatum(List* L, void* block);  

// remove an entry from the list by specifying the memory address of its block 
void* List_remove(List* L, void* block);

// iterate through the entries in the list
// begin by setting block = firstBlock. 
// then in a loop update block = nextBlock.
void* List_firstBlock(List* L);
void* List_nextBlock(void* block);

#endif
