#include "stdio.h"
#include "string.h"
#include "time.h"
#include "stdlib.h"
#include "signal.h"
#include "unistd.h" 

///// SYSTEM ACTIONS HANDLERS ///// 

// Flag to indicate program was killed and memory should be free-d
volatile sig_atomic_t cleanup_flag = 0;

// Signal handler for SIGINT
void handle_sigint(int sig) {
    // catch the exception and set the cleanup flag
    cleanup_flag = 1; 
}

void set_seed(int option) {
    switch (option) {
        case 1:
            // Use current time as seed
            srand(time(NULL)); 
            break;
        case 2:
            // Fixed seed for reproducibility
            srand(1234); 
            break;
        case 3:
            // Another fixed seed for a different sequence
            srand(5678); 
            break;
        default:
            printf("Invalid option, using default seed.\n");
            srand(time(NULL)); 
            break;
    }
}

///// DEFINITION OF EXECUTIONSTATS /////

typedef struct {
    // number of epochs
    int epochs;                     
    // maximum request size
    int max_request;                
    // size of the heap
    int heap_size;                  
    // probability of being free
    float free_prob;            

    // total sum of allocations
    int sum_allocations;            
    // number of allocations
    int num_allocations;            
    // total sum of frees
    int sum_frees;                  
    // number of frees
    int num_frees;                  
    // free tail starts at this position
    int free_tail_starts_at;        
    // number of free spaces in the active area
    int free_spaces_in_active_area; 
    // percentage of free space in the active area
    float percent_free_in_active_area;       
                  
} ExecutionStats;

ExecutionStats initialize_memory_stats(ExecutionStats *stats, int epochs, int max_request, int heap_size, float free_prob ) {
    stats = (ExecutionStats *)malloc(sizeof(ExecutionStats));

    // set the user provided configuration
    stats->epochs = epochs;
    stats->max_request = max_request;
    stats->free_prob = free_prob;
    stats->heap_size = heap_size;

    // initialize status parameters
    stats->sum_allocations = 0;
    stats->num_allocations = 0;
    stats->sum_frees = 0;
    stats->num_frees = 0;
    stats->free_tail_starts_at = -1;
    stats->free_spaces_in_active_area = -1;
    stats->percent_free_in_active_area = 0.0;

    return *stats;
}

ExecutionStats stats;

void parse_arguments(int argc, char *argv[]) {
    if (argc != 11) { // Changed from 9 to 11 to accommodate the seed option
        printf("Usage: %s -e <epochs : int> -m <max_request: positive int> -h <heap_size: positive int> -p <free_prob: float> -s <seed: positve int>\n", argv[0]);
        printf("\n* To have an infinite exeuciton set <epochs> to -1\n");
        printf("* To have a same output at each execution set <seed> to 2. Alternative static output can be obatined at <seed> = 3.\n");
        printf("* Completely random output per execution is obtained at <seed> equal to 1.\n");

        exit(EXIT_FAILURE);
    }

    int epochs = 0;
    int max_request = 0;
    int heap_size = 0;
    float free_prob = 0.0f;
    int seed = 0; // Variable for seed

    for (int i = 1; i < argc; i += 2) {
        if (strcmp(argv[i], "-e") == 0) {
            epochs = atoi(argv[i + 1]); 
        } else if (strcmp(argv[i], "-m") == 0) {
            max_request = atoi(argv[i + 1]); 
        } else if (strcmp(argv[i], "-h") == 0) {
            heap_size = atoi(argv[i + 1]); 
        } else if (strcmp(argv[i], "-p") == 0) {
            free_prob = atof(argv[i + 1]); 
            printf("%f - free prob\n", free_prob);
        } else if (strcmp(argv[i], "-s") == 0) {
            // Get seed value
            seed = atoi(argv[i + 1]); 
        } else {
            printf("Unknown flag: %s\n", argv[i]);
            exit(EXIT_FAILURE);
        }
    }

    printf("Parsed the input arguments\n");

    // Initialize memory stats with the parsed values
    stats = initialize_memory_stats(&stats, epochs, max_request, heap_size, free_prob);
    
    // Set the seed for the random number generator
    set_seed(seed);

    // Validate inputs
    if (stats.max_request < 0 || stats.heap_size <= 0 || stats.free_prob < 0.0f || stats.free_prob > 1.0f) {
        printf("Invalid arguments. Ensure epochs, max_request are non-negative, heap_size is positive, and free_prob is between 0.0 and 1.0.\n");
        exit(EXIT_FAILURE);
    }
}

