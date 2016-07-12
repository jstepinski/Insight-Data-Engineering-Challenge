#ifndef _venmoGraphParams_h
#define _venmoGraphParams_h

// Parameters of the project

// MAX_AGE is the maximum window in seconds.
// It is currently set to the 60 specified in the challenge.
#define MAX_AGE 60

// MAX_STR_LEN is the maximum length of an actor's or target's name.
#define MAX_STR_LEN 200

// The graph is stored in a table.
// The cells of the table contain the nodes of the graph.
// Multiple nodes can be stored in each cell.
// The table is expanded when the probability increases of a cell
// containing too many nodes.
// The initial number of cells in the table is given in INITIAL_TABLE_SIZE
#define INITIAL_TABLE_SIZE 4

// For the fastMedian algorithm, an array is created to store the frequency
// of each degree (called length in the code). 
// INIT_MAX_LEN is the initial guess of the maximum degree.
// Don't worry about setting it too low; like the above table, this array
// will grow dynamically and very quickly (every expansion is a doubling).
#define INIT_MAX_LEN 10

#endif
