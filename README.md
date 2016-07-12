# Insight Data Engineering Coding Challenge
author: Jan Stepinski

created: July 11, 2016

# Compilation, Execution, and Customization

In a Linux environment, simply enter ./run.sh to execute the program. 
If permissions are required, enter "chmod +x run.sh" first.

The program is written entirely in C.
The first line in run.sh compiles it with gcc. I have tested it with versions 5.4.0 and 4.6.3, so all versions in between probably also work. Furthermore, I tested the program in Cygwin on Windows 7 as well as Ubuntu. Cygwin does not include bash, so the compilation and execution instructions must be copied manually from run.sh into the terminal. The "-std=c99" is also not necessary in Cygwin, but I needed it in Ubuntu. 
The other flags in the call to gcc are standard, and no optimization is used.

The second line in run.sh executes the program with two inputs: the input file, and the output file.
Anywhere from 0 to 4, inclusive, inputs are allowed. 0 inputs mean that the defaults are used.

	The first input is the input file. The default is "input.txt".
	
	The second input is the output file. The default is "output.txt".
	
	The third input specifies the algorithm used for computing the median. 
	Only entries 1 (slow) and 2 (fast) are accepted. 
	The algorithms are explained in detail in the "Median Algorithms" section of this readme. 
	The default is 2.
	
	The fourth input allows you to print the graph after a specific line from the input file has been evaluated. 
	For example, 1031 would mean that the graph is printed after 1031 lines of the input file have been evaluated. 
	The "Graph" section of this readme explaines how to interpret the printout. 
	The default is 0, so the graph is not printed.

The source code is distributed among several files:
	list.h
	list.c
	table.h
	table.c
	main.c
	venmoGraphParams.h

The main function is in main.c. The other c files and their headers are explained in the "Graph" section of this readme.
The header "venmoGraphParams.h" can be modified by the user. Unlike the inputs to the compiled program, these parameters provide some control over some of the finer aspects of the program. They are explained within the header itself.

# Graph

The graph is stored as a hash table whose cells contain linked lists.
So "table.h" is the library of table functions, and "list.h" is the library of the list functions.

A cell of the table has the following structure in memory:
| ptr | key | list |
The ptr is the address of the cell.
The key is the char array containing the name of an actor or target from the input file.
The list is actually a pointer to a list struct that was allocated in the heap in main.
A hash table can contain more entries than it has cells. For example, it can have 10 cells but 12 entries. And even then multiple cells might be empty. This is because data is entered into cells via a hash function that creates a code corresponding to a string. Two different strings can produce the same code.
Suppose three keys produce the same code. They would all go into the same cell. The solution is as follows.
The hash code gives an address within the table.
This address is dereferenced to go to the ptr of the third entry. That ptr is dereferenced to go to the ptr of the second entry. That ptr is dereferenced to go to the ptr of the first entry. And if that final ptr is dereferenced, it produces a NULL. So we know when to stop deferencing.
This also means that multiple entries are put on top of the table rather than appended to the bottom.

Ultimately, every entry in the table is a unique string (the key) extracted from the input file. So the table contains the nodes of the graph.

There are two huge advantages of using a hash table to store nodes.

The first is scalability. If each node were stored as an element in an array, there would be a limit to the number of possible nodes. This limit is the largest integer that can be represented in binary. Even with only 32 bits, that number is huge. But it is a limit. A hash table can store multiple entries within a single cell. The only limit is RAM. So if RAM is added, the number of nodes can hypothetically increase forever. Again, that limit is highly unlikely to be attained in the 60 second window of this problem, but other problems requiring graphs might.

Dereferencing and dereferencing pointers becomes slow after a while. So we compute the load of the table as the ratio of entries to the number of cells. If the ratio exceeds a maximum load, usually between 0.75 and 1.5, the table expands. This is called rehashing. When it is rehashed, entries might be moved to new cells. So their ptr's change. But the rehashing is performed in such a way that the addresses of the keys and the lists REMAIN CONSTANT.
That is the second huge advantage of the hash table. Why it is so advantageous will become clear shortly.

The entries - which we will henceforth call cells even though a cell may contain multiple entries - of the table are graph nodes. 
The branches between nodes are the linked lists.
A linked list appears in memory as follows:

	| header |
	|   ptr  | name | timeStamp |
	    ...
	|   ptr  | name | timeStamp |
	|  NULL  |

