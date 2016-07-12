#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "list.h"
#include "table.h"
#include "venmoGraphParams.h"

unsigned long int GLOBAL_MAX_TIME = 0;

extern int* List_lenActFreq;
extern int  List_lenActFreq_size;

static void dataCleaner(void* p) {
	free(*(unsigned long int**)p);
}

static void listCleaner(void* p) {
	List_destroy(*(List**)p);
}

void updateGraph(table* T) {

	// Alg: go through all the cells in T
	// Be careful when a cell is deleted; use a while loop!
	
	void* curCell = table_firstCell(T);
	void* curBlock;
	char* key;
	char* name;
	List* activeL;
	List* linkedL;
	
	int i,j,n;
	
	while (curCell != NULL) {
		key = (char*)table_getKey(curCell);
		activeL = *(List**)table_getDatum(T, curCell, key);
		
		if (List_lenAct(activeL) == 0) {
			table_remove(T, key);
		}
		else {
			// go down the list. as soon as hit bad time, delete all after it
			curBlock = List_firstBlock(activeL);
			n = List_lenRec(activeL);
			for (i=0; i<n; i++) {
				
				// if the timestamp of the current block is too old, erase it and all that follow
				if (GLOBAL_MAX_TIME - (**(unsigned long int**)List_getDatum(activeL, curBlock)) > MAX_AGE) {
					for (j=i; j<n; j++) {
						name = *(char**)List_getName(curBlock);
						linkedL = *(List**)(name + strlen(name) + 1);
						List_incLenAct(linkedL,-1);
						if (List_lenAct(linkedL) == 0) {
							table_remove(T,name);
						}
						curBlock = List_remove(activeL, curBlock);
					}
					break;
				}
				// otherwise go on to the next block
				curBlock = List_nextBlock(curBlock);
			}
		}	
		curCell = table_nextCell(T, curCell, key);
		// If we deleted all the entries in the active list, check whether the actual length is 0, and if it is, then remove the whole cell
		if (List_lenRec(activeL)==0 && List_lenAct(activeL)==0) {
			table_remove(T, key);
		}
	}
}

void parseEntry(char* str, unsigned long int** T, char* actor, char* target) {

	int year = -1, month = -1, day = -1, hour = -1, minute = -1, second = -1;
	
	sscanf(str, "{\"created_time\": \"%d-%d-%dT%d:%d:%dZ\", \"target\": \"%[^\"]\", \"actor\": \"%[^\"]", &year, &month, &day, &hour, &minute, &second, target, actor);
	
	if ( year == -1 || month == -1 || day == -1 || hour == -1 || minute == -1 || second == -1 )
		**T = 0;
	else {
		struct tm timeDetails = { .tm_year = year - 1900,
								  .tm_mon = month - 1,
								  .tm_mday = day,
								  .tm_hour = hour,
								  .tm_min = minute,
								  .tm_sec = second,
								  .tm_isdst = -1
		};
		
		time_t tT = mktime(&timeDetails);
		**T = (unsigned long int) tT;
	}
}

int intCmpFn (const void * a, const void * b) {
   return ( *(int*)a - *(int*)b );
}

float naiveMedian(table* T) {

	int n = table_count(T);

	float* lenArr = malloc(n*sizeof(float));
	float median;
	
	void* cell;
	char* key;
	cell = table_firstCell(T);
	
	int i;
	for (i=0; i<n; i++) {
		key = (char*)table_getKey(cell);
		lenArr[i] = (float)List_lenAct(*(List**)table_getDatum(T, cell, key));
		cell = table_nextCell(T,cell,key);
	}

	qsort(lenArr, n, sizeof(int), intCmpFn);
	
	if (n % 2) {
		median = ( lenArr[(n-1)/2] );
	}
	else {
		int ind = n/2;
		median = ( (lenArr[ind]+lenArr[ind-1])/2 );
	}	
	free(lenArr);
	return median;
}

