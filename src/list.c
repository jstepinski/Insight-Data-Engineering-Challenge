#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#include "list.h"
#include "venmoGraphParams.h"

struct ListPrototype {
	
	void* header;
	
	int length_rec;
	int length_act;
	
	int dataSize;
	int keySize;
	
	cleanListFn dataDeleter;
};

List* List_create(int keySize, int dataSize, cleanListFn fn) {

	assert(dataSize > 0);
	assert(keySize > 0);
	
	List* L = malloc(sizeof(List));
	assert(L != NULL);
	
	L->length_rec = 0;
	L->length_act = 0;
	
	L->keySize = keySize;
	L->dataSize = dataSize;
	
	L->header = calloc(1,sizeof(void*));
	assert(L->header != NULL);

	L->dataDeleter = fn;
	
	return L;
}

void* List_getBlock(List* L, char* person) {

	void* block = L->header;
	
	char** nameAddr;
	char name[MAX_STR_LEN];
	
	for (int i = 0; i<List_lenRec(L); i++) {
		name[0] = '\0';
		nameAddr = (char**)((char*)block + sizeof(void*));
		strcpy(name,*nameAddr);
		
        if ( strcmp(name, person) == 0 )
            return block;
        
        block = *(void**)block;
	}	
	return NULL;
}

void* List_getName(void* block) {
	return (char*)block + sizeof(void*);
}

void* List_getDatum(List* L, void* block) {
	return (char*)block + sizeof(void*) + L->keySize;
}

void* List_firstBlock(List* L) {
	return L->header;
}

void* List_nextBlock(void* block) {
	return *(void**)block;
}

void List_put(List* L, void* keyAddr, void* datum) {

	void* curBlock;
	void* newBlock;
	void* nxtBlock;
	
	unsigned long int timeStampNew = **(unsigned long int**)datum;
	unsigned long int timeStampOld;
	
	List_incLenRec(L,1);
	List_incLenAct(L,1);
	
	curBlock = &(L->header);
	newBlock = malloc(sizeof(void**) + L->keySize + L->dataSize);
	nxtBlock = L->header;
	
	while (*(void**)nxtBlock != NULL) {
		timeStampOld = **(unsigned long int**)List_getDatum(L, nxtBlock);
		if (timeStampOld < timeStampNew)
			break;
		else {
			curBlock = nxtBlock;
			nxtBlock = *(void**)nxtBlock;
		}
	}
	
	*(void**)newBlock = nxtBlock;
	*(void**)curBlock = newBlock;
	
	void* keyDest = (char*)newBlock + sizeof(void*);
	void* datDest = (char*)newBlock + sizeof(void*) + L->keySize;
	
	memcpy(keyDest, keyAddr, L->keySize);
	memcpy(datDest, datum, L->dataSize);
	
}

int* List_lenActFreq;
int  List_lenActFreq_size = INIT_MAX_LEN;

void List_lenActFreq_initalize() {
	List_lenActFreq = calloc(List_lenActFreq_size, sizeof(int));
}

void List_lenActFreq_destroy() {
	free(List_lenActFreq);
}

void LIST_UPDATE_FREQS(int len, int inc) {												
	
	int* temp;
	len--;																			
	while (len > List_lenActFreq_size - 1) {										
		temp = realloc(List_lenActFreq, 2*List_lenActFreq_size); 
		List_lenActFreq = calloc(List_lenActFreq_size*2, sizeof(int));
		
		if (List_lenActFreq == NULL || temp == NULL) {
			printf("\n\nFATAL ERROR: cannot expand array of vertex degree frequencies\n\n");
			abort();
		}
		
		memmove(List_lenActFreq, temp, List_lenActFreq_size*sizeof(int));
		free(temp);
			
		List_lenActFreq_size = List_lenActFreq_size*2;	
	}																				
	List_lenActFreq[len] = List_lenActFreq[len] + inc;	
}

int List_lenAct(List* L) {
	return L->length_act;
}

int List_lenRec(List* L) {
	return L->length_rec;
}

void List_incLenAct(List* L, int inc) {
	
	int len = L->length_act;
	
	if (len != 0) LIST_UPDATE_FREQS(len, -1);

	L->length_act = len + inc;
	
	len = L->length_act;
	
	if (len != 0) LIST_UPDATE_FREQS(len, 1);
}

void List_incLenRec(List* L, int inc) {
	L->length_rec = L->length_rec + inc;
}

void* List_remove(List* L, void* block) {
	
	List_incLenRec(L,-1);
	List_incLenAct(L,-1);
	
	if (L->dataDeleter != NULL) {
		L->dataDeleter(List_getDatum(L, block));
	}
	
	void* nextBlock = *(void**)block;
	
	if (nextBlock != NULL) {
		memcpy(block, nextBlock, sizeof(void*)+L->keySize+L->dataSize);
		free(nextBlock);
		return block;
	}
	else {
		free(block);
		return NULL;
	}
}

void List_destroy(List* L) {
	free(L->header);
	free(L);
}