Each line in the above diagram is a "block" in the source code.
We proceed down the list by dereferencing ptr's, as we did in the hash table.
Unlike the hash table, where new entries are put in the same cell on top of one another, here new blocks are inserted so that they are ordered chronologically, with the most recent at the top. That way we can remove multiple blocks simultaneously if we encounter just one that falls outside our 60 second window as we proceed from the top.
The "names" in the list are all the nodes that connect to the entry in the hash table that stores the list.
That is, if person "A" is the "key" in the table, and his list contains names "B", "C", and "D", then node A has three branches, one to B, one to C, and one to D.

A possible representation of the graph now becomes apparent. Each entry of the table has a list, and the length of the list gives the "degree" of the graph's "node." As an input file is read, new entries can be added to the table and the lists, and old entries can be removed using the timeStamp to update the degrees.

But this is not the representation used in my program.
Because processing it is expensive in both memory and time.
It is expensive in memory because every "name" is stored twice. That is, if A is connected to B, both A and B have entries in the table, and A has B in its list, and B has A in its list. This is redundant. 
Because of that redundancy in memory, this representation is also slow to update. 

My representation avoids these redundancies and further improves speed.

Firstly, I keep track of two different list lengths. There is the actual length and the recorded length. The recorded length is the length of the list actually stored in memory. The actual length is the actual length. 
Consider a simple example, the transaction between A and B. 
Both get their entries in the table.
In the list of A, I put B and the timestamp. So for A, the recorded and actual lengths are both 1.
But then I don't give B a list. Instead I simply increment its actual length. So its recorded lenght is 0, and its actual length is 1.
Memory usage is therefore halved.
Furthermore, I do not store actual strings in the lists. 
In the table, the "key" is the actual char array. 
But in the list, the "name" is a char**, the address of the char array in the table.
Suppose I want to remove the branch between A and B because their timestamp is too old. 
I go into the list of A. I find the char** of B. Then I use that char** to erase B from the table while I'm still in A's list!

That second huge advantage of the hash table should now be clear. That is, the addresses of the cells in the table may change, but the addresses of the keys and lists REMAIN CONSTANT even after rehashing. So I can always access the table entry of B from its char** in the list of A.

The general algorithm for updating the graph is the following:

	parse an input line into the actor A, target T, and timestamp
	if the timestamp is too old, continue to the next line, but first record the median
	find the cell of A (cellA) in the table
	find the cell of T (cellT) in the table
	
	if cellA doesn't exist yet, make an empty list for A (LA) and put it in the table
		otherwise extract LA from the table
	if cellT doesn't exist yet, make an empty list for T (LT) and put it in the table
		otherwise extract LT from the table
	
	check whether A and T already have a branch between them
	if they do, update the timestamp if it is newer
	if they do not, put T in the list of A
		this will increase the actual length of both A and T, but the recorded length only of A
	
	check the load of the table, and rehash if necessary
	
	update the graph by pruning any other branches and nodes of older transactions
		use the fast method described above
		note that updating the graph must be performed AFTER the table is rehashed
	
	compute the new median degree as the median of the actual lengths of the entries of the cells

Finally, let us examine a graph representation.

