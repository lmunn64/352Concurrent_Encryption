#include "circular-queue.h"

c_queue* new_c_queue(int sz){
    c_queue *new = malloc(sizeof(c_queue));
    new->head = 0;
    new->tail = 0;
    new->counter = 0;
    new->size = sz;
    new->queue = malloc(sizeof(char)*sz);
    return new;
}

// if head past tail, then can consume
bool can_consume(c_queue* q){
    if(q->head > q->tail)
        return true;
    return false;
}

// if head past tail, then can consume
bool can_count(c_queue* q){
    if(q->head > q->counter)
        return true;
    return false;
}

// if head past tail, then can consume
bool can_read(c_queue* q){
    if(q->head < q->size){
        if(q->head >= q->tail)
            return true;
    }
    else if(q->head % q->size < q->tail){
        return true;
    }
    return false;
}
// consume a value if not empty and move tail
char consume(c_queue* q){
    char consumed_char =  q->queue[q->tail % q->size];
    q->tail++;
    return consumed_char;
}

// consume a value if not empty and move tail
char count(c_queue* q){
    char consumed_char =  q->queue[q->counter % q->size];
    q->counter++;
    return consumed_char;
}

//add a value and move head
void read(c_queue* q, char newChar){
    // if head has either not filled all parts of queue, or 
    // head is behind tail, then overwrite character and move head
    if(can_read(q)){
        q->queue[q->head % q->size] = newChar;
        q->head++;
    }
    // nothing added

}
char* To_String(c_queue* q){
    printf("[");
    for(int i =0; i < q->size; i++){
        printf("%c, ", q->queue[i]);
    }
    printf("]\n");
}