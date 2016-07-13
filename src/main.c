#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "list.h"
#include "table.h"
#include "venmoGraphParams.h"

// Global variable equal to the current maximum time stamp
// (this may not be the time stamp of the current entry, as 
//  entries need not arrive chronologically) 
unsigned long int GLOBAL_MAX_TIME = 0;

// Global array and its size, both defined in list.c
// This is the array of frequencies of list lengths (aka node 
// or vertex degrees) used for the fast median algorithm
extern int* List_lenActFreq;
extern int  List_lenActFreq_size;

// Function pointer used by List_destroy to free time stamps
static void dataCleaner(void* p) {
	free(*(unsigned long int**)p);
}

// Function pointer used by table_destroy to free lists
static void listCleaner(void* p) {
	List_destroy(*(List**)p);
}

// update the graph by removing branches whose timestamps are
// too old and nodes which have no branches
void updateGraph(table* T) {

	// Alg: go through all the cells in T
	// Be careful when a cell is deleted; use a while loop!
	
	// Variable naming convention: 
	// table memory is a "cell" identified by its "key"
	// list memory is a "block" identified by its "name"
	void* curCell = table_firstCell(T);
	void* curBlock;
	char* key;
	char* name;
	List* activeL;	// list in the table cell (node)
	List* linkedL;  // list in the cell to which the above cell 
	                // connects (i.e. node at the other end of
					// the branch)
	
	int i,j,n;
	
	// Go through all the cells of the table, but DO NOT use a 
	// for loop that ends with the table size. The size changes
	// as cells are removed within the loop.
	
	while (curCell != NULL) {
		key = (char*)table_getKey(curCell);
		activeL = *(List**)table_getDatum(T, curCell, key);
		
		// If the current cell (node) has no branches, remove it
		if (List_lenAct(activeL) == 0) {
			table_remove(T, key);
		}
		else {
			// go down the list. as soon as hit bad time, delete all after it
			// we can do this because the list is arranged chronologically
			curBlock = List_firstBlock(activeL);
			n = List_lenRec(activeL);
			for (i=0; i<n; i++) {
				
				// if the timestamp of the current block is too old, erase it and all that follow
				if (GLOBAL_MAX_TIME - (**(unsigned long int**)List_getDatum(activeL, curBlock)) > MAX_AGE) {
					for (j=i; j<n; j++) {
						name = *(char**)List_getName(curBlock);
						
						// This is the crucial step and time optimization
						// The name of the list is not actually a string but rather a pointer to a string
						// It points to the address of the key of a completely different cell.
						// We can access that cell's list by dereferencing the pointer, 
						// then shifting over in memory by the length of the string + 1 
						// (for the terminating null)
						// the branch is removed from the graph when the actual length of the list is decremented
						// and if the actual length falls to 0, we can immediately remove BOTH cells at the same time
						
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

// Input parser. The str is the input line. The actor and target
// are extracted to the actor and target strings. The time T is 
// malloc'd in the main function so that its address can be
// stored in a list (it is subsequently freed in the List_remove
// function). We pass the timestamp pointer to the parser by 
// reference so that the parser can modify it, and therefore the
// type is an  int**.
// The parser is explained in detail in the readme, but in short,
// if the input format is not matched exactly, the actor will be
// empty, the target will be empty, or the time will be 0.
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

// integer sorter used by qsort for the naive median algorithm
int intCmpFn (const void * a, const void * b) {
   return ( *(int*)a - *(int*)b );
}

// the naive median, explained in the readme
float naiveMedian(table* T) {

	int n = table_count(T);

	// array whose elements will be the list lengths (vertex degrees)
	float* lenArr = malloc(n*sizeof(float));
	float median;
	
	void* cell;
	char* key;
	cell = table_firstCell(T);
	
	// go through all the cells, get the actual length of each
	int i;
	for (i=0; i<n; i++) {
		key = (char*)table_getKey(cell);
		lenArr[i] = (float)List_lenAct(*(List**)table_getDatum(T, cell, key));
		cell = table_nextCell(T,cell,key);
	}
	
	// sort the array
	qsort(lenArr, n, sizeof(int), intCmpFn);
	
	// get the middle number (median)
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

// fast median algorithm
// tot is the total number of nodes in the graph
float fastMedian(int tot) {

	// The algorithm is explained in detail in the readme.
	// At this point we have a global array List_lenActFreq.
	// The indices of the array represent list lengths (degrees).
	// The element at index x is the frequency of length x+1.
	// Knowing the frequencies and the total (which could compute
	// from the frequencies but already know in the main), we
	// can easily compute the median.
	
	// 0 is returned if a median is not computed successfully. 

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

// Graph printer
void printGraph(table* T) {

	// Prints the entire graph. The format is explained in the
	// readme. An example is also shown. 
	// The printer is useful for debugging, and it can be called
	// by the user with the fourth argument to the executable.

	printf("\n\n*************************\n");
	printf("******PRINTING GRAPH*****\n\n");
	
	printf("There are %d nodes in the graph\n",table_count(T));
	
	// table memory is a "cell" identified by its "key"
	// list memory is a "block" identified by its "name"
	
	void* cell;
	char* key;
	void* block;
	char name[MAX_STR_LEN];
	unsigned long int timeStamp;
	List* LST;
	int i,j;
	
	cell = table_firstCell(T);
	
	// iterate through all the cells (nodes)
	for (i=0; i<table_count(T); i++) {
	
		key = (char*)table_getKey(cell);
		LST = *(List**)table_getDatum(T,cell,key);
		
		// Get a list and the name at the node
		// Print the name and list lengths
		printf("List %s:\tlenRec = %d,\tlenAct = %d\n",key,List_lenRec(LST),List_lenAct(LST));
		
		block = List_firstBlock(LST);
		
		// iterate through the list of the user and print the
		// timestamp and name of everyone with whom that user 
		// has traded and who is actually recorded in the list.
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
						// Can also be set by the user with the 3rd
						// argument to the executable
	
	int entryCounter = 1;	// Increases after every line in the input file
	int printEntry = 0;		// After which entry to print the graph
	
	clock_t timeBeg, timeEnd;	// variables for recording the time
	float medianCompTime = 0;	// the computer takes to compute the
								// median. useful for comparing the
								// algorithms.
								
	// Parse the user inputs
	// First is the input file
	// Second is the output file
	// Third is the median algorithm (1 slow, 2 fast)
	// Fourth is the input file line after which to print the graph
	// exit() rather than abort is used after bad inputs because at 
	// this point the program hasn't done anything a core dump might
	// illuminate
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
	
	void* cellA;	// actor cell
	void* cellT;	// target cell
	char* keyT;		// target key
	char* keyA;		// actor key
	
	void* checkBlockA;
	void* checkBlockT;
	unsigned long int checkTime;
	
	// TABLE_LIST GRAPH (beecause the graph is a table of lists)
	table* TLG = table_create(sizeof(List**), INITIAL_TABLE_SIZE, listCleaner);
	
	List* LA;	// actor list
	List* LT;	// target list
	
	List_lenActFreq_initalize();	// initialize the global array
									// used by the fast median
	float median;
	
	char actor[MAX_STR_LEN];
	char target[MAX_STR_LEN];
	
	unsigned long int* time;
	char nameA[MAX_STR_LEN];	// actor name
	char nameT[MAX_STR_LEN];	// target name
	
	char line[500];	// line of the input file
	
	// go through every line of the input file
	while ( fgets(line,500,fp_in) != NULL ) {
		
		target[0] = '\0';
		actor[0] = '\0';
		time = malloc(sizeof(unsigned long int));
		*time = 0;
		
		// store the timestamp in the heap, and its address in a list
		// we might lose it if it were on the stack, and keeping 
		// certain addresses constant is crucial 
		
		parseEntry(line, &time, actor, target);
		
		// skip the input line if it is faulty
		if (actor[0]=='\0' || target[0]=='\0' || *time==0)
			continue;
		else
		{
			// If the timestamp is the most recent in calendar time,
			// update the global max time
			if (*time > GLOBAL_MAX_TIME) {
				GLOBAL_MAX_TIME = *time;
			}
			
			// If the timestamp is too old, we record the median, 
			// but we don't bother updating the graph
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
			
			// A = actor
			// T = target
			
			strcpy(nameA,actor);
			strcpy(nameT,target);
			
			cellA = table_getCell(TLG, nameA);
			cellT = table_getCell(TLG, nameT);
			
			// IMPORTANT:
			// In the following code, remember that LA and LT are 
			// pointers to lists. That means we can extract them
			// from the table, modify them here locally, and they are
			// automatically updated in the table. So we do not have
			// to "put" them back in the table when we're done with
			// them. 
			
			// If A is not in the table, make an empty list for A,
			// and put it in the table
			// Otherwise, get the list from the table.
			// Then do the same for T, but also get the key of T
			if (cellA == NULL) {
				LA = List_create(sizeof(char**), sizeof(long int*), dataCleaner);
				keyA = table_put(TLG, nameA, &LA);
			}
			else {
				LA = *(List**)table_getDatum(TLG, cellA, nameA);
				keyA = table_getKey(cellA);
			}
			
			if (cellT == NULL) {
				LT = List_create(sizeof(char**), sizeof(long int*), dataCleaner);
				keyT = table_put(TLG, nameT, &LT);
			}
			else {
				LT = *(List**)table_getDatum(TLG, cellT, nameT);
				keyT = table_getKey(cellT);
			}
			
			// The following block of commented-out code was in my original submission.
			// It was designed to test whether the target had already traded with the actor.
			// But it does not test whether the actor had already traded with the target.
			// So the following two lines of input:
			//			{"created_time": "2016-03-28T23:23:12Z", "target": "T", "actor": "A"}
			// 			{"created_time": "2016-03-28T23:23:12Z", "target": "A", "actor": "T"}
			// would not crash the program, but they would produce a wrong median.
			// The new code following the commented block tests for and accomodates this scenario
			
			/*
			checkBlock = List_getBlock(LA, keyT);
			checkTime = 0;
			
			// We are checking if T already exists in A's list
			// If it does, that means A and T have already traded, so we get the timestamp of that trade.
			// Then if the current timestamp is more recent, we update the trade with the current timestamp.
			// Before doing so, we remove the old trade (if possible), because the "List_put" function adds
			// trades to a list chronologically.
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
			*/
			
			checkBlockA = List_getBlock(LA, keyT);
			checkBlockT = List_getBlock(LT, keyA);
			
			// T is not in A, but A is in T, so we update T
			if ( checkBlockA == NULL && checkBlockT != NULL) {
				checkTime = **(unsigned long int**)List_getDatum(LT, checkBlockT);
								
				if (*time > checkTime) {
					List_remove(LT, checkBlockT);
					List_incLenAct(LA, -1);
					
					List_put(LT, &keyA, &time);
					List_incLenAct(LA, 1);
				}
			}
			
			// T is in A, and A is not in T, so we update A
			if ( checkBlockA != NULL && checkBlockT == NULL) {
				checkTime = **(unsigned long int**)List_getDatum(LA, checkBlockA);
								
				if (*time > checkTime) {
					List_remove(LA, checkBlockA);
					List_incLenAct(LT, -1);
					
					List_put(LA, &keyT, &time);
					List_incLenAct(LT, 1);
				}
			}
			
			// T is not in A, and A is not in T, so we put T in A
			if ( checkBlockA == NULL && checkBlockT == NULL) {
						
				List_put(LA, &keyT, &time);
				List_incLenAct(LT, 1);
			} 
			
			// Check the load of the table, and rehash if necessary
			table_checkLoad(TLG);
			
			// Update the table
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
	
	table_destroy(TLG);
	
	printf("\nTotal median computation time:\t%.8f seconds\n\n",medianCompTime);
	
	return 0;
}