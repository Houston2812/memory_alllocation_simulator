# Memory Allocation simulator

A software to simulate the memory allocation/deallocation operations.  
The program performs allocation and deallocation operations randomly.

## Features
Program has following features:
* User can set the maximum number of executions (epochs). 
* User can configure the maximum size of the heap.
* User can configure the probability of free actions, what ultimately defines the probability of allocate actions too.
* User can set the maximum length of the string that can be allocated at a single allocation attempt.
* User can set randomization option.  
* Program performs memory allocations using First-Fit action.
* Program catches SIGINT (CTRL+C) and gracefully shuts down. 
* Program outputs the execution statistics when the execution finishes.

## CLI Interface for input
It accepts following CLI arguments:

* -e <epochs> - an integer that specifies the maximum number of epochs. If set to -1, program will execute until the allocated heap is filled, so that new data cannot be allocated.
* -m <max request> - a positive integer that specifies max length of generated string.
* -h <heap size> - a positive integer that represents maximum size of the heap that could be used in the simulation.
* -p <free prob> - a float that represents probability of firing a free action.
* -s <seed> - options used to define the seed for the current execution:
    * 1 - random,
    * 2 - static,
    * 3 - static but different from 2 and 3. These two options are provided to be able to observe
2 different repeatable output scenarios.

## Output format
Program prints the state of the memory for each epoch. State is composed of following values:
* Operation: ``' '`` for allocation and ```'F'``` for free.
* Timestep:```t=<$val$>```
* String representing the memory: ```'_'``` represents empty memory slot.
* List containing starting addresses of the empty memory blocks: ```freelist_at=val1;val2;val3;```
* List containing sizes  of the memory blocks: ```freelist_size=val1;val2;val3;```
* List containing list of elements that were allocated in the memory. This list is used for proper selection during the random memory free operations: ```freelist_at=val1;val2;val3;```. 

It also prints post execution statistics, that include:
* Total Sum of Allocations - total allocated space.
* Number of Allocations - total number of allocation operations.
* Total Sum of Frees - total free-d space.
* Number of Frees - total number of free operations.
* Free Tail Starts At - the starting address of the tailing free memory block. 
* Free Spaces in Active Area - available free space in the memory blocks before the tailing section. 
* Percent Free in Active Area - ratio of the free space in the active area to the size of the active area. 

## Example execution commands
* ```gcc malloc_simulator.c; ./a.out -e 10 -m 10 -h 100 -p 0.5 -s 1```
* ```gcc malloc_simulator.c; ./a.out -e 1000 -m 10 -h 5000 -p 0.3 -s 1```
* ```gcc malloc_simulator.c; ./a.out -e -1 -m 100 -h 5000 -p 0.6 -s 2```
* ```gcc malloc_simulator.c; ./a.out -e 1000 -m 10 -h 5000 -p 0.2 -s 3```

