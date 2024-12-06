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
pthread_mutex_t reader_pause_mutex;
//can read
pthread_cond_t input_read_cond;

//can consume
pthread_cond_t input_consume_cond;

//random reset is finished
pthread_cond_t reader_pause_cond;

//random reset is finished
pthread_cond_t io_equal_cond;

//can count
pthread_cond_t input_count_cond;

//can count
pthread_cond_t input_counted_cond;

//can count
pthread_cond_t output_counted_cond;

//can read
pthread_cond_t output_read_cond;

//can consume
pthread_cond_t output_consume_cond;

//can count
pthread_cond_t output_count_cond;

bool input_last_count;
bool output_last_count;

//last character read
char checker;

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
	pthread_mutex_lock(&reader_pause_mutex);
	reset_request = true;
	while(output_queue->queue[output_queue->tail] != checker && input_queue->queue[input_queue->tail] != checker ){
		printf("reset help\n");
		pthread_cond_wait(&io_equal_cond, &input_mutex);
	}
	print_counts();
	pthread_mutex_unlock(&reader_pause_mutex);
	
}

void reset_finished() {
	// pthread_mutex_lock(&input_mutex);
	// reset_request = false;
	// pthread_mutex_unlock(&input_mutex);
	// pthread_cond_signal(&reset_complete_cond);
	// printf("Reset finished.\n");
}


void *writer(void *arg){
	while (1) { 
		//cannot consume, so must block till encryptor reads to output buffer
		pthread_mutex_lock(&output_mutex);
		while(!can_consume(output_queue)){	
			// printf("Writer can consume: %s\n", can_consume(output_queue) ? "true" : "false");
			// printf("Writer HELP: Head: %d, Tail: %d Counter: %d\n", output_queue->head, output_queue->tail, output_queue->counter);
			// printf("Writer HELP (input): Head: %d, Tail: %d Counter: %d\n", input_queue->head, input_queue->tail, input_queue->counter);

			pthread_cond_wait(&output_consume_cond, &output_mutex);
		}
		while(!has_been_counted(output_queue)){
			printf("Writer HELP (output counted): Head: %d, Tail: %d \n", output_queue->head, output_queue->tail);
			pthread_cond_wait(&output_counted_cond, &output_mutex);
		}
		printf("\nWriter BEFORE CONSUMED: Head: %d, Tail: %d\n", output_queue->head, output_queue->tail);
		// printf("Writer: Grabbing output..\n");
		char output = consume(output_queue);
		//output can read again after consumption
		printf("Writer: consumed: %c\n", output);
		pthread_mutex_unlock(&output_mutex);
		printf("Writer AFTER CONSUMED: Head: %d, Tail: %d\n", output_queue->head, output_queue->tail);

		pthread_cond_signal(&output_read_cond);
		pthread_cond_signal(&output_count_cond);
		if(output == EOF){	
			// printf("Writer: consumed: EOF\n");
			break;
		}
		write_output(output);
		// printf("Writer: Wrote output..\n");
	}
	return NULL;
}

void *reader(void *arg){ 
	char c;
	while (1) { 	
	 	pthread_mutex_lock(&reader_pause_mutex);
		while (reset_request) {
			checker = c;
			pthread_cond_wait(&reader_pause_cond, &reader_pause_mutex);
		}
		c = read_input();
		if (c == EOF) {
			break;
		}
        pthread_mutex_unlock(&reader_pause_mutex);
		
		pthread_mutex_lock(&input_mutex);
		while(!can_read(input_queue)){
			pthread_cond_wait(&input_read_cond, &input_mutex);
		}
		read(input_queue, c);
		pthread_mutex_unlock(&input_mutex);
		
		printf("\nReader: Character read from file and placed in queue: %c\n", c);
		pthread_cond_signal(&input_consume_cond);
		// printf("Reader: unlocked\n");	
	} 
	pthread_mutex_lock(&input_mutex);
	while(!can_read(input_queue)){
		// printf("reader help\n");	
		pthread_cond_wait(&input_read_cond, &input_mutex);
	}
	// printf("READER READ C = EOF\n");
	// printf("\nReader: locked\n");
	read(input_queue, c);
	// printf("READER: EOF read(), Head: %d, Tail: %d Counter: %d\n", input_queue->head, input_queue->tail, input_queue->counter);
	pthread_mutex_unlock(&input_mutex);
	pthread_cond_signal(&input_consume_cond);
	
	// printf("\nReader: EOF Reached.\n");
	return NULL;
}