///// DEFINITION OF FREELIST STRUCT ///// 

typedef struct {
    // string that contains manipulated string
    char *list;
    // array that keeps the list of the stored elements
    int  *stored_elements; 

    // index of the array used to store selected elements
    int stored_elements_index;
    // index of the arrays usedd to store the state of the manipulated string
    int  freelist_index;

    // array that stores the state of available data chunks for allocation
    int  *freelist_size;
    // array that stores the starting addresses of available data chunks for allocation
    int  *freelist_at;
} FreeList;

// function to initialize the FreeList data structure
FreeList *initialize_struct() {
    FreeList *freelist = (FreeList *) malloc(sizeof(FreeList));

    if (!freelist) {
        printf("Failed to allocate a FreeList\n");
        return NULL;
    }

    freelist->freelist_index = 0;

    freelist->freelist_size = (int *) malloc (stats.heap_size * sizeof(int));
    if (!freelist->freelist_size) {
        printf("Failed to create a freelist_size array\n");
        return NULL;
    }
    freelist->freelist_size[freelist->freelist_index] = stats.heap_size;

    freelist->freelist_at = (int *) malloc (stats.heap_size * sizeof(int));
    if (!freelist->freelist_at) {
        printf("Failed to create a freelist_at array\n");
        return NULL;
    }
    freelist->freelist_at[freelist->freelist_index] = 0;
    
    // update the index
    freelist->freelist_index++;
    
    freelist->list = (char *) calloc (5001, sizeof(char));
    if (!freelist->list) {
        printf("Failed to allocate a manipulated string\n");
        return NULL;
    }
    memset(freelist->list, '_', stats.heap_size);

    freelist->stored_elements = (int *) malloc (stats.heap_size * sizeof(int));
    if (!freelist->stored_elements) {
        printf("Failed to create a stored_elements array\n");
        return NULL;
    }
    freelist->stored_elements_index = 0;

    return freelist;
}

// Shift elements to the left starting from the index.
// Effectively this strategy deletes the element at that index.
void remove_index_from_array(FreeList *freelist, int index) {

    int size = freelist->freelist_index;
    if (index < 0 || index >= size) {
        printf("Index out of bounds\n");
        return;
    }

    for (int i = index; i < size - 1; i++) {
        freelist->freelist_at[i] = freelist->freelist_at[i + 1];
        freelist->freelist_size[i] = freelist->freelist_size[i + 1];
    }

    freelist->freelist_index--;
}

// function to free the memory of instance of FreeList
void free_freelist_struct(FreeList *freelist) {
   if (freelist) {
        free(freelist->list);
        free(freelist->freelist_size);
        free(freelist->freelist_at);
        free(freelist);
   } 
}

///// RANDOM VALUE GENERATORS /////

// a function to generate the action
//      true - allocate -> ' ' should be printed
//      false - free -> 'F' should be printed
char generate_action() {
    float action = (float)rand() / RAND_MAX;

    if (action < stats.free_prob) {
        return 'F'; 
    } else {
        return ' '; 
    }
}

// generate a string in range [1, length]
int generate_string_length(int length) {
    int string_length = (rand() % length) + 1; 
    return string_length;
}

// pick an element from stored elements array in FreeList structure
int pick_value_to_delete(FreeList *freelist) {
    int value_to_delete;
    int index;
    int range = freelist->stored_elements_index - 1;

    if (range == 0) {
        index = 0;
    } else {
        index = (rand() % (range));
    }

    value_to_delete = freelist->stored_elements[index];
    
    // remove the deleted element from the list of stored elements if there is anything to remove
    if (freelist->stored_elements_index >= 0) {
        for (int i = index; i < range; i++) {
            freelist->stored_elements[i] = freelist->stored_elements[i + 1];
        }

        if (freelist->stored_elements_index == 0) {
            freelist->stored_elements_index = 0;
        } else {
            freelist->stored_elements_index--;
        }
    }

    return value_to_delete;
}

///// MEMORY MANAGEMENT FUNCTIONS /////

