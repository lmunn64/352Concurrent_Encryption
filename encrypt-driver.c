#include <stdio.h>
#include "encrypt-module.h"
#include "circular-queue.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>

int inputSize= -1;
int outputSize= -1;
c_queue *input_queue;
c_queue *output_queue;

bool reset_request = false;

pthread_mutex_t input_mutex;
pthread_mutex_t output_mutex;
pthread_mutex_t reset_mutex;
//can read
pthread_cond_t input_read_cond;

//can consume
pthread_cond_t input_consume_cond;

//random reset is finished
pthread_cond_t reset_complete_cond;

//random reset is finished
pthread_cond_t io_equal_cond;

//can count
pthread_cond_t input_count_cond;

//can read
pthread_cond_t output_read_cond;

//can consume
pthread_cond_t output_consume_cond;

//can count
pthread_cond_t output_count_cond;

void print_counts(){
	printf("Total input count with current key is %d\n", get_input_total_count());
	for(char i = 'A'; i <='Z'; i++){
		printf("%c:%d ", i, get_input_count(i));
	}
	printf("\n");
	printf("Total output count with current key is %d\n", get_output_total_count());
	for(char i = 'A'; i <='Z'; i++){
		printf("%c:%d ", i, get_output_count(i));
	}
	printf("\n");
}

void reset_requested() {
	// pthread_mutex_lock(&input_mutex);
	// reset_request = true;
	// while(get_input_total_count() != get_output_total_count()){
	// 	printf("reset help\n");
	// 	pthread_cond_wait(&io_equal_cond, &input_mutex);
	// }
	// pthread_mutex_unlock(&input_mutex);
	// print_counts();
}

void reset_finished() {
	// pthread_mutex_lock(&input_mutex);
	// reset_request = false;
	// pthread_cond_signal(&reset_complete_cond);
	// pthread_mutex_unlock(&input_mutex);
	// printf("Reset finished.\n");
}


void *writer(void *arg){
	while (1) { 
		//cannot consume, so must block till encryptor reads to output buffer
		pthread_mutex_lock(&output_mutex);
		while(!can_consume(output_queue)){	
			// printf("Writer can consume: %s\n", can_consume(output_queue) ? "true" : "false");
			// printf("Head: %d, Tail: %d\n", output_queue->head, output_queue->tail);
			printf("writer help\n");
			pthread_cond_wait(&output_consume_cond, &output_mutex);
		}
		printf("writer help OUT\n");
		char output = consume(output_queue);
		//output can read again after consumption
		pthread_cond_signal(&output_read_cond);
		pthread_mutex_unlock(&output_mutex);
		if(output == EOF){	
			break;
		}
		write_output(output);
	}
	return NULL;
}

void *reader(void *arg){ 
	char c;
	while ((c = read_input()) != EOF) { 	
		pthread_mutex_lock(&input_mutex);

		while(!can_read(input_queue)){
			printf("reader help\n");	
			pthread_cond_wait(&input_read_cond, &input_mutex);
		}
		// printf("\nReader: locked\n");
		while(reset_request){
			// printf("wait for reset\n");
			printf("reader reset help\n");	
			pthread_cond_wait(&reset_complete_cond, &input_mutex);
		}
		printf("READER help OUT\n");
		read(input_queue, c);
		// printf("Reader: Character read from file and placed in queue: %c\n", c);
		pthread_cond_signal(&input_consume_cond);
		pthread_cond_signal(&input_count_cond);

		pthread_mutex_unlock(&input_mutex);
		// printf("Reader: unlocked\n");	
	} 
	pthread_mutex_lock(&input_mutex);
	// printf("\nReader: locked\n");
	read(input_queue, EOF);
	pthread_cond_signal(&input_consume_cond);
	pthread_cond_signal(&input_count_cond);
	pthread_mutex_unlock(&input_mutex);
	// printf("\nReader: EOF Reached.\n");
	return NULL;
}

void *encryptor(void *arg){
	char to_encrypt;
	while(1){
		pthread_mutex_lock(&input_mutex);
		printf("ENCRY help OUT\n");
		//cannot consume, so must block until reader reads
		while(!can_consume(input_queue)){
			printf("encrypt help in\n");
			pthread_cond_wait(&input_consume_cond, &input_mutex);
		}
		//consume next char in input buffer
		to_encrypt = consume(input_queue);
		write_output(to_encrypt);
		
		//signal to input buffer can read again
		pthread_cond_signal(&input_read_cond);

		pthread_mutex_unlock(&input_mutex);
		if(to_encrypt == EOF){
			break;
		}
		
		// //Encryptor reaches EOF in buffer 
		// if(to_encrypt == EOF){
		// 	// printf("Encryptor: EOF");
		// 	pthread_mutex_lock(&output_mutex);
		// 	read(output_queue, to_encrypt);
		// 	pthread_cond_signal(&output_consume_cond);
		// 	pthread_mutex_unlock(&output_mutex);
		// 	break;
		// }
		// //encrypt
		// char encrypted = encrypt(to_encrypt);
		// // printf("Encryptor: Encrypted '%c' ---> '%c'\n", to_encrypt, encrypted);

		// //OUTPUT -- now place into output queue	
		// pthread_mutex_lock(&output_mutex);
		// // printf("\nEncryptor: output locked\n");

		// //output cannot read, must wait for writer to consume
		// while(!can_read(output_queue)){
		// 	printf("encrypt  help out\n");
		// 	pthread_cond_wait(&output_read_cond, &output_mutex);
		// }
		// read(output_queue, encrypted);
		
		// // printf("Encryptor: output read\n");
		// //output can now consume and count
		// pthread_cond_signal(&output_consume_cond);
		// pthread_cond_signal(&output_count_cond);
		// pthread_mutex_unlock(&output_mutex);
		// // printf("Encryptor: output unlocked\n");
		
	}
	
	return NULL;
}

