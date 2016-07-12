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
	if the timestamp is too old, continue to the next line
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

Finally, let us examine 




# Median Algorithms

# Input Parsing