// get index of first fit element
int get_first_fit_chunk(FreeList *freelist, int string_size) {
    int index = -1;

    for (int i = 0; i < stats.heap_size; i++) {
        if (freelist->freelist_size[i] >= string_size) {
            index = i;
            break;
        }
    } 

    return index;
}

// update the list of indices via appending
void update_struct_append(FreeList *freelist, int index, int string_length) {

    int selected_start_address = freelist->freelist_at[index];
    int selected_space_size = freelist->freelist_size[index];

    freelist->freelist_at[freelist->freelist_index] = selected_start_address + string_length;
    freelist->freelist_size[freelist->freelist_index] = selected_space_size - string_length;

    freelist->freelist_index++;

    remove_index_from_array(freelist, index);
}

// update the list of indices via prepending
void update_list_prepend(FreeList *freelist, int index, int string_length) {
    freelist->freelist_index++;

    for (int i = freelist->freelist_index - 1; i >= 0; i--) {
        freelist->freelist_at[i + 1] = freelist->freelist_at[i];
        freelist->freelist_size[i + 1] = freelist->freelist_size[i];
    }

    freelist->freelist_at[0] = index;
    freelist->freelist_size[0] = string_length;
}

// append generated string to the list
int append_to_freelist(FreeList *freelist, char *string) {

    // store value of the picked string
    freelist->stored_elements[freelist->stored_elements_index] = string[0] - '0';
    freelist->stored_elements_index++;

    if (!freelist || !string) {
        return -1;
    }

    int string_length = strlen(string);

    int index = get_first_fit_chunk(freelist, string_length);
    if (index == -1)
    {
        printf("Error unable to allocate enough memory!\n\n");
        return 1;
    }
    
    int list_index = freelist->freelist_at[index];
    
    memcpy(freelist->list + (list_index), string, string_length);

    update_struct_append(freelist, index, string_length);
    return 0; 
}

// generate a string
char * populate_string(int timestep) {
    // generate 10 char long string for ease of demonstration. It can be changed. 
    int string_length = generate_string_length(stats.max_request);

    char *allocated_str = (char *) calloc (string_length, sizeof(char));
    if (allocated_str == NULL) {
        printf("Failed to allocate for generated string");
        return NULL;
    }

    // '0' + integer -> provides an ascii code for that integer
    memset(allocated_str, '0' + (timestep % 10), string_length);

    printf("Populated string size: %d\n", string_length);
    return allocated_str;
}

// a function to delete the substring that contains the element.
// return freed space if success and 0 if failure. 
int delete_element(FreeList *freelist, int element) {
    // starting index of the element to delete
    int element_index = -1;
    // length of the substring that contains the element
    int element_space = 0;

    for (int i = 0; i < stats.heap_size; i++) {
        if (freelist->list[i] == ('0' + element)) {
            if (element_index == -1) {
                element_index = i;
            } 
            // delete the element
            freelist->list[i] = '_';
            element_space++;
        } else if (element_index != -1 && freelist->list[i] != ('0' + element)) {
            break;
        }
        
    }

    if (element_index == -1) {
        return 0;
    }
    
    update_list_prepend(freelist, element_index, element_space);
    return element_space;
}


///// PRINT FUNCTIONS /////

// print freelist_at array of the FreeList structure
void print_freelist_at(FreeList *freelist) {
    printf("freeatlist_at=");

    for (int i = 0; i < freelist->freelist_index; i++) {
        if (i == freelist->freelist_index - 1) {
            printf("%d", freelist->freelist_at[i]);
        } else {
            printf("%d;", freelist->freelist_at[i]);
        }
    }

    printf("\n");
}

// print freelist_size array of the FreeList structure
void print_freelist_size(FreeList *freelist) {
    printf("freelist_size=");

    for (int i = 0; i < freelist->freelist_index; i++) {
        if (i == freelist->freelist_index - 1) {
            printf("%d", freelist->freelist_size[i]);
        } else {
            printf("%d;", freelist->freelist_size[i]);
        }
    }

    printf("\n");
}

// print list of stored elements in the freelist
void print_freelist_stored_elements(FreeList *freelist) {
    printf("freelist_stored_elements=");

    for (int i = 0; i < freelist->stored_elements_index; i++) {
        if (i == freelist->stored_elements_index - 1) {
            printf("%d", freelist->stored_elements[i]);
        } else {
            printf("%d;", freelist->stored_elements[i]);
        }
    }

    printf("\n");
}

