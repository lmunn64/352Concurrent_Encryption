#include <stdio.h>
#include "encrypt-module.h"
#include "circular-queue.h"
#include <pthread.h>
#include <stdlib.h>

int inputSize= -1;
int outputSize= -1;
c_queue *input_queue;
c_queue *output_queue;

char c;

pthread_mutex_t input_mutex;
//can read
pthread_cond_t input_read_cond;

//cannnot read
pthread_cond_t input_nread_cond;
//can consume
pthread_cond_t input_consume_cond;
void reset_requested() {
	log_counts();
}

void reset_finished() {
}

void *reader(void *arg){
	c;
	while ((c = read_input()) != EOF) { 
		pthread_mutex_lock(&input_mutex);
		//cannot add, so must block till encrypt consumes
		while(!can_read(input_queue))
			//block
			pthread_cond_wait(&input_read_cond, &input_mutex);
		//we can now signal that encryptor can consume
		pthread_cond_signal(&input_consume_cond);
		read(input_queue, c);
		pthread_mutex_unlock(&input_mutex);
	} 
}

void *encryptor(void *arg){
	while(1){
		pthread_mutex_lock(&input_mutex);
		//cannot add, so must block till encrypt consumes
		while(!can_consume(input_queue))
			//block
			pthread_cond_wait(&input_consume_cond, &input_mutex);
		if(!can_read())
			pthread_cond_signal(&input_nread_cond);
		consume(input_queue);
		pthread_mutex_unlock(&input_mutex);
	}
}
void *input_counter(void *arg){
	while(1){
		count_input(c);
	}
}
void *output_counter(void *arg){

}
void *writer(void *arg){

}

int main(int argc, char *argv[]) {
	char* in;
	char* out;
	char* log;
	
	if(argc != 4){
		printf("%d arguments not sufficient for required 3 arguments", argc);
		return 0;
	}
	//GET ARGS FOR FILES
	in = argv[1];
	out = argv[2];
	log = argv[3];

	init(in, out ,log); 

	//GET BUFFER SIZES FROM USER
	while(inputSize< 0){
		printf("Enter input buffer size (> 1): ");
		scanf("%d", &inputSize);
	}
	input_queue = new_c_queue(inputSize);
	
	while(outputSize< 0){
		printf("Enter output buffer size (>1): ");
		scanf("%d", &outputSize);
	}
	output_queue = new_c_queue(outputSize);

	int id1=1;
	int id2=2;
	int id3=3;
	int id4=4;
	int id5=5;
	
	pthread_t readr, inp, encrypt, outp, write;
	printf("Parent creating threads\n");
	pthread_create(&readr, NULL, reader, &id1);
	pthread_create(&inp, NULL, input_counter, &id2);
	pthread_create(&encrypt, NULL, encryptor, &id3);
	pthread_create(&outp, NULL, output_counter, &id4);
	pthread_create(&write, NULL, writer, &id5);

	printf("Threads created\n");
	pthread_join(readr, NULL);
	pthread_join(inp, NULL);
	pthread_join(encrypt, NULL);
	pthread_join(outp, NULL);
	pthread_join(write, NULL);
	printf("Threads are done\n");

	// while ((c = read_input()) != EOF) { 
	// 	count_input(c); 
	// 	c = encrypt(c); 
	// 	count_output(c); 
	// 	write_output(c); 
	// } 
	printf("End of file reached.\n"); 
	log_counts();
}