bool encryptor_write(char to_encrypt){
	//whether to continue or not
	bool cont = true;

	//encrypt
	char encrypted = encrypt(to_encrypt);
	// printf("Encryptor: Encrypted '%c' ---> '%c'\n", to_encrypt, encrypted);

	//OUTPUT -- now place into output queue	
	pthread_mutex_lock(&output_mutex);
	// printf("\nEncryptor: output locked\n");

	//output cannot read, must wait for writer to consume
	while(!can_read(output_queue)){
		// printf("encrypt  help out\n");
		pthread_cond_wait(&output_read_cond, &output_mutex);
	}
	// printf("Encrypt (OUTPUT) BEFORE CONSUME: Head: %d, Tail: %d Counter: %d\n", output_queue->head, output_queue->tail, output_queue->counter);

	//Encryptor reaches EOF in buffer 
	if(to_encrypt == EOF){
		// printf("Encrypt: EOF reached, reading EOF to output\n");
		read(output_queue, to_encrypt);
		cont = false;
	}
	else{
		read(output_queue, encrypted);
	}
	// printf("Encrypt (OUTPUT) AFTER CONSUME: Head: %d, Tail: %d Counter: %d\n", output_queue->head, output_queue->tail, output_queue->counter);
	

	// printf("Encryptor: output read\n");
	pthread_mutex_unlock(&output_mutex);
	//output can now consume and count
	pthread_cond_signal(&output_consume_cond);
	
	return cont;
	// printf("Encryptor: output unlocked\n");
		
}
void *encryptor(void *arg){
	char to_encrypt;
	while(1){
		pthread_mutex_lock(&input_mutex);
		// printf("ENCRY help OUT\n");
		//cannot consume, so must block until reader reads
		while(!can_consume(input_queue)){
			// printf("Encrypt HELP: Head: %d, Tail: %d Counter: %d\n", input_queue->head, input_queue->tail, input_queue->counter);
			pthread_cond_wait(&input_consume_cond, &input_mutex);
		}
		while(!has_been_counted(input_queue)){
			// printf("Encrypt HELP (input counted): Head: %d, Tail: %d\n", input_queue->head, input_queue->tail);
			pthread_cond_wait(&input_counted_cond, &input_mutex);
		}
		//consume next char in input buffer
		// printf("Encrypt BEFORE CONSUME: Head: %d, Tail: %d Counter: %d\n", input_queue->head, input_queue->tail, input_queue->counter);
		to_encrypt = consume(input_queue);
		// printf("Encrypt AFTER CONSUME: Head: %d, Tail: %d Counter: %d\n", input_queue->head, input_queue->tail, input_queue->counter);
		//signal to input buffer can read again
		
		pthread_mutex_unlock(&input_mutex);
		pthread_cond_signal(&input_read_cond);
		pthread_cond_signal(&input_count_cond);
		//Now write to output buffer the encrypted character
		if(!encryptor_write(to_encrypt)){
			break;
		}
	}
	
	return NULL;
}



