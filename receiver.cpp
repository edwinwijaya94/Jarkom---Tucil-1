/*
* File : T1_rx.cpp
*/
#include "dcomm.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <netdb.h>
#include <pthread.h>

/* Delay to adjust speed of consuming buffer, in microseconds */
#define DELAY 100000

/* Define receive buffer size */
#define RXQSIZE 20
#define MIN_UPPER 10
#define MAX_LOWER 10

char buffer[2]; //for sending XON/XOFF signal
struct sockaddr_in serv_addr; //sockaddr for server
struct sockaddr_in cli_addr; //sockaddr for client
int cli_len= sizeof(cli_addr); // sizeof client address

Byte rxbuf[RXQSIZE];
QTYPE rcvq = { 0, 0, 0, RXQSIZE, rxbuf };
QTYPE *rxq = &rcvq;
Byte sent_xonxoff = XON;
bool send_xon = true,
	 send_xoff = false;

/* Socket */
int sockfd; // listen on sock_fd

/* Functions declaration */
static Byte *rcvchar(int sockfd, QTYPE *queue, int *j);
static Byte *q_get(QTYPE *);
static void *consume(void *param); // Thread function

using namespace std;

//main program
int main(int argc, char *argv[])
{

	Byte c;

	//create socket
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd <0)
		printf("error socket\n");

	//Insert code here to bind socket to the port number given in argv[1].
	memset((char *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET; //assign as UDP
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); //assign server address
	serv_addr.sin_port = htons(atoi(argv[1])); //assign port number

	//binding
	if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))< 0)
		perror("error on binding\n");
	else
		printf("Binding pada port: %d\n",atoi(argv[1]));

	/* Initialize XON/XOFF flags */
	sent_xonxoff = XON;

	/* Create new thread */
	pthread_t consume_thread;

	// "CONSUME" THREAD
	if(pthread_create(&consume_thread, NULL, consume, &rcvq)){
		fprintf(stderr, "Error creating thread\n");
		return 1;
	}

	// 'RECEIVE' THREAD
	int j=1;
	while (true) {
		c = *(rcvchar(sockfd, &rcvq, &j));

		//delay
		// usleep(300000);
		j++;
	}

	//join thread
	if(pthread_join(consume_thread,NULL)){
		fprintf(stderr,"Error joining thread\n");
		return 2;
	}

	return 0;
}
static Byte *rcvchar(int sockfd, QTYPE *queue, int *j)
{

	int r; //result of sending signal to server
	if(queue->count > MIN_UPPER && !send_xoff){ // buffer full, sent xoff
		send_xon = false; //set xon flag
		send_xoff = true; //set xoff flag
		sent_xonxoff = XOFF; //assign signal
		
		buffer[0]=sent_xonxoff;
		
		//send xoff signal
		r = sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));

		//check error on signal
		if(r>0)
			printf("Buffer > minimum upperlimit. Mengirim XOFF.\n");
		else
			printf("xoff signal failed\n");
	}

	//wait for next chars
	char temp[1];
	r = recvfrom(sockfd, temp, RXQSIZE, 0, (struct sockaddr*) &cli_addr, (socklen_t*) &cli_len);

	//check recvfrom error
	if(r < 0){
		printf("error recvfrom\n");
	}

	// check end of file
	if(int(temp[0]) != Endfile){
		printf("Menerima byte ke-%d.\n",*j);
		*j++;
	}
	else
		*j=0; //reset j

	//update queue attr
	queue->data[queue->rear]=temp[0]; // add received char to buffer
	queue->rear = (((queue->rear) + 1) % RXQSIZE + 1) - 1;
	(queue->count)++;

	return queue->data;
}

/* q_get returns a pointer to the buffer where data is read or NULL if
* buffer is empty.
*/
static Byte *q_get(QTYPE *queue)
{
	Byte *current;

	/* Nothing in the queue */
	if (!queue->count) {
		return (NULL);
	}
	
	current=&queue->data[queue->front];

	//update queue attr
	queue->front = (((queue->front) + 1) % RXQSIZE + 1) - 1;
	(queue->count)--;

	//sending XON signal
	if(queue->count < MAX_LOWER && !send_xon){
		send_xon = true; //set xon flag
		send_xoff = false; //set xoff flag
		sent_xonxoff= XON;

		int r;
		buffer[0]=sent_xonxoff;
		r = sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));

		//check error on signal
		if(r>0)
			printf("Buffer < maximum lowerlimit. Mengirim XON.\n");
		else
			printf("xon signal failed\n");
	}
	
	return current;
}

// Thread function
static void *consume(void *param){

	QTYPE *rcvq_ptr = (QTYPE *)param;

	int i=1; //character index
	while (true) {

		/* Call q_get */
		Byte *res;
		res = q_get(rcvq_ptr);
		if(res && (int(*res)>32 || int(*res)==CR || int(*res)==LF || int(*res)==Endfile )){
			printf("Mengkonsumsi byte ke-%d : %c\n",i,*res);
			i++;
		}
		/* Can introduce some delay here. */
		usleep(DELAY); //delay
	}
	pthread_exit(0);
}
