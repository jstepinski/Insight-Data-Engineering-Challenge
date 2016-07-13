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

	// begin by setting block to the list header, then loop over the recorded
	// length of the list and proceed to the next block by dereferencing the 
	// current block
	
	// a list block has the following structure:
	//    | blkAddr | nameAddr | timestamp |
	// "block" is the address of blkAddr
	// to get to the nameAddr, we skip over the blkAddr 
	// we dereference the nameAddr to get the name, and if it matches the 
	// person, we return the block addr.

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

	// remember the format of a list entry in memory:
	//  | block | name | time |

	// we want to add a new entry to list L
	// the entry will have keyAddr in the name. 
	// we call it keyAddr because it is the address of the key in of a different cell in the table that contains these lists
	// the time will be the heap address of the timestamp

	void* curBlock;
	void* newBlock;
	void* nxtBlock;
	
	unsigned long int timeStampNew = **(unsigned long int**)datum;
	unsigned long int timeStampOld;
	
	List_incLenRec(L,1);
	List_incLenAct(L,1);
	
	// the current block is the address of the header
	// the new block will be our new entry
	// the next block is the header.
	
	curBlock = &(L->header);
	newBlock = malloc(sizeof(void**) + L->keySize + L->dataSize);
	nxtBlock = L->header;
	
	// we begin with the header. if the list is empty, the header is empty, and we never enter the loop
	// we want our list to be sorted chronologically so that all old entries can be deleted quickly
	// to that end, we extract the time of the block. 
	// if the time is older than our time, we will put our entry on top of this block
	// otherwise we continue to the next block
	
	while (*(void**)nxtBlock != NULL) {
		timeStampOld = **(unsigned long int**)List_getDatum(L, nxtBlock);
		if (timeStampOld < timeStampNew)
			break;
		else {
			curBlock = nxtBlock;
			nxtBlock = *(void**)nxtBlock;
		}
	}
	
	// Ensure that the address in the new block references the location of the
	// next block.
	
	*(void**)newBlock = nxtBlock;
	*(void**)curBlock = newBlock;
	
	// Now that we have found a spot for our entry, 
	// we compute the locations of the key and the datum, 
	// and we copy the key and datum there
	
	void* keyDest = (char*)newBlock + sizeof(void*);
	void* datDest = (char*)newBlock + sizeof(void*) + L->keySize;
	
	memcpy(keyDest, keyAddr, L->keySize);
	memcpy(datDest, datum, L->dataSize);
	
}

int* List_lenActFreq;
int  List_lenActFreq_size = INIT_MAX_LEN;

void List_lenActFreq_initalize() {
	List_lenActFreq = calloc(List_lenActFreq_size, sizeof(int)); // use calloc to ensure all entries are 0
}

void List_lenActFreq_destroy() {
	free(List_lenActFreq);
}

void LIST_UPDATE_FREQS(int len, int inc) {												
	
	// here we update the array of degree frequencies. let's shorten the name of this array to DF for the comments
	// in our computer representation of a graph, the list is a branch, and its length len is the degree
	// index x of DF represents length len = x+1. DF[x] is the frequency of len
	// we want to increase the frequency of len by inc
	// this is easy unless len is greater than the array size
	// in that case we must reallocate the array to a larger block of memory
	// simply doing DF = realloc(DF) would not suffice, as it would not allow us to free(DF) when the program is complete
	// instead we realloc to a temporary array, calloc DF to a new block, then memmove only the original elements
	// memmove'ing all the elements would result in use moving garbage, as the temp was not calloc'd
	
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
	
	// whenever the ACTUAL length is changed, the degree of the node changes
	// if len increases by 1, we must first decrease the frequency of len, then increase the frequency of len+1
	// but be careful when len becomes 0!
	
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
	
	// unlike in the table, where new entries are simply placed on top of old ones, 
	// entries in a list are organized chronologically
	// we want to preserve this order and avoid the cost of traversing the whole list in search of our block
	// so we just begin at our block, dereference it to get the next block, then copy the next block into our block
	
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