void *input_counter(void *arg){
	char counted;
	while(1){
		pthread_mutex_lock(&input_mutex);
		while(has_been_counted(input_queue)){
			pthread_cond_wait(&input_count_cond, &input_mutex);
			printf("Input Counter: help\n");
		}
		// printf("\nInput Counter: locked\n");
		counted = count(input_queue);
		count_input(counted);
		pthread_cond_signal(&input_counted_cond);
		printf("Input Counter: counted %c\n", counted);

		pthread_mutex_unlock(&input_mutex);

		// pthread_mutex_lock(&reader_pause_mutex);
		// if(counted == c){
		// 	printf("Input Counter: counted == c");
		// 	input_last_count = true;
		// }
		// if(output_last_count && input_last_count){
		// 	printf("OUTPUT: Sent equal IO REQUEST\n");
		// 	pthread_cond_signal(&io_equal_cond);
		// }
		// pthread_mutex_unlock(&reader_pause_mutex);

		if(counted == EOF){
			break;
		}	
	}
	// printf("Input Counter: EOF reached\n\n");
	return NULL;		

}

void *output_counter(void *arg){
char counted;
	while(1){
		pthread_mutex_lock(&output_mutex);
		// printf("\nOutput Counter: locked\n");
		// printf("Can count: %s\n", can_count(output_queue) ? "true" : "false");
		while(has_been_counted(output_queue)){
			pthread_cond_wait(&output_count_cond, &output_mutex);
			printf("Output Counter: help\n");
		}

		counted = count(output_queue);

		count_output(counted);
		printf("Output Counter: counted '%c'\n", counted);
		pthread_mutex_unlock(&output_mutex); 
		pthread_cond_signal(&output_counted_cond);
		// printf("\nOutput Counter: unlocked\n");
		

		if(counted == EOF){ 
			// printf("Output Counter: EOF reached\n\n");	
			break;
		}
		// printf("OUTP help OUT\n");
		
		printf("Total input count with current key is %d\n", get_input_total_count());
		printf("Total output count with current key is %d\n", get_output_total_count());
		printf("Current c value is >>>>>>>>>>%c<<<<<<<<<<\n", checker);

		// pthread_mutex_lock(&reader_pause_mutex);
		// if(counted == encrypt(checker)){
		// 	printf("Input Counter: counted == checker");
		// 	output_last_count = true;
		// }
		// if(output_last_count && input_last_count){
			
		// 	printf("OUTPUT: Sent equal IO REQUEST\n");
		// 	pthread_cond_signal(&io_equal_cond);
			
		// }
		// pthread_mutex_unlock(&reader_pause_mutex);
		printf("Output Counter: counted character '%c'\n", counted);
		
	}
	
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
	init(in, out); 

	pthread_t readr, inp, encrypt, outp, write;
	//INIT PTHREAD MUTEX
	pthread_mutex_init(&input_mutex, NULL);
	pthread_mutex_init(&output_mutex, NULL);
	pthread_mutex_init(&reader_pause_mutex, NULL);

	//INIT PTHREAD CONDITION VARS.
	pthread_cond_init(&input_read_cond, NULL);
	pthread_cond_init(&input_consume_cond, NULL);
	pthread_cond_init(&input_count_cond, NULL);
	pthread_cond_init(&input_counted_cond, NULL);
	pthread_cond_init(&output_read_cond, NULL);
	pthread_cond_init(&output_consume_cond, NULL);
	pthread_cond_init(&output_count_cond, NULL);
	pthread_cond_init(&output_counted_cond, NULL);
	pthread_cond_init(&reader_pause_cond, NULL);
	pthread_cond_init(&io_equal_cond, NULL);

	printf("Parent creating threads\n");

	//CREATE THREADS
	pthread_create(&readr, NULL, reader, NULL);
	pthread_create(&inp, NULL, input_counter, NULL);
	pthread_create(&encrypt, NULL, encryptor, NULL);
	pthread_create(&write, NULL, writer, NULL);
	pthread_create(&outp, NULL, output_counter, NULL);

	//WAIT FOR THREADS TO COMPLETE
	pthread_join(readr, NULL);
	printf("reader done \n");
	pthread_join(inp, NULL);
	printf("inputcounter done \n");
	pthread_join(encrypt, NULL);
	printf("encryptor done \n");	
	pthread_join(outp, NULL);
	printf("outputcount done \n");
	pthread_join(write, NULL);
	printf("writer done \n");


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