void *input_counter(void *arg){
	char counted;
	while(1){
		pthread_mutex_lock(&input_mutex);
		while(!can_count(input_queue)){
			pthread_cond_wait(&input_count_cond, &input_mutex);
			printf("count in help\n");
		}
		// printf("\nInput Counter: locked\n");
		printf("INPUYT help OUT\n");
		counted = count(input_queue);
		if(counted == EOF){
			pthread_mutex_unlock(&input_mutex);
			break;
		}
		pthread_mutex_unlock(&input_mutex);
		// printf("Input Counter: unlocked\n");
		count_input(counted);
		// printf("Input Counter: counted character '%c'\n", counted);
		
	}
	// printf("Input Counter: EOF reached\n\n");
	return NULL;		

}

void *output_counter(void *arg){
char counted;
	while(1){
		pthread_mutex_lock(&output_mutex);
		while(!can_count(output_queue)){
			printf("count out help\n");
			pthread_cond_wait(&output_count_cond, &output_mutex);
		}
		counted = count(output_queue); 
		if(counted == EOF){
			pthread_mutex_unlock(&output_mutex);
			break;
		}
		pthread_mutex_unlock(&output_mutex);
		printf("OUTP help OUT\n");
		count_output(counted);
		
		if(get_input_total_count() == get_output_total_count()){
			pthread_mutex_lock(&input_mutex);
			printf("Sent equal IO REQUEST/N");
			pthread_cond_signal(&io_equal_cond);
			pthread_mutex_unlock(&input_mutex);
		}
		// printf("Output Counter: counted character '%c'\n", counted);
		
	}
	printf("Output Counter: EOF reached\n\n");
	return NULL;				

}


int main(int argc, char *argv[]) {
	char* in;
	char* out;
	char* log;
	
	if(argc != 3){
		printf("%d arguments not sufficient for required 3 arguments", argc);
		return 0;
	}
	//GET ARGS FOR FILES
	in = argv[1];
	out = argv[2];
	log = argv[3];

	//GET BUFFER SIZES FROM USER (fix char)
	while(inputSize < 0 ){
		printf("Enter input buffer size (> 1): ");
		scanf("%d", &inputSize);
	}
	input_queue = new_c_queue(inputSize);
	
	while(outputSize < 0){
		printf("Enter output buffer size (> 1): ");
		scanf("%d", &outputSize);
	}
	output_queue = new_c_queue(outputSize);
	
	//INIT PTHREADS
	int id1=1;
	int id2=2;
	int id3=3;
	int id4=4;
	int id5=5;

	init(in, out); 

	pthread_t readr, inp, encrypt, outp, write;
	//INIT PTHREAD MUTEX
	pthread_mutex_init(&input_mutex, NULL);
	pthread_mutex_init(&output_mutex, NULL);
	pthread_mutex_init(&reset_mutex, NULL);

	//INIT PTHREAD CONDITION VARS.
	pthread_cond_init(&input_read_cond, NULL);
	pthread_cond_init(&input_consume_cond, NULL);
	pthread_cond_init(&input_count_cond, NULL);
	pthread_cond_init(&output_read_cond, NULL);
	pthread_cond_init(&output_consume_cond, NULL);
	pthread_cond_init(&output_count_cond, NULL);
	pthread_cond_init(&reset_complete_cond, NULL);
	pthread_cond_init(&io_equal_cond, NULL);

	printf("Parent creating threads\n");

	//CREATE THREADS
	pthread_create(&readr, NULL, reader, &id1);
	pthread_create(&inp, NULL, input_counter, &id2);
	pthread_create(&encrypt, NULL, encryptor, &id3);
	// pthread_create(&write, NULL, writer, &id5);
	// pthread_create(&outp, NULL, output_counter, &id4);

	//WAIT FOR THREADS TO COMPLETE
	pthread_join(readr, NULL);
	printf("reader done \n");
	pthread_join(inp, NULL);
	printf("inputcounter done \n");
	pthread_join(encrypt, NULL);
	printf("encryptor done \n");
	// pthread_join(write, NULL);
	// printf("writer done \n");
	// pthread_join(outp, NULL);
	// printf("outputcount done \n");

	printf("Threads are done\n");

	// while ((c = read_input()) != EOF) { 
	// 	count_input(c); 
	// 	c = encrypt(c); 
	// 	count_output(c); 
	// 	write_output(c); 
	// } 
	printf("End of file reached.\n"); 
	print_counts();
}