float fastMedian(int tot) {

	float halfTot = ((float)tot)/2;

	int i,j;
	
	float sum = 0;
	for (i=0; i<List_lenActFreq_size; i++) {
		if (List_lenActFreq[i] != 0) {
			
			sum = sum + (float)List_lenActFreq[i];
			
			if ( fabs(sum - halfTot) < 0.00001 ) {
				for (j = i+1; j<List_lenActFreq_size; j++) {
					if (List_lenActFreq[j] > 0) {
						break;
					}
				}
				return (((float)(i+j+2))/2);
			}
			if (sum > halfTot) {
				return ((float) (i+1));
			}
		}
	}
	return 0;
}

void printGraph(table* T) {

	printf("\n\n*************************\n");
	printf("******PRINTING GRAPH*****\n\n");
	
	printf("There are %d nodes in the graph\n",table_count(T));
	
	void* cell;
	char* key;
	void* block;
	char name[MAX_STR_LEN];
	unsigned long int timeStamp;
	List* LST;
	int i,j;
	
	cell = table_firstCell(T);
	
	for (i=0; i<table_count(T); i++) {
	
		key = (char*)table_getKey(cell);
		LST = *(List**)table_getDatum(T,cell,key);
		
		printf("List %s:\tlenRec = %d,\tlenAct = %d\n",key,List_lenRec(LST),List_lenAct(LST));
		
		block = List_firstBlock(LST);
		
		printf("\tList contents\n");
		for(j=0; j<List_lenRec(LST); j++) {
			
			name[0] = '\0';
			strcpy(name,*(char**)List_getName(block));
			timeStamp = **(unsigned long int**)List_getDatum(LST, block);
			printf("\t\tTarget %s @ %ld\n",name,timeStamp);
			
			block = List_nextBlock(block);
		}
	
		printf("------------------------------\n");
		cell = table_nextCell(T,cell,key);
	}
}

