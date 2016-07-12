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
	
	The third input specifies the algorithm used for computing the median. Only entries 1 (slow) and 2 (fast) are accepted. The algorithms are explained in detail in the "Median Algorithms" section of this readme. The default is 2.
	
	The fourth input allows you to print the graph after a specific line from the input file has been evaluated. For example, 1031 would mean that the graph is printed after 1031 lines of the input file have been evaluated. The "Graph" section of this readme explaines how to interpret the printout. The default is 0, so the graph is not printed.

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



# Median Algorithms

# Input Parsing

