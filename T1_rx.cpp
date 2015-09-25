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

/* Delay to adjust speed of consuming buffer, in milliseconds */
#define DELAY 500

/* Define receive buffer size */
#define RXQSIZE 8
#define MIN_UPPER (RXQSIZE*0.5)

char buffer[2]; //for sending XON/XOFF signal
struct sockaddr_in serv_addr; //sockaddr for server
struct sockaddr_in cli_addr; //sockaddr for client
int cli_len= sizeof(cli_addr); // sizeof client address

Byte rxbuf[RXQSIZE];
QTYPE rcvq = { 0, 0, 0, RXQSIZE, rxbuf };
QTYPE *rxq = &rcvq;
Byte sent_xonxoff = XON;
bool send_xon = false,
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
	printf("start program\n");

	Byte c;

	//create socket
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd <0)
		printf("error socket\n");
	else
		printf("socket OK\n");
	cout<<atoi(argv[1])<<endl;

	//Insert code here to bind socket to the port number given in argv[1].
	memset((char *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET; //assign as UDP
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); //assign server address
	serv_addr.sin_port = htons(atoi(argv[1])); //assign port number

	//binding
	if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))< 0)
		perror("error on binding\n");
	else
		printf("%s %d %s %d\n","Binding pada",&serv_addr," port:",serv_addr.sin_port);

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
	printf("'receive' thread\n");
	int j=1;
	while (true) {
		c = *(rcvchar(sockfd, &rcvq, &j));

		/* Quit on end of file */
		/*if (int(c) == Endfile) {
			printf("break\n");
			break;
		}*/

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
	/*
	Insert code here.
	Read a character from socket and put it to the receive buffer.
	If the number of characters in the receive buffer is above certain
	level, then send XOFF and set a flag (why?).
	Return a pointer to the buffer where data is put.
	*/

	int r; //result of sending signal to server
	if(queue->count == RXQSIZE && !send_xoff){ // buffer full, sent xoff
		printf("buffer full\n");
		send_xon = false; //set xon flag
		send_xoff = true; //set xoff flag
		sent_xonxoff = XOFF; //assign signal

		buffer[0]=sent_xonxoff;
		r = sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));

		//check error on signal
		if(r>0)
			printf("xoff signal sent\n");
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
	//cout<<"front: "<<queue->front<<" "<<"msg: "<<queue->data[queue->front]<<endl;
	//cout<<"rear: "<<queue->rear<<" "<<"msg: "<<queue->data[queue->rear]<<endl;
	queue->rear = (((queue->rear) + 1) % RXQSIZE + 1) - 1;
	(queue->count)++;
	//cout<<"rcv count: "<<queue->count<<endl;

	return queue->data;
}

/* q_get returns a pointer to the buffer where data is read or NULL if
* buffer is empty.
*/
static Byte *q_get(QTYPE *queue)
{
	Byte *current;

	/* Nothing in the queue */
	//cout<<"count: "<<queue->count<<endl;
	if (!queue->count) {
		// printf("buffer empty\n");
		return (NULL);
	}
	/*
	Insert code here.
	Retrieve data from buffer, save it to "current" and "data"
	If the number of characters in the receive buffer is below certain
	level, then send XON.
	Increment front index and check for wraparound.
	*/
	//printf("a\n");
	//cout<<"index: "<<queue->front<<"info: "<<queue->data[queue->front]<<endl;
	current=&queue->data[queue->front];
	//printf("b\n");

	//update queue attr
	queue->front = (((queue->front) + 1) % RXQSIZE + 1) - 1;
	(queue->count)--;
	//printf("c\n");
	//sending XON signal
	if(queue->count < MIN_UPPER && !send_xon){
		send_xon = true; //set xon flag
		send_xoff = false; //set xoff flag
		sent_xonxoff= XON;

		int r;
		buffer[0]=sent_xonxoff;
		r = sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));

		//check error on signal
		if(r>0)
			printf("xon signal sent\n");
		else
			printf("xon signal failed\n");
	}
	//printf("d\n");
	return current;
}

// Thread function
static void *consume(void *param){
	printf("'consume' thread\n");
	QTYPE *rcvq_ptr = (QTYPE *)param;

	int i=1; //character index
	while (true) {
		//printf("consuming message\n");

		/* Call q_get */
		Byte *res;
		res = q_get(rcvq_ptr);
		if(res){
			printf("ascii code: %d\n",int(*res));
			printf("Mengkonsumsi byte ke-%d : %c\n",i,*res);
			i++;
		}
		/* Can introduce some delay here. */
		usleep(100000); //delay
	}
	pthread_exit(0);
}