// print state for the UI
void print_state(FreeList *freelist, char action, int timestep) {

    printf("\n");
    printf("%ct=%d\n", action, timestep);
    printf("*");

    for (int i = 0; i < stats.heap_size; i++) {
        printf("%c",freelist->list[i]);
    }
    printf("\n\n");

    print_freelist_at(freelist);
    print_freelist_size(freelist);
    print_freelist_stored_elements(freelist);
    printf("\n\n");

}

// function to print execution statistics
void print_execution_stats(ExecutionStats stats) {
    printf("Execution statistics:\n");
    printf("  Total Sum of Allocations: %d\n", stats.sum_allocations);
    printf("  Number of Allocations: %d\n", stats.num_allocations);
    printf("  Total Sum of Frees: %d\n", stats.sum_frees);
    printf("  Number of Frees: %d\n", stats.num_frees);
    printf("  Free Tail Starts At: %d\n", stats.free_tail_starts_at);
    printf("  Free Spaces in Active Area: %d\n", stats.free_spaces_in_active_area);
    printf("  Percent Free in Active Area: %.5f%%\n", stats.percent_free_in_active_area);
}

// function to print user configuration
void print_user_configuration(ExecutionStats stats) {
    printf("User configuration:\n");
    printf("  Number of Epochs: %d\n", stats.epochs);
    printf("  Maximum Request Size: %d\n", stats.max_request);
    printf("  Heap Size: %d\n", stats.heap_size);
    printf("  Probability of Being Free: %.5f\n", stats.free_prob);
}

///// STAT FUNCTIONS /////

// to obtain the index of the tail free space the element with max value in Freelist->freelist_at should be found.
int get_free_tail_index(FreeList *freelist) {

    int max_value = freelist->freelist_at[0]; 

    for (int i = 1; i < freelist->freelist_index; i++) {
        if (freelist->freelist_at[i] > max_value) {
            max_value = freelist->freelist_at[i]; 
        }
    }

    return max_value;
}

// a function to find number of memory slots available in the active area
int get_memory_slots_in_active_area(FreeList *freelist) {
    int sum = 0; 
    
    int exclude_value = stats.heap_size - stats.free_tail_starts_at;

    for (int i = 0; i < freelist->freelist_index; i++) {
        if (freelist->freelist_size[i] != exclude_value) {
            sum += freelist->freelist_size[i]; 
        }
    }

    return sum; 
}

int main(int argc, char *argv[]) 
{   
    // get the CLI arguments from user.
    parse_arguments( argc, argv);

    // Register the signal handler
    signal(SIGINT, handle_sigint);

    // start with the allocation as first action
    char action = ' ';
    int timestep = 1;
    FreeList *freelist = initialize_struct();

    while (1) {
        
        // If program was killed exit the while loop
        if (cleanup_flag) {
            printf("Program is killed!\n");
            break;           
        }
        
        // finish if all epochs were executed
        if (stats.epochs != -1 && timestep > stats.epochs){
            break;
        }
        
        if (action == ' ') {
            char *allocated_str = populate_string(timestep);

            // update the profiling information
            stats.num_allocations++;
            stats.sum_allocations += strlen(allocated_str);

            if(append_to_freelist(freelist, allocated_str)) {
                break;
            }
        } else if (action == 'F') {
            int element_to_delete = pick_value_to_delete(freelist);
            int freed_space = delete_element(freelist, element_to_delete);
            
            if(freed_space == 0) {
                printf("Missing an element to delete\n");
            } 
           
           // update the profiling information
            stats.num_frees++;
            stats.sum_frees += freed_space;

            // print the line in gray color
            printf("Deleting: %d\n", element_to_delete);
        }

        print_state(freelist, action, timestep);

        // increment timestamp and get next action
        timestep += 1;
        action = generate_action();
    }

    // compute the post-execution statistics
    stats.free_tail_starts_at = get_free_tail_index(freelist);
    stats.free_spaces_in_active_area = get_memory_slots_in_active_area(freelist);
    stats.percent_free_in_active_area = (float) stats.free_spaces_in_active_area / stats.free_tail_starts_at;

    print_execution_stats(stats);

    printf("\n\nCleaning up FreeList.\n");
    free_freelist_struct(freelist);
    return 0;
}