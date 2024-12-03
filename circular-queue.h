#define CIRCULAR_QUEUE
#include <stdbool.h>
#include <stdlib.h>
typedef struct circular_queue{
    int head; //read index
    int tail; //consume index (% size for array location)
    int size;
    char *queue;
} c_queue;

//create new c_queue
c_queue* new_c_queue(int sz);

//if head past tail, then can consume
bool can_consume(c_queue* q);

//if head past tail, then can consume
bool can_read(c_queue* q);

//consume a value and move tail
char consume(c_queue* q);

//add a value (if need to be overwritten, tail must be past head) 
//and move head
void read(c_queue* q, char newChar);