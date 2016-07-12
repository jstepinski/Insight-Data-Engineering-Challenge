#ifndef _list_h
#define _list_h

typedef void (*cleanListFn) (void *addr);

typedef struct ListPrototype List;

List* List_create(int keySize, int dataSize, cleanListFn fn);

void List_destroy(List* L);

void List_lenActFreq_initalize();
void List_lenActFreq_destroy();

void LIST_UPDATE_FREQS(int len, int inc);

int List_lenAct(List* L);
int List_lenRec(List* L);

void List_incLenAct(List* L, int inc);
void List_incLenRec(List* L, int inc);

void List_put(List* L, void* keyAddr, void* datum);

void* List_getBlock(List* L, char* name);
void* List_getName(void* block);
void* List_getDatum(List* L, void* block);  

void* List_remove(List* L, void* block);

void* List_firstBlock(List* L);
void* List_nextBlock(void* block);

#endif