![ScreenShot](https://cloud.githubusercontent.com/assets/20405323/16753703/e89e6214-47b9-11e6-82b0-0a1e591368d2.PNG)

The above is printed when the program is called with "10" as the 4th input, i.e. it is the graph after 10 lines of the input file have been processed. In this case, the input file is that given in the challenge.
Note that the timestamps are stored as unsigned long integers. The conversion from the time code in the venmo input is performed in the parser.
We can see that there are 6 entries in the table, or 6 nodes in the graph. In one case, the recorded length is 0, but the actual length is 4. The median degree is the median of the actual lengths, in this case of {2,3,3,3,4,5}, which is 3.

# Median Algorithms

The naive method is the slow method. In the final step of the above algorithm, after the table has been rehashed and the graph updated, the median is computed. First we allocate memory for an array whose size is the number of nodes. Then we go through all the nodes and put their actual lengths into the array. Then we sort the array. Finally we extract the median.
This method is slow because of the need to create a new array in the heap each time - its size could always be different. Then we have to sort it.

There is a MUCH faster method.
First, create an array in the heap whose initial size is specified in the venmoGraphParams header. Make it a GLOBAL array.
The indices of this array correspond to frequencies of degrees (actual list lengths).
Index 0 holds the frequency of degree 1.
Index 1 holds the frequency of degree 2.
And so on.
The linked lists are structs with their actual lengths as properties. They cost very little to extract and increment.
In the "list" library, there is an "incLenAct" function. 
The name is short for increment length actual.
Its arguments are the length and the increment. 
When a length change, we decrement its frequency in the global frequency array and increment the frequency of the new length.
	If the new length exceeds the size of the frequency array, the array must be realloced. 
	This operation must be performed very carefully so that we can properly free the array when the program completes. 
	First we use realloc to copy the existing data to a new, larger block of memory. We store the pointer to this memory in temp.
	Then we use calloc to allocate and immediately zero a block of memory of identical size.
	We store that address back in our global frequency array.
	Then we move the old data into the new address from temp and free temp.
	This allows us to free the global array when the program completes.
The reallocation can be costly, but the size is doubled each time, so it will not be performed often.
After every input line, we compute the median degree using the frequencies of all the degrees.

With the naive method, the average total time for computing the medians of the 1792 lines in the sample input file is about 0.06 seconds on my Intel i7 CPU.
With the fast method described above, the average total time is so low that it is displayed as 0 to 12 decimal places. 
That is a huge improvement.

But having the two methods allows me to verify that I am computing the correct median.

# Input Parsing

The input parser is contained in main.c.
Its core is the following two lines:

	int year = -1, month = -1, day = -1, hour = -1, minute = -1, second = -1;
	
	sscanf(str, "{\"created_time\": \"%d-%d-%dT%d:%d:%dZ\", \"target\": \"%[^\"]\", \"actor\": \"%[^\"]", &year, &month, &day, &hour, &minute, &second, target, actor);

The year, month, day, hour, minute, and second are used to create a C time struct. The struct also requires one to specify whether daylight savings time is in effect; fortunately, the "Z" in the time code stands for Zulu time, which is UTC, which has no daylight savings period. The struct can then be passed to the mktime function of the time.h standard library to create an unsigned long int.

If the format string in sscanf is not matched precisely, the parser either returns a timeStamp equal to 0 or an empty actor or target string.
Then in the main algorithm, I use

	if (actor[0]=='\0' || target[0]=='\0' || *time==0)
		continue;

That way we completely skip faulty inputs.
The following are some examples of faulty inputs:

	{"created_time": "2016-03-28T23:23:15Z", "target": "Caroline-Kaiser-2", "actor": ""}
	{"created_time": "2016-03-28T23:23:15Z", "target": "Caroline-Kaiser-2", "actor"}
	{"created_time": "2016-03-28T23:23:15Z", "target": "", "actor": "Amber-Sauer"}
	{"created_time": "2016-03-28T23:23:15Z", "target": "charlotte-macfarlane"}
	{"created_time": "2016-03-28T23:23:15Z", "actor": "Raffi-Antilian"}
	{"creaWRONGted_time": "2016-03-28T23:23:15Z", "target": "charlotte-macfarlane", "actor": "Raffi-Antilian"}
	{"created_time": "201603-28T23:23:15Z", "target": "charlotte-macfarlane", "actor": "Caroline-Kaiser-2"}
	{"created_time": "2016-03-28T2323:15Z", "target": "Raffi-Antilian", "actor": "Amber-Sauer"}
	{"created_time": "2016-03-2823:23:15Z", "target": "Raffi-Antilian", "actor": "Amber-Sauer"}
	{"": "2016-03-28T23:23:15Z", "target": "Raffi-Antilian", "actor": "Amber-Sauer"}
	{"2016-03-28T23:23:15Z", "target": "Raffi-Antilian", "actor": "Amber-Sauer"}
	"created_time": "2016-03-28T23:23:15Z", "target": "Raffi-Antilian", "actor": "Amber-Sauer"}
	
Empty actors or targets, improperly formatted time codes, stray characters, missing quotes or brackts - any of these constitutes a faulty input that is skipped.
