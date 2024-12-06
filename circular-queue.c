#include "circular-queue.h"

c_queue* new_c_queue(int sz){
    c_queue *new = malloc(sizeof(c_queue));
    new->head = -1;
    new->tail = -1;
    new->counted = true;
    new->size = sz;
    new->queue = malloc(sizeof(char)*sz);
    return new;
}

// if head past tail, then can consume
bool can_consume(c_queue* q){
    if(q->head == q->tail)
        return false;
    int next = q->tail + 1;
    if(next >= q->size){
        next = 0;
    }
    return true;
}

// if head past tail, then can consume
bool has_been_counted(c_queue* q){
    return q->counted;
}

// if head past tail, then can consume
bool can_read(c_queue* q){
    int next = q->head + 1;
    if(next >= q->size)
        next = 0;
    if(next == q->tail){
        return false;
    }
    return true;
}
// consume a value if not empty and move tail
char consume(c_queue* q){
    q->tail++;
    if(q->tail >= q->size){
        q->tail = 0;
    }
    char consumed_char =  q->queue[q->tail];  
    q->counted = false;  
    return consumed_char;
}

// consume a value if not empty and move tail
char count(c_queue* q){
    q->counted = true;
    return q->queue[q->tail];
}

//add a value and move head
void read(c_queue* q, char newChar){
    // if head has either not filled all parts of queue, or 
    // head is behind tail, then overwrite character and move head
    q->head++;
    if(q->head >= q->size){
        q->head = 0;
    }
    q->queue[q->head] = newChar;
}
char* To_String(c_queue* q){
    printf("[");
    for(int i =0; i < q->size; i++){
        printf("%c, ", q->queue[i]);
    }
    printf("]\n");
}