int main(int argc, char* argv[]) {
	
	FILE* fp_out;
	FILE* fp_in;
	
	int medianAlg = 2;	// Set to 1 to use the naiveMedian algorithm
	
	int entryCounter = 1;
	int printEntry = 0;
	
	clock_t timeBeg, timeEnd;
	float medianCompTime = 0;
	
	switch (argc) {
		case 1:
			fp_in = fopen("input.txt","r");
			if (fp_in == NULL) { printf("\n\nERROR: default input file could not be opened\n\n"); exit(0); }
			fp_out = fopen("output.txt","w");
			if (fp_out == NULL) { printf("\n\nERROR: default output file could not be opened\n\n"); fclose(fp_in); exit(0); }
			break;
		case 2:
			fp_in = fopen(argv[1],"r");
			if (fp_in == NULL) { 
				printf("\n\nERROR: user input file could not be opened\n\n"); 
				exit(0); 
			}
			fp_out = fopen("output.txt","w");
			if (fp_out == NULL) { 
				printf("\n\nERROR: default output file could not be opened\n\n"); 
				fclose(fp_in); 
				exit(0);
			}
			break;
		case 3:
			fp_in = fopen(argv[1],"r");
			if (fp_in == NULL) { 
				printf("\n\nERROR: user input file could not be opened\n\n"); 
				exit(0); 
			}
			fp_out = fopen(argv[2],"w");
			if (fp_out == NULL) { 
				printf("\n\nERROR: user output file could not be opened\n\n");  
				fclose(fp_in); 
				exit(0);
			}
			break;
		case 4:
			fp_in = fopen(argv[1],"r");
			if (fp_in == NULL) { 
				printf("\n\nERROR: user input file could not be opened\n\n"); 
				exit(0); 
			}
			fp_out = fopen(argv[2],"w");
			if (fp_out == NULL) { 
				printf("\n\nERROR: user output file could not be opened\n\n"); 
				fclose(fp_in);
				exit(0); 
			}
			medianAlg = atoi(argv[3]);
			if (medianAlg != 1 && medianAlg != 2) { 
				printf("\n\nERROR: invalid median algorithm; set 1 or 2\n\n"); 
				fclose(fp_in);
				fclose(fp_out);
				exit(0);
			}
			break;
		case 5:
			fp_in = fopen(argv[1],"r");
			if (fp_in == NULL) { 
				printf("\n\nERROR: user input file could not be opened\n\n"); 
				exit(0); 
			}
			fp_out = fopen(argv[2],"w");
			if (fp_out == NULL) { 
				printf("\n\nERROR: user output file could not be opened\n\n"); 
				fclose(fp_in);
				exit(0); 
			}
			medianAlg = atoi(argv[3]);
			if (medianAlg != 1 && medianAlg != 2) { 
				printf("\n\nERROR: invalid median algorithm; set 1 or 2\n\n"); 
				fclose(fp_in);
				fclose(fp_out);
				exit(0);
			}
			printEntry = atoi(argv[4]);
			break;
		default:
			printf("\nERROR: faulty number of inputs\n\n");
			exit(0);
			break;
	}
	
	void* cellA;
	void* cellT;
	char* keyT;
	
	void* checkBlock;
	unsigned long int checkTime;
	
	table* TLG = table_create(sizeof(List**), INITIAL_TABLE_SIZE, listCleaner);
	
	List* LA;
	List* LT;
	
	List_lenActFreq_initalize();
	
	float median;
	
	char actor[MAX_STR_LEN];
	char target[MAX_STR_LEN];
	
	unsigned long int* time;
	char nameA[MAX_STR_LEN];
	char nameT[MAX_STR_LEN];
	
	char line[500];
	while ( fgets(line,500,fp_in) != NULL ) {
		
		target[0] = '\0';
		actor[0] = '\0';
		time = malloc(sizeof(unsigned long int));
		*time = 0;
		
		parseEntry(line, &time, actor, target);
		
		if (actor[0]=='\0' || target[0]=='\0' || *time==0)
			continue;
		else
		{
		
			if (*time > GLOBAL_MAX_TIME) {
				GLOBAL_MAX_TIME = *time;
			}
			if (GLOBAL_MAX_TIME - *time > MAX_AGE) {
				if (medianAlg == 1) {
					timeBeg = clock();
					median =  naiveMedian(TLG);
					timeEnd = clock();
				}
				if (medianAlg == 2)	{
					timeBeg = clock();
					median = fastMedian(table_count(TLG));
					timeEnd = clock();
				}
				medianCompTime = medianCompTime + (float)(((float)timeEnd - (float)timeBeg)/CLOCKS_PER_SEC);
				fprintf(fp_out, "%.2f\n", median);
	
				continue;
			}
			
			strcpy(nameA,actor);
			strcpy(nameT,target);
			
			cellA = table_getCell(TLG, nameA);
			cellT = table_getCell(TLG, nameT);
					
			if (cellA == NULL) {
				LA = List_create(sizeof(char**), sizeof(long int*), dataCleaner);
				table_put(TLG, nameA, &LA);
			}
			else {
				LA = *(List**)table_getDatum(TLG, cellA, nameA);
			}
			
			if (cellT == NULL) {
				LT = List_create(sizeof(char**), sizeof(long int*), dataCleaner);
				keyT = table_put(TLG, nameT, &LT);
			}
			else {
				LT = *(List**)table_getDatum(TLG, cellT, nameT);
				keyT = table_getKey(cellT);
			}
			
			checkBlock = List_getBlock(LA, keyT);
			checkTime = 0;
			
			if (checkBlock != NULL) {
				checkTime = **(unsigned long int**)List_getDatum(LA, checkBlock);
			}
			
			if (*time > checkTime) {
				if (checkBlock != NULL) {
					List_remove(LA, checkBlock);
					List_incLenAct(LT, -1);
				}
				List_put(LA, &keyT, &time);
				List_incLenAct(LT, 1);
			}
			
			table_checkLoad(TLG);
			
			updateGraph(TLG);
			
			if (medianAlg == 1) {
				timeBeg = clock();
				median =  naiveMedian(TLG);
				timeEnd = clock();
			}
			if (medianAlg == 2)	{
				timeBeg = clock();
				median = fastMedian(table_count(TLG));
				timeEnd = clock();
			}
			medianCompTime = medianCompTime + (float)(((float)timeEnd - (float)timeBeg)/CLOCKS_PER_SEC);
			fprintf(fp_out, "%.2f\n", median);
		}
		
		if (printEntry == entryCounter) {
			printGraph(TLG);
			printf("\n\n");
		}
			
		entryCounter++;
	}
	
	fclose(fp_in);
	fclose(fp_out);

	List_lenActFreq_destroy();
	
	printf("\nTotal median computation time:\t%.8f seconds\n\n",medianCompTime);
	
	return 0;
}